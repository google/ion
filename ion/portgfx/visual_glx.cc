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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glx.h>
#include <X11/Xlib.h>

#include <cstring>
#include <memory>
#include <regex>  // NOLINT
#include <string>
#include <unordered_set>

#include "base/casts.h"
#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/sharedptr.h"
#include "ion/port/environment.h"
#include "ion/port/mutex.h"
#include "ion/portgfx/glheaders.h"
#include "ion/portgfx/visual.h"

namespace ion {
namespace portgfx {
namespace {

// Create a GLXContext using glXCreateContextAttribsARB().
GLXContext GlxCreateContextAttribsArb(Display* display, int x_screen,
                                      GLXContext share_context,
                                      const VisualSpec& spec) {
  // Explicitly use the glXGetProcAddressARB() entry point, instead of
  // VisualGlx::GetProcAddressImpl(), since we want exactly the entry point
  // named "glXCreateContextAttribsARB", with the "ARB" suffix.  Some drivers
  // (notably Nvidia) will return a non-null stub function when
  // glXGetProcAddress() is queried for any name that starts with "gl" --
  // whether the entry point actually exists or not.
  const PFNGLXCREATECONTEXTATTRIBSARBPROC glx_create_context_attribs_arb =
      reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(glXGetProcAddressARB(
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
  static const auto free_fb_config = [](GLXFBConfig* config) { XFree(config); };
  std::unique_ptr<GLXFBConfig, decltype(free_fb_config)> fb_config(
      glXChooseFBConfig(display, x_screen, fb_attributes, &num_fb_configs),
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
                            XVisualInfo* visual_info, const VisualSpec& spec) {
  if (spec.debug_context_enabled) {
    LOG(WARNING)
        << "glXCreateContext() does not support debug context creation";
  }
  GLXContext context =
      glXCreateContext(display, visual_info, share_context, True);
  if (context == nullptr) {
    LOG(ERROR) << "glXCreateContext() failed";
    return nullptr;
  }
  return context;
}

// This class wraps a GLX context in an ion::portgfx::Visual implementation.
class VisualGlx : public Visual {
 public:
  explicit VisualGlx(bool is_owned_context)
      : display_(nullptr),
        colormap_(0),
        window_(0),
        context_(nullptr),
        drawable_(0),
        is_owned_context_(is_owned_context) {}

  ~VisualGlx() override {
    if (is_owned_context_) {
      if (context_ != nullptr) {
        glXDestroyContext(display_, context_);
      }
      if (window_ != 0) {
        XDestroyWindow(display_, window_);
      }
      if (colormap_ != 0) {
        XFreeColormap(display_, colormap_);
      }
      if (display_ != nullptr) {
        XCloseDisplay(display_);
      }
    }
  }

  // Visual implementation.
  bool IsValid() const override { return (context_ != nullptr); }
  void* GetProcAddress(const char* proc_name, bool is_core) const override {
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
  bool MakeContextCurrentImpl() override {
    return glXMakeCurrent(display_, drawable_, context_);
  }
  void ClearCurrentContextImpl() override {
    glXMakeCurrent(display_, 0, nullptr);
  }
  VisualPtr CreateVisualInShareGroupImpl(const VisualSpec& spec) override {
    base::SharedPtr<VisualGlx> visual(new VisualGlx(true));
    if (!visual->InitOwned(spec, this)) {
      visual.Reset();
    }
    return visual;
  }
  bool IsOwned() const override { return is_owned_context_; }

  bool InitOwned(const VisualSpec& spec, const VisualGlx* shared_visual);
  bool InitWrapped();

 private:
  // Internal implementation details for GetProcAddress().
  static void* GetProcAddressImpl(const char* proc_name) {
    // On GLX, the implementation may assume that the C string passed to
    // glXGetProcAddressARB is a string literal, and store the pointer
    // internally for use in future string comparisons.  This means that we have
    // to store all looked up names in a persistent data structure.
    static std::unordered_set<std::string> s_lookup_strings;
    static ion::port::Mutex s_lookup_strings_mutex;

    for (const char* suffix : {"", "ARB", "EXT", "KHR", "NV"}) {
      const std::string& full_name_ref =
          [](std::string full_name) -> const std::string& {
        ion::base::LockGuard lock(&s_lookup_strings_mutex);
        return *s_lookup_strings.emplace(std::move(full_name)).first;
      }(std::string(proc_name) + suffix);
      void* func = reinterpret_cast<void*>(glXGetProcAddressARB(
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

bool VisualGlx::InitOwned(const VisualSpec& spec,
                          const VisualGlx* shared_visual) {
  DCHECK(is_owned_context_);
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
    display_ = XOpenDisplay(display_name.c_str());

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

  if (!glXQueryExtension(display_, nullptr, nullptr)) {
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
  static const auto free_visual_info = [](XVisualInfo* info) { XFree(info); };
  std::unique_ptr<XVisualInfo, decltype(free_visual_info)> info(
      glXChooseVisual(display_, x_screen, attributes), free_visual_info);
  if (info == nullptr) {
    LOG(ERROR) << "Failed to choose GLX visual.";
    return false;
  }

  // Create a colormap for the X window.
  colormap_ = XCreateColormap(display_, RootWindow(display_, info->screen),
                              info->visual, AllocNone);
  XSetWindowAttributes window_attributes;
  window_attributes.border_pixel = 0;
  window_attributes.colormap = colormap_;

  // Create the X window.
  window_ = XCreateWindow(display_, RootWindow(display_, info->screen), 0, 0,
                          spec.backbuffer_width, spec.backbuffer_height, 0,
                          info->depth, InputOutput, info->visual,
                          CWBorderPixel | CWColormap, &window_attributes);

  if (window_ == 0) {
    LOG(ERROR) << "Failed to create window.";
    return false;
  }
  drawable_ = window_;

  // Create the GLX context.
  const GLXContext shared_context =
      (shared_visual != nullptr ? shared_visual->context_ : nullptr);
  context_ =
      GlxCreateContextAttribsArb(display_, info->screen, shared_context, spec);
  if (context_ == nullptr) {
    context_ = GlxCreateContext(display_, shared_context, info.get(), spec);
  }
  if (context_ == nullptr) {
    LOG(ERROR) << "Failed to create GLX context.";
    return false;
  }

  SetIds(CreateId(),
         (shared_visual != nullptr ? shared_visual->GetShareGroupId()
                                   : CreateShareGroupId()),
         reinterpret_cast<uintptr_t>(context_));
  return true;
}

bool VisualGlx::InitWrapped() {
  DCHECK(!is_owned_context_);

  display_ = glXGetCurrentDisplay();
  if (display_ == nullptr) {
    LOG(ERROR) << "No current display.";
    return false;
  }
  drawable_ = glXGetCurrentDrawable();
  if (drawable_ == 0) {
    LOG(ERROR) << "No current drawable.";
    return false;
  }
  context_ = glXGetCurrentContext();
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
VisualPtr Visual::CreateVisual(const VisualSpec& spec) {
  base::SharedPtr<VisualGlx> visual(new VisualGlx(true));
  if (!visual->InitOwned(spec, nullptr)) {
    visual.Reset();
  }
  return visual;
}

// static
VisualPtr Visual::CreateWrappingVisual() {
  base::SharedPtr<VisualGlx> visual(new VisualGlx(false));
  if (!visual->InitWrapped()) {
    visual.Reset();
  }
  return visual;
}

// static
uintptr_t Visual::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>(glXGetCurrentContext());
}

// static
void Visual::CleanupThread() { MakeCurrent(VisualPtr()); }

}  // namespace portgfx
}  // namespace ion
