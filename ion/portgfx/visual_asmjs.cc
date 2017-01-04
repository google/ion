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
#include "ion/portgfx/glheaders.h"
#include "ion/portgfx/visual.h"
#include "ion/portgfx/visual_egl_base.h"

namespace ion {
namespace portgfx {
namespace {

// Emscripten does not permit the creation of multiple EGL contexts, so
// we use 1 as a placeholder for the one and only valid context.
static const uintptr_t kInvalidContext = 0;
static const uintptr_t kValidContext = 1ull;

// This class wraps an asm.js EGL context in an ion::portgfx::Visual
// implementation.
class VisualAsmjs : public VisualEglBase {
 public:
  explicit VisualAsmjs(bool is_is_owned_context)
      : VisualEglBase(is_is_owned_context) {}

  // Visual implementation.
  void* GetProcAddress(const char* proc_name, bool is_core) const override {
    for (const char* suffix : {"", "OES"}) {
      const std::string full_name = std::string(proc_name) + suffix;
      void* func =
          reinterpret_cast<void*>(eglGetProcAddress(full_name.c_str()));
      if (func != nullptr) {
        return func;
      }
    }
    return nullptr;
  }

  VisualPtr CreateVisualInShareGroupImpl(const VisualSpec& spec) override {
    // Currently this platform only supports the default VisualSpec.
    DCHECK(spec.backbuffer_width == 1 && spec.backbuffer_height == 1);
    base::SharedPtr<VisualAsmjs> visual(new VisualAsmjs(true));
    if (!visual->InitOwned(this, spec)) {
      visual.Reset();
    }
    return visual;
  }

  // VisualEglBase implementation.
  EGLSurface EglCreateSurface(EGLDisplay display, EGLConfig config, int width,
                              int height) const override {
    return eglCreateWindowSurface(display, config, 0, nullptr);
  }
  EGLContext EglGetCurrentContext() const override {
    return reinterpret_cast<EGLContext>(Visual::GetCurrentGlContextId());
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
VisualPtr Visual::CreateVisual(const VisualSpec& spec) {
  base::SharedPtr<VisualAsmjs> visual(new VisualAsmjs(true));
  if (!visual->InitOwned(nullptr, spec)) {
    visual.Reset();
  }
  return visual;
}

// static
VisualPtr Visual::CreateWrappingVisual() {
  base::SharedPtr<VisualAsmjs> visual(new VisualAsmjs(false));
  if (!visual->InitWrapped()) {
    visual.Reset();
  }
  return visual;
}

// static
uintptr_t Visual::GetCurrentGlContextId() {
  return EM_ASM_INT_V({return !!Module.ctx}) ? kValidContext : kInvalidContext;
}

}  // namespace portgfx
}  // namespace ion
