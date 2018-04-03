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

#ifndef ION_IMAGE_CONVERSIONUTILS_H_
#define ION_IMAGE_CONVERSIONUTILS_H_

// This file contains utility functions for converting Ion images between
// formats. The following restrictions apply:
//  - Conversion is supported between a limited number of formats.
//  - Conversion may be a multi-step process using an intermediate format.
//  - Conversion between formats containing different numbers of components may
//    be supported. An alpha channel may be removed to convert from an
//    RGBA-type format to an RGB-type format. An alpha channel containing all
//    full-opacity values may be added to convert the other way.

#include <vector>

#include "base/integral_types.h"
#include "ion/base/allocator.h"
#include "ion/gfx/image.h"

namespace ion {
namespace image {

// External image formats supported by ConvertToExternalImageData().
enum ExternalImageFormat {
  kPng,
};

// Specify possible rotation values (in 90 degree increments).
// The integer values represent counter-clockwise rotations in 90 degree
// increments (negative values will give an equivalent clockwise rotation
// instead).
// So, for example, 3 will result in a 3 * 90 == 270 degree CCW rotation.
// -5 will result in a 5 * 90 == 450 (== 90 mod 360) degree CW rotation.
enum ImageRotation {
  kNoRotation = 0,
  kRotateCCW90 = 1,
  kRotate180 = 2,
  kRotateCCW180 = kRotate180,
  kRotateCCW270 = 3,
  kRotateCW90 = -1,
  kRotateCW180 = -2,
  kRotateCW270 = -3,
};

// Converts an existing Image to the given target format and returns the
// resulting Image. It returns a NULL pointer if the conversion is not possible
// for any reason.
//
// Currently-supported conversions:
//   kLuminance, kLuminanceAlpha ->
//     kR8, kRg8, kRgb8, kRgba8, kR32f, kRg32f, kRgb32f, kRgba32f,
//     kEtc1, kDxt1, kDxt5, kPvrtc1Rgba2
//   kR8 <-> kR32f
//   kRg8 <-> kRg32f
//   kRgb8 <-> kRgb32f
//   kRgba8 <-> kRgba32f
//   kEtc1 <-> kRgb8, kRgb32f, kDxt1
//   kDxt1 <-> kRgb8, kRgb32f, kEtc1
//   kDxt5 <-> kRgba8, kRgba32f
//   kPvrtc1Rgba2 <- kRgba8, kRgba32f, kDxt5 (only one direction available).
//   kR8 <- kRg8, kRgb8, kRgba8, kEtc1, kDxt1, kDxt5
//   kR32f <- kRg32f, kRgb32f, kRgba32f
//
// Unsized formats are treated as their sized counterparts:
//   kRgb888 == kRgb8
//   kRgba8888 == kRgba8
//   kRgbafloat == kRgba32f
//
// Note also that kPvrtc1Rgba2 only supports power-of-two-sized square textures
// at least 8x8 pixels in size.
//
// The conversions between the 8-bpc and floating-point types map the range
// [0, 255] <-> [0.0f, 1.0f].
//
// The conversions to kR8/kR32f extract the red channel from an Rgb(a) image.
// These images can be used as luminance textures and use 1/4 the GPU memory of
// an uncompressed monochrome Rgb image.
//
// Conversion between 3-component and 4-component formats is not yet supported.
// The |is_wipeable| flag is passed to the DataContainer for the new Image.
// |allocator| is used for the resulting image; if it is NULL, the default
// allocator is used. |temporary_allocator| is used for internal allocations
// that will be discarded.
ION_API const gfx::ImagePtr ConvertImage(
    const gfx::ImagePtr& image, gfx::Image::Format target_format,
    bool is_wipeable,
    const base::AllocatorPtr& allocator,
    const base::AllocatorPtr& temporary_allocator);

// Converts external image |data| to an ImagePtr with data in canonical format.
// |data_size| is the number of bytes in |data|. Input format is inferred
// from |data|.
//
// Supported formats: JPEG, PNG, TGA, BMP, PSD, GIF, HDR, PIC and "ION raw"
// format (see below for specs of this "ION raw" format). This method attempts
// to interpret |data| as the above formats, one after another in the above
// order until success, otherwise returns a NULL ImagePtr (i.e. when all
// supported formats fail for any reasons).
//
// If |flip_vertically| is true, the resulting image is inverted in the Y
// dimension.  The |is_wipeable| flag is passed to the DataContainer for the
// Image. |allocator| is used for the resulting image; if it is NULL, the
// default allocator is used.
//
// "ION raw" image format specs:
// * byte #0: 1-byte const 0x89 (non-ASCII)
// * bytes #1-3: three 1-byte consts 0x52 0x41 0x57 (ASCII "RAW")
// * bytes #4-5: 2-byte const 0x0001 or 0x0100 (Endianness indicator)
// * bytes #6-7: 2-byte unsigned integer as format indicator
// * bytes #8-11: unsigned 4-byte integer for width (in pixels)
// * bytes #12-15: unsigned 4-byte integer for height (in pixels)
// * bytes #16-onwards: image data payload.
//
// Endianness of "ION raw" format:
// * Applicable to format indicator, width, height and every pixel value
// * Big/Little Endian if bytes #6-7 are 0x0001/0x0100 respectively.
//
// Formats supported in "ION raw":
// * Image::kRgba8888. Format indicator (bytes #4-5): 0
//   Payload structure (in Android at least, where it's called ARGB_8888):
//   low-addr [R7...R0][G7...G0][B7...B0][A7...A0] high-addr (Little Endian)
//   low-addr [A7...A0][B7...B0][G7...G0][R7...R0] high-addr (Big Endian)
// * Image::kRgb565. Format indicator (bytes #4-5): 1
//   Payload structure (in Android at least, where it's called RGB_565):
//   low-addr [G2...G0B4...B0][R4...R0G5...G3] high-addr (Little Endian)
//   low-addr [R4...R0G5...G3][G2...G0B4...B0] high-addr (Big Endian)
// * Image::kRgba4444. Format indicator (bytes #4-5): 2
//   Payload structure (in Android at least, where it's called ARGB_4444):
//   low-addr [B3...B0A3...A0][R3...R0G3...G0] high-addr (Little Endian)
//   low-addr [R3...R0G3...G0][B3...B0A3...A0] high-addr (Big Endian)
// * Image::kAlpha. Format indicator (bytes #4-5): 3
//   Payload structure (in Android at least, where it's called ALPHA_8):
//   low-addr [A7...A0] high-addr (Little/Big Endian)
ION_API const gfx::ImagePtr ConvertFromExternalImageData(
    const void* data, size_t data_size, bool flip_vertically, bool is_wipeable,
    const base::AllocatorPtr& allocator);

// Returns true if "Ion raw" format header is detected in |data|.
ION_API bool IsIonRawImageFormat(const void* data, size_t data_size);

// Converts an existing Image to data in |external_format|, returning a vector.
// If |flip_vertically| is true, the resulting image is inverted in the Y
// dimension. The vector will be empty if the conversion is not possible for any
// reason.
ION_API const std::vector<uint8> ConvertToExternalImageData(
    const gfx::ImagePtr& image, ExternalImageFormat external_format,
    bool flip_vertically);

// Returns an image half the width and height of |image|. Currently only kDxt1,
// kDxt5, kEtc1, and 8-bit-per-channel images are supported; other input formats
// will return a NULL pointer. The |is_wipeable| flag is passed to the
// DataContainer for the new Image. |allocator| is used for allocating the
// resulting image, unless it is NULL, then the default C++ allocator will be
// used.
ION_API const gfx::ImagePtr DownsampleImage2x(
    const gfx::ImagePtr& image, bool is_wipeable,
    const base::AllocatorPtr& allocator);

// Returns a copy of |image| scaled to the specified dimensions. Currently only
// 8-bit-per-channel formats work; other input formats will return a NULL
// pointer. The |is_wipeable| flag is passed to the DataContainer for the new
// Image. |allocator| is used for allocating the resulting image, unless it is
// NULL, then the default C++ allocator will be used.
ION_API const gfx::ImagePtr ResizeImage(
    const gfx::ImagePtr& image, uint32 out_width, uint32 out_height,
    bool is_wipeable, const base::AllocatorPtr& allocator);

// Flips an image vertically in place.
// Doesn't work with compressed image formats (logs a warning).
ION_API void FlipImage(const gfx::ImagePtr& image);

// Flips an image horizontally in place.
// Doesn't work with compressed image formats (logs a warning).
ION_API void FlipImageHorizontally(const gfx::ImagePtr& image);

// Rotates an image counter-clockwise by the specified amount (see the
// ImageRotation enum comments for details).
// Doesn't work with compressed image formats (logs a warning).
ION_API void RotateImage(const gfx::ImagePtr& image, ImageRotation rotation);

// Converts a "pre-multiplied alpha" RGBA image into a "straight alpha" RGBA
// image.  RGB values are divided by alpha (except when alpha = 0).
ION_API void StraightAlphaFromPremultipliedAlpha(const gfx::ImagePtr& image);

}  // namespace image
}  // namespace ion

#endif  // ION_IMAGE_CONVERSIONUTILS_H_
