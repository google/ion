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

#include "ion/math/fieldofview.h"
#include "ion/math/matrixutils.h"
#include "ion/math/tests/testutils.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

class FieldOfViewTest : public ::testing::Test {
 protected:
  void TestOpticalCenter(
      Angled left, Angled right, Angled bottom, Angled top,
      const Point2d& optical_center_ndc) {
    const double kTol = 1e-8;

    // Test conversion from angles to optical center.
    const FieldOfView<double> fov_from_angles(left, right, bottom, top);
    EXPECT_TRUE(PointsAlmostEqual(optical_center_ndc,
                                  fov_from_angles.GetOpticalCenter(),
                                  DBL_EPSILON));

    // Test creation of fov from total fov + optical center.
    FieldOfView<double> from_total_fov;
    EXPECT_TRUE(from_total_fov.SetFromTotalFovAndOpticalCenter(
        left + right, bottom + top, optical_center_ndc));
    EXPECT_NEAR(left.Degrees(), from_total_fov.GetLeft().Degrees(), kTol);
    EXPECT_NEAR(right.Degrees(), from_total_fov.GetRight().Degrees(), kTol);
    EXPECT_NEAR(bottom.Degrees(), from_total_fov.GetBottom().Degrees(), kTol);
    EXPECT_NEAR(top.Degrees(), from_total_fov.GetTop().Degrees(), kTol);
    EXPECT_TRUE(PointsAlmostEqual(optical_center_ndc,
                                  from_total_fov.GetOpticalCenter(),
                                  kTol));

    // Test creation of fov from centered fov + optical center.
    const FieldOfView<double> from_centered_fov =
        FieldOfView<double>::FromCenteredFovAndOpticalCenter(
            fov_from_angles.GetCenteredFovX(),
            fov_from_angles.GetCenteredFovY(), optical_center_ndc);
    EXPECT_NEAR(left.Degrees(), from_centered_fov.GetLeft().Degrees(), kTol);
    EXPECT_NEAR(right.Degrees(), from_centered_fov.GetRight().Degrees(), kTol);
    EXPECT_NEAR(bottom.Degrees(), from_centered_fov.GetBottom().Degrees(),
                kTol);
    EXPECT_NEAR(top.Degrees(), from_centered_fov.GetTop().Degrees(), kTol);
    EXPECT_TRUE(PointsAlmostEqual(optical_center_ndc,
                                  from_centered_fov.GetOpticalCenter(),
                                  kTol));
  }
};

TEST_F(FieldOfViewTest, DefaultConstructorAndSetters) {
  FieldOfViewd test_fov;
  EXPECT_EQ(0.0, test_fov.GetLeft().Radians());
  EXPECT_EQ(0.0, test_fov.GetRight().Radians());
  EXPECT_EQ(0.0, test_fov.GetBottom().Radians());
  EXPECT_EQ(0.0, test_fov.GetTop().Radians());
  test_fov.SetLeft(Angled::FromRadians(2.0));
  test_fov.SetRight(Angled::FromRadians(3.0));
  test_fov.SetBottom(Angled::FromRadians(4.0));
  test_fov.SetTop(Angled::FromRadians(5.0));
  EXPECT_EQ(2.0, test_fov.GetLeft().Radians());
  EXPECT_EQ(3.0, test_fov.GetRight().Radians());
  EXPECT_EQ(4.0, test_fov.GetBottom().Radians());
  EXPECT_EQ(5.0, test_fov.GetTop().Radians());
}

TEST_F(FieldOfViewTest, IsZero) {
  {
    const FieldOfViewd test_fov;
    EXPECT_TRUE(test_fov.IsZero());
  }
  {
    FieldOfViewd test_fov;
    test_fov.SetLeft(Angled::FromRadians(1.0));
    EXPECT_FALSE(test_fov.IsZero());
  }
  {
    FieldOfViewd test_fov;
    test_fov.SetRight(Angled::FromRadians(1.0));
    EXPECT_FALSE(test_fov.IsZero());
  }
  {
    FieldOfViewd test_fov;
    test_fov.SetBottom(Angled::FromRadians(1.0));
    EXPECT_FALSE(test_fov.IsZero());
  }
  {
    FieldOfViewd test_fov;
    test_fov.SetTop(Angled::FromRadians(1.0));
    EXPECT_FALSE(test_fov.IsZero());
  }
}

TEST_F(FieldOfViewTest, OperatorEqualAndNotEqual) {
  {
    const FieldOfViewd fov1(
        Angled::FromDegrees(10.0), Angled::FromDegrees(20.0),
        Angled::FromDegrees(30.0), Angled::FromDegrees(40.0));
    FieldOfViewd fov2(Angled::FromDegrees(10.0), Angled::FromDegrees(20.0),
                      Angled::FromDegrees(30.0), Angled::FromDegrees(40.0));
    EXPECT_TRUE(fov1 == fov2);
    EXPECT_FALSE(fov1 != fov2);
    fov2.SetRight(Angled::FromRadians(1.0));
    EXPECT_TRUE(fov1 == fov1);
    EXPECT_TRUE(fov2 == fov2);
    EXPECT_TRUE(fov1 != fov2);
    EXPECT_FALSE(fov1 == fov2);
  }
  {
    using ion::math::testing::IsAlmostEqual;
    using ::testing::Not;
    const FieldOfViewf fov1(
        Anglef::FromDegrees(10.0f), Anglef::FromDegrees(20.0f),
        Anglef::FromDegrees(30.0f), Anglef::FromDegrees(40.0f));
    FieldOfViewf fov2(Anglef::FromDegrees(10.0f), Anglef::FromDegrees(20.0f),
                      Anglef::FromDegrees(30.0f), Anglef::FromDegrees(40.0f));
    EXPECT_TRUE(fov1 == fov2);
    EXPECT_FALSE(fov1 != fov2);
    EXPECT_THAT(fov1, IsAlmostEqual(fov2, Anglef()));
    EXPECT_THAT(fov1, IsAlmostEqual(fov2, Anglef()));
    fov2.SetRight(Anglef::FromRadians(1.0f));
    EXPECT_TRUE(fov1 != fov2);
    EXPECT_FALSE(fov1 == fov2);
    EXPECT_THAT(fov1, Not(IsAlmostEqual(fov2, Anglef())));
    fov2.SetRight(Anglef::FromDegrees(21.f));
    EXPECT_THAT(fov1, IsAlmostEqual(fov2, Anglef::FromDegrees(1.5f)));
    EXPECT_THAT(fov1, IsAlmostEqual(fov2, Anglef::FromRadians(0.02f)));
    EXPECT_THAT(fov1, Not(IsAlmostEqual(fov2, Anglef::FromDegrees(0.5f))));
    EXPECT_THAT(fov1, Not(IsAlmostEqual(fov2, Anglef::FromRadians(0.015f))));
  }
}

TEST_F(FieldOfViewTest, FromTangents) {
  const float kTol = 1e-5f;
  const Anglef kLeft = Anglef::FromDegrees(10.0f);
  const Anglef kRight = Anglef::FromDegrees(20.0f);
  const Anglef kBottom = Anglef::FromDegrees(30.0f);
  const Anglef kTop = Anglef::FromDegrees(40.0f);
  const FieldOfViewf test_fov = FieldOfViewf::FromTangents(
      std::tan(-kLeft.Radians()),
      std::tan(kRight.Radians()),
      std::tan(-kBottom.Radians()),
      std::tan(kTop.Radians()));
  const FieldOfViewf test_fov2 = FieldOfViewf::FromTangents(
      Range2f(Point2f(std::tan(-kLeft.Radians()), std::tan(-kBottom.Radians())),
              Point2f(std::tan(kRight.Radians()), std::tan(kTop.Radians()))));
  EXPECT_EQ(test_fov, test_fov2);
  EXPECT_NEAR(kLeft.Degrees(), test_fov.GetLeft().Degrees(), kTol);
  EXPECT_NEAR(kRight.Degrees(), test_fov.GetRight().Degrees(), kTol);
  EXPECT_NEAR(kBottom.Degrees(), test_fov.GetBottom().Degrees(), kTol);
  EXPECT_NEAR(kTop.Degrees(), test_fov.GetTop().Degrees(), kTol);
  const Range2f tangents = test_fov.GetTangents();
  EXPECT_NEAR(std::tan(-kLeft.Radians()), tangents.GetMinPoint()[0], kTol);
  EXPECT_NEAR(std::tan(-kBottom.Radians()), tangents.GetMinPoint()[1], kTol);
  EXPECT_NEAR(std::tan(kRight.Radians()), tangents.GetMaxPoint()[0], kTol);
  EXPECT_NEAR(std::tan(kTop.Radians()), tangents.GetMaxPoint()[1], kTol);
}

TEST_F(FieldOfViewTest, FromProjectionMatrix) {
  // Ensure that we are able to correctly reconstruct a FieldOfView from a
  // projection matrix.
  const float kTol = 1e-5f;
  const Anglef kLeft = Anglef::FromDegrees(10.0f);
  const Anglef kRight = Anglef::FromDegrees(20.0f);
  const Anglef kBottom = Anglef::FromDegrees(30.0f);
  const Anglef kTop = Anglef::FromDegrees(40.0f);
  const FieldOfView<float> fov(kLeft, kRight, kBottom, kTop);

  const float kNear = 0.01f;
  const float kFar = 10.0f;
  const Matrix4f proj_mat = fov.GetProjectionMatrix(kNear, kFar);
  const FieldOfView<float> test_fov =
      FieldOfView<float>::FromProjectionMatrix(proj_mat);
  EXPECT_NEAR(kLeft.Degrees(), test_fov.GetLeft().Degrees(), kTol);
  EXPECT_NEAR(kRight.Degrees(), test_fov.GetRight().Degrees(), kTol);
  EXPECT_NEAR(kBottom.Degrees(), test_fov.GetBottom().Degrees(), kTol);
  EXPECT_NEAR(kTop.Degrees(), test_fov.GetTop().Degrees(), kTol);
}

TEST_F(FieldOfViewTest, FromInfiniteFarProjectionMatrix) {
  // Ensure that we are able to correctly reconstruct a FieldOfView from an
  // infinite projection matrix.
  const float kTol = 1e-5f;
  const Anglef kLeft = Anglef::FromDegrees(10.0f);
  const Anglef kRight = Anglef::FromDegrees(20.0f);
  const Anglef kBottom = Anglef::FromDegrees(30.0f);
  const Anglef kTop = Anglef::FromDegrees(40.0f);
  const FieldOfView<float> fov(kLeft, kRight, kBottom, kTop);

  const float kNear = 0.01f;
  const float kFarEpsilon = 0.0f;
  const Matrix4f proj_mat = fov.GetInfiniteFarProjectionMatrix(kNear,
                                                               kFarEpsilon);
  const FieldOfView<float> test_fov =
      FieldOfView<float>::FromProjectionMatrix(proj_mat);
  EXPECT_NEAR(kLeft.Degrees(), test_fov.GetLeft().Degrees(), kTol);
  EXPECT_NEAR(kRight.Degrees(), test_fov.GetRight().Degrees(), kTol);
  EXPECT_NEAR(kBottom.Degrees(), test_fov.GetBottom().Degrees(), kTol);
  EXPECT_NEAR(kTop.Degrees(), test_fov.GetTop().Degrees(), kTol);
}

// Sanity test for shorthands.
TEST_F(FieldOfViewTest, FromDegreesAndRadians) {
  const float kTol = 1e-5f;
  const Anglef kLeftDegrees = Anglef::FromDegrees(10.0f);
  const Anglef kRightDegrees = Anglef::FromDegrees(20.0f);
  const Anglef kBottomDegrees = Anglef::FromDegrees(30.0f);
  const Anglef kTopDegrees = Anglef::FromDegrees(40.0f);
  const FieldOfViewf test_fov_d = FieldOfViewf::FromDegrees(
      10.f, 20.f, 30.f, 40.f);
  EXPECT_NEAR(kLeftDegrees.Degrees(), test_fov_d.GetLeft().Degrees(), kTol);
  EXPECT_NEAR(kRightDegrees.Degrees(), test_fov_d.GetRight().Degrees(), kTol);
  EXPECT_NEAR(kBottomDegrees.Degrees(), test_fov_d.GetBottom().Degrees(), kTol);
  EXPECT_NEAR(kTopDegrees.Degrees(), test_fov_d.GetTop().Degrees(), kTol);

  const Anglef kLeftRadians = Anglef::FromRadians(0.2f);
  const Anglef kRightRadians = Anglef::FromRadians(0.3f);
  const Anglef kBottomRadians = Anglef::FromRadians(0.4f);
  const Anglef kTopRadians = Anglef::FromRadians(0.5f);
  const FieldOfViewf test_fov_r = FieldOfViewf::FromRadians(
      0.2f, 0.3f, 0.4f, 0.5f);
  EXPECT_NEAR(kLeftRadians.Radians(), test_fov_r.GetLeft().Radians(), kTol);
  EXPECT_NEAR(kRightRadians.Radians(), test_fov_r.GetRight().Radians(), kTol);
  EXPECT_NEAR(kBottomRadians.Radians(), test_fov_r.GetBottom().Radians(), kTol);
  EXPECT_NEAR(kTopRadians.Radians(), test_fov_r.GetTop().Radians(), kTol);
}

TEST_F(FieldOfViewTest, FromToTotalFovAndOpticalCenter) {
  // Optical center is...
  {
    // ...vertically centered, and horizontally outside of viewport.
    SCOPED_TRACE("FromToTotalFovAndOpticalCenter 1");
    const Angled kLeft = Angled::FromRadians(-M_PI_4);
    const Angled kRight = Angled::FromRadians(M_PI_4 + atan(0.5));
    const Angled kBottom = Angled::FromDegrees(22.5);
    const Angled kTop = Angled::FromDegrees(22.5);
    const Point2d kOpticalCenterNdc(-2.0, 0.0);
    TestOpticalCenter(kLeft, kRight, kBottom, kTop, kOpticalCenterNdc);
  }

  {
    // ...vertically at the very top, and horizontally at the very left.
    SCOPED_TRACE("FromToTotalFovAndOpticalCenter 2");
    const Angled kLeft = Angled::FromDegrees(0.0);
    const Angled kRight = Angled::FromDegrees(45.0);
    const Angled kBottom = Angled::FromDegrees(45.0);
    const Angled kTop = Angled::FromDegrees(0.0);
    const Point2d kOpticalCenterNdc(-1.0, 1.0);
    TestOpticalCenter(kLeft, kRight, kBottom, kTop, kOpticalCenterNdc);
  }

  {
    // ...vertically outside of viewport, and horizontally centered.
    SCOPED_TRACE("FromToTotalFovAndOpticalCenter 3");
    const Angled kLeft = Angled::FromDegrees(22.5);
    const Angled kRight = Angled::FromDegrees(22.5);
    const Angled kBottom = Angled::FromRadians(M_PI_4 + atan(0.5));
    const Angled kTop = Angled::FromRadians(-M_PI_4);
    const Point2d kOpticalCenterNdc(0.0, 2.0);
    TestOpticalCenter(kLeft, kRight, kBottom, kTop, kOpticalCenterNdc);
  }

  {
    // Ensure SetFromTotalFovAndOpticalCenter returns false if the input can not
    // be satisfied horizontally.
    const Angled kLeft = Angled::FromDegrees(22.5);
    const Angled kRight = Angled::FromDegrees(22.5);
    const Angled kBottom = Angled::FromDegrees(22.5);
    const Angled kTop = Angled::FromDegrees(22.5);
    FieldOfView<double> fov_from_pp;
    EXPECT_FALSE(fov_from_pp.SetFromTotalFovAndOpticalCenter(
        kLeft + kRight, kBottom + kTop, Point2d(2.0, 0.0)));
  }

  {
    // Ensure SetFromTotalFovAndOpticalCenter returns false if the input can not
    // be satisfied vertically.
    const Angled kLeft = Angled::FromDegrees(22.5);
    const Angled kRight = Angled::FromDegrees(22.5);
    const Angled kBottom = Angled::FromDegrees(22.5);
    const Angled kTop = Angled::FromDegrees(22.5);
    FieldOfView<double> fov_from_pp;
    EXPECT_FALSE(fov_from_pp.SetFromTotalFovAndOpticalCenter(
        kLeft + kRight, kBottom + kTop, Point2d(0.0, 2.0)));
  }
}

TEST_F(FieldOfViewTest, Streaming) {
  FieldOfViewd fov1(
      Angled::FromDegrees(10.0),
      Angled::FromDegrees(20.0),
      Angled::FromDegrees(30.0),
      Angled::FromDegrees(40.0));
  std::ostringstream out;
  out << fov1;
  EXPECT_EQ("FOV[10 deg, 20 deg, 30 deg, 40 deg]", out.str());

  {
    std::istringstream in("FOV[15 deg, 25 deg, 35 deg, 45 deg]");
    FieldOfViewd fov2;
    in >> fov2;
    EXPECT_EQ(Angled::FromDegrees(15.0), fov2.GetLeft());
    EXPECT_EQ(Angled::FromDegrees(25.0), fov2.GetRight());
    EXPECT_EQ(Angled::FromDegrees(35.0), fov2.GetBottom());
    EXPECT_EQ(Angled::FromDegrees(45.0), fov2.GetTop());
  }

  // Test failure cases.
  {
    std::istringstream in("OV[15 deg, 25 deg, 35 deg, 45 deg]");
    FieldOfViewd fov2;
    in >> fov2;
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetLeft());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetRight());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetBottom());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetTop());
  }

  {
    std::istringstream in("FOV[15 deg 25 deg, 35 deg, 45 deg]");
    FieldOfViewd fov2;
    in >> fov2;
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetLeft());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetRight());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetBottom());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetTop());
  }

  {
    std::istringstream in("FOV[15 deg, 25 deg 35 deg, 45 deg]");
    FieldOfViewd fov2;
    in >> fov2;
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetLeft());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetRight());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetBottom());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetTop());
  }
  {
    std::istringstream in("FOV[15 deg, 25 deg, 35 deg 45 deg]");
    FieldOfViewd fov2;
    in >> fov2;
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetLeft());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetRight());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetBottom());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetTop());
  }
  {
    std::istringstream in("FOV[15 deg, 25 deg, 35 deg, 45 deg");
    FieldOfViewd fov2;
    in >> fov2;
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetLeft());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetRight());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetBottom());
    EXPECT_EQ(Angled::FromDegrees(0.0), fov2.GetTop());
  }
}

}  // namespace math
}  // namespace ion
