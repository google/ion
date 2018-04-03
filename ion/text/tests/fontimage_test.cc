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

#include "ion/text/fontimage.h"

#include <vector>

#include "ion/base/invalid.h"
#include "ion/math/range.h"
#include "ion/math/rangeutils.h"
#include "ion/text/tests/mockfont.h"
#include "ion/text/tests/testfont.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

static void AddCharacterRange(CharIndex start, CharIndex finish,
                              const FontPtr& font, GlyphSet* glyph_set) {
  for (CharIndex i = start; i <= finish; ++i)
    glyph_set->insert(font->GetDefaultGlyphForChar(i));
}

static bool FontImageHasGlyphForChar(const FontImage::ImageData& data,
                                     const FontPtr& font, CharIndex c) {
  return FontImage::HasGlyph(data, font->GetDefaultGlyphForChar(c));
}

TEST(FontImageTest, StaticFontImageEmpty) {
  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  StaticFontImagePtr fi(new StaticFontImage(FontPtr(), 64U, glyph_set));
  EXPECT_EQ(FontImage::kStatic, fi->GetType());
  EXPECT_FALSE(fi->GetFont());
  EXPECT_EQ(64U, fi->GetMaxImageSize());
  const FontImage::ImageData& data = fi->GetImageData();
  EXPECT_TRUE(data.texture->GetLabel().empty());
}

TEST(FontImageTest, StaticFontImageNullFont) {
  // Null Font results in an empty FontImage.
  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  glyph_set.insert('A');
  FontPtr font;
  StaticFontImagePtr sfi(new StaticFontImage(font, 64U, glyph_set));
  const FontImage::ImageData& data = sfi->GetImageData();
  EXPECT_FALSE(base::IsInvalidReference(data));
  ASSERT_TRUE(data.texture);
  EXPECT_TRUE(data.texture->GetLabel().empty());
  EXPECT_FALSE(data.texture->GetImage(0U));
  EXPECT_EQ(0U, data.glyph_set.size());
  EXPECT_TRUE(data.texture_rectangle_map.empty());
}

TEST(FontImageTest, StaticFontImageNoChars) {
  // No characters results in an empty FontImage.
  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  static const size_t kFontSize = 32U;
  static const size_t kSdfPadding = 16U;
  FontPtr font(new testing::MockFont(kFontSize, kSdfPadding));
  StaticFontImagePtr sfi(new StaticFontImage(font, 64U, glyph_set));
  const FontImage::ImageData& data = sfi->GetImageData();
  ASSERT_TRUE(data.texture);
  EXPECT_EQ("MockFont_32", data.texture->GetLabel());
  EXPECT_FALSE(data.texture->GetImage(0U));
  EXPECT_EQ(0U, data.glyph_set.size());
  EXPECT_TRUE(data.texture_rectangle_map.empty());
}

TEST(FontImageTest, StaticFontImageFits) {
  // This should create a valid image - the glyphs should fit.
  static const size_t kFontSize = 32U;
  static const size_t kSdfPadding = 16U;
  FontPtr font(new testing::MockFont(kFontSize, kSdfPadding));

  const GlyphIndex glyph_A = font->GetDefaultGlyphForChar('A');
  const GlyphIndex glyph_b = font->GetDefaultGlyphForChar('b');

  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  glyph_set.insert(glyph_A);
  glyph_set.insert(glyph_b);
  StaticFontImagePtr sfi(new StaticFontImage(font, 256U, glyph_set));

  const FontImage::ImageData& data = sfi->GetImageData();
  ASSERT_TRUE(data.texture);
  EXPECT_TRUE(data.texture->GetImage(0U));
  EXPECT_EQ("MockFont_32", data.texture->GetLabel());

  // Static font images do not use sub-images.
  EXPECT_TRUE(data.texture->GetSubImages().empty());

  // Check the set of character indices.
  EXPECT_EQ(2U, data.glyph_set.size());
  EXPECT_TRUE(data.glyph_set.count(glyph_A));
  EXPECT_TRUE(data.glyph_set.count(glyph_b));

  // Check the HasGlyph() convenience function.
  EXPECT_FALSE(FontImageHasGlyphForChar(data, font, 'a'));
  EXPECT_TRUE(FontImageHasGlyphForChar(data, font, 'b'));
  EXPECT_FALSE(FontImageHasGlyphForChar(data, font, 'c'));
  EXPECT_FALSE(FontImageHasGlyphForChar(data, font, 'd'));
  EXPECT_TRUE(FontImageHasGlyphForChar(data, font, 'A'));
  EXPECT_FALSE(FontImageHasGlyphForChar(data, font, 'B'));
  EXPECT_FALSE(FontImageHasGlyphForChar(data, font, 'C'));
  EXPECT_FALSE(FontImageHasGlyphForChar(data, font, 'D'));
  EXPECT_FALSE(FontImageHasGlyphForChar(
      base::InvalidReference<FontImage::ImageData>(), font, 'b'));

  // Check the HasAllGlyphs() convenience function.
  EXPECT_TRUE(FontImage::HasAllGlyphs(data, glyph_set));
  {
    GlyphSet glyph_set2(base::AllocatorPtr(nullptr));
    glyph_set2 = glyph_set;
    glyph_set2.insert(font->GetDefaultGlyphForChar('B'));
    EXPECT_FALSE(FontImage::HasAllGlyphs(data, glyph_set2));
  }

  // Check texture coordinate rectangles using the GetTextureCoords()
  // convenience function.
  math::Range2f rect;
  static const float kTolerance = 1e-4f;
  EXPECT_FALSE(data.texture_rectangle_map.empty());
  EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_b, &rect));
  EXPECT_PRED3((math::RangesAlmostEqual<2, float>),
               math::Range2f(math::Point2f(0.320312f, 0.f),
                             math::Point2f(0.601562f, 0.84375f)), rect,
               kTolerance);
  EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_A, &rect));
  EXPECT_PRED3((math::RangesAlmostEqual<2, float>),
               math::Range2f(math::Point2f(0.f, 0.f),
                             math::Point2f(0.320312f, 0.875f)), rect,
               kTolerance);

  // Test error cases with GetTextureCoords().
  // Missing glyph.
  EXPECT_FALSE(FontImage::GetTextureCoords(
      data, font->GetDefaultGlyphForChar('c'), &rect));
  // Invalid ImageData.
  EXPECT_FALSE(FontImage::GetTextureCoords(
      base::InvalidReference<FontImage::ImageData>(), glyph_A, &rect));
}

TEST(FontImageTest, StaticFontImageFitsWithDoubling) {
  // This test requires the StaticFontImage size to be doubled in both
  // dimensions to make the glyphs fit.
  std::vector<math::Range1ui> char_ranges;
  static const size_t kFontSize = 32U;
  static const size_t kSdfPadding = 16U;
  testing::MockFontPtr font(new testing::MockFont(kFontSize, kSdfPadding));
  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  const GlyphIndex glyph_A = font->GetDefaultGlyphForChar('A');
  const GlyphIndex glyph_b = font->GetDefaultGlyphForChar('b');
  const GlyphIndex glyph_hash = font->GetDefaultGlyphForChar('#');
  glyph_set.insert(glyph_A);
  glyph_set.insert(glyph_b);
  glyph_set.insert(glyph_hash);
  StaticFontImagePtr sfi(new StaticFontImage(font, 256U, glyph_set));
  const FontImage::ImageData& data = sfi->GetImageData();
  ASSERT_TRUE(data.texture);
  EXPECT_EQ("MockFont_32", data.texture->GetLabel());
  EXPECT_TRUE(data.texture->GetImage(0U));
  EXPECT_TRUE(data.texture->GetSubImages().empty());
}

TEST(FontImageTest, StaticFontImageNoRoom) {
  std::vector<math::Range1ui> char_ranges;
  static const size_t kFontSize = 32U;
  static const size_t kSdfPadding = 16U;
  testing::MockFontPtr font(new testing::MockFont(kFontSize, kSdfPadding));
  const GlyphIndex glyph_A = font->GetDefaultGlyphForChar('A');
  const GlyphIndex glyph_b = font->GetDefaultGlyphForChar('b');
  const GlyphIndex glyph_hash = font->GetDefaultGlyphForChar('#');
  {
    // This should result in a null image because the glyphs don't fit. In this
    // case the total glyph area is larger than the maximum area (128 squared).
    GlyphSet glyph_set(base::AllocatorPtr(nullptr));
    glyph_set.insert(glyph_A);
    glyph_set.insert(glyph_b);
    glyph_set.insert(glyph_hash);
    StaticFontImagePtr sfi(new StaticFontImage(font, 128U, glyph_set));
    const FontImage::ImageData& data = sfi->GetImageData();
    EXPECT_TRUE(data.texture);
    EXPECT_EQ("MockFont_32", data.texture->GetLabel());
    EXPECT_FALSE(data.texture->GetImage(0U));
    EXPECT_TRUE(data.texture->GetSubImages().empty());
  }

  {
    // In this case the maximum area is large enough, but the glyphs can't be
    // arranged to fit.
    GlyphSet glyph_set(base::AllocatorPtr(nullptr));
    glyph_set.insert(glyph_A);
    glyph_set.insert(glyph_b);
    glyph_set.insert(glyph_hash);
    StaticFontImagePtr sfi(new StaticFontImage(font, 200U, glyph_set));
    const FontImage::ImageData& data = sfi->GetImageData();
    ASSERT_TRUE(data.texture);
    EXPECT_EQ("MockFont_32", data.texture->GetLabel());
    EXPECT_FALSE(data.texture->GetImage(0U));
    EXPECT_TRUE(data.texture->GetSubImages().empty());
  }
}

TEST(FontImageTest, DynamicFontImageEmpty) {
  DynamicFontImagePtr dfi(new DynamicFontImage(FontPtr(), 64U));
  EXPECT_EQ(FontImage::kDynamic, dfi->GetType());
  EXPECT_FALSE(dfi->GetFont());
  EXPECT_EQ(64U, dfi->GetMaxImageSize());
  EXPECT_EQ(0U, dfi->GetImageDataCount());
  EXPECT_TRUE(base::IsInvalidReference(dfi->GetImageData(0)));
  EXPECT_TRUE(base::IsInvalidReference(dfi->GetImageData(10)));
  EXPECT_EQ(0.f, dfi->GetImageDataUsedAreaFraction(0));
  EXPECT_EQ(0.f, dfi->GetImageDataUsedAreaFraction(10));
  EXPECT_FALSE(dfi->AreUpdatesDeferred());
}

TEST(FontImageTest, DynamicFontImageNullFont) {
  // Null Font means that glyphs cannot be added.
  FontPtr font;
  DynamicFontImagePtr dfi(new DynamicFontImage(font, 64U));
  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  glyph_set.insert(0x42);  // Arbitrary glyph index; font is null!
  EXPECT_TRUE(base::IsInvalidReference(dfi->FindImageData(glyph_set)));
  EXPECT_EQ(base::kInvalidIndex, dfi->FindImageDataIndex(glyph_set));
  EXPECT_EQ(base::kInvalidIndex, dfi->FindContainingImageDataIndex(glyph_set));
  EXPECT_EQ(0U, dfi->GetImageDataCount());
}

TEST(FontImageTest, DynamicFontImageNoChars) {
  static const size_t kFontSize = 32U;
  static const size_t kSdfPadding = 16U;
  FontPtr font(new testing::MockFont(kFontSize, kSdfPadding));
  DynamicFontImagePtr dfi(new DynamicFontImage(font, 64U));

  // Adding an empty GlyphSet to a DynamicFontImage should fail.
  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  EXPECT_TRUE(base::IsInvalidReference(dfi->FindImageData(glyph_set)));
  EXPECT_EQ(base::kInvalidIndex, dfi->FindImageDataIndex(glyph_set));
  EXPECT_EQ(base::kInvalidIndex, dfi->FindContainingImageDataIndex(glyph_set));
  EXPECT_EQ(0U, dfi->GetImageDataCount());

  // Adding a GlyphSet with only invalid glyphs should also fail.
  glyph_set.insert(font->GetDefaultGlyphForChar('Q'));
  EXPECT_TRUE(base::IsInvalidReference(dfi->FindImageData(glyph_set)));
  EXPECT_EQ(base::kInvalidIndex, dfi->FindImageDataIndex(glyph_set));
  EXPECT_EQ(base::kInvalidIndex, dfi->FindContainingImageDataIndex(glyph_set));
  EXPECT_EQ(0U, dfi->GetImageDataCount());
}

TEST(FontImageTest, DynamicFontImageAdding) {
  // Note: This test uses a real Font (not the MockFont) so that there are
  // sufficient characters to test several features.
  static const size_t kFontSize = 32U;
  static const size_t kSdfPadding = 16U;
  FontPtr font = testing::BuildTestFreeTypeFont("Test", kFontSize, kSdfPadding);
  DynamicFontImagePtr dfi(new DynamicFontImage(font, 128U));

  // These are used to compare texture coordinate rectangles.
  typedef std::map<GlyphIndex, math::Range2f> RectMap;
  RectMap rect_map;
  math::Range2f rect;

  // Adding these characters should create a new ImageData.
  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  AddCharacterRange('A', 'C', font, &glyph_set);
  const GlyphIndex glyph_A = font->GetDefaultGlyphForChar('A');
  const GlyphIndex glyph_B = font->GetDefaultGlyphForChar('B');
  const GlyphIndex glyph_C = font->GetDefaultGlyphForChar('C');
  const GlyphIndex glyph_D = font->GetDefaultGlyphForChar('D');
  const GlyphIndex glyph_E = font->GetDefaultGlyphForChar('E');
  const GlyphIndex glyph_F = font->GetDefaultGlyphForChar('F');
  const GlyphIndex glyph_space = font->GetDefaultGlyphForChar(' ');

  {
    EXPECT_EQ(base::kInvalidIndex,
              dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_EQ(1U, dfi->GetImageDataCount());
    EXPECT_EQ(&data, &dfi->GetImageData(0));
    EXPECT_TRUE(base::IsInvalidReference(dfi->GetImageData(1)));
    // There are no sub-images since the first time the image is created it has
    // all of the initial glyphs in the main image.
    EXPECT_TRUE(data.texture->GetSubImages().empty());

    // Check the set of character indices.
    EXPECT_EQ(3U, data.glyph_set.size());
    EXPECT_TRUE(data.glyph_set.count(glyph_A));
    EXPECT_TRUE(data.glyph_set.count(glyph_B));
    EXPECT_TRUE(data.glyph_set.count(glyph_C));

    // Check for presence of texture coordinate rectangles.
    EXPECT_FALSE(data.texture_rectangle_map.empty());
    EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_A, &rect));
    rect_map[glyph_A] = rect;
    EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_B, &rect));
    rect_map[glyph_B] = rect;
    EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_C, &rect));
    rect_map[glyph_C] = rect;

    EXPECT_EQ(0U, dfi->FindImageDataIndex(glyph_set));
    EXPECT_EQ(0U, dfi->FindContainingImageDataIndex(glyph_set));
  }
  EXPECT_NEAR(0.506f, dfi->GetImageDataUsedAreaFraction(0), 1e-3f);

  // Should be able to add 1 more glyphs to the same ImageData.
  glyph_set.clear();
  glyph_set.insert(glyph_D);
  {
    EXPECT_EQ(base::kInvalidIndex,
              dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_EQ(1U, dfi->GetImageDataCount());
    EXPECT_EQ(&data, &dfi->GetImageData(0));
    EXPECT_EQ(1U, data.texture->GetSubImages().size());

    // Check the set of character indices.
    EXPECT_EQ(4U, data.glyph_set.size());
    EXPECT_TRUE(data.glyph_set.count(glyph_A));
    EXPECT_TRUE(data.glyph_set.count(glyph_B));
    EXPECT_TRUE(data.glyph_set.count(glyph_C));
    EXPECT_TRUE(data.glyph_set.count(glyph_D));

    // Check for presence of texture coordinate rectangles and make sure the
    // previous onces have not changed.
    EXPECT_FALSE(data.texture_rectangle_map.empty());
    EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_A, &rect));
    EXPECT_EQ(rect_map.find(glyph_A)->second, rect);
    EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_B, &rect));
    EXPECT_EQ(rect_map.find(glyph_B)->second, rect);
    EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_C, &rect));
    EXPECT_EQ(rect_map.find(glyph_C)->second, rect);
    EXPECT_TRUE(FontImage::GetTextureCoords(data, glyph_D, &rect));
    rect_map[glyph_D] = rect;
  }
  EXPECT_NEAR(0.674f, dfi->GetImageDataUsedAreaFraction(0), 1e-3f);

  // Adding another glyph should cause a new ImageData to be created.
  glyph_set.clear();
  glyph_set.insert(glyph_E);
  {
    EXPECT_EQ(base::kInvalidIndex,
              dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_EQ(2U, dfi->GetImageDataCount());
    EXPECT_EQ(&data, &dfi->GetImageData(1));
    EXPECT_EQ(1U, data.glyph_set.size());
    EXPECT_TRUE(data.glyph_set.count(glyph_E));
    EXPECT_NEAR(0.161f, dfi->GetImageDataUsedAreaFraction(1), 1e-3f);
    // Since this is a new ImageData it has no sub-images.
    EXPECT_TRUE(data.texture->GetSubImages().empty());

    // The first ImageData should have remained the same.
    EXPECT_EQ(4U, dfi->GetImageData(0).glyph_set.size());
    EXPECT_NEAR(0.674f, dfi->GetImageDataUsedAreaFraction(0), 1e-3f);
  }

  // Adding another glyph should add to the second ImageData.
  glyph_set.clear();
  glyph_set.insert(glyph_F);
  {
    EXPECT_EQ(base::kInvalidIndex,
              dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_EQ(2U, dfi->GetImageDataCount());
    EXPECT_EQ(&data, &dfi->GetImageData(1));
    EXPECT_EQ(2U, data.glyph_set.size());
    EXPECT_TRUE(data.glyph_set.count(glyph_E));
    EXPECT_TRUE(data.glyph_set.count(glyph_F));
    EXPECT_NEAR(0.322f, dfi->GetImageDataUsedAreaFraction(1), 1e-3f);
    // The rect for 'F' was added.
    EXPECT_EQ(1U, data.texture->GetSubImages().size());

    // The first ImageData should have remained the same.
    EXPECT_EQ(4U, dfi->GetImageData(0).glyph_set.size());
    EXPECT_NEAR(0.674f, dfi->GetImageDataUsedAreaFraction(0), 1e-3f);
  }

  // Adding glyphs that are all within a single ImageData should reuse it
  // without changing anything. Include a glyph (space) that does not appear in
  // the Font - it should be filtered out and not cause failure.
  glyph_set.clear();
  AddCharacterRange('A', 'D', font, &glyph_set);
  glyph_set.insert(glyph_space);
  {
    EXPECT_EQ(0U, dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_EQ(2U, dfi->GetImageDataCount());
    EXPECT_EQ(&data, &dfi->GetImageData(0));
    EXPECT_EQ(4U, dfi->GetImageData(0).glyph_set.size());
    EXPECT_EQ(2U, dfi->GetImageData(1).glyph_set.size());
    EXPECT_NEAR(0.674f, dfi->GetImageDataUsedAreaFraction(0), 1e-3f);
    EXPECT_NEAR(0.322f, dfi->GetImageDataUsedAreaFraction(1), 1e-3f);
    // The dfi already contains A-D.
    EXPECT_EQ(1U, data.texture->GetSubImages().size());
  }
  glyph_set.clear();
  AddCharacterRange('E', 'F', font, &glyph_set);
  glyph_set.insert(glyph_space);
  {
    EXPECT_EQ(1U, dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_EQ(2U, dfi->GetImageDataCount());
    EXPECT_EQ(&data, &dfi->GetImageData(1));
    EXPECT_EQ(4U, dfi->GetImageData(0).glyph_set.size());
    EXPECT_EQ(2U, dfi->GetImageData(1).glyph_set.size());
    EXPECT_NEAR(0.674f, dfi->GetImageDataUsedAreaFraction(0), 1e-3f);
    EXPECT_NEAR(0.322f, dfi->GetImageDataUsedAreaFraction(1), 1e-3f);
    // The dfi already contains E and F.
    EXPECT_EQ(1U, data.texture->GetSubImages().size());
  }

  // Adding too many glyphs should result in failure, with no changes to
  // existing ImageData.
  glyph_set.clear();
  AddCharacterRange('Q', 'Z', font, &glyph_set);
  {
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_TRUE(base::IsInvalidReference(data));
    EXPECT_EQ(2U, dfi->GetImageDataCount());
    EXPECT_EQ(4U, dfi->GetImageData(0).glyph_set.size());
    EXPECT_EQ(2U, dfi->GetImageData(1).glyph_set.size());
    EXPECT_NEAR(0.674f, dfi->GetImageDataUsedAreaFraction(0), 1e-3f);
    EXPECT_NEAR(0.322f, dfi->GetImageDataUsedAreaFraction(1), 1e-3f);
  }

  // Adding just a few glyphs should add more sub-images, one per glyph.
  glyph_set.clear();
  AddCharacterRange('Q', 'R', font, &glyph_set);
  {
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_EQ(2U, dfi->GetImageDataCount());
    EXPECT_EQ(4U, dfi->GetImageData(0).glyph_set.size());
    EXPECT_EQ(4U, dfi->GetImageData(1).glyph_set.size());
    // New sub-images were added for the two glyphs.
    EXPECT_EQ(3U, data.texture->GetSubImages().size());
  }
}

TEST(FontImageTest, DynamicFontImageDeferredUpdates) {
  // Note: This test uses a real Font (not the MockFont) so that there are
  // sufficient characters to test several features.
  static const size_t kFontSize = 32U;
  static const size_t kSdfPadding = 16U;
  FontPtr font = testing::BuildTestFreeTypeFont("Test", kFontSize, kSdfPadding);
  DynamicFontImagePtr dfi(new DynamicFontImage(font, 128U));
  EXPECT_FALSE(dfi->AreUpdatesDeferred());
  dfi->EnableDeferredUpdates(true);
  EXPECT_TRUE(dfi->AreUpdatesDeferred());

  // These are used to compare texture coordinate rectangles.
  typedef std::map<CharIndex, math::Range2f> RectMap;
  RectMap rect_map;
  math::Range2f rect;

  // Adding these characters should create a new ImageData.
  GlyphSet glyph_set(base::AllocatorPtr(nullptr));
  AddCharacterRange('A', 'C', font, &glyph_set);
  const GlyphIndex glyph_D = font->GetDefaultGlyphForChar('D');
  const GlyphIndex glyph_E = font->GetDefaultGlyphForChar('E');
  const GlyphIndex glyph_F = font->GetDefaultGlyphForChar('F');
  const GlyphIndex glyph_space = font->GetDefaultGlyphForChar(' ');
  {
    EXPECT_EQ(base::kInvalidIndex,
              dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_TRUE(base::IsInvalidReference(dfi->GetImageData(1)));
    // There are no sub-images since the first time the image is created it has
    // all of the initial glyphs in the main image, even if updates are
    // deferred since the DFI's texture didn't exist before.
    EXPECT_TRUE(data.texture->GetSubImages().empty());
  }

  // Should be able to add 1 more glyph to the same ImageData.
  glyph_set.clear();
  glyph_set.insert(glyph_D);
  {
    EXPECT_EQ(base::kInvalidIndex,
              dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_EQ(0U, data.texture->GetSubImages().size());
    dfi->ProcessDeferredUpdates();
    EXPECT_EQ(1U, data.texture->GetSubImages().size());
  }

  // Adding another glyph should cause a new ImageData to be created.
  glyph_set.clear();
  glyph_set.insert(glyph_E);
  {
    EXPECT_EQ(base::kInvalidIndex,
              dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    EXPECT_TRUE(data.texture->GetSubImages().empty());
  }

  // Adding another glyph should add to the second ImageData.
  glyph_set.clear();
  glyph_set.insert(glyph_F);
  {
    EXPECT_EQ(base::kInvalidIndex,
              dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    // The rect for 'F' was added.
    EXPECT_EQ(0U, data.texture->GetSubImages().size());
    dfi->ProcessDeferredUpdates();
    EXPECT_EQ(1U, data.texture->GetSubImages().size());
  }

  // Adding glyphs that are all within a single ImageData should reuse it
  // without changing anything. Include a glyph (space) that does not appear in
  // the Font - it should be filtered out and not cause failure.
  glyph_set.clear();
  AddCharacterRange('A', 'D', font, &glyph_set);
  glyph_set.insert(glyph_space);
  {
    EXPECT_EQ(0U, dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    // The dfi already contains A-D.
    EXPECT_EQ(1U, data.texture->GetSubImages().size());
  }
  glyph_set.clear();
  AddCharacterRange('E', 'F', font, &glyph_set);
  glyph_set.insert(glyph_space);
  {
    EXPECT_EQ(1U, dfi->FindContainingImageDataIndex(glyph_set));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    // The dfi already contains E and F.
    EXPECT_EQ(1U, data.texture->GetSubImages().size());
  }

  // Adding just a few glyphs should add more sub-images, one per glyph.
  glyph_set.clear();
  AddCharacterRange('Q', 'R', font, &glyph_set);
  {
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_FALSE(base::IsInvalidReference(data));
    // New sub-images were added for the two glyphs.
    EXPECT_EQ(1U, data.texture->GetSubImages().size());
    dfi->ProcessDeferredUpdates();
    EXPECT_EQ(3U, data.texture->GetSubImages().size());
  }
}

}  // namespace text
}  // namespace ion
