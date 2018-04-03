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

#ifndef ION_TEXT_FREETYPEFONT_H_
#define ION_TEXT_FREETYPEFONT_H_

#include <memory>
#include <string>

#include "ion/math/vector.h"
#include "ion/text/font.h"

#if defined(ION_USE_ICU)
#include "third_party/iculx_hb/include/layout/ParagraphLayout.h"
#endif  // ION_USE_ICU

namespace ion {
namespace text {

// This derived Font class represents a FreeType2 font.
class ION_API FreeTypeFont : public Font {
 public:
  // This struct represents the metrics for a single glyph.
  // 
  // positioning. (FreeType2 uses 1/64ths.)
  struct GlyphMetrics {
    // The default constructor initializes everything to 0.
    GlyphMetrics()
        : size(0.f, 0.f), bitmap_offset(0.f, 0.f), advance(0.f, 0.f) {}

    // Returns true if glyph x- *or* y-size is zero.
    bool IsZeroSize() const { return size[0] == 0.0f || size[1] == 0.0f; }

    // Width and height of the glyph, in pixels.
    math::Vector2f size;

    // Distance in X and Y from the baseline to the top left pixel of the glyph
    // bitmap, in pixels. This should be added to the current position when
    // drawing the glyph.  The Y distance is *positive* for an upward offset.
    math::Vector2f bitmap_offset;

    // Number of pixels to advance in X and Y to draw the next glyph after this.
    math::Vector2f advance;
  };

  // Constructs an instance using the given name. The supplied font data may be
  // in any format that FreeType2's FT_New_Memory_Face() can handle. The data
  // must not be deallocated before destruction of the FreeTypeFont. The font
  // size will be as close as possible to the specified size.
  FreeTypeFont(const std::string& name, size_t size_in_pixels,
               size_t sdf_padding, const void* data, size_t data_size);

  // Returns the GlyphMetrics for a glyph. This defines the basic layout of a
  // given glyph.
  const GlyphMetrics& GetGlyphMetrics(GlyphIndex glyph_index) const;

  // Returns the delta that should be made to relative positioning of characters
  // beyond the metrics above.
  const math::Vector2f GetKerning(CharIndex char_index0,
                                  CharIndex char_index1) const;

#if defined(ION_USE_ICU)
  // Compute the FontRuns which will cover the string |chars| taking fallbacks
  // into account.
  void GetFontRunsForText(icu::UnicodeString chars,
                          iculx::FontRuns* runs) const;
  GlyphIndex GlyphIndexForICUFont(const icu::LEFontInstance* icu_font,
                                  int32 glyph_id) const;
#endif  // ION_USE_ICU

  // Font overrides.
  GlyphIndex GetDefaultGlyphForChar(CharIndex char_index) const override;
  const Layout BuildLayout(const std::string& text,
                           const LayoutOptions& options) const override;
  void AddFallbackFont(const FontPtr& fallback) override;

 protected:
  // This constructor is used only for testing. It simulates the failure of the
  // FreeType2 library initialization, which is otherwise extremely difficult
  // to test.
  FreeTypeFont(const std::string& name, size_t size_in_pixels,
               size_t sdf_padding);

  // The destructor is private because all base::Referent classes must have
  // protected or private destructors.
  ~FreeTypeFont() override;

  // Override Font::LoadGlyphGrid to load glyphs on demand.
  bool LoadGlyphGrid(GlyphIndex glyph_index,
                     GlyphGrid* glyph_grid) const override;

 private:
  // Helper class that does most of the work, hiding the implementation.
  class Helper;
  std::shared_ptr<Helper> helper_;
};

// Convenience typedef for shared pointer to a FreeTypeFont.
using FreeTypeFontPtr = base::SharedPtr<FreeTypeFont>;

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_FREETYPEFONT_H_
