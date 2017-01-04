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

#include "ion/base/array2.h"

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

struct Data {
  Data() : id(-1), value(0.f) {}
  Data(int id_in, float value_in) : id(id_in), value(value_in) {}
  int id;
  float value;
  bool operator==(const Data& other) const {
    return id == other.id && value == other.value;
  }
};

}  // anonymous namespace

TEST(Array2Test, Construction) {
  {
    // Default constructor.
    Array2<int> a;
    EXPECT_EQ(0U, a.GetWidth());
    EXPECT_EQ(0U, a.GetHeight());
    EXPECT_EQ(0U, a.GetSize());
  }
  {
    // Create with undefined values.
    Array2<float> a(100U, 40U);
    EXPECT_EQ(100U, a.GetWidth());
    EXPECT_EQ(40U, a.GetHeight());
    EXPECT_EQ(4000U, a.GetSize());
  }
  {
    // Create with defined values.
    Array2<int> a(15U, 7U, 42);
    EXPECT_EQ(15U, a.GetWidth());
    EXPECT_EQ(7U, a.GetHeight());
    EXPECT_EQ(105U, a.GetSize());
    for (size_t y = 0; y < a.GetHeight(); ++y) {
      for (size_t x = 0; x < a.GetWidth(); ++x) {
        EXPECT_EQ(42, a.Get(x, y));
      }
    }
  }
}

TEST(Array2Test, SetAndGet) {
  base::LogChecker log_checker;
  Array2<int> a(16U, 13U);

  // Set a couple of elements.
  EXPECT_TRUE(a.Set(3U, 7U, 14));
  EXPECT_TRUE(a.Set(5U, 12U, -100));
  EXPECT_EQ(14, a.Get(3U, 7U));
  EXPECT_EQ(-100, a.Get(5U, 12U));

  // Set 'em all.
  for (size_t y = 0; y < a.GetHeight(); ++y) {
    for (size_t x = 0; x < a.GetWidth(); ++x) {
      EXPECT_TRUE(a.Set(x, y, static_cast<int>(y * 1000 + x)));
    }
  }
  for (size_t y = 0; y < a.GetHeight(); ++y) {
    for (size_t x = 0; x < a.GetWidth(); ++x) {
      EXPECT_EQ(static_cast<int>(y * 1000 + x), a.Get(x, y));
      EXPECT_EQ(static_cast<int>(y * 1000 + x), *a.GetMutable(x, y));
    }
  }

  // Invalid indices.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  {
    EXPECT_FALSE(a.Set(16U, 4U, 1234));
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));
    EXPECT_FALSE(a.Set(0U, 13U, 1234));
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));
    EXPECT_EQ(base::InvalidReference<int>(), a.Get(0U, 13U));
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));
    EXPECT_TRUE(a.GetMutable(0U, 13U) == nullptr);
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));
  }
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(Array2Test, StructData) {
  base::LogChecker log_checker;

  Data d1(0, 0.1f);
  Data d2(1, 1.2f);
  Data d3(2, 2.3f);
  Data d4(3, 3.4f);

  Data uninitialized;

  Array2<Data> a(4, 4, uninitialized);
  a.Set(0, 0, d1);
  a.Set(0, 2, d2);
  a.Set(1, 0, d3);
  a.Set(2, 3, d4);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  a.Set(0, 4, d4);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));
  a.Set(4, 0, d4);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));

  // Used indices.
  EXPECT_EQ(d1, a.Get(0, 0));
  EXPECT_EQ(d2, a.Get(0, 2));
  EXPECT_EQ(d3, a.Get(1, 0));
  EXPECT_EQ(d4, a.Get(2, 3));
  // Unused indices.
  EXPECT_EQ(uninitialized, a.Get(0, 1));
  EXPECT_EQ(uninitialized, a.Get(0, 3));
  EXPECT_EQ(uninitialized, a.Get(1, 1));
  EXPECT_EQ(uninitialized, a.Get(2, 0));
  EXPECT_EQ(uninitialized, a.Get(2, 1));
  EXPECT_EQ(uninitialized, a.Get(2, 2));
  EXPECT_EQ(uninitialized, a.Get(3, 1));
  EXPECT_EQ(uninitialized, a.Get(3, 2));
  EXPECT_EQ(uninitialized, a.Get(3, 3));
  // Check that invalid indices return an invalid reference.
  EXPECT_TRUE(base::IsInvalidReference(a.Get(4, 0)));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));
  EXPECT_TRUE(base::IsInvalidReference(a.Get(0, 4)));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));

  // Check that GetMutable() returns valid points for valid indices and NULL
  // for invalid ones.
  EXPECT_EQ(d1, *a.GetMutable(0, 0));
  EXPECT_EQ(d2, *a.GetMutable(0, 2));
  EXPECT_EQ(d3, *a.GetMutable(1, 0));
  EXPECT_EQ(d4, *a.GetMutable(2, 3));
  EXPECT_EQ(uninitialized, *a.GetMutable(0, 1));
  EXPECT_EQ(uninitialized, *a.GetMutable(0, 3));
  EXPECT_EQ(uninitialized, *a.GetMutable(1, 1));
  EXPECT_EQ(uninitialized, *a.GetMutable(2, 0));
  EXPECT_EQ(uninitialized, *a.GetMutable(2, 1));
  EXPECT_EQ(uninitialized, *a.GetMutable(2, 2));
  EXPECT_EQ(uninitialized, *a.GetMutable(3, 1));
  EXPECT_EQ(uninitialized, *a.GetMutable(3, 2));
  EXPECT_EQ(uninitialized, *a.GetMutable(3, 3));
  // Invalid indices.
  EXPECT_TRUE(a.GetMutable(4, 0) == nullptr);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));
  EXPECT_TRUE(a.GetMutable(0, 4) == nullptr);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Bad indices"));

  // Clear the array.
  a = Array2<Data>(0U, 1U, uninitialized);
  EXPECT_EQ(0U, a.GetSize());
  a = Array2<Data>(1U, 0U, uninitialized);
  EXPECT_EQ(0U, a.GetSize());

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

}  // namespace base
}  // namespace ion
