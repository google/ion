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

#ifndef ION_MATH_UTILS_H_
#define ION_MATH_UTILS_H_

// This file contains math utility functions that are not associated with any
// particular class.

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "base/integral_types.h"

namespace ion {
namespace math {

// Tests whether a numeric value is finite.
template <typename T>
inline bool IsFinite(T x) {
  return (x == x && x != std::numeric_limits<T>::infinity() &&
          x != -std::numeric_limits<T>::infinity());
}

// Returns the absolute value of a number in a type-safe way.
template <typename T>
inline T Abs(const T& val) {
  return val >= static_cast<T>(0) ? val : -val;
}

// Tests whether a scalar is close to zero.
template <typename T, typename = typename std::enable_if<
                          std::is_arithmetic<T>::value>::type>
inline bool AlmostZero(T a, T tolerance = std::numeric_limits<T>::epsilon()) {
  return Abs(a) <= Abs(tolerance);
}

// Tests whether two scalar values are close enough.
template <typename T, typename = typename std::enable_if<
                          std::is_arithmetic<T>::value>::type>
inline bool AlmostEqual(T a, T b, T tolerance) {
  return AlmostZero(a - b, tolerance);
}

// Squares a value.
template <typename T>
inline T Square(const T& val) {
  return val * val;
}

// Returns the square root of a value.
//
// There is no standard version of sqrt() for integer types, resulting in
// ambiguity problems when an integer is passed to it. This is especially
// annoying in template functions. The Sqrt() function is specialized to avoid
// these problems.
template <typename T>
inline T Sqrt(const T& val) {
  return static_cast<T>(sqrt(static_cast<double>(val)));
}
template <>
inline float Sqrt(const float& val) {
  return sqrtf(val);
}

// Returns the cosine of the given value.
template <typename T>
inline T Cosine(T angle) {
  return cos(angle);
}
// float specialization of Cosine.
template <>
inline float Cosine(float angle) {
  return cosf(angle);
}

// Returns the sine of the given value.
template <typename T>
inline T Sine(T angle) {
  return sin(angle);
}
// float specialization of Sine.
template <>
inline float Sine(float angle) {
  return sinf(angle);
}

// Returns the tangent of the given value.
template <typename T>
inline T Tangent(T angle) {
  return tan(angle);
}
// float specialization of Tangent.
template <>
inline float Tangent(float angle) {
  return tanf(angle);
}

// Returns the factorial (!) of x.
// If x < 0, it returns 0.
template <typename T>
inline T Factorial(int x) {
  if (x < 0) return 0;
  T result = 1;
  for (; x > 0; x--) result *= x;
  return result;
}

// Returns the double factorial (!!) of x.
// For odd x:  1 * 3 * 5 * ... * (x - 2) * x
// For even x: 2 * 4 * 6 * ... * (x - 2) * x
// If x < 0, it returns 0.
template <typename T>
inline T DoubleFactorial(int x) {
  if (x < 0) return 0;
  T result = 1;
  for (; x > 0; x -= 2) result *= x;
  return result;
}

// Returns the next power of 2 greater than or equal to n. This works only for
// unsigned 32-bit or 64-bit integers.
inline uint32 NextPowerOf2(uint32 n) {
  if (n == 0) return 0;
  n -= 1;
  n |= n >> 16;
  n |= n >> 8;
  n |= n >> 4;
  n |= n >> 2;
  n |= n >> 1;
  return n + 1;
}
inline uint64 NextPowerOf2(uint64 n) {
  if (n == 0) return 0;
  n -= 1;
  n |= n >> 32;
  n |= n >> 16;
  n |= n >> 8;
  n |= n >> 4;
  n |= n >> 2;
  n |= n >> 1;
  return n + 1;
}

// Returns the base-2 logarithm of n.
template <typename T>
inline T Log2(T n) {
  static const T kInverseOfLogOf2 = static_cast<T>(1.4426950408889634);
  return static_cast<T>(log(n) * kInverseOfLogOf2);
}
// Specialize for integer types. See
// http://graphics.stanford.edu/~seander/bithacks.html#IntegerLogObvious
// for details. These implementations are in the public domain.
template <>
inline uint32 Log2(uint32 n) {
  static const uint32 kMultiplyDeBruijnBitPosition[32] = {
      0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
      8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31};

  n |= n >> 1;  // First round down to one less than a power of 2.
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;

  return kMultiplyDeBruijnBitPosition[(n * 0x07C4ACDDU) >> 27];
}
template <>
inline int Log2(int n) {
  if (n <= 0)
    return 0;
  else
    return static_cast<int>(Log2(static_cast<uint32>(n)));
}
template <>
inline uint64 Log2(uint64 n) {
  const uint32 kMultiplyDeBruijnBitPosition[64] = {
      63, 0,  58, 1,  59, 47, 53, 2,  60, 39, 48, 27, 54, 33, 42, 3,
      61, 51, 37, 40, 49, 18, 28, 20, 55, 30, 34, 11, 43, 14, 22, 4,
      62, 57, 46, 52, 38, 26, 32, 41, 50, 36, 17, 19, 29, 10, 13, 21,
      56, 45, 25, 31, 35, 16, 9,  12, 44, 24, 15, 8,  23, 7,  6,  5};
  n |= n >> 1;  // First round down to one less than a power of 2.
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n |= n >> 32;
  n -= n >> 1;

  return kMultiplyDeBruijnBitPosition[(n * 0x07EDD5E59A4E28C2ULL) >> 58];
}
template <>
inline int64 Log2(int64 n) {
  if (n <= 0)
    return 0;
  else
    return static_cast<int64>(Log2(static_cast<uint64>(n)));
}

// Clamps a value to lie between a minimum and maximum, inclusive. This is
// supported for any type for which std::min() and std::max() are implemented.
template <typename T>
inline T Clamp(const T& val, const T& min_val, const T& max_val) {
  return std::min(std::max(val, min_val), max_val);
}

// Linearly interpolates between two values. T must have multiplication and
// addition operators defined. Performs extrapolation for t outside [0, 1].
template <typename T, typename U>
inline U Lerp(const U& begin, const U& end, const T& t) {
  return static_cast<U>(begin + t * (end - begin));
}

// Suppress implicit int-to-float warning. Assume users know what they're doing.
inline int Lerp(int begin, int end, float t) {
  return static_cast<int>(static_cast<float>(begin) +
                          t * static_cast<float>(end - begin));
}

// Returns true if a value is a power of two.
inline bool IsPowerOfTwo(int value) {
  return (value != 0) && ((value & (value - 1)) == 0);
}

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_UTILS_H_
