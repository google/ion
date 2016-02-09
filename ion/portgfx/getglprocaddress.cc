/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_IOS) || \
    defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_QNX) || \
    defined(ION_PLATFORM_GENERIC_ARM) || \
    (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))
#  include <dlfcn.h>  // For dlsym().
#elif defined(ION_PLATFORM_LINUX) && !defined(ION_GFX_OGLES20)
#  define GLCOREARB_PROTOTYPES
#elif defined(ION_PLATFORM_WINDOWS)
#  undef NOGDI        // Need to get wglGetProcAddress() from windows.h.
#  include <windows.h>
#  define GLCOREARB_PROTOTYPES
#endif

#include <cstring>
#include <sstream>
#include <string>

#include "base/macros.h"
#include "ion/base/logging.h"
#include "ion/portgfx/getglprocaddress.h"
#include "ion/portgfx/glheaders.h"

#if defined(ION_PLATFORM_LINUX) && !defined(ION_GFX_OGLES20)
#  include <GL/glx.h>  // NOLINT
#endif

namespace ion {
namespace portgfx {

#if ((defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_MAC) ||       \
      defined(ION_PLATFORM_WINDOWS)) &&                                 \
     !(defined(ION_ANGLE) || defined(ION_GOOGLE_INTERNAL)))
#  define ION_USES_DOUBLE_DEPTH 1
#else
#  define ION_USES_DOUBLE_DEPTH 0
#endif

#if ION_USES_DOUBLE_DEPTH
namespace {
struct GlToEsFunctionMapping {
  const char* name;
  void* func;
};

// Wrappers for the floating-point versions of these functions.
static GLvoid ION_APIENTRY ClearDepthf(GLfloat f) {
  ::glClearDepth(static_cast<double>(f));
}

static GLvoid ION_APIENTRY DepthRangef(GLfloat nearVal, GLfloat farVal) {
  ::glDepthRange(static_cast<double>(nearVal), static_cast<double>(farVal));
}

#define MAP_GL_TO_ES_FUNCTION(name, func) { #name, func }

// Desktop platforms do not always define these.
static const GlToEsFunctionMapping kGlToEsFunctionMap[] = {
  MAP_GL_TO_ES_FUNCTION(glClearDepthf, reinterpret_cast<void*>(ClearDepthf)),
  MAP_GL_TO_ES_FUNCTION(glDepthRangef, reinterpret_cast<void*>(DepthRangef))
};

#undef MAP_GL_TO_ES_FUNCTION

const int kNumGlToEsMappings = arraysize(kGlToEsFunctionMap);

static void* MappedFunction(const char* name) {
  void* func = NULL;
  for (int i = 0; i < kNumGlToEsMappings; ++i) {
    if (strcmp(kGlToEsFunctionMap[i].name, name) == 0)
      func = kGlToEsFunctionMap[i].func;
  }
  return func;
}

};  // namespace
#endif

#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL)
namespace {
struct GlFunctionInfo {
  const char* name;
  void* function;
};

#define BIND_GLES_FUNCTION(name) { #name, reinterpret_cast<void*>(&name) }
// Asmjs and NaCl don't support any way to get function addresses, so we build a
// string-to-pointer table below.
static const GlFunctionInfo kEs2FunctionMap[] = {
  BIND_GLES_FUNCTION(glActiveTexture),
  BIND_GLES_FUNCTION(glAttachShader),
  BIND_GLES_FUNCTION(glBindAttribLocation),
  BIND_GLES_FUNCTION(glBindBuffer),
  BIND_GLES_FUNCTION(glBindFramebuffer),
  BIND_GLES_FUNCTION(glBindRenderbuffer),
  BIND_GLES_FUNCTION(glBindTexture),
  BIND_GLES_FUNCTION(glBlendColor),
  BIND_GLES_FUNCTION(glBlendEquation),
  BIND_GLES_FUNCTION(glBlendEquationSeparate),
  BIND_GLES_FUNCTION(glBlendFunc),
  BIND_GLES_FUNCTION(glBlendFuncSeparate),
  BIND_GLES_FUNCTION(glBufferData),
  BIND_GLES_FUNCTION(glBufferSubData),
  BIND_GLES_FUNCTION(glCheckFramebufferStatus),
  BIND_GLES_FUNCTION(glClear),
  BIND_GLES_FUNCTION(glClearColor),
  BIND_GLES_FUNCTION(glClearDepthf),
  BIND_GLES_FUNCTION(glClearStencil),
  BIND_GLES_FUNCTION(glColorMask),
  BIND_GLES_FUNCTION(glCompileShader),
  BIND_GLES_FUNCTION(glCompressedTexImage2D),
  BIND_GLES_FUNCTION(glCompressedTexSubImage2D),
  BIND_GLES_FUNCTION(glCopyTexImage2D),
  BIND_GLES_FUNCTION(glCopyTexSubImage2D),
  BIND_GLES_FUNCTION(glCreateProgram),
  BIND_GLES_FUNCTION(glCreateShader),
  BIND_GLES_FUNCTION(glCullFace),
  BIND_GLES_FUNCTION(glDeleteBuffers),
  BIND_GLES_FUNCTION(glDeleteFramebuffers),
  BIND_GLES_FUNCTION(glDeleteProgram),
  BIND_GLES_FUNCTION(glDeleteRenderbuffers),
  BIND_GLES_FUNCTION(glDeleteShader),
  BIND_GLES_FUNCTION(glDeleteTextures),
  BIND_GLES_FUNCTION(glDepthFunc),
  BIND_GLES_FUNCTION(glDepthMask),
  BIND_GLES_FUNCTION(glDepthRangef),
  BIND_GLES_FUNCTION(glDetachShader),
  BIND_GLES_FUNCTION(glDisable),
  BIND_GLES_FUNCTION(glDisableVertexAttribArray),
  BIND_GLES_FUNCTION(glDrawArrays),
  BIND_GLES_FUNCTION(glDrawElements),
  BIND_GLES_FUNCTION(glEnable),
  BIND_GLES_FUNCTION(glEnableVertexAttribArray),
  BIND_GLES_FUNCTION(glFinish),
  BIND_GLES_FUNCTION(glFlush),
  BIND_GLES_FUNCTION(glFramebufferRenderbuffer),
  BIND_GLES_FUNCTION(glFramebufferTexture2D),
  BIND_GLES_FUNCTION(glFrontFace),
  BIND_GLES_FUNCTION(glGenBuffers),
  BIND_GLES_FUNCTION(glGenerateMipmap),
  BIND_GLES_FUNCTION(glGenFramebuffers),
  BIND_GLES_FUNCTION(glGenRenderbuffers),
  BIND_GLES_FUNCTION(glGenTextures),
  BIND_GLES_FUNCTION(glGetActiveAttrib),
  BIND_GLES_FUNCTION(glGetActiveUniform),
  BIND_GLES_FUNCTION(glGetAttachedShaders),
  BIND_GLES_FUNCTION(glGetAttribLocation),
  BIND_GLES_FUNCTION(glGetBooleanv),
  BIND_GLES_FUNCTION(glGetBufferParameteriv),
  BIND_GLES_FUNCTION(glGetError),
  BIND_GLES_FUNCTION(glGetFloatv),
  BIND_GLES_FUNCTION(glGetFramebufferAttachmentParameteriv),
  BIND_GLES_FUNCTION(glGetIntegerv),
  BIND_GLES_FUNCTION(glGetProgramInfoLog),
  BIND_GLES_FUNCTION(glGetProgramiv),
  BIND_GLES_FUNCTION(glGetRenderbufferParameteriv),
  BIND_GLES_FUNCTION(glGetShaderInfoLog),
  BIND_GLES_FUNCTION(glGetShaderiv),
  BIND_GLES_FUNCTION(glGetShaderPrecisionFormat),
  BIND_GLES_FUNCTION(glGetShaderSource),
  BIND_GLES_FUNCTION(glGetString),
  BIND_GLES_FUNCTION(glGetTexParameterfv),
  BIND_GLES_FUNCTION(glGetTexParameteriv),
  BIND_GLES_FUNCTION(glGetUniformfv),
  BIND_GLES_FUNCTION(glGetUniformiv),
  BIND_GLES_FUNCTION(glGetVertexAttribfv),
  BIND_GLES_FUNCTION(glGetVertexAttribiv),
  BIND_GLES_FUNCTION(glGetVertexAttribPointerv),
  BIND_GLES_FUNCTION(glGetUniformLocation),
  BIND_GLES_FUNCTION(glHint),
  BIND_GLES_FUNCTION(glIsBuffer),
  BIND_GLES_FUNCTION(glIsEnabled),
  BIND_GLES_FUNCTION(glIsFramebuffer),
  BIND_GLES_FUNCTION(glIsProgram),
  BIND_GLES_FUNCTION(glIsRenderbuffer),
  BIND_GLES_FUNCTION(glIsShader),
  BIND_GLES_FUNCTION(glIsTexture),
  BIND_GLES_FUNCTION(glLineWidth),
  BIND_GLES_FUNCTION(glLinkProgram),
  BIND_GLES_FUNCTION(glPixelStorei),
  BIND_GLES_FUNCTION(glPolygonOffset),
  BIND_GLES_FUNCTION(glReadPixels),
  BIND_GLES_FUNCTION(glReleaseShaderCompiler),
  BIND_GLES_FUNCTION(glRenderbufferStorage),
  BIND_GLES_FUNCTION(glSampleCoverage),
  BIND_GLES_FUNCTION(glScissor),
  BIND_GLES_FUNCTION(glShaderBinary),
  BIND_GLES_FUNCTION(glShaderSource),
  BIND_GLES_FUNCTION(glStencilFunc),
  BIND_GLES_FUNCTION(glStencilFuncSeparate),
  BIND_GLES_FUNCTION(glStencilMask),
  BIND_GLES_FUNCTION(glStencilMaskSeparate),
  BIND_GLES_FUNCTION(glStencilOp),
  BIND_GLES_FUNCTION(glStencilOpSeparate),
  BIND_GLES_FUNCTION(glTexImage2D),
  BIND_GLES_FUNCTION(glTexParameterf),
  BIND_GLES_FUNCTION(glTexParameterfv),
  BIND_GLES_FUNCTION(glTexParameteri),
  BIND_GLES_FUNCTION(glTexParameteriv),
  BIND_GLES_FUNCTION(glTexSubImage2D),
  BIND_GLES_FUNCTION(glUniform1f),
  BIND_GLES_FUNCTION(glUniform1fv),
  BIND_GLES_FUNCTION(glUniform1i),
  BIND_GLES_FUNCTION(glUniform1iv),
  BIND_GLES_FUNCTION(glUniform2f),
  BIND_GLES_FUNCTION(glUniform2fv),
  BIND_GLES_FUNCTION(glUniform2i),
  BIND_GLES_FUNCTION(glUniform2iv),
  BIND_GLES_FUNCTION(glUniform3f),
  BIND_GLES_FUNCTION(glUniform3fv),
  BIND_GLES_FUNCTION(glUniform3i),
  BIND_GLES_FUNCTION(glUniform3iv),
  BIND_GLES_FUNCTION(glUniform4f),
  BIND_GLES_FUNCTION(glUniform4fv),
  BIND_GLES_FUNCTION(glUniform4i),
  BIND_GLES_FUNCTION(glUniform4iv),
  BIND_GLES_FUNCTION(glUniformMatrix2fv),
  BIND_GLES_FUNCTION(glUniformMatrix3fv),
  BIND_GLES_FUNCTION(glUniformMatrix4fv),
  BIND_GLES_FUNCTION(glUseProgram),
  BIND_GLES_FUNCTION(glValidateProgram),
  BIND_GLES_FUNCTION(glVertexAttrib1f),
  BIND_GLES_FUNCTION(glVertexAttrib1fv),
  BIND_GLES_FUNCTION(glVertexAttrib2f),
  BIND_GLES_FUNCTION(glVertexAttrib2fv),
  BIND_GLES_FUNCTION(glVertexAttrib3f),
  BIND_GLES_FUNCTION(glVertexAttrib3fv),
  BIND_GLES_FUNCTION(glVertexAttrib4f),
  BIND_GLES_FUNCTION(glVertexAttrib4fv),
  BIND_GLES_FUNCTION(glVertexAttribPointer),
  BIND_GLES_FUNCTION(glViewport),
};

#undef BIND_GLES_FUNCTION

const int kNumGlFunctions = arraysize(kEs2FunctionMap);

}  // namespace
#endif

namespace {

#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_GENERIC_ARM)
// We duplicate some of EGL's functionality here because, for unsupported
// functions, it returns a pointer to a private gl_unimplemented() function that
// prints an error message rather than returning a NULL pointer. This means
// there is no way to detect if a function is unsupported.
static void* GetAndroidGlLibrary() {
  void* libGLESv2 = NULL;
  if (!libGLESv2) {
    if (FILE* egl_config = fopen("/system/lib/egl/egl.cfg", "r")) {
      char line[256];
      while (fgets(line, 256, egl_config)) {
        int display;
        int implementation;
        char tag[256];
        std::istringstream s(line);
        s >> display >> implementation >> tag;
        if (!s.fail() && tag[0] != '\0') {
          // Use the first non-"android" library, which should be the hardware
          // manufacturer's version.
          if (strcmp(tag, "android")) {
            std::string library("libGLESv2_");
            library += tag;
            library += ".so";
            libGLESv2 = dlopen(library.c_str(), RTLD_NOW);
            break;
          }
        }
      }
      fclose(egl_config);
    }
    // Fallback to the android library.
    if (!libGLESv2)
      libGLESv2 =  dlopen("libGLESv2_android.so", RTLD_NOW);
    // Fallback to the default library.
    if (!libGLESv2)
      libGLESv2 =  dlopen("libGLESv2.so", RTLD_NOW);
  }
  return libGLESv2;
}
#endif

static void* LookupSymbol(const char* name, bool is_core) {
  void* func = NULL;
#if defined(ION_PLATFORM_ASMJS)
  func = reinterpret_cast<void*>(eglGetProcAddress(name));

#elif defined(ION_PLATFORM_NACL)
  for (int i = 0; i < kNumGlFunctions; ++i)
    if (strcmp(kEs2FunctionMap[i].name, name) == 0)
      return kEs2FunctionMap[i].function;

#elif defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_GENERIC_ARM) || \
      (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20))
  static void* libGLESv2 = NULL;
  if (!libGLESv2) {
#  if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_GENERIC_ARM)
    libGLESv2 = GetAndroidGlLibrary();
#  else
    // Try opening specific, currently preferred version before trying the more
    // general version.
    libGLESv2 = dlopen("libGLESv2.so.2", RTLD_NOW);
    if (!libGLESv2)
      libGLESv2 = dlopen("libGLESv2.so", RTLD_NOW);
#  endif
    DCHECK(libGLESv2);
  }
  // We cannot directly trust the return value of eglGetProcAddress() since it
  // may return a wrapper for an unimplemented function. To handle this, we
  // first check for the function in the hardware vendor's library, and only
  // query EGL for the functions we know are available. If the function is
  // actually an EGL extension-related function, however, we _must_ call
  // eglGetProcAddress().
  func = reinterpret_cast<void*>(dlsym(libGLESv2, name));
  if (func || strstr(name, "EGL")) {
    // Core ES2 functions must be used directly, only extensions should go
    // through EGL.
    if (!is_core)
      func = reinterpret_cast<void*>(eglGetProcAddress(name));
  }

#elif defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC) || \
      defined(ION_PLATFORM_QNX)
  func = reinterpret_cast<void*>(dlsym(RTLD_DEFAULT, name));

#elif defined(ION_PLATFORM_LINUX) && !defined(ION_GFX_OGLES20)
  func = reinterpret_cast<void*>(glXGetProcAddressARB(
      reinterpret_cast<const GLubyte*>(name)));

#elif defined(ION_PLATFORM_WINDOWS)
#  if defined(ION_ANGLE)
#    define winGlGetProcAddress eglGetProcAddress
#    define winGlDll "libGLESv2.dll"
#  else
#    define winGlGetProcAddress wglGetProcAddress
#    define winGlDll "opengl32.dll"
#  endif
  func = reinterpret_cast<void*>(winGlGetProcAddress(name));
  if (!func) {
    // If WGL or EGL can't find the address, check directly in the GL library.
    static HMODULE opengl_module = LoadLibrary(winGlDll);
    func = opengl_module ? GetProcAddress(opengl_module, name) : NULL;
  }
#  undef winGlGetProcAddress
#  undef winGlDll

#else
# error No valid platform defined
#endif
  return func;
}

// Returns the address of a function name that may end in a suffix. The last
// string of suffixes must be NULL.
static void* LookupSymbolWithSuffix(const std::string& name,
                                    const char** suffixes, bool is_core) {
  for (const char** suffix = suffixes; *suffix; ++suffix) {
    const std::string function_name = name + *suffix;
    if (void* func = LookupSymbol(function_name.c_str(), is_core))
      return func;
  }
  return NULL;
}

}  // namespace

void* GetGlProcAddress(const char* name, bool is_core) {
  // Asmjs and NaCl have no lookup functions, we use the above static table.
#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL)
  const char* suffixes[] = { "", NULL };
#elif defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_IOS) || \
      defined(ION_PLATFORM_QNX) || defined(ION_PLATFORM_GENERIC_ARM) ||   \
      (defined(ION_PLATFORM_LINUX) && defined(ION_GFX_OGLES20)) ||  \
      (defined(ION_PLATFORM_WINDOWS) && defined(ION_ANGLE))
  const char* suffixes[] =
      { "", "OES", "APPLE", "ARB", "EXT", "KHR", "NV", NULL };
#elif defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_WINDOWS)
  const char* suffixes[] = { "", "ARB", "EXT", "KHR", "NV", NULL };
#elif defined(ION_PLATFORM_MAC)
  // Mac has special APPLE functions that must be used when using a
  // compatibility profile, but must _not_ be used when using a core profile.
  const char* compat_suffixes[] = {"APPLE", "", "ARB", "EXT", "KHR", "NV",
                                   NULL};
  const char* core_suffixes[] = {"", "APPLE", "ARB", "EXT", "KHR", "NV", NULL};

  GLint mask = 0;
  glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
  // The above call could generate an invalid enum if we are not called from a
  // core context. Eat the error.
  glGetError();

  const bool is_core_profile = mask & GL_CONTEXT_CORE_PROFILE_BIT;
  const char** suffixes = is_core_profile ? core_suffixes : compat_suffixes;
#endif
  if (void* func = LookupSymbolWithSuffix(name, suffixes, is_core))
    return func;

#if ION_USES_DOUBLE_DEPTH
  // Check if the function is mapped to a different name.
  return MappedFunction(name);
#else
  return NULL;
#endif
}

#undef ION_USES_DOUBLE_DEPTH

}  // namespace portgfx
}  // namespace ion
