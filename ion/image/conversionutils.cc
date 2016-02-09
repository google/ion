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

#include "ion/base/logging.h"  // Ensures Ion logging code is used.

#include "base/port.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/datacontainer.h"
#include "ion/math/range.h"
#include "third_party/image_compression/image_compression/public/compressed_image.h"
#include "third_party/image_compression/image_compression/public/dxtc_compressor.h"
#include "third_party/image_compression/image_compression/public/etc_compressor.h"
#include "third_party/image_compression/image_compression/public/pvrtc_compressor.h"
#define LODEPNG_NO_COMPILE_ENCODER
#define LODEPNG_NO_COMPILE_DISK
#define LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
#define LODEPNG_NO_COMPILE_ERROR_TEXT
#define LODEPNG_NO_COMPILE_CPP
#include "third_party/lodepng/lodepng.h"
#undef LODEPNG_NO_COMPILE_ENCODER
#undef LODEPNG_NO_COMPILE_DISK
#undef LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
#undef LODEPNG_NO_COMPILE_ERROR_TEXT
#undef LODEPNG_NO_COMPILE_CPP

#include "third_party/stblib/stb_image.h"
#include "third_party/stblib/stb_image_write.h"

// This function is not declared as extern in the header, but it is accessible.
extern "C" unsigned char* stbi_write_png_to_mem(
    unsigned char*, int, int, int, int, int*);

namespace ion {
namespace image {

using gfx::Image;
using gfx::ImagePtr;
using math::Point2f;
using math::Range2f;

namespace {

// Header size of "ION raw" image format. See doc of
// ConvertFromExternalImageData() for detaied specs of this format.
const size_t kIonRawImageHeaderSizeInBytes = 16;

//-----------------------------------------------------------------------------
//
// Basic helper functions.
//
//-----------------------------------------------------------------------------

// Returns true if an Image is not NULL and has non-NULL data.
static bool ImageHasData(const ImagePtr& image) {
  if (!image.Get())
    return false;
  const base::DataContainerPtr& data = image->GetData();
  return data.Get() != NULL && data->GetData() != NULL;
}

// Returns true if an image contains alpha information (RGBA as opposed to RGB).
static bool ImageHasAlpha(const Image& image) {
  return Image::GetNumComponentsForFormat(image.GetFormat()) == 4;
}

//-----------------------------------------------------------------------------
//
// Compression/decompression helper functions.
//
//-----------------------------------------------------------------------------

// Compresses an image using the provided Compressor. Returns a NULL Image if
// there are any problems.
static const ImagePtr CompressWithCompressor(
    const Image& image, Image::Format compressed_format,
    image_codec_compression::Compressor* compressor, bool is_wipeable,
    const base::AllocatorPtr& allocator) {
  using image_codec_compression::CompressedImage;
  const CompressedImage::Format format =
      ImageHasAlpha(image) ? CompressedImage::kRGBA : CompressedImage::kRGB;

  // Compress into a local CompressedImage.
  ImagePtr result;
  CompressedImage compressed_image;
  const uint8* uncompressed_data =
      reinterpret_cast<const uint8*>(image.GetData()->GetData());
  if (compressor->Compress(format, image.GetHeight(), image.GetWidth(), 0,
                           uncompressed_data, &compressed_image)) {
    const CompressedImage::Metadata& metadata = compressed_image.GetMetadata();
    result.Reset(new(allocator) Image);
    result->Set(compressed_format, metadata.compressed_width,
                metadata.compressed_height,
                base::DataContainer::CreateAndCopy<uint8>(
                    compressed_image.GetData(), compressed_image.GetDataSize(),
                    is_wipeable, result->GetAllocator()));
  }
  return result;
}

// Decompresses an image using the provided Compressor. Returns a NULL Image if
// there are any problems.
static const ImagePtr DecompressWithCompressor(
    const Image& image, image_codec_compression::Compressor* compressor,
    bool is_wipeable, const base::AllocatorPtr& allocator) {
  using image_codec_compression::CompressedImage;

  // Determine formats and sizes.
  CompressedImage::Format compressed_format;
  Image::Format decompressed_format;
  if (ImageHasAlpha(image)) {
    compressed_format = CompressedImage::kRGBA;
    decompressed_format = Image::kRgba8888;
  } else {
    compressed_format = CompressedImage::kRGB;
    decompressed_format = Image::kRgb888;
  }
  const uint32 width = image.GetWidth();
  const uint32 height = image.GetHeight();

  // Create a CompressedImage wrapping the image data.
  const void* image_data = image.GetData()->GetData();
  DCHECK(image_data);
  image_codec_compression::CompressedImage compressed_image(
      image.GetDataSize(),
      const_cast<uint8*>(reinterpret_cast<const uint8*>(image_data)));
  const char* compressor_name = image.GetFormat() == Image::kEtc1 ?
      "etc" : (image.GetFormat() == Image::kPvrtc1Rgba2 ? "pvrtc" : "dxtc");
  compressed_image.SetMetadata(CompressedImage::Metadata(
      compressed_format, compressor_name,
      height, width, height, width, 0));

  // Decompress into a buffer and store the results in the returned Image.
  ImagePtr result_image;
  std::vector<uint8> decompressed_data;
  if (compressor->Decompress(compressed_image, &decompressed_data)) {
    result_image.Reset(new(allocator) Image);
    result_image->Set(decompressed_format, width, height,
                      base::DataContainer::CreateAndCopy<uint8>(
                          &decompressed_data[0], decompressed_data.size(),
                          is_wipeable, result_image->GetAllocator()));
  }
  return result_image;
}

// Compresses an image to the target_format.
static const ImagePtr CompressImage(const Image& image,
                                    Image::Format target_format,
                                    bool is_wipeable,
                                    const base::AllocatorPtr& allocator) {
  if (target_format == Image::kEtc1) {
    image_codec_compression::EtcCompressor compressor;
    compressor.SetCompressionStrategy(
        image_codec_compression::EtcCompressor::kHeuristic);
    return CompressWithCompressor(image, target_format, &compressor,
                                  is_wipeable, allocator);
  } else if (target_format == Image::kPvrtc1Rgba2) {
    image_codec_compression::PvrtcCompressor compressor;
    return CompressWithCompressor(image, target_format, &compressor,
                                  is_wipeable, allocator);
  } else {
    DCHECK(target_format == Image::kDxt1 || target_format == Image::kDxt5);
    image_codec_compression::DxtcCompressor compressor;
    return CompressWithCompressor(image, target_format, &compressor,
                                  is_wipeable, allocator);
  }
}

// Decompresses an image to the appropriate format.
static const ImagePtr DecompressImage(const Image& image, bool is_wipeable,
                                      const base::AllocatorPtr& allocator) {
  // Create a Compressor instance of the correct type and decompress the image.
  const Image::Format format = image.GetFormat();
  if (format == Image::kEtc1) {
    image_codec_compression::EtcCompressor compressor;
    return DecompressWithCompressor(image, &compressor, is_wipeable, allocator);
  } else {
    DCHECK(format == Image::kDxt1 || format == Image::kDxt5);
    image_codec_compression::DxtcCompressor compressor;
    return DecompressWithCompressor(image, &compressor, is_wipeable, allocator);
  }
}

static bool PngHasTransparencyChunk(const uint8* png_data, size_t data_size) {
  const size_t kFirstChunk = 33;  // first byte of the first chunk after header.
  const unsigned char* chunk = &png_data[kFirstChunk];
  const uint8* end_ptr = png_data + data_size;

  bool has_trns_chunk = false;
  while (chunk < end_ptr) {
    if (lodepng_chunk_type_equals(chunk, "IEND")) {
      // We got to end with no tRNS chunk.
      break;
    } else if (lodepng_chunk_type_equals(chunk, "tRNS")) {
      has_trns_chunk = true;
      break;
    }
    chunk = lodepng_chunk_next_const(chunk);
  }
  return has_trns_chunk;
}

// Decodes |data| to an Image using lodepng. Supported formats: PNG. |data_size|
// is the number of bytes in |data|. Returns NULL on failure.
static const ImagePtr DataToImageLodePng(const void* data,
                                         size_t data_size,
                                         bool flip_vertically,
                                         bool is_wipeable,
                                         const base::AllocatorPtr& allocator) {
  ImagePtr image;
  uint32 width = 0, height = 0;
  const uint8* data_in = static_cast<const uint8*>(data);
  LodePNGState state;
  lodepng_state_init(&state);
  static const unsigned kLodePngSuccess = 0;
  unsigned lodepng_error_code = lodepng_inspect(
      &width, &height, &state, data_in, data_size);
  if (lodepng_error_code != kLodePngSuccess) {
    lodepng_state_cleanup(&state);
    return image;
  }
  uint8* data_out = NULL;

  LodePNGColorType colortype = state.info_png.color.colortype;
  if (colortype == LCT_PALETTE) {
    colortype = PngHasTransparencyChunk(data_in, data_size) ?
        LCT_RGBA : LCT_RGB;
  } else if (colortype == LCT_GREY || colortype == LCT_RGB) {
    // Non-paletted images can also have a single transparent color defined via
    // tRNS chunk.
    if (PngHasTransparencyChunk(data_in, data_size)) {
      colortype = (colortype == LCT_GREY) ? LCT_GREY_ALPHA : LCT_RGBA;
    }
  }
  lodepng_error_code = lodepng_decode_memory(&data_out, &width, &height,
                                             data_in, data_size, colortype, 8U);
  lodepng_state_cleanup(&state);
  if (lodepng_error_code == kLodePngSuccess) {
    Image::Format format = Image::kRgba8888;
    uint32 num_channels = 4U;
    switch (colortype) {
      case LCT_RGBA:
        format = Image::kRgba8888;
        num_channels = 4;
        break;
      case LCT_RGB:
        format = Image::kRgb888;
        num_channels = 3;
        break;
      case LCT_GREY_ALPHA:
        format = Image::kLuminanceAlpha;
        num_channels = 2;
        break;
      case LCT_GREY:
        format = Image::kLuminance;
        num_channels = 1;
        break;
      default:
        DCHECK(false) << "Unexpected PNG color type";
    }
    image.Reset(new(allocator) Image);
    image->Set(format, width, height,
               base::DataContainer::CreateAndCopy<uint8>(
                   data_out, width * height * num_channels, is_wipeable,
                   image->GetAllocator()));
    free(data_out);

    if (flip_vertically) {
      FlipImage(image);
    }
  }
  return image;
}

// Decodes |data| to an Image using stblib. Supported formats: JPEG, PNG, TGA,
// BMP, PSD, GIF, HDR, PIC. |data_size| is the number of bytes in |data|.
// Returns NULL on failure.
static const ImagePtr DataToImageStb(const void* data,
                                     size_t data_size,
                                     bool flip_vertically,
                                     bool is_wipeable,
                                     const base::AllocatorPtr& allocator) {
  ImagePtr image;
  int width, height, num_components;
  if (stbi_uc* result_data = stbi_load_from_memory(
          static_cast<const stbi_uc*>(data), static_cast<int>(data_size),
          &width, &height, &num_components, 0)) {
    // Handle Luminance, Luminance Alpha, RGB and RGBA results.
    static Image::Format formats[4] = {
      Image::kLuminance,
      Image::kLuminanceAlpha,
      Image::kRgb888,
      Image::kRgba8888,
    };
    DCHECK_GE(num_components, 1) << "Unsupported component count in image.";
    DCHECK_LE(num_components, 4) << "Unsupported component count in image.";
    image.Reset(new(allocator) Image);
    image->Set(formats[num_components - 1], width, height,
               base::DataContainer::CreateAndCopy<uint8>(
                   result_data, width * height * num_components, is_wipeable,
                   image->GetAllocator()));
    stbi_image_free(result_data);
  }

  if (flip_vertically) {
    FlipImage(image);
  }
  return image;
}

// Decodes "ION raw" |data| to an Image (see conversionutils.h for format specs)
// |data_size| is the number of bytes in |data|. Returns NULL on failure.
static const ImagePtr DataToImageIonRaw(const void* data,
                                        size_t data_size,
                                        bool flip_vertically,
                                        bool is_wipeable,
                                        const base::AllocatorPtr& allocator) {
  if (!IsIonRawImageFormat(data, data_size)) {
    return ImagePtr();  // NULL
  }

  const uint16* header_ui16 = static_cast<const uint16*>(data);
  const uint32* header_ui32 = static_cast<const uint32*>(data);

  const bool byte_swap_required = (header_ui16[2] != 1);
  const uint16 format_indicator = byte_swap_required ?
      bswap_16(header_ui16[3]) : header_ui16[3];
  const uint32 width = byte_swap_required ?
      bswap_32(header_ui32[2]) : header_ui32[2];
  const uint32 height = byte_swap_required ?
      bswap_32(header_ui32[3]) : header_ui32[3];
  const size_t num_pixels = width * height;

  Image::Format format;
  switch (format_indicator) {
    case 0:  format = Image::kRgba8888; break;
    case 1:  format = Image::kRgb565;   break;
    case 2:  format = Image::kRgba4444; break;
    case 3:  format = Image::kAlpha;    break;
    default: return ImagePtr();  // NULL
  }

  size_t num_bytes_per_pixel = Image::ComputeDataSize(format, 1, 1);
  size_t payload_size_bytes = num_pixels * num_bytes_per_pixel;
  if (payload_size_bytes == 0 ||
      data_size - kIonRawImageHeaderSizeInBytes != payload_size_bytes) {
    return ImagePtr();  // NULL
  }

  ImagePtr image;
  image.Reset(new(allocator) Image);
  const uint8* payload = reinterpret_cast<const uint8*>(&header_ui32[4]);
  image->Set(format, width, height, base::DataContainer::CreateAndCopy<uint8>(
      payload, payload_size_bytes, is_wipeable, image->GetAllocator()));

  if (byte_swap_required && num_bytes_per_pixel > 1) {
    uint8* mutable_payload = image->GetData()->GetMutableData<uint8>();
    if (num_bytes_per_pixel == 4) {
      uint32* mutable_payload_ui32 = reinterpret_cast<uint32*>(mutable_payload);
      for (size_t i = 0; i < num_pixels; i++) {
        mutable_payload_ui32[i] = bswap_32(mutable_payload_ui32[i]);
      }
    } else if (num_bytes_per_pixel == 2) {
      uint16* mutable_payload_ui16 = reinterpret_cast<uint16*>(mutable_payload);
      for (size_t i = 0; i < num_pixels; i++) {
        mutable_payload_ui16[i] = bswap_16(mutable_payload_ui16[i]);
      }
    } else {
      DCHECK(false) << "Byte swap not supported yet for num_bytes_per_pixel = "
                    << num_bytes_per_pixel;
    }
  }

  if (flip_vertically) {
    FlipImage(image);
  }

  return image;
}

//-----------------------------------------------------------------------------
//
// Main internal conversion functions.
//
//-----------------------------------------------------------------------------

static uint8 MultiplyUint8ByFloat(const uint8 int_value,
                                  const float float_value) {
  return static_cast<uint8>(static_cast<float>(int_value) * float_value);
}

static const ImagePtr AllocImage(
    Image::Format format, uint32 width, uint32 height, bool is_wipeable,
    const base::AllocatorPtr& allocator) {
  ImagePtr result(new(allocator) Image);
  // This may be different from the allocator passed in if what was passed in
  // was a null pointer.
  const base::AllocatorPtr& use_allocator = result->GetAllocator();
  const size_t size = Image::ComputeDataSize(format, width, height);
  uint8* new_buffer = reinterpret_cast<uint8*>(
      use_allocator->AllocateMemory(size));
  result->Set(format, width, height,
      base::DataContainer::Create<uint8>(
          new_buffer,
          std::bind(base::DataContainer::AllocatorDeleter, use_allocator,
              std::placeholders::_1),
          is_wipeable,
          use_allocator));
  return result;
}

// Create a single channel image from the red channel of an RGB(A) image.
static const ImagePtr ExtractRedChannel(const Image& image,
                                        bool is_wipeable,
                                        const base::AllocatorPtr& allocator) {
  DCHECK(image.GetFormat() == Image::kRgb888 ||
         image.GetFormat() == Image::kRgba8888);
  const uint32 width = image.GetWidth();
  const uint32 height = image.GetHeight();
  ImagePtr result = AllocImage(
      Image::kR8, width, height, is_wipeable, allocator);
  const uint8* src_data = image.GetData()->GetData<uint8>();
  uint8* dst_data = result->GetData()->GetMutableData<uint8>();
  const size_t src_stride = Image::GetNumComponentsForFormat(image.GetFormat());
  const uint8* src_end = src_data + image.GetDataSize();
  while (src_data < src_end) {
    *dst_data = src_data[0];
    src_data += src_stride;
    dst_data++;
  }
  return result;
}

// Create an RGB(A) image from a Luminance(Alpha) image.
static const ImagePtr LuminanceToRgb(const Image& image,
                                     Image::Format target_format,
                                     bool is_wipeable,
                                     const base::AllocatorPtr& allocator) {
  DCHECK(image.GetFormat() == Image::kLuminance ||
         image.GetFormat() == Image::kLuminanceAlpha);
  DCHECK(target_format == Image::kRgb888 ||
         target_format == Image::kRgba8888);

  const uint32 width = image.GetWidth();
  const uint32 height = image.GetHeight();
  ImagePtr result = AllocImage(
      target_format, width, height, is_wipeable, allocator);
  const uint8* src_data = image.GetData()->GetData<uint8>();
  uint8* dst_data = result->GetData()->GetMutableData<uint8>();
  const uint8* src_end = src_data + image.GetDataSize();
  bool src_alpha = image.GetFormat() == Image::kLuminanceAlpha;
  bool dst_alpha = target_format == Image::kRgba8888;
  while (src_data < src_end) {
    // Copy luminance to RGB.
    *dst_data++ = *src_data;
    *dst_data++ = *src_data;
    *dst_data++ = *src_data;
    src_data++;
    // Copy alpha channel if present.
    if (dst_alpha && src_alpha) {
      *dst_data++ = *src_data++;
    } else if (dst_alpha && !src_alpha) {
      *dst_data++ = 255;
    } else if (!dst_alpha && src_alpha) {
      src_data++;
    }
  }
  return result;
}

// Converts an Image to the target_format, returning a new ImagePtr. Returns a
// NULL ImagePtr if anything goes wrong.
static const ImagePtr ImageToImage(const Image& image,
                                   Image::Format target_format,
                                   bool is_wipeable,
                                   const base::AllocatorPtr& allocator,
                                   const base::AllocatorPtr& temp_allocator) {
  const Image::Format source_format = image.GetFormat();

  // TODO(user): Implement other conversions as required.

  ImagePtr result;
  switch (source_format) {
    case Image::kDxt1:
    case Image::kEtc1:
      if (target_format == Image::kRgb888)
        result = DecompressImage(image, is_wipeable, allocator);
      break;

    case Image::kDxt5:
      if (target_format == Image::kRgba8888)
        result = DecompressImage(image, is_wipeable, allocator);
      break;

    case Image::kRgb888:
      if (target_format == Image::kDxt1 || target_format == Image::kEtc1) {
        result = CompressImage(image, target_format, is_wipeable, allocator);
      } else if (target_format == Image::kR8) {
        result = ExtractRedChannel(image, is_wipeable, allocator);
      }
      break;

    case Image::kRgba8888:
      if (target_format == Image::kDxt5 ||
          target_format == Image::kPvrtc1Rgba2) {
        result = CompressImage(image, target_format, is_wipeable, allocator);
      } else if (target_format == Image::kR8) {
        result = ExtractRedChannel(image, is_wipeable, allocator);
      }
      break;

    case Image::kLuminance:
    case Image::kLuminanceAlpha:
      if (target_format == Image::kRgba8888 ||
          target_format == Image::kRgb888) {
        result = LuminanceToRgb(image, target_format, is_wipeable, allocator);
      }
      break;

    // These are all unsupported for now.
    case Image::kAlpha:
    case Image::kPvrtc1Rgb2:
    case Image::kPvrtc1Rgb4:
    case Image::kPvrtc1Rgba2:
    case Image::kPvrtc1Rgba4:
    case Image::kRgb565:
    case Image::kRgba4444:
    case Image::kRgba5551:
    default:
      break;
  }

  // If the above failed, try converting first to the canonical format with the
  // correct component count and then to the target format.
  if (!result.Get()) {
    Image::Format canonical_format = target_format;
    switch (Image::GetNumComponentsForFormat(source_format)) {
      case 1:
        canonical_format = Image::kLuminance;
        break;
      case 2:
        canonical_format = Image::kLuminanceAlpha;
        break;
      case 3:
        canonical_format = Image::kRgb888;
        break;
      case 4:
        canonical_format = Image::kRgba8888;
        break;
      default:
        break;
    }
    if (source_format != canonical_format && canonical_format != target_format)
      result = ConvertImage(
          ImageToImage(image, canonical_format, is_wipeable,
                       temp_allocator, temp_allocator),
          target_format, is_wipeable, allocator, temp_allocator);
  }

  return result;
}

// Converts |data| encoded in a buffer to an Image. Supported formats: PNG
// (using lodepng), JPEG, PNG, TGA, BMP, PSD, GIF, HDR, PIC (using stblib) and
// "ION raw" format (see conversionutils.h for specs of "ION raw" format). This
// method attempts to interpret the given raw data as the above formats, one
// after another until success, otherwise returns NULL |data_size| is the number
// of bytes in |data|.
static const ImagePtr DataToImage(const void* data, size_t data_size,
                                  bool flip_vertically, bool is_wipeable,
                                  const base::AllocatorPtr& allocator) {
  // STB decodes png too, but Lodepng can handle formats that STB doesn't, so
  // we try decoding with Lodepng first.
  ImagePtr image = DataToImageLodePng(
      data, data_size, flip_vertically, is_wipeable, allocator);
  if (image.Get() != NULL) {
    return image;
  }

  image = DataToImageStb(
      data, data_size, flip_vertically, is_wipeable, allocator);
  if (image.Get() != NULL) {
    return image;
  }

  return DataToImageIonRaw(
      data, data_size, flip_vertically, is_wipeable, allocator);
}

// Converts an Image to the external_format, returning a byte vector. Returns
// an empty vector if anything goes wrong.
static const std::vector<uint8> ImageToData(
    const Image& image, ExternalImageFormat external_format,
    bool flip_vertically) {
  std::vector<uint8> result;

  // STBLIB supports writing to PNG, but not JPEG.
  if (external_format == kPng) {
    uint8* image_data = nullptr;
    ImagePtr flipped_image;

    if (flip_vertically) {
      flipped_image.Reset(new Image);
      flipped_image->Set(image.GetFormat(), image.GetWidth(), image.GetHeight(),
                         base::DataContainer::CreateAndCopy<uint8>(
                              image.GetData()->GetData<uint8>(),
                              image.GetDataSize(),
                              false,
                              image.GetAllocator()));
      FlipImage(flipped_image);
      image_data = const_cast<uint8*>(
          flipped_image->GetData()->GetData<uint8>());
    } else {
      image_data = const_cast<uint8*>(image.GetData()->GetData<uint8>());
    }

    int num_bytes;
    if (unsigned char* result_data = stbi_write_png_to_mem(
            image_data, 0, image.GetWidth(), image.GetHeight(),
            Image::GetNumComponentsForFormat(image.GetFormat()),
            &num_bytes)) {
      result = std::vector<uint8>(result_data, result_data + num_bytes);
      stbi_image_free(result_data);
    }
  }
  return result;
}

// Returns an Image of the same format as |image|, but with half the width and
// height. Allocations are done via |allocator| and it assumes |image| is in a
// format compatible with |compressor|.
static const ImagePtr DownsampleWithCompressor(
    const Image& image,
    image_codec_compression::Compressor* compressor,
    bool is_wipeable,
    const base::AllocatorPtr& allocator) {
  using image_codec_compression::CompressedImage;
  const CompressedImage::Format format = image.GetFormat() == Image::kDxt5 ?
      CompressedImage::kRGBA : CompressedImage::kRGB;

  const uint32 width = image.GetWidth();
  const uint32 height = image.GetHeight();

  // Create a CompressedImage wrapping the image data.
  const void* image_data = image.GetData()->GetData();
  DCHECK(image_data);
  image_codec_compression::CompressedImage compressed_image(
      image.GetDataSize(),
      const_cast<uint8*>(reinterpret_cast<const uint8*>(image_data)));
  const char* compressor_name =
      image.GetFormat() == Image::kEtc1 ? "etc" : "dxtc";
  compressed_image.SetMetadata(CompressedImage::Metadata(
      format, compressor_name,
      height, width, height, width, 0));

  // Compress into a local CompressedImage.
  CompressedImage downsampled_image;
  ImagePtr result;
  if (compressor->Downsample(compressed_image, &downsampled_image)) {
    const CompressedImage::Metadata& metadata = downsampled_image.GetMetadata();
    result.Reset(new(allocator) Image);
    result->Set(image.GetFormat(), metadata.uncompressed_width,
                metadata.uncompressed_height,
                base::DataContainer::CreateAndCopy<uint8>(
                    downsampled_image.GetData(),
                    downsampled_image.GetDataSize(),
                    is_wipeable, result->GetAllocator()));
  }
  return result;
}

// Simple 2x downsampling with box filter.
// The math here isn't correct for un-premultiplied alpha images, so you should
// pass in premultiplied images if possible.  In particular fully opaque red
// next to fully transparent green will yield a half-transparent amber pixel,
// instead of half-transparent red.
static const ImagePtr Downsample2xSimple8bpc(
    const Image& image,
    bool is_wipeable,
    const base::AllocatorPtr& allocator) {
  const uint32 num_channels =
      Image::GetNumComponentsForFormat(image.GetFormat());
  // Round up size on odd widths and heights.
  const uint32 new_width = (image.GetWidth() + 1) >> 1;
  const uint32 new_height = (image.GetHeight() + 1) >> 1;
  ImagePtr result = AllocImage(
      image.GetFormat(), new_width, new_height, is_wipeable, allocator);
  const uint8* src_data = image.GetData()->GetData<uint8>();
  uint8* dst_data = result->GetData()->GetMutableData<uint8>();
  const size_t src_stride = image.GetWidth() * num_channels;
  const size_t dst_stride = new_width * num_channels;
  for (size_t src_row = 0, src_rows = image.GetHeight();
       src_row < src_rows; src_row += 2) {
    const size_t dst_row = src_row >> 1;
    // Clamp src row to stay within image bounds.
    const size_t next_row = (src_row == src_rows - 1) ? 0 : src_stride;
    for (size_t src_col = 0, src_cols = image.GetWidth();
         src_col < src_cols; src_col += 2) {
      const uint8* src_pixel =
          src_data + src_row * src_stride + src_col * num_channels;
      const size_t dst_col = src_col >> 1;
      uint8* dst_pixel =
          dst_data + dst_row * dst_stride + dst_col * num_channels;
      // Clamp src column to stay within image bounds.
      const size_t next_col = (src_col == src_cols - 1) ? 0 : num_channels;
      for (size_t chan = 0; chan < num_channels; ++chan) {
        dst_pixel[chan] =
            static_cast<uint8>((src_pixel[chan] +
                src_pixel[chan + next_col] +
                src_pixel[chan + next_row] +
                src_pixel[chan + next_col + next_row] + 1) >> 2);
      }
    }
  }
  return result;
}

// Bilinearly interpolate an 8-bit-per-channel |image|.
// Bilinear resizing is most useful for upsizing images as it only uses a
// weighted average of the 4 closest pixel values, and has reasonable quality.
static const gfx::ImagePtr ResizeBilinear8bpc(
    const gfx::Image& image, uint32 out_width, uint32 out_height,
    bool is_wipeable, const base::AllocatorPtr& allocator) {
  ImagePtr result = AllocImage(
      image.GetFormat(), out_width, out_height, is_wipeable, allocator);
  const float xscale =
      static_cast<float>(image.GetWidth()) / static_cast<float>(out_width);
  const float yscale =
      static_cast<float>(image.GetHeight()) / static_cast<float>(out_height);
  const uint8* src_data = image.GetData()->GetData<uint8>();
  uint8* dst_data = result->GetData()->GetMutableData<uint8>();
  int num_channels = Image::GetNumComponentsForFormat(image.GetFormat());
  const size_t src_stride = image.GetWidth() * num_channels;
  const size_t dst_stride = out_width * num_channels;
  const size_t max_src_x = static_cast<size_t>(image.GetWidth() - 1);
  const size_t max_src_y = static_cast<size_t>(image.GetHeight() - 1);
  // Note that pixel values should be treated as located at the center of each
  // pixel. I.e., pixel (i,j)'s value is centered at (i+0.5,j+0.5) for the
  // purposes of computing how close a sample location is to the source pixel.
  for (uint32 dst_y = 0; dst_y < out_height; ++dst_y) {
    for (uint32 dst_x = 0; dst_x < out_width; ++dst_x) {
      uint8* dst_pixel = dst_data + dst_y * dst_stride + dst_x * num_channels;
      float src_x = (static_cast<float>(dst_x) + 0.5f) * xscale;
      float src_y = (static_cast<float>(dst_y) + 0.5f) * yscale;
      // Compute fractional distances to the nearest pixel centers, which are
      // also the interpolation weights.
      float s1 = src_x + 0.5f - std::floor(src_x + 0.5f);
      float s0 = 1.0f - s1;
      float t1 = src_y + 0.5f - std::floor(src_y + 0.5f);
      float t0 = 1.0f - t1;
      // Clamp the source pixels inside the boundary.
      size_t src_x0 =
          static_cast<size_t>(std::max(0.f, std::floor(src_x - 0.5f)));
      size_t src_y0 =
          static_cast<size_t>(std::max(0.f, std::floor(src_y - 0.5f)));
      size_t src_x1 = std::min(
          max_src_x,
          static_cast<size_t>(std::floor(src_x + 0.5f)));
      size_t src_y1 = std::min(
          max_src_y,
          static_cast<size_t>(std::floor(src_y + 0.5f)));
      const uint8* src_00 =
          src_data + src_x0 * num_channels + src_y0 * src_stride;
      const uint8* src_10 =
          src_data + src_x1 * num_channels + src_y0 * src_stride;
      const uint8* src_01 =
          src_data + src_x0 * num_channels + src_y1 * src_stride;
      const uint8* src_11 =
          src_data + src_x1 * num_channels + src_y1 * src_stride;
      for (int chan = 0; chan < num_channels; ++chan) {
        dst_pixel[chan] = static_cast<uint8>(std::floor(
            s0 * t0 * static_cast<float>(src_00[chan]) +
            s1 * t0 * static_cast<float>(src_10[chan]) +
            s0 * t1 * static_cast<float>(src_01[chan]) +
            s1 * t1 * static_cast<float>(src_11[chan]) + 0.5f));
      }
    }
  }
  return result;
}

// Returns the rect resulting from the intersection of |src_rectf| and a
// 1x1 rect with min point at (src_x, src_y).
static Range2f GetPixelRectIntersection(
    float src_x, float src_y, const Range2f& src_rectf) {
  Point2f min_pt = src_rectf.GetMinPoint();
  Point2f max_pt = src_rectf.GetMaxPoint();
  min_pt[0] = std::max(src_x, min_pt[0]);
  min_pt[1] = std::max(src_y, min_pt[1]);
  max_pt[0] = std::min(src_x + 1.f, max_pt[0]);
  max_pt[1] = std::min(src_y + 1.f, max_pt[1]);
  return Range2f(min_pt, max_pt);
}

// Returns the area of the rectangle |rect|.
static float GetRectArea(const Range2f& rect) {
  math::Vector2f size = rect.GetSize();
  return size[0] * size[1];
}

// Use a box filter to resize an 8-bit-per-channel |image|.
// Box filtering is most useful for downsizing images.
// It maps the square of the new pixel onto the source image and takes the
// area-weighted average of contributions from each of the old source pixels.
static const gfx::ImagePtr ResizeBoxFilter8bpc(
    const gfx::Image& image, uint32 out_width, uint32 out_height,
    bool is_wipeable, const base::AllocatorPtr& allocator) {
  const int kMaxChannels = 4;
  ImagePtr result;
  const int num_channels = Image::GetNumComponentsForFormat(image.GetFormat());
  DCHECK_LE(num_channels, kMaxChannels)
      << "Unsupported number of channels for resize.";
  result = AllocImage(
      image.GetFormat(), out_width, out_height, is_wipeable, allocator);
  Point2f rect_scale(
      static_cast<float>(image.GetWidth()) / static_cast<float>(out_width),
      static_cast<float>(image.GetHeight()) / static_cast<float>(out_height));
  const uint8* src_data = image.GetData()->GetData<uint8>();
  uint8* dst_data = result->GetData()->GetMutableData<uint8>();
  const size_t src_stride = image.GetWidth() * num_channels;
  const size_t dst_stride = out_width * num_channels;
  float src_area = GetRectArea(Range2f(Point2f::Zero(), rect_scale));
  for (uint32 dst_y = 0; dst_y < out_height; ++dst_y) {
    for (uint32 dst_x = 0; dst_x < out_width; ++dst_x) {
      // Compute the dst pixel's rectangle in the src image.
      float dst_xf = static_cast<float>(dst_x);
      float dst_yf = static_cast<float>(dst_y);
      Range2f src_rectf = Range2f(
          Point2f(dst_xf, dst_yf) * rect_scale,
          Point2f(dst_xf + 1.f, dst_yf + 1.f) * rect_scale);
      // Pixel coords of all the pixels in the src that touch |src_rect|.
      Point2f src_min(
          std::floor(src_rectf.GetMinPoint()[0]),
          std::floor(src_rectf.GetMinPoint()[1]));
      Point2f src_max(
          std::ceil(src_rectf.GetMaxPoint()[0]),
          std::ceil(src_rectf.GetMaxPoint()[1]));

#if ION_DEBUG
      float check_area_total = 0.f;
#endif
      float dst_value[kMaxChannels] = { 0.f };
      for (float src_y = src_min[1]; src_y < src_max[1]; ++src_y) {
        for (float src_x = src_min[0]; src_x < src_max[0]; ++src_x) {
          // Intersect src pixel's region with full src_rect.
          Range2f src_dest_intersect =
              GetPixelRectIntersection(src_x, src_y, src_rectf);
          float part_area = GetRectArea(src_dest_intersect);
#if ION_DEBUG
          check_area_total += part_area;
#endif
          size_t src_xi = static_cast<size_t>(src_x);
          size_t src_yi = static_cast<size_t>(src_y);
          const uint8* src_pixel =
              src_data + src_yi * src_stride + src_xi * num_channels;
          for (int chan = 0; chan < num_channels; ++chan) {
            dst_value[chan] += part_area * static_cast<float>(src_pixel[chan]);
          }
        }
      }
#if ION_DEBUG
      DCHECK_LT(std::abs(check_area_total - src_area), 1e-3);
#endif
      uint8* dst_pixel = dst_data + dst_y * dst_stride + dst_x * num_channels;
      for (int chan = 0; chan < num_channels; ++chan) {
        dst_pixel[chan] =
            static_cast<uint8>(std::floor(dst_value[chan] / src_area + 0.5f));
      }
    }
  }
  return result;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Public functions.
//
//-----------------------------------------------------------------------------

const ImagePtr ION_API ConvertImage(
    const ImagePtr& image, Image::Format target_format, bool is_wipeable,
    const base::AllocatorPtr& allocator,
    const base::AllocatorPtr& temporary_allocator) {
  if (!ImageHasData(image))
    return ImagePtr();

  if (image->GetFormat() == target_format) {
    if (image->GetData()->IsWipeable() == is_wipeable) {
      return image;
    } else {
      // Copy image to data container with expected wipeable flag.
      ImagePtr result;
      result.Reset(new(allocator) Image);
      result->Set(
          image->GetFormat(), image->GetWidth(), image->GetHeight(),
          base::DataContainer::CreateAndCopy<uint8>(
              reinterpret_cast<const uint8*>(image->GetData()->GetData()),
              image->GetDataSize(), is_wipeable, result->GetAllocator()));
      return result;
    }
  }

  const base::AllocatorPtr& al =
      base::AllocationManager::GetNonNullAllocator(allocator);
  const base::AllocatorPtr& temp_al =
      base::AllocationManager::GetNonNullAllocator(temporary_allocator);
  return ImageToImage(*image, target_format, is_wipeable, al, temp_al);
}

const ImagePtr ION_API ConvertFromExternalImageData(
    const void* data, size_t data_size, bool flip_vertically, bool is_wipeable,
    const base::AllocatorPtr& allocator) {
  if (!data || data_size == 0)
    return ImagePtr();

  // Convert the data to an Image and then convert to the correct target format.
  // We attempt to interpret the data in different formats, one after another,
  // using lodepng, stblib and built-in codes.
  const base::AllocatorPtr& al =
      base::AllocationManager::GetNonNullAllocator(allocator);
  return DataToImage(data, data_size, flip_vertically, is_wipeable, al);
}

bool ION_API IsIonRawImageFormat(const void* data, size_t data_size) {
  const uint8* header_ui8 = static_cast<const uint8*>(data);
  return data_size >= kIonRawImageHeaderSizeInBytes &&
      header_ui8[0] == 0x89 && header_ui8[1] == 'R' &&
      header_ui8[2] == 'A' && header_ui8[3] == 'W' /* const magic cues */ &&
      ((header_ui8[4] == 0x00 && header_ui8[5] == 0x01) ||
       (header_ui8[4] == 0x01 && header_ui8[5] == 0x00)) /* Endianness cue */;
}

const std::vector<uint8> ION_API ConvertToExternalImageData(
    const ImagePtr& image, ExternalImageFormat external_format,
    bool flip_vertically) {
  if (!ImageHasData(image))
    return std::vector<uint8>();

  return ImageToData(*image, external_format, flip_vertically);
}

const ImagePtr ION_API DownsampleImage2x(
    const ImagePtr& image, bool is_wipeable,
    const base::AllocatorPtr& allocator) {
  ImagePtr result;
  if (ImageHasData(image) &&
      image->GetWidth() > 1U && image->GetHeight() > 1U) {
    if (image->GetFormat() == Image::kEtc1) {
      image_codec_compression::EtcCompressor compressor;
      result = DownsampleWithCompressor(
          *image, &compressor, is_wipeable, allocator);
    } else if (image->GetFormat() == Image::kDxt1 ||
               image->GetFormat() == Image::kDxt5) {
      image_codec_compression::DxtcCompressor compressor;
      result = DownsampleWithCompressor(
          *image, &compressor, is_wipeable, allocator);
    } else if (Image::Is8BitPerChannelFormat(image->GetFormat())) {
      result = Downsample2xSimple8bpc(*image, is_wipeable, allocator);
    } else {
      LOG(WARNING) << "Downsampling image format "
                   << Image::GetFormatString(image->GetFormat())
                   << " not supported.";
    }
  }
  return result;
}

const gfx::ImagePtr ResizeImage(
    const gfx::ImagePtr& image, uint32 out_width, uint32 out_height,
    bool is_wipeable, const base::AllocatorPtr& allocator) {
  ImagePtr result;
  if (!ImageHasData(image)) {
    return result;
  }
  if (!Image::Is8BitPerChannelFormat(image->GetFormat())) {
    LOG(WARNING) << "Resizing image format "
                 << Image::GetFormatString(image->GetFormat())
                 << " not supported.";
    return result;
  }

  uint32 image_width = image->GetWidth();
  uint32 image_height = image->GetHeight();
  if (out_width < image_width && out_height < image_height) {
    result = ResizeBoxFilter8bpc(
        *image, out_width, out_height, is_wipeable, allocator);
  } else {
    // For upscaling or mixed scale up / scale down, use bilinear interpolation
    result = ResizeBilinear8bpc(
        *image, out_width, out_height, is_wipeable, allocator);
  }
  return result;
}


ION_API void FlipImage(const gfx::ImagePtr& image) {
  if (!ImageHasData(image) || image->GetHeight() <= 1U) {
    return;
  }
  if (image->IsCompressed()) {
    DLOG(WARNING) << "Flipping compressed images is not supported.";
    return;
  }
  const size_t height = image->GetHeight();
  base::DataContainerPtr data = image->GetData();
  uint8* image_bytes = data->GetMutableData<uint8>();
  const size_t row_size_bytes = image->GetDataSize() / height;

  // Stack allocate a buffer to enable swapping larger chunks with memcpy.
  // This method worked pretty well for both large and small images in a
  // small test program comparing different flipping algorithms.
  const size_t kBufferSize = 512U;
  uint8 tmp_buf[kBufferSize];
  for (size_t j = 0, target_j = height - 1; j < height / 2; ++j, --target_j) {
    size_t copied_bytes = 0;
    while (copied_bytes < row_size_bytes) {
      const size_t copy_size =
          std::min(row_size_bytes - copied_bytes, kBufferSize);
      uint8* src_row = image_bytes + j * row_size_bytes + copied_bytes;
      uint8* tgt_row = image_bytes + target_j * row_size_bytes + copied_bytes;
      memcpy(tmp_buf, tgt_row, copy_size);
      memcpy(tgt_row, src_row, copy_size);
      memcpy(src_row, tmp_buf, copy_size);
      copied_bytes += kBufferSize;
    }
  }
}

ION_API void FlipImageHorizontally(const gfx::ImagePtr& image) {
  if (!ImageHasData(image) || image->GetWidth() <= 1U) {
    return;
  }
  if (image->IsCompressed()) {
    DLOG(WARNING) << "Flipping compressed images is not supported.";
    return;
  }
  const size_t height = image->GetHeight();
  const size_t width = image->GetWidth();
  base::DataContainerPtr data = image->GetData();
  uint8* image_bytes = data->GetMutableData<uint8>();
  const size_t row_size_bytes = image->GetDataSize() / height;
  const size_t pixel_size_bytes = row_size_bytes / width;

  for (size_t row = 0; row < height; ++row) {
    const size_t row_start = row * row_size_bytes;

    for (size_t column = 0; column < width / 2; ++column) {
      for (size_t byte = 0; byte < pixel_size_bytes; ++byte) {
        const size_t left = column, right = width - 1 - column;
        std::swap(image_bytes[row_start + left  * pixel_size_bytes + byte],
                  image_bytes[row_start + right * pixel_size_bytes + byte]);
      }
    }
  }
}

ION_API void StraightAlphaFromPremultipliedAlpha(const gfx::ImagePtr& image) {
  if (!ImageHasData(image) || image->GetWidth() <= 1U) {
    return;
  }

  const size_t byte_count = image->GetDataSize();
  base::DataContainerPtr data = image->GetData();
  uint8* image_bytes = data->GetMutableData<uint8>();

  switch (image->GetFormat()) {
    case gfx::Image::kRgba8888:
      for (size_t i = 0; i < byte_count; i += 4) {
        const uint8 alpha_byte = image_bytes[i + 3];
        if (alpha_byte == 0)
          continue;
        const float inverse_alpha = 255.f / static_cast<float>(alpha_byte);
        image_bytes[i] = MultiplyUint8ByFloat(image_bytes[i], inverse_alpha);
        image_bytes[i + 1] =
            MultiplyUint8ByFloat(image_bytes[i + 1], inverse_alpha);
        image_bytes[i + 2] =
            MultiplyUint8ByFloat(image_bytes[i + 2], inverse_alpha);
      }
      break;
    default:
      DLOG(WARNING) << "Converting premultiplied alpha to straight alpha from"
                    << " formats other than Rgba8888 is not supported.";
  }
}

}  // namespace image
}  // namespace ion
