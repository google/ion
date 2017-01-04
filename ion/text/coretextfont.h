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

#ifndef ION_TEXT_CORETEXTFONT_H_
#define ION_TEXT_CORETEXTFONT_H_

#include <memory>
#include <string>

#include "ion/math/vector.h"
#include "ion/text/font.h"

namespace ion {
namespace text {

// This represents a single CoreText font. However, when asked to render
// characters not in the CoreText font, it will fall back to other system
// CoreText fonts, and therefore can be used to render characters supported by
// any font in the OS.
class ION_API CoreTextFont : public Font {
 public:
  // Constructs an instance using the given name. If |data| is non-NULL and
  // |data_size| non-zero, |data| will be read as TrueType data of length
  // |data_size| to build the font. Otherwise the OS will be asked to build a
  // font with the given |name|. This will try to match the name, or fall back
  // to the a default system font. For a list of available fonts see (for
  // example) iosfonts.com.
  CoreTextFont(const std::string& name, size_t size_in_pixels,
               size_t sdf_padding, const void* data, size_t data_size);

  // Returns the name of the backing system font. This can be used to check what
  // font the system used when trying to match the |name| parameter used in the
  // CoreTextFont constructor.
  std::string GetCTFontName() const;

  // Font overrides.
  GlyphIndex GetDefaultGlyphForChar(CharIndex char_index) const override;
  const Layout BuildLayout(const std::string& text,
                           const LayoutOptions& options) const override;
  void AddFallbackFont(const FontPtr& fallback) override;

 protected:
  ~CoreTextFont() override;

  bool LoadGlyphGrid(GlyphIndex glyph_index,
                     GlyphGrid* glyph_grid) const override;

 private:
  // Helper class that does most of the work, hiding the implementation.
  class Helper;
  std::unique_ptr<Helper> helper_;
};

// Convenience typedef for shared pointer to a CoreTextFont.
using CoreTextFontPtr = base::SharedPtr<CoreTextFont>;

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_CORETEXTFONT_H_
