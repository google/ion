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

#include "ion/image/renderutils.h"

#include <memory>

#include "base/integral_types.h"
#include "ion/base/datacontainer.h"
#include "ion/gfx/image.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/portgfx/glcontext.h"
#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace image {

using gfx::testing::FakeGlContext;
using gfx::testing::FakeGraphicsManager;

//-----------------------------------------------------------------------------
//
// Test harness that sets up a FakeGraphicsManager, Renderer, and TraceVerifier
// for convenience.
//
//-----------------------------------------------------------------------------

class RenderUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    gl_context_ = FakeGlContext::Create(64, 64);
    portgfx::GlContext::MakeCurrent(gl_context_);
    mgm_.Reset(new FakeGraphicsManager());
    renderer_.Reset(new gfx::Renderer(mgm_));
    tv_ = absl::make_unique<gfx::testing::TraceVerifier>(mgm_.Get());

    // Start with a clear call count.
    FakeGraphicsManager::ResetCallCount();
  }

  void TearDown() override {
    tv_.reset();
    renderer_.Reset(nullptr);
    mgm_.Reset(nullptr);
    gl_context_.Reset(nullptr);
  }

  // Builds a sample valid Texture with a 32x32 image. The contents of the
  // images do not matter because the FakeGraphicsManager is incapable of
  // rendering them anyway.
  const gfx::TexturePtr BuildTexture() {
    gfx::TexturePtr tex(new gfx::Texture());
    tex->SetImage(0U, BuildImage(32, 32));
    tex->SetSampler(BuildSampler());
    return tex;
  }

  // Builds a sample valid CubeMap with 6 32x32 images. The contents of the
  // Image do not matter because the FakeGraphicsManager is incapable of
  // rendering them anyway.
  const gfx::CubeMapTexturePtr BuildCubeMap() {
    gfx::CubeMapTexturePtr cm(new gfx::CubeMapTexture());
    gfx::ImagePtr image = BuildImage(32, 32);
    cm->SetImage(gfx::CubeMapTexture::kNegativeX, 0U, image);
    cm->SetImage(gfx::CubeMapTexture::kNegativeY, 0U, image);
    cm->SetImage(gfx::CubeMapTexture::kNegativeZ, 0U, image);
    cm->SetImage(gfx::CubeMapTexture::kPositiveX, 0U, image);
    cm->SetImage(gfx::CubeMapTexture::kPositiveY, 0U, image);
    cm->SetImage(gfx::CubeMapTexture::kPositiveZ, 0U, image);
    cm->SetSampler(BuildSampler());
    return cm;
  }

  portgfx::GlContextPtr gl_context_;
  gfx::testing::FakeGraphicsManagerPtr mgm_;
  gfx::RendererPtr renderer_;
  std::unique_ptr<gfx::testing::TraceVerifier> tv_;
  base::AllocatorPtr al_;

 private:
  const gfx::ImagePtr BuildImage(uint32 width, uint32 height) {
    gfx::ImagePtr image(new gfx::Image);
    std::vector<uint8> pixels(width * height * 3, 0);
    image->Set(gfx::Image::kRgb888, width, height,
               base::DataContainer::CreateAndCopy<uint8>(
                   &pixels[0], pixels.size(), false, al_));
    return image;
  }

  const gfx::SamplerPtr BuildSampler() {
    gfx::SamplerPtr sampler(new gfx::Sampler);
    return sampler;
  }
};

//-----------------------------------------------------------------------------
//
// RenderTextureImage() Tests.
//
//-----------------------------------------------------------------------------

TEST_F(RenderUtilsTest, RenderTextureImageNullTexture) {
  gfx::ImagePtr image =
      RenderTextureImage(gfx::TexturePtr(), 32, 32, renderer_, al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

TEST_F(RenderUtilsTest, RenderTextureImageNullRenderer) {
  gfx::ImagePtr image =
      RenderTextureImage(BuildTexture(), 32, 32, gfx::RendererPtr(), al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

TEST_F(RenderUtilsTest, RenderTextureImageBadImageSize) {
  gfx::TexturePtr tex = BuildTexture();
  gfx::ImagePtr image = RenderTextureImage(tex, 0, 0, renderer_, al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());

  image = RenderTextureImage(tex, 32, 0, renderer_, al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());

  image = RenderTextureImage(tex, 0, 32, renderer_, al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

// This test relies on trace streams, which are disabled in production builds.
#if !ION_PRODUCTION

TEST_F(RenderUtilsTest, RenderTextureImageValid) {
  gfx::TexturePtr tex = BuildTexture();
  gfx::ImagePtr image = RenderTextureImage(tex, 32, 32, renderer_, al_);
  EXPECT_TRUE(image);
  EXPECT_LT(0, FakeGraphicsManager::GetCallCount());

  // Verify some selected OpenGL calls.
  EXPECT_EQ(2U, tv_->GetCountOf("BindFramebuffer"));
  EXPECT_TRUE(tv_->VerifyCallAt(tv_->GetNthIndexOf(0U, "BindFramebuffer"))
              .HasArg(1, "GL_FRAMEBUFFER")
              .HasArg(2, "0x1"));
  EXPECT_TRUE(tv_->VerifyCallAt(tv_->GetNthIndexOf(1U, "BindFramebuffer"))
              .HasArg(1, "GL_FRAMEBUFFER")
              .HasArg(2, "0x0"));
  EXPECT_EQ(1U, tv_->GetCountOf("Viewport"));
  EXPECT_TRUE(tv_->VerifyCallAt(tv_->GetNthIndexOf(0U, "Viewport"))
              .HasArg(1, "0")     // x
              .HasArg(2, "0")     // y
              .HasArg(3, "32")    // width
              .HasArg(4, "32"));  // height
  EXPECT_EQ(2U, tv_->GetCountOf("CreateShader"));
  EXPECT_EQ(1U, tv_->GetCountOf("UseProgram"));
  EXPECT_EQ(1U, tv_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, tv_->GetCountOf("BindSampler"));
  EXPECT_EQ(1U, tv_->GetCountOf("Uniform1i"));
  EXPECT_EQ(1U, tv_->GetCountOf("DrawElements"));
  EXPECT_EQ(1U, tv_->GetCountOf("ReadPixels"));
  EXPECT_TRUE(tv_->VerifyCallAt(tv_->GetNthIndexOf(0U, "ReadPixels"))
              .HasArg(1, "0")                   // x
              .HasArg(2, "0")                   // y
              .HasArg(3, "32")                  // width
              .HasArg(4, "32")                  // height
              .HasArg(5, "GL_RGB")              // format
              .HasArg(6, "GL_UNSIGNED_BYTE"));  // type
}

#endif  // !ION_PRODUCTION

//-----------------------------------------------------------------------------
//
// RenderCubeMapTextureFaceImage() Tests.
//
//-----------------------------------------------------------------------------

TEST_F(RenderUtilsTest, RenderCubeMapTextureFaceImageNullCubeMap) {
  gfx::ImagePtr image = RenderCubeMapTextureFaceImage(
      gfx::CubeMapTexturePtr(), gfx::CubeMapTexture::kPositiveX, 32, 32,
      renderer_, al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

TEST_F(RenderUtilsTest, RenderCubeMapTextureFaceImageNullRenderer) {
  gfx::ImagePtr image = RenderCubeMapTextureFaceImage(
      BuildCubeMap(), gfx::CubeMapTexture::kPositiveX, 32, 32,
      gfx::RendererPtr(), al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

TEST_F(RenderUtilsTest, RenderCubeMapTextureFaceImageBadImageSize) {
  gfx::CubeMapTexturePtr cm = BuildCubeMap();
  gfx::ImagePtr image = RenderCubeMapTextureFaceImage(
      cm, gfx::CubeMapTexture::kPositiveX, 0, 0, renderer_, al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());

  image = RenderCubeMapTextureFaceImage(
      cm, gfx::CubeMapTexture::kPositiveX, 32, 0, renderer_, al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());

  image = RenderCubeMapTextureFaceImage(
      cm, gfx::CubeMapTexture::kPositiveX, 0, 32, renderer_, al_);
  EXPECT_FALSE(image);
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

// This test relies on trace streams, which are disabled in production builds.
#if !ION_PRODUCTION

TEST_F(RenderUtilsTest, RenderCubeMapTextureFaceImageValid) {
  gfx::CubeMapTexturePtr cm = BuildCubeMap();
  gfx::ImagePtr image = RenderCubeMapTextureFaceImage(
      cm, gfx::CubeMapTexture::kPositiveY, 32, 32, renderer_,
      base::AllocationManager::GetDefaultAllocatorForLifetime(
          base::kShortTerm));
  EXPECT_TRUE(image);
  EXPECT_LT(0, FakeGraphicsManager::GetCallCount());

  // Verify some selected OpenGL calls.
  EXPECT_EQ(2U, tv_->GetCountOf("BindFramebuffer"));
  EXPECT_TRUE(tv_->VerifyCallAt(tv_->GetNthIndexOf(0U, "BindFramebuffer"))
              .HasArg(1, "GL_FRAMEBUFFER")
              .HasArg(2, "0x1"));
  EXPECT_TRUE(tv_->VerifyCallAt(tv_->GetNthIndexOf(1U, "BindFramebuffer"))
              .HasArg(1, "GL_FRAMEBUFFER")
              .HasArg(2, "0x0"));
  EXPECT_EQ(1U, tv_->GetCountOf("Viewport"));
  EXPECT_TRUE(tv_->VerifyCallAt(tv_->GetNthIndexOf(0U, "Viewport"))
              .HasArg(1, "0")     // x
              .HasArg(2, "0")     // y
              .HasArg(3, "32")    // width
              .HasArg(4, "32"));  // height
  EXPECT_EQ(2U, tv_->GetCountOf("CreateShader"));
  EXPECT_EQ(1U, tv_->GetCountOf("UseProgram"));
  EXPECT_EQ(6U, tv_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, tv_->GetCountOf("BindSampler"));
  EXPECT_EQ(2U, tv_->GetCountOf("Uniform1i"));
  EXPECT_EQ(1U, tv_->GetCountOf("DrawElements"));
  EXPECT_EQ(1U, tv_->GetCountOf("ReadPixels"));
  EXPECT_TRUE(tv_->VerifyCallAt(tv_->GetNthIndexOf(0U, "ReadPixels"))
              .HasArg(1, "0")                   // x
              .HasArg(2, "0")                   // y
              .HasArg(3, "32")                  // width
              .HasArg(4, "32")                  // height
              .HasArg(5, "GL_RGB")              // format
              .HasArg(6, "GL_UNSIGNED_BYTE"));  // type
}

#endif  // !ION_PRODUCTION

}  // namespace image
}  // namespace ion
