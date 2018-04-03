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

#include "ion/text/sdfutils.h"

#include "ion/base/array2.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

TEST(SdfutilsTest, ComputeSdfGrid) {
  // Hard-coded SDF test.
  static const size_t kWidth = 4U;
  static const size_t kHeight = 5U;
  static const double kGridValues[kWidth * kHeight] = {
    0.0, 0.2, 0.4, 0.0,
    0.1, 0.3, 0.5, 0.3,
    0.3, 0.6, 1.0, 0.8,
    0.1, 0.3, 0.5, 0.3,
    0.0, 0.2, 0.4, 0.0,
  };
  base::Array2<double> image(kWidth, kHeight);
  for (size_t y = 0; y < kHeight; ++y) {
    for (size_t x = 0; x < kWidth; ++x) {
      image.Set(x, y, kGridValues[y * kWidth + x]);
    }
  }

  // Pad by 2 pixels on all 4 sides.
  static const size_t kPadding = 2U;
  static const size_t kSdfWidth = kWidth + 2U * kPadding;
  static const size_t kSdfHeight = kHeight + 2U * kPadding;
  const base::Array2<double> sdf = ComputeSdfGrid(image, kPadding);
  EXPECT_EQ(kSdfWidth, sdf.GetWidth());
  EXPECT_EQ(kSdfHeight, sdf.GetHeight());

  static const double kExpectedSdfValues[kSdfWidth * kSdfHeight] = {
    3.87, 3.09, 2.51, 2.30, 2.10, 2.33, 2.90, 3.69,
    3.22, 2.51, 1.67, 1.30, 1.10, 1.49, 2.33, 2.99,
    2.62, 1.81, 1.30, 0.26, 0.10, 1.10, 1.57, 2.41,
    2.40, 1.40, 0.39, 0.16, 0.00, 0.16, 1.15, 1.97,
    2.20, 1.20, 0.20, -0.10, -1.00, -0.30, 0.70, 1.70,
    2.40, 1.40, 0.39, 0.16, 0.00, 0.16, 1.15, 1.97,
    2.62, 1.81, 1.30, 0.26, 0.10, 1.10, 1.57, 2.41,
    3.22, 2.51, 1.67, 1.30, 1.10, 1.49, 2.33, 2.99,
    3.87, 3.09, 2.51, 2.30, 2.10, 2.33, 2.90, 3.69,
  };
  // 2-decimal tolerance.
  static const double kTolerance = 5.e-3;
  for (size_t y = 0; y < kSdfHeight; ++y) {
    for (size_t x = 0; x < kSdfWidth; ++x) {
      SCOPED_TRACE(::testing::Message() << "x = " << x << ", y = " << y);
      EXPECT_NEAR(kExpectedSdfValues[y * kSdfWidth + x], sdf.Get(x, y),
                  kTolerance);
    }
  }
}

}  // namespace text
}  // namespace ion
