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

#include <windows.h>

#include <memory>
#include <string>

#include "ion/base/logging.h"
#include "ion/base/sharedptr.h"
#include "ion/portgfx/eglcontextbase.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/window_win32.h"

namespace ion {
namespace portgfx {
namespace {

// This class wraps an ANGLE EGL context in an ion::portgfx::GlContext
// implementation.
class AngleContext : public EglContextBase {
 public:
  explicit AngleContext(bool is_owned_context)
      : EglContextBase(is_owned_context) {}

  ~AngleContext() override {
    // Destroy the EGL context first.
    Destroy();

    // Now destroy the window.
    window_.reset();
  }

  // GlContext implementation.
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override {
    for (const char* suffix : {"", "OES", "APPLE", "ARB", "EXT", "KHR", "NV"}) {
      const std::string full_name = std::string(proc_name) + suffix;
      void* func =
          reinterpret_cast<void*>(eglGetProcAddress(full_name.c_str()));
      if (func == nullptr) {
        // If EGL can't find the address, check directly in the EGL library.
        static HMODULE opengl_module = LoadLibrary("libGLESv2.dll");
        func = (opengl_module != nullptr
                    ? GetProcAddress(opengl_module, full_name.c_str())
                    : nullptr);
      }
      if (func != nullptr) {
        return func;
      }
    }
    return nullptr;
  }

  GlContextPtr CreateGlContextInShareGroupImpl(
      const GlContextSpec& spec) override {
    // Currently this platform only supports the default GlContextSpec.
    DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
    bas::SharedPtr<AngleContext> context(
        new AngleContext(GetShareGroupId(), true));
    if (!context->InitOwned(this)) {
      context.Reset();
    }
    return context;
  }

  // EglContextBase implementation.
  EGLSurface EglCreateSurface(EGLDisplay display, EGLConfig config,
                              const GlContextSpec& spec) const override {
    if (spec.native_window) {
      auto window = reinterpret_cast<NativeWindowType>(spec.native_window);
      return eglCreateWindowSurface(display, config, window, nullptr);
    }

    const EGLint pbuffer_attributes[] = {
        EGL_WIDTH, spec.backbuffer_width, EGL_HEIGHT, spec.backbuffer_height,
        EGL_NONE,
    };
    return eglCreatePbufferSurface(display, config, pbuffer_attributes);
  }

  EGLDisplay EglGetDisplay(NativeDisplayType native_display) const override {
    DCHECK(is_owned_context_);
    if (!is_owned_context_) {
      // This GlContext should always own its context if this path is called; if
      // not, the GlContext should have called eglGetCurrentDisplay() instead.
      return EGL_NO_DISPLAY;
    }
    const HDC hdc = window_->hdc();
    if (native_display == EGL_DEFAULT_DISPLAY && hdc != nullptr) {
      // Try creating the EGLDisplay using the stored device context first.
      const EGLDisplay display = eglGetDisplay(hdc);
      if (display != EGL_NO_DISPLAY) {
        return display;
      }
    }
    return eglGetDisplay(native_display);
  }

  bool InitOwned(const AngleContext* shared_context);

 private:
  // The (potentially) owned state.
  // The Win32 window, if one was created.
  std::unique_ptr<WindowWin32> window_;

  // Whether this GlContext owns the underlying GL context.
  const bool is_owned_context_;
};

bool AngleContext::InitOwned(const AngleContext* shared_context) {
  DCHECK(is_owned_context_);

  window_ = WindowWin32::Create();
  if (window_ == nullptr) {
    LOG(ERROR) << "Failed to create window.";
    return false;
  }

  return EglContextBase::InitOwned(shared_context);
}

}  // namespace

// static
GlContextPtr GlContext::CreateGlContext(const GlContextSpec& spec) {
  // Currently this platform only supports the default GlContextSpec.
  DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
  base::SharedPtr<AngleContext> context(
      new AngleContext(CreateShareGroupId(), true));
  if (!context->InitOwned(nullptr)) {
    context.Reset();
  }
  return context;
}

// static
GlContextPtr GlContext::CreateWrappingGlContext() {
  base::SharedPtr<GlContext> context(
      new AngleContext(CreateShareGroupId(), false));
  if (!context->InitWrapped()) {
    context.Reset();
  }
  return context;
}

// static
uintptr_t GlContext::GetCurrentGlContextId() {
  return reinterpret_cast<uintptr_t>(eglGetCurrentContext());
}

}  // namespace portgfx
}  // namespace ion
