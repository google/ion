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

// This file tests the behavior of all concrete font systems available on the
// current platform, i.e. FreeTypeFont (with or without ICU), CoreTextFont on
// Mac / iOS, and not the mock fonts.

#include "base/integral_types.h"
#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/math/rangeutils.h"
#include "ion/math/vector.h"
#include "ion/port/environment.h"
#include "ion/port/fileutils.h"
#include "ion/text/font.h"
#include "ion/text/tests/testfont.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

using math::Point2f;
using math::Point3f;
using math::Range2f;
using math::Vector2f;
using math::Vector2ui;

const std::string GetIonDirectory() {

  std::string cwd = port::GetCurrentWorkingDirectory();
  size_t ion = cwd.rfind("ion/");
  if (ion != std::string::npos) {
    return cwd.substr(0, ion + 3);
  }
  return "";
}

// This returns the available font classes with a simple Latin font.
std::vector<FontPtr> SimpleTestFonts(size_t sdf_padding) {
  std::vector<FontPtr> fonts;
  fonts.push_back(testing::BuildTestFreeTypeFont("Test", 32U, sdf_padding));
#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
  fonts.push_back(testing::BuildTestCoreTextFont("Test", 32U, sdf_padding));
#endif
  return fonts;
}

// This returns the available font classes with a more complex font (Hindi).
std::vector<FontPtr> ComplexTestFonts() {
  std::vector<FontPtr> fonts;
#if defined(ION_USE_ICU)
  auto test = testing::BuildTestFreeTypeFont("Test", 32U, 4U);
  auto devangari =
      testing::BuildTestFreeTypeFont("NotoSansDevanagari-Regular", 32U, 4U);
  // With a proper fallback set even a simple FreeType font should work.
  test->AddFallbackFont(devangari);
  fonts.push_back(test);
  fonts.push_back(devangari);
#endif
#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
  fonts.push_back(
      testing::BuildTestCoreTextFont("NotoSansDevanagari-Regular", 32U, 4U));
  // As CoreText fonts perform font fallback, even the basic test font should
  // be capable of complex layout.
  fonts.push_back(testing::BuildTestCoreTextFont("Test", 32U, 4U));
#endif
  return fonts;
}

// Returns whether two points are within a certain range of each other in X and
// Y, and the third axis is near the XY plane.
static bool PointXYNear(const Point3f& a, const Point2f& b) {
  // Quite a large error, as this is comparing two different font
  // implementations.
  static const float kError = 0.03f;
  if (fabsf(a[0] - b[0]) < kError &&
      fabsf(a[1] - b[1]) < kError &&
      fabsf(a[2]) < kError) {
    return true;
  }
  return false;
}

// Returns whether a glyph's quad is close to a certain shape.
static bool GlyphQuadNear(const Layout::Glyph& glyph,
                          float xmin, float xmax, float ymin, float ymax) {
  return PointXYNear(glyph.quad.points[0], Point2f(xmin, ymin)) &&
         PointXYNear(glyph.quad.points[1], Point2f(xmax, ymin)) &&
         PointXYNear(glyph.quad.points[2], Point2f(xmax, ymax)) &&
         PointXYNear(glyph.quad.points[3], Point2f(xmin, ymax));
}

// Checks that the layout's Position and Size are close to expectations.
static void CheckPositionAndSize(const Layout& layout, const Point2f& position,
                                 const Vector2f& size) {
  // Quite a large error, as this is comparing two different font
  // implementations.
  static const float kError = 0.03f;
  EXPECT_NEAR(position[0], layout.GetPosition()[0], kError);
  EXPECT_NEAR(position[1], layout.GetPosition()[1], kError);
  EXPECT_NEAR(size[0], layout.GetSize()[0], kError);
  EXPECT_NEAR(size[1], layout.GetSize()[1], kError);
}

TEST(PlatformFontTest, ValidFont) {
  base::LogChecker logchecker;
  std::vector<FontPtr> fonts = SimpleTestFonts(4);
  for (const FontPtr& font : fonts) {
    EXPECT_EQ("Test", font->GetName());
    EXPECT_EQ(32U, font->GetSizeInPixels());
    EXPECT_EQ(4U, font->GetSdfPadding());

    // FontMetrics.
    const Font::FontMetrics& fmet = font->GetFontMetrics();
    EXPECT_EQ(38.f, fmet.line_advance_height);
    EXPECT_NEAR(25.4f, fmet.ascender, 0.2f);
  }
}

TEST(PlatformFontTest, GlyphGrid) {
  // Tests basic properties of the GlyphGrid for a few characters.
  base::LogChecker logchecker;
  std::vector<FontPtr> fonts = SimpleTestFonts(4);
  for (const FontPtr& font : fonts) {
    {
      // Valid glyph for the letter 'A'.
      const Font::GlyphGrid& grid =
          font->GetGlyphGrid(font->GetDefaultGlyphForChar('A'));
      EXPECT_EQ(19U, grid.pixels.GetWidth());
      EXPECT_EQ(23U, grid.pixels.GetHeight());
    }

    {
      // Valid glyph for the letter 'g'.
      const Font::GlyphGrid& grid =
          font->GetGlyphGrid(font->GetDefaultGlyphForChar('g'));
      EXPECT_EQ(14U, grid.pixels.GetWidth());
      // The grid is slightly different size between FreeType and CoreText.
      EXPECT_NEAR(25.0, static_cast<double>(grid.pixels.GetHeight()), 1.0);
    }

    // Invalid glyph for character with index 1.
    EXPECT_TRUE(base::IsInvalidReference(
        font->GetGlyphGrid(font->GetDefaultGlyphForChar(1))));
  }
}

TEST(PlatformFontTest, SimpleLayout) {
  // Lays out a simple line of text and checks all fields in the layout contain
  // sane values. The allowed error bounds reflect the differences between
  // CoreText and FreeType font rendering.
  std::vector<FontPtr> fonts = SimpleTestFonts(4);
  for (const FontPtr& font : fonts) {
    LayoutOptions options;
    Layout l = font->BuildLayout("Abcd", options);

    EXPECT_EQ(l.GetGlyphCount(), 4U);

    EXPECT_TRUE(GlyphQuadNear(l.GetGlyph(0), -0.09f, 0.75f, -0.12f, 0.84f));
    EXPECT_TRUE(GlyphQuadNear(l.GetGlyph(1), 0.56f, 1.25f, -0.16f, 0.84f));
    EXPECT_TRUE(GlyphQuadNear(l.GetGlyph(2), 1.03f, 1.69f, -0.16f,  0.68f));
    EXPECT_TRUE(GlyphQuadNear(l.GetGlyph(3), 1.48f, 2.16f, -0.15f, 0.84f));

    CheckPositionAndSize(l, Point2f(0.f, -0.03f), Vector2f(2.03f, 0.75f));

    EXPECT_EQ(1.1875f, l.GetLineAdvanceHeight());
  }
}

TEST(PlatformFontTest, MetricsBasedAlignment) {
  // Lays out a simple line of text and checks position and size after using
  // metrics_based_alignment. The allowed error bounds reflect the differences
  // between CoreText and FreeType font rendering.
  std::vector<FontPtr> fonts = SimpleTestFonts(4);
  for (const FontPtr& font : fonts) {
    LayoutOptions options;
    options.metrics_based_alignment = true;
    Layout l = font->BuildLayout("Abcd", options);

    EXPECT_EQ(l.GetGlyphCount(), 4U);

    // Glyphs didnt change size.
    EXPECT_TRUE(GlyphQuadNear(l.GetGlyph(0), -0.09f, 0.75f, -0.12f, 0.84f));
    EXPECT_TRUE(GlyphQuadNear(l.GetGlyph(1), 0.56f, 1.25f, -0.16f, 0.84f));
    EXPECT_TRUE(GlyphQuadNear(l.GetGlyph(2), 1.03f, 1.69f, -0.16f,  0.68f));
    EXPECT_TRUE(GlyphQuadNear(l.GetGlyph(3), 1.48f, 2.16f, -0.15f, 0.84f));

    // Width uses advance instead of bitmap_offset + size;
    // Height uses full ascender and descender that add up to font size.
    CheckPositionAndSize(l, Point2f(0.f, -0.20f), Vector2f(2.08f, 1.00f));

    EXPECT_EQ(1.1875f, l.GetLineAdvanceHeight());
  }
}

TEST(PlatformFontTest, TargetSize) {
  // This builds a Layout that uses the full height and width of each glyph and
  // checks the bounds of the resulting text rectangle to verify the scale.
  std::vector<FontPtr> fonts = SimpleTestFonts(4);
  for (const FontPtr& font : fonts) {
    LayoutOptions options;
    options.horizontal_alignment = kAlignLeft;
    options.vertical_alignment = kAlignBaseline;

    // Any negative size should fail to layout.
    LayoutOptions invalid_options = options;
    invalid_options.target_size.Set(-200.f, 0.0f);
    EXPECT_EQ(font->BuildLayout("####", invalid_options).GetGlyphCount(), 0u);
    invalid_options.target_size.Set(0.f, -200.0f);
    EXPECT_EQ(font->BuildLayout("####", invalid_options).GetGlyphCount(), 0u);

    // Scale specified as width only.
    options.target_size.Set(200.f, 0.0f);
    Layout first_layout = font->BuildLayout("####", options);

    // Scale 2x in both dimensions.
    options.target_size[0] *= 2.0f;
    EXPECT_NEAR(
        font->BuildLayout("####", options).GetGlyph(0).quad.points[2][0],
        first_layout.GetGlyph(0).quad.points[2][0] * 2,
        0.001);

    // Scale specified as height only.
    options.target_size.Set(0.0f, 1000.0f);
    Layout layout_1000 = font->BuildLayout("####", options);
    EXPECT_NEAR(layout_1000.GetGlyph(0).quad.points[2][0],
                first_layout.GetGlyph(0).quad.points[2][0] * 15.625,
                40);

    // Non-uniform scale, should match the width-only and height-only layouts in
    // the appropriate axes.
    options.target_size.Set(200.f, 1000.f);
    Layout layout_non_uniform = font->BuildLayout("####", options);
    EXPECT_NEAR(layout_non_uniform.GetGlyph(0).quad.points[2][0],
                first_layout.GetGlyph(0).quad.points[2][0],
                0.001);
    EXPECT_NEAR(layout_non_uniform.GetGlyph(0).quad.points[2][1],
                layout_1000.GetGlyph(0).quad.points[2][1],
                0.001);
  }
}

TEST(PlatformFontTest, TargetPoint) {
  // Verify the behavior of LayoutOptions::target_point.
  std::vector<FontPtr> fonts = SimpleTestFonts(4);
  for (const FontPtr& font : fonts) {
    LayoutOptions options;
    options.horizontal_alignment = kAlignLeft;
    options.vertical_alignment = kAlignBaseline;
    // Set a scale to verify that target_point is applied after target_size.
    options.target_size.Set(200.f, 1000.f);

    Layout first_layout = font->BuildLayout("####", options);
    options.target_point = Point2f(70.f, 300.f);
    Layout offset_layout = font->BuildLayout("####", options);

    EXPECT_NEAR(offset_layout.GetGlyph(0).quad.points[2][0],
                first_layout.GetGlyph(0).quad.points[2][0] + 70.f,
                0.001);
    EXPECT_NEAR(offset_layout.GetGlyph(0).quad.points[2][1],
                first_layout.GetGlyph(0).quad.points[2][1] + 300.f,
                0.001);
  }
}

TEST(PlatformFontTest, LineSpacing) {
  // Verify the behavior of LayoutOptions::line_spacing.
  std::vector<FontPtr> fonts = SimpleTestFonts(4);
  for (const FontPtr& font : fonts) {
    LayoutOptions options;

    // Create two multiline layouts, one with vanilla line_spacing (1) and the
    // other with line_spacing 3.
    Layout layout1 = font->BuildLayout("####\n####", options);
    options.line_spacing = 3.f;
    Layout layout3 = font->BuildLayout("####\n####", options);

    // Back-calculate line spacing by dividing line height by glyph height.
    float glyph_height1 = layout1.GetGlyph(6).quad.points[0][1] -
        layout1.GetGlyph(6).quad.points[3][1];

    float glyph_height3 = layout3.GetGlyph(6).quad.points[0][1] -
        layout3.GetGlyph(6).quad.points[3][1];

    float line_height1 = layout1.GetGlyph(6).quad.points[0][1] -
        layout1.GetGlyph(0).quad.points[0][1];

    float line_height3 = layout3.GetGlyph(6).quad.points[0][1] -
        layout3.GetGlyph(0).quad.points[0][1];

    float line_spacing1 = line_height1 / glyph_height1;
    float line_spacing3 = line_height3 / glyph_height3;

    // Expect the correct ratio of line spacing.
    EXPECT_NEAR(line_spacing3 / line_spacing1, 3, 0.01);
  }
}

TEST(PlatformFontTest, Space) {
  // Test that the horizontal advance for a space is reasonable.
  std::vector<FontPtr> fonts = SimpleTestFonts(4);
  for (const FontPtr& font : fonts) {
    LayoutOptions options;
    options.horizontal_alignment = kAlignLeft;
    options.vertical_alignment = kAlignBaseline;
    options.target_size.Set(0.0f, 100.0f);

    Layout space_layout = font->BuildLayout("# #", options);
    Layout::Glyph last_glyph =
        space_layout.GetGlyph(space_layout.GetGlyphCount() - 1);
    EXPECT_NEAR(194, last_glyph.quad.points[1][0], 3);
  }
}

TEST(PlatformFontTest, MultiLine) {
  std::vector<FontPtr> fonts = SimpleTestFonts(0);
  for (const FontPtr& font : fonts) {
    LayoutOptions options;
    options.target_size = Vector2f(0, 100);

    // Two-line text with left and baseline alignment. Verify that second line
    // is below the first, the left edges of the two lines are close
    // horizontally.
    {
      options.horizontal_alignment = kAlignLeft;
      options.vertical_alignment = kAlignBaseline;
      Layout layout = font->BuildLayout("Abc\ng", options);
      EXPECT_GT(layout.GetGlyph(0).quad.points[0][1],
                layout.GetGlyph(layout.GetGlyphCount() - 1).quad.points[3][1]);
      EXPECT_NEAR(layout.GetGlyph(0).quad.points[0][1], 0, 1);
      EXPECT_NEAR(layout.GetGlyph(0).quad.points[0][0],
                  layout.GetGlyph(layout.GetGlyphCount() - 1)
                      .quad.points[0][0], 1);
    }

    {
      // Centered two-line text. Verify that the shorter line's horizontal
      // bounds are within that of the longer.
      options.horizontal_alignment = kAlignHCenter;
      Layout layout = font->BuildLayout("Abcedf\nabc", options);
      EXPECT_GT(layout.GetGlyph(5).quad.points[3][0] - 10.f,
                layout.GetGlyph(layout.GetGlyphCount() - 1).quad.points[3][0]);
      EXPECT_LT(layout.GetGlyph(0).quad.points[0][0] + 10.f,
                layout.GetGlyph(layout.GetGlyphCount() - 3).quad.points[0][0]);
    }

    {
      // Right-aligned two-line text. Verify that the second line starts
      // significantly to the right of the first, and both lines end near to the
      // origin horizontally.
      options.horizontal_alignment = kAlignRight;
      Layout layout = font->BuildLayout("Abcedf\nabc", options);
      EXPECT_LT(layout.GetGlyph(0).quad.points[0][0] + 50,
                layout.GetGlyph(layout.GetGlyphCount() - 3).quad.points[0][0]);
      EXPECT_NEAR(layout.GetGlyph(5).quad.points[2][0], 0, 1);
      EXPECT_NEAR(layout.GetGlyph(layout.GetGlyphCount() - 1)
                      .quad.points[2][0], 0, 2);
    }

    // Top alignment. Verify that the top of the first line's glyphs are close
    // to the origin vertically.
    {
      options.vertical_alignment = kAlignTop;
      Layout layout = font->BuildLayout("Abg.\ng", options);
      EXPECT_NEAR(layout.GetGlyph(0).quad.points[3][1], 0, 1);
    }

    // Bottom alignment. Verify that the bottom of the last line's glyphs are
    // close to the origin vertically.
    {
      options.vertical_alignment = kAlignBottom;
      Layout layout = font->BuildLayout("Abg.\ng", options);
      EXPECT_NEAR(layout.GetGlyph(layout.GetGlyphCount() - 1)
                      .quad.points[0][1], 0, 1);
    }
  }
}

TEST(PlatformFontTest, FontAdvancedLayout) {
  static const char kDataDir[] = "/third_party/icu/icu4c/source/stubdata/";
  port::SetEnvironmentVariableValue("ION_ICU_DIR",
                                    GetIonDirectory() + kDataDir);

  // Assert that glyph combining works correctly by observing that the width of
  // the test string shrinks when the <reph> is added (since that turns the 'र'
  // into a super-script on the 'ग' instead of being its own character).
  std::string no_reph_str = "मारग";
  std::string with_reph_str = "मार्ग";
  std::vector<FontPtr> fonts = ComplexTestFonts();
  for (const FontPtr& font : fonts) {
    LayoutOptions options;
    Layout no_reph = font->BuildLayout(no_reph_str, options);
    Layout with_reph = font->BuildLayout(with_reph_str, options);
    GlyphSet no_reph_glyphs(base::AllocatorPtr(nullptr));
    GlyphSet with_reph_glyphs(base::AllocatorPtr(nullptr));
    no_reph.GetGlyphSet(&no_reph_glyphs);
    with_reph.GetGlyphSet(&with_reph_glyphs);
    // Both layouts end up with 4 glyphs, but they are not the same glyphs!
    ASSERT_EQ(4U, no_reph.GetGlyphCount());
    ASSERT_EQ(4U, with_reph.GetGlyphCount());
    ASSERT_NE(no_reph_glyphs, with_reph_glyphs);

    // Expect that |with_reph| is no more than this times |no_reph|'s width.
    // Note that this constant has been chosen to be just large enough to pass
    // for all the available platform fonts.
    static const double kMaximumAllowedRatio = 0.82;
    EXPECT_LT(
        with_reph.GetGlyph(with_reph.GetGlyphCount() - 1).quad.points[1][0],
        no_reph.GetGlyph(no_reph.GetGlyphCount() - 1).quad.points[1][0] *
            kMaximumAllowedRatio);
  }
}

}  // namespace text
}  // namespace ion
