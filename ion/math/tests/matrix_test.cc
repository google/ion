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

#include "ion/math/matrix.h"

#include <sstream>
#include <string>

#include "absl/base/macros.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace math {

typedef Matrix<2, int> Matrix2i;

TEST(Matrix, MatricDefaultConstructorZeroInitializes) {
  // Try the default constructor for a variety of element types and expect the
  // appropriate zeros.
  Matrix<1, double> m1d;
  EXPECT_EQ(0.0, m1d(0, 0));

  Matrix<1, float> m1f;
  EXPECT_EQ(0.0f, m1f(0, 0));

  Matrix<1, int> m1i;
  EXPECT_EQ(0, m1i(0, 0));

  // For a pointer type, zero-intialization means nullptr.
  Matrix<1, void*> m1p;
  EXPECT_EQ(nullptr, m1p(0, 0));

  // Test a matrix with several elements and ensure that they're all zeroed.
  Matrix4d m4d;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      EXPECT_EQ(0.0, m4d(i, j));
    }
  }
}

TEST(Matrix, MatrixConstructor) {
  Matrix2d m2d(4.0, -5.0,
               1.5, 15.0);
  EXPECT_EQ(4.0, m2d(0, 0));
  EXPECT_EQ(-5.0, m2d(0, 1));
  EXPECT_EQ(1.5, m2d(1, 0));
  EXPECT_EQ(15.0, m2d(1, 1));

  Matrix3d m3f(6.2f, 1.8f, 2.6f,
               -7.4f, -9.2f, 1.3f,
               -4.1f, 5.3f, -1.9f);
  EXPECT_EQ(6.2f, m3f(0, 0));
  EXPECT_EQ(1.8f, m3f(0, 1));
  EXPECT_EQ(2.6f, m3f(0, 2));
  EXPECT_EQ(-7.4f, m3f(1, 0));
  EXPECT_EQ(-9.2f, m3f(1, 1));
  EXPECT_EQ(1.3f, m3f(1, 2));
  EXPECT_EQ(-4.1f, m3f(2, 0));
  EXPECT_EQ(5.3f, m3f(2, 1));
  EXPECT_EQ(-1.9f, m3f(2, 2));

  Matrix4d m4d(21.1, 22.2, 23.3, 24.4,
               25.5, 26.6, 27.7, 28.8,
               29.9, 30.0, 31.1, 32.2,
               33.3, 34.4, 35.5, 36.6);
  EXPECT_EQ(21.1, m4d(0, 0));
  EXPECT_EQ(22.2, m4d(0, 1));
  EXPECT_EQ(23.3, m4d(0, 2));
  EXPECT_EQ(24.4, m4d(0, 3));
  EXPECT_EQ(25.5, m4d(1, 0));
  EXPECT_EQ(26.6, m4d(1, 1));
  EXPECT_EQ(27.7, m4d(1, 2));
  EXPECT_EQ(28.8, m4d(1, 3));
  EXPECT_EQ(29.9, m4d(2, 0));
  EXPECT_EQ(30.0, m4d(2, 1));
  EXPECT_EQ(31.1, m4d(2, 2));
  EXPECT_EQ(32.2, m4d(2, 3));
  EXPECT_EQ(33.3, m4d(3, 0));
  EXPECT_EQ(34.4, m4d(3, 1));
  EXPECT_EQ(35.5, m4d(3, 2));
  EXPECT_EQ(36.6, m4d(3, 3));

  const double elements[4 * 4] = {
    21.1, 22.2, 23.3, 24.4,
    25.5, 26.6, 27.7, 28.8,
    29.9, 30.0, 31.1, 32.2,
    33.3, 34.4, 35.5, 36.6
  };
  Matrix4d m_from_array(elements);
  EXPECT_EQ(21.1, m_from_array(0, 0));
  EXPECT_EQ(22.2, m_from_array(0, 1));
  EXPECT_EQ(23.3, m_from_array(0, 2));
  EXPECT_EQ(24.4, m_from_array(0, 3));
  EXPECT_EQ(25.5, m_from_array(1, 0));
  EXPECT_EQ(26.6, m_from_array(1, 1));
  EXPECT_EQ(27.7, m_from_array(1, 2));
  EXPECT_EQ(28.8, m_from_array(1, 3));
  EXPECT_EQ(29.9, m_from_array(2, 0));
  EXPECT_EQ(30.0, m_from_array(2, 1));
  EXPECT_EQ(31.1, m_from_array(2, 2));
  EXPECT_EQ(32.2, m_from_array(2, 3));
  EXPECT_EQ(33.3, m_from_array(3, 0));
  EXPECT_EQ(34.4, m_from_array(3, 1));
  EXPECT_EQ(35.5, m_from_array(3, 2));
  EXPECT_EQ(36.6, m_from_array(3, 3));

  // Unaligned data.
  double buffer[ABSL_ARRAYSIZE(elements) + 1];
  double* unaligned_elements =
      reinterpret_cast<double*>(reinterpret_cast<char*>(buffer) + 1);
  memcpy(unaligned_elements, elements, sizeof(elements));
  Matrix4d m_from_array_unaligned(unaligned_elements);
  EXPECT_EQ(21.1, m_from_array_unaligned(0, 0));
  EXPECT_EQ(22.2, m_from_array_unaligned(0, 1));
  EXPECT_EQ(23.3, m_from_array_unaligned(0, 2));
  EXPECT_EQ(24.4, m_from_array_unaligned(0, 3));
  EXPECT_EQ(25.5, m_from_array_unaligned(1, 0));
  EXPECT_EQ(26.6, m_from_array_unaligned(1, 1));
  EXPECT_EQ(27.7, m_from_array_unaligned(1, 2));
  EXPECT_EQ(28.8, m_from_array_unaligned(1, 3));
  EXPECT_EQ(29.9, m_from_array_unaligned(2, 0));
  EXPECT_EQ(30.0, m_from_array_unaligned(2, 1));
  EXPECT_EQ(31.1, m_from_array_unaligned(2, 2));
  EXPECT_EQ(32.2, m_from_array_unaligned(2, 3));
  EXPECT_EQ(33.3, m_from_array_unaligned(3, 0));
  EXPECT_EQ(34.4, m_from_array_unaligned(3, 1));
  EXPECT_EQ(35.5, m_from_array_unaligned(3, 2));
  EXPECT_EQ(36.6, m_from_array_unaligned(3, 3));

  Matrix4d md(21.1, 22.2, 23.3, 24.4,
              25.5, 26.6, 27.7, 28.8,
              29.9, 30.0, 31.1, 32.2,
              33.3, 34.4, 35.5, 36.6);
  Matrix4f mf(md);
  EXPECT_EQ(21.1f, mf(0, 0));
  EXPECT_EQ(22.2f, mf(0, 1));
  EXPECT_EQ(23.3f, mf(0, 2));
  EXPECT_EQ(24.4f, mf(0, 3));
  EXPECT_EQ(25.5f, mf(1, 0));
  EXPECT_EQ(26.6f, mf(1, 1));
  EXPECT_EQ(27.7f, mf(1, 2));
  EXPECT_EQ(28.8f, mf(1, 3));
  EXPECT_EQ(29.9f, mf(2, 0));
  EXPECT_EQ(30.0f, mf(2, 1));
  EXPECT_EQ(31.1f, mf(2, 2));
  EXPECT_EQ(32.2f, mf(2, 3));
  EXPECT_EQ(33.3f, mf(3, 0));
  EXPECT_EQ(34.4f, mf(3, 1));
  EXPECT_EQ(35.5f, mf(3, 2));
  EXPECT_EQ(36.6f, mf(3, 3));
}

TEST(Matrix, Accessors) {
  Matrix4d m4d(21.1, 22.2, 23.3, 24.4,
               25.5, 26.6, 27.7, 28.8,
               29.9, 30.0, 31.1, 32.2,
               33.3, 34.4, 35.5, 36.6);
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      EXPECT_EQ(m4d(row, col), m4d[row][col]);
    }
  }

  m4d(2, 3) = 100.0;
  m4d[3][2] = 101.0;
  EXPECT_EQ(100.0, m4d(2, 3));
  EXPECT_EQ(101.0, m4d(3, 2));

  const Matrix4d& cm4d = m4d;
  EXPECT_EQ(100.0, cm4d[2][3]);
  EXPECT_EQ(101.0, cm4d[3][2]);
  EXPECT_EQ(100.0, cm4d(2, 3));
  EXPECT_EQ(101.0, cm4d(3, 2));
}

TEST(Matrix, Equals) {
  EXPECT_EQ(Matrix2d(1.0, 2.0, 3.0, 4.0), Matrix2d(1.0, 2.0, 3.0, 4.0));
  EXPECT_NE(Matrix2d(1.0, 2.0, 3.0, 4.0), Matrix2d(1.1, 2.0, 3.0, 4.0));
  EXPECT_NE(Matrix2d(1.0, 2.0, 3.0, 4.0), Matrix2d(1.0, 2.1, 3.0, 4.0));
  EXPECT_NE(Matrix2d(1.0, 2.0, 3.0, 4.0), Matrix2d(1.0, 2.0, 3.1, 4.0));
  EXPECT_NE(Matrix2d(1.0, 2.0, 3.0, 4.0), Matrix2d(1.0, 2.0, 3.0, -4.0));
}

TEST(Matrix, Zero) {
  EXPECT_EQ(Matrix2d(0.0, 0.0,
                     0.0, 0.0), Matrix2d::Zero());
  EXPECT_EQ(Matrix3f(0.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 0.0f), Matrix3f::Zero());
  EXPECT_EQ(Matrix4f(0.0f, 0.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 0.0f), Matrix4f::Zero());
  EXPECT_EQ(Matrix4d(0.0, 0.0, 0.0, 0.0,
                     0.0, 0.0, 0.0, 0.0,
                     0.0, 0.0, 0.0, 0.0,
                     0.0, 0.0, 0.0, 0.0), Matrix4d::Zero());
}

TEST(Matrix, Identity) {
  EXPECT_EQ(Matrix2d(1.0, 0.0,
                     0.0, 1.0), Matrix2d::Identity());
  EXPECT_EQ(Matrix3f(1.0f, 0.0f, 0.0f,
                     0.0f, 1.0f, 0.0f,
                     0.0f, 0.0f, 1.0f), Matrix3f::Identity());
  EXPECT_EQ(Matrix4f(1.0f, 0.0f, 0.0f, 0.0f,
                     0.0f, 1.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 1.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 1.0f), Matrix4f::Identity());
  EXPECT_EQ(Matrix4d(1.0, 0.0, 0.0, 0.0,
                     0.0, 1.0, 0.0, 0.0,
                     0.0, 0.0, 1.0, 0.0,
                     0.0, 0.0, 0.0, 1.0), Matrix4d::Identity());
}

TEST(Matrix, Data) {
  Matrix2f m2f(4.0, -5.0, 6.0, -7.0);
  EXPECT_EQ(4.0, m2f.Data()[0]);
  EXPECT_EQ(-5.0, m2f.Data()[1]);
  EXPECT_EQ(6.0, m2f.Data()[2]);
  EXPECT_EQ(-7.0, m2f.Data()[3]);
}

TEST(Matrix, MatrixSelfModifyingMathOperators) {
  Matrix3d m(1.0, 2.0, 3.0,
             4.0, 5.0, 6.0,
             7.0, 8.0, 9.0);
  m *= -2.0;
  EXPECT_EQ(Matrix3d(-2.0, -4.0, -6.0,
                     -8.0, -10.0, -12.0,
                     -14.0, -16.0, -18.0), m);

  m *= Matrix3d(2.0, 0.0, 0.0,
                0.0, 3.0, 0.0,
                4.0, 5.0, 1.0);
  EXPECT_EQ(Matrix3d(-28.0, -42.0, -6.0,
                     -64.0, -90.0, -12.0,
                     -100.0, -138.0, -18.0), m);
}

TEST(Matrix, MatrixUnaryAndBinaryMathOperators) {
  Matrix3d m(1.0, -2.0, 3.0,
             -4.0, 5.0, -6.0,
             7.0, -8.0, 9.0);

  EXPECT_EQ(Matrix3d(-1.0, 2.0, -3.0,
                     4.0, -5.0, 6.0,
                     -7.0, 8.0, -9.0), -m);

  EXPECT_EQ(Matrix3d(3.0, -6.0, 9.0,
                     -12.0, 15.0, -18.0,
                     21.0, -24.0, 27.0), m * 3.0);
  EXPECT_EQ(Matrix3d(3.0, -6.0, 9.0,
                     -12.0, 15.0, -18.0,
                     21.0, -24.0, 27.0), 3.0 * m);

  Matrix3d m0(2.0, 4.0, 6.0,
              8.0, 10.0, 12.0,
              14.0, 16.0, 18.0);
  Matrix3d m1(2.0, 0.0, 0.0,
              0.0, 3.0, 0.0,
              4.0, 5.0, 1.0);
  EXPECT_EQ(Matrix3d(28.0, 42.0, 6.0,
                     64.0, 90.0, 12.0,
                     100.0, 138.0, 18.0), m0 * m1);

  Matrix3d m2(2.0, 4.0, 6.0,
              8.0, 10.0, 12.0,
              14.0, 16.0, 18.0);

  Matrix3d m3(1.0, 0.0, 0.0,
              0.0, 3.0, -1.0,
              7.0, 13.0, -5.0);

  EXPECT_EQ(Matrix3d(3.0, 4.0, 6.0,
                     8.0, 13.0, 11.0,
                     21.0, 29.0, 13.0), m2 + m3);
  EXPECT_EQ(Matrix3d(1.0, 4.0, 6.0,
                     8.0, 7.0, 13.0,
                     7.0, 3.0, 23.0), m2 - m3);
}

TEST(Matrix, Streaming) {
  std::ostringstream out;
  out << Matrix3d(1.5, 2.5, 3.5,
                  4.5, 5.5, 6.5,
                  7.5, 8.5, 9.5);
  EXPECT_EQ(std::string("M[1.5, 2.5, 3.5 ; 4.5, 5.5, 6.5 ; 7.5, 8.5, 9.5]"),
            out.str());

  {
    std::istringstream in("M[1.5, 2.5, 3.5 ; 4.5, 5.5, 6.5 ; 7.5, 8.5, 9.5]");
    Matrix3d m = Matrix3d::Zero();
    in >> m;
    EXPECT_EQ(Matrix3d(1.5, 2.5, 3.5,
                       4.5, 5.5, 6.5,
                       7.5, 8.5, 9.5), m);
  }
  {
    std::istringstream in("M[1, 2; 4, 5 ]");
    Matrix2i m = Matrix2i::Zero();
    in >> m;
    EXPECT_EQ(Matrix2i(1, 2, 4, 5), m);
  }
  {
    std::istringstream in("M[ 1, 2 ; 4,5]");
    Matrix2i m = Matrix2i::Zero();
    in >> m;
    EXPECT_EQ(Matrix2i(1, 2, 4, 5), m);
  }
  {
    std::istringstream in("M[1, 2; 4, 5");
    Matrix2i m = Matrix2i::Zero();
    in >> m;
    EXPECT_EQ(Matrix2i(0, 0, 0, 0), m);
  }
  {
    std::istringstream in("M[1, 2, 4, ; 5]");
    Matrix2i m = Matrix2i::Zero();
    in >> m;
    EXPECT_EQ(Matrix2i(0, 0, 0, 0), m);
  }
  {
    std::istringstream in("[1, 2; 4, 5]");
    Matrix2i m = Matrix2i::Zero();
    in >> m;
    EXPECT_EQ(Matrix2i(0, 0, 0, 0), m);
  }
  {
    std::istringstream in("M1, 2; 4, 5]");
    Matrix2i m = Matrix2i::Zero();
    in >> m;
    EXPECT_EQ(Matrix2i(0, 0, 0, 0), m);
  }
  {
    std::istringstream in("M[1, 2, 4, 5]");
    Matrix2i m = Matrix2i::Zero();
    in >> m;
    EXPECT_EQ(Matrix2i(0, 0, 0, 0), m);
  }
  {
    std::istringstream in("M[1 2, 4, 5]");
    Matrix2i m = Matrix2i::Zero();
    in >> m;
    EXPECT_EQ(Matrix2i(0, 0, 0, 0), m);
  }
}

}  // namespace math
}  // namespace ion
