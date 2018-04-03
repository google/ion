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

#include "ion/math/angle.h"

#include <sstream>
#include <string>

#include "ion/math/tests/testutils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

TEST(Angle, Construction) {
  // Default constructor gives a zero angle.
  EXPECT_EQ(0.0, Angled().Radians());
  EXPECT_EQ(12.5, Angled::FromRadians(12.5).Radians());
  EXPECT_EQ(42, Angled(Anglef::FromRadians(42)).Radians());
}

TEST(Angle, Conversion) {
  EXPECT_EQ(0.0, Angled().Degrees());
  EXPECT_NEAR(90.0, Angled::FromRadians(M_PI_2).Degrees(), 1e-10);
  EXPECT_NEAR(45.0, Angled::FromRadians(M_PI_4).Degrees(), 1e-10);
}

TEST(Angle, EqualityOperators) {
  EXPECT_TRUE(Angled() == Angled());
  EXPECT_TRUE(Angled::FromRadians(0.46) == Angled::FromRadians(0.46));
  EXPECT_FALSE(Angled::FromRadians(0.46) == Angled::FromRadians(0.45));
  EXPECT_FALSE(Angled::FromRadians(0.46) == Angled::FromRadians(-0.46));
  EXPECT_FALSE(Angled() != Angled());
  EXPECT_FALSE(Angled::FromRadians(0.46) != Angled::FromRadians(0.46));
  EXPECT_TRUE(Angled::FromRadians(0.46) != Angled::FromRadians(0.45));
  EXPECT_TRUE(Angled::FromRadians(0.46) != Angled::FromRadians(-0.46));
}

TEST(Angle, ComparisonOperators) {
  EXPECT_LE(Angled::FromRadians(0.46), Angled::FromRadians(0.46));
  EXPECT_GE(Angled::FromRadians(0.46), Angled::FromRadians(0.46));
  EXPECT_GT(Angled::FromRadians(0.46), Angled::FromRadians(0.45));
  EXPECT_GE(Angled::FromRadians(0.46), Angled::FromRadians(0.45));
  EXPECT_LT(Angled::FromRadians(0.45), Angled::FromRadians(0.46));
  EXPECT_LE(Angled::FromRadians(0.45), Angled::FromRadians(0.46));
  EXPECT_FALSE(Angled::FromRadians(0.46) < Angled::FromRadians(0.46));
  EXPECT_FALSE(Angled::FromRadians(0.46) < Angled::FromRadians(0.45));
  EXPECT_FALSE(Angled::FromRadians(0.46) <= Angled::FromRadians(0.45));
  EXPECT_FALSE(Angled::FromRadians(0.45) > Angled::FromRadians(0.46));
  EXPECT_FALSE(Angled::FromRadians(0.45) >= Angled::FromRadians(0.46));
  EXPECT_FALSE(Angled::FromRadians(0.45) > Angled::FromRadians(0.45));
}

TEST(Angle, SelfModifyingOperators) {
  Anglef a = Anglef::FromRadians(2.0f);
  EXPECT_EQ(2.0f, a.Radians());
  a += Anglef::FromRadians(1.0f);
  EXPECT_EQ(3.0f, a.Radians());
  a -= Anglef::FromRadians(2.0f);
  EXPECT_EQ(1.0f, a.Radians());
  a *= 6.0f;
  EXPECT_EQ(6.0f, a.Radians());
  a /= 3.0f;
  EXPECT_EQ(2.0f, a.Radians());
}

TEST(Angle, Negation) {
  EXPECT_EQ(Angled(), -Angled());
  EXPECT_EQ(Angled::FromRadians(-0.46), -Angled::FromRadians(0.46));
  EXPECT_EQ(Angled::FromRadians(0.46), -Angled::FromRadians(-0.46));
}

TEST(Angle, BinaryOperators) {
  Angled a0 = Angled::FromRadians(2.0);
  Angled a1 = Angled::FromRadians(4.0);
  EXPECT_EQ(6.0, (a0 + a1).Radians());
  EXPECT_EQ(-2.0, (a0 - a1).Radians());
  EXPECT_EQ(10.0, (a0 * 5.0).Radians());
  EXPECT_EQ(16.0, (4.0 * a1).Radians());
  EXPECT_EQ(2.0, (a1 / 2.0).Radians());
}

TEST(Angle, Streaming) {
  std::ostringstream out1;
  out1 << Angled::FromDegrees(23.5);
  EXPECT_EQ(std::string("23.5 deg"), out1.str());
  std::ostringstream out2;
  out2 << Angled::FromRadians(M_PI_2);
  EXPECT_EQ(std::string("90 deg"), out2.str());

  std::istringstream in("41.2 deg");
  Angled a;
  in >> a;
  EXPECT_NEAR(41.2, a.Degrees(), 1e-8);

  in.clear();
  in.str("bad");
  in >> a;
  EXPECT_NEAR(41.2, a.Degrees(), 1e-8);

  in.clear();
  in.str("12,34");
  in >> a;
  EXPECT_NEAR(41.2, a.Degrees(), 1e-8);

  in.clear();
  in.str("12.34");
  in >> a;
  EXPECT_NEAR(41.2, a.Degrees(), 1e-8);

  in.clear();
  in.str("12.34 deg");
  in >> a;
  EXPECT_NEAR(12.34, a.Degrees(), 1e-8);

  in.clear();
  in.str("2 rad");
  in >> a;
  EXPECT_EQ(2.0, a.Radians());
}

TEST(Angle, AnglesAlmostEqual) {
  using ::testing::Not;
  using ion::math::testing::IsAlmostEqual;

  EXPECT_THAT(Anglef::FromDegrees(17.f),
              IsAlmostEqual(Anglef::FromDegrees(17.f), Anglef()));
  EXPECT_THAT(
      Anglef::FromDegrees(17.f),
      IsAlmostEqual(Anglef::FromDegrees(17.1f), Anglef::FromDegrees(0.2f)));
  EXPECT_THAT(Anglef::FromDegrees(17.f),
              Not(IsAlmostEqual(Anglef::FromDegrees(17.1f),
                                Anglef::FromDegrees(0.05f))));
  EXPECT_THAT(
      Anglef::FromDegrees(17.f),
      IsAlmostEqual(Anglef::FromDegrees(16.9f), Anglef::FromDegrees(0.2f)));
  EXPECT_THAT(Anglef::FromDegrees(17.f),
              Not(IsAlmostEqual(Anglef::FromDegrees(16.9f),
                                Anglef::FromDegrees(0.05f))));
  EXPECT_THAT(
      Angled::FromRadians(2.0),
      IsAlmostEqual(Angled::FromRadians(2.01), Angled::FromRadians(0.015)));
  EXPECT_THAT(Angled::FromRadians(2.0),
              Not(IsAlmostEqual(Angled::FromRadians(2.01),
                                Angled::FromRadians(0.005))));
  EXPECT_THAT(
      Angled::FromRadians(2.0),
      IsAlmostEqual(Angled::FromRadians(1.99), Angled::FromRadians(0.015)));
  EXPECT_THAT(Angled::FromRadians(2.0),
              Not(IsAlmostEqual(Angled::FromRadians(1.99),
                                Angled::FromRadians(0.005))));

  // Test boundary conditions for positive angles that are close to 360 degrees
  // away from each other.
  EXPECT_THAT(
      Anglef::FromDegrees(0.1f),
      IsAlmostEqual(Anglef::FromDegrees(359.9f), Anglef::FromDegrees(0.3f)));
  EXPECT_THAT(Anglef::FromDegrees(0.1f),
              Not(IsAlmostEqual(Anglef::FromDegrees(359.9f),
                                Anglef::FromDegrees(0.1f))));
  EXPECT_THAT(
      Angled::FromDegrees(90.0),
      IsAlmostEqual(Angled::FromDegrees(450.0), Angled::FromDegrees(5.0)));
  EXPECT_THAT(
      Angled::FromDegrees(90.0),
      IsAlmostEqual(Angled::FromDegrees(455.0), Angled::FromDegrees(5.0)));
  EXPECT_THAT(
      Angled::FromDegrees(90.0),
      IsAlmostEqual(Angled::FromDegrees(445.0), Angled::FromDegrees(5.0)));
  EXPECT_THAT(Angled::FromDegrees(90),
              Not(IsAlmostEqual(Angled::FromDegrees(455.001),
                                Angled::FromDegrees(5.0))));
  EXPECT_THAT(Angled::FromDegrees(90),
              Not(IsAlmostEqual(Angled::FromDegrees(444.999),
                                Angled::FromDegrees(5.0))));

  // Test boundary conditions for angles near the 180/-180 degree boundary.
  EXPECT_THAT(
      Angled::FromDegrees(179.0),
      IsAlmostEqual(Angled::FromDegrees(-179.0), Angled::FromDegrees(5.0)));
  EXPECT_THAT(
      Angled::FromDegrees(-179.0),
      IsAlmostEqual(Angled::FromDegrees(179.0), Angled::FromDegrees(5.0)));
  EXPECT_THAT(
      Angled::FromDegrees(177.5),
      IsAlmostEqual(Angled::FromDegrees(-177.5), Angled::FromDegrees(5.0)));
  EXPECT_THAT(
      Angled::FromDegrees(-177.5),
      IsAlmostEqual(Angled::FromDegrees(177.5), Angled::FromDegrees(5.0)));
  EXPECT_THAT(Angled::FromDegrees(177.49),
              Not(IsAlmostEqual(Angled::FromDegrees(-177.5),
                                Angled::FromDegrees(5.0))));
  EXPECT_THAT(
      Angled::FromDegrees(-177.49),
      Not(IsAlmostEqual(Angled::FromDegrees(177.5), Angled::FromDegrees(5.0))));
}

}  // namespace math
}  // namespace ion
