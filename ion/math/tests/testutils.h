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

#ifndef ION_MATH_TESTS_TESTUTILS_H_
#define ION_MATH_TESTS_TESTUTILS_H_

//
// This file contains utility functions to help test the math library.
//

#include <cmath>

#include "ion/math/matrix.h"
#include "ion/math/matrixutils.h"
#include "ion/math/rotation.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"
#include "third_party/googletest/googlemock/include/gmock/gmock.h"  // NOLINT

namespace ion {
namespace math {
namespace testing {

MATCHER_P2(IsAlmostEqual, b, tolerance,
           std::string(negation ? "further than " : "at most ") +
               ::testing::PrintToString(tolerance) +
               std::string(negation ? " from " : " away from ") +
               ::testing::PrintToString(b)) {
  return AlmostEqual(arg, b, tolerance);
}

// Returns true if all elements of two Vectors are equal within a tolerance.
template <int Dimension, typename T>
static bool VectorsAlmostEqual(const Vector<Dimension, T>& v0,
                               const Vector<Dimension, T>& v1) {
  static const T kTolerance = static_cast<T>(1e-8);
  return math::VectorsAlmostEqual(v0, v1, kTolerance);
}

// Returns true if all elements of two Points are equal within a tolerance.
template <int Dimension, typename T>
static bool PointsAlmostEqual(const Point<Dimension, T>& p0,
                              const Point<Dimension, T>& p1) {
  static const T kTolerance = static_cast<T>(1e-8);
  return math::PointsAlmostEqual(p0, p1, kTolerance);
}

// Returns true if all elements of two matrices are equal within a tolerance.
template <int Dimension, typename T>
static bool MatricesAlmostEqual(const Matrix<Dimension, T>& m0,
                                const Matrix<Dimension, T>& m1) {
  // Slightly larger tolerance required for matrix inverse math.
  static const T kTolerance = static_cast<T>(1e-6);
  return math::MatricesAlmostEqual(m0, m1, kTolerance);
}

// Returns true if the rotations are equal within a tolerance, or are within a
// tolerance of being antipodal.
template <typename T>
static bool RotationsAlmostEqual(const Rotation<T>& r0, const Rotation<T>& r1) {
  static const T kTolerance = static_cast<T>(1e-6);
  return math::VectorsAlmostEqual(r0.GetQuaternion(), r1.GetQuaternion(),
                                  kTolerance)
         || math::VectorsAlmostEqual(r0.GetQuaternion(), -r1.GetQuaternion(),
                                    kTolerance);
}

}  // namespace testing
}  // namespace math
}  // namespace ion

#endif  // ION_MATH_TESTS_TESTUTILS_H_
