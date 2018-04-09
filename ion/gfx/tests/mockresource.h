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

#ifndef ION_GFX_TESTS_MOCKRESOURCE_H_
#define ION_GFX_TESTS_MOCKRESOURCE_H_

#include <bitset>

#include "ion/base/logging.h"
#include "ion/gfx/resourcebase.h"

namespace ion {
namespace gfx {
namespace testing {

// MockResource implements ResourceBase to allow testing of change bits. Test
// programs can instantiate it with the right number of bits.
template<int NumModifiedBits>
class MockResource : public ResourceBase {
 public:
  explicit MockResource(ResourceKey key = 0) : ResourceBase(nullptr, key) {}
  ~MockResource() override {}

  void OnDestroyed() override {}

  size_t GetGpuMemoryUsed() const override { return 0U; }

  // Returns true of any bits are set.
  bool AnyModifiedBitsSet() const {
    return modified_bits_.any();
  }

  // Resets the value of a particular bit.
  void ResetModifiedBit(const int bit) {
    modified_bits_.reset(bit);
  }

  // Resets the values of all bits.
  void ResetModifiedBits() {
    modified_bits_.reset();
  }

  // Sets all bits to 1.
  void SetModifiedBits() {
    modified_bits_.set();
  }

  // Returns true if a particular bit is set.
  bool TestModifiedBit(const int bit) const {
    return modified_bits_.test(bit);
  }

  // Returns whether any bits in the range [low_bit, high_bit] are set.
  bool TestModifiedBitRange(int low_bit, int high_bit) const {
    std::bitset<NumModifiedBits> mask;
    // Set mask to all 1s.
    mask.set();
    // The right end of the bitmask now has 0s for the number of bits we want to
    // test.
    mask <<= high_bit + 1 - low_bit;
    // Flip the bits so that the rightmost number of bits we want to test are
    // all 1s.
    mask.flip();
    // Shift the bits to the desired range.
    mask <<= low_bit;
    // Return whether modified_bits_ has any of the same bits set.
    return (mask & modified_bits_).any();
  }

  // Returns true if a particular bit is set and it is the only bit set.
  bool TestOnlyModifiedBit(const int bit) const {
    return modified_bits_.test(bit) && modified_bits_.count() == 1U;
  }

  // Returns the number of bits that are set.
  size_t GetModifiedBitCount() const {
    return modified_bits_.count();
  }

 private:
  void OnChanged(const int bit) override {
    modified_bits_.set(bit);
  }

  std::bitset<NumModifiedBits> modified_bits_;
};

}  // namespace testing
}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TESTS_MOCKRESOURCE_H_
