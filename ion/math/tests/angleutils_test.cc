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

#include "ion/math/angleutils.h"

#include "ion/base/logging.h"
#include "ion/port/nullptr.h"  // For kNullFunction.
#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(AngleUtils, ArcCosine) {
  using ion::math::ArcCosine;

  // Test double values (no specialization).
  EXPECT_NEAR(M_PI / 2.0, ArcCosine(0.0).Radians(), 1e-10);
  EXPECT_NEAR(0.0, ArcCosine(1.0).Radians(), 1e-10);
  EXPECT_NEAR(M_PI, ArcCosine(-1.0).Radians(), 1e-10);

  // Test float values (specialized).
  EXPECT_NEAR(static_cast<float>(M_PI / 2.0), ArcCosine(0.0f).Radians(), 1e-6);
  EXPECT_NEAR(0.0f, ArcCosine(1.0f).Radians(), 1e-6);
  EXPECT_NEAR(static_cast<float>(M_PI), ArcCosine(-1.0f).Radians(), 1e-6);
}

TEST(AngleUtils, ArcSine) {
  using ion::math::ArcSine;

  // Test double values (no specialization).
  EXPECT_NEAR(0.0, ArcSine(0.0).Radians(), 1e-10);
  EXPECT_NEAR(M_PI / 2.0, ArcSine(1.0).Radians(), 1e-10);
  EXPECT_NEAR(-M_PI / 2.0, ArcSine(-1.0).Radians(), 1e-10);

  // Test float values (specialized).
  EXPECT_NEAR(0.0f, ArcSine(0.0f).Radians(), 1e-6);
  EXPECT_NEAR(static_cast<float>(M_PI / 2.0), ArcSine(1.0f).Radians(), 1e-6);
  EXPECT_NEAR(static_cast<float>(-M_PI / 2.0), ArcSine(-1.0f).Radians(), 1e-6);
}

TEST(AngleUtils, ArcTangent) {
  using ion::math::ArcTangent;

  // Test double values (no specialization).
  EXPECT_NEAR(0.0, ArcTangent(0.0).Radians(), 1e-10);
  EXPECT_NEAR(M_PI / 4.0, ArcTangent(1.0).Radians(), 1e-10);
  EXPECT_NEAR(-M_PI / 4.0, ArcTangent(-1.0).Radians(), 1e-10);

  // Test float values (specialized).
  EXPECT_NEAR(0.0f, ArcTangent(0.0f).Radians(), 1e-6);
  EXPECT_NEAR(static_cast<float>(M_PI / 4.0), ArcTangent(1.0f).Radians(), 1e-6);
  EXPECT_NEAR(static_cast<float>(-M_PI / 4.0), ArcTangent(-1.0f).Radians(),
              1e-6);
}

TEST(AngleUtils, ArcTangent2) {
  using ion::math::ArcTangent2;

  // Test double values (no specialization).
  EXPECT_NEAR(M_PI / 4.0, ArcTangent2(123.4, 123.4).Radians(), 1e-10);
  EXPECT_NEAR(-3.0 * M_PI / 4.0, ArcTangent2(-123.4, -123.4).Radians(), 1e-10);
  EXPECT_NEAR(-M_PI / 4.0, ArcTangent2(-123.4, 123.4).Radians(), 1e-10);
  EXPECT_NEAR(3.0 * M_PI / 4.0, ArcTangent2(123.4, -123.4).Radians(), 1e-10);

  // Test float values (specialized).
  EXPECT_NEAR(static_cast<float>(M_PI / 4.0),
              ArcTangent2(123.4f, 123.4f).Radians(), 1e-6);
  EXPECT_NEAR(static_cast<float>(-3.0 * M_PI / 4.0),
              ArcTangent2(-123.4f, -123.4f).Radians(), 1e-6);
  EXPECT_NEAR(static_cast<float>(-M_PI / 4.0),
              ArcTangent2(-123.4f, 123.4f).Radians(), 1e-6);
  EXPECT_NEAR(static_cast<float>(3.0 * M_PI / 4.0),
              ArcTangent2(123.4f, -123.4f).Radians(), 1e-6);
}

TEST(AngleUtils, Cosine) {
  using ion::math::Cosine;
  using ion::math::Angled;

  // Test Angle<T> values (specialized).
  EXPECT_NEAR(1.0, Cosine(Angled::FromRadians(0.0)), 1e-10);
  EXPECT_NEAR(0.0, Cosine(Angled::FromRadians(M_PI / 2.0)), 1e-10);
  EXPECT_NEAR(0.0, Cosine(Angled::FromRadians(-M_PI / 2.0)), 1e-10);
  EXPECT_NEAR(-1.0, Cosine(Angled::FromRadians(M_PI)), 1e-10);
  EXPECT_NEAR(-1.0, Cosine(Angled::FromRadians(-M_PI)), 1e-10);
}

TEST(AngleUtils, Sine) {
  using ion::math::Sine;
  using ion::math::Angled;

  // Test Angle<T> values (specialized).
  EXPECT_NEAR(0.0, Sine(Angled::FromRadians(0.0)), 1e-10);
  EXPECT_NEAR(1.0, Sine(Angled::FromRadians(M_PI / 2.0)), 1e-10);
  EXPECT_NEAR(-1.0, Sine(Angled::FromRadians(-M_PI / 2.0)), 1e-10);
  EXPECT_NEAR(0.0, Sine(Angled::FromRadians(M_PI)), 1e-10);
  EXPECT_NEAR(0.0, Sine(Angled::FromRadians(-M_PI)), 1e-10);
}

TEST(AngleUtils, Tangent) {
  using ion::math::Tangent;
  using ion::math::Angled;

  // Test Angle<T> values (specialized).
  EXPECT_NEAR(0.0, Tangent(Angled::FromRadians(0.0)), 1e-10);
  EXPECT_NEAR(1.0, Tangent(Angled::FromRadians(M_PI / 4.0)), 1e-10);
  EXPECT_NEAR(-1.0, Tangent(Angled::FromRadians(-M_PI / 4.0)), 1e-10);
  EXPECT_NEAR(0.0, Tangent(Angled::FromRadians(M_PI)), 1e-10);
  EXPECT_NEAR(0.0, Tangent(Angled::FromRadians(-M_PI)), 1e-10);
}

TEST(AngleUtils, AngleBetween) {
  using ion::math::Vector2f;
  using ion::math::Vector3d;

  // Test Vector3d.
#if !ION_PRODUCTION
  {
    EXPECT_DEATH_IF_SUPPORTED(AngleBetween(Vector3d::AxisX(), Vector3d::Zero()),
                              "must have unit length");
    EXPECT_DEATH_IF_SUPPORTED(AngleBetween(Vector3d::Zero(), Vector3d::AxisX()),
                              "must have unit length");
  }
#endif
  EXPECT_NEAR(0.0,
              AngleBetween(Vector3d::AxisX(),
                           Vector3d::AxisX()).Degrees(),
              std::numeric_limits<double>::epsilon() * 100);
  EXPECT_NEAR(45.0,
              AngleBetween(Vector3d(0.5 * M_SQRT2, 0.5 * M_SQRT2, 0.0),
                           Vector3d::AxisY()).Degrees(),
              std::numeric_limits<double>::epsilon() * 100);
  EXPECT_NEAR(90.0,
              AngleBetween(Vector3d::AxisX(),
                           Vector3d::AxisY()).Degrees(),
              std::numeric_limits<double>::epsilon() * 100);

  // Test Vector2f.
#if !ION_PRODUCTION
  {
    EXPECT_DEATH_IF_SUPPORTED(
        AngleBetween(Vector2f::AxisX(), Vector2f(1.0f, 3.0f)),
        "must have unit length");
    EXPECT_DEATH_IF_SUPPORTED(
        AngleBetween(Vector2f(1.0f, 3.0f), Vector2f::AxisX()),
        "must have unit length");
  }
#endif
  static const float kHalfSqrt2f = static_cast<float>(M_SQRT2 * 0.5);
  EXPECT_NEAR(0.0f,
              AngleBetween(Vector2f::AxisX(),
                           Vector2f::AxisX()).Degrees(),
              std::numeric_limits<float>::epsilon() * 100);
  EXPECT_NEAR(45.0f,
              AngleBetween(Vector2f(kHalfSqrt2f, kHalfSqrt2f),
                           Vector2f::AxisY()).Degrees(),
              std::numeric_limits<float>::epsilon() * 100);
  EXPECT_NEAR(90.0f,
              AngleBetween(Vector2f::AxisX(),
                           Vector2f::AxisY()).Degrees(),
              std::numeric_limits<float>::epsilon() * 100);
}

TEST(AngleUtils, WrapTwoPi) {
  using ion::math::WrapTwoPi;
  using ion::math::Angled;
  using ion::math::Anglef;

  // Test double values.
  EXPECT_NEAR(0.0, WrapTwoPi(Angled::FromRadians(2 * M_PI)).Radians(), 1e-10);
  EXPECT_NEAR(M_PI, WrapTwoPi(Angled::FromRadians(3 * M_PI)).Radians(), 1e-10);
  EXPECT_NEAR(M_PI, WrapTwoPi(Angled::FromRadians(-1 * M_PI)).Radians(), 1e-10);
  EXPECT_NEAR(0.0, WrapTwoPi(Angled::FromRadians(-2 * M_PI)).Radians(), 1e-10);
  EXPECT_NEAR(1.2373 * M_PI,
              WrapTwoPi(Angled::FromRadians(13.2373 * M_PI)).Radians(),
              1e-10);
  EXPECT_NEAR(1.877 * M_PI,
              WrapTwoPi(Angled::FromRadians(-12.123 * M_PI)).Radians(),
              1e-10);

  // Test float values.
  static const float kPif = static_cast<float>(M_PI);
  EXPECT_NEAR(0.0f, WrapTwoPi(Anglef::FromRadians(2 * kPif)).Radians(), 1e-6);
  EXPECT_NEAR(kPif, WrapTwoPi(Anglef::FromRadians(3 * kPif)).Radians(), 1e-6);
  EXPECT_NEAR(kPif, WrapTwoPi(Anglef::FromRadians(-1 * kPif)).Radians(), 1e-6);
  EXPECT_NEAR(0.0f, WrapTwoPi(Anglef::FromRadians(-2 * kPif)).Radians(), 1e-6);
  // Only 7 decimal digits of precision available for IEEE-754 floats, 5 after
  // decimal point for numbers greater than 10.
  EXPECT_NEAR(1.2373f * kPif,
              WrapTwoPi(Angled::FromRadians(13.2373f * kPif)).Radians(),
              1e-5);
  EXPECT_NEAR(1.877f * kPif,
              WrapTwoPi(Angled::FromRadians(-12.123f * kPif)).Radians(),
              1e-5);
}

TEST(AngleUtils, AngleLerp) {
  using ion::math::AngleLerp;
  using ion::math::Angled;
  using ion::math::Anglef;

  // Test double values.
  EXPECT_NEAR(1.5 * M_PI,
              AngleLerp(Angled::FromRadians(6 * M_PI),
                        Angled::FromRadians(3 * M_PI), -0.5).Radians(),
              1e-10);
  EXPECT_NEAR(0.0,
              AngleLerp(Angled::FromRadians(6 * M_PI),
                        Angled::FromRadians(3 * M_PI), 0.0).Radians(),
              1e-10);
  EXPECT_NEAR(0.5 * M_PI,
              AngleLerp(Angled::FromRadians(6 * M_PI),
                        Angled::FromRadians(3 * M_PI), 0.5).Radians(),
              1e-10);
  EXPECT_NEAR(M_PI,
              AngleLerp(Angled::FromRadians(6 * M_PI),
                        Angled::FromRadians(3 * M_PI), 1.0).Radians(),
              1e-10);
  EXPECT_NEAR(1.5 * M_PI,
              AngleLerp(Angled::FromRadians(6 * M_PI),
                        Angled::FromRadians(3 * M_PI), 1.5).Radians(),
              1e-10);
  EXPECT_NEAR(1.95 * M_PI,
              AngleLerp(Angled::FromRadians(0.1 * M_PI),
                        Angled::FromRadians(1.9 * M_PI), 0.75).Radians(),
              1e-10);
  EXPECT_NEAR(1.85842 * M_PI,
              AngleLerp(Angled::FromRadians(-9.4325 * M_PI),
                        Angled::FromRadians(-14.1789 * M_PI), 0.95).Radians(),
              1e-10);


  // Test float values.
  static const float kPif = static_cast<float>(M_PI);
  EXPECT_NEAR(1.5f * kPif,
              AngleLerp(Angled::FromRadians(6 * kPif),
                        Angled::FromRadians(3 * kPif), -0.5f).Radians(),
              1e-6);
  EXPECT_NEAR(0.0f,
              AngleLerp(Angled::FromRadians(6 * kPif),
                        Angled::FromRadians(3 * kPif), 0.0f).Radians(),
              1e-6);
  EXPECT_NEAR(0.5f * kPif,
              AngleLerp(Angled::FromRadians(6 * kPif),
                        Angled::FromRadians(3 * kPif), 0.5f).Radians(),
              1e-6);
  EXPECT_NEAR(kPif,
              AngleLerp(Angled::FromRadians(6 * kPif),
                        Angled::FromRadians(3 * kPif), 1.0f).Radians(),
              1e-6);
  EXPECT_NEAR(1.5f * kPif,
              AngleLerp(Angled::FromRadians(6 * kPif),
                        Angled::FromRadians(3 * kPif), 1.5f).Radians(),
              1e-6);
  EXPECT_NEAR(1.95f * kPif,
              AngleLerp(Angled::FromRadians(0.1f * kPif),
                        Angled::FromRadians(1.9f * kPif), 0.75f).Radians(),
              1e-6);
  // Only 7 decimal digits of precision available for IEEE-754 floats, 5 after
  // decimal point for numbers greater than 10.
  EXPECT_NEAR(1.85842f * M_PI,
              AngleLerp(Angled::FromRadians(-9.4325f * kPif),
                        Angled::FromRadians(-14.1789f * kPif), 0.95f).Radians(),
              1e-5);
}
