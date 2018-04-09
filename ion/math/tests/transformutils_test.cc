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

#include "ion/math/transformutils.h"

#include <array>

#include "ion/base/logchecker.h"
#include "ion/math/matrix.h"
#include "ion/math/matrixutils.h"
#include "ion/math/tests/testutils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"
#include "ion/port/nullptr.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

namespace {

// Templated test to simplify double/float coverage.
template <typename T>
static void TestPerspectiveMatrixFromView() {
  // This example is based on the gluPerspective documentation.
  const Angle<T> fovy = Angle<T>::FromDegrees(60.0);
  const T aspect(2);
  const T z_near(4);
  const T z_far(44);
  const T f = static_cast<T>(1. / tan(fovy.Radians() / 2.));
  const T C((z_far + z_near) / (z_near - z_far));
  const T D((2.f * z_far * z_near) / (z_near - z_far));
  typedef Matrix<4, T> Matrix;
  const Matrix expected(f / aspect, 0.0, 0.0, 0.0,
                        0.0, f, 0.0, 0.0,
                        0.0, 0.0, C, D,
                        0.0, 0.0, -1.0, 0.0);
  EXPECT_PRED2((testing::MatricesAlmostEqual<4, T>),
               expected,
               PerspectiveMatrixFromView(fovy, aspect, z_near, z_far));

  // Error cases.
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(Angle<T>::FromRadians(T(0)), aspect,
                                      z_near, z_far));
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(Angle<T>::FromRadians(-0.1f), aspect,
                                      z_near, z_far));
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(fovy, T(0), z_near, z_far));
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(fovy, T(-0.1), z_near, z_far));
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(fovy, aspect, T(12), T(12)));
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(fovy, aspect, T(0), z_far));
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(fovy, aspect, T(-0.1), z_far));
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(fovy, aspect, z_near, T(0)));
  EXPECT_EQ(Matrix::Identity(),
            PerspectiveMatrixFromView(fovy, aspect, z_near, T(-0.1)));
}

}  // anonymous namespace

TEST(TransformUtils, Multiply) {
  const Matrix4d m(1.0, 2.0, 3.0, 4.0,
                   5.0, 6.0, 7.0, 8.0,
                   9.0, 10.0, 11.0, 12.0,
                   13.0, 14.0, 15.0, 16.0);
  EXPECT_EQ(Vector3d(1400.0, 3800.0, 6200.0),
            m * Vector3d(100.0, 200.0, 300.0));
  EXPECT_EQ(Point3d(1404.0, 3808.0, 6212.0),
            m * Point3d(100.0, 200.0, 300.0));
}

TEST(TransformUtils, ProjectPoint) {
  // Projecting a point by a matrix of one higher dimension is the same as
  // multiplying the matrix with a point of one higher dimension with a
  // homogeneous coordinate of 1 and then dividing by the resulting homogeneous
  // coordinate.
  const Matrix4d m(1.0, 2.0, 3.0, 4.0,
                   5.0, 6.0, 7.0, 8.0,
                   9.0, 10.0, 11.0, 12.0,
                   13.0, 14.0, 15.0, 16.0);
  Point4d p = m * Point4d(100.0, 200.0, 300.0, 1.0);
  p /= p[3];
  Point3d projected = ProjectPoint(m, Point3d(100.0, 200.0, 300.0));
  EXPECT_EQ(p, Point4d(projected[0], projected[1], projected[2], 1.0));

  // Create an identity matrix.
  Matrix4d proj = Matrix4d::Identity();
  Point3d test_point(1.0, 2.0, 3.0);

  // Projecting with an identity matrix does nothing.
  EXPECT_EQ(test_point, ProjectPoint(proj, test_point));

  // Flip x and y, negate z, with w = 1.0.
  proj = Matrix4d(0.0, 1.0, 0.0, 0.0,
                  1.0, 0.0, 0.0, 0.0,
                  0.0, 0.0, -1.0, 0.0,
                  0.0, 0.0, 0.0, 1.0);
  EXPECT_EQ(Point3d(2.0, 1.0, -3.0), ProjectPoint(proj, test_point));

  // Doubling the w-component halves the result.
  proj[3][3] = 2.0;
  EXPECT_EQ(Point3d(1.0, 0.5, -1.5), ProjectPoint(proj, test_point));
}

TEST(TransformUtils, TranslationMatrix) {
  EXPECT_EQ(Matrix3f(1.0f, 0.0f, 5.0f,
                     0.0f, 1.0f, 3.0f,
                     0.0f, 0.0f, 1.0f),
            TranslationMatrix(Vector2f(5.0f, 3.0f)));

  EXPECT_EQ(Matrix4d(1.0, 0.0, 0.0, 2.0,
                     0.0, 1.0, 0.0, -9.0,
                     0.0, 0.0, 1.0, -12.0,
                     0.0, 0.0, 0.0, 1.0),
            TranslationMatrix(Vector3d(2.0, -9.0, -12.0)));

  // Should also work with a Point.
  EXPECT_EQ(Matrix4d(1.0, 0.0, 0.0, 2.0,
                     0.0, 1.0, 0.0, -9.0,
                     0.0, 0.0, 1.0, -12.0,
                     0.0, 0.0, 0.0, 1.0),
            TranslationMatrix(Point3d(2.0, -9.0, -12.0)));

  // Verify that a translation matrix actually translates.
  EXPECT_EQ(Point3d(5.5, 1.0, 5.5),
            TranslationMatrix(Vector3d(4.5, -1.0, 2.5)) *
            Point3d(1.0, 2.0, 3.0));
}

TEST(TransformUtils, ScaleMatrixH) {
  EXPECT_EQ(Matrix3f(9.0f, 0.0f, 0.0f,
                     0.0f, -8.0f, 0.0f,
                     0.0f, 0.0f, 1.0f),
            ScaleMatrixH(Vector2f(9.0f, -8.0f)));

  EXPECT_EQ(Matrix4d(-3.0, 0.0, 0.0, 0.0,
                     0.0, 2.0, 0.0, 0.0,
                     0.0, 0.0, 7.0, 0.0,
                     0.0, 0.0, 0.0, 1.0),
            ScaleMatrixH(Vector3d(-3.0, 2.0, 7.0)));

  // Verify that a scale matrix actually scales.
  EXPECT_EQ(Point3d(2.0, 6.0, -15.0),
            ScaleMatrixH(Vector3d(2.0, 3.0, -5.0)) * Point3d(1.0, 2.0, 3.0));
}

TEST(TransformUtils, ScaleMatrixNH) {
  EXPECT_EQ(Matrix3f(9.0f, 0.0f, 0.0f,
                     0.0f, -8.0f, 0.0f,
                     0.0f, 0.0f, 12.0f),
            ScaleMatrixNH(Vector3f(9.0f, -8.0f, 12.0f)));

  EXPECT_EQ(Matrix4d(-3.0, 0.0, 0.0, 0.0,
                     0.0, 2.0, 0.0, 0.0,
                     0.0, 0.0, 7.0, 0.0,
                     0.0, 0.0, 0.0, 9.0),
            ScaleMatrixNH(Vector4d(-3.0, 2.0, 7.0, 9.0)));

  // Verify that a scale matrix actually scales.
  EXPECT_EQ(Point3d(2.0, 6.0, -15.0),
            ScaleMatrixNH(Vector4d(2.0, 3.0, -5.0, 1.0)) *
            Point3d(1.0, 2.0, 3.0));
}

TEST(TransformUtils, RotationMatrixH) {
  // A rotation by 90 degrees around Z axis.
  const Matrix4d m = RotationMatrixAxisAngleH(Vector3d::AxisZ(),
                                              Angled::FromDegrees(90.0));
  // +X-axis becomes +Y-axis.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::AxisY(), m * Vector3d::AxisX());
  // +Y-axis becomes -X-axis.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               -Vector3d::AxisX(), m * Vector3d::AxisY());
  // Origin and +Z-axis stay the same.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::Zero(), m * Vector3d::Zero());
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::AxisZ(), m * Vector3d::AxisZ());

  // Generic point.
  EXPECT_PRED2((testing::PointsAlmostEqual<3, double>),
               Point3d(-2.0, 1.0, 3.0), m * Point3d(1.0, 2.0, 3.0));
}

TEST(TransformUtils, RotationMatrixNH) {
  // A rotation by -90 degrees around X axis.
  const Matrix3d m = RotationMatrixAxisAngleNH(Vector3d::AxisX(),
                                               Angled::FromDegrees(-90.0));
  // +Y-axis becomes -Z-axis.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               -Vector3d::AxisZ(), m * Vector3d::AxisY());
  // +Z-axis becomes +Y-axis.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::AxisY(), m * Vector3d::AxisZ());
  // Origin and X-axis stay the same.
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::Zero(), m * Vector3d::Zero());
  EXPECT_PRED2((testing::VectorsAlmostEqual<3, double>),
               Vector3d::AxisX(), m * Vector3d::AxisX());

  // Generic point.
  EXPECT_PRED2((testing::PointsAlmostEqual<3, double>),
               Point3d(1.0, 3.0, -2.0), m * Point3d(1.0, 2.0, 3.0));
}

TEST(TransformUtils, RangeMapping) {
  const Range2f src = Range2f(Point2f(-1.f, -2.f), Point2f(3.f, 0.f));
  const Range2f dest = Range2f(Point2f(0.f, 0.f), Point2f(8.f, 8.f));
  const Matrix3f mapping = RangeMappingMatrixH(src, dest);

  // Check endpoints.
  EXPECT_PRED2((testing::PointsAlmostEqual<2, float>),
               dest.GetMinPoint(), mapping * src.GetMinPoint());
  EXPECT_PRED2((testing::PointsAlmostEqual<2, float>),
               dest.GetMaxPoint(), mapping * src.GetMaxPoint());
  // Check a point in the middle of the source range.
  EXPECT_PRED2((testing::PointsAlmostEqual<2, float>),
               dest.GetCenter(), mapping * src.GetCenter());
  // Check reasonable behavior for empty and degenerate input ranges.
  EXPECT_TRUE(src.ContainsPoint(
      RangeMappingMatrixH(Range2f(), src) * Point2f(123.f, 234.f)));
  EXPECT_TRUE(dest.ContainsPoint(
      RangeMappingMatrixH(Range2f(), dest) * Point2f(-123.f, -4.f)));
  EXPECT_PRED2((testing::PointsAlmostEqual<2, float>), Point2f(0.f, 4.f),
      RangeMappingMatrixH(Range2f(Point2f(), Point2f(0.f, 16.f)), dest) *
          Point2f(7.f, 8.f));
  // Check points at some arbitrary place.
  const Vector2f test_point(0.125f, 0.675f);
  EXPECT_PRED2((testing::PointsAlmostEqual<2, float>),
               dest.GetMinPoint() + test_point * dest.GetSize(),
               mapping * (src.GetMinPoint() + test_point * src.GetSize()));
}

TEST(TransformUtils, Composition) {
  // Verify that transformation matrices compose as expected.
  const Matrix4d s = ScaleMatrixH(Vector3d(4.0, 5.0, 6.0));
  const Matrix4d t = TranslationMatrix(Vector3d(10.0, 20.0, 30.0));
  const Matrix4d st = s * t;
  const Matrix4d ts = t * s;

  // The ts matrix should scale, then translate, so the origin should just be
  // translated.
  EXPECT_EQ(Point3d(10.0, 20.0, 30.0), ts * Point3d::Zero());

  // The st matrix should translate, then scale, so the origin should be
  // translated and scaled.
  EXPECT_EQ(Point3d(40.0, 100.0, 180.0), st * Point3d::Zero());
}

TEST(TransformUtils, NonhomogeneousSubmatrixH) {
  Matrix4f mat_h(1.0f, 2.0f, 3.0f, 4.0f,
               5.0f, 6.0f, 7.0f, 8.0f,
               9.0f, 10.0f, 11.0f, 12.0f,
               13.0f, 14.0f, 15.0f, 16.0f);
  Matrix3f mat_nh(1.0f, 2.0f, 3.0f,
                  5.0f, 6.0f, 7.0f,
                  9.0f, 10.0f, 11.0f);
  EXPECT_PRED2((testing::MatricesAlmostEqual<3, float>),
               mat_nh, NonhomogeneousSubmatrixH(mat_h));
}

TEST(TransformUtils, OrthoInverseH) {
  // Upper left 3x3 matrix of mat is orthogonal.
  Matrix4d mat(2.0 / 3.0, -2.0 / 3.0, 1.0 / 3.0, 1.0,
               1.0 / 3.0, 2.0 / 3.0, 2.0 / 3.0, 2.0,
               2.0 / 3.0, 1.0 / 3.0, -2.0 / 3.0, 3.0,
               0.0f, 0.0f, 0.0f, 1.0f);
  EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>),
               Inverse(mat), OrthoInverseH(mat));
}

TEST(TransformUtils, LookAtMatrix) {
  const Point3d eye(0., 0., 3.);
  const Point3d center(0., 1., 0.);
  const Vector3d up = Vector3d::AxisY();

  const Vector3d front = Normalized(center - eye);
  const Vector3d right = Normalized(Cross(front, up));
  const Vector3d new_up = Normalized(Cross(right, front));

  Matrix4d lookat(right[0], right[1], right[2], 0,
                  new_up[0], new_up[1], new_up[2], 0,
                  -front[0], -front[1], -front[2], 0,
                  0, 0, 0, 1);

  lookat = lookat * TranslationMatrix(-eye);

  EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>),
               lookat, LookAtMatrixFromCenter(eye, center, up));
  EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>),
               lookat, LookAtMatrixFromDir(eye, center - eye, up));

#if !ION_PRODUCTION
  // Error cases for LookAtMatrixFromCenter and LookAtMatrixFromDir.
  // Pass in a zero direction vector.
  EXPECT_DEATH_IF_SUPPORTED(LookAtMatrixFromCenter(eye, eye, up),
                            "zero length or are parallel");
  EXPECT_DEATH_IF_SUPPORTED(LookAtMatrixFromDir(eye, Vector3d::Zero(), up),
                            "zero length or are parallel");

  // Pass in a zero up vector.
  EXPECT_DEATH_IF_SUPPORTED(
      LookAtMatrixFromCenter(eye, center, Vector3d::Zero()),
      "zero length or are parallel");
  EXPECT_DEATH_IF_SUPPORTED(
      LookAtMatrixFromDir(eye, center - eye, Vector3d::Zero()),
      "zero length or are parallel");

  // Pass in a parallel up and direction vectors.
  EXPECT_DEATH_IF_SUPPORTED(
      LookAtMatrixFromCenter(eye, center, 42.0f * (center - eye)),
      "zero length or are parallel");
  EXPECT_DEATH_IF_SUPPORTED(
      LookAtMatrixFromDir(eye, center - eye, 42.0f * (center - eye)),
      "zero length or are parallel");
#endif
}

TEST(TransformUtils, OrthographicMatrixFromFrustum) {
  // This example is based on the glFrustum documentation.
  const double x_left = 1.0;
  const double x_right = 11.0;
  const double y_bottom = -22.0;
  const double y_top = -2.0;
  const double z_near = 4.0;
  const double z_far = 44.0;
  const double X = 2.0 / (x_right - x_left);
  const double Y = 2.0 / (y_top - y_bottom);
  const double Z = 2.0 / (z_near - z_far);
  const double A = (x_right + x_left) / (x_left - x_right);
  const double B = (y_top + y_bottom) / (y_bottom - y_top);
  const double C = (z_near + z_far) / (z_near - z_far);
  const Matrix4d expected(X, 0.0, 0.0, A,
                          0.0, Y, 0.0, B,
                          0.0, 0.0, Z, C,
                          0.0, 0.0, 0.0, 1.0);
  EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>),
               expected,
               OrthographicMatrixFromFrustum(
                   x_left, x_right, y_bottom, y_top, z_near, z_far));

  // Error cases.
  EXPECT_EQ(Matrix4d::Identity(),
            OrthographicMatrixFromFrustum(10.0, 10.0, y_bottom, y_top,
                                          z_near, z_far));
  EXPECT_EQ(Matrix4d::Identity(),
            OrthographicMatrixFromFrustum(x_left, x_right, -12.0, -12.0,
                                          z_near, z_far));
  EXPECT_EQ(Matrix4d::Identity(),
            OrthographicMatrixFromFrustum(x_left, x_right, y_bottom, y_top,
                                          15.0, 15.0));
}

TEST(TransformUtils, PerspectiveMatrixFromFrustum) {
  // This example is based on the glFrustum documentation.
  const double x_left = 1.0;
  const double x_right = 11.0;
  const double y_bottom = -22.0;
  const double y_top = -2.0;
  const double z_near = 4.0;
  const double z_far = 44.0;
  const double A = (x_right + x_left) / (x_right - x_left);
  const double B = (y_top + y_bottom) / (y_top - y_bottom);
  const double C = -(z_far + z_near) / (z_far - z_near);
  const double D = -(2.0 * z_far * z_near) / (z_far - z_near);
  const Matrix4d expected(2 * z_near / (x_right - x_left), 0.0, A, 0.0,
                          0.0, 2 * z_near / (y_top - y_bottom), B, 0.0,
                          0.0, 0.0, C, D,
                          0.0, 0.0, -1.0, 0.0);
  EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>),
               expected,
               PerspectiveMatrixFromFrustum(
                   x_left, x_right, y_bottom, y_top, z_near, z_far));

  // Error cases.
  EXPECT_EQ(Matrix4d::Identity(),
            PerspectiveMatrixFromFrustum(10.0, 10.0, y_bottom, y_top,
                                         z_near, z_far));
  EXPECT_EQ(Matrix4d::Identity(),
            PerspectiveMatrixFromFrustum(x_left, x_right, -12.0, -12.0,
                                         z_near, z_far));
  EXPECT_EQ(Matrix4d::Identity(),
            PerspectiveMatrixFromFrustum(x_left, x_right, y_bottom, y_top,
                                         15.0, 15.0));
  EXPECT_EQ(Matrix4d::Identity(),
            PerspectiveMatrixFromFrustum(x_left, x_right, y_bottom, y_top,
                                         0.0, z_far));
  EXPECT_EQ(Matrix4d::Identity(),
            PerspectiveMatrixFromFrustum(x_left, x_right, y_bottom, y_top,
                                         -0.1, z_far));
  EXPECT_EQ(Matrix4d::Identity(),
            PerspectiveMatrixFromFrustum(x_left, x_right, y_bottom, y_top,
                                         z_near, 0.0));
  EXPECT_EQ(Matrix4d::Identity(),
            PerspectiveMatrixFromFrustum(x_left, x_right, y_bottom, y_top,
                                         z_near, -0.1));
}

TEST(TransformUtils, PerspectiveMatrixFromView) {
  // Test double and float versions for coverage.
  TestPerspectiveMatrixFromView<double>();
  TestPerspectiveMatrixFromView<float>();
}

TEST(TransformUtils, InfiniteFarClip) {
  const double z_near = 0.3;
  const double z_far_epsilon = 0.0f;

  const Matrix4d infinite_far_clip = PerspectiveMatrixFromInfiniteFrustum(
      -z_near, z_near, -z_near, z_near, z_near, z_far_epsilon);

  const double z_far = std::numeric_limits<float>::max();
  struct TestCase { Point3d world; Point3d clip; };
  const std::array<TestCase, 5> test_cases{{
      {{0.0, 0.0, -z_near}, {0.0, 0.0, -1.0}},       // Center near.
      {{0.0, 0.0, -z_far}, {0.0, 0.0, 1.0}},         // Center far.
      {{-z_near, 0.0, -z_near}, {-1.0, 0.0, -1.0}},  // Left near.
      {{-z_far, 0.0, -z_far}, {-1.0, 0.0, 1.0}},     // Left far.
      {{z_far, z_far, -z_far}, {1.0, 1.0, 1.0}}      // Top-right far.
  }};

  for (const TestCase& test_case : test_cases) {
    const Point3d projected_clip =
        ProjectPoint(infinite_far_clip, test_case.world);
    EXPECT_EQ(projected_clip, test_case.clip);
  }

  EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>),
               Inverse(infinite_far_clip),
               PerspectiveMatrixInverse(infinite_far_clip));
}

TEST(TransformUtils, PerspectiveMatrixInverse) {
  const double x_left = 1.0;
  const double x_right = 11.0;
  const double y_bottom = -22.0;
  const double y_top = -2.0;
  const double z_near = 4.0;
  const double z_far = 44.0;
  Matrix4d matrix = PerspectiveMatrixFromFrustum(
      x_left, x_right, y_bottom, y_top, z_near, z_far);
  EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>),
               Inverse(matrix),
               PerspectiveMatrixInverse(matrix));
}

TEST(TransformUtils, GetTranslationVector) {
  Vector<3, float> translation_a(1.0f, 2.0f, 3.0f);
  Vector<3, float> translation_b(4.0f, 5.0f, 6.0f);
  Matrix<4, float> matrix_a = TranslationMatrix(translation_a);
  Matrix<4, float> matrix_b = TranslationMatrix(translation_b);
  Vector<3, float> translation_expect = translation_a + translation_b;

  EXPECT_EQ(translation_a, GetTranslationVector(matrix_a));
  EXPECT_EQ(translation_expect, GetTranslationVector(matrix_a * matrix_b));
}

TEST(TransformUtils, GetScaleVector) {
  Vector<3, float> scale_a(1.0f, 2.0f, 3.0f);
  Vector<3, float> scale_b(4.0f, 5.0f, 6.0f);
  Matrix<4, float> matrix_a = ScaleMatrixH(scale_a);
  Matrix<4, float> matrix_b = ScaleMatrixH(scale_b);

  EXPECT_EQ(scale_a, GetScaleVector(matrix_a));

  Vector<3, float> scale_expect;
  for (int i = 0; i < 3; ++i) {
    scale_expect[i] = scale_a[i] * scale_b[i];
  }
  EXPECT_EQ(scale_expect, GetScaleVector(matrix_a * matrix_b));
}

TEST(TransformUtils, GetRotationMatrix) {
  Vector<3, float> axis(1.0f, 0.0f, 0.0f);
  Angle<float> angle_a = Angle<float>::FromDegrees(10.0f);
  Angle<float> angle_b = Angle<float>::FromDegrees(20.0f);
  Angle<float> angle_result = angle_a + angle_b;
  Rotation<float> rotation_a = Rotation<float>::FromAxisAndAngle(axis, angle_a);
  Rotation<float> rotation_b = Rotation<float>::FromAxisAndAngle(axis, angle_b);
  Rotation<float> rotation_expect =
      Rotation<float>::FromAxisAndAngle(axis, angle_result);
  Matrix<4, float> matrix_a = RotationMatrixH(rotation_a);
  Matrix<4, float> matrix_b = RotationMatrixH(rotation_b);

  EXPECT_EQ(rotation_a,
            Rotation<float>::FromRotationMatrix(GetRotationMatrix(matrix_a)));

  Rotation<float> rotation_actual = Rotation<float>::FromRotationMatrix(
      GetRotationMatrix(matrix_a * matrix_b));
  Vector<3, float> axis_expect;
  Vector<3, float> axis_actual;
  Angle<float> angle_expect;
  Angle<float> angle_actual;
  rotation_expect.GetAxisAndAngle(&axis_expect, &angle_expect);
  rotation_actual.GetAxisAndAngle(&axis_actual, &angle_actual);
  EXPECT_TRUE(VectorsAlmostEqual(axis_expect, axis_actual, 1e-4f));
  EXPECT_NEAR(angle_expect.Degrees(), angle_actual.Degrees(), 1e-4f);
}

TEST(TransformUtils, GetAllMatrixComponents) {
  float epsilon = 1e-4f;
  Vector<3, float> translation_default = Vector<3, float>(1.0f, 2.0f, 3.0f);
  Vector<3, float> scale_default = Vector<3, float>(4.0f, 5.0f, 6.0f);
  Rotation<float> rotation_default = Rotation<float>::FromAxisAndAngle(
      Vector<3, float>(7.0f, 8.0f, 9.0f), Angle<float>::FromDegrees(10.0f));
  Matrix<4, float> matrix_default = TranslationMatrix(translation_default) *
                                    RotationMatrixH(rotation_default) *
                                    ScaleMatrixH(scale_default);

  // Check Translation component.
  EXPECT_EQ(translation_default, GetTranslationVector(matrix_default));

  // Check Scale component.
  for (int i = 0; i < 3; ++i) {
    EXPECT_NEAR(scale_default[i], GetScaleVector(matrix_default)[i], epsilon);
  }

  // Check Rotation component.
  Rotation<float> rotation_expect = rotation_default;
  Rotation<float> rotation_actual =
      Rotation<float>::FromRotationMatrix(GetRotationMatrix(matrix_default));
  Vector<3, float> axis_expect;
  Vector<3, float> axis_actual;
  Angle<float> angle_expect;
  Angle<float> angle_actual;
  rotation_expect.GetAxisAndAngle(&axis_expect, &angle_expect);
  rotation_actual.GetAxisAndAngle(&axis_actual, &angle_actual);
  EXPECT_TRUE(VectorsAlmostEqual(axis_expect, axis_actual, epsilon));
  EXPECT_NEAR(angle_expect.Degrees(), angle_actual.Degrees(), epsilon);
}

TEST(TransformUtils, Interpolation) {
  Vector<3, float> translation_from(1.0f, 2.0f, 3.0f);
  Vector<3, float> translation_to(10.0f, 20.0f, 30.0f);
  Matrix<4, float> matrix_from = TranslationMatrix(translation_from);
  Matrix<4, float> matrix_to = TranslationMatrix(translation_to);

  EXPECT_EQ(matrix_from, Interpolate(matrix_from, matrix_to, 0.0f));
  EXPECT_EQ(matrix_to, Interpolate(matrix_from, matrix_to, 1.0f));

  float rate = 0.5f;
  Matrix<4, float> matrix_actual = matrix_from;
  for (int i = 1; i <= 10; ++i) {
    // Every step, we translate the translation at (1 - rate^step). This means
    // it will progress from "from" to "to". This mirrors Lerp for translation.
    // The equation is: result = (end - start) * (1 - rate^step) + start.
    Vector<3, float> translation =
        (translation_to - translation_from) *
            (1.0f - std::pow(rate, static_cast<float>(i))) +
        translation_from;
    matrix_actual = Interpolate(matrix_actual, matrix_to, rate);
    EXPECT_EQ(TranslationMatrix(translation), matrix_actual);
  }
}

}  // namespace math
}  // namespace ion
