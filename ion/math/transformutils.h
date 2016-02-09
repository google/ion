/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#ifndef ION_MATH_TRANSFORMUTILS_H_
#define ION_MATH_TRANSFORMUTILS_H_

// This file contains free functions that implement N-dimensional
// transformations involving the Matrix class.
//
// We assume that transformation matrices operate on column vectors only,
// implying the following rules:
//
//  - Since matrices are stored in row-major order, a transformation matrix has
//    the translation components in the last column.
//
//  - When two transformation matrices are multiplied, the matrix on the LHS is
//    the one with the more local effect. For example, if R is a rotation matrix
//    and T is a translation matrix, then T*R will rotate, then translate,
//    while R*T will translate, then rotate.
//
//  - Using the *= operator (which post-multiplies the RHS matrix) to compose
//    matrices may seem counterintuitive, as the matrix on the right side of
//    the operator will have a more global effect. That is, "m = R; m *= T"
//    will translate, then rotate.
//

#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/rotation.h"
#include "ion/math/vector.h"

namespace ion {
namespace math {

//-----------------------------------------------------------------------------
// Transforming vectors and points.
//-----------------------------------------------------------------------------

// Multiplies a Matrix and a column Vector of one smaller Dimension (the
// template parameter) to produce another column Vector. This assumes the
// homogeneous coordinate of the Vector is 0, so any translation component of
// the Matrix is ignored.
template <int Dimension, typename T>
const Vector<Dimension, T> operator*(const Matrix<Dimension + 1, T>& m,
                                     const Vector<Dimension, T>& v) {
  Vector<Dimension, T> result = Vector<Dimension, T>::Zero();
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result[row] += m(row, col) * v[col];
  }
  return result;
}

// Multiplies a Matrix and a Point of one smaller Dimension (the template
// parameter) to produce another Point. This assumes the homogeneous coordinate
// of the Point is and stays 1 after the transformation. Thus this will include
// translation. but not divide by the homogeneous coordinate; use
// ProjectPoint(), below, for general projections.
template <int Dimension, typename T>
const Point<Dimension, T> operator*(const Matrix<Dimension + 1, T>& m,
                                    const Point<Dimension, T>& p) {
  Point<Dimension, T> result = Point<Dimension, T>::Zero();
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result[row] += m(row, col) * p[col];
    result[row] += m(row, Dimension);  // Homogeneous coordinate.
  }
  return result;
}

// Multiplies a Matrix and a Point of one smaller Dimension (the template
// parameter) to produce another Point and projects it by dividing by the
// homogeneous coordinate. This assumes that the input Point has a homogeneous
// coordinate of 1.
template <int Dimension, typename T>
const Point<Dimension, T> ProjectPoint(const Matrix<Dimension + 1, T>& m,
                                       const Point<Dimension, T>& p) {
  Point<Dimension, T> result = Point<Dimension, T>::Zero();
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result[row] += m(row, col) * p[col];
    result[row] += m(row, Dimension);
  }

  T homogeneous_coordinate = m(Dimension, Dimension);
  for (int col = 0; col < Dimension; ++col)
    homogeneous_coordinate += m(Dimension, col) * p[col];

  // Project the point by dividing by the homogeneous coordinate.
  if (homogeneous_coordinate != T(0))
    result /= homogeneous_coordinate;
  return result;
}

//-----------------------------------------------------------------------------
// Homogeneous matrices.
//-----------------------------------------------------------------------------
// Returns the upper left 3x3 matrix of a homogeneous 4x4 matrix.
template <typename T>
inline Matrix<3, T> NonhomogeneousSubmatrixH(const Matrix<4, T>& m) {
  return Matrix<3, T>(m(0, 0), m(0, 1), m(0, 2),
                      m(1, 0), m(1, 1), m(1, 2),
                      m(2, 0), m(2, 1), m(2, 2));
}

// Returns the inverse of m iff m is orthogonal. Triggers a DCHECK otherwise.
// This function is much faster than the regular Inverse function as it only
// performs one matrix-vector multiplication and a few element permutations.
template <typename T>
const Matrix<4, T> OrthoInverseH(const Matrix<4, T>& m);

//-----------------------------------------------------------------------------
// Affine transformation matrices.
//-----------------------------------------------------------------------------

// Returns a Matrix that represents a translation by a Vector or Point. The
// Dimension template parameter (which is the dimension of the Vector or Point)
// is one less than the dimension of the matrix, meaning the matrix has
// homogeneous coordinates.
template <int Dimension, typename T>
const Matrix<Dimension + 1, T> TranslationMatrix(
    const VectorBase<Dimension, T>& t) {
  typedef Matrix<Dimension + 1, T> MatrixType;
  MatrixType result = MatrixType::Identity();
  for (int row = 0; row < Dimension; ++row)
    result[row][Dimension] = t[row];
  return result;
}

// Returns a Matrix representing a scale by the factors in a Vector whose
// Dimension is one less than the Dimension of the Matrix. This creates a
// Matrix that works with homogeneous coordinates, so the function name ends in
// "H".
template <int Dimension, typename T>
const Matrix<Dimension + 1, T> ScaleMatrixH(const Vector<Dimension, T>& s) {
  Matrix<Dimension + 1, T> result;
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension + 1; ++col)
      result[row][col] = row == col ? s[row] : static_cast<T>(0);
  }
  // Last row is all zeroes except for the last element.
  for (int col = 0; col < Dimension; ++col)
    result[Dimension][col] = static_cast<T>(0);
  result[Dimension][Dimension] = static_cast<T>(1);
  return result;
}

// Returns a Matrix representing a scale by the factors in a Vector, which is
// the same Dimension as the Matrix. This creates a Matrix that does not work
// with homogeneous coordinates, so the function name ends in "NH".
template <int Dimension, typename T>
const Matrix<Dimension, T> ScaleMatrixNH(const Vector<Dimension, T>& s) {
  Matrix<Dimension, T> result;
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result[row][col] = row == col ? s[row] : static_cast<T>(0);
  }
  return result;
}

// Returns a 4x4 Matrix representing a 3D rotation. This creates a Matrix that
// works with homogeneous coordinates, so the function name ends in "H".
template <typename T> ION_API
const Matrix<4, T> RotationMatrixH(const Rotation<T>& r);

// Returns a 3x3 Matrix representing a 3D rotation. This creates a Matrix that
// does not work with homogeneous coordinates, so the function name ends in
// "NH".
template <typename T> ION_API
const Matrix<3, T> RotationMatrixNH(const Rotation<T>& r);

// Returns a 4x4 Matrix representing a 3D rotation specified as axis and angle.
// This creates a Matrix that works with homogeneous coordinates, so the
// function name ends in "H".
template <typename T>
const Matrix<4, T> RotationMatrixAxisAngleH(const Vector<3, T>& axis,
                                            const Angle<T>& angle) {
  return RotationMatrixH(Rotation<T>::FromAxisAndAngle(axis, angle));
}

// Returns a 3x3 Matrix representing a 3D rotation specified as axis and angle.
// This creates a Matrix that does not work with homogeneous coordinates, so
// the function name ends in "NH".
template <typename T>
const Matrix<3, T> RotationMatrixAxisAngleNH(const Vector<3, T>& axis,
                                             const Angle<T>& angle) {
  return RotationMatrixNH(Rotation<T>::FromAxisAndAngle(axis, angle));
}

//-----------------------------------------------------------------------------
// View matrices.
//-----------------------------------------------------------------------------

// Returns a 4x4 viewing matrix based on the given camera parameters, which
// follow the conventions of the old gluLookAt() function (eye aka camera points
// at center aka look_at with camera roll defined by up). If the parameters
// cannot form an orthonormal basis then this returns an identity matrix.
template <typename T> ION_API
const Matrix<4, T> LookAtMatrixFromCenter(
    const Point<3, T>& eye, const Point<3, T>& center, const Vector<3, T>& up);

// Returns a 4x4 viewing matrix based on the given camera parameters, which
// use a view direction rather than look at center point. If the parameters
// cannot form an orthonormal basis then this returns an identity matrix.
template <typename T> ION_API
const Matrix<4, T> LookAtMatrixFromDir(
    const Point<3, T>& eye, const Vector<3, T>& dir, const Vector<3, T>& up);

//-----------------------------------------------------------------------------
// Projection matrices.
//-----------------------------------------------------------------------------

// Returns a 4x4 orthographic projection matrix based on the given parameters,
// which follow the conventions of the old glOrtho() function. If there are
// any problems with the parameters (such as 0 sizes in any dimension), this
// returns an identity matrix.
template <typename T> ION_API
const Matrix<4, T> OrthographicMatrixFromFrustum(
    T x_left, T x_right, T y_bottom, T y_top, T z_near, T z_far);

// Returns a 4x4 perspective projection matrix based on the given parameters,
// which follow the conventions of the old glFrustum() function. If there are
// any problems with the parameters (such as 0 sizes in any dimension or
// non-positive near or far values), this returns an identity matrix.
template <typename T> ION_API
const Matrix<4, T> PerspectiveMatrixFromFrustum(
    T x_left, T x_right, T y_bottom, T y_top, T z_near, T z_far);

// Returns a 4x4 perspective projection matrix based on the given parameters,
// which follow the conventions of the gluPerspective() function. If there are
// any problems with the parameters (such as non-positive values or z_near
// equal to z_far), this returns an identity matrix.
template <typename T> ION_API
const Matrix<4, T> PerspectiveMatrixFromView(const Angle<T>& fovy, T aspect,
                                             T z_near, T z_far);

// Returns the inverse of m iff m is a perspective projection matrix, i.e., iff
// it has the following form:
//
//     [X  0  A  0]
//     [0  Y  B  0]
//     [0  0  C  D]
//     [0  0 -1  0]
//
// Triggers a DCHECK otherwise. This function is much faster than the regular
// Inverse function as it requires only three divisions and three
// multiplications.
template <typename T>
const Matrix<4, T> PerspectiveMatrixInverse(const Matrix<4, T>& m);

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_TRANSFORMUTILS_H_
