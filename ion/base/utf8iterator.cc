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

namespace ion {
namespace base {

namespace {

//-----------------------------------------------------------------------------
//
// UTF-8 helper functions.
//
//-----------------------------------------------------------------------------

static bool Is1ByteSequence(uint8 first_byte) {
  // A 1-byte sequence is a regular Ascii character. The high-order bit is 0.
  return (first_byte & 0x80) == 0;
}

static bool Is2ByteSequence(uint8 first_byte) {
  // The first byte in a 2-byte sequence has '110' in the high-order 3 bits.
  return (first_byte & 0xe0) == 0xc0;
}

static bool Is3ByteSequence(uint8 first_byte) {
  // The first byte in a 3-byte sequence has '1110' in the high-order 4 bits.
  return (first_byte & 0xf0) == 0xe0;
}

static bool Is4ByteSequence(uint8 first_byte) {
  // The first byte in a 4-byte sequence has '11110' in the high-order 5 bits.
  return (first_byte & 0xf8) == 0xf0;
}

static bool IsContinuationByte(uint8 byte) {
  // A continuation byte in any multi-byte sequence has '10' in the high-order
  // 2 bits.
  return (byte & 0xc0) == 0x80;
}

static uint32 Compute2ByteUnicode(uint8 byte1, uint8 byte2) {
  const uint32 codepoint = (static_cast<uint32>(byte1 & 0x1f) << 6) |
      static_cast<uint32>(byte2 & 0x3f);
  return codepoint <= 0x7f ? Utf8Iterator::kInvalidCharIndex : codepoint;
}

static uint32 Compute3ByteUnicode(uint8 byte1, uint8 byte2, uint8 byte3) {
  const uint32 codepoint = (static_cast<uint32>(byte1 & 0x0f) << 12) |
      (static_cast<uint32>(byte2 & 0x3f) << 6) |
      static_cast<uint32>(byte3 & 0x3f);
  return codepoint <= 0x7ff ? Utf8Iterator::kInvalidCharIndex : codepoint;
}

static uint32 Compute4ByteUnicode(uint8 byte1, uint8 byte2,
                                  uint8 byte3, uint8 byte4) {
  const uint32 codepoint = (static_cast<uint32>(byte1 & 0x07) << 18) |
      (static_cast<uint32>(byte2 & 0x3f) << 12) |
      (static_cast<uint32>(byte3 & 0x3f) << 6) |
      static_cast<uint32>(byte4 & 0x3f);
  return codepoint <= 0xffff ? Utf8Iterator::kInvalidCharIndex : codepoint;
}

}  // anonymous namespace

const uint32 Utf8Iterator::kInvalidCharIndex = 0x110000;

//-----------------------------------------------------------------------------
//
// Utf8Iterator functions.
//
//-----------------------------------------------------------------------------

Utf8Iterator::Utf8Iterator(const std::string& utf8_string)
    : string_(utf8_string),
      byte_count_(string_.size()),
      cur_index_(0),
      state_(byte_count_ ? kInString : kEndOfString) {}

uint32 Utf8Iterator::Next() {
  uint32 unicode_index = kInvalidCharIndex;
  if (state_ == kInString) {
    // Get the first byte, which indicates the size of the UTF-8 character
    // sequence. If there are no bytes left, this will set the state to
    // kInvalid.
    const uint8 byte1 = GetNextByte();
    if (Is1ByteSequence(byte1)) {
      unicode_index = byte1;
    } else if (Is2ByteSequence(byte1)) {
      const uint8 byte2 = GetNextByte();
      if (IsContinuationByte(byte2))
        unicode_index = Compute2ByteUnicode(byte1, byte2);
    } else if (Is3ByteSequence(byte1)) {
      const uint8 byte2 = GetNextByte();
      const uint8 byte3 = GetNextByte();
      if (IsContinuationByte(byte2) && IsContinuationByte(byte3))
        unicode_index = Compute3ByteUnicode(byte1, byte2, byte3);
    } else if (Is4ByteSequence(byte1)) {
      const uint8 byte2 = GetNextByte();
      const uint8 byte3 = GetNextByte();
      const uint8 byte4 = GetNextByte();
      if (IsContinuationByte(byte2) && IsContinuationByte(byte3) &&
          IsContinuationByte(byte4))
        unicode_index = Compute4ByteUnicode(byte1, byte2, byte3, byte4);
      // Verify that the index does not exceed the maximum.
      static const uint32 kMaxValidIndex = 0x10ffff;
      if (unicode_index > kMaxValidIndex)
        unicode_index = kInvalidCharIndex;
    }
    // Set the error state if not at the end of the string and no valid
    // character was found.
    if (unicode_index == kInvalidCharIndex && state_ == kInString)
      state_ = kInvalid;
  }
  return unicode_index;
}

size_t Utf8Iterator::ComputeCharCount() const {
  size_t count = 0;
  Utf8Iterator it(string_);
  while (it.Next() != kInvalidCharIndex)
    ++count;
  // Return 0 on error.
  return it.GetState() == kEndOfString ? count : 0;
}

size_t Utf8Iterator::GetCurrentByteIndex() const { return cur_index_; }

uint8 Utf8Iterator::GetNextByte() {
  if (state_ == kInString) {
    const uint8 next_byte = string_[cur_index_];
    if (++cur_index_ == byte_count_)
      state_ = kEndOfString;
    return next_byte;
  }
  state_ = kInvalid;
  return 0;
}

}  // namespace base
}  // namespace ion
