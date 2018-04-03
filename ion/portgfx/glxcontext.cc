/**
Copyright 2017 Google Inc. All Rights Reserved.

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

#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glx.h>
#include <X11/Xlib.h>

#include <cstring>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <regex>  // NOLINT
#include <string>
#include <unordered_set>

#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/sharedptr.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/port/environment.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"

// The list of X11 entry points that we'll need.
#define ITERATE_X11_LIST(macro) \
  macro(XCloseDisplay)          \
  macro(XCreateColormap)        \
  macro(XCreateWindow)          \
  macro(XDestroyWindow)         \
  macro(XFree)                  \
  macro(XFreeColormap)          \
  macro(XOpenDisplay)

// The list of GLX entry points that we'll need.
#define ITERATE_GLX_LIST(macro) \
  macro(glXChooseFBConfig)      \
  macro(glXChooseVisual)        \
  macro(glXCreateContext)       \
  macro(glXDestroyContext)      \
  macro(glXGetCurrentContext)   \
  macro(glXGetCurrentDisplay)   \
  macro(glXGetCurrentDrawable)  \
  macro(glXGetProcAddressARB)   \
  macro(glXMakeCurrent)         \
  macro(glXQueryExtension)      \
  macro(glXSwapBuffers)

namespace ion {
namespace portgfx {
namespace {

// X11 and GLX entry points used in GlxContext are dynamically loaded at
// runtime, because test code may run on Linux on a headless (i.e. without X11)
// machine. Such tests do not expect to be able to do any rendering, so we still
// want to be able to link, load, and execute the test binary.  Attempting to
// create a GlxContext will fail but this is to be expected and handled
// gracefully.
struct EntryPoints {
#define ENTRYPOINT_DECL(x) decltype(&::x) x;
  ITERATE_X11_LIST(ENTRYPOINT_DECL)
  ITERATE_GLX_LIST(ENTRYPOINT_DECL)
#undef ENTRYPOINT_DECL
};

EntryPoints* LoadEntryPoints() {
  std::unique_ptr<EntryPoints> entry_points(new EntryPoints());

#define ENTRYPOINT_LOAD(x)                                                  \
  entry_points->x =                                                         \
      reinterpret_cast<decltype(entry_points->x)>(dlsym(RTLD_DEFAULT, #x)); \
  if (!entry_points->x) {                                                   \
    return nullptr;                                                         \
  }

  ITERATE_X11_LIST(ENTRYPOINT_LOAD)
  ITERATE_GLX_LIST(ENTRYPOINT_LOAD)
#undef ENTRYPOINT_LOAD

  return entry_points.release();
}

const EntryPoints* GetEntryPoints() {
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
      const EntryPoints, s_entry_points, LoadEntryPoints());
  return s_entry_points;
}

std::string GetMissingEntryPoints() {
  std::string missing_entry_points;
#define ENTRYPOINT_QUERY(x)                 \
  if (dlsym(RTLD_DEFAULT, #x) == nullptr) { \
    if (!missing_entry_points.empty()) {    \
      missing_entry_points.push_back(' ');  \
    }                                       \
    missing_entry_points.append(#x);        \
  }

  ITERATE_X11_LIST(ENTRYPOINT_QUERY)
  ITERATE_GLX_LIST(ENTRYPOINT_QUERY)
#undef ENTRYPOINT_QUERY

  return missing_entry_points;
}

#undef ITERATE_X11_LIST
#undef ITERATE_GLX_LIST

// Create a GLXContext using glXCreateContextAttribsARB().
GLXContext GlxCreateContextAttribsArb(Display* display, int x_screen,
                                      GLXContext share_context,
                                      const GlContextSpec& spec) {
  // Explicitly use the glXGetProcAddressARB() entry point, instead of
  // GlxContext::GetProcAddressImpl(), since we want exactly the entry point
  // named "glXCreateContextAttribsARB", with the "ARB" suffix.  Some drivers
  // (notably Nvidia) will return a non-null stub function when
  // glXGetProcAddress() is queried for any name that starts with "gl" --
  // whether the entry point actually exists or not.
  const auto entry_points = GetEntryPoints();
  const PFNGLXCREATECONTEXTATTRIBSARBPROC glx_create_context_attribs_arb =
      reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(
          entry_points->glXGetProcAddressARB(
              reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
  if (glx_create_context_attribs_arb == nullptr) {
    LOG(INFO) << "glXCreateContextAttribsARB not supported";
    return nullptr;
  }

  // Choose the FB config.
  const int fb_attributes[] = {
      // clang-format off
      GLX_DOUBLEBUFFER, True,
      GLX_DEPTH_SIZE, spec.depthbuffer_bit_depth,
      None,
      // clang-format on
  };
  int num_fb_configs = 0;
  static const auto free_fb_config = [entry_points](GLXFBConfig* config) {
    entry_points->XFree(config);
  };
  std::unique_ptr<GLXFBConfig, decltype(free_fb_config)> fb_config(
      entry_points->glXChooseFBConfig(display, x_screen, fb_attributes,
                                      &num_fb_configs),
      free_fb_config);
  if (num_fb_configs == 0 || fb_config == nullptr) {
    LOG(ERROR) << "glXChooseFBConfig() failed";
    return nullptr;
  }

  // Create the context.
  const int glx_context_debug_flag =
      (spec.debug_context_enabled ? GLX_CONTEXT_DEBUG_BIT_ARB : 0);
  const int context_attributes[] = {
      // clang-format off
      GLX_RENDER_TYPE, GLX_RGBA_TYPE,
      GLX_CONTEXT_FLAGS_ARB, glx_context_debug_flag,
      None,
      // clang-format on
  };
  GLXContext context = (*glx_create_context_attribs_arb)(
      display, *fb_config, share_context, True, context_attributes);
  if (context == nullptr) {
    LOG(ERROR) << "glXCreateContextAttribsARB() failed";
    return nullptr;
  }
  return context;
}

// Create a GLXContext using glXCreateContext.
GLXContext GlxCreateContext(Display* display, GLXContext share_context,
                            XVisualInfo* visual_info,
                            const GlContextSpec& spec) {
  const auto entry_points = GetEntryPoints();
  if (spec.debug_context_enabled) {
    LOG(WARNING)
        << "glXCreateContext() does not support debug context creation";
  }
  GLXContext context =
      entry_points->glXCreateContext(display, visual_info, share_context, True);
  if (context == nullptr) {
    LOG(ERROR) << "glXCreateContext() failed";
    return nullptr;
  }
  return context;
}

// This class wraps a GLX context in an ion::portgfx::GlContext implementation.
class GlxContext : public GlContext {
 public:
  explicit GlxContext(bool is_owned_context)
      : display_(nullptr),
        colormap_(0),
        window_(0),
        context_(nullptr),
        drawable_(0),
        is_owned_context_(is_owned_context) {}

  ~GlxContext() override {
    if (is_owned_context_) {
      const auto entry_points = GetEntryPoints();
      if (context_ != nullptr) {
        entry_points->glXDestroyContext(display_, context_);
      }
      if (window_ != 0) {
        entry_points->XDestroyWindow(display_, window_);
      }
      if (colormap_ != 0) {
        entry_points->XFreeColormap(display_, colormap_);
      }
      if (display_ != nullptr) {
        entry_points->XCloseDisplay(display_);
      }
    }
  }

  // GlContext implementation.
  bool IsValid() const override { return (context_ != nullptr); }
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override {
    void* func = GetProcAddressImpl(proc_name);
    if (func != nullptr) {
      return func;
    }

    // These functions do not appear in core GL until 4.1.
    using std::strcmp;
    if (strcmp(proc_name, "glClearDepthf") == 0) {
      static const PFNGLCLEARDEPTHPROC kClearDepthFunc =
          reinterpret_cast<PFNGLCLEARDEPTHPROC>(
              GetProcAddressImpl("glClearDepth"));
      if (kClearDepthFunc != nullptr) {
        static const auto kClearDepthLambda = [](GLfloat f) {
          return kClearDepthFunc(static_cast<double>(f));
        };
        return reinterpret_cast<void*>(+kClearDepthLambda);
      }
    } else if (strcmp(proc_name, "glDepthRangef") == 0) {
      static const PFNGLDEPTHRANGEPROC kDepthRangeFunc =
          reinterpret_cast<PFNGLDEPTHRANGEPROC>(
              GetProcAddressImpl("glDepthRange"));
      if (kDepthRangeFunc != nullptr) {
        static const auto kDepthRangeLambda = [](GLfloat n, GLfloat f) {
          return kDepthRangeFunc(static_cast<double>(n),
                                 static_cast<double>(f));
        };
        return reinterpret_cast<void*>(+kDepthRangeLambda);
      }
    }
    return nullptr;
  }
  void SwapBuffers() override {
    if (display_ && drawable_) {
      GetEntryPoints()->glXSwapBuffers(display_, drawable_);
    }
  }
  bool MakeContextCurrentImpl() override {
    return GetEntryPoints()->glXMakeCurrent(display_, drawable_, context_);
  }
  void ClearCurrentContextImpl() override {
    GetEntryPoints()->glXMakeCurrent(display_, 0, nullptr);
  }
  GlContextPtr CreateGlContextInShareGroupImpl(
      const GlContextSpec& spec) override {
    base::SharedPtr<GlxContext> context(new GlxContext(true));
    if (!context->InitOwned(spec, this)) {
      context.Reset();
    }
    return context;
  }
  bool IsOwned() const override { return is_owned_context_; }

  bool InitOwned(const GlContextSpec& spec, const GlxContext* shared_context);
  bool InitWrapped();

 private:
  // Internal implementation details for GetProcAddress().
  static void* GetProcAddressImpl(const char* proc_name) {
    const auto entry_points = GetEntryPoints();
    // On GLX, the implementation may assume that the C string passed to
    // glXGetProcAddressARB is a string literal, and store the pointer
    // internally for use in future string comparisons.  This means that we have
    // to store all looked up names in a persistent data structure.
    static std::unordered_set<std::string> s_lookup_strings;
    static std::mutex s_lookup_strings_mutex;

    for (const char* suffix : {"", "ARB", "EXT", "KHR", "NV"}) {
      const std::string& full_name_ref =
          [](std::string full_name) -> const std::string& {
        std::lock_guard<std::mutex> lock(s_lookup_strings_mutex);
        return *s_lookup_strings.emplace(std::move(full_name)).first;
      }(std::string(proc_name) + suffix);
      void* func = reinterpret_cast<void*>(entry_points->glXGetProcAddressARB(
          reinterpret_cast<const GLubyte*>(full_name_ref.c_str())));
      if (func != nullptr) {
        return func;
      }
    }
    return nullptr;
  }

  // The (potentially) owned state.
  Display* display_;
  Colormap colormap_;
  Window window_;
  GLXContext context_;

  // The unowned state.
  GLXDrawable drawable_;

  // Whether the "owned state" is actually owned.
  const bool is_owned_context_;
};

bool GlxContext::InitOwned(const GlContextSpec& spec,
                           const GlxContext* shared_context) {
  DCHECK(is_owned_context_);

  const auto entry_points = GetEntryPoints();
  if (!entry_points) {
    LOG(ERROR) << "Failed to create GlContext: missing X11/GLX entry points: "
               << GetMissingEntryPoints();
    return false;
  }

  // A direct call to XOpenDisplay() when there is no X server running takes
  // about 6 seconds to timeout.  Avoid this by checking the process list first
  // for an Xorg instance.
  static const int x_return_value = system("pgrep -c '^Xorg$' >& /dev/null");
  static const bool is_x_running = (x_return_value == 0);
  static const bool is_system_working = (x_return_value != -1);
  int x_screen = 0;
  if (is_x_running || !is_system_working) {
    std::string display_name = port::GetEnvironmentVariableValue("DISPLAY");
    if (display_name.empty()) {
      display_name = std::string(":0");
    }
    display_ = entry_points->XOpenDisplay(display_name.c_str());

    // Parse the X screen number from the DISPLAY variable.  The variable is in
    // the format:
    //
    //   hostname:displaynumber.screennumber
    //
    // where hostname defaults to localhost, and screennumber defaults to 0.
    // See: https://www.x.org/archive/X11R6.7.0/doc/X.7.html
    std::smatch m;
    if (!std::regex_match(display_name, m,
                          std::regex("(?:[[:alnum:].\\-]+)?:(?:[[:digit:]]+)"
                                     "(?:\\.([[:digit:]]+)?)?"))) {
      LOG(ERROR) << "failed to parse display_name=\"" << display_name << "\"";
      return false;
    }
    x_screen = m[1].matched ? std::stoi(m[1]) : 0;
  }
  if (display_ == nullptr) {
    LOG(ERROR) << "Failed to get X display.";
    return false;
  }

  if (!entry_points->glXQueryExtension(display_, nullptr, nullptr)) {
    LOG(ERROR) << "X connection does not support GLX.";
    return false;
  }

  // Choose a GLX visual.
  int attributes[] = {
      // clang-format off
      GLX_USE_GL,
      GLX_RGBA,
      GLX_DOUBLEBUFFER,
      GLX_DEPTH_SIZE, spec.depthbuffer_bit_depth,
      None,
      // clang-format on
  };
  static const auto free_visual_info = [entry_points](XVisualInfo* info) {
    entry_points->XFree(info);
  };
  std::unique_ptr<XVisualInfo, decltype(free_visual_info)> info(
      entry_points->glXChooseVisual(display_, x_screen, attributes),
      free_visual_info);
  if (info == nullptr) {
    LOG(ERROR) << "Failed to choose GLX visual.";
    return false;
  }

  // Create a colormap for the X window.
  colormap_ = entry_points->XCreateColormap(
      display_, RootWindow(display_, info->screen), info->visual, AllocNone);
  XSetWindowAttributes window_attributes;
  window_attributes.border_pixel = 0;
  window_attributes.colormap = colormap_;

  // Create the X window.
  window_ = entry_points->XCreateWindow(
      display_, RootWindow(display_, info->screen), 0, 0, spec.backbuffer_width,
      spec.backbuffer_height, 0, info->depth, InputOutput, info->visual,
      CWBorderPixel | CWColormap, &window_attributes);

  if (window_ == 0) {
    LOG(ERROR) << "Failed to create window.";
    return false;
  }
  drawable_ = window_;

  // Create the GLX context.
  const GLXContext glx_context =
      (shared_context != nullptr ? shared_context->context_ : nullptr);
  context_ =
      GlxCreateContextAttribsArb(display_, info->screen, glx_context, spec);
  if (context_ == nullptr) {
    context_ = GlxCreateContext(display_, glx_context, info.get(), spec);
  }
  if (context_ == nullptr) {
    LOG(ERROR) << "Failed to create GLX context.";
    return false;
  }

  SetIds(CreateId(),
         (shared_context != nullptr ? shared_context->GetShareGroupId()
                                    : CreateShareGroupId()),
         reinterpret_cast<uintptr_t>(context_));
  return true;
}

bool GlxContext::InitWrapped() {
  DCHECK(!is_owned_context_);

  const auto entry_points = GetEntryPoints();
  if (!entry_points) {
    LOG(ERROR) << "Failed to create GlContext: missing X11/GLX entry points: "
               << GetMissingEntryPoints();
    return false;
  }

  display_ = entry_points->glXGetCurrentDisplay();
  if (display_ == nullptr) {
    LOG(ERROR) << "No current display.";
    return false;
  }
  drawable_ = entry_points->glXGetCurrentDrawable();
  if (drawable_ == 0) {
    LOG(ERROR) << "No current drawable.";
    return false;
  }
  context_ = entry_points->glXGetCurrentContext();
  if (context_ == nullptr) {
    LOG(ERROR) << "No current context.";
    return false;
  }

  SetIds(CreateId(), CreateShareGroupId(),
         reinterpret_cast<uintptr_t>(context_));
  return true;
}

}  // namespace

// static
GlContextPtr GlContext::CreateGlContext(const GlContextSpec& spec) {
  base::SharedPtr<GlxContext> context(new GlxContext(true));
  if (!context->InitOwned(spec, nullptr)) {
    context.Reset();
  }
  return context;
}

// static
GlContextPtr GlContext::CreateWrappingGlContext() {
  base::SharedPtr<GlxContext> context(new GlxContext(false));
  if (!context->InitWrapped()) {
    context.Reset();
  }
  return context;
}

// static
uintptr_t GlContext::GetCurrentGlContextId() {
  const auto entry_points = GetEntryPoints();
  if (!entry_points) {
    return 0;
  }
  return reinterpret_cast<uintptr_t>(entry_points->glXGetCurrentContext());
}

}  // namespace portgfx
}  // namespace ion
