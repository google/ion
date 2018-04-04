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

#include "ion/base/stringutils.h"

#include <sstream>
#include <string>
#include <vector>

#include "ion/base/tests/testallocator.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace {

static void VerifyRemovePrefix(const std::string& final,
                               const std::string& initial,
                               const std::string& prefix) {
  std::string test_string = initial;
  EXPECT_EQ(initial != final, ion::base::RemovePrefix(prefix, &test_string));
  EXPECT_EQ(final, test_string);
}

static void VerifyRemoveSuffix(const std::string& final,
                               const std::string& initial,
                               const std::string& suffix) {
  std::string test_string = initial;
  EXPECT_EQ(initial != final, ion::base::RemoveSuffix(suffix, &test_string));
  EXPECT_EQ(final, test_string);
}

// Though encode and decode work with std::string, accept
// const char* here to simplify test code.
static void VerifyWebSafeBase64EncodeDecode(const char* encoded,
                                            const char* decoded) {
  EXPECT_EQ(std::string(decoded),
            ion::base::WebSafeBase64Decode(std::string(encoded)));
  EXPECT_EQ(std::string(encoded),
            ion::base::WebSafeBase64Encode(std::string(decoded)));
}

}  // anonymous namespace

namespace ion {
namespace base {

TEST(StringUtils, MimeBase64EncodeString) {
  EXPECT_EQ("", MimeBase64EncodeString(""));
  EXPECT_EQ("Zm9v", MimeBase64EncodeString("foo"));
  EXPECT_EQ("Zm9vCg==", MimeBase64EncodeString("foo\n"));
  EXPECT_EQ("YmFy", MimeBase64EncodeString("bar"));
  EXPECT_EQ("Zm9vIGJhcg==", MimeBase64EncodeString("foo bar"));
  EXPECT_EQ("Zm9vCmJhciBiYXQ=", MimeBase64EncodeString("foo\nbar bat"));
  EXPECT_EQ("CmNvdwltb28gDCBpY2UgY3JlYW0=",
            MimeBase64EncodeString("\ncow\tmoo \f ice cream"));
}

TEST(StringUtils, EscapeString) {
  EXPECT_EQ("", EscapeString(""));
  EXPECT_EQ("\\\"", EscapeString("\""));
  EXPECT_EQ("\\\"\\\"", EscapeString("\"\""));
  EXPECT_EQ("foo\\abar\\b", EscapeString("foo\abar\b"));
  EXPECT_EQ("foo\\nbar", EscapeString("foo\nbar"));
  EXPECT_EQ("new line\\r\\n\\f", EscapeString("new line\r\n\f"));
  EXPECT_EQ("\\ttabulated\\t", EscapeString("\ttabulated\t"));
  EXPECT_EQ("v\\valigned", EscapeString("v\valigned"));
  EXPECT_EQ("quotes\\' and double quotes \\\"",
            EscapeString("quotes\' and double quotes \""));
  EXPECT_EQ("\\?\\n\\\\\\t\\\\\\?\\f",
            EscapeString("\?\n\\\t\\\?\f"));
}

TEST(StringUtils, EscapeNewlines) {
  EXPECT_EQ("", EscapeNewlines(""));
  EXPECT_EQ("\\n", EscapeNewlines("\n"));
  EXPECT_EQ("new line\r\\n\f", EscapeNewlines("new line\r\n\f"));
  EXPECT_EQ("\\n\\n", EscapeNewlines("\n\n"));
  EXPECT_EQ("\\ n", EscapeNewlines("\\ n"));
  EXPECT_EQ("\"\\n\"", EscapeNewlines("\"\n\""));
}

TEST(StringUtils, SplitString) {
  std::vector<std::string> s;

  // Empty string, no delimiters.
  s = SplitString("", "");
  EXPECT_EQ(0U, s.size());

  // Empty string, delimiters.
  s = SplitString("", " ");
  EXPECT_EQ(0U, s.size());

  // Non-empty string, no delimiters.
  s = SplitString(" foo bar ", "");
  EXPECT_EQ(1U, s.size());
  EXPECT_EQ(" foo bar ", s[0]);

  // Non-empty string, single delimiter.
  s = SplitString(".abc..de....fgh....i..", ".");
  EXPECT_EQ(4U, s.size());
  EXPECT_EQ("abc", s[0]);
  EXPECT_EQ("de", s[1]);
  EXPECT_EQ("fgh", s[2]);
  EXPECT_EQ("i", s[3]);

  // Non-empty string, multiple delimiters.
  s = SplitString(" Hello\t    there \t \n", " \t\n");
  EXPECT_EQ(2U, s.size());
  EXPECT_EQ("Hello", s[0]);
  EXPECT_EQ("there", s[1]);

  // AllocVector version.
  testing::TestAllocatorPtr alloc(new testing::TestAllocator);
  AllocVector<std::string> sa = SplitString(" Hello\tWorld", " \t", alloc);
  EXPECT_EQ(2U, sa.size());
  EXPECT_EQ("Hello", sa[0]);
  EXPECT_EQ("World", sa[1]);
  EXPECT_LT(0U, alloc->GetBytesAllocated());
}

TEST(StringUtils, SplitStringWithoutSkipping) {
  std::vector<std::string> s;

  // Empty string, no delimiters.
  s = SplitStringWithoutSkipping("", "");
  EXPECT_EQ(0U, s.size());

  // Empty string, delimiters.
  s = SplitStringWithoutSkipping("", " ");
  EXPECT_EQ(0U, s.size());

  // Non-empty string, no delimiters.
  s = SplitStringWithoutSkipping(" foo bar ", "");
  EXPECT_EQ(1U, s.size());
  EXPECT_EQ(" foo bar ", s[0]);

  // Non-empty string, single delimiter.
  s = SplitStringWithoutSkipping(".abc..de....fgh....i..", ".");
  EXPECT_EQ(13U, s.size());
  EXPECT_EQ("", s[0]);
  EXPECT_EQ("abc", s[1]);
  EXPECT_EQ("", s[2]);
  EXPECT_EQ("de", s[3]);
  EXPECT_EQ("", s[4]);
  EXPECT_EQ("", s[5]);
  EXPECT_EQ("", s[6]);
  EXPECT_EQ("fgh", s[7]);
  EXPECT_EQ("", s[8]);
  EXPECT_EQ("", s[9]);
  EXPECT_EQ("", s[10]);
  EXPECT_EQ("i", s[11]);
  EXPECT_EQ("", s[12]);

  // Non-empty string, multiple delimiters.
  s = SplitStringWithoutSkipping(" Hello\t    there \t \n", " \t\n");
  EXPECT_EQ(10U, s.size());
  EXPECT_EQ("", s[0]);
  EXPECT_EQ("Hello", s[1]);
  EXPECT_EQ("", s[2]);
  EXPECT_EQ("", s[3]);
  EXPECT_EQ("", s[4]);
  EXPECT_EQ("", s[5]);
  EXPECT_EQ("there", s[6]);
  EXPECT_EQ("", s[7]);
  EXPECT_EQ("", s[8]);
  EXPECT_EQ("", s[9]);
}

TEST(StringUtils, QuoteString) {
  std::string str = "With a \" quote";
  std::string quoted = QuoteString(str);
  EXPECT_EQ("\"With a \\\" quote\"", quoted);
}

TEST(StringUtils, StartsWith) {
  EXPECT_TRUE(StartsWith("Hello, world!", "Hel"));
  EXPECT_TRUE(StartsWith("Hello, world!", "Hello,"));
  EXPECT_FALSE(StartsWith("Hello, world!", "hello"));
  EXPECT_FALSE(StartsWith("Hello, world!", "Goodbye"));

  EXPECT_TRUE(StartsWith("foo bar", "foo"));
  EXPECT_TRUE(StartsWith("foo", "foo"));
  EXPECT_FALSE(StartsWith("foo bar", "bar"));
  EXPECT_FALSE(StartsWith("foo bar", "foo bar cow"));
  EXPECT_FALSE(StartsWith("foo bar", ""));
}

TEST(StringUtils, EndsWith) {
  EXPECT_TRUE(EndsWith("Hello, world!", "ld!"));
  EXPECT_TRUE(EndsWith("Hello, world!", "world!"));
  EXPECT_FALSE(EndsWith("Hello, world!", "lD!"));
  EXPECT_FALSE(EndsWith("Hello, world!", "Goodbye"));

  EXPECT_TRUE(EndsWith("foo bar", "bar"));
  EXPECT_TRUE(EndsWith("foo", "foo"));
  EXPECT_FALSE(EndsWith("foo bar", "foo"));
  EXPECT_FALSE(EndsWith("foo bar", "cow foo bar"));
  EXPECT_FALSE(EndsWith("foo bar", ""));
}

TEST(StringUtils, JoinStrings) {
  std::vector<std::string> strings;
  strings.push_back("foo");
  strings.push_back("bar");
  strings.push_back("cat");
  strings.push_back("dog");
  EXPECT_EQ("foobarcatdog", JoinStrings(strings, ""));
  EXPECT_EQ("foobarcatdog", JoinStrings(strings, std::string()));
  EXPECT_EQ("foo bar cat dog", JoinStrings(strings, " "));
  EXPECT_EQ("foo\nbar\ncat\ndog", JoinStrings(strings, "\n"));
  EXPECT_EQ("foo a bar a cat a dog", JoinStrings(strings, " a "));
}

TEST(StringUtils, RemovePrefix) {
  VerifyRemovePrefix("world!", "Hello, world!", "Hello, ");
  VerifyRemovePrefix("ello, world!", "Hello, world!", "H");
  VerifyRemovePrefix("Hello, world!", "Hello, world!", "");
  VerifyRemovePrefix("Hello, world!", "Hello, world!", " ");
  VerifyRemovePrefix("Hello, world!", "Hello, world!", "ello");
  VerifyRemovePrefix("Hello, world!", "Hello Hello, world!", "Hello ");
  VerifyRemovePrefix("Hello, world world", "Hello, world world", "Hello ");
}

TEST(StringUtils, RemoveSuffix) {
  VerifyRemoveSuffix("Hello, ", "Hello, world!", "world!");
  VerifyRemoveSuffix("Hello, world", "Hello, world!", "!");
  VerifyRemoveSuffix("Hello, world!", "Hello, world!", "");
  VerifyRemoveSuffix("Hello, world!", "Hello, world!", " ");
  VerifyRemoveSuffix("Hello, world!", "Hello, world!", "world");
  VerifyRemoveSuffix("Hello, world", "Hello, world world", " world");
  VerifyRemoveSuffix("Hello, world world", "Hello, world world", "world ");
}

TEST(StringUtils, ReplaceString) {
  EXPECT_EQ("", ReplaceString("", "", ""));
  EXPECT_EQ("foo", ReplaceString("foo", "", ""));
  EXPECT_EQ("foo", ReplaceString("foo", "", "d"));
  EXPECT_EQ("foo", ReplaceString("food", "d", ""));
  EXPECT_EQ("", ReplaceString("", "a", "b"));
  EXPECT_EQ("Hello world!", ReplaceString("Hello planet!", "planet", "world"));
  EXPECT_EQ("Foo, dood, doodie",
            ReplaceString("Foo, food, foodie", "foo", "doo"));
  EXPECT_EQ("Hello world!", ReplaceString("Hello world!", "planet", "star"));

  // Make sure replacing with search pattern does not loop forever.
  EXPECT_EQ("ababab", ReplaceString("aaa", "a", "ab"));
  EXPECT_EQ("acbacbac", ReplaceString("ababa", "a", "ac"));
  EXPECT_EQ("GlorpGlorpGlorp", ReplaceString("GlGlGl", "Gl", "Glorp"));
}

TEST(StringUtils, TrimStartWhitespace) {
  EXPECT_EQ("", TrimStartWhitespace(" \t"));
  EXPECT_EQ("", TrimStartWhitespace("\n\n\n"));
  EXPECT_EQ("", TrimStartWhitespace(""));
  EXPECT_EQ("", TrimStartWhitespace(" "));
  EXPECT_EQ("foo bar", TrimStartWhitespace("foo bar"));
  EXPECT_EQ("foo", TrimStartWhitespace(" foo"));
  EXPECT_EQ("foo", TrimStartWhitespace("\tfoo"));
  EXPECT_EQ("foo", TrimStartWhitespace("\nfoo"));
  EXPECT_EQ("foo", TrimStartWhitespace("\rfoo"));
  EXPECT_EQ("foo", TrimStartWhitespace("\ffoo"));
  EXPECT_EQ("foo", TrimStartWhitespace("\vfoo"));
  EXPECT_EQ("foo", TrimStartWhitespace("\t\rfoo"));
  EXPECT_EQ("foo", TrimStartWhitespace("\n\rfoo"));
  EXPECT_EQ("foo", TrimStartWhitespace("\r \t\nfoo"));
  EXPECT_EQ("foo \t\n", TrimStartWhitespace("\t\rfoo \t\n"));
  EXPECT_EQ("foo bar \f \n", TrimStartWhitespace("\vfoo bar \f \n"));
  EXPECT_EQ("foo bar", TrimStartWhitespace(" \v \nfoo bar"));
  EXPECT_EQ("foo bar\t", TrimStartWhitespace("  \r\nfoo bar\t"));
}

TEST(StringUtils, TrimEndWhitespace) {
  EXPECT_EQ("", TrimEndWhitespace(" \t"));
  EXPECT_EQ("", TrimEndWhitespace("\n\n\n"));
  EXPECT_EQ("", TrimEndWhitespace(""));
  EXPECT_EQ("", TrimEndWhitespace(" "));
  EXPECT_EQ("foo bar", TrimEndWhitespace("foo bar"));
  EXPECT_EQ("foo", TrimEndWhitespace("foo "));
  EXPECT_EQ("foo", TrimEndWhitespace("foo\t"));
  EXPECT_EQ("foo", TrimEndWhitespace("foo\n"));
  EXPECT_EQ("foo", TrimEndWhitespace("foo\r"));
  EXPECT_EQ("foo", TrimEndWhitespace("foo\f"));
  EXPECT_EQ("foo", TrimEndWhitespace("foo\v"));
  EXPECT_EQ("foo", TrimEndWhitespace("foo\r\t"));
  EXPECT_EQ("foo", TrimEndWhitespace("foo\r\n"));
  EXPECT_EQ("foo", TrimEndWhitespace("foo\r \t\n"));
  EXPECT_EQ("\t\rfoo", TrimEndWhitespace("\t\rfoo \t\n"));
  EXPECT_EQ("\vfoo bar", TrimEndWhitespace("\vfoo bar \f \n"));
  EXPECT_EQ("foo bar", TrimEndWhitespace("foo bar \v \n"));
  EXPECT_EQ(" \tfoo bar", TrimEndWhitespace(" \tfoo bar  \r\n"));
}

TEST(StringUtils, TrimStartAndEndWhitespace) {
  EXPECT_EQ("", TrimStartAndEndWhitespace(" \t"));
  EXPECT_EQ("", TrimStartAndEndWhitespace("\n\n\n"));
  EXPECT_EQ("", TrimStartAndEndWhitespace(""));
  EXPECT_EQ("", TrimStartAndEndWhitespace(" "));
  EXPECT_EQ("", TrimStartAndEndWhitespace("  "));
  EXPECT_EQ("foo bar", TrimStartAndEndWhitespace("foo bar"));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace(" foo "));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("\rfoo\t "));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("  foo\n\t"));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("\nfoo\r"));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("foo\f"));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("\f\ffoo\v"));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("foo\r\t"));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("\r\tfoo\r\n"));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("foo\r \t\n"));
  EXPECT_EQ("foo", TrimStartAndEndWhitespace("\t\rfoo \t\n"));
  EXPECT_EQ("foo bar", TrimStartAndEndWhitespace("\vfoo bar \f \n"));
  EXPECT_EQ("foo bar", TrimStartAndEndWhitespace("foo bar \v \n"));
  EXPECT_EQ("foo bar",
            TrimStartAndEndWhitespace(" \tfoo bar  \r\n"));
}

TEST(StringUtils, UrlEncodeString) {
  EXPECT_EQ("foobar", UrlEncodeString("foobar"));
  EXPECT_EQ("foo%20bar", UrlEncodeString("foo bar"));
  EXPECT_EQ("%3c%3afoo%20%26bar%21%20%5c%27%22%3e",
            UrlEncodeString("<:foo &bar! \\\'\">"));
  EXPECT_EQ("unescaped._-$,;~()", UrlEncodeString("unescaped._-$,;~()"));
  EXPECT_EQ("C%c3%b4te%20d%e2%80%99Ivoire", UrlEncodeString("Côte d’Ivoire"));
}

TEST(StringUtils, UrlDecodeString) {
  EXPECT_EQ("", UrlDecodeString(""));
  EXPECT_EQ("a", UrlDecodeString("a"));
  EXPECT_EQ("ab", UrlDecodeString("ab"));
  EXPECT_EQ("foo", UrlDecodeString("foo"));
  EXPECT_EQ("f%oo", UrlDecodeString("f%oo"));
  EXPECT_EQ("fo%o", UrlDecodeString("fo%o"));
  EXPECT_EQ("fo%o%", UrlDecodeString("fo%o%"));
  EXPECT_EQ("fo%%o", UrlDecodeString("fo%%o"));
  EXPECT_EQ(std::string(), UrlDecodeString(std::string()));
  EXPECT_EQ("foobar", UrlDecodeString("foobar"));
  EXPECT_EQ("foo bar", UrlDecodeString("foo%20bar"));
  EXPECT_EQ("<:foo &bar! \\\'\">",
            UrlDecodeString("%3c%3afoo%20%26bar%21%20%5c%27%22%3e"));
  EXPECT_EQ("<:foo &bAr! \\\'\">",
            UrlDecodeString("%3c%3afoo%20%26bAr%21%20%5C%27%22%3E"));
  EXPECT_EQ("unescaped._-$,;~()", UrlDecodeString("unescaped._-$,;~()"));
  EXPECT_EQ("Côte d’Ivoire ", UrlDecodeString("C%C3%B4te+d%e2%80%99Ivoire+"));
}

TEST(StringUtils, AreMultiLineStringsEqual) {
  // Equal strings.
  EXPECT_TRUE(AreMultiLineStringsEqual("A\nBC  \nDEF\n\n\nGHI\n",
                                       "A\nBC  \nDEF\n\n\nGHI\n",
                                       nullptr, nullptr, nullptr, nullptr,
                                       nullptr));
  size_t index;
  std::string line0, line1, context0, context1;

  // Strings differ, NULL out-pointers.
  EXPECT_FALSE(AreMultiLineStringsEqual("A\nBC  \nDEF\n\n\nGHX\nIJ",
                                        "A\nBC  \nDEF\n\n\nGHY\nIJ",
                                        nullptr, nullptr, nullptr, nullptr,
                                        nullptr));

  // Strings differ, non-NULL out-pointers.
  EXPECT_FALSE(AreMultiLineStringsEqual("A\nBC  \nDEF\n\n\nGHX\nIJ",
                                        "A\nBC  \nDEF\n\n\nGHY\nIJ",
                                        &index, &line0, &line1,
                                        &context0, &context1));
  EXPECT_EQ(3U, index);
  EXPECT_EQ(std::string("GHX"), line0);
  EXPECT_EQ(std::string("GHY"), line1);
  EXPECT_EQ(
      std::string("    0: A\n    1: BC  \n    2: DEF\n    3: GHX\n    4: IJ\n"),
      context0);
  EXPECT_EQ(
      std::string("    0: A\n    1: BC  \n    2: DEF\n    3: GHY\n    4: IJ\n"),
      context1);

  // Strings differ after last line in one vector.
  EXPECT_FALSE(AreMultiLineStringsEqual("A\nBC  \nDEF\nG",
                                                   "A\nBC  \nDEF",
                                                   &index, &line0, &line1,
                                                   &context0, &context1));
  EXPECT_EQ(3U, index);
  EXPECT_EQ(std::string("G"), line0);
  EXPECT_EQ(std::string("<missing>"), line1);
  EXPECT_EQ(
      std::string("    0: A\n    1: BC  \n    2: DEF\n    3: G\n"),
      context0);
  EXPECT_EQ(
      std::string("    0: A\n    1: BC  \n    2: DEF\n"),
      context1);

  // And in the other vector.
  EXPECT_FALSE(AreMultiLineStringsEqual("",
                                                   "A",
                                                   &index, &line0, &line1,
                                                   &context0, &context1));
  EXPECT_EQ(0U, index);
  EXPECT_EQ(std::string("<missing>"), line0);
  EXPECT_EQ(std::string("A"), line1);
  EXPECT_EQ(std::string(""), context0);
  EXPECT_EQ(std::string("    0: A\n"), context1);

  // Strings that differ because of different number of blank lines, but no
  // other differences.
  EXPECT_TRUE(AreMultiLineStringsEqual("A\nBC  \nDEF\n\n\nGHI\n\n\n",
                                                  "A\nBC  \nDEF\n\n\nGHI\n",
                                                  nullptr, nullptr, nullptr,
                                                  nullptr, nullptr));
}

TEST(StringUtils, GetExpectedChar) {
  {
    std::istringstream in("abc");
    in >> GetExpectedChar<'a'> >> GetExpectedChar<'b'> >> GetExpectedChar<'c'>;
    EXPECT_TRUE(in.good());
  }
  {
    std::istringstream in("abc");
    in >> GetExpectedChar<'b'> >> GetExpectedChar<'a'> >> GetExpectedChar<'c'>;
    EXPECT_TRUE(in.fail());

    // The stream should not have been advanced.
    in.clear();
    in >> GetExpectedChar<'a'> >> GetExpectedChar<'b'> >> GetExpectedChar<'c'>;
    EXPECT_TRUE(in.good());
  }
  {
    std::istringstream in("abc");
    in >> GetExpectedChar<'a'> >> GetExpectedChar<'a'> >> GetExpectedChar<'c'>;
    EXPECT_TRUE(in.fail());

    // The stream should not have been advanced.
    in.clear();
    in >> GetExpectedChar<'b'> >> GetExpectedChar<'c'>;
    EXPECT_TRUE(in.good());
  }
  {
    std::istringstream in("");
    in >> GetExpectedChar<'a'>;
    EXPECT_TRUE(in.fail());
  }
  {
    std::istringstream in("aa");
    in >> GetExpectedChar<'a'> >> GetExpectedChar<'a'>;
    EXPECT_TRUE(in.good());
    in >> GetExpectedChar<'a'>;
    EXPECT_TRUE(in.fail());
  }
  {
    std::istringstream in("aa");
    EXPECT_TRUE(GetExpectedChar<'a'>(in).good());
    EXPECT_TRUE(GetExpectedChar<'a'>(in).good());
    EXPECT_TRUE(GetExpectedChar<'a'>(in).fail());
    EXPECT_TRUE(in.fail());
  }
}

TEST(StringUtils, GetExpectedString) {
  {
    std::istringstream in("foobar");
    EXPECT_TRUE(GetExpectedString(in, "foo").good());
    // The empty string should do nothing.
    EXPECT_TRUE(GetExpectedString(in, "").good());
    EXPECT_TRUE(GetExpectedString(in, "bar").good());
    EXPECT_TRUE(GetExpectedString(in, "foo").fail());
  }
  {
    std::istringstream in("foobar");
    EXPECT_TRUE(GetExpectedString(in, "bar").fail());

    // The stream should not have been advanced.
    in.clear();
    EXPECT_TRUE(GetExpectedString(in, "foobar").good());
  }
  {
    std::istringstream in("foobar");
    EXPECT_TRUE(GetExpectedString(in, "fao").fail());

    // The stream should not have been advanced.
    in.clear();
    EXPECT_TRUE(GetExpectedString(in, "foobar").good());
  }
}

TEST(StringUtils, StringToInt32) {
  EXPECT_EQ(14, StringToInt32("14"));
  EXPECT_EQ(14, StringToInt32("14abc"));
  EXPECT_EQ(0, StringToInt32("a14bc"));
  EXPECT_EQ(-5, StringToInt32("-5"));
  EXPECT_EQ(-5, StringToInt32("-5e3"));
  EXPECT_EQ(0, StringToInt32("--5"));
  EXPECT_EQ(0, StringToInt32(""));
  EXPECT_EQ(0, StringToInt32(" StringToInt32("));
  EXPECT_EQ(0, StringToInt32(" StringToInt32("));
  EXPECT_EQ(14, StringToInt32(" 14"));
  EXPECT_EQ(1, StringToInt32("1 14"));
  EXPECT_EQ(0, StringToInt32("q1 14"));
}

TEST(StringUtils, CompareCaseInsensitive) {
  EXPECT_EQ(0, CompareCaseInsensitive("hello", "hello"));
  EXPECT_EQ(0, CompareCaseInsensitive("HELLO", "hello"));
  EXPECT_EQ(-1, CompareCaseInsensitive("Hallo", "hello"));
  EXPECT_EQ(1, CompareCaseInsensitive("HelloHello", "hello"));
  EXPECT_EQ(1, CompareCaseInsensitive("helloABC", "HelloA"));
  EXPECT_EQ(-1, CompareCaseInsensitive("ello", "Hello"));
}

TEST(StringUtils, StartsWithCaseInsensitive) {
  EXPECT_TRUE(StartsWithCaseInsensitive("hello", "hello"));
  EXPECT_TRUE(StartsWithCaseInsensitive("HELLO123", "hello"));
  EXPECT_TRUE(StartsWithCaseInsensitive("HelLO", "hEllO"));
  EXPECT_FALSE(StartsWithCaseInsensitive("HelLO", "hello!"));
  EXPECT_FALSE(StartsWithCaseInsensitive("123hello", "hello"));
}

TEST(StringUtils, EndsWithCaseInsensitive) {
  EXPECT_TRUE(EndsWithCaseInsensitive("hello", "hello"));
  EXPECT_TRUE(EndsWithCaseInsensitive("hiHELLO", "hello"));
  EXPECT_TRUE(EndsWithCaseInsensitive("123HelLO", "hEllO"));
  EXPECT_FALSE(EndsWithCaseInsensitive("HelLO", "hello!"));
  EXPECT_FALSE(EndsWithCaseInsensitive("hello123", "hello"));
}

TEST(StringUtils, FindCaseInsensitive) {
  EXPECT_EQ(0, FindCaseInsensitive("hello", "hello"));
  EXPECT_EQ(0, FindCaseInsensitive("HELLO", "hello"));
  EXPECT_EQ(3, FindCaseInsensitive("123Hello", "hello"));
  EXPECT_EQ(3, FindCaseInsensitive("123HelloHello", "hello"));
  EXPECT_EQ(-1, FindCaseInsensitive("123ello", "hello"));
  EXPECT_EQ(-1, FindCaseInsensitive("123hello", "hello!"));
  EXPECT_EQ(-1, FindCaseInsensitive("123", "hello"));
  EXPECT_EQ(-1, FindCaseInsensitive("123", ""));
  EXPECT_EQ(-1, FindCaseInsensitive("", "hello"));
}

TEST(stringutils, WebSafeBase64EncodeDecode) {
  VerifyWebSafeBase64EncodeDecode("", "");
  VerifyWebSafeBase64EncodeDecode("Zm9v", "foo");
  VerifyWebSafeBase64EncodeDecode("Zm9vCg", "foo\n");
  VerifyWebSafeBase64EncodeDecode("YmFy", "bar");
  VerifyWebSafeBase64EncodeDecode("Zm9vIGJhcg", "foo bar");
  VerifyWebSafeBase64EncodeDecode("Zm9vCmJhciBiYXQ", "foo\nbar bat");
  VerifyWebSafeBase64EncodeDecode("CmNvdwltb28gDCBpY2UgY3JlYW0",
                                  "\ncow\tmoo \f ice cream");
}

}  // namespace base
}  // namespace ion
