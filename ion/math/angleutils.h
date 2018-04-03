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

#ifndef ION_MATH_ANGLEUTILS_H_
#define ION_MATH_ANGLEUTILS_H_

// This file contains math utility functions associated with the Angle class.

#include <algorithm>
#include <cmath>

#include "ion/math/angle.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"

namespace ion {
namespace math {

// Returns the inverse cosine of the given value.
template <typename T>
inline Angle<T> ArcCosine(T v) {
  return Angle<T>::FromRadians(acos(v));
}
// float specialization of ArcCosine.
template <>
inline Angle<float> ArcCosine(float v) {
  return Angle<float>::FromRadians(acosf(v));
}

// Returns the inverse sine of the given value.
template <typename T>
inline Angle<T> ArcSine(T v) {
  return Angle<T>::FromRadians(asin(v));
}
// float specialization of ArcSine.
template <>
inline Angle<float> ArcSine(float v) {
  return Angle<float>::FromRadians(asinf(v));
}

// Returns the inverse tangent of the given value.
template <typename T>
inline Angle<T> ArcTangent(T v) {
  return Angle<T>::FromRadians(atan(v));
}
// float specialization of ArcTan.
template <>
inline Angle<float> ArcTangent(float v) {
  return Angle<float>::FromRadians(atanf(v));
}

// Returns the four-quadrant inverse tangent of the given values.
template <typename T>
inline Angle<T> ArcTangent2(T y, T x) {
  return Angle<T>::FromRadians(atan2(y, x));
}
// float specialization of ArcTangent2.
template <>
inline Angle<float> ArcTangent2(float y, float x) {
  return Angle<float>::FromRadians(atan2f(y, x));
}

// ion::math::Angle specialization of Cosine.
template <typename T>
inline T Cosine(const ion::math::Angle<T>& angle) {
  return Cosine(angle.Radians());
}

// ion::math::Angle specialization of Sine.
template <typename T>
inline T Sine(const ion::math::Angle<T>& angle) {
  return Sine(angle.Radians());
}

// ion::math::Angle specialization of Tangent.
template <typename T>
inline T Tangent(const ion::math::Angle<T>& angle) {
  return Tangent(angle.Radians());
}

// Returns the angle between two unit vectors.
template <int Dimension, typename T>
inline Angle<T> AngleBetween(const ion::math::Vector<Dimension, T>& a,
                             const ion::math::Vector<Dimension, T>& b) {
  static const T kOne = static_cast<T>(1);
  // Ensure that a and b are indeed unit vectors.
  DCHECK_LE(Abs(LengthSquared(a) - kOne),
            std::numeric_limits<T>::epsilon() * 100)
      << "First input vector to AngleBetween must have unit length.";
  DCHECK_LE(Abs(LengthSquared(b) - kOne),
            std::numeric_limits<T>::epsilon() * 100)
      << "Second input vector to AngleBetween must have unit length.";

  // Clamp the dot product to [-1, 1] since the dot product could lead to values
  // slightly outside that set due to numerical inaccuracy.
  return ArcCosine(Clamp(Dot(a, b), -kOne, kOne));
}

// Wraps the angle around an interval of [0, 2PI).
// E.g. 2PI gets wrapped to 0, and -2PI gets wrapped to 0.
template <typename T>
inline Angle<T> WrapTwoPi(const Angle<T>& a) {
  static const T kTwoPi = static_cast<T>(2 * M_PI);
  if (a.Radians() >= 0.0 && a.Radians() < kTwoPi) {
    // Optimize the common case.
    return a;
  }
  T radians = static_cast<T>(fmod(a.Radians(), kTwoPi));
  // fmood returns (-2PI, 2PI) so we shift the negative results in
  // (-2PI, 0) by +2PI.
  if (radians < 0.0) {
    radians += kTwoPi;
  }
  return Angle<T>::FromRadians(radians);
}

// Returns a Lerp between angles, taking the closest-path around the
// range.
//
// The return value will always be in the range [0, 2PI).
// Note: AngleLerp performs extrapolation for t outside [0, 1].
template <typename T>
inline Angle<T> AngleLerp(Angle<T> from_angle, Angle<T> to_angle, double t) {
  T from = WrapTwoPi(from_angle).Radians();
  T to = WrapTwoPi(to_angle).Radians();
  T dist = Abs(to - from);
  static const T kPi = static_cast<T>(M_PI);
  static const T kTwoPi = static_cast<T>(2 * M_PI);
  if (dist > kPi) {
      to += to < from ? kTwoPi : -kTwoPi;
  }
  return WrapTwoPi(Angle<T>::FromRadians(Lerp(from, to, t)));
}

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_ANGLEUTILS_H_
