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

#include "ion/gfx/image.h"

#include "ion/base/enumhelper.h"
#include "ion/base/logchecker.h"
#include "ion/port/nullptr.h"  // For kNullFunction.
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

const Image::Format kAstcFormatTable[] = {
    Image::kAstc4x4Rgba,    Image::kAstc5x4Rgba,    Image::kAstc5x5Rgba,
    Image::kAstc6x5Rgba,    Image::kAstc6x6Rgba,    Image::kAstc8x5Rgba,
    Image::kAstc8x6Rgba,    Image::kAstc8x8Rgba,    Image::kAstc10x5Rgba,
    Image::kAstc10x6Rgba,   Image::kAstc10x8Rgba,   Image::kAstc10x10Rgba,
    Image::kAstc12x10Rgba,  Image::kAstc12x12Rgba,  Image::kAstc4x4Srgba,
    Image::kAstc5x4Srgba,   Image::kAstc5x5Srgba,   Image::kAstc6x5Srgba,
    Image::kAstc6x6Srgba,   Image::kAstc8x5Srgba,   Image::kAstc8x6Srgba,
    Image::kAstc8x8Srgba,   Image::kAstc10x5Srgba,  Image::kAstc10x6Srgba,
    Image::kAstc10x8Srgba,  Image::kAstc10x10Srgba, Image::kAstc12x10Srgba,
    Image::kAstc12x12Srgba,
};

// Returns an invalid format for error-checking.
static Image::Format InvalidFormat() {
  int invalid_format = 15055;
  return static_cast<Image::Format>(invalid_format);
}

// Verifies that a particular Image::Format takes up a certain number of bytes
// per pixel.
::testing::AssertionResult VerifyDataSize(uint32 bytes_per_pixel,
                                          Image::Format format) {
  if (Image::ComputeDataSize(format, 0U, 0U))
    return ::testing::AssertionFailure()
           << "Expected image with format " << Image::GetFormatString(format)
           << " but 0 dimensions to have 0 size, but it has size "
           << Image::ComputeDataSize(format, 0U, 0U);
  if (Image::ComputeDataSize(format, 0U, 0U, 0U))
    return ::testing::AssertionFailure()
           << "Expected image with format " << Image::GetFormatString(format)
           << " but 0 dimensions to have 0 size, but it has size "
           << Image::ComputeDataSize(format, 0U, 0U, 0U);
  if (Image::ComputeDataSize(format, 0U, 16U))
    return ::testing::AssertionFailure()
           << "Expected image with format " << Image::GetFormatString(format)
           << " but 0 width to have 0 size, but it has size "
           << Image::ComputeDataSize(format, 0U, 16U);
  if (Image::ComputeDataSize(format, 20U, 0U))
    return ::testing::AssertionFailure()
           << "Expected image with format " << Image::GetFormatString(format)
           << " but 0 height to have 0 size, but it has size "
           << Image::ComputeDataSize(format, 20U, 0U);
  if (Image::ComputeDataSize(format, 20U, 0U, 2U))
    return ::testing::AssertionFailure()
           << "Expected image with format " << Image::GetFormatString(format)
           << " but 0 height to have 0 size, but it has size "
           << Image::ComputeDataSize(format, 20U, 0U);
  const uint32 expected_bytes = bytes_per_pixel * 20U * 16U;
  if (Image::ComputeDataSize(format, 20U, 16U) != expected_bytes)
    return ::testing::AssertionFailure()
        << "Expected image with format " << Image::GetFormatString(format)
        << " and dimensions 20x16 to require " << expected_bytes
        << " bytes, but it requires "
        << Image::ComputeDataSize(format, 20U, 16U);
  const uint32 expected_bytes_3d = bytes_per_pixel * 20U * 16U * 5U;
  if (Image::ComputeDataSize(format, 20U, 16U, 5U) != expected_bytes_3d)
    return ::testing::AssertionFailure()
        << "Expected image with format " << Image::GetFormatString(format)
        << " and dimensions 20x16x5 to require " << expected_bytes
        << " bytes, but it requires "
        << Image::ComputeDataSize(format, 20U, 16U, 5U);
  return ::testing::AssertionSuccess();
}

}  // anonymous namespace

TEST(ImageTest, Set) {
  ImagePtr image(new Image);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(0U, image->GetWidth());
  EXPECT_EQ(0U, image->GetHeight());
  EXPECT_EQ(0U, image->GetDepth());
  EXPECT_EQ(0U, image->GetDataSize());
  EXPECT_EQ(Image::kDense, image->GetType());
  EXPECT_EQ(Image::k2d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  base::DataContainerPtr data = base::DataContainer::Create<uint8>(
      nullptr, kNullFunction, false, image->GetAllocator());

  // Test some basic formats.
  image->Set(Image::kRgb888, 4, 4, data);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(1U, image->GetDepth());
  EXPECT_EQ(data, image->GetData());
  EXPECT_EQ(Image::kDense, image->GetType());
  EXPECT_EQ(Image::k2d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  image->Set(Image::kRgba8888, 4, 4, data);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(1U, image->GetDepth());
  EXPECT_EQ(data, image->GetData());
  EXPECT_EQ(Image::kDense, image->GetType());
  EXPECT_EQ(Image::k2d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  // 3D images.
  image->Set(Image::kRgb888, 4, 4, 4, data);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(4U, image->GetDepth());
  EXPECT_EQ(data, image->GetData());
  EXPECT_EQ(Image::kDense, image->GetType());
  EXPECT_EQ(Image::k3d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  image->Set(Image::kRgba8888, 4, 4, 16, data);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(16U, image->GetDepth());
  EXPECT_EQ(data, image->GetData());
  EXPECT_EQ(Image::kDense, image->GetType());
  EXPECT_EQ(Image::k3d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  EXPECT_EQ(0U, image->GetDataSize());
  static const uint8 k2dData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                                    0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  data = base::DataContainer::CreateAndCopy<uint8>(
      k2dData, 12U, false, image->GetAllocator());
  image->Set(Image::kRgb888, 2, 2, data);
  EXPECT_EQ(Image::k2d, image->GetDimensions());
  EXPECT_EQ(12U, image->GetDataSize());

  static const uint8 k3dData[24] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
                                    0x9, 0xa, 0xb, 0xc, 0x1, 0x2, 0x3, 0x4,
                                    0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  data = base::DataContainer::CreateAndCopy<uint8>(
      k3dData, 24U, false, image->GetAllocator());
  image->Set(Image::kRgb888, 2, 2, 2, data);
  EXPECT_EQ(Image::k3d, image->GetDimensions());
  EXPECT_EQ(24U, image->GetDataSize());
}

TEST(ImageTest, SetArray) {
  ImagePtr image(new Image);
  base::DataContainerPtr data = base::DataContainer::Create<uint8>(
      nullptr, kNullFunction, false, image->GetAllocator());

  // Test some basic formats.
  image->SetArray(Image::kRgb888, 4, 4, data);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(1U, image->GetDepth());
  EXPECT_EQ(data, image->GetData());
  EXPECT_EQ(Image::kArray, image->GetType());
  EXPECT_EQ(Image::k2d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  image->SetArray(Image::kRgba8888, 4, 4, data);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(1U, image->GetDepth());
  EXPECT_EQ(data, image->GetData());
  EXPECT_EQ(Image::kArray, image->GetType());
  EXPECT_EQ(Image::k2d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  // 3D images.
  image->SetArray(Image::kRgb888, 4, 4, 4, data);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(4U, image->GetDepth());
  EXPECT_EQ(data, image->GetData());
  EXPECT_EQ(Image::kArray, image->GetType());
  EXPECT_EQ(Image::k3d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  image->SetArray(Image::kRgba8888, 4, 4, 16, data);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(16U, image->GetDepth());
  EXPECT_EQ(data, image->GetData());
  EXPECT_EQ(Image::kArray, image->GetType());
  EXPECT_EQ(Image::k3d, image->GetDimensions());
  EXPECT_FALSE(image->IsCompressed());

  EXPECT_EQ(0U, image->GetDataSize());
  static const uint8 k2dData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                                    0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  data = base::DataContainer::CreateAndCopy<uint8>(
      k2dData, 12U, false, image->GetAllocator());
  image->SetArray(Image::kRgb888, 2, 2, data);
  EXPECT_EQ(Image::k2d, image->GetDimensions());
  EXPECT_EQ(12U, image->GetDataSize());

  static const uint8 k3dData[24] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8,
                                    0x9, 0xa, 0xb, 0xc, 0x1, 0x2, 0x3, 0x4,
                                    0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  data = base::DataContainer::CreateAndCopy<uint8>(
      k3dData, 24U, false, image->GetAllocator());
  image->SetArray(Image::kRgb888, 2, 2, 2, data);
  EXPECT_EQ(Image::k3d, image->GetDimensions());
  EXPECT_EQ(24U, image->GetDataSize());
}

TEST(ImageTest, Notifications) {
  ImagePtr image(new Image);
  base::DataContainerPtr data = base::DataContainer::Create<uint8>(
      nullptr, kNullFunction, false, image->GetAllocator());

  image->Set(Image::kRgb888, 4, 4, data);
  EXPECT_EQ(1U, data->GetReceiverCount());
  image.Reset();
  EXPECT_EQ(0U, data->GetReceiverCount());
}

TEST(ImageTest, SetEglImage) {
  ImagePtr image(new Image);

  // Construct a data container to wrap the pointer.
  base::DataContainerPtr data = base::DataContainer::Create<void>(
      nullptr, kNullFunction, false, image->GetAllocator());

  // Test some basic formats.
  image->SetEglImage(data);
  EXPECT_EQ(Image::kEglImage, image->GetFormat());
  EXPECT_EQ(Image::kEgl, image->GetType());
  EXPECT_EQ(0U, image->GetWidth());
  EXPECT_EQ(0U, image->GetHeight());
  EXPECT_EQ(0U, image->GetDepth());
  ASSERT_TRUE(image->GetData());
  EXPECT_TRUE(image->GetData()->GetData() == nullptr);
  EXPECT_EQ(0U, image->GetDataSize());

  // This is deliberately not const since the interface to Set*EglImage requires
  // a non-const pointer, similar to the GL spec.
  static uint8 kData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                            0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  data = base::DataContainer::Create<void>(kData, kNullFunction, false,
                                           image->GetAllocator());
  image->SetExternalEglImage(data);
  EXPECT_EQ(Image::kEglImage, image->GetFormat());
  EXPECT_EQ(Image::kExternalEgl, image->GetType());
  EXPECT_EQ(0U, image->GetWidth());
  EXPECT_EQ(0U, image->GetHeight());
  EXPECT_EQ(0U, image->GetDepth());
  ASSERT_TRUE(image->GetData());
  EXPECT_EQ(kData, image->GetData()->GetData<uint8>());
}

TEST(ImageTest, GetFormatString) {
  EXPECT_EQ(0, strcmp("Alpha", Image::GetFormatString(Image::kAlpha)));
  EXPECT_EQ(0, strcmp("Luminance", Image::GetFormatString(Image::kLuminance)));
  EXPECT_EQ(
      0,
      strcmp("LuminanceAlpha", Image::GetFormatString(Image::kLuminanceAlpha)));
  EXPECT_EQ(0, strcmp("Rgb888", Image::GetFormatString(Image::kRgb888)));
  EXPECT_EQ(0, strcmp("Rgba8888", Image::GetFormatString(Image::kRgba8888)));
  EXPECT_EQ(0, strcmp("Rgb565", Image::GetFormatString(Image::kRgb565)));
  EXPECT_EQ(0, strcmp("Rgba4444", Image::GetFormatString(Image::kRgba4444)));
  EXPECT_EQ(0, strcmp("Rgba5551", Image::GetFormatString(Image::kRgba5551)));
  EXPECT_EQ(0, strcmp("R8", Image::GetFormatString(Image::kR8)));
  EXPECT_EQ(0, strcmp("RSigned8", Image::GetFormatString(Image::kRSigned8)));
  EXPECT_EQ(0, strcmp("R8i", Image::GetFormatString(Image::kR8i)));
  EXPECT_EQ(0, strcmp("R8ui", Image::GetFormatString(Image::kR8ui)));
  EXPECT_EQ(0, strcmp("R16fFloat", Image::GetFormatString(Image::kR16fFloat)));
  EXPECT_EQ(0, strcmp("R16fHalf", Image::GetFormatString(Image::kR16fHalf)));
  EXPECT_EQ(0, strcmp("R16i", Image::GetFormatString(Image::kR16i)));
  EXPECT_EQ(0, strcmp("R16ui", Image::GetFormatString(Image::kR16ui)));
  EXPECT_EQ(0, strcmp("R32f", Image::GetFormatString(Image::kR32f)));
  EXPECT_EQ(0, strcmp("R32i", Image::GetFormatString(Image::kR32i)));
  EXPECT_EQ(0, strcmp("R32ui", Image::GetFormatString(Image::kR32ui)));
  EXPECT_EQ(0, strcmp("Rg8", Image::GetFormatString(Image::kRg8)));
  EXPECT_EQ(0, strcmp("RgSigned8", Image::GetFormatString(Image::kRgSigned8)));
  EXPECT_EQ(0, strcmp("Rg8i", Image::GetFormatString(Image::kRg8i)));
  EXPECT_EQ(0, strcmp("Rg8ui", Image::GetFormatString(Image::kRg8ui)));
  EXPECT_EQ(0,
            strcmp("Rg16fFloat", Image::GetFormatString(Image::kRg16fFloat)));
  EXPECT_EQ(0, strcmp("Rg16fHalf", Image::GetFormatString(Image::kRg16fHalf)));
  EXPECT_EQ(0, strcmp("Rg16i", Image::GetFormatString(Image::kRg16i)));
  EXPECT_EQ(0, strcmp("Rg16ui", Image::GetFormatString(Image::kRg16ui)));
  EXPECT_EQ(0, strcmp("Rg32f", Image::GetFormatString(Image::kRg32f)));
  EXPECT_EQ(0, strcmp("Rg32i", Image::GetFormatString(Image::kRg32i)));
  EXPECT_EQ(0, strcmp("Rg32ui", Image::GetFormatString(Image::kRg32ui)));
  EXPECT_EQ(0, strcmp("Rgb8", Image::GetFormatString(Image::kRgb8)));
  EXPECT_EQ(0,
            strcmp("RgbSigned8", Image::GetFormatString(Image::kRgbSigned8)));
  EXPECT_EQ(0, strcmp("Rgb8i", Image::GetFormatString(Image::kRgb8i)));
  EXPECT_EQ(0, strcmp("Rgb8ui", Image::GetFormatString(Image::kRgb8ui)));
  EXPECT_EQ(0,
            strcmp("Rgb16fFloat", Image::GetFormatString(Image::kRgb16fFloat)));
  EXPECT_EQ(0,
            strcmp("Rgb16fHalf", Image::GetFormatString(Image::kRgb16fHalf)));
  EXPECT_EQ(0, strcmp("Rgb16i", Image::GetFormatString(Image::kRgb16i)));
  EXPECT_EQ(0, strcmp("Rgb16ui", Image::GetFormatString(Image::kRgb16ui)));
  EXPECT_EQ(0, strcmp("Rgb32f", Image::GetFormatString(Image::kRgb32f)));
  EXPECT_EQ(0, strcmp("Rgb32i", Image::GetFormatString(Image::kRgb32i)));
  EXPECT_EQ(0, strcmp("Rgb32ui", Image::GetFormatString(Image::kRgb32ui)));
  EXPECT_EQ(0, strcmp("Rgba8", Image::GetFormatString(Image::kRgba8)));
  EXPECT_EQ(0,
            strcmp("RgbaSigned8", Image::GetFormatString(Image::kRgbaSigned8)));
  EXPECT_EQ(0, strcmp("Rgba8i", Image::GetFormatString(Image::kRgba8i)));
  EXPECT_EQ(0, strcmp("Rgba8ui", Image::GetFormatString(Image::kRgba8ui)));
  EXPECT_EQ(0, strcmp("Rgb10a2", Image::GetFormatString(Image::kRgb10a2)));
  EXPECT_EQ(0, strcmp("Rgb10a2ui", Image::GetFormatString(Image::kRgb10a2ui)));
  EXPECT_EQ(
      0, strcmp("Rgba16fFloat", Image::GetFormatString(Image::kRgba16fFloat)));
  EXPECT_EQ(0,
            strcmp("Rgba16fHalf", Image::GetFormatString(Image::kRgba16fHalf)));
  EXPECT_EQ(0, strcmp("Rgba16i", Image::GetFormatString(Image::kRgba16i)));
  EXPECT_EQ(0, strcmp("Rgba16ui", Image::GetFormatString(Image::kRgba16ui)));
  EXPECT_EQ(0, strcmp("Rgba32f", Image::GetFormatString(Image::kRgba32f)));
  EXPECT_EQ(0, strcmp("Rgba32i", Image::GetFormatString(Image::kRgba32i)));
  EXPECT_EQ(0, strcmp("Rgba32ui", Image::GetFormatString(Image::kRgba32ui)));
  EXPECT_EQ(0, strcmp("RenderbufferDepth16",
                      Image::GetFormatString(Image::kRenderbufferDepth16)));
  EXPECT_EQ(0, strcmp("RenderbufferDepth24",
                      Image::GetFormatString(Image::kRenderbufferDepth24)));
  EXPECT_EQ(0, strcmp("RenderbufferDepth32f",
                      Image::GetFormatString(Image::kRenderbufferDepth32f)));
  EXPECT_EQ(
      0, strcmp("RenderbufferDepth24Stencil8",
                Image::GetFormatString(Image::kRenderbufferDepth24Stencil8)));
  EXPECT_EQ(
      0, strcmp("RenderbufferDepth32fStencil8",
                Image::GetFormatString(Image::kRenderbufferDepth32fStencil8)));
  EXPECT_EQ(0, strcmp("TextureDepth16Int",
                      Image::GetFormatString(Image::kTextureDepth16Int)));
  EXPECT_EQ(0, strcmp("TextureDepth16Short",
                      Image::GetFormatString(Image::kTextureDepth16Short)));
  EXPECT_EQ(0, strcmp("TextureDepth24",
                      Image::GetFormatString(Image::kTextureDepth24)));
  EXPECT_EQ(0, strcmp("TextureDepth24Stencil8",
                      Image::GetFormatString(Image::kTextureDepth24Stencil8)));
  EXPECT_EQ(0, strcmp("TextureDepth32f",
                      Image::GetFormatString(Image::kTextureDepth32f)));
  EXPECT_EQ(
      0, strcmp("TextureDepth32fStencil8",
                Image::GetFormatString(Image::kTextureDepth32fStencil8)));
  struct AstcCharacteristics {
    std::string format_name;
    Image::Format format_enum;
  };
  const AstcCharacteristics kAstcCharacteristicsTable[] = {
    {"Astc4x4Rgba",   Image::kAstc4x4Rgba},
    {"Astc5x4Rgba",   Image::kAstc5x4Rgba},
    {"Astc5x5Rgba",   Image::kAstc5x5Rgba},
    {"Astc6x5Rgba",   Image::kAstc6x5Rgba},
    {"Astc6x6Rgba",   Image::kAstc6x6Rgba},
    {"Astc8x5Rgba",   Image::kAstc8x5Rgba},
    {"Astc8x6Rgba",   Image::kAstc8x6Rgba},
    {"Astc8x8Rgba",   Image::kAstc8x8Rgba},
    {"Astc10x5Rgba",  Image::kAstc10x5Rgba},
    {"Astc10x6Rgba",  Image::kAstc10x6Rgba},
    {"Astc10x8Rgba",  Image::kAstc10x8Rgba},
    {"Astc10x10Rgba", Image::kAstc10x10Rgba},
    {"Astc12x10Rgba", Image::kAstc12x10Rgba},
    {"Astc12x12Rgba", Image::kAstc12x12Rgba},
    {"Astc4x4Srgba",   Image::kAstc4x4Srgba},
    {"Astc5x4Srgba",   Image::kAstc5x4Srgba},
    {"Astc5x5Srgba",   Image::kAstc5x5Srgba},
    {"Astc6x5Srgba",   Image::kAstc6x5Srgba},
    {"Astc6x6Srgba",   Image::kAstc6x6Srgba},
    {"Astc8x5Srgba",   Image::kAstc8x5Srgba},
    {"Astc8x6Srgba",   Image::kAstc8x6Srgba},
    {"Astc8x8Srgba",   Image::kAstc8x8Srgba},
    {"Astc10x5Srgba",  Image::kAstc10x5Srgba},
    {"Astc10x6Srgba",  Image::kAstc10x6Srgba},
    {"Astc10x8Srgba",  Image::kAstc10x8Srgba},
    {"Astc10x10Srgba", Image::kAstc10x10Srgba},
    {"Astc12x10Srgba", Image::kAstc12x10Srgba},
    {"Astc12x12Srgba", Image::kAstc12x12Srgba},
  };
  for (const auto& astc_characteristics : kAstcCharacteristicsTable) {
    EXPECT_EQ(astc_characteristics.format_name,
              Image::GetFormatString(astc_characteristics.format_enum));
  }
  EXPECT_EQ(0, strcmp("Dxt1", Image::GetFormatString(Image::kDxt1)));
  EXPECT_EQ(0, strcmp("Dxt5", Image::GetFormatString(Image::kDxt5)));
  EXPECT_EQ(0, strcmp("Etc1", Image::GetFormatString(Image::kEtc1)));
  EXPECT_EQ(0, strcmp("Etc2Rgb", Image::GetFormatString(Image::kEtc2Rgb)));
  EXPECT_EQ(0, strcmp("Etc2Rgba", Image::GetFormatString(Image::kEtc2Rgba)));
  EXPECT_EQ(0, strcmp("Etc2Rgba1", Image::GetFormatString(Image::kEtc2Rgba1)));
  EXPECT_EQ(0,
            strcmp("Pvrtc1Rgb2", Image::GetFormatString(Image::kPvrtc1Rgb2)));
  EXPECT_EQ(0,
            strcmp("Pvrtc1Rgb4", Image::GetFormatString(Image::kPvrtc1Rgb4)));
  EXPECT_EQ(0,
            strcmp("Pvrtc1Rgba2", Image::GetFormatString(Image::kPvrtc1Rgba2)));
  EXPECT_EQ(0,
            strcmp("Pvrtc1Rgba4", Image::GetFormatString(Image::kPvrtc1Rgba4)));
  EXPECT_EQ(0, strcmp("Srgb8", Image::GetFormatString(Image::kSrgb8)));
  EXPECT_EQ(0, strcmp("Srgba8", Image::GetFormatString(Image::kSrgba8)));
  EXPECT_EQ(0,
            strcmp("Rgb11f_11f_10f_Rev",
                   Image::GetFormatString(Image::kRgb11f_11f_10f_Rev)));
  EXPECT_EQ(0,
            strcmp("Rgb11f_11f_10f_RevFloat",
                   Image::GetFormatString(Image::kRgb11f_11f_10f_RevFloat)));
  EXPECT_EQ(0,
            strcmp("Rgb11f_11f_10f_RevHalf",
                   Image::GetFormatString(Image::kRgb11f_11f_10f_RevHalf)));
  EXPECT_EQ(0,
            strcmp("Rgb565Byte", Image::GetFormatString(Image::kRgb565Byte)));
  EXPECT_EQ(0,
            strcmp("Rgb565Short", Image::GetFormatString(Image::kRgb565Short)));
  EXPECT_EQ(0,
            strcmp("Rgb5a1Byte", Image::GetFormatString(Image::kRgb5a1Byte)));
  EXPECT_EQ(0,
            strcmp("Rgb5a1Short", Image::GetFormatString(Image::kRgb5a1Short)));
  EXPECT_EQ(0, strcmp("Rgb5a1Int", Image::GetFormatString(Image::kRgb5a1Int)));
  EXPECT_EQ(0,
            strcmp("Rgb9e5Float", Image::GetFormatString(Image::kRgb9e5Float)));
  EXPECT_EQ(0,
            strcmp("Rgb9e5Half", Image::GetFormatString(Image::kRgb9e5Half)));
  EXPECT_EQ(
      0, strcmp("Rgb9e5RevInt", Image::GetFormatString(Image::kRgb9e5RevInt)));
  EXPECT_EQ(0, strcmp("Rgba4Byte", Image::GetFormatString(Image::kRgba4Byte)));
  EXPECT_EQ(0,
            strcmp("Rgba4Short", Image::GetFormatString(Image::kRgba4Short)));
  EXPECT_EQ(0,
            strcmp("EGLImage", Image::GetFormatString(Image::kEglImage)));
  EXPECT_EQ(0, strcmp("<UNKNOWN>", Image::GetFormatString(InvalidFormat())));
}

TEST(ImageTest, GetNumComponentsForFormat) {
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kAlpha));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kLuminance));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kLuminanceAlpha));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb888));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba8888));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb565));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba4444));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba5551));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR8));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kRSigned8));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR8i));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR8ui));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR16fFloat));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR16fHalf));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR16i));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR16ui));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR32f));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR32i));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kR32ui));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg8));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRgSigned8));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg8i));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg8ui));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg16fFloat));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg16fHalf));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg16i));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg16ui));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg32f));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg32i));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(Image::kRg32ui));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb8));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgbSigned8));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb8i));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb8ui));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb16fFloat));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb16fHalf));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb16i));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb16ui));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb32f));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb32i));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb32ui));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba8));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgbaSigned8));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba8i));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba8ui));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgb10a2));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgb10a2ui));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba16fFloat));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba16fHalf));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba16i));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba16ui));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba32f));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba32i));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba32ui));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kRenderbufferDepth16));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kRenderbufferDepth24));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kRenderbufferDepth32f));
  EXPECT_EQ(
      2, Image::GetNumComponentsForFormat(Image::kRenderbufferDepth24Stencil8));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(
                   Image::kRenderbufferDepth32fStencil8));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kTextureDepth16Int));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kTextureDepth16Short));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kTextureDepth24));
  EXPECT_EQ(2,
            Image::GetNumComponentsForFormat(Image::kTextureDepth24Stencil8));
  EXPECT_EQ(1, Image::GetNumComponentsForFormat(Image::kTextureDepth32f));
  EXPECT_EQ(2, Image::GetNumComponentsForFormat(
                   Image::kTextureDepth32fStencil8));
  for (const auto& astc_format : kAstcFormatTable) {
    EXPECT_EQ(4, Image::GetNumComponentsForFormat(astc_format));
  }
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kDxt1));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kDxt5));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kEtc1));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kEtc2Rgb));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kEtc2Rgba));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kEtc2Rgba1));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kPvrtc1Rgb2));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kPvrtc1Rgb4));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kPvrtc1Rgba2));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kPvrtc1Rgba4));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kSrgb8));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kSrgba8));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb11f_11f_10f_Rev));
  EXPECT_EQ(3,
            Image::GetNumComponentsForFormat(Image::kRgb11f_11f_10f_RevFloat));
  EXPECT_EQ(3,
            Image::GetNumComponentsForFormat(Image::kRgb11f_11f_10f_RevHalf));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb565Byte));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb565Short));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgb5a1Byte));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgb5a1Short));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgb5a1Int));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb9e5Float));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb9e5Half));
  EXPECT_EQ(3, Image::GetNumComponentsForFormat(Image::kRgb9e5RevInt));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba4Byte));
  EXPECT_EQ(4, Image::GetNumComponentsForFormat(Image::kRgba4Short));
  EXPECT_EQ(0, Image::GetNumComponentsForFormat(Image::kEglImage));
  EXPECT_EQ(0, Image::GetNumComponentsForFormat(Image::kInvalid));
#if !ION_PRODUCTION
  EXPECT_DEATH_IF_SUPPORTED(Image::GetNumComponentsForFormat(InvalidFormat()),
                            "Unknown format");
#endif
}

TEST(ImageTest, IsCompressedFormat) {
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kAlpha));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kLuminance));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kLuminanceAlpha));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb888));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba8888));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb565));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba4444));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba5551));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRSigned8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR8i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR8ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR16fFloat));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR16fHalf));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR16i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR16ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR32f));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR32i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kR32ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgSigned8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg8i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg8ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg16fFloat));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg16fHalf));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg16i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg16ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg32f));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg32i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRg32ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgbSigned8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb8i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb8ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb16fFloat));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb16fHalf));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb16i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb16ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb32f));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb32i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb32ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgbaSigned8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba8i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba8ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb10a2));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb10a2ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba16fFloat));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba16fHalf));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba16i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba16ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba32f));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba32i));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba32ui));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRenderbufferDepth16));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRenderbufferDepth24));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRenderbufferDepth32f));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRenderbufferDepth24Stencil8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRenderbufferDepth32fStencil8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kTextureDepth16Int));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kTextureDepth16Short));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kTextureDepth24));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kTextureDepth24Stencil8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kTextureDepth32f));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kTextureDepth32fStencil8));
  for (const auto& astc_format : kAstcFormatTable) {
    EXPECT_TRUE(Image::IsCompressedFormat(astc_format));
  }
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kDxt1));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kDxt5));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kEtc1));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kEtc2Rgb));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kEtc2Rgba));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kEtc2Rgba1));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kPvrtc1Rgb2));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kPvrtc1Rgb4));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kPvrtc1Rgba2));
  EXPECT_TRUE(Image::IsCompressedFormat(Image::kPvrtc1Rgba4));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kSrgb8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kSrgba8));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb11f_11f_10f_Rev));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb11f_11f_10f_RevFloat));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb11f_11f_10f_RevHalf));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb565Byte));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb565Short));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb5a1Byte));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb5a1Short));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb5a1Int));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb9e5Float));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb9e5Half));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgb9e5RevInt));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba4Byte));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kRgba4Short));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kEglImage));
  EXPECT_FALSE(Image::IsCompressedFormat(Image::kInvalid));
  EXPECT_FALSE(Image::IsCompressedFormat(InvalidFormat()));
}

TEST(ImageTest, Is8bpcFormat) {
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kAlpha));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kLuminance));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kLuminanceAlpha));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgb888));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgba8888));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb565));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba4444));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba5551));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgbaFloat));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kR8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRSigned8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kR8i));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kR8ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kR16fFloat));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kR16fHalf));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kR16i));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kR16ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kR32f));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kR32i));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kR32ui));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRg8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgSigned8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRg8i));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRg8ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRg16fFloat));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRg16fHalf));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRg16i));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRg16ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRg32f));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRg32i));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRg32ui));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgb8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgbSigned8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgb8i));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgb8ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb16fFloat));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb16fHalf));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb16i));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb16ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb32f));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb32i));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb32ui));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgba8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgbaSigned8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgba8i));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kRgba8ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb10a2));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb10a2ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba16fFloat));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba16fHalf));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba16i));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba16ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba32f));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba32i));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba32ui));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRenderbufferDepth16));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRenderbufferDepth24));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRenderbufferDepth32f));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(
      Image::kRenderbufferDepth24Stencil8));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(
      Image::kRenderbufferDepth32fStencil8));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kTextureDepth16Int));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kTextureDepth16Short));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kTextureDepth24));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kTextureDepth24Stencil8));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kTextureDepth32f));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kTextureDepth32fStencil8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kStencil8));
  for (const auto& astc_format : kAstcFormatTable) {
    EXPECT_FALSE(Image::Is8BitPerChannelFormat(astc_format));
  }
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kDxt1));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kDxt5));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kEtc1));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kEtc2Rgb));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kEtc2Rgba));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kEtc2Rgba1));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kPvrtc1Rgb2));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kPvrtc1Rgb4));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kPvrtc1Rgba2));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kPvrtc1Rgba4));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kSrgb8));
  EXPECT_TRUE(Image::Is8BitPerChannelFormat(Image::kSrgba8));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb11f_11f_10f_Rev));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb11f_11f_10f_RevFloat));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb11f_11f_10f_RevHalf));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb565Byte));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb565Short));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb5a1Byte));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb5a1Short));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb5a1Int));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb9e5Float));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb9e5Half));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgb9e5RevInt));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba4Byte));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kRgba4Short));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kEglImage));
  EXPECT_FALSE(Image::Is8BitPerChannelFormat(Image::kInvalid));
}

TEST(ImageTest, ComputeDataSize) {
  EXPECT_TRUE(VerifyDataSize(1, Image::kAlpha));
  EXPECT_TRUE(VerifyDataSize(1, Image::kLuminance));
  EXPECT_TRUE(VerifyDataSize(2, Image::kLuminanceAlpha));
  EXPECT_TRUE(VerifyDataSize(3, Image::kRgb888));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgba8888));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgb565));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgba4444));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgba5551));
  EXPECT_TRUE(VerifyDataSize(1, Image::kR8));
  EXPECT_TRUE(VerifyDataSize(1, Image::kRSigned8));
  EXPECT_TRUE(VerifyDataSize(1, Image::kR8i));
  EXPECT_TRUE(VerifyDataSize(1, Image::kR8ui));
  EXPECT_TRUE(VerifyDataSize(2, Image::kR16fFloat));
  EXPECT_TRUE(VerifyDataSize(2, Image::kR16fHalf));
  EXPECT_TRUE(VerifyDataSize(2, Image::kR16i));
  EXPECT_TRUE(VerifyDataSize(2, Image::kR16ui));
  EXPECT_TRUE(VerifyDataSize(4, Image::kR32f));
  EXPECT_TRUE(VerifyDataSize(4, Image::kR32i));
  EXPECT_TRUE(VerifyDataSize(4, Image::kR32ui));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRg8));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgSigned8));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRg8i));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRg8ui));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRg16fFloat));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRg16fHalf));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRg16i));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRg16ui));
  EXPECT_TRUE(VerifyDataSize(8, Image::kRg32f));
  EXPECT_TRUE(VerifyDataSize(8, Image::kRg32i));
  EXPECT_TRUE(VerifyDataSize(8, Image::kRg32ui));
  EXPECT_TRUE(VerifyDataSize(3, Image::kRgb8));
  EXPECT_TRUE(VerifyDataSize(3, Image::kRgbSigned8));
  EXPECT_TRUE(VerifyDataSize(3, Image::kRgb8i));
  EXPECT_TRUE(VerifyDataSize(3, Image::kRgb8ui));
  EXPECT_TRUE(VerifyDataSize(6, Image::kRgb16fFloat));
  EXPECT_TRUE(VerifyDataSize(6, Image::kRgb16fHalf));
  EXPECT_TRUE(VerifyDataSize(6, Image::kRgb16i));
  EXPECT_TRUE(VerifyDataSize(6, Image::kRgb16ui));
  EXPECT_TRUE(VerifyDataSize(12, Image::kRgb32f));
  EXPECT_TRUE(VerifyDataSize(12, Image::kRgb32i));
  EXPECT_TRUE(VerifyDataSize(12, Image::kRgb32ui));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgba8));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgbaSigned8));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgba8i));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgba8ui));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgb10a2));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgb10a2ui));
  EXPECT_TRUE(VerifyDataSize(8, Image::kRgba16fFloat));
  EXPECT_TRUE(VerifyDataSize(8, Image::kRgba16fHalf));
  EXPECT_TRUE(VerifyDataSize(8, Image::kRgba16i));
  EXPECT_TRUE(VerifyDataSize(8, Image::kRgba16ui));
  EXPECT_TRUE(VerifyDataSize(16, Image::kRgba32f));
  EXPECT_TRUE(VerifyDataSize(16, Image::kRgba32i));
  EXPECT_TRUE(VerifyDataSize(16, Image::kRgba32ui));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRenderbufferDepth16));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRenderbufferDepth24));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRenderbufferDepth32f));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRenderbufferDepth24Stencil8));
  EXPECT_TRUE(VerifyDataSize(8, Image::kRenderbufferDepth32fStencil8));
  EXPECT_TRUE(VerifyDataSize(2, Image::kTextureDepth16Int));
  EXPECT_TRUE(VerifyDataSize(2, Image::kTextureDepth16Short));
  EXPECT_TRUE(VerifyDataSize(4, Image::kTextureDepth24));
  EXPECT_TRUE(VerifyDataSize(4, Image::kTextureDepth32f));
  EXPECT_TRUE(VerifyDataSize(4, Image::kTextureDepth24Stencil8));
  EXPECT_TRUE(VerifyDataSize(8, Image::kTextureDepth32fStencil8));
  EXPECT_TRUE(VerifyDataSize(3, Image::kSrgb8));
  EXPECT_TRUE(VerifyDataSize(4, Image::kSrgba8));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgb11f_11f_10f_Rev));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgb11f_11f_10f_RevFloat));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgb11f_11f_10f_RevHalf));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgb565Byte));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgb565Short));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgb5a1Byte));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgb5a1Short));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgb5a1Int));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgb9e5Float));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgb9e5Half));
  EXPECT_TRUE(VerifyDataSize(4, Image::kRgb9e5RevInt));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgba4Byte));
  EXPECT_TRUE(VerifyDataSize(2, Image::kRgba4Short));
  EXPECT_TRUE(VerifyDataSize(0, Image::kEglImage));
  EXPECT_TRUE(VerifyDataSize(0, Image::kInvalid));

  // Compressed formats have fractional bytes per pixel.
  for (const auto& astc_format : kAstcFormatTable) {
    EXPECT_EQ(0U, Image::ComputeDataSize(astc_format, 0U, 0U));
    EXPECT_EQ(0U, Image::ComputeDataSize(astc_format, 0U, 16U));
    EXPECT_EQ(0U, Image::ComputeDataSize(astc_format, 20U, 0U));
    // All ASTC block sizes have 16 bytes. 4x4 is smallest, but will round up
    // to each format's size.
    EXPECT_EQ(16U, Image::ComputeDataSize(astc_format, 4U, 4U));
  }

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kDxt1, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kDxt1, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kDxt1, 20U, 0U));
  EXPECT_EQ(160U, Image::ComputeDataSize(Image::kDxt1, 20U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kDxt5, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kDxt5, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kDxt5, 20U, 0U));
  EXPECT_EQ(320U, Image::ComputeDataSize(Image::kDxt5, 20U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc1, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc1, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc1, 20U, 0U));
  EXPECT_EQ(160U, Image::ComputeDataSize(Image::kEtc1, 20U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgb, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgb, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgb, 20U, 0U));
  EXPECT_EQ(160U, Image::ComputeDataSize(Image::kEtc2Rgb, 20U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgba, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgba, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgba, 20U, 0U));
  EXPECT_EQ(320U, Image::ComputeDataSize(Image::kEtc2Rgba, 20U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgba1, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgba1, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kEtc2Rgba1, 20U, 0U));
  EXPECT_EQ(160U, Image::ComputeDataSize(Image::kEtc2Rgba1, 20U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgb2, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgb2, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgb2, 20U, 0U));
  EXPECT_EQ(64U, Image::ComputeDataSize(Image::kPvrtc1Rgb2, 16U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgb4, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgb4, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgb4, 20U, 0U));
  EXPECT_EQ(128U, Image::ComputeDataSize(Image::kPvrtc1Rgb4, 16U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgba2, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgba2, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgba2, 20U, 0U));
  EXPECT_EQ(64U, Image::ComputeDataSize(Image::kPvrtc1Rgba2, 16U, 16U));

  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgba4, 0U, 0U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgba4, 0U, 16U));
  EXPECT_EQ(0U, Image::ComputeDataSize(Image::kPvrtc1Rgba4, 20U, 0U));
  EXPECT_EQ(128U, Image::ComputeDataSize(Image::kPvrtc1Rgba4, 16U, 16U));

#if !ION_PRODUCTION
  EXPECT_DEATH_IF_SUPPORTED(Image::ComputeDataSize(InvalidFormat(), 20U, 16U),
                            "Unknown format");
#endif
}

}  // namespace gfx
}  // namespace ion
