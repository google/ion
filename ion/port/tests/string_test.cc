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

#include "ion/port/string.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace port {

TEST(String, strnlen) {
  const char str1[] = "Hello";
  EXPECT_EQ(strnlen(str1, 10), 5U);
  EXPECT_EQ(strnlen(str1, 5), 5U);
  EXPECT_EQ(strnlen(str1, 3), 3U);

  const char str2[] = "Good\0bye";
  EXPECT_EQ(strnlen(str2, 8), 4U);
  EXPECT_EQ(strnlen(str2, 5), 4U);
}

#if defined(ION_PLATFORM_WINDOWS)
TEST(String, Utf8ToAndFromWide) {
  // Greek small letters alpha, beta, and gamma.
  const std::string utf8 = "\xCE\xB1\xCE\xB2\xCE\xB3";
  const std::wstring wide = L"\x03B1\x03B2\x03B3";
  EXPECT_EQ(wide, ion::port::Utf8ToWide(utf8));
  EXPECT_EQ(utf8, ion::port::WideToUtf8(wide));
}
#endif

}  // namespace port
}  // namespace ion
