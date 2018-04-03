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

#include "ion/base/once.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

static std::atomic<int32> s_counter;
// Simple populator function for testing lazy.
static int GetThree() {
  s_counter++;
  return 3;
}

TEST(Once, BasicLazy) {
  s_counter = 0;
  Lazy<int> lazy(&GetThree);
  EXPECT_EQ(0, s_counter);
  EXPECT_EQ(3, lazy.Get());
  EXPECT_EQ(1, s_counter);
  EXPECT_EQ(3, lazy.Get());
  EXPECT_EQ(1, s_counter);
}

TEST(Once, VectorLazy) {
  s_counter = 0;
  std::vector<Lazy<int> > lazy_vector;
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  EXPECT_EQ(0, s_counter);
  for (size_t i = 0; i < lazy_vector.size(); i++) {
    EXPECT_EQ(3, lazy_vector[i].Get());
    EXPECT_EQ(i + 1, static_cast<size_t>(s_counter));
  }
  for (size_t i = 0; i < lazy_vector.size(); i++) {
    EXPECT_EQ(3, lazy_vector[i].Get());
    EXPECT_EQ(lazy_vector.size(), static_cast<size_t>(s_counter));
  }
}

}  // namespace base
}  // namespace ion
