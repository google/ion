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

#include "ion/math/rotation.h"

#include <sstream>
#include <string>

#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/matrixutils.h"
#include "ion/math/tests/testutils.h"
#include "ion/math/transformutils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

namespace {

// Returns the angle between two 3D vectors.
template <typename T>
static const Angle<T> AngleBetweenVectors(const Vector<3, T>& v0,
                                          const Vector<3, T>& v1) {
  return Angled::FromRadians(acos(Dot(v0, v1)));
}

// Handy function for testing Rotation vs. axis/angle within tolerance.
template <typename T>
static bool RotationCloseToAxisAngle(const Vector<3, T>& expected_axis,
                                     const Angle<T>& expected_angle,
                                     const Rotation<T>& r) {
  Vector<3, T> ret_axis;
  Angle<T> ret_angle;
  r.GetAxisAndAngle(&ret_axis, &ret_angle);

  static const T kAngleTolerance = static_cast<T>(1e-10);
  return testing::VectorsAlmostEqual(ret_axis, expected_axis) &&
      Abs(expected_angle.Radians() - ret_angle.Radians()) <= kAngleTolerance;
}

// Helper function for testing axis/angle rotations.
static void TestAxisAngle(const Vector3d& axis, const Angled& angle) {
  Rotationd r;
  r.SetAxisAndAngle(axis, angle);

  Vector3d expected_axis = Normalized(axis);
  EXPECT_PRED3(RotationCloseToAxisAngle<double>, expected_axis, angle, r);

  // Also test the static convenience function.
  EXPECT_EQ(r, Rotationd::FromAxisAndAngle(axis, angle));
}

}  // anonymous namespace

TEST(Rotation, Constructor) {
  // The default constructor should be an identity rotation.
  Rotationd r;
  const Vector4d qr = r.GetQuaternion();
  const Vector4d qi = Rotationd::Identity().GetQuaternion();
  EXPECT_EQ(qi[0], qr[0]);
  EXPECT_EQ(qi[1], qr[1]);
  EXPECT_EQ(qi[2], qr[2]);
  EXPECT_EQ(qi[3], qr[3]);
}

TEST(Rotation, TypeConvertingConstructor) {
  // Test conversion from double to float.
  {
    const Rotationd rotd = Rotationd::FromAxisAndAngle(
        Vector3d(1.0, 0.0, 0.0), Angled::FromDegrees(30.0));
    const Rotationf rotf(rotd);
    EXPECT_PRED2((testing::VectorsAlmostEqual<4, float>),
                 rotf.GetQuaternion(), Vector4f(rotd.GetQuaternion()));
  }

  // Test conversion from float to double.
  {
    const Rotationf rotf = Rotationf::FromAxisAndAngle(
        Vector3f(1.0f, 0.0f, 0.0f), Anglef::FromDegrees(30.0f));
    const Rotationd rotd(rotf);
    EXPECT_PRED2((testing::VectorsAlmostEqual<4, float>),
                 Vector4f(rotd.GetQuaternion()), rotf.GetQuaternion());
  }
}

TEST(Rotation, SetQuaternion) {
  const Vector4d unnormalized(1.0, 2.0, -3.0, 4.0);
  const Vector4d normalized = Normalized(unnormalized);

  Rotationd r;
  r.SetQuaternion(normalized);
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               normalized, r.GetQuaternion());

  r.SetQuaternion(unnormalized);
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               normalized, r.GetQuaternion());
}

TEST(Rotation, EqualityOperators) {
  EXPECT_TRUE(Rotationd() == Rotationd());
  EXPECT_TRUE(Rotationd()
              == Rotationd::FromQuaternion(-Rotationd().GetQuaternion()));
  EXPECT_FALSE(Rotationd()
               != Rotationd::FromQuaternion(-Rotationd().GetQuaternion()));
  EXPECT_TRUE(Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                          Angled::FromRadians(0.42)) ==
              Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                          Angled::FromRadians(0.42)));
  EXPECT_FALSE(Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                           Angled::FromRadians(0.42)) ==
               Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                           Angled::FromRadians(0.43)));
  EXPECT_FALSE(Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                           Angled::FromRadians(0.42)) ==
               Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                           Angled::FromRadians(-0.42)));
  EXPECT_FALSE(Rotationd() != Rotationd());
  EXPECT_FALSE(Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                           Angled::FromRadians(0.42)) !=
               Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                           Angled::FromRadians(0.42)));
  EXPECT_TRUE(Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                          Angled::FromRadians(0.42)) !=
              Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                          Angled::FromRadians(0.43)));
  EXPECT_TRUE(Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                          Angled::FromRadians(0.42)) !=
              Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                          Angled::FromRadians(-0.42)));
}

TEST(Rotation, AxisAngle) {
  TestAxisAngle(Vector3d(1.0, 0.0, 0.0), Angled::FromDegrees(45.0));
  TestAxisAngle(Vector3d(0.0, 2.0, 0.0), Angled::FromDegrees(30.0));
  TestAxisAngle(Vector3d(0.0, 0.0, -1.0), Angled::FromDegrees(110.0));
  TestAxisAngle(Vector3d(1.0, -2.0, 3.0), Angled::FromDegrees(170.0));

  // Zero vector should result in identity.
  Rotationd r = Rotationd::FromAxisAndAngle(Vector3d(0.0, 0.0, 0.0),
                                            Angled::FromDegrees(45.0));
  Vector3d axis;
  Angled angle;
  r.GetAxisAndAngle(&axis, &angle);
  EXPECT_EQ(Vector3d(1.0, 0.0, 0.0), axis);
  EXPECT_EQ(0.0, angle.Radians());
}

TEST(Rotation, EulerAnglesGetRollPitchYaw) {
  // Create a composition of rotations in the order:
  // - 0.3 radians in Z
  // - 0.2 radians in X
  // - 0.1 radians in Y
  Rotationd rotation_yaw = Rotationd::FromAxisAndAngle(
      Vector3d(0, 1, 0), Angled::FromRadians(.1));
  Rotationd rotation_pitch = Rotationd::FromAxisAndAngle(
      Vector3d(1, 0, 0), Angled::FromRadians(.2));
  Rotationd rotation_roll = Rotationd::FromAxisAndAngle(
      Vector3d(0, 0, 1), Angled::FromRadians(.3));
  Rotationd rotation_final = rotation_roll * (rotation_pitch * rotation_yaw);

  // Check that the quaternion matches our expectation.
  const Vector4d check_quaternion(
      0.091157549342990724,
      0.064071347706071174,
      0.1534393020242226,
      0.98185617286608096);
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
      rotation_final.GetQuaternion(), check_quaternion);

  // Check GetRollPitchYaw() and GetEulerAngles() produce the correct component
  // angles.
  static const double kTolerance = 1e-8;
  Angled yaw, pitch, roll;
  rotation_final.GetRollPitchYaw(&roll, &pitch, &yaw);
  EXPECT_NEAR(0.1, yaw.Radians(), kTolerance);
  EXPECT_NEAR(0.2, pitch.Radians(), kTolerance);
  EXPECT_NEAR(0.3, roll.Radians(), kTolerance);
  rotation_final.GetEulerAngles(&yaw, &pitch, &roll);
  EXPECT_NEAR(0.1, yaw.Radians(), kTolerance);
  EXPECT_NEAR(0.2, pitch.Radians(), kTolerance);
  EXPECT_NEAR(0.3, roll.Radians(), kTolerance);
}

TEST(Rotation, EulerAnglesGetYawPitchRoll) {
  // Create a composition of rotations in the order:
  // - 0.1 radians in Y
  // - 0.2 radians in X
  // - 0.3 radians in Z
  Rotationd rotation_yaw =
      Rotationd::FromAxisAndAngle(Vector3d(0, 1, 0), Angled::FromRadians(.1));
  Rotationd rotation_pitch =
      Rotationd::FromAxisAndAngle(Vector3d(1, 0, 0), Angled::FromRadians(.2));
  Rotationd rotation_roll =
      Rotationd::FromAxisAndAngle(Vector3d(0, 0, 1), Angled::FromRadians(.3));
  Rotationd rotation_final = rotation_yaw * (rotation_pitch * rotation_roll);

  // Check GetYawPitchRoll() produces the correct component angles.
  static const double kTolerance = 1e-8;
  Angled yaw, pitch, roll;
  rotation_final.GetYawPitchRoll(&yaw, &pitch, &roll);
  EXPECT_NEAR(0.1, yaw.Radians(), kTolerance);
  EXPECT_NEAR(0.2, pitch.Radians(), kTolerance);
  EXPECT_NEAR(0.3, roll.Radians(), kTolerance);
}

TEST(Rotation, EulerAnglesFromRollPitchYaw) {
  // Use FromRollPitchYaw to create a composition of rotations in the order:
  // - 0.3 radians in Z
  // - 0.2 radians in X
  // - 0.1 radians in Y
  Rotationd rotation = Rotationd::FromRollPitchYaw(Angled::FromRadians(0.3),
                                                   Angled::FromRadians(0.2),
                                                   Angled::FromRadians(0.1));

  // Check GetRollPitchYaw() and GetEulerAngles() produce the correct component
  // angles.
  static const double kTolerance = 1e-8;
  Angled yaw, pitch, roll;
  rotation.GetRollPitchYaw(&roll, &pitch, &yaw);
  EXPECT_NEAR(0.1, yaw.Radians(), kTolerance);
  EXPECT_NEAR(0.2, pitch.Radians(), kTolerance);
  EXPECT_NEAR(0.3, roll.Radians(), kTolerance);
  rotation.GetEulerAngles(&yaw, &pitch, &roll);
  EXPECT_NEAR(0.1, yaw.Radians(), kTolerance);
  EXPECT_NEAR(0.2, pitch.Radians(), kTolerance);
  EXPECT_NEAR(0.3, roll.Radians(), kTolerance);

  // Check that FromEulerAngles() produces the same rotation.
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               rotation.GetQuaternion(),
               Rotationd::FromEulerAngles(Angled::FromRadians(0.1),
                                          Angled::FromRadians(0.2),
                                          Angled::FromRadians(0.3))
                   .GetQuaternion());
}

TEST(Rotation, EulerAnglesFromYawPitchRoll) {
  // Create a composition of rotations in the order:
  // - 0.1 radians in Y
  // - 0.2 radians in X
  // - 0.3 radians in Z
  Rotationd rotation = Rotationd::FromYawPitchRoll(Angled::FromRadians(0.1),
                                                   Angled::FromRadians(0.2),
                                                   Angled::FromRadians(0.3));

  // Check GetYawPitchRoll() produces the correct component angles.
  static const double kTolerance = 1e-8;
  Angled yaw, pitch, roll;
  rotation.GetYawPitchRoll(&yaw, &pitch, &roll);
  EXPECT_NEAR(0.1, yaw.Radians(), kTolerance);
  EXPECT_NEAR(0.2, pitch.Radians(), kTolerance);
  EXPECT_NEAR(0.3, roll.Radians(), kTolerance);
}

TEST(Rotation, EulerAnglesStraightUp) {
  // Create a rotation composed of 0.4 radians in Y and pi/2 radians in X,
  // which is straight up.
  Rotationd rotation_yaw = Rotationd::FromAxisAndAngle(
      Vector3d(0, 1, 0), Angled::FromRadians(.4));
  Rotationd rotation_pitch = Rotationd::FromAxisAndAngle(
      Vector3d(1, 0, 0), Angled::FromRadians(M_PI_2));
  Rotationd rotation_final = rotation_pitch * rotation_yaw;

  // Check that the quaternion matches our expectation.
  const Vector4d check_quaternion(
      0.69301172320583526,
      0.14048043101898119,
      0.14048043101898117,
      0.69301172320583537);
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
      rotation_final.GetQuaternion(), check_quaternion);

  // Check component angles.
  static const double kTolerance = 1e-8;
  Angled yaw, pitch, roll;
  rotation_final.GetRollPitchYaw(&roll, &pitch, &yaw);
  EXPECT_NEAR(0.4, yaw.Radians(), kTolerance);
  EXPECT_NEAR(M_PI_2, pitch.Radians(), kTolerance);
  EXPECT_NEAR(0., roll.Radians(), kTolerance);
}

TEST(Rotation, EulerAnglesStraightDown) {
  // Create a rotation composed of 0.5 radians in Y and -pi/2 radians in X,
  // which is straight up.
  Rotationd rotation_yaw = Rotationd::FromAxisAndAngle(
      Vector3d(0, 1, 0), Angled::FromRadians(.5));
  Rotationd rotation_pitch = Rotationd::FromAxisAndAngle(
      Vector3d(1, 0, 0), Angled::FromRadians(-M_PI_2));
  Rotationd rotation_final = rotation_pitch * rotation_yaw;

  // Check that the quaternion matches our expectation.
  const Vector4d check_quaternion(
      -0.68512454376747678,
      0.17494101728127351,
      -0.17494101728127348,
      0.68512454376747689);
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
      rotation_final.GetQuaternion(), check_quaternion);

  // Check component angles.
  static const double kTolerance = 1e-8;
  Angled yaw, pitch, roll;
  rotation_final.GetRollPitchYaw(&roll, &pitch, &yaw);
  EXPECT_NEAR(0.5, yaw.Radians(), kTolerance);
  EXPECT_NEAR(-M_PI_2, pitch.Radians(), kTolerance);
  EXPECT_NEAR(0., roll.Radians(), kTolerance);
}

TEST(Rotation, RotateInto) {
  Rotationd r;

  // Rotate X axis into Y: should be 90 degrees around Z.
  r = Rotationd::RotateInto(Vector3d(1.0, 0.0, 0.0), Vector3d(0.0, 1.0, 0.0));
  EXPECT_PRED3(RotationCloseToAxisAngle<double>,
               Vector3d(0.0, 0.0, 1.0), Angled::FromDegrees(90.0), r);

  // Do the opposite: should use -Z.
  r = Rotationd::RotateInto(Vector3d(0.0, 1.0, 0.0), Vector3d(1.0, 0.0, 0.0));
  EXPECT_PRED3(RotationCloseToAxisAngle<double>,
               Vector3d(0.0, 0.0, -1.0), Angled::FromDegrees(90.0), r);

  // Arbitrary vectors.
  {
    const Vector3d v_from = Normalized(Vector3d(2.0, 1.0, 3.0));
    const Vector3d v_to = Normalized(Vector3d(1.0, 2.0, 3.0));
    r = Rotationd::RotateInto(v_from, v_to);
    const Vector3d v_rot = RotationMatrixNH(r) * v_from;
    EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
                 v_to, v_rot);
  }

  // Parallel or near-parallel vectors should result in identity rotation.
  r = Rotationd::RotateInto(Vector3d(1.0, 2.0, -3.0), Vector3d(1.0, 2.0, -3.0));
  EXPECT_TRUE(r.IsIdentity());
  r = Rotationd::RotateInto(Vector3d(1.0, 2.0, -3.0),
                            Vector3d(1.000000001, 2.0, -3.0));
  EXPECT_TRUE(r.IsIdentity());

  // Antiparallel or near-antiparallel vectors should result in 180-degree
  // rotation around an axis perpendicular to both vectors.
  Vector3d v0(1.0, 2.0, -3.0);
  Vector3d v1(-1.0, -2.0, 3.0);
  r = Rotationd::RotateInto(v0, v1);
  Vector3d axis;
  Angled angle;
  r.GetAxisAndAngle(&axis, &angle);
  static const double kTolerance = 1e-8;
  EXPECT_NEAR(0.0, Dot(v0, axis), kTolerance);
  EXPECT_NEAR(0.0, Dot(v1, axis), kTolerance);
  EXPECT_NEAR(180.0, angle.Degrees(), kTolerance);
  v1[2] += 0.00000001;
  r = Rotationd::RotateInto(v0, v1);
  r.GetAxisAndAngle(&axis, &angle);
  EXPECT_NEAR(0.0, Dot(v0, axis), kTolerance);
  EXPECT_NEAR(0.0, Dot(v1, axis), kTolerance);
  EXPECT_NEAR(180.0, angle.Degrees(), kTolerance);

  // Antiparallel case where the "from" vector is the X axis, forcing a more
  // complex test to get the perpendicular axis.
  const Vector3d v2(1.0, 0.0, 0.0);
  const Vector3d v3(-1.0, 0.0, 0.0);
  r = Rotationd::RotateInto(v2, v3);
  r.GetAxisAndAngle(&axis, &angle);
  EXPECT_NEAR(180.0, angle.Degrees(), kTolerance);
  EXPECT_NEAR(0.0, Dot(axis, v2), kTolerance);
  EXPECT_NEAR(0.0, Dot(axis, v3), kTolerance);
}

TEST(Rotation, SelfModifyingOperators) {
  {
    // *= rotation with same axis.
    const Vector3d axis(-1.0, 4.0, -5.0);
    Rotationd r = Rotationd::FromAxisAndAngle(axis, Angled::FromDegrees(21.0));
    r *= Rotationd::FromAxisAndAngle(axis, Angled::FromDegrees(31.0));
    EXPECT_PRED3(RotationCloseToAxisAngle<double>,
                 Normalized(axis), Angled::FromDegrees(52.0), r);
  }

  {
    // *= rotation with different axis. Rotate a vector and check that the
    // rotations combined correctly.
    Rotationd r0 = Rotationd::FromAxisAndAngle(Vector3d(-2.0, 1.0, 3.0),
                                               Angled::FromDegrees(15.0));
    const Rotationd r1 = Rotationd::FromAxisAndAngle(Vector3d(4.0, 3.0, -2.0),
                                                     Angled::FromDegrees(33.0));
    const Vector3d v = Normalized(Vector3d(0.5, -2.1, -5.8));
    const Vector3d vr = RotationMatrixNH(r0) * RotationMatrixNH(r1) * v;

    r0 *= r1;
    const Vector3d vc = RotationMatrixNH(r0) * v;
    EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>), vr, vc);
  }
}

TEST(Rotation, Negation) {
  // Negation of identity rotation is itself.
  EXPECT_EQ(Rotationd(), -Rotationd());
  EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                        Angled::FromRadians(0.42)),
            -Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                         Angled::FromRadians(-0.42)));
  EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                        Angled::FromRadians(-0.42)),
            -Rotationd::FromAxisAndAngle(Vector3d(1.1, 2.2, 3.3),
                                         Angled::FromRadians(0.42)));
}

TEST(Rotationd, BinaryOperators) {
  const Rotationd r0 = Rotationd::FromAxisAndAngle(Vector3d(6.0, -2.0, 1.0),
                                                   Angled::FromDegrees(21.0));
  const Rotationd r1 = Rotationd::FromAxisAndAngle(Vector3d(-1.0, 4.0, 3.0),
                                                   Angled::FromDegrees(5.0));
  Rotationd rc = r0;
  rc *= r1;

  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, rc, r0 * r1);

  rc = r1;
  rc *= r0;

  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, rc, r1 * r0);
}

TEST(Rotation, Slerp) {
  const Rotationd r0 = Rotationd::FromAxisAndAngle(Vector3d(1.0, -2.0, 3.0),
                                                   Angled::FromDegrees(20));
  const Rotationd r1 = Rotationd::FromAxisAndAngle(Vector3d(-2.0, -1.0, 0.0),
                                                   Angled::FromDegrees(60));
  const Rotationd r2 = Rotationd::FromQuaternion(-r1.GetQuaternion());

  // Two rotations with opposite quaternions should be consdered to be the same.
  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, r1, r2);

  // Slerp with t = 0.
  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, r0,
               Rotationd::Slerp(r0, r1, 0.0));
  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, r0,
               Rotationd::Slerp(r0, r2, 0.0));

  // Slerp with t = 1.
  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, r1,
               Rotationd::Slerp(r0, r1, 1.0));
  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, r1,
               Rotationd::Slerp(r0, r2, 1.0));

  // Slerping with a rotation should be the same as slerping with the antipodal
  // representation of that rotation.
  EXPECT_PRED2(testing::RotationsAlmostEqual<double>,
               Rotationd::Slerp(r0, r1, 0.5), Rotationd::Slerp(r0, r2, 0.5));

  // Slerp with N values between 0 and 1. Apply the resulting rotation to a
  // vector v and check the angles between the vectors. The resulting angles
  // should all be 1/(N-1) of the full angle between the first and last rotated
  // vectors.
  static const int kNumSteps = 12;
  static const double kStepFraction = 1.0 / (kNumSteps - 1);

  // A randomly-chosen vector.
  const Vector3d v = Normalized(Vector3d(0.5, -2.1, -5.8));

  // The v vector rotated by the start and ending rotations.
  const Vector3d v_start = RotationMatrixNH(r0) * v;
  const Vector3d v_end = RotationMatrixNH(r1) * v;

  // 1/(N-1) of the angle between the start and end vectors.
  const Angled step_angle = AngleBetweenVectors(v_start, v_end) * kStepFraction;

  // Check the angle at each slerp step.
  Vector3d v0 = v_start;
  for (int i = 1; i < kNumSteps; ++i) {
    const double t = i * kStepFraction;
    const Vector3d v1 =
        Normalized(RotationMatrixNH(Rotationd::Slerp(r0, r1, t)) * v);
    const Vector3d v2 =
        Normalized(RotationMatrixNH(Rotationd::Slerp(r0, r2, t)) * v);
    // Use a relatively large tolerance here because radians are pretty small.
    static const double kTolerance = 1e-4;
    EXPECT_NEAR(step_angle.Radians(), AngleBetweenVectors(v0, v1).Radians(),
                kTolerance);
    EXPECT_NEAR(step_angle.Radians(), AngleBetweenVectors(v0, v2).Radians(),
                kTolerance);
    v0 = v1;
  }

  // A float version for coverage.
  {
    const Rotationf rf0 =
        Rotationf::FromAxisAndAngle(Vector3f::AxisX(), Anglef::FromDegrees(20));
    const Rotationf rf1 =
        Rotationf::FromAxisAndAngle(Vector3f::AxisX(), Anglef::FromDegrees(60));
    EXPECT_PRED2(testing::RotationsAlmostEqual<float>,
                 Rotationf::FromAxisAndAngle(Vector3f::AxisX(),
                                             Anglef::FromDegrees(40)),
                 Rotationf::Slerp(rf0, rf1, .5f));
  }
}

TEST(Rotation, Lerp) {
  // Interpolate between two very similar rotations. This should use lerp
  // instead of slerp.
  const Vector3d axis(1.0, -2.0, 3.0);
  const Rotationd r0 = Rotationd::FromAxisAndAngle(axis,
                                                   Angled::FromDegrees(20));
  const Rotationd r1 = Rotationd::FromAxisAndAngle(axis,
                                                   Angled::FromDegrees(20.01));

  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, r0,
               Rotationd::Slerp(r0, r1, 0.0));
  EXPECT_PRED2(testing::RotationsAlmostEqual<double>, r1,
               Rotationd::Slerp(r0, r1, 1.0));

  // Interpolate halfway. The resulting rotation should have the same axis and
  // half the angle.
  const Rotationd rh = Rotationd::Slerp(r0, r1, 0.5);
  EXPECT_PRED3(RotationCloseToAxisAngle<double>,
               Normalized(axis), Angled::FromDegrees(20.005), rh);
}

TEST(Rotation, Streaming) {
  std::ostringstream out;
  out << Rotationd::FromAxisAndAngle(Vector3d(1.0, 0.0, 0.0),
                                     Angled::FromDegrees(45.0));
  EXPECT_EQ(std::string("ROT[V[1, 0, 0]: 45 deg]"), out.str());
  {
    std::istringstream in("ROT[V[0, 1, 0]: 45 deg]");
    Rotationd r;
    in >> r;
    EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(0.0, 1.0, 0.0),
                                          Angled::FromDegrees(45.0)), r);
  }
  {
    std::istringstream in("ROT[V[0, 1, 0]: 45 deg");
    Rotationd r;
    in >> r;
    EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(1.0, 0.0, 0.0),
                                          Angled::FromDegrees(0.)), r);
  }
  {
    std::istringstream in("ROT[V[0, 1, 0] 45 deg]");
    Rotationd r;
    in >> r;
    EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(1.0, 0.0, 0.0),
                                          Angled::FromDegrees(0.)), r);
  }
  {
    std::istringstream in("ROT[V[0, 1, 0]: 45]");
    Rotationd r;
    in >> r;
    EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(1.0, 0.0, 0.0),
                                          Angled::FromDegrees(0.)), r);
  }
  {
    std::istringstream in("ROT[V[0, 1 0]: 45 deg]");
    Rotationd r;
    in >> r;
    EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(1.0, 0.0, 0.0),
                                          Angled::FromDegrees(0.)), r);
  }
  {
    std::istringstream in("ROt[V[0, 1, 0]: 45 deg]");
    Rotationd r;
    in >> r;
    EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(1.0, 0.0, 0.0),
                                          Angled::FromDegrees(0.)), r);
  }
  {
    std::istringstream in("ROT[ V[0, 1, 0]: 45 deg]");
    Rotationd r;
    in >> r;
    EXPECT_EQ(Rotationd::FromAxisAndAngle(Vector3d(0.0, 1.0, 0.0),
                                          Angled::FromDegrees(45.)), r);
  }
}

TEST(Rotation, FromRotationMatrix) {
  const Matrix3d identity_mat = Matrix3d::Identity();
  const Rotationd converted_identity =
      Rotationd::FromRotationMatrix(identity_mat);
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               Rotationd().GetQuaternion(), converted_identity.GetQuaternion());

  // The following matrix represents a rotation by 90 degrees around x.
  const Matrix3d x90_mat(1, 0, 0,
                         0, 0, -1,
                         0, 1, 0);
  const Rotationd x90_converted = Rotationd::FromRotationMatrix(x90_mat);
  const auto x90 = Rotationd::FromAxisAndAngle(Vector3d(1.0, 0.0, 0.0),
                                              Angled::FromRadians(M_PI / 2));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               x90.GetQuaternion(), x90_converted.GetQuaternion());


  // The following matrix represents a rotation by 180 degrees around x.
  const Matrix3d x180_mat(1, 0, 0,
                          0, -1, 0,
                          0, 0, -1);
  const Rotationd x180_converted = Rotationd::FromRotationMatrix(x180_mat);
  const auto x180 = Rotationd::FromAxisAndAngle(Vector3d(1.0, 0.0, 0.0),
                                                Angled::FromRadians(M_PI));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               x180.GetQuaternion(), x180_converted.GetQuaternion());

  // The following matrix represents a rotation by 180 degrees around y.
  const Matrix3d y180_mat(-1, 0, 0,
                          0, 1, 0,
                          0, 0, -1);
  const Rotationd y180_converted = Rotationd::FromRotationMatrix(y180_mat);
  const auto y180 = Rotationd::FromAxisAndAngle(Vector3d(0.0, 1.0, 0.0),
                                                Angled::FromRadians(M_PI));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               y180.GetQuaternion(), y180_converted.GetQuaternion());

  // The following matrix represents a rotation by 180 degrees around z.
  const Matrix3d z180_mat(-1, 0, 0,
                          0, -1, 0,
                          0, 0, 1);
  const Rotationd z180_converted = Rotationd::FromRotationMatrix(z180_mat);
  const auto z180 = Rotationd::FromAxisAndAngle(Vector3d(0.0, 0.0, 1.0),
                                                Angled::FromRadians(M_PI));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               z180.GetQuaternion(), z180_converted.GetQuaternion());

  // The following rotation should create plenty of off axis elements.
  const auto off_axis_rotation = Rotationd::FromAxisAndAngle(
      Vector3d(1.0, 1.0, -0.5), Angled::FromRadians(M_PI / 4));
  const Rotationd off_axis_rotation_converted =
      Rotationd::FromRotationMatrix(RotationMatrixNH(off_axis_rotation));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               off_axis_rotation.GetQuaternion(),
               off_axis_rotation_converted.GetQuaternion());
}

TEST(TransformUtils, RotationVectorMultiply) {
  // A rotation by -90 degrees around X axis.
  const Rotationd r = Rotationd::FromAxisAndAngle(Vector3d::AxisX(),
                                                  Angled::FromDegrees(-90.0));
  // +Y-axis becomes -Z-axis.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               -Vector3d::AxisZ(), r * Vector3d::AxisY());
  // +Z-axis becomes +Y-axis.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::AxisY(), r * Vector3d::AxisZ());
  // Origin and X-axis stay the same.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::Zero(), r * Vector3d::Zero());
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::AxisX(), r * Vector3d::AxisX());

  // Generic point.
  EXPECT_PRED2((testing::PointsAlmostEqual<3, double>),
               Point3d(1.0, 3.0, -2.0), r * Point3d(1.0, 2.0, 3.0));
}

}  // namespace math
}  // namespace ion
