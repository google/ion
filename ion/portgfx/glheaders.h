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

#ifndef ION_PORTGFX_GLHEADERS_H_
#define ION_PORTGFX_GLHEADERS_H_

// The GL header files in the opengl/ subdirectory may not be compatible with
// the system headers on the local host. Therefore, this file must be #included
// before anything that could bring in the system headers. We define a header
// guard that makes subsequent inclusions of those headers harmless.
#if defined(_GL_GL_H_)
#  error "ion/portgfx/glheaders.h must be included before system gl.h"
#else
#  define _GL_GL_H_
#endif

// Include OpenGL headers based on platform.

#if defined(ION_PLATFORM_MAC)
// Mac always has its own header files.
#  include <OpenGL/gl.h>
#  include <OpenGL/glext.h>

// From GL_ARB_debug_output
typedef void (*GLDEBUGPROC)(GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar *message, const void *userParam);

#elif defined(ION_PLATFORM_IOS)
// IOS always has its own header files.
#  include <OpenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>

// From GL_ARB_debug_output
typedef void (GL_APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id,
                                        GLenum severity, GLsizei length,
                                        const GLchar *message,
                                        const void *userParam);

#elif defined(ION_PLATFORM_ANDROID) || \
      defined(ION_GOOGLE_INTERNAL) || \
      defined(ION_PLATFORM_ASMJS) || \
      defined(ION_PLATFORM_NACL) || \
      defined(ION_PLATFORM_QNX) || \
      (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))
#  if !defined(ION_PLATFORM_NACL)
     // When using Linux with EGL, we don't want EGL to import X11 as it does by
     // default.  This can cause conflicts later on.
#if ((defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20)) || \
     defined(ION_GOOGLE_INTERNAL)) &&                            \
    !defined(MESA_EGL_NO_X11_HEADERS)
#      define MESA_EGL_NO_X11_HEADERS
#    endif

#    include <EGL/egl.h>  // NOLINT
#    include <EGL/eglext.h>  // NOLINT
#  endif

#  if (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))
#    undef MESA_EGL_NO_X11_HEADERS
#  endif

#  if __ANDROID_API__ >= 21
#    include <GLES3/gl31.h>
#  elif __ANDROID_API__ >= 18
#    include <GLES3/gl3.h>
#  else
#    include <GLES2/gl2.h>
#    if !defined(GL_GLEXT_PROTOTYPES)
#      define GL_GLEXT_PROTOTYPES
#    endif
#    include <GLES2/gl2ext.h>
#  endif

// From GL_ARB_debug_output
typedef void (GL_APIENTRY *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id,
                                        GLenum severity, GLsizei length,
                                        const GLchar *message,
                                        const void *userParam);

#else
#  if defined(ION_PLATFORM_WINDOWS)
// Use ANGLE since our build machines support only OpenGL 1.1, but have DirectX.
// Some clients may provide a GLES2+EGL implementations for use on Windows.
#if defined(ION_ANGLE) || defined(ION_GFX_OGLES20)
#      include <EGL/egl.h>  // NOLINT
#      include <EGL/eglext.h>  // NOLINT
#    endif
#    if defined(NOGDI)
#      undef NOGDI  // Need to get wgl functions from windows.h.
#    endif
#    include <windows.h>  // NOLINT
#  endif
#  if !defined(GL_GLEXT_PROTOTYPES)
#    define GL_GLEXT_PROTOTYPES  // For glGetString() to be defined.
#  endif
#  include "third_party/GL/gl/include/GL/glcorearb.h"
#  include "third_party/GL/gl/include/GL/glext.h"

// Prevent GLU from being included since it tries to redefine classes as
// structs.
#  define __glu_h__
#  define __gl_h_

#endif

#if defined(ION_PLATFORM_ASMJS) || defined(ION_GOOGLE_INTERNAL)
// Prevent some platforms from including their own gl.h headers.
#  define __gl_h_
#endif

#if defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20)
  typedef double GLdouble;

#elif !defined(__glcorearb_h_) && \
      (!defined(ION_PLATFORM_ANDROID) || (__ANDROID_API__ <= 20))

// Define GLdouble, GLuint64, and GLsync which aren't in OpenGL ES 2.0 spec.
// ... but ARE defined in newer Android NDK platforms, notably the
// ones required for 64-bit Android (API 20).  It was added
// between API 18 and 20; we know 18 needs it and "L" needs it gone.
#  include <stdint.h>
  typedef double GLdouble;
  typedef int64_t GLint64;
  typedef uint64_t GLuint64;
  typedef struct __GLsync* GLsync;
#endif

#include "ion/portgfx/glenums.h"

#endif  // ION_PORTGFX_GLHEADERS_H_
