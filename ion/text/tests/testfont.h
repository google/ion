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

#ifndef ION_TEXT_TESTS_TESTFONT_H_
#define ION_TEXT_TESTS_TESTFONT_H_

#include <string>

#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
#include "ion/text/coretextfont.h"
#endif

#include "ion/text/font.h"

namespace ion {
namespace text {
namespace testing {

// Returns a reference to a string containing font data for the test font.
const std::string& GetTestFontData();
// Returns a reference to a string containing a bitmap .ttf font.
const std::string& GetEmojiFontData();
// Returns a reference to a string containing font data for an extended unicode
// font with simple layout properties.
const std::string& GetCJKFontData();

// Builds and returns a FreeType font for testing. The font has the given name,
// size, and SDF padding.
const FontPtr BuildTestFreeTypeFont(const std::string& name,
                                    size_t size, size_t sdf_padding);

#if defined(ION_PLATFORM_MAC) || defined(ION_PLATFORM_IOS)
// Builds and returns a CoreText font for testing. The font has the given name,
// size, and SDF padding.
const CoreTextFontPtr BuildTestCoreTextFont(const std::string& name,
                                            size_t size, size_t sdf_padding);
#endif

}  // namespace testing
}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_TESTS_TESTFONT_H_
