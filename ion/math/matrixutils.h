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

#ifndef ION_MATH_MATRIXUTILS_H_
#define ION_MATH_MATRIXUTILS_H_

//
// This file contains operators and free functions that define generic Matrix
// operations. See transformutils.h for Matrix operations that are specific to
// 3D transformations.
//

#include "ion/math/matrix.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"

namespace ion {
namespace math {

//-----------------------------------------------------------------------------
// Internal helper functions.
//-----------------------------------------------------------------------------

namespace internal {

// Multiplies a matrix and some type of column vector (Vector or Point) to
// produce another column vector of the same type.
template <int Dimension, typename T, typename VectorType>
VectorType MultiplyMatrixAndVector(const Matrix<Dimension, T>& m,
                                   const VectorType& v) {
  VectorType result = VectorType::Zero();
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result[row] += m(row, col) * v[col];
  }
  return result;
}

}  // namespace internal

//-----------------------------------------------------------------------------
// Public functions.
//-----------------------------------------------------------------------------

// Returns the transpose of a matrix.
template <int Dimension, typename T>
Matrix<Dimension, T> Transpose(const Matrix<Dimension, T>& m) {
  Matrix<Dimension, T> result;
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result(row, col) = m(col, row);
  }
  return result;
}

// Multiplies a Matrix and a column Vector of the same Dimension to produce
// another column Vector.
template <int Dimension, typename T>
inline Vector<Dimension, T> operator*(const Matrix<Dimension, T>& m,
                                      const Vector<Dimension, T>& v) {
  return internal::MultiplyMatrixAndVector(m, v);
}

// Multiplies a Matrix and a Point of the same Dimension to produce another
// Point.
template <int Dimension, typename T>
inline Point<Dimension, T> operator*(const Matrix<Dimension, T>& m,
                                     const Point<Dimension, T>& p) {
  return internal::MultiplyMatrixAndVector(m, p);
}

// Returns a particular row of a matrix as a vector.
template <int Dimension, typename T>
Vector<Dimension, T> Row(const Matrix<Dimension, T>& m, int row) {
  Vector<Dimension, T> result;
  for (int col = 0; col < Dimension; ++col)
    // Note that Matrix's operator() already performs range checking, so it is
    // not necessary to do this here.
    result[col] = m(row, col);
  return result;
}

// Returns a particular column of a matrix as a vector.
template <int Dimension, typename T>
Vector<Dimension, T> Column(const Matrix<Dimension, T>& m, int col) {
  Vector<Dimension, T> result;
  for (int row = 0; row < Dimension; ++row)
    // Note that Matrix's operator() already performs range checking, so it is
    // not necessary to do this here.
    result[row] = m(row, col);
  return result;
}

// Returns a matrix with the specified row and column removed, yielding a matrix
// that is one dimension smaller.
template <int Dimension, typename T>
Matrix<Dimension - 1, T> WithoutDimension(const Matrix<Dimension, T>& m,
                                          int dim) {
  Matrix<Dimension - 1, T> result;
  for (int row = 0; row < Dimension - 1; ++row) {
    for (int col = 0; col < Dimension - 1; ++col) {
      result(row, col) = m(row + (row < dim ? 0 : 1),
                           col + (col < dim ? 0 : 1));
    }
  }
  return result;
}

// Returns a matrix expanded by an identity row and column in the specified
// dimension, yielding a matrix which is one dimension larger.
template <int Dimension, typename T>
Matrix<Dimension + 1, T> WithIdentityDimension(const Matrix<Dimension, T>& m,
                                               int dim) {
  Matrix<Dimension + 1, T> result;
  for (int row = 0; row < Dimension + 1; ++row) {
    for (int col = 0; col < Dimension + 1; ++col) {
      if (row == dim && col == dim) {
        result(row, col) = T(1);
      } else if (row == dim || col == dim) {
        result(row, col) = T(0);
      } else {
        result(row, col) = m(row - (row < dim ? 0 : 1),
                             col - (col < dim ? 0 : 1));
      }
    }
  }
  return result;
}

// Returns the determinant of the matrix. This function is defined for all the
// typedef'ed Matrix types.
template <int Dimension, typename T> ION_API
T Determinant(const Matrix<Dimension, T>& m);

// Returns the signed cofactor matrix (adjunct) of the matrix. This function is
// defined for all the typedef'ed Matrix types.
template <int Dimension, typename T> ION_API
Matrix<Dimension, T> CofactorMatrix(const Matrix<Dimension, T>& m);

// Returns the adjugate of the matrix, which is defined as the transpose of the
// cofactor matrix. This function is defined for all the typedef'ed Matrix
// types.  The determinant of the matrix is computed as a side effect, so it is
// returned in the determinant parameter if it is not null.
template <int Dimension, typename T> ION_API
Matrix<Dimension, T> AdjugateWithDeterminant(
    const Matrix<Dimension, T>& m, T* determinant);

// Returns the adjugate of the matrix, which is defined as the transpose of the
// cofactor matrix. This function is defined for all the typedef'ed Matrix
// types.
template <int Dimension, typename T>
Matrix<Dimension, T> Adjugate(const Matrix<Dimension, T>& m) {
  return AdjugateWithDeterminant<Dimension, T>(m, nullptr);
}

// Returns the inverse of the matrix. This function is defined for all the
// typedef'ed Matrix types.  The determinant of the matrix is computed as a
// side effect, so it is returned in the determinant parameter if it is not
// null. If the determinant is 0, the returned matrix has all zeroes.
template <int Dimension, typename T> ION_API
Matrix<Dimension, T> InverseWithDeterminant(
    const Matrix<Dimension, T>& m, T* determinant);

// Returns the inverse of the matrix. This function is defined for all the
// typedef'ed Matrix types. If the determinant of the matrix is 0, the returned
// matrix has all zeroes.
template <int Dimension, typename T>
inline Matrix<Dimension, T> Inverse(const Matrix<Dimension, T>& m) {
  return InverseWithDeterminant<Dimension, T>(m, nullptr);
}

// Returns true if all elements of two matrices are equal within a tolerance.
template <int Dimension, typename T>
bool AlmostEqual(const Matrix<Dimension, T>& m0,
                 const Matrix<Dimension, T>& m1, T tolerance) {
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col) {
      if (Abs(m0(row, col) - m1(row, col)) > tolerance)
        return false;
    }
  }
  return true;
}

// Alias for AlmostEqual with matrix parameters.
template <int Dimension, typename T>
bool MatricesAlmostEqual(const Matrix<Dimension, T>& m0,
                         const Matrix<Dimension, T>& m1, T tolerance) {
  return AlmostEqual(m0, m1, tolerance);
}

// Returns true if the dot product of all column vector pairs in the matrix
// is less than a provided tolerance, and if all column vectors have unit
// length. Return false otherwise.
template <int Dimension, typename T>
bool MatrixAlmostOrthogonal(const Matrix<Dimension, T>& m, T tolerance);

// Scales the rightmost column of a 4x4 Matrix (except for the bottom-right
// element) by a constant scalar. This can be used to exaggerate translation
// effects in an affine transformation.
template <typename T>
inline static void ScaleTranslationComponent(Matrix<4, T>* matrix, T scale) {
  (*matrix)(0, 3) *= scale;
  (*matrix)(1, 3) *= scale;
  (*matrix)(2, 3) *= scale;
}

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_MATRIXUTILS_H_
