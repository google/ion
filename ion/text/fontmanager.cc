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

#include "ion/text/fontmanager.h"

#include <sstream>

#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
#define ION_FONT_CORE_TEXT
#else
#define ION_FONT_FREE_TYPE
#endif

#include "ion/base/zipassetmanager.h"
#if defined(ION_FONT_CORE_TEXT)
#include "ion/text/coretextfont.h"
#elif defined(ION_FONT_FREE_TYPE)
#include "ion/text/freetypefont.h"
#endif

namespace ion {
namespace text {

//-----------------------------------------------------------------------------
//
// FontManager functions.
//
//-----------------------------------------------------------------------------

FontManager::FontManager()
    : font_map_(*this),
      font_image_map_(*this),
      memory_mapped_font_files_map_(*this) {}

FontManager::~FontManager() {}

void FontManager::AddFont(const FontPtr& font) {
  if (font.Get())
    font_map_[BuildFontKeyFromFont(*font)] = font;
}

const FontPtr FontManager::AddFont(
    const std::string& name, size_t size_in_pixels,
    size_t sdf_padding, const void* data, size_t data_size) {
  ion::text::FontPtr font = FindFont(name, size_in_pixels, sdf_padding);
  if (font.Get())
    return font;

#if defined(ION_FONT_CORE_TEXT)
  font.Reset(new ion::text::CoreTextFont(
      name, size_in_pixels, sdf_padding, data, data_size));
#elif defined(ION_FONT_FREE_TYPE)
  font.Reset(new ion::text::FreeTypeFont(
      name, size_in_pixels, sdf_padding, data, data_size));
#endif
  AddFont(font);
  return font;
}

const FontPtr FontManager::AddFontFromZipasset(const std::string& font_name,
                                               const std::string& zipasset_name,
                                               size_t size_in_pixels,
                                               size_t sdf_padding) {
  ion::text::FontPtr font = FindFont(font_name, size_in_pixels, sdf_padding);
  if (font.Get())
    return font;

  // Read the font data.
  const std::string& data =
  ion::base::ZipAssetManager::GetFileData(zipasset_name + ".ttf");
  if (ion::base::IsInvalidReference(data) || data.empty()) {
    LOG(ERROR) << "Unable to read data for font \"" << font_name << "\".";
    return font;
  }

  return AddFont(font_name, size_in_pixels, sdf_padding, &data[0], data.size());
}

const FontPtr FontManager::AddFontFromFilePath(const std::string& font_name,
                                               const std::string& file_path,
                                               size_t size_in_pixels,
                                               size_t sdf_padding) {
  ion::text::FontPtr font = FindFont(font_name, size_in_pixels, sdf_padding);
  if (font.Get())
    return font;

  // Check if the file has already been loaded into memory.
  auto font_file = memory_mapped_font_files_map_.find(file_path);
  if (font_file != memory_mapped_font_files_map_.end()) {
    if (font_file->second->GetData() == nullptr) {
      LOG(ERROR) << "Unable to read data for font \"" << font_name
                 << "\" from path \"" << file_path << "\".";
      return font;
    }
    return AddFont(font_name, size_in_pixels, sdf_padding,
                   font_file->second->GetData(),
                   font_file->second->GetLength());
  }

  // Read the font data.
  std::unique_ptr<port::MemoryMappedFile> memory_mapped_file(
      new port::MemoryMappedFile(file_path));
  const void* data = memory_mapped_file->GetData();
  if (data == nullptr) {
    LOG(ERROR) << "Unable to read data for font \"" << font_name
               << "\" from path \"" << file_path << "\".";
    return font;
  }

  font = AddFont(font_name, size_in_pixels, sdf_padding,
                 data, memory_mapped_file->GetLength());
#if defined(ION_FONT_FREE_TYPE)
  // FreeTypeFont requires that the backing data exists as long as the
  // FreeTypeFont object, so store the MemoryMappedFile here.
  if (font.Get() != nullptr) {
    memory_mapped_font_files_map_[file_path] = std::move(memory_mapped_file);
  }
#endif
  return font;
}

const FontPtr FontManager::FindFont(
    const std::string& name, size_t size_in_pixels, size_t sdf_padding) const {
  const std::string key = BuildFontKey(name, size_in_pixels, sdf_padding);
  FontMap::const_iterator it = font_map_.find(key);
  return it == font_map_.end() ? FontPtr() : it->second;
}

const std::string FontManager::BuildFontKey(
    const std::string& name, size_t size_in_pixels, size_t sdf_padding) {
  std::ostringstream s;
  s << name << '/' << size_in_pixels << '/' << sdf_padding;
  return s.str();
}

}  // namespace text
}  // namespace ion
