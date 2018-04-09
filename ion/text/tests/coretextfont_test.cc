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

#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)

#include "ion/text/coretextfont.h"

#include "base/integral_types.h"
#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/math/vector.h"
#include "ion/text/tests/testfont.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

// Returns the sum of all the values in |array|.
static double SumArrayValues(base::Array2<double> array) {
  double result = 0;
  for (size_t y = 0; y < array.GetHeight(); ++y) {
    for (size_t x = 0; x < array.GetWidth(); ++x) {
      result += array.Get(x, y);
    }
  }
  return result;
}

// Computes the union of the bounds of all the glyphs in |layout|.
static math::Range2f ComputeTextBounds(const Layout& layout) {
  math::Range2f text_bounds;
  for (size_t i = 0; i < layout.GetGlyphCount(); ++i) {
    text_bounds.ExtendByRange(layout.GetGlyph(i).bounds);
  }
  return text_bounds;
}

TEST(CoreTextFontTest, ValidSystemFont) {
  base::LogChecker logchecker;

  const FontPtr font(new CoreTextFont("Courier", 32U, 4U, nullptr, 0));
  EXPECT_EQ("Courier", font->GetName());
  EXPECT_EQ(32U, font->GetSizeInPixels());
  EXPECT_EQ(4U, font->GetSdfPadding());

  {
    // Valid glyph for the letter 'A'.
    const Font::GlyphGrid& grid =
        font->GetGlyphGrid(font->GetDefaultGlyphForChar('A'));
    EXPECT_FALSE(base::IsInvalidReference(grid));
    EXPECT_EQ(20U, grid.pixels.GetWidth());
    EXPECT_EQ(19U, grid.pixels.GetHeight());
  }

  // Invalid glyph for character with index 1.
  EXPECT_TRUE(base::IsInvalidReference(
      font->GetGlyphGrid(font->GetDefaultGlyphForChar(1))));

  // FontMetrics.
  const Font::FontMetrics& fmet = font->GetFontMetrics();
  EXPECT_EQ(32.f, fmet.line_advance_height);
  EXPECT_NEAR(24.f, fmet.ascender, 0.1f);
}

TEST(CoreTextFontTest, TrailingWhitespaceAddsGlyphs) {
  const FontPtr font(new CoreTextFont("Courier", 32U, 4U, nullptr, 0));
  const LayoutOptions options;
  const Layout layout = font->BuildLayout("size8   ", options);
  EXPECT_EQ(8U, layout.GetGlyphCount());
}

TEST(CoreTextFontTest, WhitespaceHasValidQuad) {
  const FontPtr font(new CoreTextFont("Courier", 32U, 4U, nullptr, 0));
  const LayoutOptions options;
  const Layout layout = font->BuildLayout("foo bar", options);
  EXPECT_EQ(7U, layout.GetGlyphCount());
  const Layout::Glyph& space = layout.GetGlyph(3);
  EXPECT_EQ(0.f, space.bounds.GetSize()[0]);
  EXPECT_EQ(0.f, space.bounds.GetSize()[1]);
  EXPECT_FALSE(std::isnan(space.quad.points[0][0]));
  EXPECT_FALSE(std::isnan(space.quad.points[0][1]));
}

TEST(CoreTextFontTest, LayoutOptionsPixelPerfect) {
  const CoreTextFontPtr font = testing::BuildTestCoreTextFont("Test", 32U, 4U);
  LayoutOptions options;

  // Specify neither width nor height. Width and height of layout will be their
  // natural size in pixels based on the chosen font.
  options.target_size = math::Vector2f::Zero();

  // Test one line of text.
  const math::Range2f single_line_text_bounds = ComputeTextBounds(
        font->BuildLayout("Testy test", options));
  // Check sizes against golden values.
  EXPECT_FLOAT_EQ(133.14062f, single_line_text_bounds.GetSize()[0]);
  EXPECT_FLOAT_EQ(29.75f, single_line_text_bounds.GetSize()[1]);

  // Test several lines of text.
  const math::Range2f multi_line_text_bounds = ComputeTextBounds(
        font->BuildLayout("Test\nthree\nlines", options));
  // Check sizes against golden values.
  EXPECT_FLOAT_EQ(67.859375f, multi_line_text_bounds.GetSize()[0]);
  EXPECT_FLOAT_EQ(99.203125f, multi_line_text_bounds.GetSize()[1]);
}

TEST(CoreTextFontTest, ValidFontWithData) {
  base::LogChecker logchecker;

  const CoreTextFontPtr font = testing::BuildTestCoreTextFont("Test", 32U, 4U);
  EXPECT_EQ("Test", font->GetName());
  EXPECT_EQ("Tuffy Regular", font->GetCTFontName());
  EXPECT_EQ(32U, font->GetSizeInPixels());
  EXPECT_EQ(4U, font->GetSdfPadding());

  {
    // Valid glyph for the letter 'A'.
    const Font::GlyphGrid& grid =
        font->GetGlyphGrid(font->GetDefaultGlyphForChar('A'));
    EXPECT_EQ(19U, grid.pixels.GetWidth());
    EXPECT_EQ(23U, grid.pixels.GetHeight());
  }

  // Invalid glyph for character with index 1.
  EXPECT_TRUE(base::IsInvalidReference(
      font->GetGlyphGrid(font->GetDefaultGlyphForChar(1))));

  // FontMetrics.
  const Font::FontMetrics& fmet = font->GetFontMetrics();
  EXPECT_EQ(38.f, fmet.line_advance_height);
  EXPECT_NEAR(25.2f, fmet.ascender, 0.1f);

  // Verify that font can render non-Latin script through font fallback.
  EXPECT_EQ(font->BuildLayout("मारग", LayoutOptions()).GetGlyphCount(), 4U);

  // The following string fails to generate a frame. This appears to be invalid
  // UTF8 encoding. This should not crash.
  // 
  // CoreTextFont::Helper::CreateFrame.
  size_t glyphCount =
      font->BuildLayout("Новая Басманна\321", LayoutOptions()).GetGlyphCount();
  EXPECT_EQ(glyphCount, 0U);
  EXPECT_TRUE(
      logchecker.HasMessage("ERROR",
                            "CreateFrame failed on: Новая Басманна\321"));
}

TEST(CoreTextFontTest, KnownFontSuffixes) {
  // Tests the fonts produced by suffixing the font name "HelveticaNeue" with
  // "-Bold", "-Italic", and "-BoldItalic".
  base::LogChecker logchecker;
  const std::string kBaseName = "HelveticaNeue";

  const FontPtr vanilla_font(
      new CoreTextFont(kBaseName, 32U, 0U, nullptr, 0));
  const FontPtr bold_font(
      new CoreTextFont(kBaseName + "-Bold", 32U, 0U, nullptr, 0));
  const FontPtr italic_font(
      new CoreTextFont(kBaseName + "-Italic", 32U, 0U, nullptr, 0));
  const FontPtr bold_italic_font(
      new CoreTextFont(kBaseName + "-BoldItalic", 32U, 0U, nullptr, 0));

  EXPECT_EQ(kBaseName, vanilla_font->GetName());

  // Get the grid for the letter 'l' in the base font.
  const Font::GlyphGrid& vanilla_grid =
      vanilla_font->GetGlyphGrid(vanilla_font->GetDefaultGlyphForChar('l'));

  // Bold l's pixels should be in total slightly darker.
  const Font::GlyphGrid& bold_grid =
      bold_font->GetGlyphGrid(bold_font->GetDefaultGlyphForChar('l'));
  EXPECT_GT(SumArrayValues(bold_grid.pixels),
            SumArrayValues(vanilla_grid.pixels) * 1.3);

  // Italic l should be wider.
  const Font::GlyphGrid& italic_grid =
      italic_font->GetGlyphGrid(italic_font->GetDefaultGlyphForChar('l'));

  EXPECT_GT(italic_grid.pixels.GetWidth(), vanilla_grid.pixels.GetWidth());

  // Bold-italic l should be darker and wider.
  const Font::GlyphGrid& bold_italic_grid = bold_italic_font->GetGlyphGrid(
      bold_italic_font->GetDefaultGlyphForChar('l'));
  EXPECT_GT(bold_italic_grid.pixels.GetWidth(),
            vanilla_grid.pixels.GetWidth());
  EXPECT_GT(SumArrayValues(bold_italic_grid.pixels),
            SumArrayValues(italic_grid.pixels) * 1.3);
}

TEST(CoreTextFontTest, UnknownFontSuffixes) {
  // Demonstrates that appending a suffix when requesting an unknown system font
  // name doesn't change which system font is used.
  base::LogChecker logchecker;
  const std::string kBaseName = "abcdef";

  const FontPtr vanilla_font(
      new CoreTextFont(kBaseName, 32U, 0U, nullptr, 0));
  const FontPtr italic_font(
      new CoreTextFont(kBaseName + "-Italic", 32U, 0U, nullptr, 0));

  EXPECT_EQ(kBaseName, vanilla_font->GetName());

  const Font::GlyphGrid& vanilla_grid =
      vanilla_font->GetGlyphGrid(vanilla_font->GetDefaultGlyphForChar('l'));
  const Font::GlyphGrid& italic_grid =
      italic_font->GetGlyphGrid(italic_font->GetDefaultGlyphForChar('l'));

  EXPECT_EQ(italic_grid.pixels.GetWidth(), vanilla_grid.pixels.GetWidth());
  EXPECT_EQ(SumArrayValues(vanilla_grid.pixels),
            SumArrayValues(italic_grid.pixels));
}

}  // namespace text
}  // namespace ion

#endif  // defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
