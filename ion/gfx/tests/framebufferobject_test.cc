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

#include "ion/gfx/framebufferobject.h"

#include <memory>

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/image.h"
#include "ion/gfx/tests/mockresource.h"
#include "ion/gfx/texture.h"

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
    resource_.reset(new MockFramebufferObjectResource);
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    fbo_->SetResource(0U, resource_.get());
    EXPECT_EQ(resource_.get(), fbo_->GetResource(0U));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { fbo_.Reset(nullptr); }

  FramebufferObjectPtr fbo_;
  std::unique_ptr<MockFramebufferObjectResource> resource_;
};

}  // anonymous namespace

TEST_F(FramebufferObjectTest, DefaultUnbound) {
  // Check that attachments are unbound.
  const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
  const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
  const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
  EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(FramebufferObjectTest, OnlyOneColorAttachmentIsSupported)   {
  base::LogChecker logchecker;
  base::SetBreakHandler(kNullFunction);
  fbo_->GetColorAttachment(1U);
#if ION_DEBUG
  EXPECT_TRUE(logchecker.HasMessage(
      "DFATAL", "Only a single color attachment is supported"));
#endif
  fbo_->GetColorAttachment(1U);
#if ION_DEBUG
  EXPECT_TRUE(logchecker.HasMessage(
      "DFATAL", "Only a single color attachment is supported"));
#endif
  base::RestoreDefaultBreakHandler();
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
  FramebufferObject::Attachment color_buffer2(Image::kRgba8, 2);
  FramebufferObject::Attachment color_buffer4(Image::kRgba8, 4);
  FramebufferObject::Attachment depth_buffer2(kDepthBufferFormat, 2);
  FramebufferObject::Attachment depth_buffer4(kDepthBufferFormat, 4);

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
  FramebufferObject::Attachment color(image);
  FramebufferObject::Attachment depth(image);
  FramebufferObject::Attachment stencil(image);

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

  fbo_->SetColorAttachment(0U, FramebufferObject::Attachment(ImagePtr()));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
    EXPECT_EQ(static_cast<Image::Format>(base::kInvalidIndex),
              color.GetFormat());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, stencil.GetBinding());
  }
}

TEST_F(FramebufferObjectTest, Resize) {
  // Check that we can create resize the fbo.
  TexturePtr color_tex(new Texture);
  FramebufferObject::Attachment color(color_tex);
  FramebufferObject::Attachment depth(Image::kRenderbufferDepth16);
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

TEST_F(FramebufferObjectTest, InvalidCubemap) {
  // Check that an invalid format produces an unbound attachment.
  CubeMapTexturePtr color_tex(nullptr);
  CubeMapTexturePtr depth_tex(nullptr);
  CubeMapTexturePtr stencil_tex(nullptr);
  FramebufferObject::Attachment color(color_tex, CubeMapTexture::kNegativeX);
  FramebufferObject::Attachment depth(depth_tex, CubeMapTexture::kNegativeY);
  FramebufferObject::Attachment stencil(stencil_tex,
                                        CubeMapTexture::kNegativeZ);
  EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
}

TEST_F(FramebufferObjectTest, InvalidTexture) {
  // Check that an invalid format produces an unbound attachment.
  TexturePtr color_tex(nullptr);
  TexturePtr depth_tex(nullptr);
  TexturePtr stencil_tex(nullptr);
  FramebufferObject::Attachment color(color_tex);
  FramebufferObject::Attachment depth(depth_tex);
  FramebufferObject::Attachment stencil(stencil_tex);
  EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
  EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
}

TEST_F(FramebufferObjectTest, Cubemaps) {
  // Check that we can create texture attachments.
  CubeMapTexturePtr color_tex(new CubeMapTexture);
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

  EXPECT_EQ(FramebufferObject::kCubeMapTexture, color.GetBinding());
  EXPECT_EQ(0U, color.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kCubeMapTexture, color_mip.GetBinding());
  EXPECT_EQ(1U, color_mip.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kCubeMapTexture, depth.GetBinding());
  EXPECT_EQ(0U, depth.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kCubeMapTexture, stencil.GetBinding());
  EXPECT_EQ(0U, stencil.GetMipLevel());
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
  TexturePtr color_tex(new Texture);
  TexturePtr depth_tex(new Texture);
  ImagePtr depth_image(new Image);
  depth_image->Set(Image::kRenderbufferDepth24, 16, 16,
                   base::DataContainerPtr());
  depth_tex->SetImage(0U, depth_image);
  TexturePtr stencil_tex(new Texture);
  ImagePtr stencil_image(new Image);
  stencil_image->Set(Image::kStencil8, 16, 16, base::DataContainerPtr());
  stencil_tex->SetImage(0U, stencil_image);
  FramebufferObject::Attachment color(color_tex);
  FramebufferObject::Attachment color_mip(color_tex, 1);
  FramebufferObject::Attachment depth(depth_tex);
  FramebufferObject::Attachment stencil(stencil_tex);
  EXPECT_EQ(FramebufferObject::kTexture, color.GetBinding());
  EXPECT_EQ(0U, color.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kTexture, color_mip.GetBinding());
  EXPECT_EQ(1U, color_mip.GetMipLevel());
  EXPECT_EQ(FramebufferObject::kTexture, depth.GetBinding());
  EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  fbo_->SetColorAttachment(0U, color);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kColorAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kColorAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_EQ(color_tex.Get(), color.GetTexture().Get());
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  fbo_->SetDepthAttachment(depth);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kDepthAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kDepthAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, stencil.GetBinding());
    EXPECT_EQ(color_tex.Get(), color.GetTexture().Get());
    EXPECT_EQ(depth_tex.Get(), depth.GetTexture().Get());
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  fbo_->SetStencilAttachment(stencil);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      FramebufferObject::kStencilAttachmentChanged));
  resource_->ResetModifiedBit(FramebufferObject::kStencilAttachmentChanged);
  {
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_EQ(color_tex.Get(), color.GetTexture().Get());
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
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kTexture, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_EQ(color_tex.Get(), color.GetTexture().Get());
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
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kRenderbuffer, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_TRUE(color.GetTexture().Get() == nullptr);
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
    const FramebufferObject::Attachment& color = fbo_->GetColorAttachment(0U);
    const FramebufferObject::Attachment& depth = fbo_->GetDepthAttachment();
    const FramebufferObject::Attachment& stencil = fbo_->GetStencilAttachment();
    EXPECT_EQ(FramebufferObject::kUnbound, color.GetBinding());
    EXPECT_EQ(FramebufferObject::kUnbound, depth.GetBinding());
    EXPECT_EQ(FramebufferObject::kTexture, stencil.GetBinding());
    EXPECT_TRUE(color.GetTexture().Get() == nullptr);
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_EQ(stencil_tex.Get(), stencil.GetTexture().Get());
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
    EXPECT_TRUE(color.GetTexture().Get() == nullptr);
    EXPECT_TRUE(depth.GetTexture().Get() == nullptr);
    EXPECT_TRUE(stencil.GetTexture().Get() == nullptr);
  }

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(FramebufferObjectTest, Notifications) {
  // Check that modifying a Texture sends notifications to an owning
  // FramebufferObject, ensuring that attachments are rebound.
  TexturePtr color_tex(new Texture);
  TexturePtr depth_tex(new Texture);
  TexturePtr depth_egl_tex(new Texture);
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
  FramebufferObject::Attachment depth_egl_rb(depth_egl_image);

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

}  // namespace gfx
}  // namespace ion
