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

#if defined(ION_PLATFORM_MAC)
#  include <OpenGL/OpenGL.h>  // For CGLContext.
#endif

#include "ion/portgfx/setswapinterval.h"

#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace portgfx {

// Setting the swap interval is very platform-dependent, so we define a wrapper
// function for it here. It does nothing on iOS and Android. An interval of 0
// is equivalent to disabling vsync, while an interval of 1 is equivalent to
// enabling vsync. Higher values are also possible.
//
// iOS only supports setting vsync through a CADisplayLink that is only
// available where explicitly created in obj-c code, usually where the GL
// context is created . We could force applications to pass their CADisplayLink
// here, but that leads to a platform-specific interface, and the CADisplayLink
// itself already has a single function to change the swap interval.
//
// There are also numerous reports that eglSwapInterval does nothing on many
// Android devices.
bool SetSwapInterval(int interval) {
  GlContextPtr gl_context = GlContext::GetCurrent();
  if (!gl_context) {
    return false;
  }
  if (interval < 0) {
    return false;
  }
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_ANDROID) || \
    defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL) || \
    defined(ION_GOOGLE_INTERNAL)
  return true;
#elif defined(ION_GFX_OGLES20) || defined(ION_ANGLE)
  EGLDisplay display = eglGetCurrentDisplay();
  return display != EGL_NO_DISPLAY &&
      eglSwapInterval(display, interval) == EGL_TRUE;
#elif defined(ION_PLATFORM_LINUX)
  typedef int (ION_APIENTRY *SwapIntervalProc)(int interval);
  SwapIntervalProc vsync_func = reinterpret_cast<SwapIntervalProc>(
      gl_context->GetProcAddress("glXSwapIntervalSGI", 0));
  return vsync_func && vsync_func(interval) == 0;
#elif defined(ION_PLATFORM_MAC)
  if (CGLContextObj context = CGLGetCurrentContext()) {
    CGLSetParameter(context, kCGLCPSwapInterval, &interval);
    int new_interval = -1;
    CGLGetParameter(context, kCGLCPSwapInterval, &new_interval);
    return interval == new_interval;
  }
  return false;
#elif defined(ION_PLATFORM_WINDOWS)
  typedef BOOL (ION_APIENTRY *SwapIntervalProc)(int interval);
  typedef int (ION_APIENTRY *GetSwapIntervalProc)();
  SwapIntervalProc vsync_set_func = reinterpret_cast<SwapIntervalProc>(
      gl_context->GetProcAddress("wglSwapIntervalEXT", 0));
  GetSwapIntervalProc vsync_get_func = reinterpret_cast<GetSwapIntervalProc>(
      gl_context->GetProcAddress("wglGetSwapIntervalEXT", 0));
  if (vsync_get_func && vsync_set_func) {
    BOOL set_val = vsync_set_func(interval);
    int get_val = vsync_get_func();
    return set_val == TRUE && get_val == interval;
  }
  return false;
#endif
}

}  // namespace portgfx
}  // namespace ion
