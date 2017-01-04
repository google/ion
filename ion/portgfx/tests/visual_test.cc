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

#include "ion/portgfx/visual.h"

#include "ion/base/logging.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(Visual, Visual) {
  // Get the current GL context for coverage.
  ion::portgfx::Visual::GetCurrent();
  // Get an ID without a Visual for coverage.
  ion::portgfx::Visual::GetCurrentId();

  // Create an initial context.
  ion::portgfx::VisualPtr visual = ion::portgfx::Visual::CreateVisual();
  if (!visual) {
    LOG(INFO) << "Unable to create an OpenGL context. This test "
              << "cannot run and will now exit.";
    return;
  }

  ion::portgfx::Visual::MakeCurrent(visual);
  const uintptr_t id = ion::portgfx::Visual::GetCurrentId();
  const uintptr_t share_group_id = visual->GetShareGroupId();
  if (visual->IsValid()) {
    EXPECT_EQ(visual, ion::portgfx::Visual::GetCurrent());
    EXPECT_NE(0U, id);
  } else {
    EXPECT_EQ(0U, id);
  }
  EXPECT_EQ(id, visual->GetId());

  // Make another non-shared visual
  ion::portgfx::VisualPtr unshared_visual =
      ion::portgfx::Visual::CreateVisual();
  if (unshared_visual && unshared_visual->IsValid()) {
    EXPECT_EQ(visual, ion::portgfx::Visual::GetCurrent());
    EXPECT_NE(unshared_visual, ion::portgfx::Visual::GetCurrent());
    EXPECT_NE(id, unshared_visual->GetId());
    EXPECT_NE(unshared_visual->GetId(), ion::portgfx::Visual::GetCurrentId());
    EXPECT_NE(share_group_id, unshared_visual->GetShareGroupId());
    EXPECT_NE(0U, unshared_visual->GetShareGroupId());
  }

  // Share the context.
  ion::portgfx::VisualPtr share_visual =
      ion::portgfx::Visual::CreateVisualInCurrentShareGroup();
  // Creating the visual doesn't make it current.
  if (share_visual && share_visual->IsValid()) {
    EXPECT_EQ(visual, ion::portgfx::Visual::GetCurrent());
    EXPECT_NE(share_visual, ion::portgfx::Visual::GetCurrent());
    EXPECT_NE(id, share_visual->GetId());
    EXPECT_NE(share_visual->GetId(), ion::portgfx::Visual::GetCurrentId());
    EXPECT_EQ(share_group_id, share_visual->GetShareGroupId());

    ion::portgfx::Visual::MakeCurrent(share_visual);
    const uintptr_t new_id = ion::portgfx::Visual::GetCurrentId();
    EXPECT_EQ(share_visual, ion::portgfx::Visual::GetCurrent());
    EXPECT_EQ(new_id, share_visual->GetId());
    EXPECT_NE(0U, new_id);
  }

  // Create another share context in the same group.
  ion::portgfx::VisualPtr share_visual2 =
      ion::portgfx::Visual::CreateVisualInCurrentShareGroup();
  if (share_visual2) {
    // Creating the visual doesn't make it current.
    EXPECT_EQ(share_visual, ion::portgfx::Visual::GetCurrent());
    EXPECT_NE(share_visual2, ion::portgfx::Visual::GetCurrent());
    EXPECT_NE(id, share_visual2->GetId());
    EXPECT_NE(share_visual2->GetId(), ion::portgfx::Visual::GetCurrentId());
    EXPECT_EQ(share_group_id, share_visual2->GetShareGroupId());

    ion::portgfx::Visual::MakeCurrent(share_visual2);
    const uintptr_t new_id2 = ion::portgfx::Visual::GetCurrentId();
    EXPECT_EQ(share_visual2, ion::portgfx::Visual::GetCurrent());
    EXPECT_EQ(new_id2, share_visual2->GetId());
    EXPECT_EQ(share_group_id, share_visual2->GetShareGroupId());

    // Clearing a non-current Visual should clear the OpenGL context.
    ion::portgfx::Visual::CleanupThread();
    EXPECT_EQ(ion::portgfx::VisualPtr(), ion::portgfx::Visual::GetCurrent());
    EXPECT_NE(new_id2, ion::portgfx::Visual::GetCurrentId());
    ion::portgfx::Visual::MakeCurrent(share_visual2);
    EXPECT_EQ(new_id2, ion::portgfx::Visual::GetCurrentId());
    EXPECT_EQ(share_group_id, share_visual2->GetShareGroupId());
    ion::portgfx::Visual::MakeCurrent(ion::portgfx::VisualPtr());
  }
}

TEST(Visual, GetProcAddress) {
  // OpenGL requires a context to be current for addresses to be looked up.
  ion::portgfx::VisualPtr visual = ion::portgfx::Visual::CreateVisual();
  ion::portgfx::Visual::MakeCurrent(visual);
  if (!visual || !visual->IsValid()) {
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
    LOG(INFO) << "This system reports having OpenGL version " << major << "."
              << minor << ", but Ion requires OpenGL >= 2.0.  This test "
              << "cannot run and will now exit.";
    return;
  }

  // We can only test functions in the Core group.
  EXPECT_FALSE(visual->GetProcAddress("glActiveTexture", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glAttachShader", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBindAttribLocation", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBindBuffer", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBindFramebuffer", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBindRenderbuffer", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBindTexture", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBlendColor", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBlendEquation", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBlendEquationSeparate", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBlendFunc", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBlendFuncSeparate", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBufferData", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glBufferSubData", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCheckFramebufferStatus", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glClear", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glClearColor", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glClearDepthf", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glClearStencil", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glColorMask", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCompileShader", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCompressedTexImage2D", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCompressedTexSubImage2D", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCopyTexImage2D", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCopyTexSubImage2D", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCreateProgram", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCreateShader", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glCullFace", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDeleteBuffers", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDeleteFramebuffers", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDeleteProgram", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDeleteRenderbuffers", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDeleteShader", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDeleteTextures", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDepthFunc", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDepthMask", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDepthRangef", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDetachShader", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDisable", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDisableVertexAttribArray", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDrawArrays", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glDrawElements", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glEnable", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glEnableVertexAttribArray", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glFinish", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glFlush", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glFramebufferRenderbuffer", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glFramebufferTexture2D", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glFrontFace", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGenBuffers", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGenerateMipmap", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGenFramebuffers", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGenRenderbuffers", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGenTextures", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetActiveAttrib", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetActiveUniform", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetAttachedShaders", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetAttribLocation", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetBooleanv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetBufferParameteriv", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetError", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetFloatv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetFramebufferAttachmentParameteriv",
                                      true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetIntegerv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetProgramInfoLog", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetProgramiv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetRenderbufferParameteriv", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetShaderInfoLog", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetShaderiv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetShaderPrecisionFormat", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetShaderSource", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetString", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetTexParameterfv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetTexParameteriv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetUniformfv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetUniformiv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetUniformLocation", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetVertexAttribfv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetVertexAttribiv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glGetVertexAttribPointerv", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glHint", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glIsBuffer", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glIsEnabled", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glIsFramebuffer", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glIsProgram", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glIsRenderbuffer", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glIsShader", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glIsTexture", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glLineWidth", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glLinkProgram", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glPixelStorei", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glPolygonOffset", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glReadPixels", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glReleaseShaderCompiler", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glRenderbufferStorage", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glSampleCoverage", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glScissor", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glShaderBinary", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glShaderSource", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glStencilFunc", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glStencilFuncSeparate", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glStencilMask", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glStencilMaskSeparate", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glStencilOp", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glStencilOpSeparate", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glTexImage2D", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glTexParameterf", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glTexParameterfv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glTexParameteri", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glTexParameteriv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glTexSubImage2D", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform1f", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform1fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform1i", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform1iv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform2f", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform2fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform2i", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform2iv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform3f", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform3fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform3i", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform3iv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform4f", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform4fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform4i", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniform4iv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniformMatrix2fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniformMatrix3fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUniformMatrix4fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glUseProgram", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glValidateProgram", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttrib1f", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttrib1fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttrib2f", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttrib2fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttrib3f", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttrib3fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttrib4f", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttrib4fv", true) == nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glVertexAttribPointer", true) ==
               nullptr);
  EXPECT_FALSE(visual->GetProcAddress("glViewport", true) == nullptr);

  // Mesa-based OpenGL implementations will return a non-nullptr result when
  // passed any "well-formed" function name ("gl..."), so use something else
  // here so the test passes on all machines.
  EXPECT_TRUE(visual->GetProcAddress("NoSuchFunction", true) == nullptr);
}
