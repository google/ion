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

#include "ion/gfx/texture.h"

#include <memory>

#include "ion/base/datacontainer.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/image.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/tests/mockresource.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

using math::Point2ui;
using math::Point3ui;

// Struct definition used to test Texture::ExpectedDimensionsForMipmap.
struct MipmapDefaults {
  static const uint32 kDefaultBaseSize;
  static const uint32 kDefaultMipmapSize;
  static const uint32 kDefaultLevel;
  static const uint32 kExpectedWidth;
  static const uint32 kExpectedHeight;

  MipmapDefaults() { Reset(); }
  void Reset();
  bool SetExpected(uint32* expected_width, uint32* expected_height);

  std::stringstream msg_stream;
  uint32 base_width_;
  uint32 base_height_;
  uint32 mipmap_width_;
  uint32 mipmap_height_;
  uint32 mipmap_level_;
};

const uint32 MipmapDefaults::kDefaultBaseSize = 64;
const uint32 MipmapDefaults::kDefaultMipmapSize = 16;
const uint32 MipmapDefaults::kDefaultLevel = 2;
const uint32 MipmapDefaults::kExpectedWidth =
    MipmapDefaults::kDefaultMipmapSize;
const uint32 MipmapDefaults::kExpectedHeight =
    MipmapDefaults::kDefaultMipmapSize;


void MipmapDefaults::Reset() {
  msg_stream.str("");
  msg_stream.clear();
  base_width_ = kDefaultBaseSize;
  base_height_ = kDefaultBaseSize;
  mipmap_width_ = kDefaultMipmapSize;
  mipmap_height_ = kDefaultMipmapSize;
  mipmap_level_ = kDefaultLevel;
}

bool MipmapDefaults::SetExpected(uint32* expected_width,
                                 uint32* expected_height) {
  return Texture::ExpectedDimensionsForMipmap(mipmap_width_,
                                              mipmap_height_,
                                              mipmap_level_,
                                              base_width_,
                                              base_height_,
                                              expected_width,
                                              expected_height);
}

typedef testing::MockResource<Texture::kNumChanges> MockTextureResource;

}  // anonymous namespace

class TextureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    texture_.Reset(new Texture());
    resource_ = absl::make_unique<MockTextureResource>();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    texture_->SetResource(0U, 0, resource_.get());
    EXPECT_EQ(resource_.get(), texture_->GetResource(0U, 0));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { texture_.Reset(nullptr); }

  TexturePtr texture_;
  std::unique_ptr<MockTextureResource> resource_;
};

TEST_F(TextureTest, DefaultModes) {
  // Check that the texture does not have an Image.
  EXPECT_FALSE(texture_->HasImage(0U));
  // Check that the texture does not have an Sampler.
  EXPECT_FALSE(texture_->GetSampler());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(TextureTest, SetImage) {
  ImagePtr image(new Image());
  texture_->SetImage(0U, image);

  // Check that the texture has an image.
  EXPECT_TRUE(texture_->HasImage(0U));

  // Check that the image is the one we set.
  EXPECT_EQ(image.Get(), texture_->GetImage(0U).Get());

  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged));
}

TEST_F(TextureTest, SetSampler) {
  SamplerPtr sampler(new Sampler());
  texture_->SetSampler(sampler);

  // Check that the texture has an Sampler and that it is the one we set.
  EXPECT_EQ(sampler.Get(), texture_->GetSampler().Get());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSamplerChanged));
}

TEST_F(TextureTest, ImmutableTextures) {
  base::LogChecker log_checker;

  SamplerPtr sampler(new Sampler());
  texture_->SetSampler(sampler);
  resource_->ResetModifiedBits();

  EXPECT_FALSE(texture_->GetImmutableImage());
  EXPECT_EQ(0U, texture_->GetImmutableLevels());
  EXPECT_FALSE(texture_->IsProtected());

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
  texture_->SetImage(0U, unused_image);
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
  EXPECT_EQ(image.Get(), texture_->GetImage(0U).Get());
  EXPECT_EQ(image.Get(), texture_->GetImage(1U).Get());
  EXPECT_FALSE(texture_->GetImage(2U));

  ImagePtr image2(new Image());
  EXPECT_FALSE(texture_->SetImmutableImage(image2, 4U));
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "SetImmutableImage() called on an already immutable"));
  EXPECT_EQ(image.Get(), texture_->GetImmutableImage().Get());
  EXPECT_EQ(2U, texture_->GetImmutableLevels());
  EXPECT_TRUE(texture_->IsProtected());

  // Calling SetImage() on an immutable Texture is an error.
  EXPECT_TRUE(texture_->HasImage(0U));
  EXPECT_EQ(image.Get(), texture_->GetImage(0U).Get());
  texture_->SetImage(0U, image2);
  EXPECT_TRUE(
      log_checker.HasMessage("ERROR", "SetImage() called on immutable"));
  EXPECT_TRUE(texture_->HasImage(0U));
  EXPECT_EQ(image.Get(), texture_->GetImage(0U).Get());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(TextureTest, MipmapLevels) {
  EXPECT_EQ(0, texture_->GetBaseLevel());
  texture_->SetBaseLevel(1);
  EXPECT_EQ(1, texture_->GetBaseLevel());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kBaseLevelChanged));
  resource_->ResetModifiedBits();
  texture_->SetBaseLevel(12);
  EXPECT_EQ(12, texture_->GetBaseLevel());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kBaseLevelChanged));
  resource_->ResetModifiedBits();
  texture_->SetBaseLevel(12);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_EQ(1000, texture_->GetMaxLevel());
  texture_->SetMaxLevel(120);
  EXPECT_EQ(120, texture_->GetMaxLevel());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMaxLevelChanged));
  resource_->ResetModifiedBits();
  texture_->SetMaxLevel(456);
  EXPECT_EQ(456, texture_->GetMaxLevel());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMaxLevelChanged));
  resource_->ResetModifiedBits();
  texture_->SetMaxLevel(456);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(TextureTest, Swizzles) {
  EXPECT_EQ(Texture::kRed, texture_->GetSwizzleRed());
  EXPECT_EQ(Texture::kGreen, texture_->GetSwizzleGreen());
  EXPECT_EQ(Texture::kBlue, texture_->GetSwizzleBlue());
  EXPECT_EQ(Texture::kAlpha, texture_->GetSwizzleAlpha());

  texture_->SetSwizzleRed(Texture::kGreen);
  EXPECT_EQ(Texture::kGreen, texture_->GetSwizzleRed());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSwizzleRedChanged));
  texture_->SetSwizzleRed(Texture::kGreen);
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSwizzleGreen(Texture::kBlue);
  EXPECT_EQ(Texture::kBlue, texture_->GetSwizzleGreen());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSwizzleGreenChanged));
  texture_->SetSwizzleGreen(Texture::kBlue);
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSwizzleBlue(Texture::kAlpha);
  EXPECT_EQ(Texture::kAlpha, texture_->GetSwizzleBlue());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSwizzleBlueChanged));
  texture_->SetSwizzleBlue(Texture::kAlpha);
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSwizzleAlpha(Texture::kRed);
  EXPECT_EQ(Texture::kRed, texture_->GetSwizzleAlpha());
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSwizzleAlphaChanged));
  texture_->SetSwizzleAlpha(Texture::kRed);
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSwizzles(
      Texture::kRed, Texture::kGreen, Texture::kBlue, Texture::kAlpha);
  EXPECT_EQ(Texture::kRed, texture_->GetSwizzleRed());
  EXPECT_EQ(Texture::kGreen, texture_->GetSwizzleGreen());
  EXPECT_EQ(Texture::kBlue, texture_->GetSwizzleBlue());
  EXPECT_EQ(Texture::kAlpha, texture_->GetSwizzleAlpha());
  EXPECT_EQ(4U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kSwizzleRedChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kSwizzleGreenChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kSwizzleBlueChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kSwizzleAlphaChanged));
}

TEST_F(TextureTest, SetSubImage) {
  ImagePtr image1(new Image());
  ImagePtr image2(new Image());
  Point2ui corner1_2d(100, 12);
  Point3ui corner1(100, 12, 0);
  Point3ui corner2(0, 512, 10);

  const base::AllocVector<Texture::SubImage>& images = texture_->GetSubImages();
  EXPECT_EQ(0U, images.size());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  texture_->SetSubImage(2U, corner1_2d, image1);
  EXPECT_EQ(1U, images.size());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSubImageChanged));
  resource_->ResetModifiedBits();

  texture_->SetSubImage(1U, corner2, image2);
  EXPECT_EQ(2U, images.size());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSubImageChanged));
  resource_->ResetModifiedBits();

  // Check that the texture has an image.
  EXPECT_EQ(image1.Get(), images[0].image.Get());
  EXPECT_EQ(corner1, images[0].offset);
  EXPECT_EQ(2U, images[0].level);
  EXPECT_EQ(image2.Get(), images[1].image.Get());
  EXPECT_EQ(corner2, images[1].offset);
  EXPECT_EQ(1U, images[1].level);

  texture_->ClearSubImages();
  EXPECT_EQ(0U, images.size());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(TextureTest, SetMipmapImage) {
  ImagePtr image(new Image());
  texture_->SetImage(0U, image);

  // Check that the texture has an image.
  EXPECT_TRUE(texture_->HasImage(0U));
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged));
  resource_->ResetModifiedBits();

  ImagePtr mipmap0(new Image());
  ImagePtr mipmap1(new Image());
  ImagePtr mipmap2(new Image());
  texture_->SetImage(0U, mipmap0);
  EXPECT_TRUE(texture_->HasImage(0));
  EXPECT_FALSE(texture_->HasImage(1));
  EXPECT_EQ(mipmap0.Get(), texture_->GetImage(0).Get());
  EXPECT_EQ(1U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged));
  resource_->ResetModifiedBits();

  texture_->SetImage(1U, mipmap1);
  EXPECT_TRUE(texture_->HasImage(0));
  EXPECT_EQ(mipmap0.Get(), texture_->GetImage(0).Get());
  EXPECT_TRUE(texture_->HasImage(1));
  EXPECT_EQ(mipmap1.Get(), texture_->GetImage(1).Get());
  // Only the new mipmap bit should be set.
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged + 1));
  resource_->ResetModifiedBits();

  texture_->SetImage(2U, mipmap2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged + 2));
  EXPECT_TRUE(texture_->HasImage(0));
  EXPECT_EQ(mipmap0.Get(), texture_->GetImage(0).Get());
  EXPECT_TRUE(texture_->HasImage(2));
  EXPECT_EQ(mipmap1.Get(), texture_->GetImage(1).Get());
  EXPECT_TRUE(texture_->HasImage(2));
  EXPECT_EQ(mipmap2.Get(), texture_->GetImage(2).Get());
  resource_->ResetModifiedBits();

  texture_->SetImage(0U, mipmap2);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged));
  EXPECT_TRUE(texture_->HasImage(0));
  EXPECT_EQ(mipmap2.Get(), texture_->GetImage(0).Get());
  EXPECT_TRUE(texture_->HasImage(2));
  EXPECT_EQ(mipmap1.Get(), texture_->GetImage(1).Get());
  EXPECT_TRUE(texture_->HasImage(2));
  EXPECT_EQ(mipmap2.Get(), texture_->GetImage(2).Get());
}

TEST_F(TextureTest, Notifications) {
  // Check that modifying an Image or its DataContainer propagates to the
  // Texture, and that changes to a Sampler also propagate to its owning
  // Textures.
  ImagePtr image(new Image());
  texture_->SetImage(0U, image);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged));
  resource_->ResetModifiedBit(Texture::kMipmapChanged);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  SamplerPtr sampler(new Sampler());
  texture_->SetSampler(sampler);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSamplerChanged));
  resource_->ResetModifiedBit(Texture::kSamplerChanged);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  sampler->SetAutogenerateMipmapsEnabled(true);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSamplerChanged));
  resource_->ResetModifiedBit(Texture::kSamplerChanged);

  sampler->SetWrapT(Sampler::kClampToEdge);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSamplerChanged));
  resource_->ResetModifiedBit(Texture::kSamplerChanged);

  sampler->SetWrapT(Sampler::kClampToEdge);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  EXPECT_EQ(1U, sampler->GetReceiverCount());
  texture_->SetSampler(SamplerPtr());
  EXPECT_EQ(0U, sampler->GetReceiverCount());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kSamplerChanged));
  resource_->ResetModifiedBit(Texture::kSamplerChanged);
  sampler->SetWrapT(Sampler::kRepeat);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Set the image.
  uint8 raw_data[12] = { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                         0x7, 0x8, 0x9, 0xa, 0xb, 0xc };
  base::DataContainerPtr data = base::DataContainer::CreateAndCopy<uint8>(
      raw_data, 12U, false, image->GetAllocator());
  image->Set(Image::kRgb888, 2, 2, data);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged));
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Try some mipmaps.
  ImagePtr mipmap0(new Image());
  ImagePtr mipmap2(new Image());
  texture_->SetImage(0U, mipmap0);
  texture_->SetImage(2U, mipmap2);
  // Use the same image for two mipmaps.
  texture_->SetImage(3U, mipmap2);
  // Three bits are set since three different mipmaps changed.
  EXPECT_EQ(3U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged + 2));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged + 3));
  resource_->ResetModifiedBits();

  // The image should not be linked to the Texture anymore.
  image->Set(Image::kRgb888, 2, 2, base::DataContainerPtr());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  mipmap0->Set(Image::kRgb888, 2, 2, data);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged));
  resource_->ResetModifiedBits();
  mipmap2->Set(Image::kRgb888, 2, 2, data);
  EXPECT_EQ(2U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged + 2));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged + 3));
  resource_->ResetModifiedBits();

  // Change the DataContainer; it should trigger all mipmaps since they depend
  // on it.
  data->GetMutableData<void*>();
  EXPECT_EQ(3U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged + 2));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged + 3));
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Check that removals occur properly.
  EXPECT_EQ(1U, mipmap0->GetReceiverCount());
  texture_->SetImage(0U, image);
  EXPECT_EQ(0U, mipmap0->GetReceiverCount());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged));
  resource_->ResetModifiedBits();
  data->GetMutableData<void*>();
  EXPECT_EQ(2U, resource_->GetModifiedBitCount());
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged + 2));
  EXPECT_TRUE(resource_->TestModifiedBit(Texture::kMipmapChanged + 3));
  resource_->ResetModifiedBits();
  image->Set(Image::kRgb888, 2, 2, data);
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Texture::kMipmapChanged));
  resource_->ResetModifiedBits();

  texture_->SetSampler(sampler);
  EXPECT_EQ(1U, sampler->GetReceiverCount());
  EXPECT_EQ(1U, image->GetReceiverCount());
  EXPECT_EQ(1U, mipmap2->GetReceiverCount());
  texture_.Reset();
  EXPECT_EQ(0U, sampler->GetReceiverCount());
  EXPECT_EQ(0U, image->GetReceiverCount());
  EXPECT_EQ(0U, mipmap0->GetReceiverCount());
  EXPECT_EQ(0U, mipmap2->GetReceiverCount());
}

TEST_F(TextureTest, ExpectedDimensionsForMipmap) {
  MipmapDefaults mipmap_defaults;
  base::LogChecker log_checker;
  uint32 expected_width;
  uint32 expected_height;

  // Test for NPOT dimensioned mipmap.
  mipmap_defaults.mipmap_width_ -= 1;
  EXPECT_FALSE(mipmap_defaults.SetExpected(&expected_width, &expected_height));
  mipmap_defaults.msg_stream
      << "Mipmap width: " << mipmap_defaults.mipmap_width_
      << " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     mipmap_defaults.msg_stream.str()));
  EXPECT_EQ(0U, expected_width);
  EXPECT_EQ(0U, expected_height);
  mipmap_defaults.Reset();

  // Test for excessive mipmap level.
  mipmap_defaults.mipmap_level_ = math::Log2(mipmap_defaults.base_width_) + 1U;
  EXPECT_FALSE(mipmap_defaults.SetExpected(&expected_width, &expected_height));
  mipmap_defaults.msg_stream
      << "Mipmap level is: " << mipmap_defaults.mipmap_level_
      << " but maximum level is: " << math::Log2(mipmap_defaults.base_width_);
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     mipmap_defaults.msg_stream.str()));
  EXPECT_EQ(0U, expected_width);
  EXPECT_EQ(0U, expected_height);
  mipmap_defaults.Reset();

  // Test for incorrect dimensions.
  mipmap_defaults.mipmap_height_ = mipmap_defaults.mipmap_width_ * 2U;
  mipmap_defaults.base_width_ *= 2U;
  mipmap_defaults.base_height_ = mipmap_defaults.base_width_ * 2U;
  EXPECT_FALSE(mipmap_defaults.SetExpected(&expected_width, &expected_height));
  mipmap_defaults.msg_stream
      << "Mipmap level " << mipmap_defaults.mipmap_level_ << " has incorrect "
      << "dimensions [" << mipmap_defaults.mipmap_width_ << "x"
      << mipmap_defaults.mipmap_height_ << "], expected [" << expected_width
      << "x" << expected_height << "].  Base dimensions: ("
      << mipmap_defaults.base_width_ << ", " << mipmap_defaults.base_height_
      << ").  Ignoring.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     mipmap_defaults.msg_stream.str()));
  EXPECT_EQ(32U, expected_width);
  EXPECT_EQ(64U, expected_height);
  mipmap_defaults.Reset();

  // Test for failure in congruency with base dimensions.
  mipmap_defaults.mipmap_width_ >>= 1;
  EXPECT_FALSE(mipmap_defaults.SetExpected(&expected_width, &expected_height));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad aspect ratio for mipmap."));
  EXPECT_EQ(0U, expected_width);
  EXPECT_EQ(0U, expected_height);
  mipmap_defaults.Reset();

  // Test for success in the square case.
  EXPECT_TRUE(mipmap_defaults.SetExpected(&expected_width, &expected_height));
  EXPECT_EQ(expected_width, MipmapDefaults::kExpectedWidth);
  EXPECT_EQ(expected_height, MipmapDefaults::kExpectedHeight);
  mipmap_defaults.Reset();

  // Test for success in the non-square case.
  mipmap_defaults.base_width_ >>= 1;
  mipmap_defaults.mipmap_width_ >>= 1;
  EXPECT_TRUE(mipmap_defaults.SetExpected(&expected_width, &expected_height));
  EXPECT_EQ(expected_width, MipmapDefaults::kExpectedWidth >> 1);
  EXPECT_EQ(expected_height, MipmapDefaults::kExpectedHeight);
  mipmap_defaults.Reset();

  // Test 2x1 aspect rectangle at level n - 2.  The level size should be (2x1).
  mipmap_defaults.base_height_ >>= 1;
  mipmap_defaults.mipmap_width_ = 2;
  mipmap_defaults.mipmap_height_ = 1;
  mipmap_defaults.mipmap_level_ = math::Log2(mipmap_defaults.base_width_) - 1U;
  EXPECT_TRUE(mipmap_defaults.SetExpected(&expected_width, &expected_height));
  EXPECT_EQ(2U, expected_width);
  EXPECT_EQ(1U, expected_height);
  mipmap_defaults.Reset();

  // Test 2x1 aspect rectangle at level n - 1.  The level size should be (1x1).
  mipmap_defaults.base_height_ >>= 1;
  mipmap_defaults.mipmap_width_ = 1;
  mipmap_defaults.mipmap_height_ = 1;
  mipmap_defaults.mipmap_level_ = math::Log2(mipmap_defaults.base_width_);
  EXPECT_TRUE(mipmap_defaults.SetExpected(&expected_width, &expected_height));
  EXPECT_EQ(1U, expected_width);
  EXPECT_EQ(1U, expected_height);
  mipmap_defaults.Reset();
}

TEST_F(TextureTest, MultisamplingState) {
  base::LogChecker log_checker;

  // Check default state.
  EXPECT_EQ(0, texture_->GetMultisampleSamples());
  EXPECT_TRUE(texture_->IsMultisampleFixedSampleLocations());

  // Change state, check modified bit.
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  texture_->SetMultisampling(4, false);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());

  // Check changed state.
  EXPECT_EQ(4, texture_->GetMultisampleSamples());
  EXPECT_FALSE(texture_->IsMultisampleFixedSampleLocations());

  // Set to same state, check no modified bit.
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  texture_->SetMultisampling(4, false);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Revert state, check modified bit.
  resource_->ResetModifiedBits();
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  texture_->SetMultisampling(0, true);
  EXPECT_TRUE(resource_->AnyModifiedBitsSet());

  // Check reverted state.
  EXPECT_EQ(0, texture_->GetMultisampleSamples());
  EXPECT_TRUE(texture_->IsMultisampleFixedSampleLocations());

  // Bad multisampling samples (< 0). Should log a warning and have no effect.
  texture_->SetMultisampling(-19, true);
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "Ignoring bad number of samples: -19"));
  EXPECT_EQ(0, texture_->GetMultisampleSamples());
  EXPECT_TRUE(texture_->IsMultisampleFixedSampleLocations());
}

}  // namespace gfx
}  // namespace ion
