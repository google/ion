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

#ifndef ION_TEXT_TESTS_MOCKFONT_H_
#define ION_TEXT_TESTS_MOCKFONT_H_

#include "ion/text/font.h"

namespace ion {
namespace text {
namespace testing {

// MockFont is a version of Font that allows metrics to be installed directly
// for testing text layout. It installs glyphs for the 'A', 'b', 'g', '.', '#',
// division sign (Unicode U+00F7, UTF-8 c3 b7) and space characters with
// reasonable metrics for each. The '#' character is defined as the maximum
// width and height for the font for easy testing. The space character has zero
// size.
class MockFont : public Font {
 public:
  // The constructor sets the font size and SDF padding value.
  MockFont(size_t size_px, size_t sdf_padding)
      : Font("MockFont", size_px, sdf_padding) {
    AddAllGlyphData();
    FontMetrics metrics;
    metrics.line_advance_height = static_cast<float>(size_px) * 2.f;
    metrics.ascender = static_cast<float>(size_px) * 0.8f;;
    SetFontMetrics(metrics);
  }

  GlyphIndex GetDefaultGlyphForChar(CharIndex char_index) const override {
    const auto& it = char_map_.find(char_index);
    return it == char_map_.end() ? 0 : it->second;
  }

  const Layout BuildLayout(const std::string& text,
                           const LayoutOptions& options) const override {
    return Layout();
  }

  void AddFallbackFont(const FontPtr& fallback) override {}

 private:
  void AddGlyphForChar(CharIndex char_index, const GlyphGrid& grid) {
    // Glyph index of "0" is reserved for "invalid" by most font systems, so we
    // mimic that.
    const GlyphIndex glyph_index =
        static_cast<GlyphIndex>(char_map_.size()) + 1;
    const bool inserted =
        char_map_.insert(std::make_pair(char_index, glyph_index)).second;
    DCHECK(inserted);
    AddGlyph(glyph_index, grid);
  }

  void AddAllGlyphData() {
    AddGlyphForChar('A', BuildGlyphGrid(50.f, 80.f));
    AddGlyphForChar('b', BuildGlyphGrid(40.f, 76.f));
    AddGlyphForChar('g', BuildGlyphGrid(40.f, 60.f));
    AddGlyphForChar('.', BuildGlyphGrid(4.f, 4.f));
    // '#' has maximum width and height.
    AddGlyphForChar('#', BuildGlyphGrid(50.f, 100.f));
    // Division sign.
    AddGlyphForChar(0x00f7, BuildGlyphGrid(50.f, 40.f));
    // Space
    AddGlyphForChar(' ', BuildGlyphGrid(0.0f, 0.0f));
  }

  static const GlyphGrid BuildGlyphGrid(
      float x_size, float y_size) {
    GlyphGrid gd;
    gd.pixels = base::Array2<double>(static_cast<size_t>(x_size),
                                     static_cast<size_t>(y_size), 0.5);
    return gd;
  }

  std::map<CharIndex, GlyphIndex> char_map_;
};

// Convenience typedef for shared pointer to a MockFont.
using MockFontPtr = base::SharedPtr<MockFont>;

}  // namespace testing
}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_TESTS_MOCKFONT_H_
