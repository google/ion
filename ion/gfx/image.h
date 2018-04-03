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

#ifndef ION_GFX_IMAGE_H_
#define ION_GFX_IMAGE_H_

#include "base/integral_types.h"
#include "ion/base/datacontainer.h"
#include "ion/base/notifier.h"

namespace ion {
namespace gfx {

// Convenience typedef for shared pointer to an Image.
class Image;
using ImagePtr = base::SharedPtr<Image>;

// An Image represents 2D image data that can be used in a texture supplied to
// a shader. The image data is stored in a DataContainer to provide flexibility
// regarding storage lifetime.
class ION_API Image : public base::Notifier {
 public:
  // Supported image formats.
  enum Format {
    // "Unsized" formats.
    kAlpha,             // Single-component alpha image, 8 bits per pixel.
    kLuminance,         // Single-component luminance image, 8 bits per pixel.
    kLuminanceAlpha,    // Two-component luminance+alpha image, 8 bits each.
    kRgb888,            // RGB color image, 8 bits each.
    kRgba8888,          // RGBA color image, 8 bits each.
    kRgb565,            // RGB color image, 5 bits red and blue, 6 bits green.
    kRgba4444,          // RGBA color+alpha image, 4 bits each.
    kRgba5551,          // RGBA color+alpha image, 5 bits per color, 1 bit
    kRgbaFloat,         // RGBA 32-bit floating point image (for OpenGL ES 2.0
                        //   compatibility, where both format and
                        //   internal_format are GL_RGBA).

    // Single-component red channel images.
    kR8,                // Float image, 8-bits red.
    kRSigned8,          // Float image, 8-bits red, signed data.
    kR8i,               // Integer image, 8-bits red.
    kR8ui,              // Integer image, 8-bits red.
    kR16fFloat,         // Float image, 16-bits red, 32-bit float data.
    kR16fHalf,          // Float image, 16-bits red, 16-bit half float data.
    kR16i,              // Integer image, 16-bits red.
    kR16ui,             // Integer image, 16-bits red.
    kR32f,              // Float image, 32-bits red.
    kR32i,              // Integer image, 32-bits red.
    kR32ui,             // Integer image, 32-bits red.

    // Two-component red-green images.
    kRg8,               // Float image, 8-bits each component.
    kRgSigned8,         // Float image, 8-bits each component, signed data.
    kRg8i,              // Integer image, 8-bits each component.
    kRg8ui,             // Integer image, 8-bits each component.
    kRg16fFloat,        // Float image, 16-bits each component, float data.
    kRg16fHalf,         // Float image, 16-bits each component, half float data.
    kRg16i,             // Integer image, 16-bits each component.
    kRg16ui,            // Integer image, 16-bits each component.
    kRg32f,             // Float image, 32-bits each component.
    kRg32i,             // Integer image, 32-bits each component.
    kRg32ui,            // Integer image, 32-bits each component.

    // Three channel RGB images.
    kRgb8,              // Float image, 8-bits each component.
    kRgbSigned8,        // Float image, 8-bits each component, signed data.
    kRgb8i,             // Integer image, 8-bits each component, signed data.
    kRgb8ui,            // Integer image, 8-bits each component.
    kRgb16fFloat,       // Float image, 16-bits each component, float data.
    kRgb16fHalf,        // Float image, 16-bits each component, half float data.
    kRgb16i,            // Integer image, 16-bits each component, signed data.
    kRgb16ui,           // Integer image, 16-bits each component.
    kRgb32f,            // float image, 32-bits each component.
    kRgb32i,            // Integer image, 32-bits each component, signed data.
    kRgb32ui,           // Integer image, 32-bits each component.

    // Four channel RGBA images.
    kRgba8,             // Float image, 8-bits each component.
    kRgbaSigned8,       // Float image, 8-bits each component, signed data.
    kRgba8i,            // Integer image, 8-bits each component, signed data.
    kRgba8ui,           // Integer image, 8-bits each component.
    kRgb10a2,           // Float image, 10 bits per color, 2 bits alpha.
    kRgb10a2ui,         // Integer image, 10 bits per color, 2 bits alpha.
    kRgba16fFloat,      // Float image, 16-bits each component, float data.
    kRgba16fHalf,       // Float image, 16-bits each component, half float data.
    kRgba16i,           // Integer image, 16-bits each component, signed data.
    kRgba16ui,          // Integer image, 16-bits each component.
    kRgba32f,           // Float image, 32-bits each component.
    kRgba32i,           // Integer image, 32-bits each component, signed data.
    kRgba32ui,          // Integer image, 32-bits each component.

    // Depth and depth/stencil renderbuffers.
    kRenderbufferDepth16,           // Depth rb, 16-bit depth.
    kRenderbufferDepth24,           // Depth rb, 24-bit depth.
    kRenderbufferDepth32f,          // Depth rb, 32-bit float depth.
    kRenderbufferDepth24Stencil8,   // Depth stencil rb, 24-bit depth, 8-bit
                                    //   stencil.
    kRenderbufferDepth32fStencil8,  // Depth stencil rb, 32-bit float depth,
                                    //    8-bit stencil.

    // Depth textures.
    kTextureDepth16Int,        // Depth image, 16-bit depth, uint32 data.
    kTextureDepth16Short,      // Depth image, 16-bit depth, uint16 data.
    kTextureDepth24,           // Depth image, 24-bit depth, uint32 data.
    kTextureDepth24Stencil8,   // Depth stencil image, 24-bit depth, 8-bit
                               //   stencil.
    kTextureDepth32f,          // Depth image, 32-bit depth, float data.
    kTextureDepth32fStencil8,  // Depth stencil rb, 32-bit float depth,
                               //    8-bit stencil.

    // Stencil images.
    kStencil8,          // Stencil image, 8-bits

    // Compressed images. Note the alphabetical order.
    // N.B. Only the ASTC block size and sRGB option are fundamentally defined
    // at the image level. The encoded blocks may be any ASTC encoding (e.g.
    // LDR or HDR); this depends on the color mode of the block, not the GL
    // texture format.
    kAstc4x4Rgba,    // ASTC, 4x4 block, RGBA.
    kAstc5x4Rgba,    // ASTC, 5x4 block, RGBA.
    kAstc5x5Rgba,    // ASTC, 5x5 block, RGBA.
    kAstc6x5Rgba,    // ASTC, 6x5 block, RGBA.
    kAstc6x6Rgba,    // ASTC, 6x6 block, RGBA.
    kAstc8x5Rgba,    // ASTC, 8x5 block, RGBA.
    kAstc8x6Rgba,    // ASTC, 8x6 block, RGBA.
    kAstc8x8Rgba,    // ASTC, 8x8 block, RGBA.
    kAstc10x5Rgba,   // ASTC, 10x5 block, RGBA.
    kAstc10x6Rgba,   // ASTC, 10x6 block, RGBA.
    kAstc10x8Rgba,   // ASTC, 10x8 block, RGBA.
    kAstc10x10Rgba,  // ASTC, 10x10 block, RGBA.
    kAstc12x10Rgba,  // ASTC, 12x10 block, RGBA.
    kAstc12x12Rgba,  // ASTC, 12x12 block, RGBA.
    kAstc4x4Srgba,    // ASTC, 4x4 block, sRGBA.
    kAstc5x4Srgba,    // ASTC, 5x4 block, sRGBA.
    kAstc5x5Srgba,    // ASTC, 5x5 block, sRGBA.
    kAstc6x5Srgba,    // ASTC, 6x5 block, sRGBA.
    kAstc6x6Srgba,    // ASTC, 6x6 block, sRGBA.
    kAstc8x5Srgba,    // ASTC, 8x5 block, sRGBA.
    kAstc8x6Srgba,    // ASTC, 8x6 block, sRGBA.
    kAstc8x8Srgba,    // ASTC, 8x8 block, sRGBA.
    kAstc10x5Srgba,   // ASTC, 10x5 block, sRGBA.
    kAstc10x6Srgba,   // ASTC, 10x6 block, sRGBA.
    kAstc10x8Srgba,   // ASTC, 10x8 block, sRGBA.
    kAstc10x10Srgba,  // ASTC, 10x10 block, sRGBA.
    kAstc12x10Srgba,  // ASTC, 12x10 block, sRGBA.
    kAstc12x12Srgba,  // ASTC, 12x12 block, sRGBA.
    kDxt1,              // DXT1-compressed image (no alpha).
    kDxt1Rgba,          // DXT1-compressed image (1 bit alpha).
    kDxt5,              // DXT5-compressed image (with alpha).
    kEtc1,              // ETC1-compressed image (no alpha).
    kEtc2Rgb,           // ETC2-compressed image (no alpha).
    kEtc2Rgba,          // ETC2-compressed image (with full alpha).
    kEtc2Rgba1,         // ETC2-compressed image (with 1-bit alpha).
    kPvrtc1Rgb2,        // PVRTC1-compressed image (2 bits per pixel, no alpha).
    kPvrtc1Rgb4,        // PVRTC1-compressed image (4 bits per pixel, no alpha).
    kPvrtc1Rgba2,       // PVRTC1-compressed image (2 bits per pixel, with
                        //   alpha).
    kPvrtc1Rgba4,       // PVRTC1-compressed image (4 bits per pixel, with
                        //   alpha).

    // SRGB(A) images.
    kSrgb8,             // Float image, 8-bits each component.
    kSrgba8,            // Float image, 8-bits each component.

    // Packed sized images.
    kRgb11f_11f_10f_Rev,       // RGB color image, 10 bits red and green, 11
                               //   bits blue, packed 10f,11f,11f uint32 data.
    kRgb11f_11f_10f_RevFloat,  // RGB color image, 10 bits red and green, 11
                               //   bits blue, packed float data.
    kRgb11f_11f_10f_RevHalf,   // RGB color image, 10 bits red and green, 11
                               //   bits blue, packed half float data.
    kRgb565Byte,               // RGB color image, 5 bits red and blue, 6 bits
                               //  green, uint8 data.
    kRgb565Short,              // RGB color image, 5 bits red and blue, 6 bits
                               //  green, packed 565 uint16 data.
    kRgb5a1Byte,               // RGBA color+alpha image, 5 bits per color, 1
                               //   bit alpha, uint8 data.
    kRgb5a1Short,              // RGBA color+alpha image, 5 bits per color, 1
                               //   bit alpha, packed 5551 uint16 data.
    kRgb5a1Int,                // RGBA color+alpha image, 5 bits per color, 1
                               //   bit alpha, packed 2,10,10,10 uint32 data.
    kRgb9e5Float,              // RGB color+alpha image, 9 bits per color, 5
                               //   bits exponent, float data.
    kRgb9e5Half,               // RGB color+alpha image, 9 bits per color, 5
                               //   bits exponent, half float data.
    kRgb9e5RevInt,             // RGB color+alpha image, 9 bits per color, 5
                               //   bits exponent, packed 5999 uint32 data.
    kRgba4Byte,                // RGBA color+alpha image, 4 bits each, uint8
                               //   data.
    kRgba4Short,               // RGBA color+alpha image, 4 bits each, packed
                               //   4444 uint16 data.

    kEglImage,                 // A texture backed by an EGLImage, which may
                               //   have arbitrary format.

    kInvalid                   // An invalid format.
  };

  // The kind of Image, either array or dense, or an EGL type. An array image is
  // a series of planes that are not interpolated, while a dense image uses
  // filtering across the last dimension. An EGL image is one supplied by the
  // EGL library via the OES_EGL_image extension, while an external EGL image is
  // one created via the OES_EGL_image_external extension.
  enum Type {
    kArray,
    kDense,
    kEgl,
    kExternalEgl,
  };

  // The number of dimensions in the image. Note that an Nd array texture has N
  // + 1 dimensions.
  enum Dimensions {
    k2d,
    k3d
  };

  // Struct representing the GL types for a particular Format (see above).
  struct PixelFormat {
    uint32 internal_format;
    uint32 format;
    uint32 type;
  };

  static const uint32 kNumFormats;

  // The default constructor creates an empty (0x0) dense 2D image with format
  // kRgb888.
  Image();

  // Sets the image to the given size and format and using the data in the given
  // DataContainer, which is assumed to be the correct size.
  void Set(Format format, uint32 width, uint32 height,
           const base::DataContainerPtr& data);
  // Overload that creates a 3D texture.
  void Set(Format format, uint32 width, uint32 height, uint32 depth,
           const base::DataContainerPtr& data);

  // Similar to Set(), but creates an array of 1D textures.
  void SetArray(Format format, uint32 width, uint32 num_planes,
                const base::DataContainerPtr& data);
  // Similar to Set(), but creates an array of 2D textures.
  void SetArray(Format format, uint32 width, uint32 height, uint32 num_planes,
                const base::DataContainerPtr& data);

  // Sets the image to be of EGLImage type. The width, height, format, and image
  // data are all determined opaquely based on the passed image. The passed data
  // container may just wrap a void pointer. If the passed data is NULL, then it
  // must be supplied via EGL outside of Ion after retrieving the texture ID
  // from a Renderer.
  void SetEglImage(const base::DataContainerPtr& image);

  // Similar to SetEglImage, but sets the GL target type to GL_TEXTURE_ARRAY
  // rather than GL_TEXTURE_2D.
  void SetEglImageArray(const base::DataContainerPtr& image);

  // Sets the image to be of external EGLImage type. The width, height, format,
  // and image data are all determined opaquely based on the passed external
  // image. The passed data container may just wrap a void pointer. If the
  // passed data is NULL, then it must be supplied outside of Ion after
  // retrieving the texture ID from a Renderer.
  void SetExternalEglImage(const base::DataContainerPtr& external_image);

  Format GetFormat() const { return format_; }
  Type GetType() const { return type_; }
  Dimensions GetDimensions() const { return dims_; }
  uint32 GetWidth() const { return width_; }
  uint32 GetHeight() const { return height_; }
  uint32 GetDepth() const { return depth_; }
  size_t GetDataSize() const { return data_size_; }
  const base::DataContainerPtr& GetData() const { return data_; }

  // Returns true if the image format is one of the compressed types.
  bool IsCompressed() const { return IsCompressedFormat(format_); }

  // Convenience function that returns a string representing the name of a
  // given Format.
  static const char* GetFormatString(Format format);

  // Convenience function that returns a PixelFormat given a Format.
  static const PixelFormat& GetPixelFormat(Format format);

  // Convenience function that returns the number of components for a given
  // format.
  static int GetNumComponentsForFormat(Format format);

  // Convenience function that returns true if the given format represents
  // compressed image data.
  static bool IsCompressedFormat(Format format);
  // Returns whether the specified format has 8 bits per channel.
  // E.g. kRgba8888, kLuminanceAlpha, kRgb8ui, etc.
  // It does *not* include compressed formats even if they decompress to
  // something that is 8 bits per channel.
  static bool Is8BitPerChannelFormat(Format format);

  // Convenience functions that return the correct data size in bytes of an
  // image having the given format and dimensions.
  static size_t ComputeDataSize(Format format, uint32 width, uint32 height);
  static size_t ComputeDataSize(Format format, uint32 width, uint32 height,
                                uint32 depth);

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~Image() override;

 private:
  // Pass notifications on to an owning ResourceHolder, e.g., a Texture.
  void OnNotify(const base::Notifier* notifier) override {
    if (notifier == data_.Get())
      Notify();
  }

  // Sets the internal state of the Image.
  void SetData(Type type, Dimensions dims, Format format, uint32 width,
               uint32 height, uint32 depth, const base::DataContainerPtr& data);

  Format format_;
  uint32 width_;
  uint32 height_;
  uint32 depth_;
  size_t data_size_;
  Type type_;
  Dimensions dims_;
  base::DataContainerPtr data_;
};

inline bool Image::IsCompressedFormat(Image::Format format) {
  return format == kDxt1 || format == kDxt1Rgba || format == kDxt5 ||
         format == kEtc1 || format == kEtc2Rgb || format == kEtc2Rgba ||
         format == kEtc2Rgba1 || format == kPvrtc1Rgb2 ||
         format == kPvrtc1Rgb4 || format == kPvrtc1Rgba2 ||
         format == kPvrtc1Rgba4 ||
         (format >= kAstc4x4Rgba && format <= kAstc12x12Srgba);
}

inline bool Image::Is8BitPerChannelFormat(Image::Format format) {
  // For most formats we can determine they are 8-bits per channel by looking
  // at the number of channels vs the size of one pixel.  But the following
  // break that logic.
  if (format == Image::kRgb5a1Int ||
      format == Image::kRgb10a2 ||
      format == Image::kRgb10a2ui ||
      format == Image::kEglImage ||
      format == Image::kInvalid) {
    return false;
  }
  size_t bytes_per_pixel = Image::ComputeDataSize(format, 1, 1);
  size_t channels =
      static_cast<size_t>(Image::GetNumComponentsForFormat(format));
  return channels == bytes_per_pixel;
}

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_IMAGE_H_
