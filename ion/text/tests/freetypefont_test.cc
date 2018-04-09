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

#include "ion/text/freetypefont.h"

#include "base/integral_types.h"
#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/base/tests/testallocator.h"
#include "ion/math/vector.h"
#include "ion/text/tests/testfont.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

namespace {

static const CharIndex kPileOfPoo = static_cast<CharIndex>(0x1F4A9);

// This derived FreeTypeFont class exists to test FreeType library
// initialization failure.
class FontWithLibraryFailure : public FreeTypeFont {
 public:
  // Use the FreeTypeFont constructor that simulates failure.
  FontWithLibraryFailure() : FreeTypeFont("LibraryInitFailure", 32U, 4U) {}
};

// Builds and returns a FreeType font.
static const FreeTypeFontPtr BuildFontWithAllocator(const std::string& name,
    size_t size, size_t sdf_padding, const base::AllocatorPtr& alloc) {
  const std::string& data = testing::GetTestFontData();
  return FreeTypeFontPtr(
      new(alloc) FreeTypeFont(name, size, sdf_padding, &data[0], data.size()));
}

// Builds and returns a FreeType font.
static const FreeTypeFontPtr BuildFont(
    const std::string& name, size_t size, size_t sdf_padding) {
  return BuildFontWithAllocator(name, size, sdf_padding, base::AllocatorPtr());
}

// Computes the union of the bounds of all the glyphs in |layout|.
static math::Range2f ComputeTextBounds(const Layout& layout) {
  math::Range2f text_bounds;
  // Make sure all the glyph bounds are in the text bounds.
  for (size_t i = 0; i < layout.GetGlyphCount(); ++i) {
    text_bounds.ExtendByRange(layout.GetGlyph(i).bounds);
  }
  return text_bounds;
}

}  // anonymous namespace

TEST(FreeTypeFontTest, ValidFont) {
  base::LogChecker logchecker;

  FreeTypeFontPtr font = BuildFont("Test", 32U, 4U);
  EXPECT_EQ("Test", font->GetName());
  EXPECT_EQ(32U, font->GetSizeInPixels());
  EXPECT_EQ(4U, font->GetSdfPadding());

  {
    // Valid glyph metrics for the letter 'A'.
    const FreeTypeFont::GlyphMetrics& metrics =
        font->GetGlyphMetrics(font->GetDefaultGlyphForChar('A'));
    EXPECT_FALSE(base::IsInvalidReference(metrics));
    EXPECT_EQ(math::Vector2f(19.f, 23.f), metrics.size);
    EXPECT_EQ(math::Vector2f(1.f, 23.f), metrics.bitmap_offset);
    EXPECT_EQ(math::Vector2f(20.f, 0.f), metrics.advance);
  }

  // Invalid glyph for character with index 1.
  EXPECT_TRUE(base::IsInvalidReference(
      font->GetGlyphGrid(font->GetDefaultGlyphForChar(1))));

  // FontMetrics.
  const Font::FontMetrics& fmet = font->GetFontMetrics();
  EXPECT_EQ(38.f, fmet.line_advance_height);
  EXPECT_NEAR(25.4f, fmet.ascender, 0.1f);

  // Kerning. The test font has some weird values, but these work.
  EXPECT_EQ(math::Vector2f(-1.f, 0.f), font->GetKerning('I', 'X'));
  EXPECT_EQ(math::Vector2f(1.f, 0.f), font->GetKerning('M', 'M'));
}

TEST(FreeTypeFontTest, TrailingWhitespaceAddsGlyphs) {
  const FreeTypeFontPtr font = BuildFont("Test", 32U, 4U);
  const LayoutOptions options;
  const Layout layout = font->BuildLayout("size8   ", options);
  EXPECT_EQ(8U, layout.GetGlyphCount());
}

TEST(FreeTypeFontTest, LayoutOptionsPixelPerfect) {
  const FreeTypeFontPtr font = BuildFont("Test", 32U, 4U);
  LayoutOptions options;

  // Specify neither width nor height. Width and height of layout will be their
  // natural size in pixels based on the chosen font.
  options.target_size = math::Vector2f::Zero();

  // Test one line of text.
  const math::Range2f single_line_text_bounds = ComputeTextBounds(
        font->BuildLayout("Testy test", options));
  // Check sizes against golden values.
  EXPECT_FLOAT_EQ(137.0f, single_line_text_bounds.GetSize()[0]);
  EXPECT_FLOAT_EQ(31.0f, single_line_text_bounds.GetSize()[1]);

  // Test several lines of text.
  const math::Range2f multi_line_text_bounds = ComputeTextBounds(
        font->BuildLayout("Test\nthree\nlines", options));
  // Check sizes against golden values.
  EXPECT_FLOAT_EQ(69.0f, multi_line_text_bounds.GetSize()[0]);
  EXPECT_FLOAT_EQ(100.0f, multi_line_text_bounds.GetSize()[1]);
}

TEST(FreeTypeFontTest, BuildLayoutWithSpacing) {
  const FreeTypeFontPtr font = BuildFont("Testdf", 32U, 0U);
  LayoutOptions options;
  Layout no_spacing = font->BuildLayout("abc", options);
  EXPECT_EQ(3U, no_spacing.GetGlyphCount());
  // Test only the x-coordinate of the lower left point of glyph's quad.
  EXPECT_FLOAT_EQ(0.03125f, no_spacing.GetGlyph(0).quad.points[0][0]);
  EXPECT_FLOAT_EQ(0.5625f, no_spacing.GetGlyph(1).quad.points[0][0]);
  EXPECT_FLOAT_EQ(1.03125f, no_spacing.GetGlyph(2).quad.points[0][0]);
  // The same text with additional horizontal spacing between glyphs
  // (3 physical pixels).
  options.glyph_spacing = 3;
  Layout spacing = font->BuildLayout("abc", options);
  EXPECT_EQ(3U, spacing.GetGlyphCount());
  EXPECT_FLOAT_EQ(0.03125f, spacing.GetGlyph(0).quad.points[0][0]);
  EXPECT_FLOAT_EQ(0.65625f, spacing.GetGlyph(1).quad.points[0][0]);
  EXPECT_FLOAT_EQ(1.21875f, spacing.GetGlyph(2).quad.points[0][0]);
}

TEST(FreeTypeFontTest, ValidBitmapFont) {
  base::LogChecker logchecker;

  const std::string& data = testing::GetEmojiFontData();
  FreeTypeFontPtr font(
      new FreeTypeFont("Emoji", 32U, 4U, &data[0], data.size()));
  EXPECT_EQ("Emoji", font->GetName());

  {
    // Can attempt to load GlyphMetrics for an emoji without crashing.
    const FreeTypeFont::GlyphMetrics& metrics =
        font->GetGlyphMetrics(font->GetDefaultGlyphForChar(kPileOfPoo));
    (void)metrics;
    // 
    // EXPECT_FALSE(base::IsInvalidReference(metrics));
    // EXPECT_EQ(math::Vector2f(19.f, 23.f), metrics.size);
    // EXPECT_EQ(math::Vector2f(1.f, 23.f), metrics.bitmap_offset);
    // EXPECT_EQ(math::Vector2f(20.f, 0.f), metrics.advance);
  }

  // FontMetrics.
  const Font::FontMetrics& fmet = font->GetFontMetrics();
  EXPECT_EQ(132.f, fmet.line_advance_height);
  EXPECT_NEAR(24.3f, fmet.ascender, 0.1f);
}

// 
TEST(FreeTypeFontTest, FallbackFont) {
  base::LogChecker logchecker;
  CharIndex katakana_ka = static_cast<CharIndex>(0x30AB);
  FreeTypeFontPtr font = BuildFont("Test", 32U, 4U);

  // The test font does not have the katakana character ka, and gives zero for
  // a kerning vector containing it.
  EXPECT_TRUE(base::IsInvalidReference(
      font->GetGlyphGrid(font->GetDefaultGlyphForChar(katakana_ka))));
  EXPECT_TRUE(base::IsInvalidReference(
      font->GetGlyphMetrics(font->GetDefaultGlyphForChar(katakana_ka))));
  EXPECT_EQ(math::Vector2f::Zero(),
            font->GetKerning(katakana_ka, katakana_ka));

  // Create the CJK font which does have the katakana character ka.
  const std::string& data = testing::GetCJKFontData();
  FreeTypeFontPtr fallback(
      new FreeTypeFont("CJK", 32U, 4U, &data[0], data.size()));

  // Add the CJK font as a fallback, and verify that the katakana character ka
  // is now available.
  font->AddFallbackFont(fallback);
  {
    const FreeTypeFont::GlyphMetrics& metrics1 =
        font->GetGlyphMetrics(font->GetDefaultGlyphForChar(katakana_ka));
    const FreeTypeFont::GlyphMetrics& metrics2 = fallback->GetGlyphMetrics(
        fallback->GetDefaultGlyphForChar(katakana_ka));
    EXPECT_FALSE(base::IsInvalidReference(metrics1));
    EXPECT_FALSE(base::IsInvalidReference(metrics2));
    EXPECT_EQ(metrics1.size, metrics2.size);
    EXPECT_EQ(metrics1.bitmap_offset, metrics2.bitmap_offset);
    EXPECT_EQ(metrics1.advance, metrics2.advance);
  }

  // Check that kerning values are correct. For a pair that contains a character
  // in the main font and the fallback we expect a kern vector of zero, for a
  // pair that are both only in the fallback we expect the fallback's kern
  // vector.
  EXPECT_EQ(math::Vector2f::Zero(), font->GetKerning('I', katakana_ka));
  EXPECT_EQ(fallback->GetKerning(katakana_ka, katakana_ka),
            font->GetKerning(katakana_ka, katakana_ka));
}

TEST(FreeTypeFontTest, LibraryInitFailure) {
  // Simulate library initialization failure, which is otherwise very hard to
  // test.
  base::LogChecker logchecker;
  FreeTypeFontPtr font(new FontWithLibraryFailure());
  EXPECT_TRUE(logchecker.HasMessage(
      "ERROR", "Could not initialize the FreeType library"));
}

TEST(FreeTypeFontTest, LoadFaceFailure) {
  base::LogChecker logchecker;

  // Try loading from invalid data.
  uint8 bad_data[4] = { 0xff, 0xff, 0xff, 0xff };
  FreeTypeFontPtr font(new FreeTypeFont("Test", 32U, 4U, bad_data, 4U));
  EXPECT_TRUE(logchecker.HasMessage(
      "ERROR", "Could not read the FreeType font data"));
}

TEST(FreeTypeFontTest, FontsWithDifferentAllocators) {
  base::testing::TestAllocatorPtr alloc1(new base::testing::TestAllocator);
  base::testing::TestAllocatorPtr alloc2(new base::testing::TestAllocator);
  base::AllocationTrackerPtr tracker1 = alloc1->GetTracker();
  base::AllocationTrackerPtr tracker2 = alloc2->GetTracker();

  // Used to verify that the FullAllocationTrackers in the TestAllocators
  // log no error messages.
  ion::base::LogChecker log_checker;

  // Both allocators have allocated zero bytes.
  EXPECT_EQ(0U, tracker1->GetActiveAllocationBytesCount());
  EXPECT_EQ(0U, tracker2->GetActiveAllocationBytesCount());

  // After building a font with alloc1, it will have > 0 bytes allocated,
  // and alloc2 will still have 0 bytes allocated.
  FreeTypeFontPtr font1 = BuildFontWithAllocator("Test", 32U, 4U, alloc1);
  EXPECT_LT(0U, tracker1->GetActiveAllocationBytesCount());
  EXPECT_EQ(0U, tracker2->GetActiveAllocationBytesCount());

  // After building a font with alloc2, it should now have the same number of
  // bytes allocated as alloc1.
  FreeTypeFontPtr font2 = BuildFontWithAllocator("Test", 32U, 4U, alloc2);
  EXPECT_LT(0U, tracker1->GetActiveAllocationBytesCount());
  EXPECT_EQ(tracker1->GetActiveAllocationBytesCount(),
            tracker2->GetActiveAllocationBytesCount());

  // After freeing font1, alloc1 should have fewer allocated bytes than alloc2,
  // and after freeing font2 they should be the same again.
  font1.Reset();
  EXPECT_LT(tracker1->GetActiveAllocationBytesCount(),
            tracker2->GetActiveAllocationBytesCount());
  font2.Reset();
  EXPECT_EQ(tracker1->GetActiveAllocationBytesCount(),
            tracker2->GetActiveAllocationBytesCount());

  // Log should be empty.
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

}  // namespace text
}  // namespace ion
