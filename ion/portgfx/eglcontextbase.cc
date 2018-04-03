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

#include "ion/portgfx/eglcontextbase.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "ion/base/logging.h"
#include "ion/port/environment.h"

// Some EGL header installations (notably: the Android NDK) still do not have
// updated EGL headers that define this bit.
#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x00000040
#endif

namespace ion {
namespace portgfx {

EglContextBase::EglContextBase(bool is_owned_context)
    : surface_(EGL_NO_SURFACE),
      context_(EGL_NO_CONTEXT),
      display_(EGL_NO_DISPLAY),
      draw_surface_(EGL_NO_SURFACE),
      read_surface_(EGL_NO_SURFACE),
      is_owned_context_(is_owned_context) {}

EglContextBase::~EglContextBase() { Destroy(); }

void EglContextBase::Destroy() {
  if (is_owned_context_) {
    if (context_ != EGL_NO_CONTEXT) {
      EGLBoolean success = eglDestroyContext(display_, context_);
      // If EGL context destruction fails, GL resources might be leaked,
      // so this is a good check to perform in non-production builds.
      DCHECK(success) << "eglDestroyContext failed";
      context_ = EGL_NO_CONTEXT;
    }
    if (surface_ != EGL_NO_SURFACE) {
      eglDestroySurface(display_, surface_);
      surface_ = EGL_NO_SURFACE;
      draw_surface_ = EGL_NO_SURFACE;
      read_surface_ = EGL_NO_SURFACE;
    }
  }
}

bool EglContextBase::IsValid() const { return (context_ != EGL_NO_CONTEXT); }

void EglContextBase::SwapBuffers() {
  if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
    eglSwapBuffers(display_, surface_);
  }
}

bool EglContextBase::MakeContextCurrentImpl() {
  return this->EglMakeCurrent(display_, draw_surface_, read_surface_, context_);
}

void EglContextBase::ClearCurrentContextImpl() {
  this->EglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
}

void EglContextBase::RefreshGlContextImpl() {
  draw_surface_ = eglGetCurrentSurface(EGL_DRAW);
  read_surface_ = eglGetCurrentSurface(EGL_READ);
}

void EglContextBase::CleanupThreadImpl() const {
  // The underlying EGL implementation has a small amount of thread-local state
  // (error status, current OpenGL context pointer, etc) needs to be freed.
  eglReleaseThread();
}

bool EglContextBase::InitOwned(const EglContextBase* shared_context,
                               const GlContextSpec& spec) {
  DCHECK(is_owned_context_);

  // Initialize the EGLDisplay.
  display_ = this->EglGetDisplay(reinterpret_cast<void*>(EGL_DEFAULT_DISPLAY));
  if (display_ == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Failed to get EGL display.";
    return false;
  }
  EGLint major = 0;
  EGLint minor = 0;
  if (!eglInitialize(display_, &major, &minor)) {
    LOG(ERROR) << "Failed to initialize EGL.";
    return false;
  }
  if (major < 1 || minor < 2) {
    LOG(ERROR) << "System does not support at least EGL 1.2.";
    return false;
  }

  // Choose the EGL frame buffer configuration.  Prefer a GLES3 context, then
  // fall back to GLES2 if that is unavailable.
  EGLConfig egl_config;
  EGLint num_configs;
  EGLint gles_version = 3;
  static const EGLint kGles3Attributes[] = {
      EGL_BUFFER_SIZE,
      24,
      EGL_DEPTH_SIZE,
      spec.depthbuffer_bit_depth,
      EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
  };
  if (!eglChooseConfig(display_, kGles3Attributes, &egl_config, 1,
                       &num_configs)) {
    gles_version = 2;
    static const EGLint kGles2Attributes[] = {
        EGL_BUFFER_SIZE,
        24,
        EGL_DEPTH_SIZE,
        spec.depthbuffer_bit_depth,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };
    if (!eglChooseConfig(display_, kGles2Attributes, &egl_config, 1,
                         &num_configs)) {
      LOG(ERROR) << "Could not choose EGL config.";
      return false;
    }
  }

  // Create the EGLSurface.
  surface_ = this->EglCreateSurface(display_, egl_config, spec);
  if (surface_ == EGL_NO_SURFACE) {
    LOG(ERROR) << "Failed to create EGL surface.";
    return false;
  }
  draw_surface_ = surface_;
  read_surface_ = surface_;

  // Create the EGLContext.
  static const EGLint kContextAttributes[] = {
      EGL_CONTEXT_CLIENT_VERSION,
      gles_version,
      EGL_NONE,
  };
  const EGLContext egl_context =
      (shared_context != nullptr ? shared_context->context_ : EGL_NO_CONTEXT);
  context_ = this->EglCreateContext(display_, egl_config, egl_context,
                                    kContextAttributes);
  if (context_ == EGL_NO_CONTEXT) {
    LOG(ERROR) << "Failed to create EGL context.";
    return false;
  }

  SetIds(CreateId(),
         (shared_context != nullptr ? shared_context->GetShareGroupId()
                                    : CreateShareGroupId()),
         reinterpret_cast<uintptr_t>(context_));
  return true;
}

bool EglContextBase::InitWrapped() {
  // Some platforms do not support displays (asmjs) so do not error if the
  // display is zero.
  display_ = eglGetCurrentDisplay();
  // Some EGL contexts support binding without a read and/or draw surface (see
  // EGL_KHR_surfaceless_context), so do not error if none of |draw_surface_|
  // and |read_surface_| is valid.
  draw_surface_ = eglGetCurrentSurface(EGL_DRAW);
  read_surface_ = eglGetCurrentSurface(EGL_READ);
  context_ = this->EglGetCurrentContext();
  if (context_ == EGL_NO_CONTEXT) {
    LOG(ERROR) << "No current context.";
    return false;
  }

  SetIds(CreateId(), CreateShareGroupId(),
         reinterpret_cast<uintptr_t>(context_));
  return true;
}

EGLDisplay EglContextBase::EglGetDisplay(void* native_display) const {
  return eglGetDisplay(reinterpret_cast<NativeDisplayType>(native_display));
}

EGLContext EglContextBase::EglGetCurrentContext() const {
  return eglGetCurrentContext();
}

EGLBoolean EglContextBase::EglMakeCurrent(EGLDisplay display, EGLSurface draw,
                                          EGLSurface read,
                                          EGLContext context) const {
  return eglMakeCurrent(display, draw, read, context);
}

EGLSurface EglContextBase::EglCreateContext(EGLDisplay display,
                                            EGLConfig config,
                                            EGLContext share_context,
                                            EGLint const* attrib_list) const {
  return eglCreateContext(display, config, share_context, attrib_list);
}

}  // namespace portgfx
}  // namespace ion
