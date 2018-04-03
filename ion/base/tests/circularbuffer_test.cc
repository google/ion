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
#include "ion/base/logchecker.h"
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

TEST(CircularBuffer, DropOldestItem) {
  CircularBuffer<int> buffer(5, ion::base::AllocatorPtr(), true);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.DropOldestItem();

  EXPECT_EQ(5U, buffer.GetCapacity());
  EXPECT_EQ(1U, buffer.GetSize());
  EXPECT_EQ(2, buffer.GetItem(0));

  buffer.DropOldestItem();

  EXPECT_EQ(5U, buffer.GetCapacity());
  EXPECT_EQ(0U, buffer.GetSize());

  buffer.AddItem(1);
  EXPECT_EQ(1U, buffer.GetSize());
  EXPECT_EQ(1, buffer.GetItem(0));

  buffer.AddItem(2);
  EXPECT_EQ(2U, buffer.GetSize());
  EXPECT_EQ(1, buffer.GetItem(0));
  EXPECT_EQ(2, buffer.GetItem(1));

  buffer.AddItem(3);
  EXPECT_EQ(3U, buffer.GetSize());
  EXPECT_EQ(1, buffer.GetItem(0));
  EXPECT_EQ(2, buffer.GetItem(1));
  EXPECT_EQ(3, buffer.GetItem(2));

  buffer.AddItem(4);
  EXPECT_EQ(4U, buffer.GetSize());
  EXPECT_EQ(1, buffer.GetItem(0));
  EXPECT_EQ(2, buffer.GetItem(1));
  EXPECT_EQ(3, buffer.GetItem(2));
  EXPECT_EQ(4, buffer.GetItem(3));

  buffer.AddItem(5);
  EXPECT_EQ(5U, buffer.GetCapacity());
  EXPECT_EQ(5U, buffer.GetSize());
  EXPECT_EQ(1, buffer.GetItem(0));
  EXPECT_EQ(2, buffer.GetItem(1));
  EXPECT_EQ(3, buffer.GetItem(2));
  EXPECT_EQ(4, buffer.GetItem(3));
  EXPECT_EQ(5, buffer.GetItem(4));

  buffer.DropOldestItem();
  EXPECT_EQ(4U, buffer.GetSize());
  EXPECT_EQ(2, buffer.GetItem(0));
  EXPECT_EQ(3, buffer.GetItem(1));
  EXPECT_EQ(4, buffer.GetItem(2));
  EXPECT_EQ(5, buffer.GetItem(3));

  buffer.DropOldestItem();
  EXPECT_EQ(3U, buffer.GetSize());
  EXPECT_EQ(3, buffer.GetItem(0));
  EXPECT_EQ(4, buffer.GetItem(1));
  EXPECT_EQ(5, buffer.GetItem(2));

  buffer.DropOldestItem();
  EXPECT_EQ(2U, buffer.GetSize());
  EXPECT_EQ(4, buffer.GetItem(0));
  EXPECT_EQ(5, buffer.GetItem(1));

  buffer.DropOldestItem();
  EXPECT_EQ(1U, buffer.GetSize());
  EXPECT_EQ(5, buffer.GetItem(0));

  buffer.DropOldestItem();
  EXPECT_EQ(0U, buffer.GetSize());
}

TEST(CircularBuffer, DropOldestItems) {
  CircularBuffer<int> buffer(6, ion::base::AllocatorPtr(), true);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.DropOldestItems(2);

  EXPECT_EQ(6U, buffer.GetCapacity());
  EXPECT_EQ(0U, buffer.GetSize());

  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);
  buffer.AddItem(4);
  buffer.AddItem(5);

  buffer.DropOldestItems(3);
  EXPECT_EQ(6U, buffer.GetCapacity());
  EXPECT_EQ(2U, buffer.GetSize());

  buffer.AddItem(6);
  buffer.AddItem(7);
  buffer.AddItem(8);
  buffer.AddItem(9);
  buffer.AddItem(10);

  EXPECT_EQ(6U, buffer.GetCapacity());
  EXPECT_EQ(6U, buffer.GetSize());

  buffer.DropOldestItems(3);

  EXPECT_EQ(6U, buffer.GetCapacity());
  EXPECT_EQ(3U, buffer.GetSize());
  EXPECT_EQ(8, buffer.GetItem(0));
  EXPECT_EQ(9, buffer.GetItem(1));
  EXPECT_EQ(10, buffer.GetItem(2));

  buffer.DropOldestItems(3);
  EXPECT_EQ(6U, buffer.GetCapacity());
  EXPECT_EQ(0U, buffer.GetSize());
}

TEST(CircularBuffer, GetOldestAndGetNewest) {
  CircularBuffer<int> buffer(10, ion::base::AllocatorPtr(), true);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  EXPECT_EQ(1, buffer.GetOldest());
  EXPECT_EQ(3, buffer.GetNewest());

  buffer.DropOldestItem();

  EXPECT_EQ(2, buffer.GetOldest());
  EXPECT_EQ(3, buffer.GetNewest());

  buffer.AddItem(4);
  buffer.AddItem(5);
  buffer.AddItem(6);
  buffer.AddItem(7);

  EXPECT_EQ(2, buffer.GetOldest());
  EXPECT_EQ(7, buffer.GetNewest());

  buffer.DropOldestItem();
  buffer.DropOldestItem();
  buffer.DropOldestItem();
  buffer.DropOldestItem();
  buffer.DropOldestItem();

  EXPECT_EQ(7, buffer.GetOldest());
  EXPECT_EQ(7, buffer.GetNewest());
}

TEST(CircularBuffer, IsEmptyIsFull) {
  CircularBuffer<int> buffer(4, ion::base::AllocatorPtr(), true);
  EXPECT_TRUE(buffer.IsEmpty());
  EXPECT_FALSE(buffer.IsFull());

  buffer.AddItem(1);

  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_FALSE(buffer.IsFull());

  buffer.AddItem(2);

  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_FALSE(buffer.IsFull());

  buffer.AddItem(3);
  buffer.AddItem(4);

  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_TRUE(buffer.IsFull());

  buffer.AddItem(5);
  buffer.AddItem(6);
  buffer.AddItem(7);
  buffer.AddItem(8);

  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_TRUE(buffer.IsFull());

  buffer.DropOldestItem();

  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_FALSE(buffer.IsFull());

  buffer.AddItem(9);

  EXPECT_FALSE(buffer.IsEmpty());
  EXPECT_TRUE(buffer.IsFull());

  buffer.Clear();

  EXPECT_TRUE(buffer.IsEmpty());
  EXPECT_FALSE(buffer.IsFull());
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

TEST(CircularBuffer, BoolBuffer) {
  CircularBuffer<bool> buffer(2, ion::base::AllocatorPtr(), false);
  buffer.AddItem(false);
  buffer.AddItem(false);
  EXPECT_EQ(2U, buffer.GetSize());
  EXPECT_FALSE(buffer.GetItem(0));
  EXPECT_FALSE(buffer.GetItem(1));

  buffer.AddItem(true);
  buffer.AddItem(true);
  EXPECT_EQ(2U, buffer.GetSize());
  EXPECT_TRUE(buffer.GetItem(0));
  EXPECT_TRUE(buffer.GetItem(1));
}

TEST(CircularBuffer, IteratorPostIncrement) {
  CircularBuffer<int> buffer(5, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  auto begin = buffer.cbegin();
  auto iter = begin;
  EXPECT_EQ(begin, buffer.cbegin());
  EXPECT_EQ(begin, iter);

  EXPECT_EQ(*iter, 1);
  auto iter1 = iter++;
  EXPECT_EQ(begin, iter1);
  EXPECT_EQ(*iter, 2);
  iter++;
  EXPECT_EQ(*iter, 3);
  iter++;

  auto end = buffer.cend();
  EXPECT_EQ(iter, end);
  EXPECT_EQ(buffer.cend(), end);
}

TEST(CircularBuffer, IteratorPreIncrement) {
  CircularBuffer<int> buffer(5, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  auto begin = buffer.cbegin();
  auto iter = begin;
  EXPECT_EQ(begin, buffer.cbegin());
  EXPECT_EQ(begin, iter);

  EXPECT_EQ(*iter, 1);
  EXPECT_NE(++iter, begin);
  EXPECT_EQ(*iter, 2);
  ++iter;
  EXPECT_EQ(*iter, 3);
  ++iter;

  auto end = buffer.cend();
  EXPECT_EQ(iter, end);
  EXPECT_EQ(buffer.cend(), end);
}

TEST(CircularBuffer, IteratorPostDecrement) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  auto end = buffer.cend();
  auto iter = end;
  EXPECT_EQ(iter, end);
  EXPECT_EQ(iter--, end);
  EXPECT_EQ(*(iter--), 3);
  EXPECT_EQ(*(iter--), 2);
  EXPECT_EQ(*iter, 1);
  auto begin = buffer.cbegin();
  EXPECT_EQ(iter, begin);
  EXPECT_EQ(begin, buffer.cbegin());
}

TEST(CircularBuffer, IteratorPreDecrement) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  auto end = buffer.cend();
  auto iter = end;
  EXPECT_EQ(iter, end);
  EXPECT_NE(--iter, end);
  EXPECT_EQ(*iter, 3);
  EXPECT_EQ(*(--iter), 2);
  EXPECT_EQ(*(--iter), 1);
  auto begin = buffer.cbegin();
  EXPECT_EQ(iter, begin);
  EXPECT_EQ(begin, buffer.cbegin());
}

TEST(CircularBuffer, IteratorOffsets) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  auto begin = buffer.cbegin();
  auto iter = begin + 1;
  EXPECT_NE(iter, begin);
  EXPECT_EQ(*iter, 2);
  iter = iter + 2;
  EXPECT_EQ(iter, buffer.cend());
  iter = iter - 3;
  EXPECT_EQ(*iter, 1);

  iter += 1;
  EXPECT_EQ(*iter, 2);
  iter -= 1;
  EXPECT_EQ(*iter, 1);

  iter = 1 + iter;
  EXPECT_EQ(*iter, 2);
  iter = -1 + iter;
  EXPECT_EQ(*iter, 1);

  auto next_iter = std::next(iter);
  EXPECT_NE(iter, next_iter);
  EXPECT_EQ(next_iter - iter, 1);
}

TEST(CircularBuffer, IteratorDereferencing) {
  struct Foo {
    int value;

    bool operator==(const Foo& rhs) const { return value == rhs.value; }
  };

  CircularBuffer<Foo> buffer(3, ion::base::AllocatorPtr(), false);
  buffer.AddItem(Foo{1});
  buffer.AddItem(Foo{2});
  buffer.AddItem(Foo{3});

  auto begin = buffer.cbegin();
  auto iter = begin;
  EXPECT_EQ(begin, iter);
  EXPECT_EQ(iter->value, 1);
  EXPECT_EQ((*iter).value, 1);
  EXPECT_EQ(begin, iter++);
  EXPECT_EQ(iter->value, 2);
  EXPECT_EQ((*iter).value, 2);
  EXPECT_EQ(iter[0], Foo{2});
  EXPECT_EQ(iter[1], Foo{3});
}

TEST(CircularBuffer, IteratorWrapAround) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);
  buffer.AddItem(4);

  auto begin = buffer.cbegin();
  EXPECT_EQ(*begin, 2);
  auto iter = begin;
  EXPECT_EQ(begin, iter);
  ++iter;
  EXPECT_NE(iter, begin);
  EXPECT_EQ(*iter, 3);
  ++iter;
  EXPECT_EQ(*iter, 4);
  EXPECT_GT(&*begin, &*iter);
  ++iter;
  EXPECT_EQ(iter, buffer.cend());
}

TEST(CircularBuffer, BeginEnd) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  auto begin = buffer.begin();
  auto end = buffer.end();

  EXPECT_NE(begin, end);
  ++begin;
  --end;
  EXPECT_NE(begin, end);
  --end;
  EXPECT_EQ(begin, end);
}

TEST(CircularBuffer, ReverseBeginEnd) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  auto begin = buffer.rbegin();
  auto end = buffer.rend();

  EXPECT_EQ(begin[0], end[-3]);
  EXPECT_EQ(begin[1], end[-2]);
  EXPECT_EQ(begin[2], end[-1]);

  EXPECT_NE(begin, end);
  EXPECT_EQ(*begin, 3);
  EXPECT_EQ(begin[0], 3);
  ++begin;  // Begin references '1'.
  --end;  // End references '3'.
  EXPECT_NE(begin, end);
  EXPECT_EQ(begin[-1], 3);
  EXPECT_EQ(begin[0], 2);
  EXPECT_EQ(begin[1], 1);

  EXPECT_EQ(*end, 1);
  EXPECT_EQ(end[0], 1);
  EXPECT_EQ(end[-1], 2);
  EXPECT_EQ(end[-2], 3);
  --end;
  EXPECT_EQ(end[0], 2);
  EXPECT_EQ(end[-1], 3);
  EXPECT_EQ(begin, end);
}

TEST(CircularBuffer, ForRangeBasedLoop) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), false);
  buffer.AddItem(1);
  buffer.AddItem(2);
  buffer.AddItem(3);

  int expected = 1;
  for (const auto val : buffer) {
    EXPECT_EQ(expected, val);
    expected++;
  }
}

// Tests a copy constructor where the capacity of the new buffer is equal.
TEST(CircularBuffer, CopyConstructorTest) {
  CircularBuffer<int> buffer(5, ion::base::AllocatorPtr(), false);
  std::vector<int> test_values = {1, 2, 3, 4, 5};
  for (int test_val : test_values) {
    buffer.AddItem(test_val);
  }

  CircularBuffer<int> copy_constructed_buffer(
      /*source_buffer=*/buffer,
      /*alloc=*/ion::base::AllocatorPtr());
  EXPECT_EQ(buffer.GetCapacity(), copy_constructed_buffer.GetCapacity());
  for (size_t i = 0; i < copy_constructed_buffer.GetCapacity(); i++) {
    EXPECT_EQ(buffer.GetItem(i), copy_constructed_buffer.GetItem(i));
  }
}

// Tests a copy constructor where the capacity of the new buffer is larger.
TEST(CircularBuffer, CopyConstructorTestWithNewCapacity) {
  CircularBuffer<int> buffer(3, ion::base::AllocatorPtr(), false);
  std::vector<int> test_values = {1, 2, 3};
  for (int test_val : test_values) {
    buffer.AddItem(test_val);
  }

  constexpr size_t new_capacity = 10;
  CircularBuffer<int> copy_constructed_buffer(
      /*source_buffer=*/buffer,
      /*alloc=*/ion::base::AllocatorPtr(),
      /*capacity=*/new_capacity);
  EXPECT_EQ(new_capacity, copy_constructed_buffer.GetCapacity());
  for (size_t i = 0; i < buffer.GetCapacity(); i++) {
    EXPECT_EQ(buffer.GetItem(i), copy_constructed_buffer.GetItem(i));
  }
}

#if !ION_PRODUCTION
TEST(CircularBuffer, CopyConstructorWithInvalidNewCapacity) {
  constexpr size_t source_buffer_size = 7;
  CircularBuffer<int> buffer(source_buffer_size, ion::base::AllocatorPtr(),
                             false);

  // Iterate through a series of invalid sizes and make sure they all fail to
  // copy construct.  Note size of 0 is a valid size.
  for (size_t i = 1; i < source_buffer_size; i++) {
    EXPECT_DEATH_IF_SUPPORTED(
        CircularBuffer<int>(
            /*source_buffer=*/buffer,
            /*alloc=*/ion::base::AllocatorPtr(),
            /*capacity=*/i),
        "CircularBuffer copy constructor invoked with invalid capacity.");
  }
}
#endif

}  // namespace base
}  // namespace ion
