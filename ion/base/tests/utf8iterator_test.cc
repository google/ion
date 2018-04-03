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

#include "ion/base/utf8iterator.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Convenience function that converts an Ascii character to a Unicode index.
static const uint32 ToUnicode(char ascii) {
  return static_cast<uint32>(ascii);
}

// Tests the given invalid UTF-8 string. The string should consist of a space
// character (' ') followed by some invalid UTF-8 character sequence, with
// optional valid characters following it.
static void TestInvalidString(const std::string& what, const std::string& s) {
  Utf8Iterator it(s);

  SCOPED_TRACE(what);

  // The space character should be returned correctly.
  EXPECT_EQ(ToUnicode(' '), it.Next());
  EXPECT_EQ(Utf8Iterator::kInString, it.GetState());

  // The next character is invalid.
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kInvalid, it.GetState());

  // Iterating past an invalid character should have no effect.
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kInvalid, it.GetState());
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kInvalid, it.GetState());
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kInvalid, it.GetState());

  // All invalid strings should result in a character count of 0.
  EXPECT_EQ(0U, it.ComputeCharCount());
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Tests.
//
//-----------------------------------------------------------------------------

TEST(Utf8Iterator, Empty) {
  Utf8Iterator it("");
  EXPECT_EQ(Utf8Iterator::kEndOfString, it.GetState());
  EXPECT_EQ(0U, it.ComputeCharCount());

  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());

  EXPECT_EQ(Utf8Iterator::kEndOfString, it.GetState());
  EXPECT_EQ(0U, it.ComputeCharCount());

  // The iterator should remain in end-of-string state.
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kEndOfString, it.GetState());
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kEndOfString, it.GetState());
}

TEST(Utf8Iterator, AsciiOnly) {
  Utf8Iterator it("abcd 0123");
  EXPECT_EQ(Utf8Iterator::kInString, it.GetState());
  EXPECT_EQ(9U, it.ComputeCharCount());
  EXPECT_EQ(0U, it.GetCurrentByteIndex());
  EXPECT_EQ(ToUnicode('a'), it.Next());
  EXPECT_EQ(ToUnicode('b'), it.Next());
  EXPECT_EQ(ToUnicode('c'), it.Next());
  EXPECT_EQ(ToUnicode('d'), it.Next());
  EXPECT_EQ(ToUnicode(' '), it.Next());
  EXPECT_EQ(ToUnicode('0'), it.Next());
  EXPECT_EQ(ToUnicode('1'), it.Next());
  EXPECT_EQ(ToUnicode('2'), it.Next());
  EXPECT_EQ(8U, it.GetCurrentByteIndex());
  EXPECT_EQ(Utf8Iterator::kInString, it.GetState());
  EXPECT_EQ(ToUnicode('3'), it.Next());
  EXPECT_EQ(Utf8Iterator::kEndOfString, it.GetState());
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
  EXPECT_EQ(Utf8Iterator::kEndOfString, it.GetState());
  EXPECT_EQ(9U, it.ComputeCharCount());
}

TEST(Utf8Iterator, AsciiAndUnicode) {
  // Construct a string that tests all edge cases of Unicode indices.
  std::string s =
      // Smallest/greatest 1-byte codes.
      "\x01"
      "\x7f"
      // Smallest/greatest 2-byte codes.
      "\xc2\x80"
      "\xdf\xbf"
      // Smallest/greatest 3-byte codes.
      "\xe0\xa0\x80"
      "\xef\xbf\xbf"
      // Smallest/greatest 4-byte codes.
      "\xf0\x90\x80\x80"
      "\xf4\x8f\xbf\xbf";
  EXPECT_EQ(2U * (1U + 2U + 3U + 4U), s.size());

  Utf8Iterator it(s);
  EXPECT_EQ(8U, it.ComputeCharCount());
  EXPECT_EQ(0U, it.GetCurrentByteIndex());
  EXPECT_EQ(0x0001U, it.Next());
  EXPECT_EQ(1U, it.GetCurrentByteIndex());
  EXPECT_EQ(0x007fU, it.Next());
  EXPECT_EQ(2U, it.GetCurrentByteIndex());
  EXPECT_EQ(0x0080U, it.Next());
  EXPECT_EQ(4U, it.GetCurrentByteIndex());
  EXPECT_EQ(0x07ffU, it.Next());
  EXPECT_EQ(6U, it.GetCurrentByteIndex());
  EXPECT_EQ(0x0800U, it.Next());
  EXPECT_EQ(9U, it.GetCurrentByteIndex());
  EXPECT_EQ(0xffffU, it.Next());
  EXPECT_EQ(12U, it.GetCurrentByteIndex());
  EXPECT_EQ(0x010000U, it.Next());
  EXPECT_EQ(16U, it.GetCurrentByteIndex());
  EXPECT_EQ(Utf8Iterator::kInString, it.GetState());
  EXPECT_EQ(0x10ffffU, it.Next());
  EXPECT_EQ(20U, it.GetCurrentByteIndex());
  EXPECT_EQ(Utf8Iterator::kEndOfString, it.GetState());
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
}

TEST(Utf8Iterator, NulInString) {
  // Check that a NUL inside of a string is handled correctly. This has to be
  // constructed specially otherwise the std::string constructor things it is a
  // NUL-terminated string.
  std::string s = "\xe2\x82\xa1";
  s.push_back(0);
  s += "\xe2\x82\xa1";
  EXPECT_EQ(7U, s.size());

  Utf8Iterator it(s);
  EXPECT_EQ(3U, it.ComputeCharCount());
  EXPECT_EQ(0x20A1U, it.Next());
  EXPECT_EQ(0x0000U, it.Next());
  EXPECT_EQ(Utf8Iterator::kInString, it.GetState());
  EXPECT_EQ(0x20A1U, it.Next());
  EXPECT_EQ(Utf8Iterator::kEndOfString, it.GetState());
  EXPECT_EQ(Utf8Iterator::kInvalidCharIndex, it.Next());
}

TEST(Utf8Iterator, OverlongEncoding) {
  TestInvalidString("Overlong space (2 bytes instead of 1)",
                    " \xc0\xa0 ");
  TestInvalidString("Overlong cent (3 bytes instead of 2)",
                    " \xe0\x82\xa2 ");
  TestInvalidString("Overlong euro sign (4 bytes instead of 3)",
                    " \xf0\x82\x82\xac ");
}

TEST(Utf8Iterator, Invalid) {
  TestInvalidString("Continuation byte as 1st byte",
                    " \x80  ");

  TestInvalidString("2-byte sequence missing continuation byte",
                    " \xc5");
  TestInvalidString("2-byte sequence with invalid continuation byte",
                    " \xc5\xc0  ");

  TestInvalidString("3-byte sequence missing 1st continuation byte",
                    " \xe1");
  TestInvalidString("3-byte sequence missing 2nd continuation byte",
                    " \xe1\xa5");
  TestInvalidString("3-byte sequence with invalid 1st continuation byte",
                    " \xe8\xc0\xab ");
  TestInvalidString("3-byte sequence with invalid 2nd continuation byte",
                    " \xef\xab\xc0  ");

  TestInvalidString("4-byte sequence missing 1st continuation byte",
                    " \xf0");
  TestInvalidString("4-byte sequence missing 2nd continuation byte",
                    " \xf1\xbb");
  TestInvalidString("4-byte sequence missing 3rd continuation byte",
                    " \xf2\xbc\xbe");
  TestInvalidString("4-byte sequence with invalid 1st continuation byte",
                    " \xf3\xc1\xa5\xb0 ");
  TestInvalidString("4-byte sequence with invalid 2nd continuation byte",
                    " \xf3\xa5\xc1\xb0  ");
  TestInvalidString("4-byte sequence with invalid 3rd continuation byte",
                    " \xf3\xa5\xb0\xc1  ");

  TestInvalidString("4-byte sequence exceeding max index by 1",
                    " \xf4\x8f\xbf\xc0  ");
  TestInvalidString("4-byte sequence exceeding max index by 2",
                    " \xf4\x8f\xbf\xc1  ");
  TestInvalidString("4-byte sequence exceeding max index by a lot",
                    " \xf4\xbf\xbf\xbf  ");
}

}  // namespace base
}  // namespace ion
