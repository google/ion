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

#ifndef ION_BASE_TESTS_MULTILINESTRINGSEQUAL_H_
#define ION_BASE_TESTS_MULTILINESTRINGSEQUAL_H_

#include <string>

#include "ion/base/stringutils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {
namespace testing {

// Compares two multi-line strings for equality. If they differ, this prints
// a more useful message than the standard string comparison does.
static inline ::testing::AssertionResult MultiLineStringsEqual(
    const std::string& s0, const std::string& s1) {
  size_t index;
  std::string line0, line1, context0, context1;
  if (base::AreMultiLineStringsEqual(
          s0, s1, &index, &line0, &line1, &context0, &context1)) {
    return ::testing::AssertionSuccess();
  } else {
    return ::testing::AssertionFailure()
           << "Strings differ at line " << index << "\n"
           << "    Expected        : \"" << line0 << "\"\n"
           << "    Expected Context:\n" << context0 << "\n"
           << "    Actual          : \"" << line1 << "\"\n"
           << "    Actual Context  :\n" << context1 << "\n";
  }
}

// Remove carriage returns to appease Windows.  The alternative would be
// a #if to define a different set of expected strings, but we can't
// reliably detect if the current environment includes carriage returns.
static inline std::string SanitizeLineEndings(std::string str) {
  str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
  return str;
}

// Multi-Line equivalence tests that ignore \r characters in the actual output.
#define EXPECT_EQ_ML(a, b) \
  EXPECT_EQ(a, ion::base::testing::SanitizeLineEndings(b))
#define EXPECT_NEQ_ML(a, b) \
  EXPECT_NE(a, ion::base::testing::SanitizeLineEndings(b))

}  // namespace testing
}  // namespace base
}  // namespace ion

#endif  // ION_BASE_TESTS_MULTILINESTRINGSEQUAL_H_
