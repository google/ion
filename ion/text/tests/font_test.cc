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

#include "ion/text/font.h"

#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/text/tests/mockfont.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

namespace {

// This derived Font class allows a concrete instance to be constructed.
class TestFont : public Font {
 public:
  TestFont(const std::string& name, size_t size_in_pixels, size_t sdf_padding)
      : Font(name, size_in_pixels, sdf_padding) {}

  void AddGlyphGrid(CharIndex char_index, const base::Array2<double>& pixels) {
    // Glyph index of "0" is reserved for "invalid" by most font systems, so we
    // mimic that.
    const GlyphIndex glyph_index = static_cast<GlyphIndex>(charmap_.size()) + 1;
    const bool inserted =
        charmap_.insert(std::make_pair(char_index, glyph_index)).second;
    DCHECK(inserted);
    GlyphGrid grid;
    grid.pixels = pixels;
    AddGlyph(glyph_index, grid);
  }

  GlyphIndex GetDefaultGlyphForChar(CharIndex char_index) const override {
    auto it = charmap_.find(char_index);
    return it == charmap_.end() ? 0 : it->second;
  }

  const Layout BuildLayout(const std::string& text,
                           const LayoutOptions& options) const override {
    return Layout();
  }

  void AddFallbackFont(const FontPtr& fallback) override {}

  const GlyphGrid& GetGlyphGridForChar(CharIndex char_index) const {
    const GlyphIndex glyph_index = GetDefaultGlyphForChar(char_index);
    if (glyph_index == 0) return base::InvalidReference<Font::GlyphGrid>();
    return GetGlyphGrid(glyph_index);
  }

  // Expose this function for testing.
  using Font::CacheSdfGrid;

  std::map<CharIndex, GlyphIndex> charmap_;
};
using TestFontPtr = base::SharedPtr<TestFont>;

}  // anonymous namespace

TEST(FontTest, Font) {
  const std::string kName("myFontName");
  const size_t kSize = 16U;
  const size_t kPadding = 4U;

  // Construct the Font, but do not add glyphs.
  TestFontPtr font(new TestFont(kName, kSize, kPadding));

  // Test default values.
  EXPECT_EQ(kName, font->GetName());
  EXPECT_EQ(kSize, font->GetSizeInPixels());
  EXPECT_EQ(kPadding, font->GetSdfPadding());

  // Add the glyphs.
  font->AddGlyphGrid(13, base::Array2<double>(14U, 45U, 0.3));
  font->AddGlyphGrid(41, base::Array2<double>(42U, 29U, 0.1));
  EXPECT_EQ(kName, font->GetName());
  EXPECT_EQ(kSize, font->GetSizeInPixels());
  EXPECT_EQ(kPadding, font->GetSdfPadding());

  ASSERT_FALSE(base::IsInvalidReference(font->GetGlyphGridForChar(13)));
  EXPECT_EQ(0.3, font->GetGlyphGridForChar(13).pixels.Get(2, 3));
  EXPECT_FALSE(font->GetGlyphGridForChar(13).is_sdf);

  ASSERT_FALSE(base::IsInvalidReference(font->GetGlyphGridForChar(41)));
  EXPECT_EQ(0.1, font->GetGlyphGridForChar(41).pixels.Get(2, 3));
  EXPECT_FALSE(font->GetGlyphGridForChar(41).is_sdf);

  EXPECT_TRUE(base::IsInvalidReference(font->GetGlyphGridForChar(0)));
  EXPECT_TRUE(base::IsInvalidReference(font->GetGlyphGridForChar(12)));
  EXPECT_TRUE(base::IsInvalidReference(font->GetGlyphGridForChar(256)));
  EXPECT_TRUE(base::IsInvalidReference(font->GetGlyphGridForChar(1000)));

  // Set SDF grid.
  const GlyphIndex g13 = font->GetDefaultGlyphForChar(13);
  EXPECT_TRUE(font->CacheSdfGrid(g13, base::Array2<double>(14U, 45U, -0.8)));
  EXPECT_EQ(-0.8, font->GetGlyphGridForChar(13).pixels.Get(2, 3));
  EXPECT_TRUE(font->GetGlyphGridForChar(13).is_sdf);

  // Test failure to modify SDF grid.
  base::LogChecker log_checker;

  // Index does not refer to a valid glyph.
  const GlyphIndex g10 = font->GetDefaultGlyphForChar(10);
  EXPECT_FALSE(font->CacheSdfGrid(g10, base::Array2<double>(10U, 12U, -0.2)));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Invalid glyph"));

  // Already set an SDF grid.
  EXPECT_FALSE(font->CacheSdfGrid(g13, base::Array2<double>(14U, 45U, -0.2)));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Grid is already an SDF grid"));
}

TEST(FontTest, AddGlyphsForAsciiCharacterRange) {
  FontPtr font(new testing::MockFont(32U, 0U));
  GlyphSet glyphs(ion::base::AllocatorPtr(nullptr));
  font->AddGlyphsForAsciiCharacterRange(1, 127, &glyphs);
  // 6 of the 7 glyphs in MockFont are in the ASCII range, with DIVISION SIGN
  // being outside it.
  EXPECT_EQ(glyphs.size(), 6U);

  glyphs.clear();
  font->AddGlyphsForAsciiCharacterRange('a', 'z', &glyphs);
  ASSERT_EQ(glyphs.size(), 2U);  // Only 'b' and 'g'.
  EXPECT_TRUE(glyphs.count(font->GetDefaultGlyphForChar('b')));
  EXPECT_TRUE(glyphs.count(font->GetDefaultGlyphForChar('g')));
}

}  // namespace text
}  // namespace ion
