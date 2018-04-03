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

#include "ion/base/enumhelper.h"

#include "base/integral_types.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

enum Values {
  kValue1,
  kValue2,
  kValue3
};

template <>
const EnumHelper::EnumData<Values> EnumHelper::GetEnumData() {
  static const uint32 kValues[] = {
    1U, 2U, 3U
  };
  static const char* kStrings[] = {
    "Value1", "Value2", "Value3"
  };
  return EnumData<Values>(IndexMap<Values, uint32>(kValues, 3), kStrings);
}

TEST(EnumHelper, Gets) {
  EXPECT_EQ(3U, EnumHelper::GetCount<Values>());

  EXPECT_EQ("Value1", EnumHelper::GetString(kValue1));
  EXPECT_EQ("Value2", EnumHelper::GetString(kValue2));
  EXPECT_EQ("Value3", EnumHelper::GetString(kValue3));

  EXPECT_EQ(1U, EnumHelper::GetConstant(kValue1));
  EXPECT_EQ(2U, EnumHelper::GetConstant(kValue2));
  EXPECT_EQ(3U, EnumHelper::GetConstant(kValue3));

  EXPECT_EQ(kValue1, EnumHelper::GetEnum<Values>(1U));
  EXPECT_EQ(kValue2, EnumHelper::GetEnum<Values>(2U));
  EXPECT_EQ(kValue3, EnumHelper::GetEnum<Values>(3U));
}

}  // namespace base
}  // namespace ion
