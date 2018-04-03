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

#ifndef ION_TEXT_FONTMANAGER_H_
#define ION_TEXT_FONTMANAGER_H_

#include <string>
#include <vector>

#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/external/gtest/gunit_prod.h"  // For FRIEND_TEST().
#include "ion/port/memorymappedfile.h"
#include "ion/text/font.h"
#include "ion/text/fontimage.h"

namespace ion {
namespace text {

// The FontManager provides the main interface for fonts used to create text
// strings to render. It also provides a way to cache FontImage instances for
// reuse.
class ION_API FontManager : public base::Referent {
 public:
  FontManager();

  // Adds a Font to the manager. It will then be accessible via FindFont().
  // This does nothing if the Font is NULL.
  virtual void AddFont(const FontPtr& font);

  // Constructs and adds a font to the manager. If a font with the given specs
  // already exists, just returns the already existing font. This will choose
  // the correct Font subclass for the current platform. If |data| is non-NULL
  // and |data_size| non-zero, |data| will be read as TrueType data of length
  // |data_size| to build the font. Otherwise, behavior depends on specific
  // font implementations, see CoreTextFont and FreeTypeFont.
  virtual const FontPtr AddFont(const std::string& name, size_t size_in_pixels,
                                size_t sdf_padding, const void* data,
                                size_t data_size);

  // Constructs and adds a font with name |font_name| from the zipasset with
  // name |zipasset_name|. If a font with the given specs already exists, just
  // returns the already existing font.
  virtual const FontPtr AddFontFromZipasset(const std::string& font_name,
                                            const std::string& zipasset_name,
                                            size_t size_in_pixels,
                                            size_t sdf_padding);

  // Constructs and adds a font with name |font_name| by loading the file at
  // |file_path|. If a font with the given specs already exists, just returns
  // the already existing font.
  virtual const FontPtr AddFontFromFilePath(const std::string& font_name,
                                            const std::string& file_path,
                                            size_t size_in_pixels,
                                            size_t sdf_padding);

  // Returns the Font associated with the given name and size. This will return
  // a NULL pointer unless the font was previously added with AddFont().
  virtual const FontPtr FindFont(const std::string& name, size_t size_in_pixels,
                                 size_t sdf_padding) const;

  // This can be used to cache FontImage instances in the manager. It
  // associates a FontImage with a client-defined string key. Passing a NULL
  // FontImage pointer removes the entry for that key.
  virtual void CacheFontImage(const std::string& key,
                              const FontImagePtr& font_image) {
    if (font_image.Get())
      font_image_map_[key] = font_image;
    else
      font_image_map_.erase(key);
  }

  // This can be used to cache FontImage instances in the manager. It
  // associates a FontImage with a string key that Ion derives based on the
  // provided font. Passing a NULL FontImage pointer removes the entry for that
  // key.
  virtual void CacheFontImage(const FontPtr& font,
                              const FontImagePtr& font_image) {
    CacheFontImage(BuildFontKeyFromFont(*font), font_image);
  }

  // Returns the FontImage associated with the given key. It may be NULL.
  virtual const FontImagePtr GetCachedFontImage(const std::string& key) const {
    const FontImageMap::const_iterator it = font_image_map_.find(key);
    return it == font_image_map_.end() ? FontImagePtr() : it->second;
  }

  // Returns the FontImage associated with the given Font. It may be NULL.
  virtual const FontImagePtr GetCachedFontImage(const FontPtr& font) const {
    return GetCachedFontImage(BuildFontKeyFromFont(*font));
  }

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~FontManager() override;

 private:
  typedef base::AllocMap<std::string, FontPtr> FontMap;
  typedef base::AllocMap<std::string, FontImagePtr> FontImageMap;
  typedef base::AllocMap<std::string, std::unique_ptr<port::MemoryMappedFile>>
      MemoryMappedFileMap;

  // Constructs a string key from a Font for use in the Font map.
  static const std::string BuildFontKeyFromFont(const Font& font) {
    return BuildFontKey(font.GetName(), font.GetSizeInPixels(),
                        font.GetSdfPadding());
  }

  // Constructs a string key from a font name, size, and SDF padding amount;
  // the key can then be used for the Font map.
  static const std::string BuildFontKey(
      const std::string& name, size_t size_in_pixels, size_t sdf_padding);

  // Maps a Font key to a Font instance.
  FontMap font_map_;
  // Maps a user-supplied string key to a FontImage instance.
  FontImageMap font_image_map_;
  // Maps a file path to the MemoryMappedFile that backs one or more Font
  // instances in |font_map_| that were loaded via AddFontFromFilePath() when
  // using FreeTypeFonts. This is necessary because FreeTypeFont requires that
  // the data backing it exist as long as the FreeTypeFont object does. This is
  // cached per-path so that the same font being loaded at multiple sizes only
  // maps the file into memory once.
  MemoryMappedFileMap memory_mapped_font_files_map_;

  // Allow tests to access private functions.
  FRIEND_TEST(FontManagerTest, BuildFontKey);
};

// Convenience typedef for shared pointer to a FontManager.
using FontManagerPtr = base::SharedPtr<FontManager>;

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_FONTMANAGER_H_
