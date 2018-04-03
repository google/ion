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

#include "ion/base/scalarsequence.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

using ion::base::ScalarSequenceGenerator;

TEST(ScalarSequence, EmptySizeT) {
  using sequence = ScalarSequenceGenerator<size_t, 0>::Sequence;
  auto array = sequence::ToArray();
  EXPECT_EQ(array.size(), 0U);
}

TEST(ScalarSequence, OneSizeT) {
  using sequence = ScalarSequenceGenerator<size_t, 1>::Sequence;
  auto array = sequence::ToArray();
  EXPECT_EQ(array.size(), 1U);
  EXPECT_EQ(array[0], 0U);
}

TEST(ScalarSequence, TwoSizeT) {
  using sequence = ScalarSequenceGenerator<size_t, 2>::Sequence;
  auto array = sequence::ToArray();
  EXPECT_EQ(array.size(), 2U);
  EXPECT_EQ(array[0], 0U);
  EXPECT_EQ(array[1], 1U);
}

TEST(ScalarSequence, TwoSizeTStepTwo) {
  using sequence = ScalarSequenceGenerator<size_t, 2, 2>::Sequence;
  auto array = sequence::ToArray();
  EXPECT_EQ(array.size(), 2U);
  EXPECT_EQ(array[0], 0U);
  EXPECT_EQ(array[1], 2U);
}
