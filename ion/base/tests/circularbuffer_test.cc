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

#include "ion/base/circularbuffer.h"

#include "ion/base/allocator.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

TEST(CircularBuffer, NotFilled) {
  CircularBuffer<int> buffer(5, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  EXPECT_EQ(5U, buffer.GetCapacity());
  EXPECT_EQ(3U, buffer.GetSize());
  EXPECT_EQ(1, buffer.GetItem(0));
  EXPECT_EQ(2, buffer.GetItem(1));
  EXPECT_EQ(3, buffer.GetItem(2));
}

TEST(CircularBuffer, NotFilled2) {
  CircularBuffer<int> buffer(5, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);
  buffer.AddItem(4);

  EXPECT_EQ(5U, buffer.GetCapacity());
  EXPECT_EQ(4U, buffer.GetSize());
  EXPECT_EQ(1, buffer.GetItem(0));
  EXPECT_EQ(2, buffer.GetItem(1));
  EXPECT_EQ(3, buffer.GetItem(2));
  EXPECT_EQ(4, buffer.GetItem(3));
}

TEST(CircularBuffer, Filled) {
  CircularBuffer<int> buffer(5, ion::base::AllocatorPtr(), true);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);
  buffer.AddItem(4);
  buffer.AddItem(5);
  buffer.AddItem(6);
  buffer.AddItem(7);

  EXPECT_EQ(5U, buffer.GetCapacity());
  EXPECT_EQ(5U, buffer.GetSize());
  EXPECT_EQ(3, buffer.GetItem(0));
  EXPECT_EQ(4, buffer.GetItem(1));
  EXPECT_EQ(5, buffer.GetItem(2));
  EXPECT_EQ(6, buffer.GetItem(3));
  EXPECT_EQ(7, buffer.GetItem(4));
}

TEST(CircularBuffer, Filled2) {
  CircularBuffer<int> buffer(5, ion::base::AllocatorPtr(), true);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);
  buffer.AddItem(4);
  buffer.AddItem(5);
  buffer.AddItem(6);

  EXPECT_EQ(5U, buffer.GetCapacity());
  EXPECT_EQ(5U, buffer.GetSize());
  EXPECT_EQ(2, buffer.GetItem(0));
  EXPECT_EQ(3, buffer.GetItem(1));
  EXPECT_EQ(4, buffer.GetItem(2));
  EXPECT_EQ(5, buffer.GetItem(3));
  EXPECT_EQ(6, buffer.GetItem(4));
}

TEST(CircularBuffer, Clear) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), true);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  EXPECT_EQ(3U, buffer.GetCapacity());
  EXPECT_EQ(3U, buffer.GetSize());
  EXPECT_EQ(1, buffer.GetItem(0));
  EXPECT_EQ(2, buffer.GetItem(1));
  EXPECT_EQ(3, buffer.GetItem(2));

  buffer.Clear();

  EXPECT_EQ(3U, buffer.GetCapacity());
  EXPECT_EQ(0U, buffer.GetSize());

  buffer.AddItem(4);
  buffer.AddItem(5);

  EXPECT_EQ(3U, buffer.GetCapacity());
  EXPECT_EQ(2U, buffer.GetSize());
  EXPECT_EQ(4, buffer.GetItem(0));
  EXPECT_EQ(5, buffer.GetItem(1));
}

}  // namespace base
}  // namespace ion
