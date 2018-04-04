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

#include "ion/base/indexmap.h"

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/port/nullptr.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

// The ordered index type.
enum OrderedIndex { kZero, kOne, kTwo, kThree };

// The unordered index type.
enum UnorderedIndex { kCat = 12, kDog = 9, kPig = 41, kWolf = 14 };

TEST(IndexMap, Basic) {
  static const UnorderedIndex u[] = { kCat, kDog, kPig, kWolf };
  ion::base::IndexMap<OrderedIndex, UnorderedIndex> m(u, 4);

  EXPECT_EQ(4U, m.GetCount());

  // Ordered to unordered.
  EXPECT_EQ(kCat, m.GetUnorderedIndex(kZero));
  EXPECT_EQ(kDog, m.GetUnorderedIndex(kOne));
  EXPECT_EQ(kPig, m.GetUnorderedIndex(kTwo));
  EXPECT_EQ(kWolf, m.GetUnorderedIndex(kThree));

  // Unordered to ordered.
  EXPECT_EQ(kZero, m.GetOrderedIndex(kCat));
  EXPECT_EQ(kOne, m.GetOrderedIndex(kDog));
  EXPECT_EQ(kTwo, m.GetOrderedIndex(kPig));
  EXPECT_EQ(kThree, m.GetOrderedIndex(kWolf));
}

TEST(IndexMap, Invalid) {
  static const UnorderedIndex u[] = { kCat, kDog, kPig, kWolf };
  ion::base::IndexMap<OrderedIndex, UnorderedIndex> m(u, 4);

#if !ION_PRODUCTION
  EXPECT_DEATH_IF_SUPPORTED(
      m.GetOrderedIndex(ion::base::InvalidEnumValue<UnorderedIndex>()),
      "Invalid unordered index");
#endif
}
