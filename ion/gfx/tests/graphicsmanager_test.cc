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

#include "ion/gfx/graphicsmanager.h"

#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "ion/base/logchecker.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/gfx/updatestatetable.h"
#include "ion/math/range.h"
#include "ion/port/barrier.h"
#include "ion/portgfx/glcontext.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

class GraphicsManagerTest : public ::testing::Test {
 protected:
  using GlVersions = GraphicsManager::GlVersions;
  void SetUp() override {
    fake_gl_context_ = testing::FakeGlContext::Create(800, 800);
    portgfx::GlContext::MakeCurrent(fake_gl_context_);
    mgr_.Reset(new GraphicsManager);
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override {
    portgfx::GlContext::MakeCurrent(portgfx::GlContextPtr());
  }

  // These functions are not public in GraphicsManager, so we make this class
  // a friend and expose it via these shims.
  bool IsFunctionAvailable(const std::string& function_name) {
    return mgr_->IsFunctionAvailable(function_name);
  }

  bool IsFunctionAvailable(const GraphicsManagerPtr& mgr,
                           const std::string& function_name) {
    return mgr->IsFunctionAvailable(function_name);
  }

  bool CheckSupport(const GlVersions& versions,
                    const std::string& extensions,
                    const std::string& disabled_renderers) {
    return mgr_->CheckSupport(versions, extensions, disabled_renderers);
  }

  portgfx::GlContextPtr fake_gl_context_;
  GraphicsManagerPtr mgr_;
};

class ThreadedGraphicsManagerTest : public ::testing::Test {
 public:
  // Methods to bind to runnable_ in derived tests.
  int CheckExtension(const std::string& extension_name) {
    return mgr_->IsExtensionSupported(extension_name) ? 1 : 0;
  }

  int CheckFunction(const std::string& function_name) {
    return mgr_->IsFunctionAvailable(function_name) ? 1 : 0;
  }

  int CheckFunctionGroup(GraphicsManager::FeatureId group) {
    return mgr_->IsFeatureAvailable(group) ? 1 : 0;
  }

  int GetIntConstant(GraphicsManager::Constant cap) {
    return mgr_->GetConstant<int>(cap);
  }

  int CheckRenderer() {
    return static_cast<int>(mgr_->GetGlRenderer().length());
  }

  int CheckVersion() {
    return static_cast<int>(mgr_->GetGlVersion());
  }

  int CheckFlavor() {
    return static_cast<int>(mgr_->GetGlFlavor());
  }

  int CheckProfile() {
    return static_cast<int>(mgr_->GetGlProfileType());
  }

 protected:
  ThreadedGraphicsManagerTest() : waiter_(2) {}

  void SetUp() override {
    // Spawn a thread which will block on the barrier until the test is ready
    // to continue.
    background_thread_ =
        std::thread(&ThreadedGraphicsManagerTest::ThreadCallback, this);
    gl_context_ = testing::FakeGlContext::Create(800, 800);
    portgfx::GlContext::MakeCurrent(gl_context_);
    // FakeGraphicsManager is used to ensure stable testing of extensions and
    // bypass problems with GlContext taking 6 seconds to fail to construct on
    // some testing environments.
    mgr_.Reset(new testing::FakeGraphicsManager());
  }

  void TearDown() override {
    background_thread_.join();
    mgr_.Reset(nullptr);
    gl_context_.Reset(nullptr);
  }

  bool ThreadCallback() {
    // Background thread needs a fake GL context.
    portgfx::GlContextPtr gl_context = testing::FakeGlContext::Create(800, 800);
    portgfx::GlContext::MakeCurrent(gl_context);
    waiter_.Wait();
    background_result_ = runnable_();
    waiter_.Wait();
    return true;
  }

  // 
  // testing.
  port::Barrier waiter_;
  std::function<int()> runnable_;
  portgfx::GlContextPtr gl_context_;
  GraphicsManagerPtr mgr_;
  int background_result_;
  std::thread background_thread_;
};

// Tracing is disabled in production builds.
#if ION_PRODUCTION
#  define VERIFY_TRUE(call)
#else
#  define VERIFY_TRUE(call) EXPECT_TRUE(call)
#endif

#if !ION_PRODUCTION
TEST(GraphicsManagerDeathTest, NoContext) {
  portgfx::GlContext::MakeCurrent(portgfx::GlContextPtr(nullptr));
  GraphicsManagerPtr mgr;
  EXPECT_DEATH_IF_SUPPORTED(mgr.Reset(new GraphicsManager), "GL context");
}
#endif

TEST_F(GraphicsManagerTest, Capabilities) {
  base::LogChecker log_checker;

  testing::TraceVerifier verifier(mgr_.Get());
  EXPECT_EQ(math::Range1f(1.f, 256.f),
            mgr_->GetConstant<math::Range1f>(
                GraphicsManager::kAliasedLineWidthRange));
  VERIFY_TRUE(verifier.VerifyOneCall("GetFloatv"));
  verifier.Reset();
  EXPECT_EQ(math::Range1f(1.f, 8192.f),
            mgr_->GetConstant<math::Range1f>(
                GraphicsManager::kAliasedPointSizeRange));
  VERIFY_TRUE(verifier.VerifyOneCall("GetFloatv"));
  verifier.Reset();
  EXPECT_EQ(4096, mgr_->GetConstant<int>(
                      GraphicsManager::kMax3dTextureSize));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(4096, mgr_->GetConstant<int>(
                      GraphicsManager::kMaxArrayTextureLayers));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(8, mgr_->GetConstant<int>(
                   GraphicsManager::kMaxClipDistances));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(4, mgr_->GetConstant<int>(
                   GraphicsManager::kMaxColorAttachments));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(96, mgr_->GetConstant<int>(
                    GraphicsManager::kMaxCombinedTextureImageUnits));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(8192, mgr_->GetConstant<int>(
                      GraphicsManager::kMaxCubeMapTextureSize));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(4, mgr_->GetConstant<int>(
                   GraphicsManager::kMaxDrawBuffers));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(1024, mgr_->GetConstant<int>(
                      GraphicsManager::kMaxFragmentUniformComponents));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(256, mgr_->GetConstant<int>(
                     GraphicsManager::kMaxFragmentUniformVectors));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(4096, mgr_->GetConstant<int>(
                      GraphicsManager::kMaxRenderbufferSize));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(16, mgr_->GetConstant<int>(
                    GraphicsManager::kMaxSampleMaskWords));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(16, mgr_->GetConstant<int>(
                    GraphicsManager::kMaxSamples));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(0xFFFFFFFFFFFFFFFF, mgr_->GetConstant<uint64_t>(
                    GraphicsManager::kMaxServerWaitTimeout));
  VERIFY_TRUE(verifier.VerifyOneCall("GetInteger64v"));
  verifier.Reset();
  EXPECT_EQ(32, mgr_->GetConstant<int>(
                    GraphicsManager::kMaxTextureImageUnits));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(16.f, mgr_->GetConstant<float>(
                      GraphicsManager::kMaxTextureMaxAnisotropy));
  VERIFY_TRUE(verifier.VerifyOneCall("GetFloatv"));
  verifier.Reset();
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(-1, mgr_->GetConstant<int>(
                    GraphicsManager::kMaxTextureMaxAnisotropy));
  EXPECT_EQ(0U, verifier.GetCallCount());
  verifier.Reset();
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Invalid type requested"));
  EXPECT_EQ(8192,
            mgr_->GetConstant<int>(GraphicsManager::kMaxTextureSize));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(15,
            mgr_->GetConstant<int>(GraphicsManager::kMaxVaryingVectors));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(32,
            mgr_->GetConstant<int>(GraphicsManager::kMaxVertexAttribs));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(32, mgr_->GetConstant<int>(
                    GraphicsManager::kMaxVertexTextureImageUnits));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(1536, mgr_->GetConstant<int>(
                      GraphicsManager::kMaxVertexUniformComponents));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(384, mgr_->GetConstant<int>(
                     GraphicsManager::kMaxVertexUniformVectors));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(math::Point2i(8192, 8192), mgr_->GetConstant<math::Point2i>(
                                           GraphicsManager::kMaxViewportDims));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  EXPECT_EQ(4, mgr_->GetConstant<int>(
                   GraphicsManager::kMaxViews));
  VERIFY_TRUE(verifier.VerifyOneCall("GetIntegerv"));
  verifier.Reset();
  std::vector<GLenum> binary_formats;
  binary_formats.push_back(0xbadf00d);
  EXPECT_EQ(binary_formats, mgr_->GetConstant<std::vector<GLenum>>(
                                GraphicsManager::kShaderBinaryFormats));
  VERIFY_TRUE(verifier.VerifyTwoCalls("GetIntegerv", "GetIntegerv"));
  verifier.Reset();
  EXPECT_TRUE(GraphicsManager::ShaderPrecision(math::Range1i(127, 127), 23)
            .IsValid());
  EXPECT_FALSE(GraphicsManager::ShaderPrecision(math::Range1i(0, 0), 0)
            .IsValid());

  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(127, 127), 23),
            mgr_->GetConstant<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kFragmentShaderHighFloatPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(127, 127), 23),
            mgr_->GetConstant<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kVertexShaderHighIntPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(7, 7), 8),
            mgr_->GetConstant<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kFragmentShaderLowFloatPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(7, 7), 8),
            mgr_->GetConstant<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kFragmentShaderLowIntPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(15, 15), 10),
            mgr_->GetConstant<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kVertexShaderMediumFloatPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();
  EXPECT_EQ(GraphicsManager::ShaderPrecision(math::Range1i(15, 15), 10),
            mgr_->GetConstant<GraphicsManager::ShaderPrecision>(
                GraphicsManager::kVertexShaderMediumIntPrecisionFormat));
  VERIFY_TRUE(verifier.VerifyOneCall("GetShaderPrecision"));
  verifier.Reset();

  // Check that values are cached.
  EXPECT_EQ(4096, mgr_->GetConstant<int>(
                      GraphicsManager::kMaxRenderbufferSize));
  EXPECT_EQ(32, mgr_->GetConstant<int>(
                    GraphicsManager::kMaxTextureImageUnits));
  EXPECT_EQ(16.f, mgr_->GetConstant<float>(
                      GraphicsManager::kMaxTextureMaxAnisotropy));
  EXPECT_EQ(15,
            mgr_->GetConstant<int>(GraphicsManager::kMaxVaryingVectors));
  EXPECT_EQ(0U, verifier.GetCallCount());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // A new GraphicsManager should have its own set of caps.
  testing::FakeGraphicsManagerPtr mgr2(
      new testing::FakeGraphicsManager());
  testing::TraceVerifier verifier2(mgr2.Get());

  EXPECT_EQ(math::Range1f(1.f, 256.f),
            mgr2->GetConstant<math::Range1f>(
                GraphicsManager::kAliasedLineWidthRange));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetFloatv"));
  verifier2.Reset();
  EXPECT_EQ(96, mgr2->GetConstant<int>(
                    GraphicsManager::kMaxCombinedTextureImageUnits));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetIntegerv"));
  verifier2.Reset();
  EXPECT_EQ(8192, mgr2->GetConstant<int>(
                      GraphicsManager::kMaxCubeMapTextureSize));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetIntegerv"));
  verifier2.Reset();
  EXPECT_EQ(256, mgr2->GetConstant<int>(
                     GraphicsManager::kMaxFragmentUniformVectors));
  VERIFY_TRUE(verifier2.VerifyOneCall("GetIntegerv"));
  verifier2.Reset();
  EXPECT_EQ(256, mgr2->GetConstant<int>(
                     GraphicsManager::kMaxFragmentUniformVectors));
  EXPECT_EQ(0U, verifier2.GetCallCount());
  EXPECT_EQ(0U, verifier.GetCallCount());
}

TEST_F(GraphicsManagerTest, PopulateConstantCache) {
  // Tracing is disabled in production builds.
#if !ION_PRODUCTION
  testing::TraceVerifier verifier(mgr_.Get());
  mgr_->PopulateConstantCache();
  EXPECT_LT(54U, verifier.GetCountOf("GetIntegerv"));
#endif
}

TEST_F(GraphicsManagerTest, IsFunctionAvailable) {
  EXPECT_TRUE(IsFunctionAvailable("CreateShader"));
  EXPECT_TRUE(IsFunctionAvailable("FramebufferTexture2DMultisampleEXT"));
  EXPECT_TRUE(IsFunctionAvailable("GetError"));
  EXPECT_TRUE(IsFunctionAvailable("RenderbufferStorageMultisample"));
  EXPECT_FALSE(IsFunctionAvailable("NoSuchFunction"));
}

TEST_F(GraphicsManagerTest, FeatureDetection) {
  using testing::FakeGraphicsManager;
  FakeGraphicsManager* gm = static_cast<FakeGraphicsManager*>(mgr_.Get());
  gm->SetExtensionsString("GL_EXT_lisp_shaders GL_ARB_unicorn_distillation");
  gm->SetRendererString("Fantasy Renderer");

  gm->SetVersionString("4.0 Ion OpenGL");
  EXPECT_TRUE(CheckSupport(GlVersions(40U, 0U, 0U), "", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 10U, 10U), "", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(40U, 0U, 0U), "", "sy Re"));

  gm->SetVersionString("3.0 Ion OpenGL ES");
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "", "Renderer"));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "", "Fantasy"));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 35U, 0U), "", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 30U, 0U), "", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 20U, 0U), "", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 0U), "lisp_shaders", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "shaders", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "lisp_shader", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 0U), "lisp_shaders",
                           "Nightmare"));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 30U, 0U), "lisp_shaders", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 0U), "EXT_lisp_shaders", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 0U),
                           "EXT_lisp_shaders,KHR_brain_interface", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U),
                            "ARB_lisp_shaders,KHR_brain_interface", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 30U, 0U), "EXT_lisp_shaders", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 35U, 0U), "EXT_lisp_shaders",
                           "Nightmare"));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 30U, 0U), "ARB_lisp_shaders",
                           "Fictional"));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 35U, 0U), "lisp_shaders", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "ARB_lisp_shaders", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 0U),
                           "ARB_lisp_shaders,ARB_unicorn_distillation", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 0U),
                           "ARB_lisp_shaders,ARB_unicorn_distillation,"
                           "EXT_brain_interface", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(30U, 0U, 0U), "", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 30U, 0U), "", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 30U), "", ""));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "lisp_shaders", "Fantasy"));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "lisp_shaders",
                            "Fictional,Nightmare,Fantasy"));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "lisp_shaders",
                            "Fictional,Fantasy,Nightmare"));
  EXPECT_FALSE(CheckSupport(GlVersions(0U, 0U, 0U), "lisp_shaders",
                            "Fantasy Renderer"));

  gm->SetVersionString("2.0 Ion WebGL");
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 0U), "unicorn_distillation", ""));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 30U),
                           "ARB_unicorn_distillation", "Nightmare"));
  EXPECT_TRUE(CheckSupport(GlVersions(0U, 0U, 0U),
                           "ARB_unicorn_distillation", "Nightmare"));
  EXPECT_FALSE(CheckSupport(GlVersions(40U, 30U, 30U),
                            "EXT_unicorn_distillation", "Nightmare"));
  EXPECT_TRUE(CheckSupport(GlVersions(40U, 30U, 20U),
                           "EXT_unicorn_distillation", "Nightmare"));
  EXPECT_TRUE(CheckSupport(GlVersions(40U, 30U, 20U),
                           "KHR_brain_interface", "Nightmare"));
  EXPECT_FALSE(CheckSupport(GlVersions(40U, 30U, 20U),
                            "EXT_lisp_shader,KHR_brain_interface", "Fantasy"));
  EXPECT_FALSE(CheckSupport(GlVersions(40U, 30U, 30U),
                            "EXT_lisp_shader,KHR_brain_interface", ""));
}

TEST_F(GraphicsManagerTest, GetFeatureDebugString) {
  std::string features = mgr_->GetFeatureDebugString();
#if ION_PRODUCTION
  EXPECT_TRUE(features.empty());
#else
  EXPECT_FALSE(features.empty());
  EXPECT_NE(std::string::npos, features.find("kCore: available: yes"));
#endif
}

TEST_F(GraphicsManagerTest, StateCapsEnabled) {
  // Check that state table capabilities are enabled when features are present.
  testing::TraceVerifier verifier(mgr_.Get());

  {
    const bool multisampled = mgr_->IsFeatureAvailable(
        GraphicsManager::kMultisampleCapability);
    StateTablePtr st1(new StateTable(500, 500));
    StateTablePtr st2(new StateTable(500, 500));
    st1->Enable(StateTable::kMultisample, false);
    st2->Enable(StateTable::kMultisample, true);
    UpdateFromStateTable(*st2, st1.Get(), mgr_.Get());
    if (multisampled) {
      VERIFY_TRUE(verifier.VerifyOneCall("Enable(GL_MULTISAMPLE)"));
    } else {
      VERIFY_TRUE(verifier.VerifyNoCalls());
    }
    verifier.Reset();
  }

  {
    const bool debug_output = mgr_->IsFeatureAvailable(
        GraphicsManager::kDebugOutput);
    StateTablePtr st1(new StateTable(500, 500));
    StateTablePtr st2(new StateTable(500, 500));
    st1->Enable(StateTable::kDebugOutputSynchronous, false);
    st2->Enable(StateTable::kDebugOutputSynchronous, true);
    UpdateFromStateTable(*st2, st1.Get(), mgr_.Get());
    if (debug_output) {
      VERIFY_TRUE(
          verifier.VerifyOneCall("Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS)"));
    } else {
      VERIFY_TRUE(verifier.VerifyNoCalls());
    }
    verifier.Reset();
  }

  {
    const bool clip_distances = mgr_->IsFeatureAvailable(
        GraphicsManager::kClipDistance);
    StateTablePtr st1(new StateTable(500, 500));
    StateTablePtr st2(new StateTable(500, 500));
    st1->Enable(StateTable::kClipDistance4, false);
    st2->Enable(StateTable::kClipDistance4, true);
    UpdateFromStateTable(*st2, st1.Get(), mgr_.Get());
    if (clip_distances) {
      VERIFY_TRUE(verifier.VerifyOneCall("Enable(GL_CLIP_DISTANCE4)"));
    } else {
      VERIFY_TRUE(verifier.VerifyNoCalls());
    }
    verifier.Reset();
  }
}

TEST_F(GraphicsManagerTest, EnableErrorChecking) {
  // Check that default value is correct.
#if ION_CHECK_GL_ERRORS
  EXPECT_TRUE(mgr_->IsErrorCheckingEnabled());
#else
  EXPECT_FALSE(mgr_->IsErrorCheckingEnabled());
#endif

  // Check that values change appropriately.
  mgr_->EnableErrorChecking(true);
  EXPECT_TRUE(mgr_->IsErrorCheckingEnabled());
  mgr_->EnableErrorChecking(false);
  EXPECT_FALSE(mgr_->IsErrorCheckingEnabled());
}


TEST_F(GraphicsManagerTest, RendererBlacklisting) {
  // Verify that some features blacklisted by renderer are indeed disabled.
  using testing::FakeGraphicsManager;
  FakeGraphicsManager* gm = static_cast<FakeGraphicsManager*>(mgr_.Get());
  EXPECT_TRUE(gm->IsFeatureAvailable(GraphicsManager::kSamplerObjects));
  gm->SetRendererString("Mali renderer");
  EXPECT_FALSE(gm->IsFeatureAvailable(GraphicsManager::kSamplerObjects));
  gm->SetRendererString("Mali-renderer");
  EXPECT_FALSE(gm->IsFeatureAvailable(GraphicsManager::kSamplerObjects));
  gm->SetRendererString("Vivante GC1000");
  EXPECT_FALSE(gm->IsFeatureAvailable(GraphicsManager::kMapBuffer));
  EXPECT_FALSE(gm->IsFeatureAvailable(GraphicsManager::kMapBufferRange));
  gm->SetRendererString("VideoCore IV HW");
  EXPECT_FALSE(gm->IsFeatureAvailable(GraphicsManager::kMapBuffer));
  EXPECT_FALSE(gm->IsFeatureAvailable(GraphicsManager::kMapBufferRange));
  gm->SetRendererString("Another renderer");
  EXPECT_TRUE(gm->IsFeatureAvailable(GraphicsManager::kSamplerObjects));
}

TEST_F(GraphicsManagerTest, MultipleGraphicsManagers) {
  GraphicsManagerPtr mgr2;
#if defined(ION_PLATFORM_NACL) || defined(ION_PLATFORM_ASMJS)
  // NaCl and ASMJS can't access OpenGL without an actual browser.
  mgr2.Reset(new testing::FakeGraphicsManager());
#else
  mgr2.Reset(new GraphicsManager);
#endif
  if (mgr_->IsFeatureAvailable(GraphicsManager::kCore)) {
    EXPECT_TRUE(IsFunctionAvailable(mgr_, "ActiveTexture"));
  }
  bool core_available = false;
  if (mgr2->IsFeatureAvailable(GraphicsManager::kCore)) {
    EXPECT_TRUE(IsFunctionAvailable(mgr2, "ActiveTexture"));
    core_available = true;
  }
  mgr_.Reset(nullptr);
  EXPECT_EQ(
      core_available, mgr2->IsFeatureAvailable(GraphicsManager::kCore));
  if (mgr2->IsFeatureAvailable(GraphicsManager::kCore)) {
    EXPECT_TRUE(IsFunctionAvailable(mgr2, "ActiveTexture"));
  }
}

TEST_F(GraphicsManagerTest, ErrorChecking) {
  // GetError() should work the same regardless of whether error checking
  // is enabled, even in production mode.
  mgr_->EnableErrorChecking(false);
  mgr_->ActiveTexture(GL_LINK_STATUS);
  EXPECT_EQ(GLenum{GL_INVALID_ENUM}, mgr_->GetError());
  EXPECT_EQ(GLenum{GL_NO_ERROR}, mgr_->GetError());
  mgr_->FramebufferRenderbuffer(GL_TEXTURE0, GL_FLOAT, GL_COMPILE_STATUS, 0);
  mgr_->EnableErrorChecking(true);
  mgr_->EnableErrorChecking(false);
  EXPECT_EQ(GLenum{GL_INVALID_ENUM}, mgr_->GetError());
  EXPECT_EQ(GLenum{GL_NO_ERROR}, mgr_->GetError());
  mgr_->Enable(GL_FRAMEBUFFER);
  mgr_->EnableErrorChecking(true);
  EXPECT_EQ(GLenum{GL_INVALID_ENUM}, mgr_->GetError());
  EXPECT_EQ(GLenum{GL_NO_ERROR}, mgr_->GetError());
  mgr_->Disable(GL_TRIANGLES);
  EXPECT_EQ(GLenum{GL_INVALID_ENUM}, mgr_->GetError());
  EXPECT_EQ(GLenum{GL_NO_ERROR}, mgr_->GetError());
  mgr_->FrontFace(GL_COLOR_ATTACHMENT0);
  mgr_->EnableErrorChecking(false);
  mgr_->EnableErrorChecking(true);
  EXPECT_EQ(GLenum{GL_INVALID_ENUM}, mgr_->GetError());
  EXPECT_EQ(GLenum{GL_NO_ERROR}, mgr_->GetError());
  mgr_->Clear(GL_RGBA);
  mgr_->EnableErrorChecking(false);
  EXPECT_EQ(GLenum{GL_INVALID_VALUE}, mgr_->GetError());
  EXPECT_EQ(GLenum{GL_NO_ERROR}, mgr_->GetError());
}

TEST_F(GraphicsManagerTest, ErrorSilencer) {
  mgr_->EnableErrorChecking(false);
  {
    GraphicsManager::ErrorSilencer silencer(mgr_.Get());
    mgr_->ActiveTexture(GL_LINK_STATUS);
    mgr_->Enable(GL_FRAMEBUFFER);
    mgr_->Clear(GL_RGBA);
  }
  EXPECT_EQ(GLenum{GL_NO_ERROR}, mgr_->GetError());
  mgr_->BindTexture(GL_TEXTURE_2D, 1234U);
  {
    GraphicsManager::ErrorSilencer silencer(mgr_.Get());
    mgr_->ActiveTexture(GL_LINK_STATUS);
    mgr_->Enable(GL_FRAMEBUFFER);
    mgr_->Clear(GL_RGBA);
  }
  // The calls above all generate GL_INVALID_ENUM
  EXPECT_EQ(GLenum{GL_INVALID_VALUE}, mgr_->GetError());

  mgr_->EnableErrorChecking(true);
  {
    GraphicsManager::ErrorSilencer silencer(mgr_.Get());
    mgr_->ActiveTexture(GL_LINK_STATUS);
    mgr_->Enable(GL_FRAMEBUFFER);
    mgr_->Clear(GL_RGBA);
  }
  EXPECT_EQ(GLenum{GL_NO_ERROR}, mgr_->GetError());
  mgr_->BindTexture(GL_TEXTURE_2D, 1234U);
  {
    GraphicsManager::ErrorSilencer silencer(mgr_.Get());
    mgr_->ActiveTexture(GL_LINK_STATUS);
    mgr_->Enable(GL_FRAMEBUFFER);
    mgr_->Clear(GL_RGBA);
  }
  EXPECT_EQ(GLenum{GL_INVALID_VALUE}, mgr_->GetError());
}

TEST_F(GraphicsManagerTest, ParseGlVersionString) {
  EXPECT_EQ(20, GraphicsManager::ParseGlVersionString("2.0"));
  EXPECT_EQ(32, GraphicsManager::ParseGlVersionString("OpenGL ES 3.2"));
  EXPECT_EQ(20, GraphicsManager::ParseGlVersionString("OpenGL ES-CM 2.0"));
  EXPECT_EQ(30, GraphicsManager::ParseGlVersionString(
                    "OpenGL ES 3.0 SwiftShader 3.3.0.7"));
  EXPECT_EQ(51, GraphicsManager::ParseGlVersionString("5.1.1"));
  EXPECT_EQ(50, GraphicsManager::ParseGlVersionString("5.0 NVIDIA"));
  // Illegal strings should return 0.
  EXPECT_EQ(0, GraphicsManager::ParseGlVersionString("4"));
  EXPECT_EQ(0, GraphicsManager::ParseGlVersionString("4a"));
  EXPECT_EQ(0, GraphicsManager::ParseGlVersionString(""));
  EXPECT_EQ(0, GraphicsManager::ParseGlVersionString("."));
  EXPECT_EQ(0, GraphicsManager::ParseGlVersionString(".ZAMBONI"));
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
  GraphicsManager::FeatureId group = GraphicsManager::kCore;
  runnable_ = std::bind(
      &ThreadedGraphicsManagerTest::CheckFunctionGroup, this, group);
  waiter_.Wait();
  int result = CheckFunctionGroup(group);
  waiter_.Wait();
  EXPECT_EQ(1, result);
  EXPECT_EQ(result, background_result_);
}

TEST_F(ThreadedGraphicsManagerTest, ConcurrentCapability) {
  GraphicsManager::Constant cap = GraphicsManager::kMaxTextureSize;
  runnable_ = std::bind(
      &ThreadedGraphicsManagerTest::GetIntConstant, this, cap);
  waiter_.Wait();
  int result = GetIntConstant(cap);
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
  runnable_ = std::bind(&ThreadedGraphicsManagerTest::CheckFlavor, this);
  waiter_.Wait();
  int result = CheckFlavor();
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
