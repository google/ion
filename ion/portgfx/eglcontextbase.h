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

#ifndef ION_PORTGFX_EGLCONTEXTBASE_H_
#define ION_PORTGFX_EGLCONTEXTBASE_H_

#include <memory>

#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace portgfx {

// This class wraps an EGL context in an ion::portgfx::GlContext implementation.
// This implementation exposes some of the EGL calls as virtual functions, so
// that subclasses are able to modify their behaviors.  This implementation
// provides the basis for supporting:
//
// * EGL on Android
// * EGL on Linux
// * EGL on asm.js
// * EGL on ANGLE on Windows
class EglContextBase : public GlContext {
 public:
  ~EglContextBase() override;

  // GlContext implementation.
  bool IsValid() const override;
  void SwapBuffers() override;
  bool MakeContextCurrentImpl() override;
  void ClearCurrentContextImpl() override;
  void RefreshGlContextImpl() override;
  void CleanupThreadImpl() const override;
  bool IsOwned() const override { return is_owned_context_; }

  bool InitOwned(const EglContextBase* shared_context,
                 const GlContextSpec& spec);
  bool InitWrapped();

  // Some platforms require special handling for these entry points.  The
  // default implementations use vanilla EGL.
  virtual EGLDisplay EglGetDisplay(void* native_display) const;
  virtual EGLSurface EglCreateSurface(EGLDisplay display, EGLConfig config,
                                      const GlContextSpec& spec) const = 0;
  virtual EGLSurface EglCreateContext(EGLDisplay display, EGLConfig config,
                                      EGLContext share_context,
                                      EGLint const* attrib_list) const;
  virtual EGLContext EglGetCurrentContext() const;
  virtual EGLBoolean EglMakeCurrent(EGLDisplay display, EGLSurface draw,
                                    EGLSurface read, EGLContext context) const;

 protected:
  explicit EglContextBase(bool is_owned_context);

  // Destroys the EGL context owned by this GlContext, if owned.  Made
  // accessible to subclasses here which need control over the order of
  // destruction.
  void Destroy();

  // The (potentially) owned state.
  EGLSurface surface_;
  EGLContext context_;

  // The unowned state.
  EGLDisplay display_;
  EGLSurface draw_surface_;
  EGLSurface read_surface_;

  // Whether the "owned state" is actually owned.
  const bool is_owned_context_;
};

}  // namespace portgfx
}  // namespace ion

#endif  // ION_PORTGFX_EGLCONTEXTBASE_H_
