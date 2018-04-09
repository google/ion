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

#ifndef ION_BASE_TESTS_LOGGING_TEST_UTIL_H_
#define ION_BASE_TESTS_LOGGING_TEST_UTIL_H_

#include <ostream>

#include "ion/base/logging.h"

namespace ion {
namespace base {
namespace testing {

class TestInt;
std::ostream& operator<<(std::ostream& os, const TestInt& rhs);

// TestInt is an integer instrumented with a simple test stat, the comparison
// count.
class TestInt {
 public:
  // Default constructor initializes a zero integer that has never been
  // compared.
  TestInt() : value_(0), compare_count_(0) {}
  // Initialize a TestInt with given value, which has never been compared.
  explicit TestInt(int value) : value_(value), compare_count_(0) {}

  // Comparisons. Not implemented in terms of each other as in this case that is
  // actually less readable than a table. Note that Value modifies the TestInt
  // to collect the comparison statistic, which is mutable.

  // clang-format off
  bool operator ==(const TestInt& rhs) const { return Value() == rhs.Value(); }
  bool operator !=(const TestInt& rhs) const { return Value() != rhs.Value(); }
  bool operator <=(const TestInt& rhs) const { return Value() <= rhs.Value(); }
  bool operator < (const TestInt& rhs) const { return Value() <  rhs.Value(); }
  bool operator >=(const TestInt& rhs) const { return Value() >= rhs.Value(); }
  bool operator > (const TestInt& rhs) const { return Value() >  rhs.Value(); }
  // clang-format on

  // Retrieve the value for test logging/inspection.
  int GetValue() const { return value_; }

  // Retrieve the value for instrumented comparison.
  int Value() const {
    ++compare_count_;
    return value_;
  }

  // Retrieve the comparison count for test examination.
  int GetComparisonCount() const { return compare_count_; }

 private:
  // Holds definition of the TestInt's actual value.
  int value_;

  // Total comparisons performed on or using the TestInt. Mutable as some
  // CHECK/QCHECK implementations require const expression inputs.
  mutable int compare_count_;
};

// The insertion operator logs the value and comparison count of |rhs| to |os|.
inline std::ostream& operator<<(std::ostream& os, const TestInt& rhs) {
  os << "{" << rhs.GetValue() << ", " << rhs.GetComparisonCount() << "}";
  return os;
}

}  // namespace testing
}  // namespace base
}  // namespace ion

#endif  // ION_BASE_TESTS_LOGGING_TEST_UTIL_H_
