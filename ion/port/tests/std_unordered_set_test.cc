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

#include <string>
#include <unordered_set>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

// These tests are to verify that unordered_set works on the local platform.

TEST(UnorderedSet, IntSet) {
  typedef std::unordered_set<int> SetType;
  SetType myset;

  EXPECT_EQ(0U, myset.count(0));
  EXPECT_EQ(0U, myset.count(12));

  myset.insert(12);
  EXPECT_EQ(0U, myset.count(0));
  EXPECT_EQ(1U, myset.count(12));

  myset.insert(12);
  EXPECT_EQ(0U, myset.count(0));
  EXPECT_EQ(1U, myset.count(12));

  myset.insert(0);
  myset.insert(0);
  myset.insert(0);
  EXPECT_EQ(1U, myset.count(0));
  EXPECT_EQ(1U, myset.count(12));
}
