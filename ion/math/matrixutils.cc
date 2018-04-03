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

#include "ion/base/logging.h"
#include "ion/math/utils.h"
#include "ion/math/vectorutils.h"

namespace ion {
namespace math {

namespace {

//-----------------------------------------------------------------------------
// Internal Cofactor helper functions.  Each of these computes the signed
// cofactor element for a row and column of a matrix of a certain size.
// -----------------------------------------------------------------------------

// Returns true if the cofactor for a given row and column should be negated.
static bool IsCofactorNegated(int row, int col) {
  // Negated iff (row + col) is odd.
  return ((row + col) & 1) != 0;
}

template <typename T>
static T CofactorElement3(const Matrix<3, T>& m, int row, int col) {
  static const int index[3][2] = { {1, 2}, {0, 2}, {0, 1} };
  const int i0 = index[row][0];
  const int i1 = index[row][1];
  const int j0 = index[col][0];
  const int j1 = index[col][1];
  const T cofactor = m(i0, j0) * m(i1, j1) - m(i0, j1) * m(i1, j0);
  return IsCofactorNegated(row, col) ? -cofactor : cofactor;
}

template <typename T>
static T CofactorElement4(const Matrix<4, T>& m, int row, int col) {
  // The cofactor of element (row,col) is the determinant of the 3x3 submatrix
  // formed by removing that row and column.
  static const int index[4][3] = { {1, 2, 3}, {0, 2, 3}, {0, 1, 3}, {0, 1, 2} };
  const int i0 = index[row][0];
  const int i1 = index[row][1];
  const int i2 = index[row][2];
  const int j0 = index[col][0];
  const int j1 = index[col][1];
  const int j2 = index[col][2];
  const T c0 =  m(i0, j0) * (m(i1, j1) * m(i2, j2) - m(i1, j2) * m(i2, j1));
  const T c1 = -m(i0, j1) * (m(i1, j0) * m(i2, j2) - m(i1, j2) * m(i2, j0));
  const T c2 =  m(i0, j2) * (m(i1, j0) * m(i2, j1) - m(i1, j1) * m(i2, j0));
  const T cofactor = c0 + c1 + c2;
  return IsCofactorNegated(row, col) ? -cofactor : cofactor;
}

//-----------------------------------------------------------------------------
// Internal Determinant helper functions.  Each of these computes the
// determinant of a matrix of a certain size.
// -----------------------------------------------------------------------------

template <typename T>
static T Determinant2(const Matrix<2, T>& m) {
  return m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0);
}

template <typename T>
static T Determinant3(const Matrix<3, T>& m) {
  return (m(0, 0) * CofactorElement3(m, 0, 0) +
          m(0, 1) * CofactorElement3(m, 0, 1) +
          m(0, 2) * CofactorElement3(m, 0, 2));
}

template <typename T>
static T Determinant4(const Matrix<4, T>& m) {
  return (m(0, 0) * CofactorElement4(m, 0, 0) +
          m(0, 1) * CofactorElement4(m, 0, 1) +
          m(0, 2) * CofactorElement4(m, 0, 2) +
          m(0, 3) * CofactorElement4(m, 0, 3));
}

//-----------------------------------------------------------------------------
// Internal CofactorMatrix helper functions.  Each of these computes the
// cofactor matrix for a matrix of a certain size.
// -----------------------------------------------------------------------------

template <typename T>
Matrix<2, T> CofactorMatrix2(const Matrix<2, T>& m) {
  return Matrix<2, T>(m(1, 1), -m(1, 0),
                      -m(0, 1), m(0, 0));
}

template <typename T>
Matrix<3, T> CofactorMatrix3(const Matrix<3, T>& m) {
  Matrix<3, T> result;
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col)
      result(row, col) = CofactorElement3(m, row, col);
  }
  return result;
}

template <typename T>
Matrix<4, T> CofactorMatrix4(const Matrix<4, T>& m) {
  Matrix<4, T> result;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col)
      result(row, col) = CofactorElement4(m, row, col);
  }
  return result;
}

//-----------------------------------------------------------------------------
// Internal Adjugate helper functions.  Each of these computes the adjugate
// matrix for a matrix of a certain size.
// -----------------------------------------------------------------------------

template <typename T>
Matrix<2, T> Adjugate2(const Matrix<2, T>& m, T* determinant) {
  const T m00 = m(0, 0);
  const T m01 = m(0, 1);
  const T m10 = m(1, 0);
  const T m11 = m(1, 1);
  if (determinant)
    *determinant = m00 * m11 - m01 * m10;
  return Matrix<2, T>(m11, -m01, -m10, m00);
}

template <typename T>
Matrix<3, T> Adjugate3(const Matrix<3, T>& m, T* determinant) {
  const Matrix<3, T> cofactor_matrix = CofactorMatrix3(m);
  if (determinant) {
    *determinant = m(0, 0) * cofactor_matrix(0, 0) +
                   m(0, 1) * cofactor_matrix(0, 1) +
                   m(0, 2) * cofactor_matrix(0, 2);
  }
  return Transpose(cofactor_matrix);
}

template <typename T>
Matrix<4, T> Adjugate4(const Matrix<4, T>& m, T* determinant) {
  // For 4x4 do not compute the adjugate as the transpose of the cofactor
  // matrix, because this results in extra work. Several calculations can be
  // shared across the sub-determinants.
  //
  // This approach is explained in David Eberly's Geometric Tools book,
  // excerpted here:
  //   http://www.geometrictools.com/Documentation/LaplaceExpansionTheorem.pdf
  const T s0 = m(0, 0) * m(1, 1) - m(1, 0) * m(0, 1);
  const T s1 = m(0, 0) * m(1, 2) - m(1, 0) * m(0, 2);
  const T s2 = m(0, 0) * m(1, 3) - m(1, 0) * m(0, 3);

  const T s3 = m(0, 1) * m(1, 2) - m(1, 1) * m(0, 2);
  const T s4 = m(0, 1) * m(1, 3) - m(1, 1) * m(0, 3);
  const T s5 = m(0, 2) * m(1, 3) - m(1, 2) * m(0, 3);

  const T c0 = m(2, 0) * m(3, 1) - m(3, 0) * m(2, 1);
  const T c1 = m(2, 0) * m(3, 2) - m(3, 0) * m(2, 2);
  const T c2 = m(2, 0) * m(3, 3) - m(3, 0) * m(2, 3);

  const T c3 = m(2, 1) * m(3, 2) - m(3, 1) * m(2, 2);
  const T c4 = m(2, 1) * m(3, 3) - m(3, 1) * m(2, 3);
  const T c5 = m(2, 2) * m(3, 3) - m(3, 2) * m(2, 3);

  if (determinant)
    *determinant = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;

  return Matrix<4, T>(
      m(1, 1) * c5 - m(1, 2) * c4 + m(1, 3) * c3,
      -m(0, 1) * c5 + m(0, 2) * c4 - m(0, 3) * c3,
      m(3, 1) * s5 - m(3, 2) * s4 + m(3, 3) * s3,
      -m(2, 1) * s5 + m(2, 2) * s4 - m(2, 3) * s3,

      -m(1, 0) * c5 + m(1, 2) * c2 - m(1, 3) * c1,
      m(0, 0) * c5 - m(0, 2) * c2 + m(0, 3) * c1,
      -m(3, 0) * s5 + m(3, 2) * s2 - m(3, 3) * s1,
      m(2, 0) * s5 - m(2, 2) * s2 + m(2, 3) * s1,

      m(1, 0) * c4 - m(1, 1) * c2 + m(1, 3) * c0,
      -m(0, 0) * c4 + m(0, 1) * c2 - m(0, 3) * c0,
      m(3, 0) * s4 - m(3, 1) * s2 + m(3, 3) * s0,
      -m(2, 0) * s4 + m(2, 1) * s2 - m(2, 3) * s0,

      -m(1, 0) * c3 + m(1, 1) * c1 - m(1, 2) * c0,
      m(0, 0) * c3 - m(0, 1) * c1 + m(0, 2) * c0,
      -m(3, 0) * s3 + m(3, 1) * s1 - m(3, 2) * s0,
      m(2, 0) * s3 - m(2, 1) * s1 + m(2, 2) * s0);
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
// Public functions.
//
// Partial specialization of template functions is not allowed in C++, so these
// functions are implemented using fully-specialized functions that invoke
// helper functions that operate on matrices of a specific size.
// -----------------------------------------------------------------------------

//
// The unspecialized versions should never be called.
//

template <int Dimension, typename T>
T Determinant(const Matrix<Dimension, T>& m) {
  DCHECK(false) << "Unspecialized Matrix Determinant function used.";
}

template <int Dimension, typename T>
Matrix<Dimension, T> CofactorMatrix(const Matrix<Dimension, T>& m) {
  DCHECK(false) << "Unspecialized Matrix Cofactor function used.";
}

template <int Dimension, typename T>
Matrix<Dimension, T> AdjugateWithDeterminant(
    const Matrix<Dimension, T>& m, T* determinant) {
  DCHECK(false) << "Unspecialized Matrix Adjugate function used.";
}

template <int Dimension, typename T>
Matrix<Dimension, T> InverseWithDeterminant(
    const Matrix<Dimension, T>& m, T* determinant) {
  // The inverse is the adjugate divided by the determinant.
  T det;
  Matrix<Dimension, T> adjugate = AdjugateWithDeterminant(m, &det);
  if (determinant)
    *determinant = det;
  if (det == static_cast<T>(0))
    return Matrix<Dimension, T>::Zero();
  else
    return adjugate * (static_cast<T>(1) / det);
}

template <int Dimension, typename T>
bool MatrixAlmostOrthogonal(const Matrix<Dimension, T>& m, T tolerance) {
  for (int col1 = 0; col1 < Dimension; ++col1) {
    Vector<Dimension, T> column = Column(m, col1);
    // Test for pairwise orthogonality of column vectors.
    for (int col2 = col1 + 1; col2 < Dimension; ++col2)
      if (Dot(column, Column(m, col2)) > tolerance)
        return false;
    // Test for unit length.
    if (Abs(LengthSquared(column) - static_cast<T>(1)) > tolerance)
      return false;
  }
  return true;
}

//
// Fully specialize or instantiate all of these for all supported types.
//

#define ION_SPECIALIZE_MATRIX_FUNCS(dim, scalar)                             \
template <> scalar ION_API Determinant(const Matrix<dim, scalar>& m) {       \
  return Determinant ## dim(m);                                              \
}                                                                            \
template <> Matrix<dim, scalar> ION_API CofactorMatrix(                      \
    const Matrix<dim, scalar>& m) {                                          \
  return CofactorMatrix ## dim(m);                                           \
}                                                                            \
template <> Matrix<dim, scalar> ION_API AdjugateWithDeterminant(             \
    const Matrix<dim, scalar>& m, scalar* determinant) {                     \
  return Adjugate ## dim(m, determinant);                                    \
}                                                                            \
                                                                             \
/* Explicit instantiations. */                                               \
template Matrix<dim, scalar> ION_API InverseWithDeterminant(                 \
    const Matrix<dim, scalar>& m, scalar* determinant);                      \
template bool ION_API MatrixAlmostOrthogonal(                                \
    const Matrix<dim, scalar>& m, scalar tolerance)

ION_SPECIALIZE_MATRIX_FUNCS(2, float);
ION_SPECIALIZE_MATRIX_FUNCS(2, double);
ION_SPECIALIZE_MATRIX_FUNCS(3, float);
ION_SPECIALIZE_MATRIX_FUNCS(3, double);
ION_SPECIALIZE_MATRIX_FUNCS(4, float);
ION_SPECIALIZE_MATRIX_FUNCS(4, double);

#undef ION_SPECIALIZE_MATRIX_FUNCS

}  // namespace math
}  // namespace ion
