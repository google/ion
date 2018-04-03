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

#ifndef ION_BASE_STRINGUTILS_H_
#define ION_BASE_STRINGUTILS_H_

// This file contains generic utility functions that operate on strings.

#include <istream>  // NOLINT
#include <string>
#include <vector>

#include "base/integral_types.h"
#include "ion/base/stlalloc/allocvector.h"

namespace ion {
namespace base {

// Returns an escaped version of the passed string. For example:
//   EscapeString("\aBell\bNew "Line\n") returns "\\aBell\\bNew \"Line\\n";
ION_API std::string EscapeString(const std::string& str);

// Returns a string with all newlines replaced by "\\n".
ION_API std::string EscapeNewlines(const std::string& str);

// Returns a mime base-64 encoded version of the passed string.
// Output is padded with ='s (see https://en.wikipedia.org/wiki/Base64).
ION_API std::string MimeBase64EncodeString(const std::string& str);

// Splits a string into a vector of substrings, given a set of delimiter
// characters (expressed as a string). Empty strings are skipped, as are
// consecutive delimiters.
//
// For example:  SplitString(" Hello\t    there \t \n", " \t\n");
// will return a vector containing two strings, "Hello" and "there".
ION_API std::vector<std::string> SplitString(
    const std::string& str, const std::string& delimiters);

// A version of SplitString taking an Ion Allocator.
ION_API AllocVector<std::string> SplitString(
    const std::string& str, const std::string& delimiters,
    const AllocatorPtr& alloc);

// Splits a string into a vector of substrings, given a set of delimiter
// characters (expressed as a string). Empty strings are skipped, but
// consecutive delimiters are not.
//
// For example:  SplitStringWithoutSkipping("Hello\n\nthere\n", "\n");
// will return a vector containing three strings, "Hello", "", and "there".
ION_API std::vector<std::string> SplitStringWithoutSkipping(
    const std::string& str, const std::string& delimiters);

// Returns a quoted and escaped version of the passed string. For example:
//   QuoteString("Hello") returns "\"Hello\"".
inline std::string QuoteString(const std::string& val) {
  std::ostringstream out;
  out << '"' << EscapeString(val) << '"';
  return out.str();
}

// Returns whether target begins with start.
inline bool StartsWith(const std::string& target, const std::string& start) {
  return start.length() && start.compare(target.substr(0, start.size())) == 0;
}

// Returns whether target ends with end.
inline bool EndsWith(const std::string& target, const std::string& end) {
  return end.length() && end.length() <= target.length() &&
       target.compare(target.length() - end.length(), end.length(), end) == 0;
}

// Joins the strings in the passed vector together with the passed glue. The
// glue may be empty, in which case the strings are simply concatenated. If
// strings contains no strings then an empty string is returned.
inline std::string JoinStrings(const std::vector<std::string>& strings,
                                     const std::string& glue) {
  std::string joined;
  if (const size_t count = strings.size()) {
    joined = strings[0];
    for (size_t i = 1; i < count; ++i)
      joined += glue + strings[i];
  }
  return joined;
}

// Removes prefix from the beginning of target if target starts with it. Returns
// whether prefix was removed.
inline bool RemovePrefix(const std::string& prefix, std::string* target) {
  if (StartsWith(*target, prefix)) {
    *target = target->substr(prefix.length(),
                             target->length() - prefix.length());
    return true;
  }
  return false;
}

// Removes suffix from the end of target if target ends with it. Returns whether
// suffix was removed.
inline bool RemoveSuffix(const std::string& suffix, std::string* target) {
  if (EndsWith(*target, suffix)) {
    *target = target->substr(0, target->length() - suffix.length());
    return true;
  }
  return false;
}

// Returns a string with all instances of from replaced with to.
inline std::string ReplaceString(const std::string& search,
                                 const std::string& from,
                                 const std::string& to) {
  if (search.empty() || from.empty())
    return search;

  std::string replaced = search;
  const size_t to_size = to.size();
  for (size_t start_pos = replaced.find(from); start_pos != std::string::npos;
       start_pos = replaced.find(from, start_pos + to_size))
    replaced.replace(start_pos, from.length(), to);
  return replaced;
}

// Removes any whitespace characters at the beginning of the string.
inline std::string TrimStartWhitespace(const std::string& target) {
  const size_t pos = target.find_first_not_of(" \f\n\r\t\v");
  std::string trimmed;
  if (pos != std::string::npos)
    trimmed = target.substr(pos, std::string::npos);
  return trimmed;
}

// Removes any whitespace characters at the end of the string.
inline std::string TrimEndWhitespace(const std::string& target) {
  const size_t pos = target.find_last_not_of(" \f\n\r\t\v");
  std::string trimmed;
  if (pos != std::string::npos)
    trimmed = target.substr(0, pos + 1U);
  return trimmed;
}

// Removes any whitespace characters at the beginning and end of the string.
inline std::string TrimStartAndEndWhitespace(const std::string& target) {
  return TrimEndWhitespace(TrimStartWhitespace(target));
}

// Returns a decoded version of a URL-encoded string.
ION_API std::string UrlDecodeString(const std::string& str);

// Returns a URL-encoded version of a string.
ION_API std::string UrlEncodeString(const std::string& str);

// This function can be useful for comparing multi-line strings in tests.  If
// the two multi-line strings are equal, it just returns true. Otherwise, it
// splits the strings by newlines and determines the first line that
// differs. It sets first_different_index to the index (starting a 0) of that
// line, sets line0 and line1 to the contents of those lines in the two
// strings, sets context0 and context1 to be the lines near where the difference
// was found, and returns false. Any of the out-parameters may be NULL.
ION_API bool AreMultiLineStringsEqual(
    const std::string& s0, const std::string& s1,
    size_t* first_different_index, std::string* line0, std::string* line1,
    std::string* context0, std::string* context1);

// Case-insensitive comparison of str1 and str2.
ION_API int CompareCaseInsensitive(const std::string& str1,
                                   const std::string& str2);

// Returns whether target begins with start (case-insensitive).
ION_API bool StartsWithCaseInsensitive(const std::string& target,
                                       const std::string& start);

// Returns whether target ends with end (case-insensitive).
ION_API bool EndsWithCaseInsensitive(const std::string& target,
                                     const std::string& end);

// Case-insensitive version of std::string find.
ION_API int FindCaseInsensitive(const std::string& target,
                                const std::string& substr);

// Decodes a Base64 encoded string. Follows the RFC 4648 standard,
// accepting either base64 or base64url encoding:
//   * base64: 62 is '+', 63 is '/', and = is for padding.
//   * base64url: 62 is '-', 63 is '_', and no padding is used.
//
// In this implementation, base64 is transformed to base64url before
// decoding the string.
//
// A zero-length array is returned if the decode fails.
ION_API std::string WebSafeBase64Decode(const std::string& str);

// Encodes a byte array using RFC 4648 base64url ('-' and '_' for 62
// and 63, respectively, and no padding).  The returned string will be
// safe for use in URLs.
ION_API std::string WebSafeBase64Encode(const std::string& input);

// Reads a single character from the stream and returns the stream. If the read
// character does not match the expected char, then this ungets the character
// and sets the stream's failure bit. This function can be used in various ways,
// such as in a boolean test (istream automatically casts to bool), or inline
// with the >> operator.
template <char expected>
inline std::istream& GetExpectedChar(std::istream& in) {  // NOLINT
  char c = expected + 1;
  if (in >> c && c != expected) {
    // Restore the stream.
    in.unget();
    in.setstate(std::ios_base::failbit);
  }
  return in;
}

// Attempts to read a string from the stream and returns the stream. If the
// stream does not start with the expected string, then this ungets the read
// portion of the string and sets the stream's failure bit.
inline std::istream& GetExpectedString(std::istream& in,  // NOLINT
                                       const std::string& expected) {
  if (!in.fail()) {
    const size_t length = expected.length();
    for (size_t i = 0; i < length; ++i) {
      char c;
      in >> c;
      if (!in.good() || c != expected[i]) {
        // Restore the stream.
        for (size_t j = 0; j <= i; ++j)
          in.unget();
        in.setstate(std::ios_base::failbit);
        break;
      }
    }
  }
  return in;
}

// Extracts and returns an integral value from str. If str does not start with
// an integer then returns 0.
ION_API int32 StringToInt32(const std::string& str);

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STRINGUTILS_H_
