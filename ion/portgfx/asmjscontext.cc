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

#include <EGL/egl.h>
#include <emscripten.h>

#include <string>

#include "ion/base/sharedptr.h"
#include "ion/portgfx/eglcontextbase.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace portgfx {
namespace {

// Emscripten does not permit the creation of multiple EGL contexts, so
// we use 1 as a placeholder for the one and only valid context.
static const uintptr_t kInvalidContext = 0;
static const uintptr_t kValidContext = 1ull;

// This class wraps an asm.js EGL context in an ion::portgfx::GlContext
// implementation.
class AsmjsContext : public EglContextBase {
 public:
  explicit AsmjsContext(bool is_is_owned_context)
      : EglContextBase(is_is_owned_context) {}

  // GlContext implementation.
  void* GetProcAddress(const char* proc_name, uint32_t flags) const override {
    return reinterpret_cast<void*>(eglGetProcAddress(proc_name));
  }

  GlContextPtr CreateGlContextInShareGroupImpl(
      const GlContextSpec& spec) override {
    // Currently this platform only supports the default GlContextSpec.
    DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
    base::SharedPtr<AsmjsContext> context(new AsmjsContext(true));
    if (!context->InitOwned(this, spec)) {
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
    return eglCreateWindowSurface(display, config, 0, nullptr);
  }
  EGLContext EglGetCurrentContext() const override {
    return reinterpret_cast<EGLContext>(GlContext::GetCurrentGlContextId());
  }
  EGLBoolean EglMakeCurrent(EGLDisplay display, EGLSurface draw,
                            EGLSurface read,
                            EGLContext context) const override {
    DCHECK(context == this->EglGetCurrentContext() ||
           context == EGL_NO_CONTEXT);
    return true;
  }
};

}  // namespace

// static
GlContextPtr GlContext::CreateGlContext(const GlContextSpec& spec) {
  base::SharedPtr<AsmjsContext> context(new AsmjsContext(true));
  if (!context->InitOwned(nullptr, spec)) {
    context.Reset();
  }
  return context;
}

// static
GlContextPtr GlContext::CreateWrappingGlContext() {
  base::SharedPtr<AsmjsContext> context(new AsmjsContext(false));
  if (!context->InitWrapped()) {
    context.Reset();
  }
  return context;
}

// static
uintptr_t GlContext::GetCurrentGlContextId() {
  return EM_ASM_INT_V({return !!Module.ctx}) ? kValidContext : kInvalidContext;
}

}  // namespace portgfx
}  // namespace ion
