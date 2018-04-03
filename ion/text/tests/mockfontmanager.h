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

#ifndef ION_TEXT_TESTS_MOCKFONTMANAGER_H_
#define ION_TEXT_TESTS_MOCKFONTMANAGER_H_

#include "testing/base/public/gmock.h"

#include "ion/text/fontmanager.h"

namespace ion {
namespace text {
namespace testing {

class MockFontManager : public FontManager {
 public:
  MockFontManager() {}

  MOCK_METHOD1(AddFont, void(const FontPtr& font));
  MOCK_METHOD5(AddFont, const FontPtr(const std::string& name,
                                      size_t size_in_pixels, size_t sdf_padding,
                                      const void* data, size_t data_size));
  MOCK_METHOD4(AddFontFromZipasset,
               const FontPtr(const std::string& font_name,
                             const std::string& zipasset_name,
                             size_t size_in_pixels, size_t sdf_padding));
  MOCK_METHOD4(AddFontFromFilePath,
               const FontPtr(const std::string& font_name,
                             const std::string& file_path,
                             size_t size_in_pixels, size_t sdf_padding));
  MOCK_CONST_METHOD3(FindFont,
                     const FontPtr(const std::string& name,
                                   size_t size_in_pixels, size_t sdf_padding));
  MOCK_METHOD2(CacheFontImage,
               void(const std::string& key, const FontImagePtr& font_image));
  MOCK_METHOD2(CacheFontImage,
               void(const FontPtr& font, const FontImagePtr& font_image));
  MOCK_CONST_METHOD1(GetCachedFontImage,
                     const FontImagePtr(const std::string& key));
  MOCK_CONST_METHOD1(GetCachedFontImage,
                     const FontImagePtr(const FontPtr& font));
};

}  // namespace testing
}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_TESTS_MOCKFONTMANAGER_H_
