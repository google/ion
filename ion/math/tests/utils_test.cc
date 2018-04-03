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

#include "ion/math/utils.h"
#include "ion/math/tests/testutils.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(Utils, IsFinite) {
  using ion::math::IsFinite;

  EXPECT_FALSE(IsFinite(std::numeric_limits<float>::quiet_NaN()));
  EXPECT_FALSE(IsFinite(std::numeric_limits<float>::infinity()));
  EXPECT_FALSE(IsFinite(-std::numeric_limits<float>::infinity()));
  EXPECT_TRUE(IsFinite(0.0f));
  EXPECT_TRUE(IsFinite(9999999999.0f));
  EXPECT_TRUE(IsFinite(-9999999999.0f));

  EXPECT_FALSE(IsFinite(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_FALSE(IsFinite(std::numeric_limits<double>::infinity()));
  EXPECT_FALSE(IsFinite(-std::numeric_limits<double>::infinity()));
  EXPECT_TRUE(IsFinite(0.0));
  EXPECT_TRUE(IsFinite(9999999999.0));
  EXPECT_TRUE(IsFinite(-9999999999.0));
}

TEST(Utils, Abs) {
  using ion::math::Abs;

  EXPECT_EQ(0, Abs(0));
  EXPECT_EQ(0LL, Abs(0LL));
  EXPECT_EQ(0.0f, Abs(0.0f));
  EXPECT_EQ(0.0, Abs(0.0));

  EXPECT_EQ(1, Abs(1));
  EXPECT_EQ(1LL, Abs(1LL));
  EXPECT_EQ(0.1f, Abs(0.1f));
  EXPECT_EQ(0.01, Abs(0.01));

  EXPECT_EQ(1, Abs(-1));
  EXPECT_EQ(1LL, Abs(-1LL));
  EXPECT_EQ(0.1f, Abs(-0.1f));
  EXPECT_EQ(0.01, Abs(-0.01));
}

TEST(Utils, ScalarsAlmostEqual) {
  using ion::math::testing::IsAlmostEqual;
  using ::testing::Not;

  EXPECT_THAT(14.f, IsAlmostEqual(14.001f, 0.0015f));
  EXPECT_THAT(14.f, Not(IsAlmostEqual(14.002f, 0.0015f)));
  EXPECT_THAT(14.0, IsAlmostEqual(14.001, 0.0015));
  EXPECT_THAT(14.0, Not(IsAlmostEqual(14.002, 0.0015)));
}

TEST(Utils, ScalarsAlmostZero) {
  using ion::math::AlmostZero;

  EXPECT_TRUE(AlmostZero(0.0f));
  EXPECT_TRUE(AlmostZero(std::numeric_limits<float>::epsilon()));
  EXPECT_FALSE(AlmostZero(std::numeric_limits<float>::epsilon() * 2.0f));
  EXPECT_TRUE(AlmostZero(-std::numeric_limits<float>::epsilon()));
  EXPECT_FALSE(AlmostZero(-std::numeric_limits<float>::epsilon() * 2.0f));

  EXPECT_TRUE(AlmostZero(0.0));
  EXPECT_TRUE(AlmostZero(std::numeric_limits<double>::epsilon()));
  EXPECT_FALSE(AlmostZero(std::numeric_limits<double>::epsilon() * 2.0));
  EXPECT_TRUE(AlmostZero(-std::numeric_limits<double>::epsilon()));
  EXPECT_FALSE(AlmostZero(-std::numeric_limits<double>::epsilon() * 2.0));

  const float tolerance_float = 0.001f;
  EXPECT_TRUE(AlmostZero(0.0f, tolerance_float));
  EXPECT_TRUE(AlmostZero(tolerance_float, tolerance_float));
  EXPECT_FALSE(AlmostZero(tolerance_float * 2.0f, tolerance_float));
  EXPECT_TRUE(AlmostZero(-tolerance_float, tolerance_float));
  EXPECT_FALSE(AlmostZero(-tolerance_float * 2.0f, tolerance_float));

  const double tolerance_double = 0.001;
  EXPECT_TRUE(AlmostZero(0.0, tolerance_double));
  EXPECT_TRUE(AlmostZero(tolerance_double, tolerance_double));
  EXPECT_FALSE(AlmostZero(tolerance_double * 2.0f, tolerance_double));
  EXPECT_TRUE(AlmostZero(-tolerance_double, tolerance_double));
  EXPECT_FALSE(AlmostZero(-tolerance_double * 2.0f, tolerance_double));
}

TEST(Utils, Square) {
  using ion::math::Square;

  EXPECT_EQ(9, Square(3));
  EXPECT_EQ(9, Square(-3));

  EXPECT_NEAR(1.21, Square(1.1), 1e-10);
  EXPECT_NEAR(1.21, Square(-1.1), 1e-10);
}

TEST(Utils, Sqrt) {
  using ion::math::Sqrt;

  // Integers should work ok.
  EXPECT_EQ(0, Sqrt(0));
  EXPECT_EQ(1, Sqrt(1));
  EXPECT_EQ(3, Sqrt(9));
  EXPECT_EQ(5, Sqrt(26));

  // Doubles and floats are specialized.
  EXPECT_NEAR(1.1, Sqrt(1.21), 1e-10);
  EXPECT_NEAR(1.1f, Sqrt(1.21f), 1e-8f);
}

TEST(Utils, Cosine) {
  using ion::math::Cosine;

  // Test double values (no specialization).
  EXPECT_NEAR(1.0, Cosine(0.0), 1e-10);
  EXPECT_NEAR(0.0, Cosine(M_PI / 2.0), 1e-10);
  EXPECT_NEAR(0.0, Cosine(-M_PI / 2.0), 1e-10);
  EXPECT_NEAR(-1.0, Cosine(M_PI), 1e-10);
  EXPECT_NEAR(-1.0, Cosine(-M_PI), 1e-10);

  // Test float values (specialized).
  EXPECT_NEAR(1.0f, Cosine(0.0f), 1e-7);
  EXPECT_NEAR(0.0f, Cosine(static_cast<float>(M_PI / 2.0)), 1e-7);
  EXPECT_NEAR(0.0f, Cosine(static_cast<float>(-M_PI / 2.0)), 1e-7);
  EXPECT_NEAR(-1.0f, Cosine(static_cast<float>(M_PI)), 1e-7);
  EXPECT_NEAR(-1.0f, Cosine(static_cast<float>(-M_PI)), 1e-7);
}

TEST(Utils, Sine) {
  using ion::math::Sine;

  // Test double values (no specialization).
  EXPECT_NEAR(0.0, Sine(0.0), 1e-10);
  EXPECT_NEAR(1.0, Sine(M_PI / 2.0), 1e-10);
  EXPECT_NEAR(-1.0, Sine(-M_PI / 2.0), 1e-10);
  EXPECT_NEAR(0.0, Sine(M_PI), 1e-10);
  EXPECT_NEAR(0.0, Sine(-M_PI), 1e-10);

  // Test float values (specialized).
  EXPECT_NEAR(0.0f, Sine(0.0f), 1e-7);
  EXPECT_NEAR(1.0f, Sine(static_cast<float>(M_PI / 2.0)), 1e-7);
  EXPECT_NEAR(-1.0f, Sine(static_cast<float>(-M_PI / 2.0)), 1e-7);
  EXPECT_NEAR(0.0f, Sine(static_cast<float>(M_PI)), 1e-7);
  EXPECT_NEAR(0.0f, Sine(static_cast<float>(-M_PI)), 1e-7);
}

TEST(Utils, Tangent) {
  using ion::math::Tangent;

  // Test double values (no specialization).
  EXPECT_NEAR(0.0, Tangent(0.0), 1e-10);
  EXPECT_NEAR(1.0, Tangent(M_PI / 4.0), 1e-10);
  EXPECT_NEAR(-1.0, Tangent(-M_PI / 4.0), 1e-10);
  EXPECT_NEAR(0.0, Tangent(M_PI), 1e-10);
  EXPECT_NEAR(0.0, Tangent(-M_PI), 1e-10);

  // Test float values (specialized).
  EXPECT_NEAR(0.0f, Tangent(0.0f), 1e-7);
  EXPECT_NEAR(1.0f, Tangent(static_cast<float>(M_PI / 4.0)), 1e-7);
  EXPECT_NEAR(-1.0f, Tangent(static_cast<float>(-M_PI / 4.0)), 1e-7);
  EXPECT_NEAR(0.0f, Tangent(static_cast<float>(M_PI)), 1e-7);
  EXPECT_NEAR(0.0f, Tangent(static_cast<float>(-M_PI)), 1e-7);
}

TEST(Utils, Factorial) {
  using ion::math::Factorial;

  // No specialized versions, so we just test 32-bit int.
  int accumulated_result = 1;
  EXPECT_EQ(accumulated_result, Factorial<int>(0));
  EXPECT_EQ(accumulated_result, Factorial<int>(1));
  EXPECT_EQ(accumulated_result *= 2, Factorial<int>(2));
  EXPECT_EQ(accumulated_result *= 3, Factorial<int>(3));
  EXPECT_EQ(accumulated_result *= 4, Factorial<int>(4));
  EXPECT_EQ(accumulated_result *= 5, Factorial<int>(5));
  EXPECT_EQ(accumulated_result *= 6, Factorial<int>(6));
  EXPECT_EQ(accumulated_result *= 7, Factorial<int>(7));
  EXPECT_EQ(accumulated_result *= 8, Factorial<int>(8));
}

TEST(Utils, DoubleFactorial) {
  using ion::math::DoubleFactorial;

  // Test 32-bit even values.
  int accumulated_result = 1;
  EXPECT_EQ(accumulated_result, DoubleFactorial<int>(0));
  EXPECT_EQ(accumulated_result *= 2, DoubleFactorial<int>(2));
  EXPECT_EQ(accumulated_result *= 4, DoubleFactorial<int>(4));
  EXPECT_EQ(accumulated_result *= 6, DoubleFactorial<int>(6));
  EXPECT_EQ(accumulated_result *= 8, DoubleFactorial<int>(8));
  EXPECT_EQ(accumulated_result *= 10, DoubleFactorial<int>(10));

  // Test 32-bit odd values.
  accumulated_result = 1;
  EXPECT_EQ(accumulated_result, DoubleFactorial<int>(1));
  EXPECT_EQ(accumulated_result *= 3, DoubleFactorial<int>(3));
  EXPECT_EQ(accumulated_result *= 5, DoubleFactorial<int>(5));
  EXPECT_EQ(accumulated_result *= 7, DoubleFactorial<int>(7));
  EXPECT_EQ(accumulated_result *= 9, DoubleFactorial<int>(9));
  EXPECT_EQ(accumulated_result *= 11, DoubleFactorial<int>(11));
}

TEST(Utils, NextPowerOf2) {
  using ion::math::NextPowerOf2;

  // 32-bit.
  EXPECT_EQ(0U, NextPowerOf2(0U));
  EXPECT_EQ(1U, NextPowerOf2(1U));
  EXPECT_EQ(2U, NextPowerOf2(2U));
  EXPECT_EQ(4U, NextPowerOf2(3U));
  EXPECT_EQ(4U, NextPowerOf2(4U));
  EXPECT_EQ(16U, NextPowerOf2(15U));
  EXPECT_EQ(16U, NextPowerOf2(16U));
  EXPECT_EQ(32U, NextPowerOf2(17U));
  EXPECT_EQ(32U, NextPowerOf2(31U));
  EXPECT_EQ(32U, NextPowerOf2(32U));
  EXPECT_EQ(64U, NextPowerOf2(33U));
  EXPECT_EQ(1U << 30, NextPowerOf2(1U << 30));
  EXPECT_EQ(1U << 31, NextPowerOf2((1U << 30) + 1));

  // 64-bit.
  EXPECT_EQ(0ULL, NextPowerOf2(0ULL));
  EXPECT_EQ(1ULL, NextPowerOf2(1ULL));
  EXPECT_EQ(2ULL, NextPowerOf2(2ULL));
  EXPECT_EQ(4ULL, NextPowerOf2(3ULL));
  EXPECT_EQ(4ULL, NextPowerOf2(4ULL));
  EXPECT_EQ(16ULL, NextPowerOf2(15ULL));
  EXPECT_EQ(16ULL, NextPowerOf2(16ULL));
  EXPECT_EQ(32ULL, NextPowerOf2(17ULL));
  EXPECT_EQ(32ULL, NextPowerOf2(31ULL));
  EXPECT_EQ(32ULL, NextPowerOf2(32ULL));
  EXPECT_EQ(64ULL, NextPowerOf2(33ULL));
  EXPECT_EQ(1ULL << 30, NextPowerOf2(1ULL << 30));
  EXPECT_EQ(1ULL << 61, NextPowerOf2(1ULL << 61));
  EXPECT_EQ(1ULL << 62, NextPowerOf2((1ULL << 61) + 1));
  EXPECT_EQ(1ULL << 62, NextPowerOf2(1ULL << 62));
  EXPECT_EQ(1ULL << 63, NextPowerOf2((1ULL << 62) + 1));
}

TEST(Utils, Log2) {
  using ion::math::Log2;

  // Powers of 2.
  EXPECT_NEAR(0., Log2(1.), 1e-10);
  EXPECT_NEAR(1., Log2(2.), 1e-10);
  EXPECT_NEAR(3., Log2(8.), 1e-10);
  EXPECT_NEAR(7., Log2(128.), 1e-10);
  EXPECT_NEAR(10., Log2(1024.), 1e-10);

  // Non powers of two.
  EXPECT_NEAR(1.58496250072116, Log2(3.), 1e-10);
  EXPECT_NEAR(4.75488750216347, Log2(27.), 1e-10);
  EXPECT_NEAR(9.01122725542326, Log2(516.), 1e-10);

  // Integers are specialized.
  EXPECT_EQ(0, Log2(-1));
  EXPECT_EQ(0, Log2(0));
  EXPECT_EQ(0, Log2(1));
  EXPECT_EQ(1, Log2(2));
  EXPECT_EQ(3, Log2(8));
  EXPECT_EQ(5, Log2(32));
  EXPECT_EQ(5, Log2(63));
  EXPECT_EQ(6, Log2(65));
  EXPECT_EQ(9, Log2(1023));
  EXPECT_EQ(10, Log2(1024));

  // 64-bit integers are also specialized.
  EXPECT_EQ(0LL, Log2(-1LL));
  EXPECT_EQ(0LL, Log2(0LL));
  EXPECT_EQ(0LL, Log2(1LL));
  EXPECT_EQ(1LL, Log2(2LL));
  EXPECT_EQ(3LL, Log2(8LL));
  EXPECT_EQ(5LL, Log2(32LL));
  EXPECT_EQ(5LL, Log2(63LL));
  EXPECT_EQ(6LL, Log2(65LL));
  EXPECT_EQ(9LL, Log2(1023LL));
  EXPECT_EQ(10LL, Log2(1024LL));
  EXPECT_EQ(39LL, Log2(1099511627775LL));
  EXPECT_EQ(40LL, Log2(1099511627776LL));
  EXPECT_EQ(40LL, Log2(1099511627777LL));
  EXPECT_EQ(45ULL, Log2(35184372088832ULL));
  EXPECT_EQ(45ULL, Log2(70368744177663ULL));
  EXPECT_EQ(46ULL, Log2(70368744177664ULL));
}

// Integers should work ok.
TEST(Utils, Clamp) {
  using ion::math::Clamp;

  // Integers.
  EXPECT_EQ(17, Clamp(17, 13, 100));
  EXPECT_EQ(14, Clamp(14, 13, 100));
  EXPECT_EQ(13, Clamp(13, 13, 100));
  EXPECT_EQ(13, Clamp(4, 13, 100));
  EXPECT_EQ(13, Clamp(0, 13, 100));
  EXPECT_EQ(13, Clamp(-100, 13, 100));
  EXPECT_EQ(100, Clamp(100, 13, 100));
  EXPECT_EQ(100, Clamp(101, 13, 100));
  EXPECT_EQ(100, Clamp(123525543, 13, 100));

  // Doubles.
  EXPECT_EQ(17.0, Clamp(17.0, 13.0, 100.0));
  EXPECT_EQ(14.0, Clamp(14.0, 13.0, 100.0));
  EXPECT_EQ(13.0, Clamp(13.0, 13.0, 100.0));
  EXPECT_EQ(13.0, Clamp(4.0, 13.0, 100.0));
  EXPECT_EQ(13.0, Clamp(0.0, 13.0, 100.0));
  EXPECT_EQ(13.0, Clamp(-100.0, 13.0, 100.0));
  EXPECT_EQ(100.0, Clamp(100.0, 13.0, 100.0));
  EXPECT_EQ(100.0, Clamp(101.0, 13.0, 100.0));
  EXPECT_EQ(100.0, Clamp(123525543.0, 13.0, 100.0));
}

TEST(Utils, Lerp) {
  using ion::math::Lerp;

  // Integers.
  EXPECT_EQ(250, Lerp(100, 200, 1.5));
  EXPECT_EQ(200, Lerp(100, 200, 1.0));
  EXPECT_EQ(117, Lerp(100, 200, 0.17));
  EXPECT_EQ(100, Lerp(100, 200, 0.0));
  EXPECT_EQ(99, Lerp(100, 200, -0.01));
  EXPECT_EQ(0, Lerp(100, 200, -1.0));
  EXPECT_EQ(-100, Lerp(100, 200, -2.0));

  // Floats.
  EXPECT_EQ(2.5f, Lerp(1.0f, 2.0f, 1.5f));
  EXPECT_EQ(2.0f, Lerp(1.0f, 2.0f, 1.0f));
  EXPECT_EQ(1.17f, Lerp(1.0f, 2.0f, 0.17f));
  EXPECT_EQ(1.0f, Lerp(1.0f, 2.0f, 0.0f));
  EXPECT_EQ(0.99f, Lerp(1.0f, 2.0f, -0.01f));
  EXPECT_EQ(0.0f, Lerp(1.0f, 2.0f, -1.0f));
  EXPECT_EQ(-1.0f, Lerp(1.0f, 2.0f, -2.0f));

  // Doubles.
  EXPECT_EQ(2.5, Lerp(1.0, 2.0, 1.5));
  EXPECT_EQ(2.0, Lerp(1.0, 2.0, 1.0));
  EXPECT_EQ(1.17, Lerp(1.0, 2.0, 0.17));
  EXPECT_EQ(1.0, Lerp(1.0, 2.0, 0.0));
  EXPECT_EQ(0.99, Lerp(1.0, 2.0, -0.01));
  EXPECT_EQ(0.0, Lerp(1.0, 2.0, -1.0));
  EXPECT_EQ(-1.0, Lerp(1.0, 2.0, -2.0));

  // Verify Lerp'ing between ints using a float doesn't cause a conversion
  // warning. Even though there is a genuine narrowing concern on this
  // operation for ints over 8 million, it's common enough that we explicitly
  // suppress the warning for Lerp.
  EXPECT_EQ(50, Lerp(0, 100, 0.5f));
}

TEST(Utils, IsPowerOfTwo) {
  using ion::math::IsPowerOfTwo;
  EXPECT_FALSE(IsPowerOfTwo(0));

  EXPECT_TRUE(IsPowerOfTwo(1));
  EXPECT_TRUE(IsPowerOfTwo(2));
  EXPECT_FALSE(IsPowerOfTwo(3));
  EXPECT_TRUE(IsPowerOfTwo(4));

  EXPECT_FALSE(IsPowerOfTwo(63));
  EXPECT_TRUE(IsPowerOfTwo(64));
  EXPECT_FALSE(IsPowerOfTwo(65));

  EXPECT_FALSE(IsPowerOfTwo(-1));
  EXPECT_FALSE(IsPowerOfTwo(-2));
  EXPECT_FALSE(IsPowerOfTwo(-4));

  for (int i = 2; i < 30; ++i) {
    const int p2 = 1 << i;
    SCOPED_TRACE(i);
    EXPECT_FALSE(IsPowerOfTwo(p2 - 1));
    EXPECT_TRUE(IsPowerOfTwo(p2));
    EXPECT_FALSE(IsPowerOfTwo(p2 + 1));
  }
}
