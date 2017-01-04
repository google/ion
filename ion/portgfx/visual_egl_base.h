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

#ifndef ION_PORTGFX_VISUAL_EGL_BASE_H_
#define ION_PORTGFX_VISUAL_EGL_BASE_H_

#include <memory>

#include "ion/portgfx/glheaders.h"
#include "ion/portgfx/visual.h"

namespace ion {
namespace portgfx {

// This class wraps an EGL context in an ion::portgfx::Visual implementation.
// This implementation exposes some of the EGL calls as virtual functions, so
// that subclasses are able to modify their behaviors.  This implementation
// provides the basis for supporting:
//
// * EGL on Android
// * EGL on Linux
// * EGL on asm.js
// * EGL on ANGLE on Windows
class VisualEglBase : public Visual {
 public:
  ~VisualEglBase() override;

  // Visual implementation.
  bool IsValid() const override;
  bool MakeContextCurrentImpl() override;
  void ClearCurrentContextImpl() override;
  void RefreshVisualImpl() override;
  bool IsOwned() const override { return is_owned_context_; }

  bool InitOwned(const VisualEglBase* shared_visual, const VisualSpec& spec);
  bool InitWrapped();

  // Some platforms require special handling for these entry points.  The
  // default implementations use vanilla EGL.
  virtual EGLDisplay EglGetDisplay(NativeDisplayType native_display) const;
  virtual EGLSurface EglCreateSurface(EGLDisplay display, EGLConfig config,
                                      int width, int height) const = 0;
  virtual EGLContext EglGetCurrentContext() const;
  virtual EGLBoolean EglMakeCurrent(EGLDisplay display, EGLSurface draw,
                                    EGLSurface read, EGLContext context) const;

 protected:
  explicit VisualEglBase(bool is_owned_context);

  // Destroys the EGL context owned by this Visual, if owned.  Made accessible
  // to subclasses here which need control over the order of destruction.
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

#endif  // ION_PORTGFX_VISUAL_EGL_BASE_H_
