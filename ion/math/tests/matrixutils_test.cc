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

#include "ion/math/matrixutils.h"

#include "ion/math/tests/testutils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

TEST(MatrixUtils, Transpose) {
  EXPECT_EQ(Matrix3d(1.0, 4.0, 7.0,
                     2.0, 5.0, 8.0,
                     3.0, 6.0, 9.0),
            Transpose(Matrix3d(1.0, 2.0, 3.0,
                               4.0, 5.0, 6.0,
                               7.0, 8.0, 9.0)));
  EXPECT_EQ(Matrix4d(1.0, 4.0, 7.0, 33.0,
                     2.0, 5.0, 8.0, 44.0,
                     3.0, 6.0, 9.0, 55.0,
                     11.0, 12.0, 13.0, 69.0),
            Transpose(Matrix4d(1.0, 2.0, 3.0, 11.0,
                               4.0, 5.0, 6.0, 12.0,
                               7.0, 8.0, 9.0, 13.0,
                               33.0, 44.0, 55.0, 69.0)));
}

TEST(MatrixUtils, MultiplyVectorAndPoint) {
  Matrix3d m(1.0, 2.0, 3.0,
             4.0, 5.0, 6.0,
             7.0, 8.0, 9.0);

  EXPECT_EQ(Vector3d(140.0, 320.0, 500.0),
            m * Vector3d(10.0, 20.0, 30.0));

  EXPECT_EQ(Point3d(140.0, 320.0, 500.0),
            m * Point3d(10.0, 20.0, 30.0));
}

TEST(MatrixUtils, DimensionUtils) {
  Matrix4d m1(1.1, 1.2, 1.3, 1.4,
              2.1, 2.2, 2.3, 2.4,
              3.1, 3.2, 3.3, 3.4,
              4.1, 4.2, 4.3, 4.4);
  EXPECT_EQ(Matrix3d(2.2, 2.3, 2.4,
                     3.2, 3.3, 3.4,
                     4.2, 4.3, 4.4),
            WithoutDimension(m1, 0));
  EXPECT_EQ(Matrix3d(1.1, 1.2, 1.4,
                     2.1, 2.2, 2.4,
                     4.1, 4.2, 4.4),
            WithoutDimension(m1, 2));
  EXPECT_EQ(Matrix3d(1.1, 1.2, 1.3,
                     2.1, 2.2, 2.3,
                     3.1, 3.2, 3.3),
            WithoutDimension(m1, 3));

  Matrix3d m2(1.1, 1.2, 1.3,
              2.1, 2.2, 2.3,
              3.1, 3.2, 3.3);
  EXPECT_EQ(Matrix4d(1.0, 0.0, 0.0, 0.0,
                     0.0, 1.1, 1.2, 1.3,
                     0.0, 2.1, 2.2, 2.3,
                     0.0, 3.1, 3.2, 3.3),
            WithIdentityDimension(m2, 0));
  EXPECT_EQ(Matrix4d(1.1, 1.2, 0.0, 1.3,
                     2.1, 2.2, 0.0, 2.3,
                     0.0, 0.0, 1.0, 0.0,
                     3.1, 3.2, 0.0, 3.3),
            WithIdentityDimension(m2, 2));
  EXPECT_EQ(Matrix4d(1.1, 1.2, 1.3, 0.0,
                     2.1, 2.2, 2.3, 0.0,
                     3.1, 3.2, 3.3, 0.0,
                     0.0, 0.0, 0.0, 1.0),
              WithIdentityDimension(m2, 3));
}

TEST(MatrixUtils, Determinant) {
  EXPECT_EQ(7.0f, Determinant(Matrix2f(2.0f, 3.0f,
                                       1.0f, 5.0f)));

  EXPECT_EQ(103.0, Determinant(Matrix3d(5.0, -2.0, 1.0,
                                        0.0, 3.0, -1.0,
                                        2.0, 0.0, 7.0)));

  EXPECT_EQ(322.0, Determinant(Matrix4d(1.0, 2.0, 8.0, 0.0,
                                        5.0, 6.0, 2.0, 8.0,
                                        9.0, 1.0, 11.0, 12.0,
                                        0.0, 3.0, 2.0, -1.0)));
}

TEST(MatrixUtils, CofactorMatrix) {
  EXPECT_EQ(Matrix2f(5.0f, -1.0f,
                     -3.0f, 2.0f),
            CofactorMatrix(Matrix2f(2.0f, 3.0f,
                                    1.0f, 5.0f)));

  EXPECT_EQ(Matrix3d(-24.0, 18.0, 5.0,
                     20.0, -15.0, -4.0,
                     -5.0, 4.0, 1.0),
            CofactorMatrix(Matrix3d(1.0, 0.0, 5.0,
                                    2.0, 1.0, 6.0,
                                    3.0, 4.0, 0.0)));

  EXPECT_EQ(Matrix4d(-60.0, -74.0, 78.0, 24.0,
                     41.0, -29.0, -75.0, 27.0,
                     39.0, -17.0, -29.0, -59.0,
                     -152.0, 44.0, 24.0, -26.0),
            CofactorMatrix(Matrix4d(1.0, 4.0, -1.0, 0.0,
                                    2.0, 3.0, 5.0, -2.0,
                                    0.0, 3.0, 1.0, 6.0,
                                    3.0, 0.0, 2.0, 1.0)));
}

TEST(MatrixUtils, Adjugate) {
  float det_f;
  double det_d;
  EXPECT_EQ(Matrix2f(5.0f, -3.0f,
                     -1.0f, 2.0f),
            AdjugateWithDeterminant(Matrix2f(2.0f, 3.0f,
                                             1.0f, 5.0f), &det_f));
  EXPECT_EQ(7.0f, det_f);

  EXPECT_EQ(Matrix3f(-24.0f, 20.0f, -5.0f,
                     18.0f, -15.0f, 4.0f,
                     5.0f, -4.0f, 1.0f),
            AdjugateWithDeterminant(Matrix3f(1.0f, 0.0f, 5.0f,
                                             2.0f, 1.0f, 6.0f,
                                             3.0f, 4.0f, 0.0f), &det_f));
  EXPECT_EQ(1.0f, det_f);
  EXPECT_EQ(Matrix3f(-24.0f, 20.0f, 11.0f,
                      -18.0f, 15.0f, -4.0f,
                      -11.0f, 1.0f, 3.0f),
             AdjugateWithDeterminant(Matrix3f(1.0f, -1.0f, -5.0f,
                                              2.0f, 1.0f, -6.0f,
                                              3.0f, -4.0f, 0.0f), &det_f));
  EXPECT_EQ(49.0f, det_f);

  EXPECT_EQ(Matrix4d(-60.0, 41.0, 39.0, -152.0,
                     -74.0, -29.0, -17.0, 44.0,
                     78.0, -75.0, -29.0, 24.0,
                     24.0, 27.0, -59.0, -26.0),
            AdjugateWithDeterminant(Matrix4d(1.0, 4.0, -1.0, 0.0,
                                             2.0, 3.0, 5.0, -2.0,
                                             0.0, 3.0, 1.0, 6.0,
                                             3.0, 0.0, 2.0, 1.0), &det_d));
  EXPECT_EQ(-434.0, det_d);

  EXPECT_EQ(Matrix4d(-60.0, 41.0, 39.0, -152.0,
                     -74.0, -29.0, -17.0, 44.0,
                     78.0, -75.0, -29.0, 24.0,
                     24.0, 27.0, -59.0, -26.0),
            Adjugate(Matrix4d(1.0, 4.0, -1.0, 0.0,
                              2.0, 3.0, 5.0, -2.0,
                              0.0, 3.0, 1.0, 6.0,
                              3.0, 0.0, 2.0, 1.0)));
}

TEST(MatrixUtils, Inverse) {
  {
    const float kDet = 7.0f;
    float det;
    EXPECT_PRED2((testing::MatricesAlmostEqual<2, float>),
                 Matrix2f(5.0f / kDet, -3.0f / kDet,
                          -1.0f / kDet, 2.0f / kDet),
                 InverseWithDeterminant(Matrix2f(2.0f, 3.0f,
                                                 1.0f, 5.0f), &det));
    EXPECT_EQ(kDet, det);
  }

  {
    const double kDet = 1.0;
    double det;
    EXPECT_PRED2((testing::MatricesAlmostEqual<3, double>),
                 Matrix3d(-24.0 / kDet, 20.0 / kDet, -5.0 / kDet,
                          18.0 / kDet, -15.0 / kDet, 4.0 / kDet,
                          5.0 / kDet, -4.0 / kDet, 1.0 / kDet),
                 InverseWithDeterminant(Matrix3d(1.0, 0.0, 5.0,
                                                 2.0, 1.0, 6.0,
                                                 3.0, 4.0, 0.0), &det));
    EXPECT_EQ(1.0, det);
  }

  {
    const double kDet = -434.0;
    double det;
    EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>),
                 Matrix4d(-60.0 / kDet, 41.0 / kDet, 39.0 / kDet, -152.0 / kDet,
                          -74.0 / kDet, -29.0 / kDet, -17.0 / kDet, 44.0 / kDet,
                          78.0 / kDet, -75.0 / kDet, -29.0 / kDet, 24.0 / kDet,
                          24.0 / kDet, 27.0 / kDet, -59.0 / kDet, -26.0 / kDet),
                 InverseWithDeterminant(Matrix4d(1.0, 4.0, -1.0, 0.0,
                                                 2.0, 3.0, 5.0, -2.0,
                                                 0.0, 3.0, 1.0, 6.0,
                                                 3.0, 0.0, 2.0, 1.0), &det));
    EXPECT_EQ(kDet, det);
  }

  {
    const float kDet = -434.f;
    EXPECT_PRED2((testing::MatricesAlmostEqual<4, float>),
                 Matrix4f(-60.f / kDet, 41.f / kDet, 39.f / kDet, -152.f / kDet,
                          -74.f / kDet, -29.f / kDet, -17.f / kDet, 44.f / kDet,
                          78.f / kDet, -75.f / kDet, -29.f / kDet, 24.f / kDet,
                          24.f / kDet, 27.f / kDet, -59.f / kDet, -26.f / kDet),
                 Inverse(Matrix4f(1.f, 4.f, -1.f, 0.f,
                                  2.f, 3.f, 5.f, -2.f,
                                  0.f, 3.f, 1.f, 6.f,
                                  3.f, 0.f, 2.f, 1.f)));
  }

  {
    // This should fail with a 0 determinant.
    double det;
    EXPECT_EQ(Matrix2d::Zero(),
              InverseWithDeterminant(Matrix2d(4.0, 6.0, 2.0, 3.0), &det));
    EXPECT_EQ(0.0, det);
    EXPECT_EQ(Matrix2d::Zero(), Inverse(Matrix2d(4.0, 6.0, 2.0, 3.0)));
  }
}

TEST(MatrixUtils, MatricesAlmostEqual) {
  EXPECT_TRUE(MatricesAlmostEqual(Matrix2f(1.0f, 2.0f, 3.0f, -4.0f),
                                  Matrix2f(1.0f, 2.0f, 3.0f, -4.0f), 0.0f));
  EXPECT_TRUE(MatricesAlmostEqual(Matrix2f(1.0f, 2.0f, 3.0f, -4.0f),
                                  Matrix2f(1.0f, 2.1f, 3.0f, -4.0f), 0.11f));
  EXPECT_TRUE(MatricesAlmostEqual(
                  Matrix3d(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, -9.0),
                  Matrix3d(1.0, 2.0, 3.0, 4.0, 5.1, 6.0, 7.0, 8.0, -9.0),
                  0.11));
  EXPECT_TRUE(MatricesAlmostEqual(
                  Matrix3d(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, -9.0),
                  Matrix3d(1.0, 1.9, 3.0, 4.0, 5.1, 6.0, 7.0, 8.0, -8.9),
                  0.11));
  EXPECT_FALSE(MatricesAlmostEqual(
                   Matrix3d(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, -9.0),
                   Matrix3d(1.0, 1.9, 3.0, 4.0, 5.1, 6.2, 7.0, 8.0, -8.9),
                   0.11));
  EXPECT_FALSE(MatricesAlmostEqual(
                   Matrix3d(1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, -9.0),
                   Matrix3d(1.0, 1.9, 3.0, 4.0, 5.1, 6.1, 7.1, 8.0, -8.8),
                   0.11));
}

TEST(MatrixUtils, ScaleTranslationComponent) {
  Matrix4d mat(1.0, 2.0, 3.0, 4.0,
               5.0, 6.0, 7.0, 8.0,
               9.0, 10.0, 11.0, 12.0,
               13.0, 14.0, 15.0, 16.0);
  Matrix4d mat_scaled(1.0, 2.0, 3.0, 40.0,
                      5.0, 6.0, 7.0, 80.0,
                      9.0, 10.0, 11.0, 120.0,
                      13.0, 14.0, 15.0, 16.0);
  ScaleTranslationComponent(&mat, 10.0);
  EXPECT_PRED2((testing::MatricesAlmostEqual<4, double>), mat, mat_scaled);
}

TEST(MatrixUtils, Row) {
  Matrix4d mat(1.0, 2.0, 3.0, 4.0,
               5.0, 6.0, 7.0, 8.0,
               9.0, 10.0, 11.0, 12.0,
               13.0, 14.0, 15.0, 16.0);
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               Vector4d(1.0, 2.0, 3.0, 4.0), Row(mat, 0));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               Vector4d(5.0, 6.0, 7.0, 8.0), Row(mat, 1));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               Vector4d(9.0, 10.0, 11.0, 12.0), Row(mat, 2));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, double>),
               Vector4d(13.0, 14.0, 15.0, 16.0), Row(mat, 3));
}

TEST(MatrixUtils, Column) {
  Matrix4f mat(1.0f, 2.0f, 3.0f, 4.0f,
               5.0f, 6.0f, 7.0f, 8.0f,
               9.0f, 10.0f, 11.0f, 12.0f,
               13.0f, 14.0f, 15.0f, 16.0f);
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, float>),
               Vector4f(1.0f, 5.0f, 9.0f, 13.0f), Column(mat, 0));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, float>),
               Vector4f(2.0f, 6.0f, 10.0f, 14.0f), Column(mat, 1));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, float>),
               Vector4f(3.0f, 7.0f, 11.0f, 15.0f), Column(mat, 2));
  EXPECT_PRED2((testing::VectorsAlmostEqual<4, float>),
               Vector4f(4.0f, 8.0f, 12.0f, 16.0f), Column(mat, 3));
}

TEST(MatrixUtils, IsAlmostOrthogonal) {
  // 2D case.
  EXPECT_TRUE(MatrixAlmostOrthogonal(Matrix2f(1.0f, 0.0f,
                                              0.0f, 1.0f), 1e-6f));
  // Test for pairwise orthogonality of basis vectors.
  EXPECT_FALSE(MatrixAlmostOrthogonal(Matrix2f(1.0f, 0.01f,
                                               0.0f, 1.0f), 1e-6f));
  // Test for unit length.
  EXPECT_FALSE(MatrixAlmostOrthogonal(Matrix2f(2.0f, 0.0f,
                                               0.0f, 1.0f), 1e-6f));

  // 3D case.
  EXPECT_TRUE(MatrixAlmostOrthogonal(Matrix3d(1.0, 0.0, 0.0,
                                              0.0, 1.0, 0.0,
                                              0.0, 0.0, 1.0), 1e-6));
  // Test for pairwise orthogonality of basis vectors.
  EXPECT_FALSE(MatrixAlmostOrthogonal(Matrix3d(1.0, 0.0, 0.01,
                                               0.0, 1.0, 0.0,
                                               0.0, 0.0, 1.0), 1e-6));
  // Test for unit length.
  EXPECT_FALSE(MatrixAlmostOrthogonal(Matrix3d(2.0, 0.0, 0.0,
                                               0.0, 1.0, 0.0,
                                               0.0, 0.0, 1.0), 1e-6));
}

}  // namespace math
}  // namespace ion
