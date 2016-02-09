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

#include "ion/image/conversionutils.h"

#include <string.h>  // For memcmp().

#include <algorithm>
#include <iomanip>
#include <vector>

#include "base/macros.h"  // For ARRAYSIZE().
#include "ion/base/datacontainer.h"
#include "ion/base/logchecker.h"
#include "ion/image/tests/image_bytes.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace image {

using gfx::Image;
using gfx::ImagePtr;

namespace {

//-----------------------------------------------------------------------------
//
// Helper variables and functions.
//
//-----------------------------------------------------------------------------

// These constants are used to iterate over supported formats. They need to be
// kept in sync with the headers if those change.
static const Image::Format kMinImageFormat = Image::kAlpha;
static const Image::Format kMaxImageFormat = Image::kRgba4Short;

// Returns true if a given conversion should be supported.
static bool ConversionIsSupported(Image::Format from, Image::Format to) {
  static const int kNumImageFormats = kMaxImageFormat - kMinImageFormat + 1;
  static bool support_matrix[kNumImageFormats][kNumImageFormats];
  static bool is_matrix_initialized = false;

  if (!is_matrix_initialized) {
    // This assumes that the enum starts at 0 and there are no gaps.
    DCHECK_EQ(0, kMinImageFormat);
    DCHECK_EQ(kNumImageFormats, kMaxImageFormat + 1);

    // Start with all false values except for identity relationships.
    for (int from = 0; from < kNumImageFormats; ++from) {
      for (int to = 0; to < kNumImageFormats; ++to) {
        support_matrix[from][to] = (from == to);
      }
    }

    // Insert known supported conversions.
    support_matrix[Image::kDxt1][Image::kEtc1] = true;
    support_matrix[Image::kDxt1][Image::kRgb888] = true;
    support_matrix[Image::kDxt1][Image::kR8] = true;
    support_matrix[Image::kDxt5][Image::kPvrtc1Rgba2] = true;
    support_matrix[Image::kDxt5][Image::kRgba8888] = true;
    support_matrix[Image::kDxt5][Image::kR8] = true;
    support_matrix[Image::kEtc1][Image::kDxt1] = true;
    support_matrix[Image::kEtc1][Image::kRgb888] = true;
    support_matrix[Image::kEtc1][Image::kR8] = true;
    support_matrix[Image::kRgb888][Image::kDxt1] = true;
    support_matrix[Image::kRgb888][Image::kEtc1] = true;
    support_matrix[Image::kRgb888][Image::kR8] = true;
    support_matrix[Image::kRgba8888][Image::kDxt5] = true;
    support_matrix[Image::kRgba8888][Image::kPvrtc1Rgba2] = true;
    support_matrix[Image::kRgba8888][Image::kR8] = true;
    support_matrix[Image::kLuminance][Image::kRgb888] = true;
    support_matrix[Image::kLuminance][Image::kRgba8888] = true;
    support_matrix[Image::kLuminanceAlpha][Image::kRgb888] = true;
    support_matrix[Image::kLuminanceAlpha][Image::kRgba8888] = true;
  }

  return support_matrix[from][to];
}

// Creates an image with a specified format and size. The data in the image
// consists of the correct number of bytes, starting with 0x00 and
// incrementing/wrapping.
static const ImagePtr CreateImage(Image::Format format,
                                  uint32 width, uint32 height) {
  const size_t data_size = Image::ComputeDataSize(format, width, height);
  std::vector<uint8> data(data_size);
  for (size_t i = 0; i < data_size; ++i)
    data[i] = static_cast<uint8>(i & 0xff);

  ImagePtr image(new Image);
  image->Set(format, width, height,
             base::DataContainer::CreateAndCopy<uint8>(&data[0], data.size(),
                                                       false,
                                                       image->GetAllocator()));
  return image;
}

// Creates an image with a specified format and size. The data in the image
// consists of the correct number of bytes, starting with pattern[0] cycling
// through the values in pattern, wrapping as needed.
static const ImagePtr CreateImageWithPattern(
    Image::Format format, uint32 width, uint32 height,
    const vector<uint8>& pattern) {
  const size_t data_size = Image::ComputeDataSize(format, width, height);
  std::vector<uint8> data(data_size);
  size_t pattern_size = pattern.size();
  for (size_t i = 0; i < data_size; ++i)
    data[i] = static_cast<uint8>(pattern[i % pattern_size]);

  ImagePtr image(new Image);
  image->Set(format, width, height,
             base::DataContainer::CreateAndCopy<uint8>(&data[0], data.size(),
                                                       false,
                                                       image->GetAllocator()));
  return image;
}

// Creates and returns a vector of bytes representing an 8x8 JPEG image.
static const std::vector<uint8> Create8x8JpegData() {
  using testing::kJpeg8x8ImageBytes;
  static const size_t kArraySize = ARRAYSIZE(kJpeg8x8ImageBytes);
  return std::vector<uint8>(&kJpeg8x8ImageBytes[0],
                            &kJpeg8x8ImageBytes[kArraySize]);
}

// Creates and returns a vector of bytes representing an 8x8 JPEG image.
static const std::vector<uint8> Create8x8GrayJpegData() {
  using testing::kJpeg8x8GrayImageBytes;
  static const size_t kArraySize = ARRAYSIZE(kJpeg8x8GrayImageBytes);
  return std::vector<uint8>(&kJpeg8x8GrayImageBytes[0],
                            &kJpeg8x8GrayImageBytes[kArraySize]);
}

// Creates and returns a vector of bytes representing an 8x8 PNG image with
// RGB data.
static const std::vector<uint8> Create8x8PngRgbData() {
  // The striped image causes problems with RGB PNG images because it has
  // different colors. The extra colors are needed for this to keep the PNG from
  // having 4-bit palette entries, which STBLIB cannot handle.
  using testing::kPngRgb8x8ImageBytes;
  static const size_t kArraySize = ARRAYSIZE(kPngRgb8x8ImageBytes);
  return std::vector<uint8>(&kPngRgb8x8ImageBytes[0],
                            &kPngRgb8x8ImageBytes[kArraySize]);
}

// Creates and returns a vector of bytes representing an 8x8 PNG image with
// RGBA data.
static const std::vector<uint8> Create8x8PngRgbaData() {
  using testing::kPngRgba8x8ImageBytes;
  static const size_t kArraySize = ARRAYSIZE(kPngRgba8x8ImageBytes);
  return std::vector<uint8>(&kPngRgba8x8ImageBytes[0],
                            &kPngRgba8x8ImageBytes[kArraySize]);
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// RGBA8888 Big-Endian format but with a totally invalid header.
static const std::vector<uint8> CreateRgba8888IonRawInvalidHeaderData() {
  using testing::kInvalidIonRawHeaderBytes;
  using testing::kRgba8888IonRawBigEndian3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kInvalidIonRawHeaderBytes,
      kInvalidIonRawHeaderBytes + ARRAYSIZE(kInvalidIonRawHeaderBytes));
  result.insert(result.end(), kRgba8888IonRawBigEndian3x3ImageBytes,
      kRgba8888IonRawBigEndian3x3ImageBytes +
      ARRAYSIZE(kRgba8888IonRawBigEndian3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// RGBA8888 Big-Endian format.
static const std::vector<uint8> CreateRgba8888IonRaw3x3BigEndianData() {
  using testing::kRgba8888IonRaw3x3BigEndianHeaderBytes;
  using testing::kRgba8888IonRawBigEndian3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kRgba8888IonRaw3x3BigEndianHeaderBytes,
      kRgba8888IonRaw3x3BigEndianHeaderBytes +
      ARRAYSIZE(kRgba8888IonRaw3x3BigEndianHeaderBytes));
  result.insert(result.end(), kRgba8888IonRawBigEndian3x3ImageBytes,
      kRgba8888IonRawBigEndian3x3ImageBytes +
      ARRAYSIZE(kRgba8888IonRawBigEndian3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// RGBA8888 Little-Endian format.
static const std::vector<uint8> CreateRgba8888IonRaw3x3LittleEndianData() {
  using testing::kRgba8888IonRaw3x3LittleEndianHeaderBytes;
  using testing::kRgba8888IonRawLittleEndian3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kRgba8888IonRaw3x3LittleEndianHeaderBytes,
      kRgba8888IonRaw3x3LittleEndianHeaderBytes +
      ARRAYSIZE(kRgba8888IonRaw3x3LittleEndianHeaderBytes));
  result.insert(result.end(), kRgba8888IonRawLittleEndian3x3ImageBytes,
      kRgba8888IonRawLittleEndian3x3ImageBytes +
      ARRAYSIZE(kRgba8888IonRawLittleEndian3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// RGB565 Big-Endian format.
static const std::vector<uint8> CreateRgb565IonRaw3x3BigEndianData() {
  using testing::kRgb565IonRaw3x3BigEndianHeaderBytes;
  using testing::kRgb565IonRawBigEndian3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kRgb565IonRaw3x3BigEndianHeaderBytes,
      kRgb565IonRaw3x3BigEndianHeaderBytes +
      ARRAYSIZE(kRgb565IonRaw3x3BigEndianHeaderBytes));
  result.insert(result.end(), kRgb565IonRawBigEndian3x3ImageBytes,
      kRgb565IonRawBigEndian3x3ImageBytes +
      ARRAYSIZE(kRgb565IonRawBigEndian3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// RGB565 Little-Endian format.
static const std::vector<uint8> CreateRgb565IonRaw3x3LittleEndianData() {
  using testing::kRgb565IonRaw3x3LittleEndianHeaderBytes;
  using testing::kRgb565IonRawLittleEndian3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kRgb565IonRaw3x3LittleEndianHeaderBytes,
      kRgb565IonRaw3x3LittleEndianHeaderBytes +
      ARRAYSIZE(kRgb565IonRaw3x3LittleEndianHeaderBytes));
  result.insert(result.end(), kRgb565IonRawLittleEndian3x3ImageBytes,
      kRgb565IonRawLittleEndian3x3ImageBytes +
      ARRAYSIZE(kRgb565IonRawLittleEndian3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// RGBA4444 Big-Endian format.
static const std::vector<uint8> CreateRgba4444IonRaw3x3BigEndianData() {
  using testing::kRgba4444IonRaw3x3BigEndianHeaderBytes;
  using testing::kRgba4444IonRawBigEndian3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kRgba4444IonRaw3x3BigEndianHeaderBytes,
      kRgba4444IonRaw3x3BigEndianHeaderBytes +
      ARRAYSIZE(kRgba4444IonRaw3x3BigEndianHeaderBytes));
  result.insert(result.end(), kRgba4444IonRawBigEndian3x3ImageBytes,
      kRgba4444IonRawBigEndian3x3ImageBytes +
      ARRAYSIZE(kRgba4444IonRawBigEndian3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// RGBA4444 Little-Endian format.
static const std::vector<uint8> CreateRgba4444IonRaw3x3LittleEndianData() {
  using testing::kRgba4444IonRaw3x3LittleEndianHeaderBytes;
  using testing::kRgba4444IonRawLittleEndian3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kRgba4444IonRaw3x3LittleEndianHeaderBytes,
      kRgba4444IonRaw3x3LittleEndianHeaderBytes +
      ARRAYSIZE(kRgba4444IonRaw3x3LittleEndianHeaderBytes));
  result.insert(result.end(), kRgba4444IonRawLittleEndian3x3ImageBytes,
      kRgba4444IonRawLittleEndian3x3ImageBytes +
      ARRAYSIZE(kRgba4444IonRawLittleEndian3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// 8-bit alpha Big-Endian format.
static const std::vector<uint8> CreateAlphaIonRaw3x3BigEndianData() {
  using testing::kAlphaIonRaw3x3BigEndianHeaderBytes;
  using testing::kAlphaIonRaw3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kAlphaIonRaw3x3BigEndianHeaderBytes,
      kAlphaIonRaw3x3BigEndianHeaderBytes +
      ARRAYSIZE(kAlphaIonRaw3x3BigEndianHeaderBytes));
  result.insert(result.end(), kAlphaIonRaw3x3ImageBytes,
      kAlphaIonRaw3x3ImageBytes +
      ARRAYSIZE(kAlphaIonRaw3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 3x3 image in "ION raw"
// 8-bit alpha Little-Endian format.
static const std::vector<uint8> CreateAlphaIonRaw3x3LittleEndianData() {
  using testing::kAlphaIonRaw3x3LittleEndianHeaderBytes;
  using testing::kAlphaIonRaw3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kAlphaIonRaw3x3LittleEndianHeaderBytes,
      kAlphaIonRaw3x3LittleEndianHeaderBytes +
      ARRAYSIZE(kAlphaIonRaw3x3LittleEndianHeaderBytes));
  result.insert(result.end(), kAlphaIonRaw3x3ImageBytes,
      kAlphaIonRaw3x3ImageBytes +
      ARRAYSIZE(kAlphaIonRaw3x3ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a 2x2 image in "ION raw"
// RGBA8888 format but with dimension 3x3 indicated in the header.
static const std::vector<uint8> CreateRgba8888IonRawWrongSizeData() {
  using testing::kRgba8888IonRaw3x3BigEndianHeaderBytes;
  using testing::kRgba8888IonRaw2x2ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kRgba8888IonRaw3x3BigEndianHeaderBytes,
      kRgba8888IonRaw3x3BigEndianHeaderBytes +
      ARRAYSIZE(kRgba8888IonRaw3x3BigEndianHeaderBytes));
  result.insert(result.end(), kRgba8888IonRaw2x2ImageBytes,
      kRgba8888IonRaw2x2ImageBytes + ARRAYSIZE(kRgba8888IonRaw2x2ImageBytes));
  return result;
}

// Creates and returns a vector of bytes representing a payloadless
// image in "ION raw" RGBA8888 format.
static const std::vector<uint8> CreateRgba8888IonRawPayloadlessData() {
  using testing::kRgba8888IonRawPayloadlessHeaderBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kRgba8888IonRawPayloadlessHeaderBytes,
      kRgba8888IonRawPayloadlessHeaderBytes +
      ARRAYSIZE(kRgba8888IonRawPayloadlessHeaderBytes));
  return result;
}

// Creates and returns a vector of bytes representing an image in a
// not-supported-yet "ION raw" format.
static const std::vector<uint8> CreateUnknownIonRawData() {
  using testing::kUnknownIonRawHeaderBytes;
  using testing::kRgba8888IonRawBigEndian3x3ImageBytes;
  std::vector<uint8> result = std::vector<uint8>();
  result.insert(result.end(), kUnknownIonRawHeaderBytes,
      kUnknownIonRawHeaderBytes + ARRAYSIZE(kUnknownIonRawHeaderBytes));
  result.insert(result.end(), kRgba8888IonRawBigEndian3x3ImageBytes,
      kRgba8888IonRawBigEndian3x3ImageBytes +
      ARRAYSIZE(kRgba8888IonRawBigEndian3x3ImageBytes));
  return result;
}

// Compares the data in an Image with the data in a byte array for size match
// and equality, printing a useful message about how they differ on failure.
static ::testing::AssertionResult ImageMatchesBytes(
    const Image& image, const uint8* expected_bytes, size_t num_bytes) {
  if (image.GetDataSize() != num_bytes) {
    return ::testing::AssertionFailure()
        << "Image is size " << image.GetDataSize()
        << ", expected " << num_bytes;
  }

  const uint8* image_bytes =
      static_cast<const uint8*>(image.GetData()->GetData());
  for (size_t i = 0; i < num_bytes; ++i) {
    if (image_bytes[i] != expected_bytes[i]) {
      return ::testing::AssertionFailure()
          << "Images differs at byte " << i << ": got "
          << std::hex
          << static_cast<uint32>(image_bytes[i]) << ", expected "
          << static_cast<uint32>(expected_bytes[i]);
    }
  }
  return ::testing::AssertionSuccess();
}

// Compares 2 images to see if they are the same except for vertical row
// flipping.
static void CompareFlipped(const Image& image, const Image& flipped) {
  EXPECT_EQ(image.GetFormat(), flipped.GetFormat());
  EXPECT_EQ(image.GetWidth(), flipped.GetWidth());
  EXPECT_EQ(image.GetHeight(), flipped.GetHeight());
  EXPECT_EQ(image.GetDataSize(), flipped.GetDataSize());

  const uint8* image_data = image.GetData()->GetData<uint8>();
  const uint8* flipped_data = flipped.GetData()->GetData<uint8>();
  const size_t row_size = Image::ComputeDataSize(
      image.GetFormat(), image.GetWidth(), 1 /* height */);
  const size_t height = image.GetHeight();
  for (size_t row_id = 0; row_id < height; ++row_id) {
    const uint8* image_row = image_data + row_id * row_size;
    const uint8* flipped_row = flipped_data + (height - row_id - 1) * row_size;
    EXPECT_EQ(0, memcmp(image_row, flipped_row, row_size));
  }
}

static void TestIonRaw(const uint8* ion_raw_data, size_t ion_raw_data_size,
    Image::Format expected_format, uint32 expected_width,
    uint32 expected_height, uint32 expected_size,
    const uint8* expected_image_bytes, size_t expected_image_bytes_size) {
  // Test canonical format (RGBA8888), width, height (pixels) , size (bytes).
  ImagePtr image = ConvertFromExternalImageData(ion_raw_data,
      ion_raw_data_size, false, false, base::AllocatorPtr());
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(expected_format, image->GetFormat());
  EXPECT_EQ(expected_width, image->GetWidth());
  EXPECT_EQ(expected_height, image->GetHeight());
  EXPECT_EQ(expected_size, image->GetDataSize());
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Test image bytes (payload).
  EXPECT_TRUE(ImageMatchesBytes(*image,
      expected_image_bytes, expected_image_bytes_size));

  // Test vertical flipping.
  ImagePtr flipped = ConvertFromExternalImageData(ion_raw_data,
      ion_raw_data_size, true, false, base::AllocatorPtr());
  ASSERT_FALSE(flipped.Get() == NULL);
  CompareFlipped(*image, *flipped);

  // Test data wiping.
  ImagePtr wipeable = ConvertFromExternalImageData(ion_raw_data,
      ion_raw_data_size, false, true, base::AllocatorPtr());
  EXPECT_TRUE(wipeable->GetData()->IsWipeable());
}

static void TestNullIonRaw(const uint8* ion_raw_data,
                           size_t ion_raw_data_size) {
  ImagePtr image = ConvertFromExternalImageData(ion_raw_data,
      ion_raw_data_size, false, false, base::AllocatorPtr());
  ASSERT_TRUE(image.Get() == NULL);
}

// Copy N bytes per pixel from a source array of 4 byte pixels.
static void Copy4BppToN(const uint8 *src, size_t num_pixels, uint8 *dst,
    size_t num_channels) {
  for (uint i = 0; i < num_pixels; i++) {
    for (uint j = 0; j < 4; j++) {
      if (j < num_channels) *(dst++) = *src;
      src++;
    }
  }
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// The tests.
//
//-----------------------------------------------------------------------------

TEST(ConversionUtils, EmptyInput) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.

  for (Image::Format f = kMinImageFormat; f <= kMaxImageFormat;
       f = static_cast<Image::Format>(f + 1)) {
    SCOPED_TRACE(Image::GetFormatString(f));

    // Converting from a NULL image to any format should return a NULL pointer.
    EXPECT_TRUE(ConvertImage(ImagePtr(), f, false, al, al).Get() == NULL);
  }

  // Converting from NULL data to any format should return a NULL pointer.
  EXPECT_TRUE(ConvertFromExternalImageData(
      NULL, 64U, false, false, al).Get() == NULL);

  // Converting from 0-size data to any format should return a NULL pointer.
  int dummy;
  EXPECT_TRUE(ConvertFromExternalImageData(
      &dummy, 0, false, false, al).Get() == NULL);

  // Converting from a NULL image to external format should return empty vector.
  SCOPED_TRACE(kPng);
  EXPECT_TRUE(ConvertToExternalImageData(ImagePtr(), kPng, false).empty());

  // Downsampling from a NULL image should return a NULL pointer.
  EXPECT_TRUE(DownsampleImage2x(ImagePtr(), false, al).Get() == NULL);

  // Resizing from a NULL image should return a NULL pointer.
  EXPECT_TRUE(ResizeImage(ImagePtr(), 5, 5, false, al).Get() == NULL);
}

TEST(ConversionUtils, ImageToImage) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.

  for (Image::Format from = kMinImageFormat; from <= kMaxImageFormat;
       from = static_cast<Image::Format>(from + 1)) {
    SCOPED_TRACE(::testing::Message() << "From "
                                      << Image::GetFormatString(from));
    bool is_wipeable = true;
    for (Image::Format to = kMinImageFormat; to <= kMaxImageFormat;
         to = static_cast<Image::Format>(to + 1)) {
      SCOPED_TRACE(::testing::Message() << "To " << Image::GetFormatString(to));
      // Image must be at least 8 pixels wide for PVRTC.
      ImagePtr from_img = CreateImage(from, 8U, 8U);
      if (ConversionIsSupported(from, to)) {
        ImagePtr to_img = ConvertImage(from_img, to, is_wipeable, al, al);
        EXPECT_FALSE(to_img.Get() == NULL);
        EXPECT_EQ(to, to_img->GetFormat());
        EXPECT_EQ(is_wipeable, to_img->GetData()->IsWipeable());
      } else {
        EXPECT_TRUE(ConvertImage(from_img, to, false, al, al).Get() == NULL);
      }
      is_wipeable = !is_wipeable;
    }
  }
}

TEST(ConversionUtils, ExtractRedChannel) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  bool is_wipeable = true;
  uint8 temp[64];

  // Verify that the red channel is extracted.

  // Two RGBA pixels.
  const uint8 data[] = { 1, 2, 3, 4,   5, 6, 7, 8 };
  const size_t pattern_pixels = sizeof(data) / 4;
  Image::Format test_formats[] = { Image::kRgb888, Image::kRgba8888 };

  for (unsigned int i = 0U; i < ARRAYSIZE(test_formats); ++i) {
    const size_t num_channels =
        Image::GetNumComponentsForFormat(test_formats[i]);
    const size_t pattern_size = pattern_pixels * num_channels;
    // Extract the N test channels from the 4 channel source data.
    EXPECT_GE(sizeof(temp), pattern_pixels * num_channels);
    Copy4BppToN(data, pattern_pixels, temp, num_channels);
    vector<uint8> pattern(pattern_size);
    std::copy(temp, temp + pattern_size, pattern.begin());
    {
      ImagePtr image = CreateImageWithPattern(test_formats[i], 2, 2, pattern);
      ImagePtr extracted = ConvertImage(image, Image::kR8, is_wipeable, al, al);
      EXPECT_EQ(extracted->GetFormat(), Image::kR8);
      EXPECT_EQ(extracted->GetWidth(), 2U);
      EXPECT_EQ(extracted->GetHeight(), 2U);
      const uint8 expected[] = { 1, 5, 1, 5 };
      EXPECT_TRUE(ImageMatchesBytes(*extracted, expected, ARRAYSIZE(expected)));
    }
  }
}

TEST(ConversionUtils, LuminanceToRgb) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  bool is_wipeable = true;
  uint8 temp[64];

  // Two luminance-alpha pixels (zeros will be ignored).
  const uint8 data[] = { 1, 2, 0, 0,  3, 4, 0, 0 };
  const size_t pattern_pixels = sizeof(data) / 4;
  Image::Format src_formats[] = { Image::kLuminance, Image::kLuminanceAlpha };
  Image::Format dst_formats[] = { Image::kRgb888, Image::kRgba8888 };

  for (unsigned int j = 0U; j < ARRAYSIZE(src_formats); ++j) {
    for (unsigned int i = 0U; i < ARRAYSIZE(dst_formats); ++i) {
      // Extract the N src channels from the 2 channel LumAlpha data.
      const size_t src_channels =
          Image::GetNumComponentsForFormat(src_formats[j]);
      const size_t src_size = pattern_pixels * src_channels;
      EXPECT_GE(sizeof(temp), src_size);
      Copy4BppToN(data, pattern_pixels, temp, src_channels);
      vector<uint8> src_pattern(src_size);
      std::copy(temp, temp + src_size, src_pattern.begin());

      ImagePtr image =
          CreateImageWithPattern(src_formats[j], 2, 1, src_pattern);
      ImagePtr extracted =
          ConvertImage(image, dst_formats[i], is_wipeable, al, al);
      EXPECT_TRUE(extracted.Get() != NULL);

      EXPECT_EQ(extracted->GetFormat(), dst_formats[i]);
      EXPECT_EQ(extracted->GetWidth(), 2U);
      EXPECT_EQ(extracted->GetHeight(), 1U);

      const uint8* expected;
      const uint8 data_l_to_rgb[] = { 1, 1, 1, 3, 3, 3 };
      const uint8 data_l_to_rgba[] = { 1, 1, 1, 255, 3, 3, 3, 255 };
      const uint8 data_la_to_rgb[] = { 1, 1, 1, 3, 3, 3 };
      const uint8 data_la_to_rgba[] = { 1, 1, 1, 2, 3, 3, 3, 4 };
      if (src_formats[j] == Image::kLuminance) {
        if (dst_formats[i] == Image::kRgb888) {
          expected = data_l_to_rgb;
        } else {
          expected = data_l_to_rgba;
        }
      } else {
        if (dst_formats[i] == Image::kRgb888) {
          expected = data_la_to_rgb;
        } else {
          expected = data_la_to_rgba;
        }
      }

      // Extract the N dst channels from the 4 channel RGBA data.
      const size_t dst_size = pattern_pixels *
          Image::GetNumComponentsForFormat(dst_formats[i]);
      EXPECT_TRUE(ImageMatchesBytes(*extracted, expected, dst_size));
    }
  }
}

TEST(ConversionUtils, CompressAndDecompressRGB) {
  base::LogChecker log_checker;
  base::AllocatorPtr al;  // NULL pointer means use default allocator.

  // Create a sample RGB image.
  ImagePtr image = CreateImage(Image::kRgb888, 4, 4);
  static const size_t kDataSizeRGB4x4 = 4 * 4 * 3;

  // Compress using DXTC.
  image = ConvertImage(image, Image::kDxt1, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kDxt1, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(8U, image->GetDataSize());  // 1 DXT1 block = 32 bits.
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Recompress; should have no effect.
  ImagePtr saved_image = image;
  image = ConvertImage(image, Image::kDxt1, false, al, al);
  EXPECT_EQ(saved_image, image);
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Decompress back to RGB.
  image = ConvertImage(image, Image::kRgb888, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(kDataSizeRGB4x4, image->GetDataSize());
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Decompress again; should have no effect.
  saved_image = image;
  image = ConvertImage(image, Image::kRgb888, false, al, al);
  EXPECT_EQ(saved_image, image);
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Compress using ETC.
  image = ConvertImage(image, Image::kEtc1, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kEtc1, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(8U, image->GetDataSize());  // 1 ETC1 block = 32 bits.
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Recompress; should have no effect.
  saved_image = image;
  image = ConvertImage(image, Image::kEtc1, false, al, al);
  EXPECT_EQ(saved_image, image);
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Decompress back to RGB.
  image = ConvertImage(image, Image::kRgb888, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(kDataSizeRGB4x4, image->GetDataSize());
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Decompress again; should have no effect.
  image = ConvertImage(image, Image::kRgb888, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(kDataSizeRGB4x4, image->GetDataSize());
  EXPECT_FALSE(image->IsCompressed());
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Compress to DXT1 and then to ETC1. Should work fine.
  image = ConvertImage(image, Image::kDxt1, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kDxt1, image->GetFormat());
  EXPECT_FALSE(image->GetData()->IsWipeable());
  image = ConvertImage(image, Image::kEtc1, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kEtc1, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(8U, image->GetDataSize());  // 1 ETC1 block = 32 bits.
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Decompress should still work.
  image = ConvertImage(image, Image::kRgb888, false, al, al);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_FALSE(image->GetData()->IsWipeable());
}

TEST(ConversionUtils, CompressAndDecompressRGBA) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.

  // Create a sample RGBA image.
  ImagePtr image = CreateImage(Image::kRgba8888, 4, 4);
  static const size_t kDataSizeRGBA4x4 = 4 * 4 * 4;

  // Compress using DXTC.
  image = ConvertImage(image, Image::kDxt5, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kDxt5, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(16U, image->GetDataSize());  // 1 DXT5 block = 64 bits.
  EXPECT_FALSE(image->GetData()->IsWipeable());

  // Decompress back to RGBA.
  image = ConvertImage(image, Image::kRgba8888, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(4U, image->GetWidth());
  EXPECT_EQ(4U, image->GetHeight());
  EXPECT_EQ(kDataSizeRGBA4x4, image->GetDataSize());
  EXPECT_FALSE(image->GetData()->IsWipeable());
}

// PVRTC can only be compressed, not decompressed.
TEST(ConversionUtils, CompressPvrtc1Rgba2) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.

  // Create a sample RGBA image.
  ImagePtr image = CreateImage(Image::kRgba8888, 8, 8);

  // Compress using PVRTC.
  image = ConvertImage(image, Image::kPvrtc1Rgba2, false, al, al);
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kPvrtc1Rgba2, image->GetFormat());
  EXPECT_EQ(8U, image->GetWidth());
  EXPECT_EQ(8U, image->GetHeight());
  EXPECT_EQ(16U, image->GetDataSize());  // 2 bits per pixel.
  EXPECT_FALSE(image->GetData()->IsWipeable());
}

TEST(ConversionUtils, Jpeg) {
  // Create some sample JPEG data and convert it to RGB888.
  const std::vector<uint8> jpeg_data = Create8x8JpegData();
  ImagePtr image = ConvertFromExternalImageData(&jpeg_data[0], jpeg_data.size(),
      false, false, base::AllocatorPtr());
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(8U, image->GetWidth());
  EXPECT_EQ(8U, image->GetHeight());
  EXPECT_EQ(8U * 8U * 3U, image->GetDataSize());
  EXPECT_FALSE(image->GetData()->IsWipeable());

  using testing::kExpectedJpegBytes;
  static const size_t kExpectedArraySize = ARRAYSIZE(kExpectedJpegBytes);
  EXPECT_TRUE(ImageMatchesBytes(*image,
                                kExpectedJpegBytes, kExpectedArraySize));

  // Test vertical flipping.
  ImagePtr flipped = ConvertFromExternalImageData(&jpeg_data[0],
      jpeg_data.size(), true, false, base::AllocatorPtr());
  ASSERT_FALSE(flipped.Get() == NULL);
  CompareFlipped(*image, *flipped);

  // Test data wiping.
  ImagePtr wipeable = ConvertFromExternalImageData(&jpeg_data[0],
      jpeg_data.size(), false, true, base::AllocatorPtr());
  EXPECT_TRUE(wipeable->GetData()->IsWipeable());
}

TEST(ConversionUtils, PngRgb) {
  // Create some sample PNG data and convert it to RGB888.
  const std::vector<uint8> png_data = Create8x8PngRgbData();
  ImagePtr image = ConvertFromExternalImageData(&png_data[0], png_data.size(),
      false, false, base::AllocatorPtr());
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(8U, image->GetWidth());
  EXPECT_EQ(8U, image->GetHeight());
  EXPECT_EQ(8U * 8U * 3U, image->GetDataSize());

  using testing::kExpectedPngRgbBytes;
  static const size_t kExpectedArraySize = ARRAYSIZE(kExpectedPngRgbBytes);
  EXPECT_TRUE(ImageMatchesBytes(*image,
                                kExpectedPngRgbBytes, kExpectedArraySize));

  // Converting back to PNG should work.
  EXPECT_FALSE(ConvertToExternalImageData(image, kPng, false).empty());

  // Test vertical flipping when reading data.
  ImagePtr flipped = ConvertFromExternalImageData(&png_data[0], png_data.size(),
      true, false, base::AllocatorPtr());
  ASSERT_FALSE(flipped.Get() == NULL);
  CompareFlipped(*image, *flipped);

  // Test vertical flipping when writing data.  Unflipped conversion of the
  // flipped data should result in the same flipped Image.
  const std::vector<uint8> flipped_ext =
      ConvertToExternalImageData(image, kPng, true);
  ASSERT_FALSE(flipped_ext.empty());
  ImagePtr flipped2 = ConvertFromExternalImageData(&flipped_ext[0],
      flipped_ext.size(), false, false, base::AllocatorPtr());
  CompareFlipped(*image, *flipped2);
}

TEST(ConversionUtils, PngRgba) {
  // Create some sample PNG data and convert it to RGBA8888.
  const std::vector<uint8> png_data = Create8x8PngRgbaData();
  ImagePtr image = ConvertFromExternalImageData(&png_data[0], png_data.size(),
      false, false, base::AllocatorPtr());
  ASSERT_FALSE(image.Get() == NULL);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(8U, image->GetWidth());
  EXPECT_EQ(8U, image->GetHeight());
  EXPECT_EQ(8U * 8U * 4U, image->GetDataSize());

  using testing::kExpectedPngRgbaBytes;
  static const size_t kExpectedArraySize = ARRAYSIZE(kExpectedPngRgbaBytes);
  EXPECT_TRUE(
      ImageMatchesBytes(*image, kExpectedPngRgbaBytes, kExpectedArraySize));

  // Converting back to PNG should work.
  EXPECT_FALSE(ConvertToExternalImageData(image, kPng, false).empty());
}

TEST(ConversionUtils, Rgba8888IonRawBigEndian) {
  const std::vector<uint8> ion_raw_data =
      CreateRgba8888IonRaw3x3BigEndianData();
  TestIonRaw(&ion_raw_data[0], ion_raw_data.size(), Image::kRgba8888,
      3U, 3U, 3U * 3U * 4U, testing::kExpectedRgba8888IonRaw3x3ImageBytes,
      testing::kExpectedRgba8888IonRaw3x3ImageSizeInBytes);
}

TEST(ConversionUtils, Rgba8888IonRawLittleEndian) {
  const std::vector<uint8> ion_raw_data =
      CreateRgba8888IonRaw3x3LittleEndianData();
  TestIonRaw(&ion_raw_data[0], ion_raw_data.size(), Image::kRgba8888,
      3U, 3U, 3U * 3U * 4U, testing::kExpectedRgba8888IonRaw3x3ImageBytes,
      testing::kExpectedRgba8888IonRaw3x3ImageSizeInBytes);
}

TEST(ConversionUtils, Rgb565IonRawBigEndian) {
  const std::vector<uint8> ion_raw_data =
      CreateRgb565IonRaw3x3BigEndianData();
  TestIonRaw(&ion_raw_data[0], ion_raw_data.size(), Image::kRgb565,
      3U, 3U, 3U * 3U * 2U, testing::kExpectedRgb565IonRaw3x3ImageBytes,
      testing::kExpectedRgb565IonRaw3x3ImageSizeInBytes);
}

TEST(ConversionUtils, Rgb565IonRawLittleEndian) {
  const std::vector<uint8> ion_raw_data =
      CreateRgb565IonRaw3x3LittleEndianData();
  TestIonRaw(&ion_raw_data[0], ion_raw_data.size(), Image::kRgb565,
      3U, 3U, 3U * 3U * 2U, testing::kExpectedRgb565IonRaw3x3ImageBytes,
      testing::kExpectedRgb565IonRaw3x3ImageSizeInBytes);
}

TEST(ConversionUtils, Rgba4444IonRawBigEndian) {
  const std::vector<uint8> ion_raw_data =
      CreateRgba4444IonRaw3x3BigEndianData();
  TestIonRaw(&ion_raw_data[0], ion_raw_data.size(), Image::kRgba4444,
      3U, 3U, 3U * 3U * 2U, testing::kExpectedRgba4444IonRaw3x3ImageBytes,
      testing::kExpectedRgba4444IonRaw3x3ImageSizeInBytes);
}

TEST(ConversionUtils, Rgba4444IonRawLittleEndian) {
  const std::vector<uint8> ion_raw_data =
      CreateRgba4444IonRaw3x3LittleEndianData();
  TestIonRaw(&ion_raw_data[0], ion_raw_data.size(), Image::kRgba4444,
      3U, 3U, 3U * 3U * 2U, testing::kExpectedRgba4444IonRaw3x3ImageBytes,
      testing::kExpectedRgba4444IonRaw3x3ImageSizeInBytes);
}

TEST(ConversionUtils, AlphaIonRawBigEndian) {
  const std::vector<uint8> ion_raw_data =
      CreateAlphaIonRaw3x3BigEndianData();
  TestIonRaw(&ion_raw_data[0], ion_raw_data.size(), Image::kAlpha,
      3U, 3U, 3U * 3U * 1U, testing::kAlphaIonRaw3x3ImageBytes,
      ARRAYSIZE(testing::kAlphaIonRaw3x3ImageBytes));
}

TEST(ConversionUtils, AlphaIonRawLittleEndian) {
  const std::vector<uint8> ion_raw_data =
      CreateAlphaIonRaw3x3LittleEndianData();
  TestIonRaw(&ion_raw_data[0], ion_raw_data.size(), Image::kAlpha,
      3U, 3U, 3U * 3U * 1U, testing::kAlphaIonRaw3x3ImageBytes,
      ARRAYSIZE(testing::kAlphaIonRaw3x3ImageBytes));
}

TEST(ConversionUtils, Rgba8888IonRawInvalidHeader) {
  const std::vector<uint8> ion_raw_data =
      CreateRgba8888IonRawInvalidHeaderData();
  TestNullIonRaw(&ion_raw_data[0], ion_raw_data.size());
}

TEST(ConversionUtils, Rgba8888IonRawWrongSize) {
  const std::vector<uint8> ion_raw_data = CreateRgba8888IonRawWrongSizeData();
  TestNullIonRaw(&ion_raw_data[0], ion_raw_data.size());
}

TEST(ConversionUtils, Rgba8888IonRawPayloadless) {
  const std::vector<uint8> ion_raw_data = CreateRgba8888IonRawPayloadlessData();
  TestNullIonRaw(&ion_raw_data[0], ion_raw_data.size());
}

TEST(ConversionUtils, UnknownIonRaw) {
  const std::vector<uint8> ion_raw_data = CreateUnknownIonRawData();
  TestNullIonRaw(&ion_raw_data[0], ion_raw_data.size());
}

TEST(ConversionUtils, InferFormat) {
  base::LogChecker log_checker;
  // Make sure JPEG, PNG and "ION raw" formats can be inferred when converting
  // from external data.
  {
    const std::vector<uint8> jpeg_data = Create8x8JpegData();
    ImagePtr image = ConvertFromExternalImageData(&jpeg_data[0],
        jpeg_data.size(), false, false, base::AllocatorPtr());
    ASSERT_FALSE(image.Get() == NULL);
    EXPECT_TRUE(image->GetFormat() == Image::kRgb888);
  }
  {
    const std::vector<uint8> jpeg_data = Create8x8GrayJpegData();
    ImagePtr image = ConvertFromExternalImageData(&jpeg_data[0],
        jpeg_data.size(), false, false, base::AllocatorPtr());
    ASSERT_FALSE(image.Get() == NULL);
    EXPECT_TRUE(image->GetFormat() == Image::kLuminance);
  }
  {
    const std::vector<uint8> png_data = Create8x8PngRgbData();
    ImagePtr image = ConvertFromExternalImageData(&png_data[0], png_data.size(),
        false, false, base::AllocatorPtr());
    ASSERT_FALSE(image.Get() == NULL);
    EXPECT_EQ(Image::kRgb888, image->GetFormat());
  }
  {
    const std::vector<uint8> png_data = Create8x8PngRgbaData();
    ImagePtr image = ConvertFromExternalImageData(&png_data[0], png_data.size(),
        false, false, base::AllocatorPtr());
    ASSERT_FALSE(image.Get() == NULL);
    EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  }
  {
    const std::vector<uint8> ion_raw_data =
        CreateRgba8888IonRaw3x3BigEndianData();
    ImagePtr image = ConvertFromExternalImageData(&ion_raw_data[0],
        ion_raw_data.size(), false, false, base::AllocatorPtr());
    ASSERT_FALSE(image.Get() == NULL);
    EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  }
  {
    const std::vector<uint8> ion_raw_data =
        CreateRgb565IonRaw3x3BigEndianData();
    ImagePtr image = ConvertFromExternalImageData(&ion_raw_data[0],
        ion_raw_data.size(), false, false, base::AllocatorPtr());
    ASSERT_FALSE(image.Get() == NULL);
    EXPECT_EQ(Image::kRgb565, image->GetFormat());
  }
  {
    const std::vector<uint8> ion_raw_data =
        CreateRgba4444IonRaw3x3BigEndianData();
    ImagePtr image = ConvertFromExternalImageData(&ion_raw_data[0],
        ion_raw_data.size(), false, false, base::AllocatorPtr());
    ASSERT_FALSE(image.Get() == NULL);
    EXPECT_EQ(Image::kRgba4444, image->GetFormat());
  }
  {
    // This is a luminance-alpha image with one bit per channel which
    // the STB image library can't handle.  Make sure Lodepng does handle it.
    ImagePtr image = ConvertFromExternalImageData(
        testing::kPngLumAlpha48x48ImageBytes,
        ARRAYSIZE(testing::kPngLumAlpha48x48ImageBytes),
        false, false, base::AllocatorPtr());
    ASSERT_FALSE(image.Get() == NULL);
    // Currently Lodepng decodes all paletted images to RGBA.
    EXPECT_EQ(Image::kLuminanceAlpha, image->GetFormat());
    EXPECT_EQ(48U, image->GetWidth());
    EXPECT_EQ(48U, image->GetHeight());
    // Colors should be transparent black and white.  Check a few.
    const uint8* image_bytes =
        static_cast<const uint8*>(image->GetData()->GetData());
    EXPECT_EQ(0U, image_bytes[0]);
    EXPECT_EQ(0U, image_bytes[1]);
    const uint32 num_channels =
        Image::GetNumComponentsForFormat(image->GetFormat());
    for (uint32 y = 0U; y < 48U; ++y) {
      for (uint32 x = 0U; x < 48U; ++x) {
        const uint32 pixel = y * 48U + x;
        const uint8 expected =
            testing::kExpectedLumAlpha48x48ImageBytes[pixel] == '#' ? 255U : 0U;
        EXPECT_EQ(expected, image_bytes[pixel * num_channels + 0]);
        EXPECT_EQ(expected, image_bytes[pixel * num_channels + 1]);
      }
    }
  }
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(ConversionUtils, DownsampleImage2x) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  base::LogChecker logchecker;

  Image::Format test_formats[] =
      { Image::kDxt1, Image::kEtc1, Image::kDxt5, Image::kRgba8888,
        Image::kRgb888, Image::kLuminanceAlpha, Image::kLuminance };

  bool is_wipeable = true;
  for (unsigned int i = 0U; i < ARRAYSIZE(test_formats); ++i) {
    // Verify that a downsampled image is created for a supported format at a
    // reasonable size.
    ImagePtr downsampled =
        DownsampleImage2x(CreateImage(test_formats[i], 128, 128),
                          is_wipeable, al);
    EXPECT_TRUE(downsampled->GetFormat() == test_formats[i]);
    EXPECT_EQ(downsampled->GetWidth(), 64U);
    EXPECT_EQ(downsampled->GetData()->IsWipeable(), is_wipeable);

    // Expect that no downsampled image is created for image height/width of 1.
    EXPECT_TRUE(DownsampleImage2x(CreateImage(test_formats[i], 1, 128),
                                  is_wipeable, al).Get() == NULL);
    EXPECT_TRUE(DownsampleImage2x(CreateImage(test_formats[i], 128, 1),
                                  is_wipeable, al).Get() == NULL);
    is_wipeable = !is_wipeable;
  }
  EXPECT_FALSE(logchecker.HasAnyMessages());

  // Expect downsampling an unsupported format creates no downsampled image.
  EXPECT_TRUE(
      DownsampleImage2x(CreateImage(Image::kRgb565, 128, 128),
                        false, al).Get() == NULL);
#if ION_DEBUG
  EXPECT_TRUE(logchecker.HasMessage("WARNING", "not supported"));
#endif
}

TEST(ConversionUtils, DownsampleImage2x8bpc) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  bool is_wipeable = true;
  uint8 temp[64];

  // Verify that the data in downsampled supported 8bpc images is correct.

  // Make colors more distinct.
  // 3 pixels: black and white and green.
  const uint8 data[] = {
    0,     0,   0, 255,
    255, 255, 255, 255,
    0,   255,   0, 255
  };
  const size_t pattern_pixels = sizeof(data) / 4;
  Image::Format test_formats[] =
      { Image::kLuminance, Image::kLuminanceAlpha, Image::kRgb888,
        Image::kRgba8888 };

  for (unsigned int i = 0U; i < ARRAYSIZE(test_formats); ++i) {
    const size_t num_channels = i + 1;
    const size_t pattern_size = pattern_pixels * num_channels;
    // Extract the N test channels from the 4 channel source data.
    EXPECT_GE(sizeof(temp), pattern_pixels * num_channels);
    Copy4BppToN(data, pattern_pixels, temp, num_channels);
    vector<uint8> pattern(pattern_size);
    std::copy(temp, temp + pattern_size, pattern.begin());
    {
      ImagePtr image = CreateImageWithPattern(test_formats[i], 4, 4, pattern);
      ImagePtr downsampled = DownsampleImage2x(image, is_wipeable, al);
      EXPECT_EQ(downsampled->GetWidth(), 2U);
      EXPECT_EQ(downsampled->GetHeight(), 2U);
      // Rgba src image is:
      //   B W G B
      //   W G B W
      //   G B W G
      //   B W G B
      const uint8 expected[] = {
        127, 191, 127, 255,   64, 127, 64, 255,
        64,  127,  64, 255,   64, 191, 64, 255
      };
      const size_t expected_pixels = ARRAYSIZE(expected) / 4;
      EXPECT_GE(sizeof(temp), expected_pixels * num_channels);
      // Extract the N expected channels from the 4 channel source data.
      Copy4BppToN(expected, expected_pixels, temp, num_channels);
      EXPECT_TRUE(ImageMatchesBytes(*downsampled, temp,
          expected_pixels * num_channels));
    }

    // Repeat with odd size
    {
      ImagePtr image = CreateImageWithPattern(test_formats[i], 5, 5, pattern);
      ImagePtr downsampled = DownsampleImage2x(image, is_wipeable, al);
      EXPECT_EQ(downsampled->GetWidth(), 3U);
      EXPECT_EQ(downsampled->GetHeight(), 3U);
      // Rgba src image is:
      //   B W G B W
      //   G B W G B
      //   W G B W G
      //   B W G B W
      //   G B W G B
      const uint8 expected[] = {
        64,  127,  64, 255,   64, 191,  64, 255,  127, 127, 127, 255,
        127, 191, 127, 255,   64, 127,  64, 255,  127, 255, 127, 255,
        0,   127,   0, 255,  127, 255, 127, 255,    0,   0,   0, 255
      };
      const size_t expected_pixels = ARRAYSIZE(expected) / 4;
      EXPECT_GE(sizeof(temp), expected_pixels * num_channels);
      // Extract the N expected channels from the 4 channel source data.
      Copy4BppToN(expected, expected_pixels, temp, num_channels);
      EXPECT_TRUE(ImageMatchesBytes(*downsampled, temp,
          expected_pixels * num_channels));
    }
  }
}

TEST(ConversionUtils, ResizeImageSame) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  bool is_wipeable = true;

  ImagePtr image = CreateImage(Image::kRgba8888, 2, 2);
  image = ResizeImage(image, 2, 2, is_wipeable, al);
  const uint8 expected[] = {
    0x0, 0x1, 0x2, 0x3,  0x4, 0x5, 0x6, 0x7,
    0x8, 0x9, 0xA, 0xB,  0xC, 0xD, 0xE, 0xF
  };
  EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
}

TEST(ConversionUtils, ResizeImageHalf) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  bool is_wipeable = true;
  uint8 temp[64];

  // Verify that the data in downsampled supported 8bpc images is correct.

  // Make colors more distinct.
  // 3 pixels: black and white and green.
  const uint8 data[] = {
    0,     0,   0, 255,
    255, 255, 255, 255,
    0,   255,   0, 255
  };
  const size_t pattern_pixels = sizeof(data) / 4;
  Image::Format test_formats[] =
      { Image::kLuminance, Image::kLuminanceAlpha, Image::kRgb888,
        Image::kRgba8888 };

  for (unsigned int i = 0U; i < ARRAYSIZE(test_formats); ++i) {
    const size_t num_channels = i + 1;
    const size_t pattern_size = pattern_pixels * num_channels;
    // Extract the N test channels from the 4 channel source data.
    EXPECT_GE(sizeof(temp), pattern_pixels * num_channels);
    Copy4BppToN(data, pattern_pixels, temp, num_channels);
    vector<uint8> pattern(pattern_size);
    std::copy(temp, temp + pattern_size, pattern.begin());
    {
      ImagePtr image = CreateImageWithPattern(test_formats[i], 4, 4, pattern);
      ImagePtr downsampled = ResizeImage(image, 2, 2, is_wipeable, al);
      EXPECT_EQ(downsampled->GetWidth(), 2U);
      EXPECT_EQ(downsampled->GetHeight(), 2U);
      // Rgba src image is:
      //   B W G B
      //   W G B W
      //   G B W G
      //   B W G B
      const uint8 expected[] = {
        128, 191, 128, 255,   64, 128, 64, 255,
        64,  128,  64, 255,   64, 191, 64, 255
      };
      const size_t expected_pixels = ARRAYSIZE(expected) / 4;
      EXPECT_GE(sizeof(temp), expected_pixels * num_channels);
      // Extract the N expected channels from the 4 channel source data.
      Copy4BppToN(expected, expected_pixels, temp, num_channels);
      EXPECT_TRUE(ImageMatchesBytes(*downsampled, temp,
          expected_pixels * num_channels));
    }

    // Repeat with odd size
    {
      ImagePtr image = CreateImageWithPattern(test_formats[i], 5, 5, pattern);
      ImagePtr downsampled = ResizeImage(image, 3, 3, is_wipeable, al);
      EXPECT_EQ(downsampled->GetWidth(), 3U);
      EXPECT_EQ(downsampled->GetHeight(), 3U);
      // Rgba src image is:
      //   B W G B W
      //   G B W G B
      //   W G B W G
      //   B W G B W
      //   G B W G B
      // Going from 5x5 to 3x3, the dst pixel is 1.67x1.67 src units.
      // So for example the first channel is 61.  That comes from the weighted
      // average of the red channel of the upper left 2x2 values in the src
      // image:
      //    (1.0*1.0 * 0 + .67*1.0 * 255
      //   + 1.0*.67 * 0 + .67*.67 * 0  ) / 1.67*1.67
      //   = 255*.67 / 1.67^2 = 61.2
      // which gets rounded to 61.
      const uint8 expected[] = {
        61, 122, 61, 255,    92, 204, 92, 255,    92, 133, 92, 255,
        112, 204, 112, 255,  71, 143, 71, 255,    92, 204, 92, 255,
        41, 133, 41, 255,    112, 204, 112, 255,  61, 122, 61, 255,
      };
      const size_t expected_pixels = ARRAYSIZE(expected) / 4;
      EXPECT_GE(sizeof(temp), expected_pixels * num_channels);
      // Extract the N expected channels from the 4 channel source data.
      Copy4BppToN(expected, expected_pixels, temp, num_channels);
      EXPECT_TRUE(ImageMatchesBytes(*downsampled, temp,
          expected_pixels * num_channels));
    }
  }
}

TEST(ConversionUtils, ResizeImageTo1x1) {
  // Resizing any image to 1x1 should result in a pixel that is the average of
  // all the input pixel values.
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  bool is_wipeable = true;

  Image::Format test_formats[] =
      { Image::kLuminance, Image::kLuminanceAlpha, Image::kRgb888,
        Image::kRgba8888 };
  // The byte values in source are 0,1,2,3...etc.
  // So for luminance the average is [(0+1+...+14)/15 == (0+14)/2].
  // for lum+alpha it's [(0+2+...+28)/15 == (0+28)/2, (1+3+...+29)/15 == 1+29/2]
  // And so on.
  uint8 expected[][4] = {
    { 7 },  // (0+14)/2
    { 14, 15 },  // (0+28)/2, (1+29)/2
    { 21, 22, 23 },  // (0+42)/2, (1+43)/2, (2+44)/2
    { 28, 29, 30, 31 }  // (0+56)/2, (1+57)/2, (2+58)/2, (3+59)/2
  };
  const uint32 kWidth = 5;
  const uint32 kHeight = 3;

  for (unsigned int i = 0U; i < ARRAYSIZE(test_formats); ++i) {
    const size_t num_channels =
        Image::GetNumComponentsForFormat(test_formats[i]);
    ImagePtr image = CreateImage(test_formats[i], kWidth, kHeight);
    ImagePtr downsampled = ResizeImage(image, 1, 1, is_wipeable, al);
    EXPECT_EQ(downsampled->GetWidth(), 1U);
    EXPECT_EQ(downsampled->GetHeight(), 1U);
    // Extract the N expected channels from the 4 channel source data.
    EXPECT_TRUE(ImageMatchesBytes(*downsampled, expected[i], num_channels));
  }
}

TEST(ConversionUtils, ResizeImageDouble) {
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  bool is_wipeable = true;
  uint8 temp[64];

  // Verify that the data in downsampled supported 8bpc images is correct.

  // 3 pixels: black and white and green.  Repeat those in the input.
  const uint8 data[] = {
    0,     0,   0, 255,
    255, 255, 255, 255,
    0,   255,   0, 255
  };
  const size_t pattern_pixels = sizeof(data) / 4;
  Image::Format test_formats[] =
      { Image::kLuminance, Image::kLuminanceAlpha, Image::kRgb888,
        Image::kRgba8888 };

  for (unsigned int i = 0U; i < ARRAYSIZE(test_formats); ++i) {
    const size_t num_channels = i + 1;
    const size_t pattern_size = pattern_pixels * num_channels;
    // Extract the N test channels from the 4 channel source data.
    EXPECT_GE(sizeof(temp), pattern_pixels * num_channels);
    Copy4BppToN(data, pattern_pixels, temp, num_channels);
    vector<uint8> pattern(pattern_size);
    std::copy(temp, temp + pattern_size, pattern.begin());
    {
      ImagePtr image = CreateImageWithPattern(test_formats[i], 2, 2, pattern);
      ImagePtr downsampled = ResizeImage(image, 4, 4, is_wipeable, al);
      EXPECT_EQ(downsampled->GetWidth(), 4U);
      EXPECT_EQ(downsampled->GetHeight(), 4U);
      // Rgba src image is:
      //   B W
      //   G B
      // Output should be:
      //   B    B/W  W/B  W
      //   B/G  B+   W+   W/B
      //   G/B  G+   B+   B/W
      //   G    G/B  B/G  B
      // Where A/B means 0.75*A + 0.25*B.
      // And A+ means 0.75^2*A + 0.25*0.75*B + 0.25*0.75*C + 0.25^2*D
      // where B, and C are directly adjacent and D is the diagonally adjacent
      // color.
      const uint8 A = 255;
      const uint8 expected[] = {
        0,   0, 0, A,  64,  64, 64, A,  191, 191, 191, A,  255, 255, 255, A,
        0,  64, 0, A,  48,  96, 48, A,  143, 159, 143, A,  191, 191, 191, A,
        0, 191, 0, A,  16, 159, 16, A,   48,  96,  48, A,   64,  64,  64, A,
        0, 255, 0, A,   0, 191,  0, A,    0,  64,   0, A,    0,   0,   0, A
      };
      const size_t expected_pixels = ARRAYSIZE(expected) / 4;
      EXPECT_GE(sizeof(temp), expected_pixels * num_channels);
      // Extract the N expected channels from the 4 channel source data.
      Copy4BppToN(expected, expected_pixels, temp, num_channels);
      EXPECT_TRUE(ImageMatchesBytes(*downsampled, temp,
          expected_pixels * num_channels));
    }
  }
}

TEST(ConversionUtils, ResizeImageUnsupported) {
  // Non-8bpc images aren't supported, and resizing should return NULL.
  base::AllocatorPtr al;  // NULL pointer means use default allocator.
  ImagePtr float_image = CreateImage(Image::kRg16fFloat, 5, 5);
  EXPECT_TRUE(ResizeImage(float_image, 5, 5, false, al).Get() == NULL);
  EXPECT_TRUE(ResizeImage(float_image, 5, 10, false, al).Get() == NULL);
  EXPECT_TRUE(ResizeImage(float_image, 10, 5, false, al).Get() == NULL);
  EXPECT_TRUE(ResizeImage(float_image, 10, 10, false, al).Get() == NULL);
  EXPECT_TRUE(ResizeImage(float_image, 2, 5, false, al).Get() == NULL);
  EXPECT_TRUE(ResizeImage(float_image, 5, 2, false, al).Get() == NULL);
  EXPECT_TRUE(ResizeImage(float_image, 2, 2, false, al).Get() == NULL);
}

TEST(ConversionUtils, FlipImage) {
  // RGBA even height.
  {
    ImagePtr image = CreateImage(Image::kRgba8888, 2, 2);
    FlipImage(image);
    const uint8 expected[] = {
      0x8, 0x9, 0xA, 0xB,  0xC, 0xD, 0xE, 0xF,
      0x0, 0x1, 0x2, 0x3,  0x4, 0x5, 0x6, 0x7
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // RGBA odd height.
  {
    ImagePtr image = CreateImage(Image::kRgba8888, 2, 3);
    FlipImage(image);
    const uint8 expected[] = {
      0x10, 0x11, 0x12, 0x13,  0x14, 0x15, 0x16, 0x17,
      0x8, 0x9, 0xA, 0xB,  0xC, 0xD, 0xE, 0xF,
      0x0, 0x1, 0x2, 0x3,  0x4, 0x5, 0x6, 0x7
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // RGB even height.
  {
    ImagePtr image = CreateImage(Image::kRgb888, 2, 2);
    FlipImage(image);
    const uint8 expected[] = {
      0x6, 0x7, 0x8,   0x9, 0xA, 0xB,
      0x0, 0x1, 0x2,   0x3, 0x4, 0x5
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // RGB odd height.
  {
    ImagePtr image = CreateImage(Image::kRgb888, 2, 3);
    FlipImage(image);
    const uint8 expected[] = {
      0xC, 0xD, 0xE,  0xF, 0x10, 0x11,
      0x6, 0x7, 0x8,  0x9, 0xA, 0xB,
      0x0, 0x1, 0x2,  0x3, 0x4, 0x5,
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // Luminance odd height.
  {
    ImagePtr image = CreateImage(Image::kLuminance, 2, 5);
    FlipImage(image);
    const uint8 expected[] = {
      0x8, 0x9,
      0x6, 0x7,
      0x4, 0x5,
      0x2, 0x3,
      0x0, 0x1,
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // Image wider than the internal row buffer used for swapping.
  {
    const size_t kWidth = 2051;
    ImagePtr image = CreateImage(Image::kLuminance, kWidth, 2);
    FlipImage(image);
    vector<uint8> expected(kWidth * 2);
    for (size_t i = 0; i < kWidth; ++i) {
      expected[i] = (i + kWidth) & 0xff;
    }
    for (size_t i = kWidth; i < kWidth * 2; ++i) {
      expected[i] = (i - kWidth) & 0xff;
    }
    // This test only makes sense if the two rows of the image are different.
    EXPECT_NE(expected[0], expected[kWidth]);
    EXPECT_TRUE(ImageMatchesBytes(*image, &expected[0], expected.size()));
  }

  // No content.  Also height of one, neither of which need flipping.
  {
    base::LogChecker logchecker;
    ImagePtr image;
    FlipImage(image);
    EXPECT_FALSE(logchecker.HasAnyMessages());

    ImagePtr image1 = CreateImage(Image::kLuminance, 5, 1);
    FlipImage(image1);
    EXPECT_FALSE(logchecker.HasAnyMessages());
    const uint8 expected[] = {
      0x0, 0x1, 0x2, 0x3, 0x4
    };
    EXPECT_TRUE(ImageMatchesBytes(*image1, expected, ARRAYSIZE(expected)));
  }

  // Check that for a compressed image we log a warning.
  {
    base::LogChecker logchecker;
    base::AllocatorPtr al;  // NULL pointer means use default allocator.
    ImagePtr image = CreateImage(Image::kRgb888, 4, 4);
    // Compress using DXTC.
    image = ConvertImage(image, Image::kDxt1, false, al, al);
    vector<uint8> bytes_before_flip(image->GetDataSize());
    EXPECT_EQ(8U, bytes_before_flip.size());
    std::copy(
      image->GetData()->GetData<uint8>(),
      image->GetData()->GetData<uint8>() + image->GetDataSize(),
      bytes_before_flip.begin());
    FlipImage(image);
#if ION_DEBUG
    EXPECT_TRUE(logchecker.HasMessage("WARNING", "not supported"));
#endif
    // Check that content didn't change.
    EXPECT_TRUE(ImageMatchesBytes(
        *image, &bytes_before_flip[0], bytes_before_flip.size()));
  }
}

TEST(ConversionUtils, FlipImageHorizontally) {
  // RGBA even width.
  {
    ImagePtr image = CreateImage(Image::kRgba8888, 2, 2);
    FlipImageHorizontally(image);
    const uint8 expected[] = {
      0x4, 0x5, 0x6, 0x7,  0x0, 0x1, 0x2, 0x3,
      0xC, 0xD, 0xE, 0xF,  0x8, 0x9, 0xA, 0xB,
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // RGBA odd width.
  {
    ImagePtr image = CreateImage(Image::kRgba8888, 3, 2);
    FlipImageHorizontally(image);
    const uint8 expected[] = {
      0x8, 0x9, 0xA, 0xB,      0x4, 0x5, 0x6, 0x7,      0x0, 0x1, 0x2, 0x3,
      0x14, 0x15, 0x16, 0x17,  0x10, 0x11, 0x12, 0x13,  0xC, 0xD, 0xE, 0xF
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // RGB even width.
  {
    ImagePtr image = CreateImage(Image::kRgb888, 2, 2);
    FlipImageHorizontally(image);
    const uint8 expected[] = {
      0x3, 0x4, 0x5,  0x0, 0x1, 0x2,
      0x9, 0xA, 0xB,  0x6, 0x7, 0x8,
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // RGB odd width.
  {
    ImagePtr image = CreateImage(Image::kRgb888, 3, 2);
    FlipImageHorizontally(image);
    const uint8 expected[] = {
      0x6, 0x7, 0x8,    0x3, 0x4, 0x5,  0x0, 0x1, 0x2,
      0xF, 0x10, 0x11,  0xC, 0xD, 0xE,  0x9, 0xA, 0xB
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // Luminance odd width.
  {
    ImagePtr image = CreateImage(Image::kLuminance, 5, 2);
    FlipImageHorizontally(image);
    const uint8 expected[] = {
      0x4, 0x3, 0x2, 0x1, 0x0,
      0x9, 0x8, 0x7, 0x6, 0x5,
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // No content.  Also width of one, neither of which need flipping.
  {
    base::LogChecker logchecker;
    ImagePtr image;
    FlipImageHorizontally(image);
    EXPECT_FALSE(logchecker.HasAnyMessages());

    ImagePtr image1 = CreateImage(Image::kLuminance, 1, 5);
    FlipImageHorizontally(image1);
    EXPECT_FALSE(logchecker.HasAnyMessages());
    const uint8 expected[] = {
      0x0,
      0x1,
      0x2,
      0x3,
      0x4
    };
    EXPECT_TRUE(ImageMatchesBytes(*image1, expected, ARRAYSIZE(expected)));
  }

  // Check that for a compressed image we log a warning.
  {
    base::LogChecker logchecker;
    base::AllocatorPtr al;  // NULL pointer means use default allocator.
    ImagePtr image = CreateImage(Image::kRgb888, 4, 4);
    // Compress using DXTC.
    image = ConvertImage(image, Image::kDxt1, false, al, al);
    vector<uint8> bytes_before_flip(image->GetDataSize());
    EXPECT_EQ(8U, bytes_before_flip.size());
    std::copy(
      image->GetData()->GetData<uint8>(),
      image->GetData()->GetData<uint8>() + image->GetDataSize(),
      bytes_before_flip.begin());
    FlipImageHorizontally(image);
#if ION_DEBUG
    EXPECT_TRUE(logchecker.HasMessage("WARNING", "not supported"));
#endif
    // Check that content didn't change.
    EXPECT_TRUE(ImageMatchesBytes(
        *image, &bytes_before_flip[0], bytes_before_flip.size()));
  }
}

TEST(ConversionUtils, StraightAlphaFromPremultipliedAlpha) {
  {
    ImagePtr image = CreateImage(Image::kRgba8888, 2, 2);
    StraightAlphaFromPremultipliedAlpha(image);
    const uint8 expected[] = {
      0x0, 0x55, 0xAA, 0x3,  0x91, 0xB6, 0xDA, 0x7,
      0xB9, 0xD0, 0xE7, 0xB,  0xCC, 0xDD, 0xEE, 0xF,
    };
    EXPECT_TRUE(ImageMatchesBytes(*image, expected, ARRAYSIZE(expected)));
  }

  // Check that we log a warning for invalid formats.
  {
    base::LogChecker logchecker;
    base::AllocatorPtr al;  // NULL pointer means use default allocator.

    // Not 8bit.
    ImagePtr image = CreateImage(Image::kRgba4444, 4, 4);
    StraightAlphaFromPremultipliedAlpha(image);
#if ION_DEBUG
    EXPECT_TRUE(logchecker.HasMessage("WARNING", "not supported"));
#endif

    // Not Rgba.
    image = CreateImage(Image::kRgb888, 4, 4);
    vector<uint8> rgb888_bytes_before_call(image->GetDataSize());
    std::copy(
      image->GetData()->GetData<uint8>(),
      image->GetData()->GetData<uint8>() + image->GetDataSize(),
      rgb888_bytes_before_call.begin());
     StraightAlphaFromPremultipliedAlpha(image);
#if ION_DEBUG
    EXPECT_TRUE(logchecker.HasMessage("WARNING", "not supported"));
#endif
    // Check that content didn't change.
    EXPECT_TRUE(ImageMatchesBytes(*image,
        &rgb888_bytes_before_call[0], rgb888_bytes_before_call.size()));
  }
}

}  // namespace image
}  // namespace ion
