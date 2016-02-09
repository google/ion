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

#include "ion/gfx/graphicsmanager.h"

#include <vector>

#include "ion/base/logchecker.h"
#include "ion/gfx/tests/mockgraphicsmanager.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/math/range.h"
#include "ion/port/barrier.h"
#include "ion/port/threadutils.h"
#include "ion/portgfx/visual.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

class GraphicsManagerTest : public ::testing::Test {
 protected:
  GraphicsManagerTest() : visual_(portgfx::Visual::CreateVisual()) {}

  void SetUp() override {
#if defined(ION_PLATFORM_NACL) || defined(ION_PLATFORM_ASMJS)
    // NaCl and ASMJS can't access OpenGL without an actual browser.
    mock_visual_.reset(new testing::MockVisual(800, 800));
    mgr_.Reset(new testing::MockGraphicsManager());
#else
    portgfx::Visual::MakeCurrent(visual_.get());
    mgr_.Reset(new GraphicsManager);
#endif
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override {
    mgr_.Reset(NULL);
    mock_visual_.reset();
    portgfx::Visual::MakeCurrent(NULL);
    visual_.reset();
  }

  std::unique_ptr<portgfx::Visual> visual_;
  std::unique_ptr<testing::MockVisual> mock_visual_;
  GraphicsManagerPtr mgr_;
};

class ThreadedGraphicsManagerTest : public ::testing::Test {
 public:
  // Methods to bind to runnable_ in derived tests.
  int CheckExtension(std::string extension_name) {
    return mgr_->IsExtensionSupported(extension_name) ? 1 : 0;
  }

  int CheckFunction(std::string function_name) {
    return mgr_->IsFunctionAvailable(function_name) ? 1 : 0;
  }

  int CheckFunctionGroup(GraphicsManager::FunctionGroupId group) {
    return mgr_->IsFunctionGroupAvailable(group) ? 1 : 0;
  }

  int CheckCapability(GraphicsManager::Capability cap) {
    return mgr_->GetCapabilityValue<int>(cap);
  }

  int CheckRenderer() {
    return static_cast<int>(mgr_->GetGlRenderer().length());
  }

  int CheckVersion() {
    return static_cast<int>(mgr_->GetGlVersion());
  }

  int CheckApi() {
    return static_cast<int>(mgr_->GetGlApiStandard());
  }

  int CheckProfile() {
    return static_cast<int>(mgr_->GetGlProfileType());
  }

 protected:
  ThreadedGraphicsManagerTest()
      : waiter_(2),
        background_function_(
            std::bind(&ThreadedGraphicsManagerTest::ThreadCallback, this)) {}

  void SetUp() override {
    // Spawn a thread which will block on the barrier until the test is ready
    // to continue.
    background_thread_ = port::SpawnThreadStd(&background_function_);
    visual_.reset(new testing::MockVisual(800, 800));
    // MockGraphicsManager is used to ensure stable testing of extensions and
    // bypass problems with Visual taking 6 seconds to fail to construct on
    // some testing environments.
    mgr_.Reset(new testing::MockGraphicsManager());
  }

  void TearDown() override {
    port::JoinThread(background_thread_);
    mgr_.Reset(NULL);
    visual_.reset();
  }

  bool ThreadCallback() {
    // Background thread needs a mock visual.
    testing::MockVisual visual(800, 800);
    waiter_.Wait();
    background_result_ = runnable_();
    waiter_.Wait();
    return true;
  }

  // TODO(user): use SpinBarrier here instead for better thread safety
  // testing.
  port::Barrier waiter_;
  std::function<int()> runnable_;
  std::unique_ptr<testing::MockVisual> visual_;
  GraphicsManagerPtr mgr_;
  int background_result_;
  port::ThreadId background_thread_;
  port::ThreadStdFunc background_function_;
};

// Test class that uses GraphicsManager's enabling mechanism combined with
// MockGraphicsManager's ability to set GL strings.
class DisablingGraphicsManager : public testing::MockGraphicsManager {
 public:
  DisablingGraphicsManager() {}
  void EnableFunctionGroupIfAvailable(
      FunctionGroupId group, const GlVersions& versions,
      const std::string& extensions,
      const std::string& disabled_renderers) override {
    GraphicsManager::EnableFunctionGroupIfAvailable(group, versions, extensions,
                                                    disabled_renderers);
  }

 private:
  ~DisablingGraphicsManager() override {}
};

// Tracing is disabled in production builds.
#if ION_PRODUCTION
#  define VERIFY_TRUE(call)
#else
#  define VERIFY_TRUE(call) EXPECT_TRUE(call)
#endif

TEST_F(GraphicsManagerTest, Capabilities) {
  base::LogChecker log_checker;

  // We use a MockGraphicsManager for deterministic testing here. NaCl and ASMJS
  // already have one created.
#if !defined(ION_PLATFORM_NACL) && !defined(ION_PLATFORM_ASMJS)
  mock_visual_.reset(new testing::MockVisual(800, 800));
  mgr_.Reset(new testing::MockGraphicsManager());
#endif
  testing::TraceVerifier verifier(mgr_.Get());

  EXPECT_EQ(math::Range1f(1.f, 256.f),
            mgr_->GetCapabilityValue<math::Range1f>(
                GraphicsManager::kAliasedLineWidthRange));
  VERIFY_TRUE(verifier.VerifyOneCall("GetFloatv"));
  verifier.Reset();
  EXPECT_EQ(math::Range1f(1.f, 8192.f),
            mgr_->GetCapabilityValue<math::Range1f>(
                GraphicsManager::kAliasedPointSizeRange));
  VERIFY_TRUE(verifier.VerifyOneCall("GetFloatv"));
  verifier.Reset();

  std::vector<int> compressed_formats;
  compressed_formats.push_back(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
  compressed_formats.push_back(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG);
  compressed_formats.push_back(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG);
  compressed_formats.push_back(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG);
  compressed_formats.push_back(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG);
  compressed_formats.push_back(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
  compressed_formats.push_back(GL_ETC1_RGB8_OES);
  EXPECT_EQ(compressed_formats,
            mgr_->GetCapabilityValue<std::vector<int> >(
                GraphicsManager::kCompressedTextureFormats));
  VERIFY_TRUE(verifier.VerifyTwoCalls("GetIntegerv", "GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(GL_UNSIGNED_BYTE,
            mgr_->GetCapabilityValue<int>(
                GraphicsManager::kImplementationColorReadFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(GL_RGB, mgr_->GetCapabilityValue<int>(
                        GraphicsManager::kImplementationColorReadType));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(32, mgr_->GetCapabilityValue<int>(
                    GraphicsManager::kMaxCombinedTextureImageUnits));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(8192, mgr_->GetCapabilityValue<int>(
                      GraphicsManager::kMaxCubeMapTextureSize));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(256, mgr_->GetCapabilityValue<int>(
                     GraphicsManager::kMaxFragmentUniformComponents));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(512, mgr_->GetCapabilityValue<int>(
                     GraphicsManager::kMaxFragmentUniformVectors));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(16, mgr_->GetCapabilityValue<int>(
                    GraphicsManager::kMaxSampleMaskWords));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(4096, mgr_->GetCapabilityValue<int>(
                      GraphicsManager::kMaxRenderbufferSize));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(32, mgr_->GetCapabilityValue<int>(
                    GraphicsManager::kMaxTextureImageUnits));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(16.f, mgr_->GetCapabilityValue<float>(
                      GraphicsManager::kMaxTextureMaxAnisotropy));
  VERIFY_TRUE(verifier.VerifyOneCall("GetFloatv"));
  verifier.Reset();
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(-1, mgr_->GetCapabilityValue<int>(
                    GraphicsManager::kMaxTextureMaxAnisotropy));
  EXPECT_EQ(0U, verifier.GetCallCount());
  verifier.Reset();
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Invalid type requested"));
  EXPECT_EQ(8192,
            mgr_->GetCapabilityValue<int>(GraphicsManager::kMaxTextureSize));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(15,
            mgr_->GetCapabilityValue<int>(GraphicsManager::kMaxVaryingVectors));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(32,
            mgr_->GetCapabilityValue<int>(GraphicsManager::kMaxVertexAttribs));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(32, mgr_->GetCapabilityValue<int>(
                    GraphicsManager::kMaxVertexTextureImageUnits));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(512, mgr_->GetCapabilityValue<int>(
                     GraphicsManager::kMaxVertexUniformComponents));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(1024, mgr_->GetCapabilityValue<int>(
                      GraphicsManager::kMaxVertexUniformVectors));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(math::Range1i(8192, 8192), mgr_->GetCapabilityValue<math::Range1i>(
                                           GraphicsManager::kMaxViewportDims));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  std::vector<int> binary_formats;
  binary_formats.push_back(0xbadf00d);
  EXPECT_EQ(binary_formats, mgr_->GetCapabilityValue<std::vector<int> >(
                                GraphicsManager::kShaderBinaryFormats));
  VERIFY_TRUE(verifier.VerifyTwoCalls("GetIntegerv", "GetIntegerv"));
  verifier.Reset();
  EXPECT_TRUE(GraphicsManager::ShaderPrecision(math::Range1i(127, 127), 23)
            .IsValid());
  EXPECT_FALSE(GraphicsManager::ShaderPrecision(math::Range1i(0, 0), 0)
            .IsValid());

  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(127, 127), 23),
            mgr_->GetCapabilityValue<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kFragmentShaderHighFloatPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(127, 127), 23),
            mgr_->GetCapabilityValue<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kVertexShaderHighIntPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(7, 7), 8),
            mgr_->GetCapabilityValue<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kFragmentShaderLowFloatPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(7, 7), 8),
            mgr_->GetCapabilityValue<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kFragmentShaderLowIntPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(15, 15), 10),
            mgr_->GetCapabilityValue<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kVertexShaderMediumFloatPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(15, 15), 10),
            mgr_->GetCapabilityValue<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kVertexShaderMediumIntPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();

  // Check that values are cached.
  EXPECT_EQ(4096, mgr_->GetCapabilityValue<int>(
                      GraphicsManager::kMaxRenderbufferSize));
  EXPECT_EQ(32, mgr_->GetCapabilityValue<int>(
                    GraphicsManager::kMaxTextureImageUnits));
  EXPECT_EQ(16.f, mgr_->GetCapabilityValue<float>(
                      GraphicsManager::kMaxTextureMaxAnisotropy));
  EXPECT_EQ(15,
            mgr_->GetCapabilityValue<int>(GraphicsManager::kMaxVaryingVectors));
  EXPECT_EQ(0U, verifier.GetCallCount());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // A new GraphicsManager should have its own set of caps.
  testing::MockGraphicsManagerPtr mgr2(
      new testing::MockGraphicsManager());
  testing::TraceVerifier verifier2(mgr2.Get());

  EXPECT_EQ(math::Range1f(1.f, 256.f),
            mgr2->GetCapabilityValue<math::Range1f>(
                GraphicsManager::kAliasedLineWidthRange));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetFloatv"));
  verifier2.Reset();
  EXPECT_EQ(GL_UNSIGNED_BYTE,
            mgr2->GetCapabilityValue<int>(
                GraphicsManager::kImplementationColorReadFormat));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetIntegerv"));
  verifier2.Reset();
  EXPECT_EQ(GL_RGB, mgr2->GetCapabilityValue<int>(
                        GraphicsManager::kImplementationColorReadType));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetIntegerv"));
  verifier2.Reset();
  EXPECT_EQ(32, mgr2->GetCapabilityValue<int>(
                    GraphicsManager::kMaxCombinedTextureImageUnits));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetIntegerv"));
  verifier2.Reset();
  EXPECT_EQ(8192, mgr2->GetCapabilityValue<int>(
                      GraphicsManager::kMaxCubeMapTextureSize));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetIntegerv"));
  verifier2.Reset();
  EXPECT_EQ(512, mgr2->GetCapabilityValue<int>(
                     GraphicsManager::kMaxFragmentUniformVectors));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetIntegerv"));
  verifier2.Reset();
  EXPECT_EQ(512, mgr2->GetCapabilityValue<int>(
                     GraphicsManager::kMaxFragmentUniformVectors));
  EXPECT_EQ(0U, verifier2.GetCallCount());
  EXPECT_EQ(0U, verifier.GetCallCount());
}

TEST_F(GraphicsManagerTest, IsFunctionAvailable) {
  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kCore)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ActiveTexture"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("AttachShader"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BindAttribLocation"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BindBuffer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BindFramebuffer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BindRenderbuffer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BindTexture"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BlendColor"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BlendEquation"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BlendEquationSeparate"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BlendFunc"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BlendFuncSeparate"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BufferData"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BufferSubData"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CheckFramebufferStatus"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Clear"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ClearColor"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ClearDepthf"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ClearStencil"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ColorMask"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CompileShader"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CompressedTexImage2D"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CompressedTexSubImage2D"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CopyTexImage2D"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CopyTexSubImage2D"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CreateProgram"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CreateShader"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("CullFace"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteBuffers"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteFramebuffers"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteProgram"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteRenderbuffers"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteShader"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteTextures"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DepthFunc"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DepthMask"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DepthRangef"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DetachShader"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Disable"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DisableVertexAttribArray"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DrawArrays"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DrawElements"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Enable"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("EnableVertexAttribArray"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Finish"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Flush"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("FramebufferRenderbuffer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("FramebufferTexture2D"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("FrontFace"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GenBuffers"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GenerateMipmap"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GenFramebuffers"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GenRenderbuffers"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GenTextures"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetActiveAttrib"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetActiveUniform"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetAttachedShaders"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetAttribLocation"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetBooleanv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetBufferParameteriv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetError"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetFloatv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable(
        "GetFramebufferAttachmentParameteriv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetIntegerv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetProgramInfoLog"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetProgramiv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetRenderbufferParameteriv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetShaderInfoLog"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetShaderiv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetShaderPrecisionFormat"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetShaderSource"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetString"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetTexParameterfv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetTexParameteriv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetUniformfv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetUniformiv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetUniformLocation"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetVertexAttribfv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetVertexAttribiv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetVertexAttribPointerv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Hint"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsBuffer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsEnabled"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsFramebuffer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsProgram"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsRenderbuffer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsShader"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsTexture"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("LineWidth"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("LinkProgram"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("PixelStorei"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("PolygonOffset"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ReadPixels"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ReleaseShaderCompiler"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("RenderbufferStorage"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("SampleCoverage"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Scissor"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ShaderBinary"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ShaderSource"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("StencilFunc"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("StencilFuncSeparate"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("StencilMask"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("StencilMaskSeparate"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("StencilOp"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("StencilOpSeparate"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexImage2D"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexParameterf"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexParameterfv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexParameteri"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexParameteriv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexSubImage2D"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform1f"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform1fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform1i"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform1iv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform2f"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform2fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform2i"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform2iv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform3f"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform3fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform3i"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform3iv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform4f"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform4fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform4i"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Uniform4iv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("UniformMatrix2fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("UniformMatrix3fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("UniformMatrix4fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("UseProgram"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ValidateProgram"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttrib1f"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttrib1fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttrib2f"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttrib2fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttrib3f"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttrib3fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttrib4f"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttrib4fv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttribPointer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("Viewport"));
  }

  EXPECT_FALSE(mgr_->IsFunctionAvailable("NoSuchFunction"));

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kDebugLabel)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetObjectLabel"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("LabelObject"));
    EXPECT_TRUE(mgr_->IsExtensionSupported("debug_label"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("GetObjectLabel") &&
                 mgr_->IsFunctionAvailable("LabelObject") &&
                 mgr_->IsExtensionSupported("debug_label"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kDebugMarker)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("InsertEventMarker"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("PopGroupMarker"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("PushGroupMarker"));
    EXPECT_TRUE(mgr_->IsExtensionSupported("debug_marker"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("InsertEventMarker") &&
                 mgr_->IsFunctionAvailable("PopGroupMarker") &&
                 mgr_->IsFunctionAvailable("PushGroupMarker") &&
                 mgr_->IsExtensionSupported("debug_marker"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kDebugOutput)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DebugMessageControl"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DebugMessageInsert"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DebugMessageCallback"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetDebugMessageLog"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetPointerv"));
    EXPECT_TRUE(mgr_->IsExtensionSupported("debug_output") ||
                mgr_->IsExtensionSupported("debug"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("DebugMessageControl") &&
                 mgr_->IsFunctionAvailable("DebugMessageInsert") &&
                 mgr_->IsFunctionAvailable("DebugMessageCallback") &&
                 mgr_->IsFunctionAvailable("GetDebugMessageLog") &&
                 mgr_->IsFunctionAvailable("GetPointerv") &&
                 (mgr_->IsExtensionSupported("debug_output") ||
                  mgr_->IsExtensionSupported("debug")));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kEglImage)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("EGLImageTargetTexture2DOES"));
    EXPECT_TRUE(
        mgr_->IsFunctionAvailable("EGLImageTargetRenderbufferStorageOES"));
  } else {
    EXPECT_FALSE(
        mgr_->IsFunctionAvailable("EGLImageTargetTexture2DOES") &&
        mgr_->IsFunctionAvailable("EGLImageTargetRenderbufferStorageOES") &&
        mgr_->IsExtensionSupported("image_external"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kMapBuffer)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("MapBuffer"));
  } else {
    // The function group is not available if:
    // - The function is already not available, meaning that the returned
    //   function pointer from portgfx::GetGlProcAddress() was NULL.
    // - None of the extensions that provide this function group are supported.
    // It can happen that a function pointer is valid, but none of the
    // extensions that provide it are available, and the function itself does
    // nothing. This occurs on Mac for several extensions.
    EXPECT_FALSE(mgr_->IsFunctionAvailable("MapBuffer") &&
                 (mgr_->IsExtensionSupported("mapbuffer") ||
                  mgr_->IsExtensionSupported("vertex_buffer_object")));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kMapBufferBase)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetBufferPointerv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("UnmapBuffer"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("GetBufferPointerv") &&
                 mgr_->IsFunctionAvailable("UnmapBuffer"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kMapBufferRange)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("FlushMappedBufferRange"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("MapBufferRange"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("FlushMappedBufferRange") &&
                 mgr_->IsFunctionAvailable("MapBufferRange") &&
                 mgr_->IsExtensionSupported("map_buffer_range"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kPointSize)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("PointSize"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("PointSize"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kSamplerObjects)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BindSampler"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteSamplers"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GenSamplers"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetSamplerParameterfv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetSamplerParameteriv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsSampler"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("SamplerParameterf"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("SamplerParameterfv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("SamplerParameteri"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("SamplerParameteriv"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("BindSampler") &&
                 mgr_->IsFunctionAvailable("DeleteSamplers") &&
                 mgr_->IsFunctionAvailable("GenSamplers") &&
                 mgr_->IsFunctionAvailable("GetSamplerParameterfv") &&
                 mgr_->IsFunctionAvailable("GetSamplerParameteriv") &&
                 mgr_->IsFunctionAvailable("IsSampler") &&
                 mgr_->IsFunctionAvailable("SamplerParameterf") &&
                 mgr_->IsFunctionAvailable("SamplerParameterfv") &&
                 mgr_->IsFunctionAvailable("SamplerParameteri") &&
                 mgr_->IsFunctionAvailable("SamplerParameteriv") &&
                 mgr_->IsExtensionSupported("sampler_objects"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BindVertexArray"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteVertexArrays"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GenVertexArrays"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsVertexArray"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("BindVertexArray") &&
                 mgr_->IsFunctionAvailable("DeleteVertexArrays") &&
                 mgr_->IsFunctionAvailable("GenVertexArrays") &&
                 mgr_->IsFunctionAvailable("IsVertexArray") &&
                 mgr_->IsExtensionSupported("vertex_array_object"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kTextureMultisample)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexImage2DMultisample"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexImage3DMultisample"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetMultisamplefv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("SampleMaski"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("TexImage2DMultisample") &&
                 mgr_->IsFunctionAvailable("TexImage3DMultisample") &&
                 mgr_->IsFunctionAvailable("GetMultisamplefv") &&
                 mgr_->IsFunctionAvailable("SampleMaski") &&
                 mgr_->IsExtensionSupported("texture_multisample"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kChooseBuffer)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DrawBuffer"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ReadBuffer"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("DrawBuffer") &&
                 mgr_->IsFunctionAvailable("ReadBuffer"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kFramebufferBlit)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BlitFramebuffer"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("BlitFramebuffer") &&
                 mgr_->IsExtensionSupported("framebuffer_blit"));
  }

  if (mgr_->IsFunctionGroupAvailable(
      GraphicsManager::kMultisampleFramebufferResolve)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ResolveMultisampleFramebuffer"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("ResolveMultisampleFramebuffer") &&
                 mgr_->IsExtensionSupported("framebuffer_multisample"));
  }

  if (mgr_->IsFunctionGroupAvailable(
      GraphicsManager::kFramebufferMultisample)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("RenderbufferStorageMultisample"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("RenderbufferStorageMultisample") &&
                 mgr_->IsExtensionSupported("framebuffer_multisample"));
  }

  if (mgr_->IsFunctionGroupAvailable(
      GraphicsManager::kTextureStorageMultisample)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexStorage2DMultisample"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TexStorage3DMultisample"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("TexStorage2DMultisample") &&
                 mgr_->IsFunctionAvailable("TexStorage3DMultisample") &&
                 mgr_->IsExtensionSupported("texture_storage_multisample"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kInstancedDrawing)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DrawArraysInstanced"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DrawElementsInstanced"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("VertexAttribDivisor"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("DrawArraysInstanced") &&
                 mgr_->IsFunctionAvailable("DrawElementsInstanced") &&
                 mgr_->IsFunctionAvailable("VertexAttribDivisor") &&
                 mgr_->IsExtensionSupported("instanced_drawing"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kSync)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("FenceSync"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetSynciv"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("WaitSync"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ClientWaitSync"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteSync"));
  } else {
    EXPECT_FALSE(mgr_->IsFunctionAvailable("FenceSync") &&
                 mgr_->IsFunctionAvailable("GetSynciv") &&
                 mgr_->IsFunctionAvailable("WaitSync") &&
                 mgr_->IsFunctionAvailable("ClientWaitSync") &&
                 mgr_->IsFunctionAvailable("DeleteSync") &&
                 mgr_->IsExtensionSupported("sync"));
  }

  if (mgr_->IsFunctionGroupAvailable(GraphicsManager::kTransformFeedback)) {
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BeginTransformFeedback"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("BindTransformFeedback"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("DeleteTransformFeedbacks"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("EndTransformFeedback"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GenTransformFeedbacks"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("GetTransformFeedbackVarying"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("IsTransformFeedback"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("PauseTransformFeedback"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("ResumeTransformFeedback"));
    EXPECT_TRUE(mgr_->IsFunctionAvailable("TransformFeedbackVaryings"));
  } else {
    // The reason why the else case has some specific checking here is that
    // TransformFeedback functions can still be found even if there is no
    // extension for TranaformFeedback function group. The extension will need
    // to be checked specifically here.
    EXPECT_FALSE(mgr_->IsFunctionAvailable("BeginTransformFeedback") &&
                 mgr_->IsFunctionAvailable("BindTransformFeedback") &&
                 mgr_->IsFunctionAvailable("DeleteTransformFeedbacks") &&
                 mgr_->IsFunctionAvailable("EndTransformFeedback") &&
                 mgr_->IsFunctionAvailable("GenTransformFeedbacks") &&
                 mgr_->IsFunctionAvailable("GetTransformFeedbackVarying") &&
                 mgr_->IsFunctionAvailable("IsTransformFeedback") &&
                 mgr_->IsFunctionAvailable("PauseTransformFeedback") &&
                 mgr_->IsFunctionAvailable("ResumeTransformFeedback") &&
                 mgr_->IsFunctionAvailable("TransformFeedbackVaryings") &&
                 mgr_->IsExtensionSupported("transform_feedback"));
  }
}

TEST_F(GraphicsManagerTest, EnableErrorChecking) {
  // Check that default value is correct.
  EXPECT_FALSE(mgr_->IsErrorCheckingEnabled());

  // Check that values change appropriately.
  mgr_->EnableErrorChecking(true);
  EXPECT_TRUE(mgr_->IsErrorCheckingEnabled());
  mgr_->EnableErrorChecking(false);
  EXPECT_FALSE(mgr_->IsErrorCheckingEnabled());
}

TEST_F(GraphicsManagerTest, SetTracingStream) {
  // Check that default value is correct.
  EXPECT_EQ(NULL, mgr_->GetTracingStream());

  // Check that the stream changes appropriately.
  mgr_->SetTracingStream(&std::cout);
  EXPECT_EQ(&std::cout, mgr_->GetTracingStream());
}

TEST_F(GraphicsManagerTest, SetTracingPrefix) {
  // Check that default value is correct.
  EXPECT_EQ("", mgr_->GetTracingPrefix());

  // Check that the stream changes appropriately.
  mgr_->SetTracingPrefix("prefix");
  EXPECT_EQ("prefix", mgr_->GetTracingPrefix());
}

TEST_F(GraphicsManagerTest, DisabledFunctionGroups) {
  mock_visual_.reset(new testing::MockVisual(800, 800));
  DisablingGraphicsManager* disabling_manager = new DisablingGraphicsManager;
  mgr_.Reset(disabling_manager);
  EXPECT_TRUE(mgr_->IsFunctionGroupAvailable(GraphicsManager::kSamplerObjects));
  disabling_manager->SetRendererString("Mali renderer");
  EXPECT_FALSE(
      mgr_->IsFunctionGroupAvailable(GraphicsManager::kSamplerObjects));
  disabling_manager->SetRendererString("Mali-renderer");
  EXPECT_FALSE(
      mgr_->IsFunctionGroupAvailable(GraphicsManager::kSamplerObjects));
  disabling_manager->SetRendererString("Vivante GC1000");
  EXPECT_FALSE(
      mgr_->IsFunctionGroupAvailable(GraphicsManager::kMapBuffer));
  EXPECT_FALSE(
      mgr_->IsFunctionGroupAvailable(GraphicsManager::kMapBufferRange));
  disabling_manager->SetRendererString("VideoCore IV HW");
  EXPECT_FALSE(
      mgr_->IsFunctionGroupAvailable(GraphicsManager::kMapBuffer));
  EXPECT_FALSE(
      mgr_->IsFunctionGroupAvailable(GraphicsManager::kMapBufferRange));
  disabling_manager->SetRendererString("Another renderer");
  EXPECT_TRUE(mgr_->IsFunctionGroupAvailable(GraphicsManager::kSamplerObjects));
}

TEST(MultipleGraphicsManagerTest, MultipleGraphicsManagers) {
#if defined(ION_PLATFORM_NACL) || defined(ION_PLATFORM_ASMJS)
  // NaCl and ASMJS can't access OpenGL without an actual browser.
  // MockVisual must be destroyed last, as the MockGraphicsManager destructor
  // depends on it existing.
  testing::MockVisual visual(800, 800);
  GraphicsManagerPtr mgr1;
  GraphicsManagerPtr mgr2;
  mgr1.Reset(new testing::MockGraphicsManager());
  mgr2.Reset(new testing::MockGraphicsManager());
#else
  GraphicsManagerPtr mgr1;
  GraphicsManagerPtr mgr2;
  mgr1.Reset(new GraphicsManager);
  mgr2.Reset(new GraphicsManager);
#endif
  if (mgr1->IsFunctionGroupAvailable(GraphicsManager::kCore)) {
    EXPECT_TRUE(mgr1->IsFunctionAvailable("ActiveTexture"));
  }
  bool core_available = false;
  if (mgr2->IsFunctionGroupAvailable(GraphicsManager::kCore)) {
    EXPECT_TRUE(mgr2->IsFunctionAvailable("ActiveTexture"));
    core_available = true;
  }
  mgr1.Reset(NULL);
  EXPECT_EQ(
      core_available, mgr2->IsFunctionGroupAvailable(GraphicsManager::kCore));
  if (mgr2->IsFunctionGroupAvailable(GraphicsManager::kCore)) {
    EXPECT_TRUE(mgr2->IsFunctionAvailable("ActiveTexture"));
  }
}

#if !defined(ION_PLATFORM_ASMJS)

TEST_F(ThreadedGraphicsManagerTest, ConcurrentExtensionsPositive) {
  std::string extension_name("debug_label");
  runnable_ = std::bind(
      &ThreadedGraphicsManagerTest::CheckExtension, this, extension_name);
  waiter_.Wait();
  int result = CheckExtension(extension_name);
  waiter_.Wait();
  EXPECT_EQ(1, result);
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentExtensionsNegative) {
  std::string extension_name("no_such_extension!");
  runnable_ = std::bind(
      &ThreadedGraphicsManagerTest::CheckExtension, this, extension_name);
  waiter_.Wait();
  int result = CheckExtension(extension_name);
  waiter_.Wait();
  EXPECT_EQ(0, result);
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentFunctionPositive) {
  std::string function_name("ActiveTexture");
  runnable_ = std::bind(
      &ThreadedGraphicsManagerTest::CheckFunction, this, function_name);
  waiter_.Wait();
  int result = CheckFunction(function_name);
  waiter_.Wait();
  EXPECT_EQ(1, result);
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentFunctionNegative) {
  std::string function_name("no_such_function!");
  runnable_ = std::bind(
      &ThreadedGraphicsManagerTest::CheckFunction, this, function_name);
  waiter_.Wait();
  int result = CheckFunction(function_name);
  waiter_.Wait();
  EXPECT_EQ(0, result);
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentFunctionGroupPositive) {
  GraphicsManager::FunctionGroupId group = GraphicsManager::kCore;
  runnable_ = std::bind(
      &ThreadedGraphicsManagerTest::CheckFunctionGroup, this, group);
  waiter_.Wait();
  int result = CheckFunctionGroup(group);
  waiter_.Wait();
  EXPECT_EQ(1, result);
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentCapability) {
  GraphicsManager::Capability cap = GraphicsManager::kMaxTextureSize;
  runnable_ = std::bind(
      &ThreadedGraphicsManagerTest::CheckCapability, this, cap);
  waiter_.Wait();
  int result = CheckCapability(cap);
  waiter_.Wait();
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentVersion) {
  runnable_ = std::bind(&ThreadedGraphicsManagerTest::CheckVersion, this);
  waiter_.Wait();
  int result = CheckVersion();
  waiter_.Wait();
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentRenderer) {
  runnable_ = std::bind(&ThreadedGraphicsManagerTest::CheckRenderer, this);
  waiter_.Wait();
  int result = CheckRenderer();
  waiter_.Wait();
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentApi) {
  runnable_ = std::bind(&ThreadedGraphicsManagerTest::CheckApi, this);
  waiter_.Wait();
  int result = CheckApi();
  waiter_.Wait();
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentProfile) {
  runnable_ = std::bind(&ThreadedGraphicsManagerTest::CheckProfile, this);
  waiter_.Wait();
  int result = CheckProfile();
  waiter_.Wait();
  EXPECT_EQ(result, background_result_);
}

#endif  // !defined(ION_PLATFORM_ASMJS)

}  // namespace gfx
}  // namespace ion
