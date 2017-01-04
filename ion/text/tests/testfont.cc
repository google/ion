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

#include "ion/text/tests/testfont.h"

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
#include "ion/text/coretextfont.h"
#endif
#include "ion/text/freetypefont.h"

// Resources for the test font.
ION_REGISTER_ASSETS(IonTextTests);

namespace ion {
namespace text {
namespace testing {

const std::string& GetTestFontData() {
  IonTextTests::RegisterAssetsOnce();
  // Get the font data from the zip assets.
  static const std::string& data =
      base::ZipAssetManager::GetFileData("Tuffy.ttf");
  CHECK(!base::IsInvalidReference(data));
  CHECK(!data.empty());
  return data;
}

const std::string& GetDevanagariFontData() {
  IonTextTests::RegisterAssetsOnce();
  // Get the font data from the zip assets.
  static const std::string& data =
      base::ZipAssetManager::GetFileData("NotoSansDevanagari-Regular.ttf");
  CHECK(!base::IsInvalidReference(data));
  CHECK(!data.empty());
  return data;
}

const std::string& GetEmojiFontData() {
  IonTextTests::RegisterAssetsOnce();
  // Get the font data from the zip assets.
  static const std::string& data =
      base::ZipAssetManager::GetFileData("NotoColorEmoji.ttf");
  CHECK(!base::IsInvalidReference(data));
  CHECK(!data.empty());
  return data;
}

const std::string& GetCJKFontData() {
  IonTextTests::RegisterAssetsOnce();
  // Get the font data from the zip assets.
  static const std::string& data =
      base::ZipAssetManager::GetFileData("NotoSansCJK-Regular.ttc");
  CHECK(!base::IsInvalidReference(data));
  CHECK(!data.empty());
  return data;
}

template<class FontClass>
static FontClass* BuildTestFont(
    const std::string& name, size_t size, size_t sdf_padding) {
  const std::string& data = (name == "NotoSansDevanagari-Regular")
                             ? GetDevanagariFontData()
                             : (name == "Emoji")
                               ? GetEmojiFontData()
                               : GetTestFontData();
  return new FontClass(name, size, sdf_padding, &data[0], data.size());
}


const FontPtr BuildTestFreeTypeFont(const std::string& name,
                                    size_t size, size_t sdf_padding) {
  return FontPtr(BuildTestFont<FreeTypeFont>(name, size, sdf_padding));
}

#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
const CoreTextFontPtr BuildTestCoreTextFont(const std::string& name,
                                            size_t size, size_t sdf_padding) {
  return CoreTextFontPtr(BuildTestFont<CoreTextFont>(name, size, sdf_padding));
}
#endif

}  // namespace testing
}  // namespace text
}  // namespace ion
