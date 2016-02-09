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

#include "ion/portgfx/getglprocaddress.h"

#include "ion/base/logging.h"
#define GLCOREARB_PROTOTYPES  // For glGetString() to be defined.
#include "ion/portgfx/glheaders.h"
#include "ion/portgfx/visual.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(GetGlProcAddress, All) {
  using ion::portgfx::GetGlProcAddress;

  // OpenGL requires a context to be current for addresses to be looked up.
  std::unique_ptr<ion::portgfx::Visual> visual(
      ion::portgfx::Visual::CreateVisual());
  ion::portgfx::Visual::MakeCurrent(visual.get());
  if (!visual->IsValid()) {
    LOG(INFO) << "Unable to create an OpenGL context. This test "
              << "cannot run and will now exit.";
    return;
  }

  // Check that the local OpenGL is at least version 2.0, and if not, print a
  // notification and exit gracefully.
  const int version = visual->GetGlVersion();
  const int major = version / 10;
  const int minor = version % 10;
  if (version < 20) {
    LOG(INFO) << "This system reports having OpenGL version " << major
              << "." << minor << ", but Ion requires OpenGL >= 2.0.  This test "
              << "cannot run and will now exit.";
    return;
  }

  // We can only test functions in the Core group.
  EXPECT_FALSE(GetGlProcAddress("glActiveTexture", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glAttachShader", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBindAttribLocation", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBindBuffer", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBindFramebuffer", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBindRenderbuffer", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBindTexture", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBlendColor", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBlendEquation", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBlendEquationSeparate", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBlendFunc", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBlendFuncSeparate", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBufferData", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glBufferSubData", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCheckFramebufferStatus", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glClear", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glClearColor", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glClearDepthf", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glClearStencil", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glColorMask", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCompileShader", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCompressedTexImage2D", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCompressedTexSubImage2D", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCopyTexImage2D", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCopyTexSubImage2D", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCreateProgram", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCreateShader", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glCullFace", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDeleteBuffers", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDeleteFramebuffers", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDeleteProgram", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDeleteRenderbuffers", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDeleteShader", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDeleteTextures", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDepthFunc", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDepthMask", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDepthRangef", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDetachShader", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDisable", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDisableVertexAttribArray", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDrawArrays", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glDrawElements", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glEnable", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glEnableVertexAttribArray", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glFinish", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glFlush", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glFramebufferRenderbuffer", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glFramebufferTexture2D", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glFrontFace", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGenBuffers", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGenerateMipmap", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGenFramebuffers", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGenRenderbuffers", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGenTextures", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetActiveAttrib", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetActiveUniform", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetAttachedShaders", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetAttribLocation", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetBooleanv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetBufferParameteriv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetError", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetFloatv", true) == NULL);
  EXPECT_FALSE(
      GetGlProcAddress("glGetFramebufferAttachmentParameteriv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetIntegerv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetProgramInfoLog", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetProgramiv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetRenderbufferParameteriv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetShaderInfoLog", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetShaderiv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetShaderPrecisionFormat", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetShaderSource", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetString", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetTexParameterfv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetTexParameteriv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetUniformfv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetUniformiv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetUniformLocation", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetVertexAttribfv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetVertexAttribiv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glGetVertexAttribPointerv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glHint", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glIsBuffer", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glIsEnabled", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glIsFramebuffer", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glIsProgram", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glIsRenderbuffer", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glIsShader", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glIsTexture", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glLineWidth", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glLinkProgram", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glPixelStorei", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glPolygonOffset", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glReadPixels", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glReleaseShaderCompiler", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glRenderbufferStorage", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glSampleCoverage", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glScissor", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glShaderBinary", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glShaderSource", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glStencilFunc", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glStencilFuncSeparate", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glStencilMask", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glStencilMaskSeparate", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glStencilOp", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glStencilOpSeparate", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glTexImage2D", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glTexParameterf", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glTexParameterfv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glTexParameteri", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glTexParameteriv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glTexSubImage2D", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform1f", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform1fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform1i", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform1iv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform2f", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform2fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform2i", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform2iv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform3f", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform3fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform3i", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform3iv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform4f", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform4fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform4i", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniform4iv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniformMatrix2fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniformMatrix3fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUniformMatrix4fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glUseProgram", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glValidateProgram", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttrib1f", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttrib1fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttrib2f", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttrib2fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttrib3f", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttrib3fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttrib4f", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttrib4fv", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glVertexAttribPointer", true) == NULL);
  EXPECT_FALSE(GetGlProcAddress("glViewport", true) == NULL);

  // Mesa-based OpenGL implementations will return a non-NULL result when
  // passed any "well-formed" function name ("gl..."), so use something else
  // here so the test passes on all machines.
  EXPECT_TRUE(GetGlProcAddress("NoSuchFunction", true) == NULL);
}
