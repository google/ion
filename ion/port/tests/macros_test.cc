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

#include "ion/port/macros.h"

#include <algorithm>
#include <string>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace port {

TEST(Macros, PrettyFunction) {
  std::string name = ION_PRETTY_FUNCTION;
  EXPECT_EQ(1, std::count(name.begin(), name.end(), '('));
  EXPECT_EQ(1, std::count(name.begin(), name.end(), ')'));
  EXPECT_NE(std::string::npos, name.find("PrettyFunction"));
}

}  // namespace port
}  // namespace ion
