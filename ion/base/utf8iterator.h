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

#ifndef ION_BASE_UTF8ITERATOR_H_
#define ION_BASE_UTF8ITERATOR_H_

#include <string>

#include "base/integral_types.h"

namespace ion {
namespace base {

// The Utf8Iterator class iterates over characters in strings encoded with
// UTF-8, extracting the Unicode index for each character. It can also compute
// the total number of characters in the string.
//
// For easy reference, here are the ranges for UTF-8 characters.
//
//     Hex      Decimal                      What
//   -------   ---------    --------------------------------------------
//   00 - 7f     0 - 127    Only byte of a 1-byte character (Ascii)
//   80 - bf   128 - 191    Continuation byte of a multi-byte sequence
//   c0 - c1   192 - 193    <Invalid bytes - should never appear>
//   c2 - df   194 - 223    First byte of a 2-byte sequence
//   e0 - ef   224 - 239    First byte of a 3-byte sequence
//   f0 - f4   240 - 244    First byte of a 4-byte sequence
//   f5 - ff   245 - 255    <Invalid bytes - should never appear>
//
// In addition, no Unicode index should exceed the maximum of 0x10ffff (4-byte
// sequence f4/8f/bf/bf). Some sequences that start with byte f4 (244) may
// exceed this limit and be illegal.
//
class ION_API Utf8Iterator {
 public:
  // Iterator states.
  enum State {
    kInString,     // Still iterating over characters.
    kEndOfString,  // Hit the end of the string.
    kInvalid,      // Hit an invalid UTF-8 sequence.
  };

  // An invalid Unicode character index.
  static const uint32 kInvalidCharIndex;

  // The constructor is passed an std::string in UTF-8 format.
  explicit Utf8Iterator(const std::string& utf8_string);

  // Returns the Unicode index (up to 21 bits) for the next character in the
  // string, or kInvalidCharIndex if there are no characters remaining or an
  // error occurred.
  uint32 Next();

  // Returns the state of the iterator. This can be used once iteration
  // terminates to determine whether an error occurred or the end of string was
  // reached.
  State GetState() const { return state_; }

  // Convenience function that computes and returns the number of Unicode
  // characters in the string by iterating over it. This returns 0 if there are
  // any encoding errors in the string.
  size_t ComputeCharCount() const;

  // Returns the byte index of the character to be returned by the next call
  // to Next.
  size_t GetCurrentByteIndex() const;

 private:
  // Returns the next byte in the string, incrementing cur_index_ and setting
  // the state to kEndOfString if this is the last byte. Sets the state to
  // kInvalid and returns 0 if there is no next byte.
  uint8 GetNextByte();

  // String passed to the constructor. This is a copy to make the iterator
  // work, regardless of the lifetime of the original string.
  const std::string string_;
  // Number of bytes in the string, cached for performance reasons.
  const size_t byte_count_;
  // Current position in the string.
  size_t cur_index_;
  // Iterator state.
  State state_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_UTF8ITERATOR_H_
