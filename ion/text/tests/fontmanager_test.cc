/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#include "ion/base/logchecker.h"
#include "ion/math/range.h"
#include "ion/math/rangeutils.h"
#include "ion/math/vectorutils.h"
#include "ion/text/fonts/roboto_regular.h"
#include "ion/text/tests/mockfont.h"
#include "ion/text/tests/mockfontimage.h"
#include "ion/text/tests/testfont.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

TEST(FontManagerTest, FontMap) {
  base::LogChecker logchecker;

  // Initialize a Font with the data.
  FontManagerPtr fm(new FontManager);
  FontPtr font = testing::BuildTestFreeTypeFont("Test", 32U, 4U);
  EXPECT_FALSE(font.Get() == NULL);

  // Should not be able to find the font in the manager yet.
  EXPECT_TRUE(fm->FindFont("Test", 32U, 4U).Get() == NULL);

  // Add the font to the map.
  fm->AddFont(font);

  // Now should be able to find the font in the manager.
  EXPECT_FALSE(fm->FindFont("Test", 32U, 4U).Get() == NULL);

  // Should not be able to find missing fonts.
  EXPECT_TRUE(fm->FindFont("Test", 33U, 4U).Get() == NULL);
  EXPECT_TRUE(fm->FindFont("Test", 32U, 6U).Get() == NULL);
  EXPECT_TRUE(fm->FindFont("Testy", 32U, 4U).Get() == NULL);
}

TEST(FontManagerTest, AddFontWithData) {
  base::LogChecker logchecker;

  // Initialize a Font with the data.
  FontManagerPtr fm(new FontManager);
  std::string font_data = testing::GetTestFontData();

  // Should not be able to find the font in the manager yet.
  EXPECT_TRUE(fm->FindFont("Test", 32U, 4U).Get() == NULL);

  // Create and add font to the map.
  FontPtr font = fm->AddFont("Test", 32U, 4U, &font_data[0], font_data.size());

  // Verify that adding the font data twice results in the same Font object.
  FontPtr font2 = fm->AddFont("Test", 32U, 4U, &font_data[0], font_data.size());
  EXPECT_EQ(font, font2);

  // Font should have been successfully created.
  EXPECT_FALSE(font.Get() == NULL);

  // Now should be able to find the font in the manager.
  EXPECT_FALSE(fm->FindFont("Test", 32U, 4U).Get() == NULL);
}

TEST(FontManagerTest, AddFontFromZipasset) {
  base::LogChecker logchecker;

  // Initialize a Font with the data.
  FontManagerPtr fm(new FontManager);
  roboto_regular_assets::RegisterAssetsOnce();

  // Should not be able to find the font in the manager yet.
  EXPECT_TRUE(fm->FindFont("roboto_foo", 32U, 4U).Get() == NULL);

  // Create and add font to the map.
  FontPtr font =
      fm->AddFontFromZipasset("roboto_foo", "roboto_regular", 32U, 4U);

  // Verify that adding the font data twice results in the same Font object.
  FontPtr font2 =
      fm->AddFontFromZipasset("roboto_foo", "roboto_regular", 32U, 4U);
  EXPECT_EQ(font, font2);

  // Font should have been successfully created.
  EXPECT_FALSE(font.Get() == NULL);

  // Now should be able to find the font in the manager.
  EXPECT_FALSE(fm->FindFont("roboto_foo", 32U, 4U).Get() == NULL);

  // Reading an invalid zipasset should result in an error message.
  FontPtr font3 =
      fm->AddFontFromZipasset("roboto_bar", "does_not_exist", 32U, 4U);
  EXPECT_TRUE(font3.Get() == NULL);
  EXPECT_TRUE(logchecker.HasMessage(
      "ERROR", "Unable to read data for font \"roboto_bar\"."));
}

TEST(FontManagerTest, BuildFontKey) {
  EXPECT_EQ("TestFont/32/4", FontManager::BuildFontKey("TestFont", 32U, 4U));
  EXPECT_EQ("Some Name With Spaces/64/0",
            FontManager::BuildFontKey("Some Name With Spaces", 64U, 0U));
}

TEST(FontManagerTest, CacheFontImage) {
  base::LogChecker logchecker;
  FontManagerPtr fm(new FontManager);

  const std::string key("Some string");
  EXPECT_TRUE(fm->GetCachedFontImage(key).Get() == NULL);

  // Create and cache a MockFontImage.
  FontImagePtr font_image(new testing::MockFontImage());
  EXPECT_FALSE(font_image.Get() == NULL);
  fm->CacheFontImage(key, font_image);
  FontImagePtr f = fm->GetCachedFontImage(key);
  EXPECT_FALSE(f.Get() == NULL);
  EXPECT_EQ(font_image, f);

  // Replace with a different MockFontImage.
  FontImagePtr font_image2(new testing::MockFontImage());
  EXPECT_FALSE(font_image2.Get() == NULL);
  fm->CacheFontImage(key, font_image2);
  f = fm->GetCachedFontImage(key);
  EXPECT_FALSE(f.Get() == NULL);
  EXPECT_EQ(font_image2, f);

  // Replace with a NULL pointer.
  fm->CacheFontImage(key, FontImagePtr(NULL));
  f = fm->GetCachedFontImage(key);
  EXPECT_TRUE(f.Get() == NULL);

  // Cache a MockFontImage, but use the Font (not a string) as a key.
  FontPtr font(new testing::MockFont(12, 4));
  fm->CacheFontImage(font, font_image);
  f = fm->GetCachedFontImage(font);
  EXPECT_EQ(font_image, f);
}

}  // namespace text
}  // namespace ion
