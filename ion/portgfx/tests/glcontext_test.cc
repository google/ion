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

#include "ion/portgfx/glcontext.h"

#include "ion/base/logchecker.h"
#include "ion/base/logging.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace portgfx {

class GlContextFlagsTest : public ::testing::TestWithParam<uint32_t> {};

TEST(GlContext, GlContext) {
  // Get the current GL context for coverage.
  ion::portgfx::GlContext::GetCurrent();
  // Get an ID without a GlContext for coverage.
  ion::portgfx::GlContext::GetCurrentId();

  // Create an initial context.
  ion::portgfx::GlContextPtr context =
      ion::portgfx::GlContext::CreateGlContext();
  if (!context) {
    LOG(INFO) << "Unable to create an OpenGL context. This test "
              << "cannot run and will now exit.";
    return;
  }

  ion::portgfx::GlContext::MakeCurrent(context);
  const uintptr_t id = ion::portgfx::GlContext::GetCurrentId();
  const uintptr_t share_group_id = context->GetShareGroupId();
  if (context->IsValid()) {
    EXPECT_EQ(context, ion::portgfx::GlContext::GetCurrent());
    EXPECT_NE(0U, id);
  } else {
    EXPECT_EQ(0U, id);
  }
  EXPECT_EQ(id, context->GetId());

  // Make another non-shared context
  ion::portgfx::GlContextPtr unshared_context =
      ion::portgfx::GlContext::CreateGlContext();
  if (unshared_context && unshared_context->IsValid()) {
    EXPECT_EQ(context, ion::portgfx::GlContext::GetCurrent());
    EXPECT_NE(unshared_context, ion::portgfx::GlContext::GetCurrent());
    EXPECT_NE(id, unshared_context->GetId());
    EXPECT_NE(unshared_context->GetId(),
              ion::portgfx::GlContext::GetCurrentId());
    EXPECT_NE(share_group_id, unshared_context->GetShareGroupId());
    EXPECT_NE(0U, unshared_context->GetShareGroupId());
  }

  // Share the context.
  ion::portgfx::GlContextPtr share_context =
      ion::portgfx::GlContext::CreateGlContextInCurrentShareGroup();
  // Creating the context doesn't make it current.
  if (share_context && share_context->IsValid()) {
    EXPECT_EQ(context, ion::portgfx::GlContext::GetCurrent());
    EXPECT_NE(share_context, ion::portgfx::GlContext::GetCurrent());
    EXPECT_NE(id, share_context->GetId());
    EXPECT_NE(share_context->GetId(), ion::portgfx::GlContext::GetCurrentId());
    EXPECT_EQ(share_group_id, share_context->GetShareGroupId());

    ion::portgfx::GlContext::MakeCurrent(share_context);
    const uintptr_t new_id = ion::portgfx::GlContext::GetCurrentId();
    EXPECT_EQ(share_context, ion::portgfx::GlContext::GetCurrent());
    EXPECT_EQ(new_id, share_context->GetId());
    EXPECT_NE(0U, new_id);
  }

  // Create another share context in the same group.
  ion::portgfx::GlContextPtr share_context2 =
      ion::portgfx::GlContext::CreateGlContextInCurrentShareGroup();
  if (share_context2) {
    // Creating the context doesn't make it current.
    EXPECT_EQ(share_context, ion::portgfx::GlContext::GetCurrent());
    EXPECT_NE(share_context2, ion::portgfx::GlContext::GetCurrent());
    EXPECT_NE(id, share_context2->GetId());
    EXPECT_NE(share_context2->GetId(), ion::portgfx::GlContext::GetCurrentId());
    EXPECT_EQ(share_group_id, share_context2->GetShareGroupId());

    ion::portgfx::GlContext::MakeCurrent(share_context2);
    const uintptr_t new_id2 = ion::portgfx::GlContext::GetCurrentId();
    EXPECT_EQ(share_context2, ion::portgfx::GlContext::GetCurrent());
    EXPECT_EQ(new_id2, share_context2->GetId());
    EXPECT_EQ(share_group_id, share_context2->GetShareGroupId());

    // Clearing a non-current GlContext should clear the OpenGL context.
    ion::portgfx::GlContext::CleanupThread();
    EXPECT_EQ(ion::portgfx::GlContextPtr(),
              ion::portgfx::GlContext::GetCurrent());
    EXPECT_NE(new_id2, ion::portgfx::GlContext::GetCurrentId());
    ion::portgfx::GlContext::MakeCurrent(share_context2);
    EXPECT_EQ(new_id2, ion::portgfx::GlContext::GetCurrentId());
    EXPECT_EQ(share_group_id, share_context2->GetShareGroupId());
    ion::portgfx::GlContext::MakeCurrent(ion::portgfx::GlContextPtr());
  }
}

// This is more of a functional test, since we can't actually guarantee that
// buffers have been swapped.
TEST(GlContext, SwapBuffers) {
  // Create an initial context.
  ion::portgfx::GlContextPtr context =
      ion::portgfx::GlContext::CreateGlContext();
  if (!context) {
    LOG(INFO) << "Unable to create an OpenGL context. This test "
              << "cannot run and will now exit.";
    return;
  }

  ion::portgfx::GlContext::MakeCurrent(context);
  if (context->IsValid()) {
    context->SwapBuffers();
  }
}

// This test triggers a ClangTidy warning about function size.
TEST_P(GlContextFlagsTest, GetProcAddress) {  // NOLINT
  // OpenGL requires a context to be current for addresses to be looked up.
  ion::portgfx::GlContextPtr context =
      ion::portgfx::GlContext::CreateGlContext();
  ion::portgfx::GlContext::MakeCurrent(context);
  if (!context || !context->IsValid()) {
    LOG(INFO) << "Unable to create an OpenGL context. This test "
              << "cannot run and will now exit.";
    return;
  }

  // Unit tests on Windows seem to be restricted to pre-OpenGL 2.0
  // functionality, so return early.
#if defined(ION_PLATFORM_WINDOWS)
  return;
#endif

  base::LogChecker log_checker;
  // We can only test functions in the Core group.
  const uint32_t flag = GetParam();
  EXPECT_NE(nullptr, context->GetProcAddress("glActiveTexture", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glAttachShader", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBindAttribLocation", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBindBuffer", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBindFramebuffer", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBindRenderbuffer", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBindTexture", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBlendColor", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBlendEquation", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBlendEquationSeparate", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBlendFunc", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBlendFuncSeparate", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBufferData", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glBufferSubData", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glCheckFramebufferStatus", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glClear", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glClearColor", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glClearDepthf", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glClearStencil", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glColorMask", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glCompileShader", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glCompressedTexImage2D", flag));
  EXPECT_NE(nullptr,
            context->GetProcAddress("glCompressedTexSubImage2D", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glCopyTexImage2D", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glCopyTexSubImage2D", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glCreateProgram", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glCreateShader", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glCullFace", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDeleteBuffers", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDeleteFramebuffers", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDeleteProgram", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDeleteRenderbuffers", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDeleteShader", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDeleteTextures", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDepthFunc", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDepthMask", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDepthRangef", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDetachShader", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDisable", flag));
  EXPECT_NE(nullptr,
            context->GetProcAddress("glDisableVertexAttribArray", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDrawArrays", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glDrawElements", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glEnable", flag));
  EXPECT_NE(nullptr,
            context->GetProcAddress("glEnableVertexAttribArray", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glFinish", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glFlush", flag));
  EXPECT_NE(nullptr,
            context->GetProcAddress("glFramebufferRenderbuffer", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glFramebufferTexture2D", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glFrontFace", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGenBuffers", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGenerateMipmap", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGenFramebuffers", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGenRenderbuffers", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGenTextures", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetActiveAttrib", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetActiveUniform", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetAttachedShaders", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetAttribLocation", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetBooleanv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetBufferParameteriv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetError", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetFloatv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress(
                         "glGetFramebufferAttachmentParameteriv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetIntegerv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetProgramInfoLog", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetProgramiv", flag));
  EXPECT_NE(nullptr,
            context->GetProcAddress("glGetRenderbufferParameteriv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetShaderInfoLog", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetShaderiv", flag));
  EXPECT_NE(nullptr,
            context->GetProcAddress("glGetShaderPrecisionFormat", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetShaderSource", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetString", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetTexParameterfv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetTexParameteriv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetUniformfv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetUniformiv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetUniformLocation", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetVertexAttribfv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glGetVertexAttribiv", flag));
  EXPECT_NE(nullptr,
            context->GetProcAddress("glGetVertexAttribPointerv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glHint", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glIsBuffer", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glIsEnabled", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glIsFramebuffer", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glIsProgram", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glIsRenderbuffer", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glIsShader", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glIsTexture", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glLineWidth", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glLinkProgram", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glPixelStorei", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glPolygonOffset", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glReadPixels", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glReleaseShaderCompiler", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glRenderbufferStorage", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glSampleCoverage", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glScissor", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glShaderBinary", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glShaderSource", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glStencilFunc", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glStencilFuncSeparate", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glStencilMask", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glStencilMaskSeparate", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glStencilOp", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glStencilOpSeparate", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glTexImage2D", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glTexParameterf", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glTexParameterfv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glTexParameteri", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glTexParameteriv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glTexSubImage2D", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform1f", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform1fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform1i", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform1iv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform2f", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform2fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform2i", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform2iv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform3f", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform3fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform3i", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform3iv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform4f", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform4fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform4i", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniform4iv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniformMatrix2fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniformMatrix3fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUniformMatrix4fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glUseProgram", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glValidateProgram", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttrib1f", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttrib1fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttrib2f", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttrib2fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttrib3f", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttrib3fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttrib4f", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttrib4fv", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glVertexAttribPointer", flag));
  EXPECT_NE(nullptr, context->GetProcAddress("glViewport", flag));

  // Mesa-based OpenGL implementations will return a non-nullptr result when
  // passed any "well-formed" function name ("gl..."), so use something else
  // here so the test passes on all machines.
  EXPECT_EQ(nullptr, context->GetProcAddress("NoSuchFunction", flag));

#if defined(ION_PLATFORM_ANDROID)
  // Check that on Android the EXT_debug_marker extensions are not loaded
  // purely, since the Android EGL loader makes them noops.
  if (flag & ion::portgfx::GlContext::kProcAddressPure) {
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }
  // We can't validate that the functions return non-nullptr since emulators
  // don't have them.  We can at least validate that we try to load it the right
  // way, however.
  context->GetProcAddress("glInsertEventMarker", flag);
  EXPECT_TRUE(log_checker.HasMessage("INFO", "Forcing non-pure loading"));
  context->GetProcAddress("glPushGroupMarker", flag);
  EXPECT_TRUE(log_checker.HasMessage("INFO", "Forcing non-pure loading"));
  context->GetProcAddress("glPopGroupMarker", flag);
  EXPECT_TRUE(log_checker.HasMessage("INFO", "Forcing non-pure loading"));
#endif
}

INSTANTIATE_TEST_CASE_P(
    FramePosting, GlContextFlagsTest,
    ::testing::Values(ion::portgfx::GlContext::kProcAddressCore |
                          ion::portgfx::GlContext::kProcAddressPure,
                      ion::portgfx::GlContext::kProcAddressCore));

}  // namespace portgfx
}  // namespace ion
