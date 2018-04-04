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

#if defined(NOGDI)
#undef NOGDI
#endif
#include <windows.h>
#include <wingdi.h>

#include <stdint.h>

#include <memory>
#include <string>

#include "ion/base/logging.h"
#include "ion/base/sharedptr.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"
#include "ion/portgfx/window_win32.h"

namespace ion {
namespace portgfx {
namespace {

// This class wraps a WGL context in an ion::portgfx::GlContext implementation.
class WglContext : public GlContext {
 public:
  explicit WglContext(bool is_owned_context)
      : hglrc_(nullptr), hdc_(nullptr), is_owned_context_(is_owned_context) {}

  ~WglContext() override {
    if (is_owned_context_) {
      if (hglrc_ != nullptr) {
        wglDeleteContext(hglrc_);
      }
    }

    // Delete the |window_| after any WGL context dependent on it has been
    // deleted.
    window_.reset();
  }

  // GlContext implementation.
  bool IsValid() const override { return (hglrc_ != nullptr); }
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override {
    const bool is_core = flags & kProcAddressCore;
    void* func = GetProcAddressImpl(proc_name, is_core);
    if (func != nullptr) {
      return func;
    }

    // These functions do not appear in core GL until 4.1.
    using std::strcmp;
    if (strcmp(proc_name, "glClearDepthf") == 0) {
      static const PFNGLCLEARDEPTHPROC kClearDepthFunc =
          reinterpret_cast<PFNGLCLEARDEPTHPROC>(
              GetProcAddressImpl("glClearDepth", is_core));
      if (kClearDepthFunc != nullptr) {
        static const auto kClearDepthLambda = [](GLfloat f) {
          return kClearDepthFunc(static_cast<double>(f));
        };
        return reinterpret_cast<void*>(
            static_cast<PFNGLCLEARDEPTHFPROC>(kClearDepthLambda));
      }
    } else if (strcmp(proc_name, "glDepthRangef") == 0) {
      static const PFNGLDEPTHRANGEPROC kDepthRangeFunc =
          reinterpret_cast<PFNGLDEPTHRANGEPROC>(
              GetProcAddressImpl("glDepthRange", is_core));
      if (kDepthRangeFunc != nullptr) {
        static const auto kDepthRangeLambda = [](GLfloat n, GLfloat f) {
          return kDepthRangeFunc(static_cast<double>(n),
                                 static_cast<double>(f));
        };
        return reinterpret_cast<void*>(
            static_cast<PFNGLDEPTHRANGEFPROC>(kDepthRangeLambda));
      }
    }
    return nullptr;
  }
  void SwapBuffers() override {
    if (hdc_) {
      ::SwapBuffers(hdc_);
    }
  }
  bool MakeContextCurrentImpl() override {
    return wglMakeCurrent(hdc_, hglrc_);
  }
  void ClearCurrentContextImpl() override { wglMakeCurrent(nullptr, nullptr); }
  GlContextPtr CreateGlContextInShareGroupImpl(
      const GlContextSpec& spec) override {
    // Currently this platform only supports the default GlContextSpec.
    DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
    base::SharedPtr<WglContext> context(new WglContext(true));
    if (!context->InitOwned(this)) {
      context.Reset();
    }
    return context;
  }
  bool IsOwned() const override { return is_owned_context_; }

  bool InitOwned(const WglContext* shared_context);
  bool InitWrapped();

 private:
  // Internal implementation details for GetProcAddress().
  void* GetProcAddressImpl(const char* proc_name, bool is_core) const {
    static HMODULE gl_module = ::LoadLibraryA("opengl32.dll");

    for (const char* suffix : {"", "ARB", "EXT", "KHR", "NV"}) {
      const std::string full_name = std::string(proc_name) + suffix;
      void* func =
          reinterpret_cast<void*>(wglGetProcAddress(full_name.c_str()));
      if (gl_module && func == nullptr) {
        func =
            static_cast<void*>(::GetProcAddress(gl_module, full_name.c_str()));
      }
      if (func != nullptr) {
        return func;
      }
    }
    return nullptr;
  }

  // The (potentially) owned state.
  // The graphics context.
  HGLRC hglrc_;

  // The Win32 window, if one was created.
  std::unique_ptr<WindowWin32> window_;

  // The unowned state.
  // The device context this implementation will draw to.
  HDC hdc_;

  // Whether the "owned state" is actually owned.
  const bool is_owned_context_;
};

bool WglContext::InitOwned(const WglContext* shared_context) {
  DCHECK(is_owned_context_);

  // Create the WindowWin32.  We will need the device context from this window
  // to create the WGL context.
  window_ = WindowWin32::Create();
  if (window_ == nullptr) {
    LOG(ERROR) << "Failed to create window.";
    return false;
  }
  hdc_ = window_->hdc();

  // Create the WGL context.
  hglrc_ = wglCreateContext(hdc_);
  if (hglrc_ == nullptr) {
    LOG(ERROR) << "Failed to create context.";
    return false;
  }
  if (shared_context != nullptr) {
    if (!wglShareLists(shared_context->hglrc_, hglrc_)) {
      LOG(ERROR) << "Failed to share contexts.";
      return false;
    }
  }

  SetIds(CreateId(),
         (shared_context != nullptr ? shared_context->GetShareGroupId()
                                    : CreateShareGroupId()),
         reinterpret_cast<uintptr_t>(hglrc_));
  return true;
}

bool WglContext::InitWrapped() {
  DCHECK(!is_owned_context_);

  hglrc_ = wglGetCurrentContext();
  if (hglrc_ == nullptr) {
    LOG(ERROR) << "No current context.";
    return false;
  }

  hdc_ = wglGetCurrentDC();
  if (hdc_ == nullptr) {
    LOG(ERROR) << "No current device context.";
    return false;
  }

  SetIds(CreateId(), CreateShareGroupId(), reinterpret_cast<uintptr_t>(hglrc_));
  return true;
}

}  // namespace

// static
GlContextPtr GlContext::CreateGlContext(const GlContextSpec& spec) {
  // Currently this platform only supports the default GlContextSpec.
  DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
  base::SharedPtr<WglContext> context(new WglContext(true));
  if (!context->InitOwned(nullptr)) {
    context.Reset();
  }
  return context;
}

// static
GlContextPtr GlContext::CreateWrappingGlContext() {
  base::SharedPtr<WglContext> context(new WglContext(false));
  if (!context->InitWrapped()) {
    context.Reset();
  }
  return context;
}

// static
uintptr_t GlContext::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>(wglGetCurrentContext());
}

}  // namespace portgfx
}  // namespace ion
