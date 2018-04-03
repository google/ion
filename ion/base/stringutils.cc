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

#include <ctype.h>

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "ion/base/logging.h"
#include "third_party/omaha/omaha/base/security/b64.h"

namespace ion {
namespace base {

namespace {

inline static int HexToChar(int c) {
  const int l = tolower(c);
  return isdigit(l) ? l - '0' : l - 'W';
}

static char ToUpper(char c) {
  // Wrapper to remove overload ambiguity.
  return static_cast<char>(toupper(c));
}

static bool LessI(char c1, char c2) {
  return toupper(c1) < toupper(c2);
}

static bool EqualI(char c1, char c2) {
  return toupper(c1) == toupper(c2);
}

}  // anonymous namespace

std::string ION_API MimeBase64EncodeString(const std::string& str) {
  static const char alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  // Calculate the length of the encoded string.
  const size_t length = str.length();
  // There are 4 output bytes for every 3 in the input.
  size_t dest_length = (length / 3U) * 4U;
  // If there are any leftover bytes...
  const size_t overflow = length % 3U;
  // ... then we need space for that and an additional byte.
  if (overflow)
    dest_length += overflow + 1U;

  // Pad the escaped string to have a length that is a multiple of 4.
  const size_t padding_length = (4U - (dest_length % 4U)) % 4U;
  dest_length += padding_length;

  std::string dest;
  dest.reserve(dest_length);
  for (size_t source_pos = 0U; source_pos < length; source_pos += 3) {
    const uint8 octet_a = str[source_pos];
    const uint8 octet_b = static_cast<uint8>(
        source_pos + 1 >= length ? 0 : str[source_pos + 1]);
    const uint8 octet_c = static_cast<uint8>(
        source_pos + 2 >= length ? 0 : str[source_pos + 2]);

    // Write the six high bits of the first octet.
    dest.push_back(alphabet[octet_a >> 2]);
    // Write the low two bits of the first octet and the high four bits of the
    // second.
    dest.push_back(alphabet[((octet_a & 0x3) << 4) | (octet_b >> 4)]);
    // Write the low four bits of the second octet and the high two bits of the
    // third.
    if (source_pos + 1U < length)
      dest.push_back(alphabet[(octet_b & 0xf) << 2 | (octet_c >> 6)]);
    // Write the low six bits of the third octet.
    if (source_pos + 2U < length)
      dest.push_back(alphabet[octet_c & 0x3f]);
  }
  for (size_t i = 0; i < padding_length; ++i)
    dest.push_back('=');

  DCHECK_EQ(dest_length, dest.length());

  return dest;
}

std::string ION_API EscapeString(const std::string& str) {
  const size_t length = str.length();
  std::string out;
  out.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    switch (str[i]) {
      case '\a':
        out.append("\\a");
        break;
      case '\b':
        out.append("\\b");
        break;
      case '\f':
        out.append("\\f");
        break;
      case '\n':
        out.append("\\n");
        break;
      case '\r':
        out.append("\\r");
        break;
      case '\t':
        out.append("\\t");
        break;
      case '\v':
        out.append("\\v");
        break;
      case '\\':
        out.append("\\\\");
        break;
      case '\'':
        out.append("\\'");
        break;
      case '\"':
        out.append("\\\"");
        break;
      case '\?':
        out.append("\\\?");
        break;
      default:
        out.append(1, str[i]);
        break;
    }
  }
  return out;
}

std::string ION_API EscapeNewlines(const std::string& str) {
  const size_t length = str.length();
  std::string out;
  out.reserve(length);
  for (size_t i = 0; i < length; ++i) {
    switch (str[i]) {
      case '\n':
        out.append("\\n");
        break;
      default:
        out.append(1, str[i]);
        break;
    }
  }
  return out;
}

template <typename Alloc>
void SplitStringHelper(
    const std::string& str, const std::string& delimiters,
    std::vector<std::string, Alloc>* strings) {
  size_t end_pos = 0;
  while (true) {
    // Skip delimiters at the current position.
    size_t start_pos = str.find_first_not_of(delimiters, end_pos);
    if (start_pos == std::string::npos)
      break;

    // Find the end of the string of non-delimiter characters.
    end_pos = str.find_first_of(delimiters, start_pos);

    strings->push_back(str.substr(start_pos, end_pos - start_pos));
  }
}

std::vector<std::string> ION_API SplitString(
    const std::string& str, const std::string& delimiters) {
  std::vector<std::string> strings;
  SplitStringHelper(str, delimiters, &strings);
  return strings;
}

AllocVector<std::string> ION_API SplitString(
    const std::string& str, const std::string& delimiters,
    const AllocatorPtr& alloc) {
  AllocVector<std::string> strings(alloc);
  SplitStringHelper(str, delimiters, &strings);
  return strings;
}

std::vector<std::string> ION_API SplitStringWithoutSkipping(
    const std::string& str, const std::string& delimiters) {
  std::vector<std::string> strings;

  size_t end_pos = 0;
  const size_t length = str.length();
  while (end_pos != std::string::npos && end_pos < length) {
    const size_t start_pos = end_pos;
    // Find the end of the string of non-delimiter characters.
    end_pos = str.find_first_of(delimiters, start_pos);

    strings.push_back(str.substr(start_pos, end_pos - start_pos));
    // Move to the next character if we are not already at the end of the
    // string.
    if (end_pos != std::string::npos)
      end_pos++;
  }
  return strings;
}

ION_API std::string UrlDecodeString(const std::string& str) {
  std::string decoded;
  const size_t count = str.size();
  for (size_t i = 0; i < count; ++i) {
    if (str[i] == '+') {
      decoded.push_back(' ');
    } else if (i + 2U < count) {
      const int c1 = static_cast<int>(str[i + 1]);
      const int c2 = static_cast<int>(str[i + 2]);
      if (str[i] == '%' && isxdigit(c1) && isxdigit(c2)) {
        const char c = static_cast<char>((HexToChar(c1) << 4) | HexToChar(c2));
        decoded.push_back(c);
        i += 2U;
      } else {
        decoded.push_back(str[i]);
      }
    } else {
      decoded.push_back(str[i]);
    }
  }

  return decoded;
}

ION_API std::string UrlEncodeString(const std::string& str) {
  // Non-alphanumeric characters that should not be escaped.
  static const char* kUnescaped = "._-$,;~()";
  // Hexadecimal digits.
  static const char* kHexDigits = "0123456789abcdef";

  std::string encoded;
  encoded.reserve(str.length() * 3);
  const size_t length = str.length();
  for (size_t i = 0; i < length; ++i) {
    if (isalnum(str[i]) || strchr(kUnescaped, str[i]) != nullptr) {
      encoded.push_back(str[i]);
    } else {
      encoded.push_back('%');
      encoded.push_back(kHexDigits[static_cast<uint8>(str[i]) >> 4]);
      encoded.push_back(kHexDigits[static_cast<uint8>(str[i]) & 0xf]);
    }
  }
  return encoded;
}

bool ION_API AreMultiLineStringsEqual(
    const std::string& s0, const std::string& s1,
    size_t* first_different_index, std::string* line0, std::string* line1,
    std::string* context0, std::string* context1) {
  if (s0 == s1) {
    return true;
  } else {
    const std::vector<std::string> v0 = SplitString(s0, "\n");
    const std::vector<std::string> v1 = SplitString(s1, "\n");
    const size_t num_lines = std::min(v0.size(), v1.size());
    size_t bad_index = num_lines;
    for (size_t i = 0; i < num_lines; ++i) {
      if (v0[i] != v1[i]) {
        bad_index = i;
        break;
      }
    }
    // If no difference was found in the loop, one of the vectors may be longer
    // than the other, and bad_index will be correctly set to num_lines.
    // However, if the sizes are the same, this means that the strings differ
    // only because of blank lines, and therefore they should be considered
    // equal.
    if (bad_index >= num_lines && v0.size() == v1.size())
      return true;
    if (first_different_index)
      *first_different_index = bad_index;
    if (line0)
      *line0 = bad_index < v0.size() ? v0[bad_index] : std::string("<missing>");
    if (line1)
      *line1 = bad_index < v1.size() ? v1[bad_index] : std::string("<missing>");
    // If requested, set some lines for context so that a caller knows where in
    // a large string the difference occurred.
    if (context0 || context1) {
      static const size_t kContextLines = 5;
      const size_t context_start =
          bad_index - std::min(kContextLines, bad_index);
      if (context0) {
        std::ostringstream str;
        const size_t context_end =
            std::min(v0.size(), bad_index + kContextLines + 1U);
        for (size_t i = context_start; i < context_end; ++i) {
          str << std::setfill(' ') << std::setw(5) << i << ": ";
          str << v0[i] << "\n";
        }
        *context0 = str.str();
      }
      if (context1) {
        std::ostringstream str;
        const size_t context_end =
            std::min(v1.size(), bad_index + kContextLines + 1U);
        for (size_t i = context_start; i < context_end; ++i) {
          str << std::setfill(' ') << std::setw(5) << i << ": ";
          str << v1[i] << "\n";
        }
        *context1 = str.str();
      }
    }
    return false;
  }
}

int32 ION_API StringToInt32(const std::string& str) {
  int32 value = 0;
  std::istringstream stream(str);
  stream >> value;
  return value;
}

ION_API int CompareCaseInsensitive(
    const std::string& str1, const std::string& str2) {
  if (std::lexicographical_compare(str1.begin(), str1.end(),
                                   str2.begin(), str2.end(), LessI))
    return -1;
  else if (str1.size() == str2.size() &&
           std::equal(str1.begin(), str1.end(), str2.begin(), EqualI))
    return 0;
  else
    return 1;
}

ION_API bool StartsWithCaseInsensitive(const std::string& target,
                               const std::string& start) {
  return !start.empty() && start.length() <= target.length() &&
      std::equal(start.begin(), start.end(), target.begin(), EqualI);
}

ION_API bool EndsWithCaseInsensitive(const std::string& target,
                             const std::string& end) {
  return !end.empty() && end.length() <= target.length() &&
      std::equal(end.rbegin(), end.rend(), target.rbegin(), EqualI);
}

ION_API int FindCaseInsensitive(
    const std::string& target, const std::string& substr) {
  if (substr.empty()) {
    return -1;
  }
  std::string target_upper(target.length(), 0);
  std::string substr_upper(substr.length(), 0);
  std::transform(target.begin(), target.end(), target_upper.begin(), ToUpper);
  std::transform(substr.begin(), substr.end(), substr_upper.begin(), ToUpper);
  size_t result = target_upper.find(substr_upper);
  if (result == std::string::npos) {
    return -1;
  }
  return static_cast<int>(result);
}

ION_API std::string WebSafeBase64Decode(const std::string& str) {
  // Perform some cleanup on input |str| to make it base64 with URL
  // and filename safe alphabet. Specifically:
  //   * Strip '=' from the end;
  //   * Convert '+' to '-' (62 in alphabet).
  //   * Convert '/' to '_' (63 in alphabet).
  std::string encoded = str;
  size_t length = encoded.length();
  size_t eq = length;
  while (eq != 0 && encoded[eq - 1] == '=') {
    encoded[eq - 1] = 0;
    --eq;
  }
  for (size_t i = 0; i < eq; ++i) {
    if (encoded[i] == '+')
      encoded[i] = '-';
    else if (encoded[i] == '/')
      encoded[i] = '_';
  }

  // Very conservative size estimate - decoded buffer is never larger
  // than input string.
  const size_t encoded_length = encoded.length();

  std::string decoded;
  // Add one to reserve space for the null terminator added by encoded.c_str().
  decoded.resize(encoded_length + 1);

  // std::string is guaranteed to hold contiguous characters in the C++11
  // standard, and likely all implementations prior were contiguous as well.
  // See http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html#530
  int decoded_length = B64_decode(encoded.c_str(),
                                  reinterpret_cast<uint8*>(&(decoded[0])),
                                  static_cast<int>(encoded_length));
  if (decoded_length == -1) {
    // Either an char is out of range or exceeded output max length.
    return "";
  }
  decoded.resize(decoded_length);
  return decoded;
}

ION_API std::string WebSafeBase64Encode(const std::string& input) {
  const size_t length = input.size();
  // Allocate at least enough memory; there are 4 output bytes for every 3 in
  // the input, plus the zero termination.
  const size_t max_encoded_length = ((length + 2U) / 3U) * 4U + 1;

  std::string encoded;
  encoded.resize(max_encoded_length);

  // std::string is guaranteed to hold contiguous characters in the C++11
  // standard, and likely all implementations prior were contiguous as well.
  // See http://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html#530
  int encoded_length = B64_encode(reinterpret_cast<const uint8*>(input.data()),
                                  static_cast<int>(input.size()),
                                  reinterpret_cast<char*>(&(encoded[0])),
                                  static_cast<int>(max_encoded_length));
  encoded.resize(encoded_length);
  return encoded;
}

}  // namespace base
}  // namespace ion
