/**
Copyright 2016 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

// File used on all platforms but iOS, which must use a .mm file to invoke the
// objective-c compiler.

#if defined(ION_PLATFORM_WINDOWS)
#  if defined(NOGDI)
#    undef NOGDI
#  endif
#  include <windows.h>  // NOLINT
#  undef ERROR  // This is defined in Windows headers.
#endif

// Must be below <windows.h>
#include "ion/portgfx/visual.h"

#if defined(ION_PLATFORM_ASMJS)
#  include <emscripten.h>
#endif

#include <cstddef>  // For NULL.
#include <cstring>  // For strchr().
#include <unordered_map>

#include "ion/portgfx/glheaders.h"

#if defined(ION_PLATFORM_LINUX) && !defined(ION_GFX_OGLES20)
#  include <GL/glx.h>  // NOLINT
#  undef None  // Defined in X.h, but reused in gtest.h.
#  undef Bool  // Defined in X.h, but reused in gtest.h.

#elif defined(ION_PLATFORM_NACL)
#  include "ppapi/cpp/graphics_3d.h"
#  include "ppapi/cpp/instance.h"
#  include "ppapi/cpp/module.h"
#  include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"
#endif

#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/stringutils.h"
#include "ion/base/threadlocalobject.h"
#include "ion/port/environment.h"
#include "ion/port/mutex.h"

namespace ion {
namespace portgfx {

namespace {

//-----------------------------------------------------------------------------
//
// Visual thread local storage holder.
//
//-----------------------------------------------------------------------------

class VisualStorage {
 public:
  VisualStorage() : current_(NULL) {}
  const Visual* GetCurrent() const { return current_; }
  void SetCurrent(const Visual* value) { current_ = value; }

 private:
  const Visual* current_;
};

// Returns the VisualStorage used by the calling thread.
static VisualStorage* GetHolder() {
  ION_DECLARE_SAFE_STATIC_POINTER(base::ThreadLocalObject<VisualStorage>,
                                  s_helper);
  return s_helper->Get();
}

//-----------------------------------------------------------------------------
//
// Global tracking of Visual instances, needed to properly track user-created
// GL contexts created outside of Ion.
//
//-----------------------------------------------------------------------------

// A map of Visual IDs to instances.
typedef std::unordered_map<size_t, const Visual*> VisualMap;

// Returns a map of GL context pointers to Visual objects.
static VisualMap& GetVisualMap() {
  ION_DECLARE_SAFE_STATIC_POINTER(VisualMap, visuals);
  return *visuals;
}

// Returns a Mutex used to lock access to the global VisualMap.
static port::Mutex* GetVisualMapMutex() {
  ION_DECLARE_SAFE_STATIC_POINTER(port::Mutex, mutex);
  return mutex;
}

#if defined(ION_PLATFORM_WINDOWS)

// Windows window class creator.
class IonWindowClass {
 public:
  IonWindowClass() {
    WNDCLASSEX window_class;
    memset(&window_class, 0, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_OWNDC;
    window_class.lpfnWndProc = &DefWindowProc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.lpszClassName = kClassName;
    atom_ = RegisterClassEx(&window_class);
  }

  ~IonWindowClass() { UnregisterClass(kClassName, GetModuleHandle(nullptr)); }

  ATOM GetAtom() const { return atom_; }

 private:
  static const char kClassName[];

  ATOM atom_;
};

const char IonWindowClass::kClassName[] = "ION";

ATOM GetIonWindowClass() {
  ION_DECLARE_SAFE_STATIC_POINTER(IonWindowClass, window_class);
  return window_class->GetAtom();
}

#endif

}  // anonymous namespace

struct Visual::VisualInfo {
  // These functions are in VisualInfo since they are used both here and in
  // visual_darwin.mm.
  //
  // Returns the address of the currently bound GL context.
  static void* GetCurrentContext();
  // Clears the currently bound GL context (i.e., sets it to NULL).
  static void ClearCurrentContext();

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_ASMJS) || \
    defined(ION_PLATFORM_GENERIC_ARM) || \
    (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))
  VisualInfo()
      : display(NULL),
        surface(NULL),
        context(NULL) {}
  void InitEgl(Type type);
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
#elif defined(ION_PLATFORM_LINUX)
  VisualInfo()
      : display(NULL),
        info(NULL),
        context(NULL),
        colormap(0),
        window(0) {}
  Display* display;
  XVisualInfo* info;
  GLXContext context;
  Colormap colormap;
  Window window;
#elif defined(ION_PLATFORM_NACL)
  VisualInfo() : context(0) {}
  PP_Resource context;
#elif defined(ION_PLATFORM_WINDOWS)
  VisualInfo()
#  if defined(ION_ANGLE)
      : display(nullptr),
        surface(nullptr),
        context(nullptr),
#  else
      : device_context(nullptr),
        context(nullptr),
#  endif
        window(nullptr) {
  }
#  if defined(ION_ANGLE)
  void InitEgl(Type type);
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
#  else
  HDC device_context;
  HGLRC context;
#  endif
  HWND window;  // Only populated if this VisualInfo created a window.
#endif
};

const Visual* Visual::GetCurrent() {
  // If there is a valid GL context bound then see if we have a Visual which
  // wraps it.
  const Visual* current = GetHolder()->GetCurrent();
  // Special case for tests, which use a MockVisual.
  if (current && current->type_ == kMock)
    return current;

  // If the context ID reported by the windowing API and the ID of the Visual
  // stored in the current thread's holder are the same, then we already have
  // the correct Visual and can avoid taking a lock.
  const size_t id = reinterpret_cast<size_t>(VisualInfo::GetCurrentContext());
  if (current && id == current->GetId()) {
    return current;
  }

  // Either we have not created a visual for this context yet, or someone else
  // changed the current context when we weren't looking. We have to get the
  // correct Visual from the visual map.
  current = nullptr;

  VisualMap& visuals = GetVisualMap();
  base::LockGuard guard(GetVisualMapMutex());
  if (id) {
    // If we already have a Visual associated with the current context then
    // return it.
    VisualMap::iterator it = visuals.find(id);
    if (it != visuals.end())
      current = it->second;
  }
  if (current) {
    // Ensure that this is actually the current Visual.
    GetHolder()->SetCurrent(current);
  } else if (id) {
    // We have nothing mapped to a user-created GL context that is already
    // bound. Create a new Visual that wraps the current context.
    DCHECK(id);
    DCHECK(visuals.find(id) == visuals.end());
    Visual* new_visual = new Visual(kCurrent);
    new_visual->UpdateId();
    current = new_visual;
    DCHECK_EQ(id, current->GetId());
    visuals[id] = new_visual;
    if (!MakeCurrent(current))
      current = nullptr;
  } else {
    // There's no GL context bound, so make that explicit with the Visual.
    GetHolder()->SetCurrent(nullptr);
  }

  return current;
}

bool Visual::MakeCurrent(const Visual* new_visual) {
  if (!new_visual) {
    if (const Visual* current = GetCurrent()) {
      GetHolder()->SetCurrent(NULL);
      if (current->type_ != kMock) {
        VisualInfo::ClearCurrentContext();
      }
    }
    return true;
  } else if (new_visual->IsValid()) {
    const bool success = new_visual->MakeCurrent();
    if (success) {
      GetHolder()->SetCurrent(new_visual);
    } else {
      GetHolder()->SetCurrent(NULL);
    }
    return success;
  } else {
    LOG(WARNING) << "Failed to make Visual current: " << new_visual;
    return false;
    // NOTE: this assumes that we know that a failed MakeCurrent() attempt
    // doesn't change the current OpenGL context.
  }
}

void Visual::DestroyWrappingVisual(const Visual* visual) {
  if (visual) {
    const Visual* visual_to_delete = NULL;
    {
      VisualMap& visuals = GetVisualMap();
      base::LockGuard guard(GetVisualMapMutex());

      // If the visual is of type kCurrent delete it (it will be removed from
      // the VisualMap in its destructor).
      VisualMap::iterator it = visuals.find(visual->GetId());
      if (it != visuals.end()) {
        DCHECK_EQ(visual, it->second);
        // If the visual wraps a non-Visual GL context, delete it.
        if (visual->type_ == kCurrent)
          visual_to_delete = it->second;
      }
    }
    if (visual_to_delete)
      delete visual_to_delete;
  }
}

size_t Visual::GetCurrentId() {
  // Normally, we want to return the true ID as reported by the platform API.
  // However, when testing, the platform API is not used and there is no
  // OpenGL context active, so we treat whatever is in the thread-local holder
  // as authoritative.
  const Visual* ptr = GetHolder()->GetCurrent();
  if (ptr && ptr->type_ == kMock) {
    return ptr->GetId();
  }
  return reinterpret_cast<size_t>(VisualInfo::GetCurrentContext());
}

void Visual::TeardownVisual(Visual* visual) {
  DCHECK(visual);
  const bool was_current = visual == GetHolder()->GetCurrent();
  if (was_current) {
    GetHolder()->SetCurrent(NULL);
  }
  // Find the Visual in the global map and remove it.
  if (visual->GetId()) {
    base::LockGuard guard(GetVisualMapMutex());
    VisualMap& visuals = GetVisualMap();
    VisualMap::iterator it = visuals.find(visual->GetId());
    DCHECK(it != visuals.end());
    visuals.erase(it);
  }

  if (visual->type_ == kNew) {
    // Unbind the context before nuking it.
    if (was_current) {
      VisualInfo::ClearCurrentContext();
    }
    visual->TeardownContextNew();
  } else if (visual->type_ == kShare) {
    visual->TeardownContextShared();
  } else {
    // Don't do any cleanup in kCurrent.
  }
}

std::unique_ptr<Visual> Visual::CreateVisual() {
  Visual* visual = new Visual(kNew);
  visual->UpdateId();
  RegisterVisual(visual);
  return std::unique_ptr<Visual>(visual);
}

std::unique_ptr<Visual> Visual::CreateVisualInCurrentShareGroup() {
  const Visual* current = GetCurrent();
  if (!current || !current->IsValid()) {
    LOG(WARNING) << "GetCurrent() returned NULL or InValid()";
  }
  Visual* visual = current ? current->CreateVisualInShareGroup() : NULL;
  if (visual) {
    visual->UpdateId();
    RegisterVisual(visual);
  }
  return std::unique_ptr<Visual>(visual);
}

void Visual::RegisterVisual(Visual *visual) {
  if (visual && visual->IsValid()) {
    base::LockGuard guard(GetVisualMapMutex());
    VisualMap& visuals = GetVisualMap();
    visuals[visual->GetId()] = visual;
  }
}

Visual* Visual::CreateVisualInShareGroup() const {
  DCHECK_EQ(this, GetCurrent());
  return new Visual(kShare);
}

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_ASMJS) || \
    defined(ION_PLATFORM_GENERIC_ARM) || defined(ION_ANGLE) || \
    (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))

void Visual::VisualInfo::InitEgl(Visual::Type type) {
  if (display == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Could not get EGL display";
    return;
  }

  EGLint major = 0;
  EGLint minor = 0;
  if (!eglInitialize(display, &major, &minor)) {
    LOG(ERROR) << "Could not init EGL";
    return;
  }
  if (major < 1 || minor < 2) {
    LOG(ERROR) << "System does not support at least EGL 1.2";
    return;
  }

  if (type == kCurrent) {
#if defined(ION_PLATFORM_ASMJS)
    // Asmjs typically uses the browser to create and manage the current GL
    // context.
    context = reinterpret_cast<EGLContext>(EM_ASM_INT_V({
      return Module.ctx ? 1 : 0;
    }));
#else
    surface = eglGetCurrentSurface(EGL_DRAW);
    if (surface == EGL_NO_SURFACE)
      LOG(ERROR) <<
          "Unable to get current surface while creating a kCurrent Visual.";
    context = eglGetCurrentContext();
    if (context == EGL_NO_CONTEXT)
      LOG(ERROR) <<
          "Unable to get current context while creating a kCurrent Visual.";
#endif
  } else {
    EGLint attr[] = {
      EGL_BUFFER_SIZE, 24,
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT,
      EGL_NONE
    };

    EGLConfig ecfg;
    EGLint num_config;
    if (!eglChooseConfig(display, attr, &ecfg, 1, &num_config)) {
      LOG(ERROR) << "Could not to choose config (egl code: " << eglGetError()
                 << ")";
      return;
    }

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_GENERIC_ARM) || \
    (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))
    // Since we are only using new/shared contexts for background OpenGl uploads
    // the width and height of our PBufferSurface are unimportant, except 0,0
    // sized buffers will crash some platforms.
    EGLint buffer_attr[] = {
      EGL_WIDTH, 1,
      EGL_HEIGHT, 1,
      EGL_NONE
    };
    surface = eglCreatePbufferSurface(display, ecfg, buffer_attr);
#elif defined(ION_PLATFORM_ASMJS)
    surface = eglCreateWindowSurface(display, ecfg, NULL, NULL);
#else
    surface = eglCreateWindowSurface(display, ecfg, window, NULL);
#endif
    if (surface == EGL_NO_SURFACE) {
      LOG(ERROR) << "Could not create EGL surface (egl code: " << eglGetError()
                 << ")";
      return;
    }

    const EGLint ctxattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    const EGLContext share_context =
        type == kShare ? eglGetCurrentContext() : EGL_NO_CONTEXT;
    if (kShare == type && EGL_NO_CONTEXT == share_context) {
      LOG(ERROR) << "Attempting to share a NULL context.";
    }
    context = eglCreateContext(display, ecfg, share_context, ctxattr);
    if (context == EGL_NO_CONTEXT)
      LOG(ERROR) << "Could not create EGL context (egl code: " << eglGetError()
                 << ").";
  }
}
#endif

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_ASMJS) || \
    defined(ION_PLATFORM_GENERIC_ARM) || \
    (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))
void* Visual::VisualInfo::GetCurrentContext() {
#if defined(ION_PLATFORM_ASMJS)
  // Since asmjs has no threads, there will only be one context.
  EGLContext context = reinterpret_cast<EGLContext>(EM_ASM_INT_V({
    return Module.ctx ? 1 : 0;
  }));
  return context;
#else
  return eglGetCurrentSurface(EGL_DRAW) != EGL_NO_SURFACE ?
      eglGetCurrentContext() : NULL;
#endif
}

void Visual::VisualInfo::ClearCurrentContext() {
  eglMakeCurrent(eglGetDisplay((EGLNativeDisplayType) EGL_DEFAULT_DISPLAY),
                 EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

Visual::Visual(Type type)
    : visual_(new VisualInfo), type_(type) {
  visual_->display = eglGetDisplay((EGLNativeDisplayType) EGL_DEFAULT_DISPLAY);
  if (visual_->display == EGL_NO_DISPLAY)
    return;
  visual_->InitEgl(type);
  if (type_ == kCurrent) {
    MakeCurrent(this);
  }
}

Visual::Visual() : visual_(new VisualInfo), type_(kMock) {}

bool Visual::IsValid() const {
  return visual_->context;
}

bool Visual::MakeCurrent() const {
#if defined(ION_PLATFORM_ASMJS)
  return visual_->context != 0;
#endif
  const EGLBoolean success = eglMakeCurrent(
      visual_->display, visual_->surface, visual_->surface, visual_->context);
  if (success == EGL_FALSE) {
    LOG(ERROR) << "Unable to make Visual current.";
    return false;
  } else {
    return true;
  }
}

void Visual::TeardownContextNew() {
  eglDestroyContext(visual_->display, visual_->context);
  eglDestroySurface(visual_->display, visual_->surface);
  eglTerminate(visual_->display);
}

void Visual::TeardownContextShared() {
  if (eglGetCurrentContext() != visual_->context) {
    eglDestroyContext(visual_->display, visual_->context);
  }
  if (eglGetCurrentSurface(EGL_DRAW) != visual_->surface) {
    eglDestroySurface(visual_->display, visual_->surface);
  }
  if (eglGetDisplay((EGLNativeDisplayType) EGL_DEFAULT_DISPLAY) !=
      visual_->display) {
    eglTerminate(visual_->display);
  }
}

Visual::~Visual() {
  TeardownVisual(this);
}

#elif defined(ION_PLATFORM_LINUX) && !defined(ION_GFX_OGLES20)
namespace {

// Returns a pointer to a Display object if there is an X server running,
// otherwise returns NULL. Note that a non-NULL return value must be
// closed with XCloseDisplay(). This wrapper is necessary because a
// call to XOpenDisplay() when there is no X server running takes about
// 6 seconds to timeout.
Display* OpenDisplay() {
  static const int x_return_value =
      system("pgrep -c '^Xorg$' >& /dev/null");
  static const bool is_x_running =
      x_return_value == 0;
  static const bool is_system_working =
      x_return_value != -1;
  Display* display = nullptr;
  if (is_x_running || !is_system_working) {
    std::string display_name = port::GetEnvironmentVariableValue("DISPLAY");
    if (!display_name.empty()) {
      display = XOpenDisplay(display_name.c_str());
    } else {
      display = XOpenDisplay(":0");
    }
  }
  return display;
}

// Returns a Mutex to lock when creating or destroying X11 resources, since X11
// is not thread-safe.
port::Mutex* GetXMutex() {
  ION_DECLARE_SAFE_STATIC_POINTER(port::Mutex, mutex);
  return mutex;
}

}  // anonymous namespace

void* Visual::VisualInfo::GetCurrentContext() {
  return glXGetCurrentContext();
}

void Visual::VisualInfo::ClearCurrentContext() {
  base::LockGuard guard(GetXMutex());
  if (Display* display = OpenDisplay()) {
    glXMakeCurrent(display, 0, NULL);
    XCloseDisplay(display);
  }
}

Visual::Visual(Type type)
    : visual_(new VisualInfo), type_(type) {
  base::LockGuard guard(GetXMutex());
  if (type == kCurrent) {
    visual_->display = glXGetCurrentDisplay();
    visual_->window = glXGetCurrentDrawable();
    visual_->context = glXGetCurrentContext();
    guard.Unlock();
    MakeCurrent(this);
    return;
  } else {
    // Open the first display.
    visual_->display = OpenDisplay();
    if (!visual_->display)
      return;

    int error_base, event_base;
    // Ensure that the X server supports GLX.
    if (!glXQueryExtension(visual_->display, &error_base, &event_base)) {
      // If the server is not running or does not support GLX, just return,
      // which will cause glGetString() to fail, and the test will not be run.
      visual_->context = NULL;
      return;
    }

    // Create the X Visual.
    int attributes[] = { GLX_RGBA, 0 };
    visual_->info = glXChooseVisual(
        visual_->display, DefaultScreen(visual_->display), attributes);
    if (!visual_->info)
      return;

    // Create the GL context.
    GLXContext share_context = type == kShare ? glXGetCurrentContext() : NULL;
    visual_->context =
        glXCreateContext(visual_->display, visual_->info, share_context, True);
    if (!visual_->context)
      return;

    // Create a colormap for the window.
    visual_->colormap = XCreateColormap(
        visual_->display, RootWindow(visual_->display, visual_->info->screen),
        visual_->info->visual, AllocNone);
    XSetWindowAttributes window_attributes;
    window_attributes.border_pixel = 0;
    window_attributes.colormap = visual_->colormap;
    visual_->window = XCreateWindow(
        visual_->display, RootWindow(visual_->display, visual_->info->screen),
        0, 0, 1, 1, 0, visual_->info->depth, InputOutput, visual_->info->visual,
        CWBorderPixel | CWColormap, &window_attributes);
  }
}

Visual::Visual() : visual_(new VisualInfo), type_(kMock) {}

bool Visual::IsValid() const {
  return visual_->display && visual_->context && visual_->window;
}

bool Visual::MakeCurrent() const {
  const int success =
      glXMakeCurrent(visual_->display, visual_->window, visual_->context);
  if (!success) {
    LOG(ERROR) << "Unable to make Visual current.";
    return false;
  } else {
    return true;
  }
}

void Visual::TeardownContextNew() {
  base::LockGuard guard(GetXMutex());

  if (visual_->context)
    glXDestroyContext(visual_->display, visual_->context);
  if (visual_->window)
    XDestroyWindow(visual_->display, visual_->window);
  if (visual_->colormap)
    XFreeColormap(visual_->display, visual_->colormap);
  if (visual_->info)
    XFree(visual_->info);
  if (visual_->display)
    XCloseDisplay(visual_->display);
}

void Visual::TeardownContextShared() {
  base::LockGuard guard(GetXMutex());

  const Display* display = glXGetCurrentDisplay();
  const Window window = glXGetCurrentDrawable();
  const GLXContext context = glXGetCurrentContext();

  if (visual_->context && context != visual_->context)
    glXDestroyContext(visual_->display, visual_->context);
  if (visual_->window && window != visual_->window)
    XDestroyWindow(visual_->display, visual_->window);
  if (visual_->colormap)
    XFreeColormap(visual_->display, visual_->colormap);
  if (visual_->info)
    XFree(visual_->info);
  if (visual_->display && display != visual_->display)
    XCloseDisplay(visual_->display);
}

Visual::~Visual() {
  TeardownVisual(this);
}

#elif defined(ION_PLATFORM_NACL)
void* Visual::VisualInfo::GetCurrentContext() {
  return reinterpret_cast<void*>(glGetCurrentContextPPAPI());
}

void Visual::VisualInfo::ClearCurrentContext() {
  glSetCurrentContextPPAPI(0);
}

Visual::Visual(Type type)
    : visual_(new VisualInfo), type_(type) {
  if (type == kCurrent) {
    visual_->context = glGetCurrentContextPPAPI();
    MakeCurrent(this);
  } else {
    pp::Module* module = pp::Module::Get();
    if (!module || !glInitializePPAPI(module->get_browser_interface())) {
      LOG(ERROR) << "Unable to initialize GL PPAPI since there is no browser!";
      return;
    }

    const PPB_Graphics3D* interface = reinterpret_cast<const PPB_Graphics3D*>(
        module->GetBrowserInterface(PPB_GRAPHICS_3D_INTERFACE));
    if (!interface) {
      LOG(ERROR) << "Unable to initialize PP Graphics3D interface!";
      return;
    }

    int32_t attribs[] = {
      PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
      PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
      PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 8,
      PP_GRAPHICS3DATTRIB_NONE
    };

    PP_Resource share_context =
        type == kShare ? glGetCurrentContextPPAPI() : 0U;

    const pp::Module::InstanceMap& instances = module->current_instances();
    for (pp::Module::InstanceMap::const_iterator iter = instances.begin();
         iter != instances.end(); ++iter) {
      // Choose the first instance.
      if (pp::Instance* instance = iter->second) {
        visual_->context =
            interface->Create(instance->pp_instance(), share_context, attribs);
        break;
      }
    }
  }
}

Visual::Visual() : visual_(new VisualInfo), type_(kMock) {}

bool Visual::IsValid() const {
  return visual_->context;
}

bool Visual::MakeCurrent() const {
  glSetCurrentContextPPAPI(visual_->context);
  if (visual_->context != glGetCurrentContextPPAPI()) {
    LOG(ERROR) << "Unable to make Visual current.";
    return false;
  } else {
    return true;
  }
}

void Visual::TeardownContextNew() {
  glTerminatePPAPI();
}

void Visual::TeardownContextShared() {
}

Visual::~Visual() {
  TeardownVisual(this);
}

#elif defined(ION_PLATFORM_WINDOWS)
void* Visual::VisualInfo::GetCurrentContext() {
#  if defined(ION_ANGLE)
  return eglGetCurrentContext();
#  else
  return wglGetCurrentContext();
#  endif
}

void Visual::VisualInfo::ClearCurrentContext() {
#  if defined(ION_ANGLE)
  eglMakeCurrent(eglGetDisplay((EGLNativeDisplayType)EGL_DEFAULT_DISPLAY),
                 EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
#  else
  wglMakeCurrent(nullptr, nullptr);
#  endif
}

Visual::Visual(Visual::Type type)
    : visual_(new VisualInfo), type_(type) {
  if (type == kCurrent) {
#  if defined(ION_ANGLE)
    visual_->context = eglGetCurrentContext();
#  else
    // wglMakeCurrent() can bind to an arbitrary device context to the OpenGL
    // rendering context when it is called.  For the purposes of Visual, we
    // assume that we will continue to bind the same device context that is
    // currently bound to the current rendering context.
    visual_->device_context = wglGetCurrentDC();
    visual_->context = wglGetCurrentContext();

    // If we're asked to bind to the current context, then there should exist
    // current device and rendering contexts.
    DCHECK(visual_->device_context);
    DCHECK(visual_->context);
#  endif
    MakeCurrent(this);
  } else {
    const DWORD dwExStyle = 0;
    const LPCTSTR lpClassName = reinterpret_cast<LPCTSTR>(GetIonWindowClass());
    const LPCTSTR lpWindowName = "ION";
    const DWORD dwStyle = 0;
    const int x = CW_USEDEFAULT;
    const int y = CW_USEDEFAULT;
    const int nWidth = CW_USEDEFAULT;
    const int nHeight = CW_USEDEFAULT;
    const HWND hWndParent = nullptr;
    const HMENU hMenu = nullptr;
    const HINSTANCE hInstance = GetModuleHandle(nullptr);
    const LPVOID lpParam = nullptr;
    visual_->window =
        CreateWindowEx(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y,
                       nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
    if (!visual_->window)
      return;

    // |device_context| is retrieved from a window created with CS_OWNDC style,
    // and does not need to be released.
    const HDC device_context = GetDC(visual_->window);
    if (!device_context)
      return;

    PIXELFORMATDESCRIPTOR format_descriptor;
    memset(&format_descriptor, 0, sizeof(format_descriptor));
    format_descriptor.nSize = sizeof(format_descriptor);
    format_descriptor.nVersion = 1;
    format_descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    format_descriptor.iPixelType = PFD_TYPE_RGBA;
    format_descriptor.cColorBits = 24;
    format_descriptor.cAlphaBits = 8;
    format_descriptor.cDepthBits = 8;
    format_descriptor.iLayerType = PFD_MAIN_PLANE;
    const int pixel_format =
        ChoosePixelFormat(device_context, &format_descriptor);
    if (pixel_format == 0)
      return;

    if (!SetPixelFormat(device_context, pixel_format, &format_descriptor))
      return;

#if defined(ION_ANGLE)
    visual_->display = eglGetDisplay((EGLNativeDisplayType)device_context);
    if (visual_->display == EGL_NO_DISPLAY)
      if ((visual_->display = eglGetDisplay(
          (EGLNativeDisplayType) EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY)
          return;

    visual_->InitEgl(type);

#else
    visual_->context = wglCreateContext(device_context);
    if (!visual_->context)
      return;
    if (type == kShare) {
      const HGLRC share_context = wglGetCurrentContext();
      wglShareLists(share_context, visual_->context);
    }
    visual_->device_context = device_context;
#endif
  }
}

Visual::Visual() : visual_(new VisualInfo), type_(kMock) {}

bool Visual::IsValid() const { return visual_->context; }

bool Visual::MakeCurrent() const {
  bool succeeded = false;
#  if defined(ION_ANGLE)
  static const EGLBoolean kFailed = EGL_FALSE;
  const EGLBoolean success = eglMakeCurrent(visual_->display, visual_->surface,
                                            visual_->surface, visual_->context);
#  else
  static const BOOL kFailed = FALSE;
  const BOOL success =
      wglMakeCurrent(visual_->device_context, visual_->context);
#  endif
  if (success == kFailed) {
    LOG(ERROR) << "Unable to make Visual current.";
    return false;
  } else {
    return true;
  }
}

#if defined(ION_ANGLE)
void Visual::TeardownContextNew() {
  eglDestroyContext(visual_->display, visual_->context);
  eglDestroySurface(visual_->display, visual_->surface);
  eglTerminate(visual_->display);
}

void Visual::TeardownContextShared() {
  TeardownContextNew();
}
#else
void Visual::TeardownContextNew() {
  if (visual_->context)
    wglDeleteContext(visual_->context);
  // |visual_->device_context| was created from a window created with the
  // CS_OWNED style, and does not need to be released here.
}

void Visual::TeardownContextShared() {
  TeardownContextNew();
}
#endif

Visual::~Visual() {
  TeardownVisual(this);

  if (visual_->window) {
    DestroyWindow(visual_->window);
  }
}

#elif !defined(ION_PLATFORM_IOS) && !defined(ION_PLATFORM_MAC)
void* Visual::VisualInfo::GetCurrentContext() { return NULL; }
void Visual::VisualInfo::ClearCurrentContext() {}

// For platforms that we cannot (or do not know how to) create contexts.
// iOS and Mac have their own objective-c implementations in visual_darwin.mm.
Visual::Visual(Type type) : visual_(new VisualInfo), type_(type) {}
Visual::Visual() : visual_(new VisualInfo), type_(kMock) {}
bool Visual::IsValid() const { return false; }
void Visual::MakeCurrent() const {}
void Visual::TeardownContextNew() {}
void Visual::TeardownContextShared() {}
Visual::~Visual() {
  TeardownVisual(this);
}
#endif

#if !defined(ION_PLATFORM_IOS) && !defined(ION_PLATFORM_MAC)
void Visual::UpdateId() {
  SetId(reinterpret_cast<size_t>(reinterpret_cast<void*>(visual_->context)));
}
#endif

int Visual::GetGlVersion() const {
  // glGetIntegerv(GL_MAJOR_VERSION) is (surprisingly) not supported on all
  // platforms (e.g. mac), so we use the GL_VERSION string instead.
  const char* version_string = NULL;
  const char* dot_string = NULL;

  // Try to get the local OpenGL version by looking for major.minor in the
  // version string.
  version_string = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  if (!version_string) {
    LOG(WARNING) << "This system does not seem to support OpenGL.";
    return 0;
  }
  if (ion::base::StartsWith(version_string, "WebGL"))
    version_string = "2.0";
  dot_string = strchr(version_string, '.');
  if (!dot_string) {
    LOG(WARNING) << "Unable to determine the OpenGL version.";
    return 0;
  }

  int major = 0;
  int minor = 0;
  major = dot_string[-1] - '0';
  minor = dot_string[1] - '0';
  return major * 10 + minor;
}

void Visual::RefreshCurrentVisual() {
#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_ASMJS) || \
    defined(ION_PLATFORM_GENERIC_ARM) || defined(ION_ANGLE) || \
    (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))
  const Visual* visual = GetCurrent();
  if (visual->type_ != kMock) {
    visual->visual_->surface = eglGetCurrentSurface(EGL_DRAW);
  }
#endif
}

}  // namespace portgfx
}  // namespace ion
