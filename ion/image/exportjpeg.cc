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

#include "ion/image/exportjpeg.h"

#include "ion/base/datacontainer.h"
extern "C" {
#include "third_party/libjpeg_turbo//jpeglib.h"
}

namespace ion {
namespace image {

using gfx::Image;
using gfx::ImagePtr;

namespace {

void JpegInitializeDestination(j_compress_ptr cinfo) {
  const size_t kInitialBufferSize = 1024;
  std::vector<uint8>* const buffer =
      static_cast<std::vector<uint8>*>(cinfo->client_data);
  buffer->resize(kInitialBufferSize);
  cinfo->dest->next_output_byte = &(*buffer)[0];
  cinfo->dest->free_in_buffer = buffer->size();
}

boolean JpegOnEmptyOutputBuffer(j_compress_ptr cinfo) {
  std::vector<uint8>* const buffer =
      static_cast<std::vector<uint8>*>(cinfo->client_data);
  const size_t old_size = buffer->size();
  buffer->resize(old_size * 2);
  cinfo->dest->next_output_byte = &(*buffer)[old_size];
  cinfo->dest->free_in_buffer = buffer->size() - old_size;
  return true;
}

void JpegTerminateDestination(j_compress_ptr cinfo) {
  std::vector<uint8>* const buffer =
      static_cast<std::vector<uint8>*>(cinfo->client_data);
  buffer->resize(buffer->size() - cinfo->dest->free_in_buffer);
}

}  // anonymous namespace

const std::vector<uint8> ION_API ConvertToJpeg(
    const ImagePtr& image, bool flip_vertically, int quality) {
  if (!image || !image->GetData() || !image->GetData()->GetData())
    return std::vector<uint8>();
  // 
  if (image->GetFormat() != Image::Format::kRgb888)
    return std::vector<uint8>();
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  // 
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  // Set up output to memory buffer.
  std::vector<uint8> jpeg_output;
  cinfo.client_data = &jpeg_output;
  struct jpeg_destination_mgr dest;
  cinfo.dest = &dest;
  cinfo.dest->init_destination = &JpegInitializeDestination;
  cinfo.dest->empty_output_buffer = &JpegOnEmptyOutputBuffer;
  cinfo.dest->term_destination = &JpegTerminateDestination;
  // Set image and compression parameters.
  cinfo.image_width = image->GetWidth();
  cinfo.image_height = image->GetHeight();
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, true /* limit to baseline-JPEG values */);
  // Compress the Image.
  jpeg_start_compress(&cinfo, true /* write all tables*/);
  const int stride = image->GetWidth() * 3;
  // Presumably jpeg_write_scanlines() will not modify the data, although it
  // does not declare the data pointer const.
  unsigned char* data = image->GetData()->GetMutableData<unsigned char>();
  JSAMPROW row_pointer[1];
  for (unsigned int row = 0; row < cinfo.image_height; ++row) {
    row_pointer[0] =
        &data[(flip_vertically ? cinfo.image_height - 1 - row : row) * stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  return jpeg_output;
}



}  // namespace image
}  // namespace ion
