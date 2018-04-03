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

#include "ion/gfx/cubemaptexture.h"

#include <memory>

#include "ion/base/datacontainer.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/image.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/tests/mockresource.h"
#include "ion/math/vector.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

using math::Point2ui;
using math::Point3ui;

typedef testing::MockResource<CubeMapTexture::kNumChanges>
    MockCubeMapTextureResource;

class CubeMapTextureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    texture_.Reset(new CubeMapTexture());
    resource_ = absl::make_unique<MockCubeMapTextureResource>();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    texture_->SetResource(0U, 0, resource_.get());
    EXPECT_EQ(resource_.get(), texture_->GetResource(0U, 0));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { texture_.Reset(nullptr); }

  CubeMapTexturePtr texture_;
  std::unique_ptr<MockCubeMapTextureResource> resource_;
};

TEST_F(CubeMapTextureTest, DefaultModes) {
  // Check that the texture does not have an Image in any of its faces.
  for (int i = 0; i < 6; ++i) {
    SCOPED_TRACE(::testing::Message() << "Face " << i);
    CubeMapTexture::CubeFace face = static_cast<CubeMapTexture::CubeFace>(i);
    EXPECT_FALSE(texture_->HasImage(face, 0U));
  }
  // Check that the texture does not have an Sampler.
  EXPECT_FALSE(texture_->GetSampler().Get());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(CubeMapTextureTest, SetImage) {
  for (int i = 0; i < 6; ++i) {
    SCOPED_TRACE(::testing::Message() << "Face " << i);
    CubeMapTexture::CubeFace face = static_cast<CubeMapTexture::CubeFace>(i);

    ImagePtr image(new Image());
    texture_->SetImage(face, 0U, image);

    // Check that the texture has an image.
    EXPECT_TRUE(texture_->HasImage(face, 0U));

    // Check that the image is the one we set.
    EXPECT_EQ(image.Get(), texture_->GetImage(face, 0U).Get());
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    EXPECT_TRUE(
        resource_->TestOnlyModifiedBit(static_cast<CubeMapTexture::CubeFace>(
            CubeMapTexture::kNegativeXMipmapChanged + i * kMipmapSlotCount)));
    resource_->ResetModifiedBits();
  }
}

TEST_F(CubeMapTextureTest, SetSampler) {
  SamplerPtr sampler(new Sampler());
  texture_->SetSampler(sampler);

  // Check that the texture has an Sampler and that it is the one we set.
  EXPECT_EQ(sampler.Get(), texture_->GetSampler().Get());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(CubeMapTexture::kSamplerChanged));
}

TEST_F(CubeMapTextureTest, ImmutableTextures) {
  base::LogChecker log_checker;

  SamplerPtr sampler(new Sampler());
  texture_->SetSampler(sampler);

  EXPECT_FALSE(texture_->GetImmutableImage());
  EXPECT_EQ(0U, texture_->GetImmutableLevels());
  resource_->ResetModifiedBits();

  ImagePtr image(new Image());
  // It is an error to try to specify 0 levels.
  EXPECT_FALSE(texture_->SetProtectedImage(image, 0U));
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "SetImmutableImage() called with levels == 0"));
  EXPECT_FALSE(texture_->GetImmutableImage());
  EXPECT_EQ(0U, texture_->GetImmutableLevels());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  EXPECT_FALSE(texture_->IsProtected());

  // This image should be removed once the immutable image is set.
  ImagePtr unused_image(new Image);
  texture_->SetImage(CubeMapTexture::kNegativeX, 0U, unused_image);
  resource_->ResetModifiedBits();
  // This should succeed.
  EXPECT_TRUE(texture_->SetProtectedImage(image, 2U));
  EXPECT_EQ(image.Get(), texture_->GetImmutableImage().Get());
  EXPECT_EQ(2U, texture_->GetImmutableLevels());
  EXPECT_TRUE(texture_->IsProtected());
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(TextureBase::kImmutableImageChanged));
  resource_->ResetModifiedBits();
  EXPECT_EQ(image.Get(), texture_->GetImmutableImage().Get());
  EXPECT_EQ(image.Get(),
            texture_->GetImage(CubeMapTexture::kNegativeX, 0U).Get());
  EXPECT_EQ(image.Get(),
            texture_->GetImage(CubeMapTexture::kNegativeY, 1U).Get());
  EXPECT_FALSE(texture_->GetImage(CubeMapTexture::kNegativeZ, 2U).Get());

  ImagePtr image2(new Image());
  EXPECT_FALSE(texture_->SetImmutableImage(image2, 4U));
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "SetImmutableImage() called on an already immutable"));
  EXPECT_EQ(image.Get(), texture_->GetImmutableImage().Get());
  EXPECT_EQ(2U, texture_->GetImmutableLevels());
  EXPECT_TRUE(texture_->IsProtected());

  // Calling SetImage() on an immutable Texture is an error.
  EXPECT_TRUE(texture_->HasImage(CubeMapTexture::kNegativeX, 0U));
  EXPECT_EQ(image.Get(),
            texture_->GetImage(CubeMapTexture::kNegativeX, 0U).Get());
  texture_->SetImage(CubeMapTexture::kNegativeX, 0U, image2);
  EXPECT_TRUE(
      log_checker.HasMessage("ERROR", "SetImage() called on immutable"));
  EXPECT_TRUE(texture_->HasImage(CubeMapTexture::kNegativeX, 0U));
  EXPECT_EQ(image.Get(),
            texture_->GetImage(CubeMapTexture::kNegativeX, 0U).Get());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(CubeMapTextureTest, MipmapLevels) {
  EXPECT_EQ(0, texture_->GetBaseLevel());
  texture_->SetBaseLevel(1);
  EXPECT_EQ(1, texture_->GetBaseLevel());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(CubeMapTexture::kBaseLevelChanged));
  resource_->ResetModifiedBits();
  texture_->SetBaseLevel(12);
  EXPECT_EQ(12, texture_->GetBaseLevel());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(CubeMapTexture::kBaseLevelChanged));
  resource_->ResetModifiedBits();
  texture_->SetBaseLevel(12);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_EQ(1000, texture_->GetMaxLevel());
  texture_->SetMaxLevel(120);
  EXPECT_EQ(120, texture_->GetMaxLevel());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(CubeMapTexture::kMaxLevelChanged));
  resource_->ResetModifiedBits();
  texture_->SetMaxLevel(456);
  EXPECT_EQ(456, texture_->GetMaxLevel());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(CubeMapTexture::kMaxLevelChanged));
  resource_->ResetModifiedBits();
  texture_->SetMaxLevel(456);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(CubeMapTextureTest, Swizzles) {
  EXPECT_EQ(CubeMapTexture::kRed, texture_->GetSwizzleRed());
  EXPECT_EQ(CubeMapTexture::kGreen, texture_->GetSwizzleGreen());
  EXPECT_EQ(CubeMapTexture::kBlue, texture_->GetSwizzleBlue());
  EXPECT_EQ(CubeMapTexture::kAlpha, texture_->GetSwizzleAlpha());

  texture_->SetSwizzleRed(CubeMapTexture::kGreen);
  EXPECT_EQ(CubeMapTexture::kGreen, texture_->GetSwizzleRed());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(CubeMapTexture::kSwizzleRedChanged));
  texture_->SetSwizzleRed(CubeMapTexture::kGreen);
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSwizzleGreen(CubeMapTexture::kBlue);
  EXPECT_EQ(CubeMapTexture::kBlue, texture_->GetSwizzleGreen());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(CubeMapTexture::kSwizzleGreenChanged));
  texture_->SetSwizzleGreen(CubeMapTexture::kBlue);
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSwizzleBlue(CubeMapTexture::kAlpha);
  EXPECT_EQ(CubeMapTexture::kAlpha, texture_->GetSwizzleBlue());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(CubeMapTexture::kSwizzleBlueChanged));
  texture_->SetSwizzleBlue(CubeMapTexture::kAlpha);
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSwizzleAlpha(CubeMapTexture::kRed);
  EXPECT_EQ(CubeMapTexture::kRed, texture_->GetSwizzleAlpha());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(
      resource_->TestOnlyModifiedBit(CubeMapTexture::kSwizzleAlphaChanged));
  texture_->SetSwizzleAlpha(CubeMapTexture::kRed);
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSwizzles(
      Texture::kRed, Texture::kGreen, Texture::kBlue, Texture::kAlpha);
  EXPECT_EQ(CubeMapTexture::kRed, texture_->GetSwizzleRed());
  EXPECT_EQ(CubeMapTexture::kGreen, texture_->GetSwizzleGreen());
  EXPECT_EQ(CubeMapTexture::kBlue, texture_->GetSwizzleBlue());
  EXPECT_EQ(CubeMapTexture::kAlpha, texture_->GetSwizzleAlpha());
  EXPECT_EQ(4U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(CubeMapTexture::kSwizzleRedChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(CubeMapTexture::kSwizzleGreenChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(CubeMapTexture::kSwizzleBlueChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(CubeMapTexture::kSwizzleAlphaChanged));
}

TEST_F(CubeMapTextureTest, SetSubImage) {
  ImagePtr image1(new Image());
  ImagePtr image2(new Image());
  Point2ui corner1_2d(100, 12);
  Point3ui corner1(100, 12, 0);
  Point3ui corner2(0, 512, 10);

  for (int i = 0; i < 6; ++i) {
    SCOPED_TRACE(::testing::Message() << "Face " << i);
    CubeMapTexture::CubeFace face = static_cast<CubeMapTexture::CubeFace>(i);

    const base::AllocVector<Texture::SubImage>& images =
        texture_->GetSubImages(face);
    EXPECT_EQ(0U, images.size());
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    texture_->SetSubImage(face, 2U, corner1_2d, image1);
    EXPECT_EQ(1U, images.size());
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(
        CubeMapTexture::kNegativeXSubImageChanged + i));
    resource_->ResetModifiedBits();

    texture_->SetSubImage(face, 1U, corner2, image2);
    EXPECT_EQ(2U, images.size());
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(
        CubeMapTexture::kNegativeXSubImageChanged + i));
    resource_->ResetModifiedBits();

    // Check that the texture has an image.
    EXPECT_EQ(image1.Get(), images[0].image.Get());
    EXPECT_EQ(corner1, images[0].offset);
    EXPECT_EQ(2U, images[0].level);
    EXPECT_EQ(image2.Get(), images[1].image.Get());
    EXPECT_EQ(corner2, images[1].offset);
    EXPECT_EQ(1U, images[1].level);

    texture_->ClearSubImages(face);
    EXPECT_EQ(0U, images.size());
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }
}

TEST_F(CubeMapTextureTest, SetMipmapmage) {
  for (int i = 0; i < 6; ++i) {
    SCOPED_TRACE(::testing::Message() << "Face " << i);
    CubeMapTexture::CubeFace face = static_cast<CubeMapTexture::CubeFace>(i);
    const int mipmap_changed_bit = CubeMapTexture::kNegativeXMipmapChanged +
        i * static_cast<int>(kMipmapSlotCount);

    ImagePtr image(new Image());
    texture_->SetImage(face, 0U, image);

    // Check that the texture has an image.
    EXPECT_TRUE(texture_->HasImage(face, 0U));
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit));
    resource_->ResetModifiedBits();

    ImagePtr mipmap0(new Image());
    ImagePtr mipmap1(new Image());
    ImagePtr mipmap2(new Image());
    texture_->SetImage(face, 0U, mipmap0);
    EXPECT_TRUE(texture_->HasImage(face, 0));
    EXPECT_FALSE(texture_->HasImage(face, 1));
    EXPECT_EQ(1U, texture_->GetImageCount(face));
    EXPECT_EQ(mipmap0.Get(), texture_->GetImage(face, 0).Get());
    EXPECT_EQ(1U, resource_->GetModifiedBitCount());
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit));
    resource_->ResetModifiedBits();

    texture_->SetImage(face, 1U, mipmap1);
    EXPECT_EQ(2U, texture_->GetImageCount(face));
    EXPECT_TRUE(texture_->HasImage(face, 0));
    EXPECT_EQ(mipmap0.Get(), texture_->GetImage(face, 0).Get());
    EXPECT_TRUE(texture_->HasImage(face, 1));
    EXPECT_EQ(mipmap1.Get(), texture_->GetImage(face, 1).Get());
    // Only the new mipmap bit should be set.
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit + 1));
    resource_->ResetModifiedBits();

    texture_->SetImage(face, 2U, mipmap2);
    EXPECT_EQ(3U, texture_->GetImageCount(face));
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit + 2));
    EXPECT_TRUE(texture_->HasImage(face, 0));
    EXPECT_EQ(mipmap0.Get(), texture_->GetImage(face, 0).Get());
    EXPECT_TRUE(texture_->HasImage(face, 2));
    EXPECT_EQ(mipmap1.Get(), texture_->GetImage(face, 1).Get());
    EXPECT_TRUE(texture_->HasImage(face, 2));
    EXPECT_EQ(mipmap2.Get(), texture_->GetImage(face, 2).Get());
    resource_->ResetModifiedBits();

    texture_->SetImage(face, 0U, mipmap2);
    EXPECT_EQ(3U, texture_->GetImageCount(face));
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit));
    EXPECT_TRUE(texture_->HasImage(face, 0));
    EXPECT_EQ(mipmap2.Get(), texture_->GetImage(face, 0).Get());
    EXPECT_TRUE(texture_->HasImage(face, 2));
    EXPECT_EQ(mipmap1.Get(), texture_->GetImage(face, 1).Get());
    EXPECT_TRUE(texture_->HasImage(face, 2));
    EXPECT_EQ(mipmap2.Get(), texture_->GetImage(face, 2).Get());
    resource_->ResetModifiedBits();
  }
}

TEST_F(CubeMapTextureTest, Notifications) {
  // Check that modifying an Image or its DataContainer propagates to the
  // CubeMapTexture, and that changes to a Sampler also propagate to its owning
  // CubeMapTextures.
  ImagePtr image;
  ImagePtr mipmap0;
  ImagePtr mipmap2;
  SamplerPtr sampler;
  for (int i = 0; i < 6; ++i) {
    SCOPED_TRACE(::testing::Message() << "Face " << i);
    CubeMapTexture::CubeFace face = static_cast<CubeMapTexture::CubeFace>(i);
    const int mipmap_changed_bit = CubeMapTexture::kNegativeXMipmapChanged +
        i * static_cast<int>(kMipmapSlotCount);

    image.Reset(new Image());
    texture_->SetImage(face, 0U, image);
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit));
    resource_->ResetModifiedBit(mipmap_changed_bit);
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    sampler.Reset(new Sampler());
    texture_->SetSampler(sampler);
    EXPECT_TRUE(
        resource_->TestOnlyModifiedBit(CubeMapTexture::kSamplerChanged));
    resource_->ResetModifiedBit(CubeMapTexture::kSamplerChanged);
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    sampler->SetAutogenerateMipmapsEnabled(true);
    EXPECT_TRUE(
        resource_->TestOnlyModifiedBit(CubeMapTexture::kSamplerChanged));
    resource_->ResetModifiedBit(CubeMapTexture::kSamplerChanged);

    sampler->SetWrapT(Sampler::kClampToEdge);
    EXPECT_TRUE(
        resource_->TestOnlyModifiedBit(CubeMapTexture::kSamplerChanged));
    resource_->ResetModifiedBit(CubeMapTexture::kSamplerChanged);

    sampler->SetWrapT(Sampler::kClampToEdge);
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    EXPECT_EQ(1U, sampler->GetReceiverCount());
    texture_->SetSampler(SamplerPtr());
    EXPECT_EQ(0U, sampler->GetReceiverCount());
    EXPECT_TRUE(
        resource_->TestOnlyModifiedBit(CubeMapTexture::kSamplerChanged));
    resource_->ResetModifiedBit(CubeMapTexture::kSamplerChanged);
    sampler->SetWrapT(Sampler::kRepeat);
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    // Set the image.
    uint8 raw_data[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                          0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
    base::DataContainerPtr data = base::DataContainer::CreateAndCopy<uint8>(
        raw_data, 12U, false, image->GetAllocator());
    image->Set(Image::kRgb888, 2, 2, data);
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit));
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    // Try some mipmaps.
    mipmap0.Reset(new Image());
    mipmap2.Reset(new Image());
    texture_->SetImage(face, 0U, mipmap0);
    texture_->SetImage(face, 2U, mipmap2);
    // Use the same image for two mipmaps.
    texture_->SetImage(face, 3U, mipmap2);
    // Three bits are set since three different mipmaps changed.
    EXPECT_EQ(3U, resource_->GetModifiedBitCount());
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit));
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit + 2));
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit + 3));
    resource_->ResetModifiedBits();

    // The image should not be linked to the Texture anymore.
    image->Set(Image::kRgb888, 2, 2, base::DataContainerPtr());
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    mipmap0->Set(Image::kRgb888, 2, 2, data);
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit));
    resource_->ResetModifiedBits();
    mipmap2->Set(Image::kRgb888, 2, 2, data);
    EXPECT_EQ(2U, resource_->GetModifiedBitCount());
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit + 2));
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit + 3));
    resource_->ResetModifiedBits();

    // Change the DataContainer; it should trigger all mipmaps since they depend
    // on it.
    data->GetMutableData<void*>();
    EXPECT_EQ(3U, resource_->GetModifiedBitCount());
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit));
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit + 2));
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit + 3));
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());

    // Check that removals occur properly.
    texture_->SetImage(face, 0U, image);
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit));
    resource_->ResetModifiedBits();
    data->GetMutableData<void*>();
    EXPECT_EQ(2U, resource_->GetModifiedBitCount());
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit + 2));
    EXPECT_TRUE(resource_->TestModifiedBit(mipmap_changed_bit + 3));
    resource_->ResetModifiedBits();
    image->Set(Image::kRgb888, 2, 2, data);
    EXPECT_TRUE(resource_->TestOnlyModifiedBit(mipmap_changed_bit));
    resource_->ResetModifiedBits();
  }

  texture_->SetSampler(sampler);
  EXPECT_EQ(1U, sampler->GetReceiverCount());
  EXPECT_EQ(1U, image->GetReceiverCount());
  EXPECT_EQ(1U, mipmap2->GetReceiverCount());
  texture_.Reset();
  EXPECT_EQ(0U, sampler->GetReceiverCount());
  EXPECT_EQ(0U, image->GetReceiverCount());
  EXPECT_EQ(0U, mipmap0->GetReceiverCount());
  EXPECT_EQ(0U, mipmap2->GetReceiverCount());}

}  // namespace gfx
}  // namespace ion
