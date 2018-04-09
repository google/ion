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
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/static_assert.h"
#include "ion/portgfx/glheaders.h"
#include "absl/base/macros.h"

namespace ion {
namespace gfx {

const uint32 Image::kNumFormats = kEglImage + 1;

Image::Image()
    : format_(kRgb888),
      width_(0),
      height_(0),
      depth_(0),
      data_size_(0),
      type_(kDense),
      dims_(k2d) {}

Image::~Image() {
  if (base::DataContainer* data = data_.Get()) data->RemoveReceiver(this);
}

void Image::Set(Format format, uint32 width, uint32 height,
                const base::DataContainerPtr& data) {
  SetData(kDense, k2d, format, width, height, 1, data);
}

void Image::Set(Format format, uint32 width, uint32 height, uint32 depth,
                const base::DataContainerPtr& data) {
  SetData(kDense, k3d, format, width, height, depth, data);
}

void Image::SetArray(Format format, uint32 width, uint32 num_planes,
                     const base::DataContainerPtr& data) {
  SetData(kArray, k2d, format, width, num_planes, 1, data);
}
void Image::SetArray(Format format, uint32 width, uint32 height,
                     uint32 num_planes, const base::DataContainerPtr& data) {
  Set(format, width, height, num_planes, data);
  SetData(kArray, k3d, format, width, height, num_planes, data);
}

void Image::SetEglImage(const base::DataContainerPtr& image) {
  // External textures are special, since their specification is done outside of
  // GL.
  SetData(kEgl, k2d, kEglImage, 0, 0, 0, image);
}

void Image::SetEglImageArray(const base::DataContainerPtr& image) {
  // External textures are special, since their specification is done outside of
  // GL.
  SetData(kEgl, k3d, kEglImage, 0, 0, 0, image);
}

void Image::SetExternalEglImage(const base::DataContainerPtr& external_image) {
  // External textures are special, since their specification is done outside of
  // GL.
  SetData(kExternalEgl, k2d, kEglImage, 0, 0, 0, external_image);
}

const Image::PixelFormat& Image::GetPixelFormat(Format format) {
  // See http://www.khronos.org/opengles/sdk/docs/man3/xhtml/glTexImage2D.xml
  // for the table most of these values are sourced from.
  static const PixelFormat kPixelFormats[] = {
      /* kAlpha                   */ {GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE},
      /* kLuminance               */ {GL_LUMINANCE, GL_LUMINANCE,
                                      GL_UNSIGNED_BYTE},
      /* kLuminanceAlpha          */ {GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA,
                                      GL_UNSIGNED_BYTE},
      /* kRgb888                  */ {GL_RGB, GL_RGB, GL_UNSIGNED_BYTE},
      /* kRgba8888                */ {GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE},
      /* kRgb565                  */ {GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
      /* kRgba4444                */ {GL_RGBA, GL_RGBA,
                                      GL_UNSIGNED_SHORT_4_4_4_4},
      /* kRgba5551                */ {GL_RGBA, GL_RGBA,
                                      GL_UNSIGNED_SHORT_5_5_5_1},
      /* kRgbaFloat               */ {GL_RGBA, GL_RGBA, GL_FLOAT},
      /* kR8                      */ {GL_R8, GL_RED, GL_UNSIGNED_BYTE},
      /* kRSigned8                */ {GL_R8_SNORM, GL_RED, GL_BYTE},
      /* kR8i                     */ {GL_R8I, GL_RED_INTEGER, GL_BYTE},
      /* kR8ui                    */ {GL_R8UI, GL_RED_INTEGER,
                                      GL_UNSIGNED_BYTE},
      /* kR16fFloat               */ {GL_R16F, GL_RED, GL_FLOAT},
      /* kR16fHalf                */ {GL_R16F, GL_RED, GL_HALF_FLOAT},
      /* kR16i                    */ {GL_R16I, GL_RED_INTEGER, GL_SHORT},
      /* kR16ui                   */ {GL_R16UI, GL_RED_INTEGER,
                                      GL_UNSIGNED_SHORT},
      /* kR32f                    */ {GL_R32F, GL_RED, GL_FLOAT},
      /* kR32i                    */ {GL_R32I, GL_RED_INTEGER, GL_INT},
      /* kR32ui                   */ {GL_R32UI, GL_RED_INTEGER,
                                      GL_UNSIGNED_INT},
      /* kRg8                     */ {GL_RG8, GL_RG, GL_UNSIGNED_BYTE},
      /* kRgSigned8               */ {GL_RG8_SNORM, GL_RG, GL_BYTE},
      /* kRg8i                    */ {GL_RG8I, GL_RG_INTEGER, GL_BYTE},
      /* kRg8ui                   */ {GL_RG8UI, GL_RG_INTEGER,
                                      GL_UNSIGNED_BYTE},
      /* kRg16fFloat              */ {GL_RG16F, GL_RG, GL_FLOAT},
      /* kRg16fHalf               */ {GL_RG16F, GL_RG, GL_HALF_FLOAT},
      /* kRg16i                   */ {GL_RG16I, GL_RG_INTEGER, GL_SHORT},
      /* kRg16ui                  */ {GL_RG16UI, GL_RG_INTEGER,
                                      GL_UNSIGNED_SHORT},
      /* kRg32f                   */ {GL_RG32F, GL_RG, GL_FLOAT},
      /* kRg32i                   */ {GL_RG32I, GL_RG_INTEGER, GL_INT},
      /* kRg32ui                  */ {GL_RG32UI, GL_RG_INTEGER,
                                      GL_UNSIGNED_INT},
      /* kRgb8                    */ {GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE},
      /* kRgbSigned8              */ {GL_RGB8_SNORM, GL_RGB, GL_BYTE},
      /* kRgb8i                   */ {GL_RGB8I, GL_RGB_INTEGER, GL_BYTE},
      /* kRgb8ui                  */ {GL_RGB8UI, GL_RGB_INTEGER,
                                      GL_UNSIGNED_BYTE},
      /* kRgb16fFloat             */ {GL_RGB16F, GL_RGB, GL_FLOAT},
      /* kRgb16fHalf              */ {GL_RGB16F, GL_RGB, GL_HALF_FLOAT},
      /* kRgb16i                  */ {GL_RGB16I, GL_RGB_INTEGER, GL_SHORT},
      /* kRgb16ui                 */ {GL_RGB16UI, GL_RGB_INTEGER,
                                      GL_UNSIGNED_SHORT},
      /* kRgb32f                  */ {GL_RGB32F, GL_RGB, GL_FLOAT},
      /* kRgb32i                  */ {GL_RGB32I, GL_RGB_INTEGER, GL_INT},
      /* kRgb32ui                 */ {GL_RGB32UI, GL_RGB_INTEGER,
                                      GL_UNSIGNED_INT},
      /* kRgba8                   */ {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},
      /* kRgbaSigned8             */ {GL_RGBA8_SNORM, GL_RGBA, GL_BYTE},
      /* kRgba8i                  */ {GL_RGBA8I, GL_RGBA_INTEGER, GL_BYTE},
      /* kRgba8ui                 */ {GL_RGBA8UI, GL_RGBA_INTEGER,
                                      GL_UNSIGNED_BYTE},
      /* kRgb10a2                 */ {GL_RGB10_A2, GL_RGBA,
                                      GL_UNSIGNED_INT_2_10_10_10_REV},
      /* kRgb10a2ui               */ {GL_RGB10_A2UI, GL_RGBA_INTEGER,
                                      GL_UNSIGNED_INT_2_10_10_10_REV},
      /* kRgba16fFloat            */ {GL_RGBA16F, GL_RGBA, GL_FLOAT},
      /* kRgba16fHalf             */ {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT},
      /* kRgba16i                 */ {GL_RGBA16I, GL_RGBA_INTEGER, GL_SHORT},
      /* kRgba16ui                */ {GL_RGBA16UI, GL_RGBA_INTEGER,
                                      GL_UNSIGNED_SHORT},
      /* kRgba32f                 */ {GL_RGBA32F, GL_RGBA, GL_FLOAT},
      /* kRgba32i                 */ {GL_RGBA32I, GL_RGBA_INTEGER, GL_INT},
      /* kRgba32ui                */ {GL_RGBA32UI, GL_RGBA_INTEGER,
                                      GL_UNSIGNED_INT},
      /* kRenderbufferDepth16     */ {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT,
                                      GL_UNSIGNED_SHORT},
      /* kRenderbufferDepth24     */ {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT,
                                      GL_UNSIGNED_INT},
      /* kRenderbufferDepth32f    */ {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT,
                                      GL_FLOAT},
      /* kRenderbufferDepth24Stencil8 */ {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL,
                                          GL_UNSIGNED_INT_24_8},
      /* kRenderbufferDepth32fStencil8*/ {GL_DEPTH32F_STENCIL8,
                                          GL_DEPTH_STENCIL,
                                          GL_FLOAT_32_UNSIGNED_INT_24_8_REV},
      /* kTextureDepth16Int       */ {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,
                                      GL_UNSIGNED_INT},
      /* kTextureDepth16Short     */ {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT,
                                      GL_UNSIGNED_SHORT},
      /* kTextureDepth24          */ {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT,
                                      GL_UNSIGNED_INT},
      /* kTextureDepth24Stencil8  */ {GL_DEPTH_STENCIL, GL_DEPTH_STENCIL,
                                      GL_UNSIGNED_INT_24_8},
      /* kTextureDepth32f         */ {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT,
                                      GL_FLOAT},
      /* kTextureDepth32fStencil8 */ {GL_DEPTH32F_STENCIL8,
                                      GL_DEPTH_STENCIL,
                                      GL_FLOAT_32_UNSIGNED_INT_24_8_REV},
      /* kStencil8                */ {GL_STENCIL_INDEX8, GL_STENCIL,
                                      GL_UNSIGNED_BYTE},
      /* kAstc4x4Rgba             */ {GL_COMPRESSED_RGBA_ASTC_4x4_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc5x4Rgba             */ {GL_COMPRESSED_RGBA_ASTC_5x4_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc5x5Rgba             */ {GL_COMPRESSED_RGBA_ASTC_5x5_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc6x5Rgba             */ {GL_COMPRESSED_RGBA_ASTC_6x5_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc6x6Rgba             */ {GL_COMPRESSED_RGBA_ASTC_6x6_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc8x5Rgba             */ {GL_COMPRESSED_RGBA_ASTC_8x5_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc8x6Rgba             */ {GL_COMPRESSED_RGBA_ASTC_8x6_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc8x8Rgba             */ {GL_COMPRESSED_RGBA_ASTC_8x8_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc10x5Rgba            */ {GL_COMPRESSED_RGBA_ASTC_10x5_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc10x6Rgba            */ {GL_COMPRESSED_RGBA_ASTC_10x6_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc10x8Rgba            */ {GL_COMPRESSED_RGBA_ASTC_10x8_KHR, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kAstc10x10Rgba           */ {GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc12x10Rgba           */ {GL_COMPRESSED_RGBA_ASTC_12x10_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc12x12Rgba           */ {GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc4x4Srgba            */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc5x4Srgba            */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc5x5Srgba            */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc6x5Srgba            */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc6x6Srgba            */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc8x5Srgba            */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc8x6Srgba            */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc8x8Srgba            */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc10x5Srgba           */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc10x6Srgba           */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc10x8Srgba           */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc10x10Srgba          */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc12x10Srgba          */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kAstc12x12Srgba          */ {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR,
                                      GL_RGBA, GL_UNSIGNED_BYTE},
      /* kDxt1                    */ {GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
                                      GL_RGB,
                                      GL_UNSIGNED_BYTE},
      /* kDxt1Rgba                */ {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
                                      GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kDxt5                    */ {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                                      GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kEtc1                    */ {GL_ETC1_RGB8_OES, GL_RGB,
                                      GL_UNSIGNED_BYTE},
      /* kEtc2Rgb                 */ {GL_COMPRESSED_RGB8_ETC2, GL_RGB,
                                      GL_UNSIGNED_BYTE},
      /* kEtc2Rgba                */ {GL_COMPRESSED_RGBA8_ETC2_EAC, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kEtc2Rgba1               */
      {GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_RGBA, GL_UNSIGNED_BYTE},
      /* kPvrtc1Rgb2              */ {GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,
                                      GL_RGB,
                                      GL_UNSIGNED_BYTE},
      /* kPvrtc1Rgb4              */ {GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG,
                                      GL_RGB,
                                      GL_UNSIGNED_BYTE},
      /* kPvrtc1Rgba2             */ {GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,
                                      GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kPvrtc1Rgba4             */ {GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,
                                      GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kSrgb8                   */ {GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE},
      /* kSrgba8                  */ {GL_SRGB8_ALPHA8, GL_RGBA,
                                      GL_UNSIGNED_BYTE},
      /* kRgb11f_11f_10f_Rev      */ {GL_R11F_G11F_B10F, GL_RGB,
                                      GL_UNSIGNED_INT_10F_11F_11F_REV},
      /* kRgb11f_11f_10f_RevFloat */ {GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT},
      /* kRgb11f_11f_10f_RevHalf  */ {GL_R11F_G11F_B10F, GL_RGB, GL_HALF_FLOAT},
      /* kRgb565Byte              */ {GL_RGB565, GL_RGB, GL_UNSIGNED_BYTE},
      /* kRgb565Short             */ {GL_RGB565, GL_RGB,
                                      GL_UNSIGNED_SHORT_5_6_5},
      /* kRgb5a1Byte              */ {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_BYTE},
      /* kRgb5a1Short             */ {GL_RGB5_A1, GL_RGBA,
                                      GL_UNSIGNED_SHORT_5_5_5_1},
      /* kRgb5a1Int               */ {GL_RGB5_A1, GL_RGBA,
                                      GL_UNSIGNED_INT_2_10_10_10_REV},
      /* kRgb9e5Float             */ {GL_RGB9_E5, GL_RGB, GL_FLOAT},
      /* kRgb9e5Half              */ {GL_RGB9_E5, GL_RGB, GL_HALF_FLOAT},
      /* kRgb9e5RevInt            */ {GL_RGB9_E5, GL_RGB,
                                      GL_UNSIGNED_INT_5_9_9_9_REV},
      /* kRgba4Byte               */ {GL_RGBA4, GL_RGBA, GL_UNSIGNED_BYTE},
      /* kRgba4Short              */ {GL_RGBA4, GL_RGBA,
                                      GL_UNSIGNED_SHORT_4_4_4_4}};
  // Note EglImage doesn't have a table entry here, hence kNumFormats - 1.
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kPixelFormats) == kNumFormats - 1,
                    "Missing entries in kPixelFormats; it must define an entry "
                    "for every entry of Image::Format, except kInvalid, which "
                    "must be last, immediately preceded by kEglImage.");
  static const PixelFormat kInvalidPixelFormat = {0, 0, 0};

  return (format == kInvalid || format == kEglImage) ? kInvalidPixelFormat
                                                     : kPixelFormats[format];
}

const char* Image::GetFormatString(Format format) {
  static const char* kStrings[] = {
      "Alpha",
      "Luminance",
      "LuminanceAlpha",
      "Rgb888",
      "Rgba8888",
      "Rgb565",
      "Rgba4444",
      "Rgba5551",
      "RgbaFloat",
      "R8",
      "RSigned8",
      "R8i",
      "R8ui",
      "R16fFloat",
      "R16fHalf",
      "R16i",
      "R16ui",
      "R32f",
      "R32i",
      "R32ui",
      "Rg8",
      "RgSigned8",
      "Rg8i",
      "Rg8ui",
      "Rg16fFloat",
      "Rg16fHalf",
      "Rg16i",
      "Rg16ui",
      "Rg32f",
      "Rg32i",
      "Rg32ui",
      "Rgb8",
      "RgbSigned8",
      "Rgb8i",
      "Rgb8ui",
      "Rgb16fFloat",
      "Rgb16fHalf",
      "Rgb16i",
      "Rgb16ui",
      "Rgb32f",
      "Rgb32i",
      "Rgb32ui",
      "Rgba8",
      "RgbaSigned8",
      "Rgba8i",
      "Rgba8ui",
      "Rgb10a2",
      "Rgb10a2ui",
      "Rgba16fFloat",
      "Rgba16fHalf",
      "Rgba16i",
      "Rgba16ui",
      "Rgba32f",
      "Rgba32i",
      "Rgba32ui",
      "RenderbufferDepth16",
      "RenderbufferDepth24",
      "RenderbufferDepth32f",
      "RenderbufferDepth24Stencil8",
      "RenderbufferDepth32fStencil8",
      "TextureDepth16Int",
      "TextureDepth16Short",
      "TextureDepth24",
      "TextureDepth24Stencil8",
      "TextureDepth32f",
      "TextureDepth32fStencil8",
      "Stencil8",
      "Astc4x4Rgba",
      "Astc5x4Rgba",
      "Astc5x5Rgba",
      "Astc6x5Rgba",
      "Astc6x6Rgba",
      "Astc8x5Rgba",
      "Astc8x6Rgba",
      "Astc8x8Rgba",
      "Astc10x5Rgba",
      "Astc10x6Rgba",
      "Astc10x8Rgba",
      "Astc10x10Rgba",
      "Astc12x10Rgba",
      "Astc12x12Rgba",
      "Astc4x4Srgba",
      "Astc5x4Srgba",
      "Astc5x5Srgba",
      "Astc6x5Srgba",
      "Astc6x6Srgba",
      "Astc8x5Srgba",
      "Astc8x6Srgba",
      "Astc8x8Srgba",
      "Astc10x5Srgba",
      "Astc10x6Srgba",
      "Astc10x8Srgba",
      "Astc10x10Srgba",
      "Astc12x10Srgba",
      "Astc12x12Srgba",
      "Dxt1",
      "Dxt1Rgba",
      "Dxt5",
      "Etc1",
      "Etc2Rgb",
      "Etc2Rgba",
      "Etc2Rgba1",
      "Pvrtc1Rgb2",
      "Pvrtc1Rgb4",
      "Pvrtc1Rgba2",
      "Pvrtc1Rgba4",
      "Srgb8",
      "Srgba8",
      "Rgb11f_11f_10f_Rev",
      "Rgb11f_11f_10f_RevFloat",
      "Rgb11f_11f_10f_RevHalf",
      "Rgb565Byte",
      "Rgb565Short",
      "Rgb5a1Byte",
      "Rgb5a1Short",
      "Rgb5a1Int",
      "Rgb9e5Float",
      "Rgb9e5Half",
      "Rgb9e5RevInt",
      "Rgba4Byte",
      "Rgba4Short",
      "EGLImage"
  };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kStrings) == kNumFormats,
                    "Missing entries in kStrings; it must define an entry "
                    "for every entry of Image::Format, except kInvalid, which "
                    "must be last.");
  return static_cast<uint32>(format) >= kNumFormats ? "<UNKNOWN>"
                                                    : kStrings[format];
}

int Image::GetNumComponentsForFormat(Format format) {
  switch (format) {
    case kAlpha:
    case kRenderbufferDepth16:
    case kRenderbufferDepth24:
    case kRenderbufferDepth32f:
    case kTextureDepth16Int:
    case kTextureDepth16Short:
    case kTextureDepth24:
    case kTextureDepth32f:
    case kStencil8:
    case kLuminance:
    case kR8:
    case kRSigned8:
    case kR8i:
    case kR8ui:
    case kR16fFloat:
    case kR16fHalf:
    case kR16i:
    case kR16ui:
    case kR32f:
    case kR32i:
    case kR32ui:
      return 1;

    case kRenderbufferDepth24Stencil8:
    case kRenderbufferDepth32fStencil8:
    case kTextureDepth24Stencil8:
    case kTextureDepth32fStencil8:
    case kLuminanceAlpha:
    case kRg8:
    case kRgSigned8:
    case kRg8i:
    case kRg8ui:
    case kRg16fFloat:
    case kRg16fHalf:
    case kRg16i:
    case kRg16ui:
    case kRg32f:
    case kRg32i:
    case kRg32ui:
      return 2;

    case kDxt1:
    case kEtc1:
    case kEtc2Rgb:
    case kPvrtc1Rgb2:
    case kPvrtc1Rgb4:
    case kRgb565:
    case kRgb888:
    case kRgb8:
    case kRgbSigned8:
    case kRgb8i:
    case kRgb8ui:
    case kRgb16fFloat:
    case kRgb16fHalf:
    case kRgb16i:
    case kRgb16ui:
    case kRgb32f:
    case kRgb32i:
    case kRgb32ui:
    case kRgb11f_11f_10f_Rev:
    case kRgb11f_11f_10f_RevFloat:
    case kRgb11f_11f_10f_RevHalf:
    case kRgb565Byte:
    case kRgb565Short:
    case kRgb9e5Float:
    case kRgb9e5Half:
    case kRgb9e5RevInt:
    case kSrgb8:
      return 3;

    case kAstc4x4Rgba:
    case kAstc5x4Rgba:
    case kAstc5x5Rgba:
    case kAstc6x5Rgba:
    case kAstc6x6Rgba:
    case kAstc8x5Rgba:
    case kAstc8x6Rgba:
    case kAstc8x8Rgba:
    case kAstc10x5Rgba:
    case kAstc10x6Rgba:
    case kAstc10x8Rgba:
    case kAstc10x10Rgba:
    case kAstc12x10Rgba:
    case kAstc12x12Rgba:
    case kAstc4x4Srgba:
    case kAstc5x4Srgba:
    case kAstc5x5Srgba:
    case kAstc6x5Srgba:
    case kAstc6x6Srgba:
    case kAstc8x5Srgba:
    case kAstc8x6Srgba:
    case kAstc8x8Srgba:
    case kAstc10x5Srgba:
    case kAstc10x6Srgba:
    case kAstc10x8Srgba:
    case kAstc10x10Srgba:
    case kAstc12x10Srgba:
    case kAstc12x12Srgba:
    case kDxt1Rgba:
    case kDxt5:
    case kEtc2Rgba:
    case kEtc2Rgba1:
    case kPvrtc1Rgba2:
    case kPvrtc1Rgba4:
    case kRgb10a2:
    case kRgb10a2ui:
    case kRgba4444:
    case kRgba5551:
    case kRgba8888:
    case kRgba8:
    case kRgbaSigned8:
    case kRgba8i:
    case kRgba8ui:
    case kRgba16fFloat:
    case kRgba16fHalf:
    case kRgba16i:
    case kRgba16ui:
    case kRgba32f:
    case kRgba32i:
    case kRgba32ui:
    case kRgb5a1Byte:
    case kRgb5a1Short:
    case kRgb5a1Int:
    case kRgba4Byte:
    case kRgba4Short:
    case kSrgba8:
    case kRgbaFloat:
      return 4;

    case kEglImage:
    case kInvalid:
      return 0;

    default:
      DCHECK(false) << "Unknown format";
      return 0;
  }
}

namespace {

// Converts image dimensions and block size to the required byte size for an
// ASTC encoded image.
size_t AstcTotalBytesFromImageSize(int width, int height, int footprint_width,
                                   int footprint_height) {
  // Each MxN block of pixels requires 16 bytes. Round up image dimensions to
  // block size.
  return 16 * ((width + footprint_width - 1) / footprint_width) *
         ((height + footprint_height - 1) / footprint_height);
}

}  // namespace

size_t Image::ComputeDataSize(Format format, uint32 width, uint32 height) {
  switch (format) {
    case kAlpha:
    case kLuminance:
    case kStencil8:
    case kR8:
    case kRSigned8:
    case kR8i:
    case kR8ui:
      return width * height;

    case kDxt1:
    case kDxt1Rgba:
    case kEtc1:
    case kEtc2Rgb:
    case kEtc2Rgba1:
      // Each 4x4 block of pixels requires 8 bytes.
      return 8 * ((width + 3) / 4) * ((height + 3) / 4);

    case kEtc2Rgba:
    case kDxt5:
      // Each 4x4 block of pixels requires 16 bytes.
      return 16 * ((width + 3) / 4) * ((height + 3) / 4);

    case kRenderbufferDepth16:
    case kTextureDepth16Int:
    case kTextureDepth16Short:
    case kLuminanceAlpha:
    case kRgb565:
    case kRgba4444:
    case kRgba5551:
    case kR16fFloat:
    case kR16fHalf:
    case kR16i:
    case kR16ui:
    case kRg8:
    case kRgSigned8:
    case kRg8i:
    case kRg8ui:
    case kRgb565Byte:
    case kRgb565Short:
    case kRgb5a1Byte:
    case kRgb5a1Int:
    case kRgb5a1Short:
    case kRgba4Byte:
    case kRgba4Short:
      return 2 * width * height;

    case kPvrtc1Rgb2:
    case kPvrtc1Rgba2:
      return width * height / 4;

    case kPvrtc1Rgb4:
    case kPvrtc1Rgba4:
      return width * height / 2;

    case kAstc4x4Rgba:
    case kAstc4x4Srgba:
      return AstcTotalBytesFromImageSize(width, height, 4, 4);
    case kAstc5x4Rgba:
    case kAstc5x4Srgba:
      return AstcTotalBytesFromImageSize(width, height, 5, 4);
    case kAstc5x5Rgba:
    case kAstc5x5Srgba:
      return AstcTotalBytesFromImageSize(width, height, 5, 5);
    case kAstc6x5Rgba:
    case kAstc6x5Srgba:
      return AstcTotalBytesFromImageSize(width, height, 6, 5);
    case kAstc6x6Rgba:
    case kAstc6x6Srgba:
      return AstcTotalBytesFromImageSize(width, height, 6, 6);
    case kAstc8x5Rgba:
    case kAstc8x5Srgba:
      return AstcTotalBytesFromImageSize(width, height, 8, 5);
    case kAstc8x6Rgba:
    case kAstc8x6Srgba:
      return AstcTotalBytesFromImageSize(width, height, 8, 6);
    case kAstc8x8Rgba:
    case kAstc8x8Srgba:
      return AstcTotalBytesFromImageSize(width, height, 8, 8);
    case kAstc10x5Rgba:
    case kAstc10x5Srgba:
      return AstcTotalBytesFromImageSize(width, height, 10, 5);
    case kAstc10x6Rgba:
    case kAstc10x6Srgba:
      return AstcTotalBytesFromImageSize(width, height, 10, 6);
    case kAstc10x8Rgba:
    case kAstc10x8Srgba:
      return AstcTotalBytesFromImageSize(width, height, 10, 8);
    case kAstc10x10Rgba:
    case kAstc10x10Srgba:
      return AstcTotalBytesFromImageSize(width, height, 10, 10);
    case kAstc12x10Rgba:
    case kAstc12x10Srgba:
      return AstcTotalBytesFromImageSize(width, height, 12, 10);
    case kAstc12x12Rgba:
    case kAstc12x12Srgba:
      return AstcTotalBytesFromImageSize(width, height, 12, 12);

    case kRgb888:
    case kRgb8:
    case kRgbSigned8:
    case kRgb8i:
    case kRgb8ui:
    case kSrgb8:
      return 3 * width * height;

    case kRgba8888:
    case kR32f:
    case kR32i:
    case kR32ui:
    case kRg16fHalf:
    case kRg16fFloat:
    case kRg16i:
    case kRg16ui:
    case kRgb10a2:
    case kRgb10a2ui:
    case kRgba8:
    case kRgbaSigned8:
    case kRgba8i:
    case kRgba8ui:
    case kRenderbufferDepth24:
    case kRenderbufferDepth24Stencil8:
    case kRenderbufferDepth32f:
    case kTextureDepth24:
    case kTextureDepth24Stencil8:
    case kTextureDepth32f:
    case kRgb11f_11f_10f_Rev:
    case kRgb11f_11f_10f_RevFloat:
    case kRgb11f_11f_10f_RevHalf:
    case kRgb9e5Float:
    case kRgb9e5Half:
    case kRgb9e5RevInt:
    case kSrgba8:
      return 4 * width * height;

    case kRgb16fFloat:
    case kRgb16fHalf:
    case kRgb16i:
    case kRgb16ui:
      return 6 * width * height;

    case kRg32f:
    case kRg32i:
    case kRg32ui:
    case kRgba16fFloat:
    case kRgba16fHalf:
    case kRgba16i:
    case kRgba16ui:
    case kRenderbufferDepth32fStencil8:
    case kTextureDepth32fStencil8:
      return 8 * width * height;

    case kRgb32f:
    case kRgb32i:
    case kRgb32ui:
      return 12 * width * height;

    case kRgba32f:
    case kRgba32i:
    case kRgba32ui:
    case kRgbaFloat:
      return 16 * width * height;

    case kEglImage:
    case kInvalid:
      return 0U;

    default:
      DCHECK(false) << "Unknown format";
      return 0U;
  }
}

size_t Image::ComputeDataSize(Format format, uint32 width, uint32 height,
                              uint32 depth) {
  return ComputeDataSize(format, width, height) * depth;
}

void Image::SetData(Type type, Dimensions dims, Format format, uint32 width,
                    uint32 height, uint32 depth,
                    const base::DataContainerPtr& data) {
  type_ = type;
  dims_ = dims;
  format_ = format;
  width_ = width;
  height_ = height;
  depth_ = depth;
  if (base::DataContainer* old_data = data_.Get())
    old_data->RemoveReceiver(this);
  data_ = data;
  if (base::DataContainer* new_data = data_.Get())
    new_data->AddReceiver(this);
  if (data_.Get() && data_->GetData())
    data_size_ = ComputeDataSize(format, width, height, depth);
  else
    data_size_ = 0;
  Notify();
}

}  // namespace gfx

namespace base {

using gfx::Image;

// Specialize for Image::Dimension.
template <> ION_API
const EnumHelper::EnumData<Image::Dimensions> EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {0, 0};
  static const char* kStrings[] = {"2", "3"};
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<Image::Dimensions>(
      base::IndexMap<Image::Dimensions, GLenum>(kValues,
                                                ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

// Specialize for Image::Type.
template <> ION_API
const EnumHelper::EnumData<Image::Type> EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {0, 0, 0, 0};
  static const char* kStrings[] = {"Array", "Dense", "EGLImage",
                                   "External EGLImage"};
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<Image::Type>(
      base::IndexMap<Image::Type, GLenum>(kValues, ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

}  // namespace base
}  // namespace ion
