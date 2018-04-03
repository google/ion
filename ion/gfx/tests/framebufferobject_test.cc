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

#include "ion/gfx/framebufferobject.h"

#include <memory>

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/image.h"
#include "ion/gfx/tests/mockresource.h"
#include "ion/gfx/texture.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

typedef testing::MockResource<
    FramebufferObject::kNumChanges> MockFramebufferObjectResource;

static const Image::Format kDepthBufferFormat = Image::kRenderbufferDepth16;

class FramebufferObjectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fbo_.Reset(new FramebufferObject(512, 512));
    resource_ = absl::make_unique<MockFramebufferObjectResource>();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    fbo_->SetResource(0U, 0, resource_.get());
    EXPECT_EQ(resource_.get(), fbo_->GetResource(0U, 0));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  void SetExplicitBuffers() {
    // Set explicit draw buffers and read buffer, so that testing change bits
    // is more straightforward.
    fbo_->SetDrawBuffers({-1});
    fbo_->SetReadBuffer(-1);
    resource_->ResetModifiedBit(FramebufferObject::kDrawBuffersChanged);
    resource_->ResetModifiedBit(FramebufferObject::kReadBufferChanged);
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { fbo_.Reset(nullptr); }

  FramebufferObjectPtr fbo_;
  std::unique_ptr<MockFramebufferObjectResource> resource_;
};
#if ION_DEBUG
typedef FramebufferObjectTest FramebufferObjectDeathTest;
#endif

}  // anonymous namespace

TEST_F(FramebufferObjectTest, DefaultUnbound) {
  // Check that attachments are unbound.
  const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
  const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
  EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  EXPECT_EQ(-1, fbo_->GetReadBuffer());
  for (size_t i = 0; i < kColorAttachmentSlotCount; ++i) {
    EXPECT_EQ(FramebufferObject::kUnbound,
              fbo_->GetColorAttachment(i).GetBinding());
    EXPECT_EQ(-1, fbo_->GetDrawBuffer(i));
  }

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(FramebufferObjectTest, InvalidRenderbufferFormat) {
  // Check that an invalid format produces an unbound attachment.
  base::LogChecker log_checker;
  FramebufferObject::Attachment color(
      static_cast<Image::Format>(base::kInvalidIndex));
  fbo_->SetColorAttachment(0U, color);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid color attachment"));
  FramebufferObject::Attachment depth(
      static_cast<Image::Format>(base::kInvalidIndex));
  fbo_->SetDepthAttachment(depth);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid depth attachment"));
  FramebufferObject::Attachment stencil(
      static_cast<Image::Format>(base::kInvalidIndex));
  fbo_->SetStencilAttachment(stencil);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid stencil attachment"));
  EXPECT_EQ(FramebufferObject::kUnbound,
            fbo_->GetColorAttachment(0U).GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound,
            fbo_->GetDepthAttachment().GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound,
            fbo_->GetStencilAttachment().GetBinding());
}

TEST_F(FramebufferObjectTest, MultisamplingRenderBuffer) {
  FramebufferObject::Attachment color_buffer2 =
      FramebufferObject::Attachment::CreateMultisampled(Image::kRgba8, 2);
  FramebufferObject::Attachment color_buffer4 =
      FramebufferObject::Attachment::CreateMultisampled(Image::kRgba8, 4);
  FramebufferObject::Attachment depth_buffer2 =
      FramebufferObject::Attachment::CreateMultisampled(kDepthBufferFormat, 2);
  FramebufferObject::Attachment depth_buffer4 =
      FramebufferObject::Attachment::CreateMultisampled(kDepthBufferFormat, 4);

  EXPECT_EQ(FramebufferObject::kRenderbuffer, color_buffer2.GetBinding());
  EXPECT_EQ(FramebufferObject::kRenderbuffer, color_buffer4.GetBinding());
  EXPECT_EQ(FramebufferObject::kRenderbuffer, depth_buffer2.GetBinding());
  EXPECT_EQ(FramebufferObject::kRenderbuffer, depth_buffer4.GetBinding());

  EXPECT_EQ(2U, color_buffer2.GetSamples());
  EXPECT_EQ(4U, color_buffer4.GetSamples());
  EXPECT_EQ(2U, depth_buffer2.GetSamples());
  EXPECT_EQ(4U, depth_buffer4.GetSamples());

  EXPECT_EQ(Image::kRgba8, color_buffer2.GetFormat());
  EXPECT_EQ(Image::kRgba8, color_buffer4.GetFormat());
  EXPECT_EQ(kDepthBufferFormat, depth_buffer2.GetFormat());
  EXPECT_EQ(kDepthBufferFormat, depth_buffer4.GetFormat());

  EXPECT_TRUE(color_buffer2.IsImplicitMultisamplingCompatible());
  EXPECT_TRUE(color_buffer4.IsImplicitMultisamplingCompatible());
  EXPECT_TRUE(depth_buffer2.IsImplicitMultisamplingCompatible());
  EXPECT_TRUE(depth_buffer4.IsImplicitMultisamplingCompatible());

  SetExplicitBuffers();

  fbo_->SetColorAttachment(0U, color_buffer2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba8, color.GetFormat());
    EXPECT_EQ(2U, color.GetSamples());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }

  fbo_->SetColorAttachment(0U, color_buffer4);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba8, color.GetFormat());
    EXPECT_EQ(4U, color.GetSamples());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }

  fbo_->SetDepthAttachment(depth_buffer2);
  EXPECT_FALSE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba8, color.GetFormat());
    EXPECT_EQ(4U, color.GetSamples());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(kDepthBufferFormat, depth.GetFormat());
    EXPECT_EQ(2U, depth.GetSamples());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }

  fbo_->SetDepthAttachment(depth_buffer4);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba8, color.GetFormat());
    EXPECT_EQ(4U, color.GetSamples());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(kDepthBufferFormat, depth.GetFormat());
    EXPECT_EQ(4U, depth.GetSamples());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }
}

TEST_F(FramebufferObjectTest, Renderbuffers) {
  // Check that we can create renderbuffer attachments.
  FramebufferObject::Attachment color(Image::kRgba4Byte);
  FramebufferObject::Attachment depth(Image::kRenderbufferDepth16);
  FramebufferObject::Attachment stencil(Image::kStencil8);
  EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
  EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
  EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
  EXPECT_EQ(Image::kRgba4Byte, color.GetFormat());
  EXPECT_EQ(Image::kRenderbufferDepth16, depth.GetFormat());
  EXPECT_EQ(Image::kStencil8, stencil.GetFormat());
  EXPECT_FALSE(color.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(depth.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(stencil.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  SetExplicitBuffers();

  fbo_->SetColorAttachment(0U, color);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba4Byte, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }

  fbo_->SetDepthAttachment(depth);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba4Byte, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(Image::kRenderbufferDepth16, depth.GetFormat());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }

  fbo_->SetStencilAttachment(stencil);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba4Byte, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(Image::kRenderbufferDepth16, depth.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
    EXPECT_EQ(Image::kStencil8, stencil.GetFormat());
  }

  // No change if same attachment info is passed.
  fbo_->SetColorAttachment(0U,
      FramebufferObject::Attachment(Image::kRgba4Byte));
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba4Byte, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(Image::kRenderbufferDepth16, depth.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
    EXPECT_EQ(Image::kStencil8, stencil.GetFormat());
  }

  // Reset depth back to unbound.
  fbo_->SetDepthAttachment(FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba4Byte, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
    EXPECT_EQ(Image::kStencil8, stencil.GetFormat());
  }

  // No change if same attachment info is passed.
  fbo_->SetDepthAttachment(FramebufferObject::Attachment());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kRgba4Byte, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
    EXPECT_EQ(Image::kStencil8, stencil.GetFormat());
  }

  // Reset color0 back to unbound.
  fbo_->SetColorAttachment(0U, FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
    EXPECT_EQ(Image::kStencil8, stencil.GetFormat());
  }

  // Reset stencil back to unbound.
  fbo_->SetStencilAttachment(FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(FramebufferObjectTest, EglRenderbuffers) {
  // Check that we can create renderbuffer attachments.
  base::DataContainerPtr data = base::DataContainer::Create<void>(
      nullptr, kNullFunction, false, base::AllocatorPtr());
  ImagePtr image(new Image);
  image->SetEglImage(data);
  FramebufferObject::Attachment color =
      FramebufferObject::Attachment::CreateFromEglImage(image);
  FramebufferObject::Attachment depth =
      FramebufferObject::Attachment::CreateFromEglImage(image);
  FramebufferObject::Attachment stencil =
      FramebufferObject::Attachment::CreateFromEglImage(image);

  SetExplicitBuffers();

  EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
  EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
  EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
  EXPECT_EQ(Image::kEglImage, color.GetFormat());
  EXPECT_EQ(Image::kEglImage, depth.GetFormat());
  EXPECT_EQ(Image::kEglImage, stencil.GetFormat());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  fbo_->SetColorAttachment(0U, color);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kEglImage, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }

  fbo_->SetDepthAttachment(depth);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kEglImage, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(Image::kEglImage, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
  }

  fbo_->SetStencilAttachment(stencil);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(Image::kEglImage, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(Image::kEglImage, color.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
    EXPECT_EQ(Image::kEglImage, color.GetFormat());
  }
}

#if ION_DEBUG
TEST_F(FramebufferObjectDeathTest, EglRenderbufferInvalidImage) {
  EXPECT_DEATH_IF_SUPPORTED(
      fbo_->SetColorAttachment(0U,
          FramebufferObject::Attachment::CreateFromEglImage(ImagePtr())),
      "passed image is null");

  ImagePtr bad_image(new Image);
  bad_image->Set(Image::kRgba8, 100, 100, base::DataContainerPtr());
  EXPECT_DEATH_IF_SUPPORTED(
      fbo_->SetColorAttachment(0U,
          FramebufferObject::Attachment::CreateFromEglImage(bad_image)),
      "passed image is not an EGL image");

  // Test external EGL image for coverage.
  ImagePtr good_image(new Image);
  good_image->SetExternalEglImage(base::DataContainerPtr());
  fbo_->SetColorAttachment(0U,
      FramebufferObject::Attachment::CreateFromEglImage(good_image));
}
#endif

TEST_F(FramebufferObjectTest, Resize) {
  // Check that we can create resize the fbo.
  TexturePtr color_tex(new Texture);
  ImagePtr color_image(new Image);
  color_image->Set(Image::kRgba8888, 512, 512, base::DataContainerPtr());
  color_tex->SetImage(0U, color_image);
  FramebufferObject::Attachment color(color_tex);
  FramebufferObject::Attachment depth(Image::kRenderbufferDepth16);
  SetExplicitBuffers();
  fbo_->SetColorAttachment(0U, color);
  fbo_->SetDepthAttachment(depth);
  EXPECT_TRUE(
      resource_->TestModifiedBit(FramebufferObject::kColorAttachmentChanged));
  EXPECT_TRUE(
      resource_->TestModifiedBit(FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);

  EXPECT_EQ(512U, fbo_->GetWidth());
  EXPECT_EQ(512U, fbo_->GetHeight());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  fbo_->Resize(217, 341);
  EXPECT_EQ(217U, fbo_->GetWidth());
  EXPECT_EQ(341U, fbo_->GetHeight());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(FramebufferObject::kDimensionsChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDimensionsChanged);

  fbo_->Resize(341, 217);
  EXPECT_EQ(341U, fbo_->GetWidth());
  EXPECT_EQ(217U, fbo_->GetHeight());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(FramebufferObject::kDimensionsChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDimensionsChanged);

  // No change if same dims are passsed.
  fbo_->Resize(341, 217);
  EXPECT_EQ(341U, fbo_->GetWidth());
  EXPECT_EQ(217U, fbo_->GetHeight());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

#if ION_DEBUG
TEST_F(FramebufferObjectDeathTest, InvalidCubemap) {
  // Check that an invalid format produces an unbound attachment.
  CubeMapTexturePtr color_tex(nullptr);
  CubeMapTexturePtr depth_tex(nullptr);
  CubeMapTexturePtr stencil_tex(nullptr);
  EXPECT_DEATH_IF_SUPPORTED(
      FramebufferObject::Attachment color(color_tex,
                                          CubeMapTexture::kNegativeX),
      "DFATAL");
  EXPECT_DEATH_IF_SUPPORTED(
      FramebufferObject::Attachment depth(depth_tex,
                                          CubeMapTexture::kNegativeY),
      "DFATAL");
  EXPECT_DEATH_IF_SUPPORTED(
      FramebufferObject::Attachment stencil(stencil_tex,
                                            CubeMapTexture::kNegativeZ),
      "DFATAL");
}
#endif

#if ION_DEBUG
TEST_F(FramebufferObjectDeathTest, InvalidTexture) {
  // Check that an invalid format produces an unbound attachment.
  TexturePtr color_tex(nullptr);
  TexturePtr depth_tex(nullptr);
  TexturePtr stencil_tex(nullptr);
  EXPECT_DEATH_IF_SUPPORTED(
      FramebufferObject::Attachment color(color_tex),
      "DFATAL");
  EXPECT_DEATH_IF_SUPPORTED(
      FramebufferObject::Attachment depth(depth_tex),
      "DFATAL");
  EXPECT_DEATH_IF_SUPPORTED(
      FramebufferObject::Attachment stencil(stencil_tex),
      "DFATAL");
}
#endif

TEST_F(FramebufferObjectTest, Cubemaps) {
  // Check that we can create texture attachments.
  CubeMapTexturePtr color_tex(new CubeMapTexture);
  ImagePtr color_image(new Image);
  color_image->Set(Image::kRgba8888, 16, 16, base::DataContainerPtr());
  color_tex->SetImage(CubeMapTexture::kNegativeX, 0U, color_image);
  CubeMapTexturePtr depth_tex(new CubeMapTexture);
  ImagePtr depth_image(new Image);
  depth_image->Set(Image::kRenderbufferDepth24, 16, 16,
                   base::DataContainerPtr());
  depth_tex->SetImage(CubeMapTexture::kNegativeY, 0U, depth_image);
  CubeMapTexturePtr stencil_tex(new CubeMapTexture);
  ImagePtr stencil_image(new Image);
  stencil_image->Set(Image::kStencil8, 16, 16, base::DataContainerPtr());
  stencil_tex->SetImage(CubeMapTexture::kNegativeZ, 0U, stencil_image);
  FramebufferObject::Attachment color(color_tex, CubeMapTexture::kNegativeX);
  FramebufferObject::Attachment color_mip(
      color_tex, CubeMapTexture::kNegativeX, 1);
  FramebufferObject::Attachment depth(depth_tex, CubeMapTexture::kNegativeY);
  FramebufferObject::Attachment stencil(stencil_tex,
                                        CubeMapTexture::kNegativeZ);
  SetExplicitBuffers();

  EXPECT_EQ(FramebufferObject::kCubeMapTexture, color.GetBinding());
  EXPECT_EQ(0U, color.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kCubeMapTexture, color_mip.GetBinding());
  EXPECT_EQ(1U, color_mip.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kCubeMapTexture, depth.GetBinding());
  EXPECT_EQ(0U, depth.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kCubeMapTexture, stencil.GetBinding());
  EXPECT_EQ(0U, stencil.GetMipLevel());
  EXPECT_FALSE(color.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(color_mip.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(depth.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(stencil.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  fbo_->SetColorAttachment(0U, color);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_EQ(color_tex.Get(), color.GetCubeMapTexture().Get());
    EXPECT_TRUE(depth.GetCubeMapTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetCubeMapTexture().Get() == nullptr);
  }

  fbo_->SetDepthAttachment(depth);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_EQ(color_tex.Get(), color.GetCubeMapTexture().Get());
    EXPECT_EQ(depth_tex.Get(), depth.GetCubeMapTexture().Get());
    EXPECT_TRUE(stencil.GetCubeMapTexture().Get() == nullptr);
  }

  fbo_->SetStencilAttachment(stencil);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, stencil.GetBinding());
    EXPECT_EQ(color_tex.Get(), color.GetCubeMapTexture().Get());
    EXPECT_EQ(depth_tex.Get(), depth.GetCubeMapTexture().Get());
    EXPECT_EQ(stencil_tex.Get(), stencil.GetCubeMapTexture().Get());
  }

  // Reset depth back to a renderbuffer.
  fbo_->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, stencil.GetBinding());
    EXPECT_EQ(color_tex.Get(), color.GetCubeMapTexture().Get());
    EXPECT_TRUE(depth.GetCubeMapTexture().Get() == nullptr);
    EXPECT_EQ(Image::kRenderbufferDepth16, depth.GetFormat());
    EXPECT_EQ(stencil_tex.Get(), stencil.GetCubeMapTexture().Get());
  }

  // Reset color0 back to unbound.
  fbo_->SetColorAttachment(0U, FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, stencil.GetBinding());
    EXPECT_TRUE(color.GetCubeMapTexture().Get() == nullptr);
    EXPECT_TRUE(depth.GetCubeMapTexture().Get() == nullptr);
    EXPECT_EQ(Image::kRenderbufferDepth16, depth.GetFormat());
    EXPECT_EQ(stencil_tex.Get(), stencil.GetCubeMapTexture().Get());
  }

  // Reset depth back to unbound.
  fbo_->SetDepthAttachment(FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kCubeMapTexture, stencil.GetBinding());
    EXPECT_TRUE(color.GetCubeMapTexture().Get() == nullptr);
    EXPECT_TRUE(depth.GetCubeMapTexture().Get() == nullptr);
    EXPECT_EQ(stencil_tex.Get(), stencil.GetCubeMapTexture().Get());
  }

  // Reset stencil back to unbound.
  fbo_->SetStencilAttachment(FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_TRUE(color.GetCubeMapTexture().Get() == nullptr);
    EXPECT_TRUE(depth.GetCubeMapTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetCubeMapTexture().Get() == nullptr);
  }

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(FramebufferObjectTest, Textures) {
  // Check that we can create texture attachments.
  TexturePtr color_tex0(new Texture);
  TexturePtr color_tex1(new Texture);
  ImagePtr color_image(new Image);
  color_image->Set(Image::kRgba8888, 16, 16, base::DataContainerPtr());
  color_tex0->SetImage(0U, color_image);
  color_tex1->SetImage(0U, color_image);
  TexturePtr depth_tex(new Texture);
  ImagePtr depth_image(new Image);
  depth_image->Set(Image::kRenderbufferDepth24, 16, 16,
                   base::DataContainerPtr());
  depth_tex->SetImage(0U, depth_image);
  TexturePtr stencil_tex(new Texture);
  ImagePtr stencil_image(new Image);
  stencil_image->Set(Image::kStencil8, 16, 16, base::DataContainerPtr());
  stencil_tex->SetImage(0U, stencil_image);
  FramebufferObject::Attachment color0(color_tex0);
  FramebufferObject::Attachment color_mip0(color_tex0, 1);
  FramebufferObject::Attachment color1(color_tex1);
  FramebufferObject::Attachment color_mip1(color_tex1, 2);
  FramebufferObject::Attachment depth(depth_tex);
  FramebufferObject::Attachment stencil(stencil_tex);
  SetExplicitBuffers();

  EXPECT_EQ(FramebufferObject::kTexture, color0.GetBinding());
  EXPECT_EQ(0U, color0.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kTexture, color_mip0.GetBinding());
  EXPECT_EQ(1U, color_mip0.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kTexture, color1.GetBinding());
  EXPECT_EQ(0U, color1.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kTexture, color_mip1.GetBinding());
  EXPECT_EQ(2U, color_mip1.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kTexture, depth.GetBinding());
  EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
  EXPECT_FALSE(color0.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(color_mip0.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(color1.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(color_mip1.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(depth.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(stencil.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  fbo_->SetColorAttachment(0U, color0);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_EQ(color_tex0.Get(), color0.GetTexture().Get());
    EXPECT_TRUE(color1.GetTexture().Get() == nullptr);
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  fbo_->SetDepthAttachment(depth);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_EQ(color_tex0.Get(), color0.GetTexture().Get());
    EXPECT_TRUE(color1.GetTexture().Get() == nullptr);
    EXPECT_EQ(depth_tex.Get(), depth.GetTexture().Get());
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  fbo_->SetStencilAttachment(stencil);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_EQ(color_tex0.Get(), color0.GetTexture().Get());
    EXPECT_TRUE(color1.GetTexture().Get() == nullptr);
    EXPECT_EQ(depth_tex.Get(), depth.GetTexture().Get());
    EXPECT_EQ(stencil_tex.Get(), stencil.GetTexture().Get());
  }

  fbo_->SetColorAttachment(1U, color1);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged + 1));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged + 1);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_EQ(color_tex0.Get(), color0.GetTexture().Get());
    EXPECT_EQ(color_tex1.Get(), color1.GetTexture().Get());
    EXPECT_EQ(depth_tex.Get(), depth.GetTexture().Get());
    EXPECT_EQ(stencil_tex.Get(), stencil.GetTexture().Get());
  }

  // Reset depth back to a renderbuffer.
  fbo_->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_EQ(color_tex0.Get(), color0.GetTexture().Get());
    EXPECT_EQ(color_tex1.Get(), color1.GetTexture().Get());
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_EQ(Image::kRenderbufferDepth16, depth.GetFormat());
    EXPECT_EQ(stencil_tex.Get(), stencil.GetTexture().Get());
  }

  // Reset color0 back to unbound.
  fbo_->SetColorAttachment(0U, FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_TRUE(color0.GetTexture().Get() == nullptr);
    EXPECT_EQ(color_tex1.Get(), color1.GetTexture().Get());
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_EQ(Image::kRenderbufferDepth16, depth.GetFormat());
    EXPECT_EQ(stencil_tex.Get(), stencil.GetTexture().Get());
  }

  // Reset depth back to unbound.
  fbo_->SetDepthAttachment(FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_TRUE(color0.GetTexture().Get() == nullptr);
    EXPECT_EQ(color_tex1.Get(), color1.GetTexture().Get());
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_EQ(stencil_tex.Get(), stencil.GetTexture().Get());
  }

  // Reset stencil back to unbound.
  fbo_->SetStencilAttachment(FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_TRUE(color0.GetTexture().Get() == nullptr);
    EXPECT_EQ(color_tex1.Get(), color1.GetTexture().Get());
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  // Reset color attachment 1 to unbound.
  fbo_->SetColorAttachment(1U, FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged + 1));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged + 1);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_TRUE(color0.GetTexture().Get() == nullptr);
    EXPECT_TRUE(color1.GetTexture().Get() == nullptr);
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(FramebufferObjectTest, PackedDepthStencil) {
  // Check that we can create renderbuffer attachments with packed depth-stencil
  // formats.
  static const Image::Format kRenderbufferPackedFormats[] = {
      Image::Format::kRenderbufferDepth24Stencil8,
      Image::Format::kRenderbufferDepth32fStencil8,
  };

  for (Image::Format format : kRenderbufferPackedFormats) {
    resource_->ResetModifiedBits();
    FramebufferObject::Attachment color(Image::kRgba4Byte);
    FramebufferObject::Attachment depth_stencil(format);
    EXPECT_EQ(FramebufferObject::kRenderbuffer, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth_stencil.GetBinding());
    EXPECT_EQ(Image::kRgba4Byte, color.GetFormat());
    EXPECT_EQ(format, depth_stencil.GetFormat());

    SetExplicitBuffers();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    fbo_->SetColorAttachment(0U, color);
    fbo_->SetDepthAttachment(depth_stencil);
    fbo_->SetStencilAttachment(depth_stencil);

    resource_->TestModifiedBit(FramebufferObject::kColorAttachmentChanged);
    resource_->TestModifiedBit(FramebufferObject::kDepthAttachmentChanged);
    resource_->TestModifiedBit(FramebufferObject::kStencilAttachmentChanged);

    EXPECT_EQ(FramebufferObject::kRenderbuffer,
              fbo_->GetColorAttachment(0U).GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer,
              fbo_->GetDepthAttachment().GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer,
              fbo_->GetStencilAttachment().GetBinding());
  }

  // Check that we can create texture attachments with packed depth-stencil
  // formats.
  static const Image::Format kTexturePackedFormats[] = {
      Image::Format::kTextureDepth24Stencil8,
      Image::Format::kTextureDepth32fStencil8,
  };

  for (Image::Format format : kTexturePackedFormats) {
    resource_->ResetModifiedBits();
    TexturePtr color_tex(new Texture);
    ImagePtr color_image(new Image);
    color_image->Set(Image::kRgba8888, 16, 16, base::DataContainerPtr());
    color_tex->SetImage(0U, color_image);
    TexturePtr depth_stencil_tex(new Texture);
    ImagePtr depth_stencil_image(new Image);
    depth_stencil_image->Set(format, 16, 16, base::DataContainerPtr());
    depth_stencil_tex->SetImage(0U, depth_stencil_image);
    FramebufferObject::Attachment color(color_tex);
    FramebufferObject::Attachment depth_stencil(depth_stencil_tex);
    EXPECT_EQ(FramebufferObject::kTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, depth_stencil.GetBinding());
    EXPECT_EQ(format, depth_stencil.GetFormat());

    SetExplicitBuffers();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    fbo_->SetColorAttachment(0U, color);
    fbo_->SetDepthAttachment(depth_stencil);
    fbo_->SetStencilAttachment(depth_stencil);

    resource_->TestModifiedBit(FramebufferObject::kColorAttachmentChanged);
    resource_->TestModifiedBit(FramebufferObject::kDepthAttachmentChanged);
    resource_->TestModifiedBit(FramebufferObject::kStencilAttachmentChanged);

    EXPECT_EQ(FramebufferObject::kTexture,
              fbo_->GetColorAttachment(0U).GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture,
              fbo_->GetDepthAttachment().GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture,
              fbo_->GetStencilAttachment().GetBinding());
  }
}

TEST_F(FramebufferObjectTest, TextureLayers) {
  TexturePtr color_tex(new Texture);
  ImagePtr color_image(new Image);
  color_image->SetArray(Image::kRgba8888, 128, 128, 16,
                        base::DataContainerPtr());
  color_tex->SetImage(0U, color_image);
  FramebufferObject::Attachment color_layer =
      FramebufferObject::Attachment::CreateFromLayer(color_tex, 7);
  SetExplicitBuffers();

  EXPECT_EQ(FramebufferObject::kTextureLayer, color_layer.GetBinding());
  EXPECT_EQ(0U, color_layer.GetMipLevel());
  EXPECT_EQ(7U, color_layer.GetLayer());
  EXPECT_EQ(0U, color_layer.GetBaseViewIndex());
  EXPECT_EQ(0U, color_layer.GetSamples());
  EXPECT_EQ(color_tex, color_layer.GetTexture());
  EXPECT_TRUE(color_layer.GetImage().Get() == nullptr);
  EXPECT_FALSE(color_layer.IsImplicitMultisamplingCompatible());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  fbo_->SetColorAttachment(1U, color_layer);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged + 1));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged + 1);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kTextureLayer, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_TRUE(color0.GetTexture().Get() == nullptr);
    EXPECT_EQ(color_tex.Get(), color1.GetTexture().Get());
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  fbo_->SetColorAttachment(1U, FramebufferObject::Attachment());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged + 1));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged + 1);
  {
    const FramebufferObject::Attachment& color0 = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& color1 = fbo_->GetColorAttachment(1U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color0.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, color1.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_TRUE(color0.GetTexture().Get() == nullptr);
    EXPECT_TRUE(color1.GetTexture().Get() == nullptr);
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(FramebufferObjectTest, MultiviewAttachments) {
  // Create an OpenGL array texture with 16 slices
  TexturePtr color_array(new Texture);
  ImagePtr color_array_image(new Image);
  color_array_image->Set(Image::kRgba8888, 128, 128, 16,
                         base::DataContainerPtr());
  color_array->SetImage(0U, color_array_image);
  // Create another array texture, this time backed by an EGLImage
  static uint8 kTexelData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                                 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  TexturePtr color_array_egl(new Texture);
  ImagePtr color_array_image_egl(new Image);
  color_array_image_egl->SetEglImageArray(
          base::DataContainer::Create<void>(kTexelData, kNullFunction, false,
                                            base::AllocatorPtr()));
  color_array_egl->SetImage(0U, color_array_image_egl);
  // Validate properties on three types of multiview framebuffers.
  FramebufferObject::Attachment multiview =
      FramebufferObject::Attachment::CreateMultiview(color_array, 6U, 2U);
  FramebufferObject::Attachment ims_multiview =
      FramebufferObject::Attachment::CreateImplicitlyMultisampledMultiview(
          color_array, 6U, 2U, 4U);
  FramebufferObject::Attachment egl_multiview =
      FramebufferObject::Attachment::CreateMultiview(color_array_egl, 6U, 2U);
  EXPECT_EQ(FramebufferObject::kMultiview, multiview.GetBinding());
  EXPECT_EQ(FramebufferObject::kMultiview, ims_multiview.GetBinding());
  EXPECT_EQ(FramebufferObject::kMultiview, egl_multiview.GetBinding());
  EXPECT_EQ(6U, multiview.GetBaseViewIndex());
  EXPECT_EQ(6U, ims_multiview.GetBaseViewIndex());
  EXPECT_EQ(6U, egl_multiview.GetBaseViewIndex());
  EXPECT_EQ(0U, multiview.GetLayer());
  EXPECT_EQ(0U, ims_multiview.GetLayer());
  EXPECT_EQ(0U, egl_multiview.GetLayer());
  EXPECT_EQ(2U, multiview.GetNumViews());
  EXPECT_EQ(2U, ims_multiview.GetNumViews());
  EXPECT_EQ(2U, egl_multiview.GetNumViews());
  EXPECT_EQ(0U, multiview.GetSamples());
  EXPECT_EQ(4U, ims_multiview.GetSamples());
  EXPECT_EQ(0U, egl_multiview.GetSamples());
  EXPECT_EQ(0U, multiview.GetMipLevel());
  EXPECT_EQ(0U, ims_multiview.GetMipLevel());
  EXPECT_EQ(0U, egl_multiview.GetMipLevel());
}

#if ION_DEBUG
TEST_F(FramebufferObjectDeathTest, MultiviewZeroViewsError) {
  TexturePtr tex(new Texture);
  ImagePtr image(new Image);
  image->Set(Image::kRgba8888, 64, 64, 16, base::DataContainerPtr());
  tex->SetImage(0U, image);
  EXPECT_DEATH_IF_SUPPORTED(
      fbo_->SetColorAttachment(0U,
          FramebufferObject::Attachment::CreateMultiview(tex, 0, 0)),
      "Multiview attachment cannot have zero views");
  EXPECT_DEATH_IF_SUPPORTED(
      fbo_->SetColorAttachment(0U,
          FramebufferObject::Attachment::CreateImplicitlyMultisampledMultiview(
              tex, 0, 0, 8)),
      "Multiview attachment cannot have zero views");
}
#endif

TEST_F(FramebufferObjectTest, ImplicitlyMultisampledAttachments) {
  TexturePtr color_tex(new Texture);
  TexturePtr color_array(new Texture);
  CubeMapTexturePtr color_cube(new CubeMapTexture);
  ImagePtr color_image(new Image);
  color_image->Set(Image::kRgba8888, 128, 128, base::DataContainerPtr());
  color_tex->SetImage(0U, color_image);
  color_cube->SetImage(CubeMapTexture::kNegativeZ, 0U, color_image);
  ImagePtr color_array_image(new Image);
  color_array_image->Set(Image::kRgba8888, 128, 128, 16,
                         base::DataContainerPtr());
  color_array->SetImage(0U, color_array_image);

  FramebufferObject::Attachment imstex =
      FramebufferObject::Attachment::CreateImplicitlyMultisampled(color_tex, 4);
  FramebufferObject::Attachment imscube =
      FramebufferObject::Attachment::CreateImplicitlyMultisampled(
          color_cube, CubeMapTexture::kNegativeZ, 4);
  FramebufferObject::Attachment imsmultiview =
      FramebufferObject::Attachment::CreateImplicitlyMultisampledMultiview(
          color_array, 6, 2, 4);
  FramebufferObject::Attachment empty;

  EXPECT_EQ(FramebufferObject::kTexture, imstex.GetBinding());
  EXPECT_EQ(FramebufferObject::kCubeMapTexture, imscube.GetBinding());
  EXPECT_EQ(FramebufferObject::kMultiview, imsmultiview.GetBinding());
  EXPECT_EQ(4U, imstex.GetSamples());
  EXPECT_EQ(4U, imscube.GetSamples());
  EXPECT_EQ(4U, imsmultiview.GetSamples());
  EXPECT_EQ(color_tex.Get(), imstex.GetTexture().Get());
  EXPECT_EQ(color_cube.Get(), imscube.GetCubeMapTexture().Get());
  EXPECT_EQ(color_array.Get(), imsmultiview.GetTexture().Get());
  EXPECT_TRUE(imstex.IsImplicitMultisamplingCompatible());
  EXPECT_TRUE(imscube.IsImplicitMultisamplingCompatible());
  EXPECT_TRUE(imsmultiview.IsImplicitMultisamplingCompatible());
  EXPECT_TRUE(empty.IsImplicitMultisamplingCompatible());
}

#if ION_DEBUG
TEST_F(FramebufferObjectDeathTest, ImplicitlyMultisampledTextureError) {
  TexturePtr ms_tex(new Texture);
  ImagePtr image(new Image);
  image->Set(Image::kRgba8888, 128, 128, base::DataContainerPtr());
  ms_tex->SetImage(0U, image);
  ms_tex->SetMultisampling(8, false);
  EXPECT_DEATH_IF_SUPPORTED(
      fbo_->SetColorAttachment(0U,
          FramebufferObject::Attachment::CreateImplicitlyMultisampled(
              ms_tex, 8)),
      "Cannot create an implicitly multisampled attachment");
}
#endif

TEST_F(FramebufferObjectTest, MultisampledTextureNotImplicitMSCompatible) {
  TexturePtr ms_tex(new Texture);
  ImagePtr image(new Image);
  image->Set(Image::kRgba8888, 128, 128, base::DataContainerPtr());
  ms_tex->SetImage(0U, image);
  ms_tex->SetMultisampling(8, false);
  FramebufferObject::Attachment ms_attachment(ms_tex);

  EXPECT_EQ(8U, ms_attachment.GetSamples());
  EXPECT_FALSE(ms_attachment.IsImplicitMultisamplingCompatible());
}

TEST_F(FramebufferObjectTest, Notifications) {
  // Check that modifying a Texture sends notifications to an owning
  // FramebufferObject, ensuring that attachments are rebound.
  TexturePtr color_tex(new Texture);
  TexturePtr depth_tex(new Texture);
  TexturePtr depth_egl_tex(new Texture);
  ImagePtr color_image(new Image);
  color_image->Set(Image::kRgba8888, 16, 16, base::DataContainerPtr());
  color_tex->SetImage(0U, color_image);
  ImagePtr depth_image(new Image);
  depth_image->Set(Image::kRenderbufferDepth24, 16, 16,
                   base::DataContainerPtr());
  depth_tex->SetImage(0U, depth_image);
  ImagePtr depth_egl_image(new Image);
  base::DataContainerPtr data = base::DataContainer::Create<void>(
      nullptr, kNullFunction, false, base::AllocatorPtr());
  depth_egl_image->SetEglImage(data);
  depth_egl_tex->SetImage(0U, depth_egl_image);
  TexturePtr stencil_tex(new Texture);
  ImagePtr stencil_image(new Image);
  stencil_image->Set(Image::kStencil8, 16, 16, base::DataContainerPtr());
  stencil_tex->SetImage(0U, stencil_image);
  FramebufferObject::Attachment color(color_tex);
  FramebufferObject::Attachment color_mip(color_tex, 1);
  FramebufferObject::Attachment depth(depth_tex);
  FramebufferObject::Attachment stencil(stencil_tex);
  FramebufferObject::Attachment depth_egl(depth_egl_tex);
  FramebufferObject::Attachment depth_egl_rb =
      FramebufferObject::Attachment::CreateFromEglImage(depth_egl_image);
  SetExplicitBuffers();

  fbo_->SetColorAttachment(0U, color);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);

  // Modify the color texture.
  color_tex->SetBaseLevel(2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);

  // Try the same with depth and stencil to make sure we can distinguish between
  // textures.
  fbo_->SetDepthAttachment(depth);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  depth_tex->SetBaseLevel(2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);

  fbo_->SetDepthAttachment(depth_egl);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  depth_egl_tex->SetBaseLevel(2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);

  fbo_->SetDepthAttachment(depth_egl_rb);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  depth_egl_image->SetExternalEglImage(data);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);

  fbo_->SetStencilAttachment(stencil);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  stencil_tex->SetBaseLevel(2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);

  // Test with cubemaps.
  CubeMapTexturePtr color_tex_cube(new CubeMapTexture);
  CubeMapTexturePtr depth_tex_cube(new CubeMapTexture);
  CubeMapTexturePtr stencil_tex_cube(new CubeMapTexture);
  color_tex_cube->SetImage(CubeMapTexture::kNegativeX, 0U, color_image);
  depth_tex_cube->SetImage(CubeMapTexture::kNegativeY, 0U, depth_image);
  stencil_tex_cube->SetImage(CubeMapTexture::kNegativeZ, 0U, stencil_image);
  FramebufferObject::Attachment color_cube(color_tex_cube,
                                           CubeMapTexture::kNegativeX);
  FramebufferObject::Attachment depth_cube(depth_tex_cube,
                                           CubeMapTexture::kNegativeY);
  FramebufferObject::Attachment stencil_cube(stencil_tex_cube,
                                             CubeMapTexture::kNegativeZ);
  fbo_->SetColorAttachment(0U, color_cube);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);

  // Modify the color texture.
  color_tex_cube->SetBaseLevel(2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);

  // Try the same with depth and stencil to make sure we can distinguish between
  // textures.
  fbo_->SetDepthAttachment(depth_cube);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  depth_tex_cube->SetBaseLevel(2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);

  fbo_->SetStencilAttachment(stencil_cube);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  stencil_tex_cube->SetBaseLevel(2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
}

TEST_F(FramebufferObjectTest, DrawBuffers) {
  // Test default behavior
  fbo_->SetColorAttachment(1U, FramebufferObject::Attachment(Image::kRgba8));
  fbo_->SetColorAttachment(3U, FramebufferObject::Attachment(Image::kRgba8));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged + 1);
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged + 3);
  resource_->ResetModifiedBit(FramebufferObject::kReadBufferChanged);
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(0U));
  EXPECT_EQ(1, fbo_->GetDrawBuffer(1U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(2U));
  EXPECT_EQ(3, fbo_->GetDrawBuffer(3U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(78U));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDrawBuffersChanged));

  fbo_->SetDrawBuffer(3U, -1);
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(0U));
  EXPECT_EQ(1, fbo_->GetDrawBuffer(1U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(2U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(3U));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDrawBuffersChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDrawBuffersChanged);

  base::AllocVector<int32> buffers((base::AllocatorPtr()));
  buffers.resize(4);
  buffers[0] = 1;
  buffers[1] = 0;
  buffers[2] = -1;
  buffers[3] = 3;
  fbo_->SetDrawBuffers(buffers);
  EXPECT_EQ(1, fbo_->GetDrawBuffer(0U));
  EXPECT_EQ(0, fbo_->GetDrawBuffer(1U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(2U));
  EXPECT_EQ(3, fbo_->GetDrawBuffer(3U));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDrawBuffersChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDrawBuffersChanged);

  fbo_->SetDrawBuffers({2, 3, 0, 1});
  EXPECT_EQ(2, fbo_->GetDrawBuffer(0U));
  EXPECT_EQ(3, fbo_->GetDrawBuffer(1U));
  EXPECT_EQ(0, fbo_->GetDrawBuffer(2U));
  EXPECT_EQ(1, fbo_->GetDrawBuffer(3U));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDrawBuffersChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDrawBuffersChanged);

  // Verify that setting only some buffers disables all that were unspecified.
  fbo_->SetDrawBuffers({1, 2});
  EXPECT_EQ(1, fbo_->GetDrawBuffer(0U));
  EXPECT_EQ(2, fbo_->GetDrawBuffer(1U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(2U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(3U));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDrawBuffersChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDrawBuffersChanged);

  // Reset back to automatic defaults.
  fbo_->ResetDrawBuffers();
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(0U));
  EXPECT_EQ(1, fbo_->GetDrawBuffer(1U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(2U));
  EXPECT_EQ(3, fbo_->GetDrawBuffer(3U));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDrawBuffersChanged));
}

TEST_F(FramebufferObjectTest, DrawBuffersValidation) {
  base::LogChecker log_checker;

  fbo_->SetDrawBuffer(23U, 0);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Out of bounds index"));
  {
    base::AllocVector<int32> bufs((base::AllocatorPtr()));
    bufs.resize(12U, -1);
    bufs[9] = 0;
    fbo_->SetDrawBuffers(bufs);
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "Trying to set more than"));
  }
  fbo_->SetDrawBuffers({-1, -1, -1, -1, 1, -1, -1, -1, 0, -1, 2});
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Trying to set more than"));

  fbo_->SetDrawBuffer(2U, 3);
  fbo_->SetDrawBuffer(2U, 50);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Out of bounds buffer number"));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(2U));
  {
    base::AllocVector<int32> bufs((base::AllocatorPtr()));
    bufs.resize(4U, -1);
    bufs[1] = 12;
    fbo_->SetDrawBuffers(bufs);
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Out of bounds buffer number"));
    EXPECT_EQ(-1, fbo_->GetDrawBuffer(1U));
  }
  fbo_->SetDrawBuffers({0, 1, 2, 3});
  fbo_->SetDrawBuffers({-1, 0, -1, 17});
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Out of bounds buffer number"));
  EXPECT_EQ(0, fbo_->GetDrawBuffer(1U));
  EXPECT_EQ(-1, fbo_->GetDrawBuffer(3U));
}

TEST_F(FramebufferObjectTest, ReadBuffer) {
  // Test initial state
  EXPECT_EQ(fbo_->GetReadBuffer(), -1);

  // Verify that the default read buffer depends on attachments
  fbo_->SetColorAttachment(2U, FramebufferObject::Attachment(Image::kRgba8));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged + 2);
  resource_->ResetModifiedBit(FramebufferObject::kDrawBuffersChanged);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kReadBufferChanged));
  resource_->ResetModifiedBit(FramebufferObject::kReadBufferChanged);
  EXPECT_EQ(fbo_->GetReadBuffer(), 2);

  // Verify that manual setting works
  fbo_->SetReadBuffer(3);
  EXPECT_EQ(3, fbo_->GetReadBuffer());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kReadBufferChanged));

  // Reset to automatic defaults
  fbo_->ResetReadBuffer();
  EXPECT_EQ(fbo_->GetReadBuffer(), 2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kReadBufferChanged));
}

TEST_F(FramebufferObjectTest, ForEachAttachment) {
  size_t num_attachments = 0;
  fbo_->SetColorAttachment(2U, FramebufferObject::Attachment(Image::kRgba8));
  fbo_->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  fbo_->SetStencilAttachment(FramebufferObject::Attachment(Image::kStencil8));
  fbo_->ForEachAttachment(
      [&num_attachments](const FramebufferObject::Attachment& a, int b) -> void{
    ++num_attachments;
    EXPECT_EQ(a.GetFormat() == Image::kRgba8,
              b == FramebufferObject::kColorAttachmentChanged + 2U);
    EXPECT_EQ(a.GetFormat() == Image::kRenderbufferDepth16,
              b == FramebufferObject::kDepthAttachmentChanged);
    EXPECT_EQ(a.GetFormat() == Image::kStencil8,
              b == FramebufferObject::kStencilAttachmentChanged);
  });
  EXPECT_EQ(kColorAttachmentSlotCount + 2, num_attachments);
}

}  // namespace gfx
}  // namespace ion
