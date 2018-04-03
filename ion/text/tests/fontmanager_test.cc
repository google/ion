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

#include "ion/text/fontmanager.h"

#include "ion/base/logchecker.h"
#include "ion/base/zipassetmanager.h"
#include "ion/math/range.h"
#include "ion/math/rangeutils.h"
#include "ion/math/vectorutils.h"
#include "ion/port/fileutils.h"
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
  EXPECT_NE(nullptr, font.Get());

  // Should not be able to find the font in the manager yet.
  EXPECT_EQ(nullptr, fm->FindFont("Test", 32U, 4U).Get());

  // Add the font to the map.
  fm->AddFont(font);

  // Now should be able to find the font in the manager.
  EXPECT_NE(nullptr, fm->FindFont("Test", 32U, 4U).Get());

  // Should not be able to find missing fonts.
  EXPECT_EQ(nullptr, fm->FindFont("Test", 33U, 4U).Get());
  EXPECT_EQ(nullptr, fm->FindFont("Test", 32U, 6U).Get());
  EXPECT_EQ(nullptr, fm->FindFont("Testy", 32U, 4U).Get());
}

TEST(FontManagerTest, AddFontWithData) {
  base::LogChecker logchecker;

  // Initialize a Font with the data.
  FontManagerPtr fm(new FontManager);
  std::string font_data = testing::GetTestFontData();

  // Should not be able to find the font in the manager yet.
  EXPECT_EQ(nullptr, fm->FindFont("Test", 32U, 4U).Get());

  // Create and add font to the map.
  FontPtr font = fm->AddFont("Test", 32U, 4U, &font_data[0], font_data.size());

  // Verify that adding the font data twice results in the same Font object.
  FontPtr font2 = fm->AddFont("Test", 32U, 4U, &font_data[0], font_data.size());
  EXPECT_EQ(font, font2);

  // Font should have been successfully created.
  EXPECT_NE(nullptr, font.Get());

  // Now should be able to find the font in the manager.
  EXPECT_NE(nullptr, fm->FindFont("Test", 32U, 4U).Get());
}

TEST(FontManagerTest, AddFontFromZipasset) {
  base::LogChecker logchecker;

  // Initialize a Font with the data.
  FontManagerPtr fm(new FontManager);
  roboto_regular_assets::RegisterAssetsOnce();

  // Should not be able to find the font in the manager yet.
  EXPECT_EQ(nullptr, fm->FindFont("roboto_foo", 32U, 4U).Get());

  // Create and add font to the map.
  FontPtr font =
      fm->AddFontFromZipasset("roboto_foo", "roboto_regular", 32U, 4U);

  // Verify that adding the font data twice results in the same Font object.
  FontPtr font2 =
      fm->AddFontFromZipasset("roboto_foo", "roboto_regular", 32U, 4U);
  EXPECT_EQ(font, font2);

  // Font should have been successfully created.
  EXPECT_NE(nullptr, font.Get());

  // Now should be able to find the font in the manager.
  EXPECT_NE(nullptr, fm->FindFont("roboto_foo", 32U, 4U).Get());

  // Reading an invalid zipasset should result in an error message.
  FontPtr font3 =
      fm->AddFontFromZipasset("roboto_bar", "does_not_exist", 32U, 4U);
  EXPECT_EQ(nullptr, font3.Get());
  EXPECT_TRUE(logchecker.HasMessage(
      "ERROR", "Unable to read data for font \"roboto_bar\"."));
}

TEST(FontManagerTest, AddFontFromFilePath) {
#if defined(ION_PLATFORM_NACL)
  // Nacl has no file reading capabilities, so just check that it errors in a
  // reasonable way.
  base::LogChecker logchecker;

  // Create a new FontManager.
  FontManagerPtr fm(new FontManager);

  // Should not be able to find the font in the manager.
  EXPECT_EQ(nullptr, fm->FindFont("roboto_foo", 32U, 4U).Get());

  // Reading an any file path should result in an error message.
  FontPtr font =
      fm->AddFontFromFilePath("roboto_foo", "does_not_exist", 32U, 4U);
  EXPECT_EQ(nullptr, font.Get());
  EXPECT_TRUE(logchecker.HasMessage(
      "ERROR",
      "Unable to read data for font \"roboto_foo\" from path "
      "\"does_not_exist\"."));
#else   // defined(ION_PLATFORM_NACL)
  base::LogChecker logchecker;
  // Write the roboto_regular font to a temporary file.
  roboto_regular_assets::RegisterAssetsOnce();
  const std::string& data =
      base::ZipAssetManager::GetFileData("roboto_regular.ttf");
  ASSERT_FALSE(base::IsInvalidReference(data));
  ASSERT_FALSE(data.empty());

  const std::string filename = port::GetTemporaryFilename();
  FILE* file = port::OpenFile(filename, "wb");
  ASSERT_TRUE(file);
  ASSERT_EQ(data.size(), fwrite(data.data(), 1U, data.size(), file));
  ASSERT_EQ(0, fclose(file));

  // Create a new FontManager.
  FontManagerPtr fm(new FontManager);

  // Should not be able to find the font in the manager yet.
  EXPECT_EQ(nullptr, fm->FindFont("roboto_foo", 32U, 4U).Get());

  // Create and add font to the map.
  FontPtr font = fm->AddFontFromFilePath("roboto_foo", filename, 32U, 4U);

  // Verify that adding the font data twice results in the same Font object.
  FontPtr font2 = fm->AddFontFromFilePath("roboto_foo", filename, 32U, 4U);
  EXPECT_EQ(font, font2);

  // Font should have been successfully created.
  EXPECT_NE(nullptr, font.Get());

  // Now should be able to find the font in the manager.
  EXPECT_NE(nullptr, fm->FindFont("roboto_foo", 32U, 4U).Get());

  // Reading an invalid file path should result in an error message.
  FontPtr font3 =
      fm->AddFontFromFilePath("roboto_bar", "does_not_exist", 32U, 4U);
  EXPECT_EQ(nullptr, font3.Get());
  EXPECT_TRUE(logchecker.HasMessage(
      "ERROR",
      "Unable to read data for font \"roboto_bar\" from path "
      "\"does_not_exist\"."));
#endif  // defined(ION_PLATFORM_NACL)
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
  EXPECT_EQ(nullptr, fm->GetCachedFontImage(key).Get());

  // Create and cache a MockFontImage.
  FontImagePtr font_image(new testing::MockFontImage());
  EXPECT_NE(nullptr, font_image.Get());
  fm->CacheFontImage(key, font_image);
  FontImagePtr f = fm->GetCachedFontImage(key);
  EXPECT_NE(nullptr, f.Get());
  EXPECT_EQ(font_image, f);

  // Replace with a different MockFontImage.
  FontImagePtr font_image2(new testing::MockFontImage());
  EXPECT_NE(nullptr, font_image2.Get());
  fm->CacheFontImage(key, font_image2);
  f = fm->GetCachedFontImage(key);
  EXPECT_NE(nullptr, f.Get());
  EXPECT_EQ(font_image2, f);

  // Replace with a nullptr pointer.
  fm->CacheFontImage(key, FontImagePtr(nullptr));
  f = fm->GetCachedFontImage(key);
  EXPECT_EQ(nullptr, f.Get());

  // Cache a MockFontImage, but use the Font (not a string) as a key.
  FontPtr font(new testing::MockFont(12, 4));
  fm->CacheFontImage(font, font_image);
  f = fm->GetCachedFontImage(font);
  EXPECT_EQ(font_image, f);
}

}  // namespace text
}  // namespace ion
