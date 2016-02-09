/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#include "ion/base/invalid.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

enum DummyEnum {
  Valid1,
  Valid2,
};

struct DummyStruct {
  int foo;
  char bar[32];
};

TEST(Invalid, InvalidIndex) {
  EXPECT_EQ(static_cast<size_t>(-1), ion::base::kInvalidIndex);
}

TEST(Invalid, InvalidReference) {
  const int& invalid_int = ion::base::InvalidReference<int>();
  EXPECT_TRUE(&invalid_int != NULL);

  const DummyStruct& invalid_dummy = ion::base::InvalidReference<DummyStruct>();
  EXPECT_TRUE(&invalid_dummy != NULL);
}

TEST(Invalid, InvalidEnum) {
  EXPECT_EQ(-1, static_cast<int>(ion::base::InvalidEnumValue<DummyEnum>()));
}
