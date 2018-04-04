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

#include "ion/port/align.h"

#include "base/integral_types.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace {

// This struct should have aligned member variables.
struct Aligned {
  bool ION_ALIGN(16) b;
  int16 ION_ALIGN(16) i;
  char ION_ALIGN(16) c;
};

static bool IsSupposedToBe16ByteAligned(const void* p) {
#if ION_ALIGNMENT_ENABLED
  return !(reinterpret_cast<size_t>(p) % 16);
#else
  return false;
#endif
}

#if ION_ALIGNMENT_ENABLED
const bool kAligned = true;
#else
const bool kAligned = false;
#endif

}  // anonymous namespace

// There is no good way to test that items without the ION_ALIGN macro are not
// aligned properly, because the compiler is free to align anything it wants.
// So just make sure that things we expect to be aligned are aligned properly.

TEST(Align, Variables) {
  bool b(true);
  int16 ION_ALIGN(16) i;
  char ION_ALIGN(16) c = 'Q';

  EXPECT_TRUE(b);  // Otherwise, b is unused.

  EXPECT_EQ(kAligned, IsSupposedToBe16ByteAligned(&i));
  EXPECT_EQ(kAligned, IsSupposedToBe16ByteAligned(&c));
  EXPECT_EQ('Q', c);
}

TEST(Align, Members) {
  Aligned a;
  EXPECT_EQ(kAligned, IsSupposedToBe16ByteAligned(&a.b));
  EXPECT_EQ(kAligned, IsSupposedToBe16ByteAligned(&a.i));
  EXPECT_EQ(kAligned, IsSupposedToBe16ByteAligned(&a.c));
  // The struct must be at least 33 bytes if the members are aligned.
  EXPECT_LE(kAligned ? 33U : 4U, sizeof(a));
}

TEST(Align, AlignOf) {
  EXPECT_EQ(1U, ION_ALIGNOF(uint8));
  EXPECT_EQ(2U, ION_ALIGNOF(uint16));
  EXPECT_EQ(4U, ION_ALIGNOF(int));
  EXPECT_EQ(8U, ION_ALIGNOF(double));
  EXPECT_EQ(kAligned, ION_ALIGNOF(Aligned) == 16U);
}
