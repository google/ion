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

#include <cmath>

#include "ion/base/logging.h"
#include "ion/base/static_assert.h"
#include "ion/math/angleutils.h"
#include "ion/math/matrixutils.h"
#include "ion/math/utils.h"
#include "ion/math/vectorutils.h"

namespace ion {
namespace math {

//-----------------------------------------------------------------------------
// Internal RotationMatrix helper functions.
// -----------------------------------------------------------------------------

namespace {

// Epsilon for floating-point precision error.
static const float EPSILON = 1e-8f;

// Sets the upper 3x3 of a Matrix to represent a 3D rotation.
template <int Dimension, typename T>
void RotationMatrix3x3(const Rotation<T>& r, Matrix<Dimension, T>* matrix) {
  ION_STATIC_ASSERT(Dimension >= 3, "Bad Dimension in RotationMatrix3x3");
  DCHECK(matrix);

  //
  // Given a quaternion (a,b,c,d) where d is the scalar part, the 3x3 rotation
  // matrix is:
  //
  //   a^2 - b^2 - c^2 + d^2         2ab - 2cd               2ac + 2bd
  //         2ab + 2cd        -a^2 + b^2 - c^2 + d^2         2bc - 2ad
  //         2ac - 2bd               2bc + 2ad        -a^2 - b^2 + c^2 + d^2
  //
  const Vector<4, T>& quat = r.GetQuaternion();
  const T aa = Square(quat[0]);
  const T bb = Square(quat[1]);
  const T cc = Square(quat[2]);
  const T dd = Square(quat[3]);

  const T ab = quat[0] * quat[1];
  const T ac = quat[0] * quat[2];
  const T bc = quat[1] * quat[2];

  const T ad = quat[0] * quat[3];
  const T bd = quat[1] * quat[3];
  const T cd = quat[2] * quat[3];

  Matrix<Dimension, T>& m = *matrix;
  m[0][0] = aa - bb - cc + dd;
  m[0][1] = 2 * ab - 2 * cd;
  m[0][2] = 2 * ac + 2 * bd;
  m[1][0] = 2 * ab + 2 * cd;
  m[1][1] = -aa + bb - cc + dd;
  m[1][2] = 2 * bc - 2 * ad;
  m[2][0] = 2 * ac - 2 * bd;
  m[2][1] = 2 * bc + 2 * ad;
  m[2][2] = -aa - bb + cc + dd;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------

template <typename T>
Matrix<4, T> OrthoInverseH(const Matrix<4, T>& m) {
  static const T kZero = static_cast<T>(0);
  static const T kOne = static_cast<T>(1);
  static const Vector<4, T> kZeroZeroZeroOne(kZero, kZero, kZero, kOne);

  // Verify assumptions. Use a tolerance slightly larger than epsilon to allow
  // for slight de-orthogonalization during matrix multiplication chains.
  Matrix<3, T> nh = NonhomogeneousSubmatrixH(m);
  DCHECK(MatrixAlmostOrthogonal(nh, std::numeric_limits<T>::epsilon() *
                                    static_cast<T>(10)))
      << "Non-orthogonal matrix received in OrthoInverseH: " << m;
  DCHECK(VectorsAlmostEqual(kZeroZeroZeroOne, Row(m, 3),
                            std::numeric_limits<T>::epsilon()))
      << "Invalid 4th row in OrthoInverseH: " << Row(m, 3);

  // Using blockwise inversion, one can show that the following holds:
  //
  //     A = | M   v |     inv(A) = | inv(M)  -inv(M) * b |
  //         | 0   1 |              |   0          1      |
  //
  // where v is any column vector, and 0 is a zero row vector.
  // In addition, if A is orthogonal, we know that inv(A) = transpose(A).
  //
  Vector<3, T> translation =
      -(Transpose(nh) * Vector<3, T>(m(0, 3), m(1, 3), m(2, 3)));
  return Matrix<4, T>(m(0, 0), m(1, 0), m(2, 0), translation[0],
                      m(0, 1), m(1, 1), m(2, 1), translation[1],
                      m(0, 2), m(1, 2), m(2, 2), translation[2],
                      kZero, kZero, kZero, kOne);
}

template <typename T>
ION_API Matrix<4, T> RotationMatrixH(const Rotation<T>& r) {
  Matrix<4, T> m;
  RotationMatrix3x3(r, &m);
  for (int i = 0; i < 3; ++i)
    m[i][3] = m[3][i] = static_cast<T>(0);
  m[3][3] = static_cast<T>(1);
  return m;
}

template <typename T>
ION_API Matrix<3, T> RotationMatrixNH(const Rotation<T>& r) {
  Matrix<3, T> m;
  RotationMatrix3x3(r, &m);
  return m;
}

template <typename T>
ION_API Matrix<4, T> LookAtMatrixFromCenter(const Point<3, T>& eye,
                                            const Point<3, T>& center,
                                            const Vector<3, T>& up) {
  const Vector<3, T> dir = center - eye;
  // dir will be normalized below.
  return LookAtMatrixFromDir(eye, dir, up);
}

template <typename T>
ION_API Matrix<4, T> LookAtMatrixFromDir(const Point<3, T>& eye,
                                         const Vector<3, T>& dir,
                                         const Vector<3, T>& up) {
  // Check for degenerate cases.
  DCHECK(eye == eye);
  DCHECK(dir == dir);
  DCHECK(up == up);
  DCHECK_GE(LengthSquared(Cross(dir, up)),
            std::numeric_limits<T>::epsilon())
      << "LookAtMatrixFromDir received front and up vectors that have "
      << "either zero length or are parallel to each other. "
      << "[dir: " << dir << " up: " << up << "]";

  const Vector<3, T> front = Normalized(dir);
  const Vector<3, T> right = Normalized(Cross(front, up));
  const Vector<3, T> new_up = Normalized(Cross(right, front));
  math::Matrix<4, T> mat(right[0], right[1], right[2], 0,
                         new_up[0], new_up[1], new_up[2], 0,
                         -front[0], -front[1], -front[2], 0,
                         0, 0, 0, 1);

  return mat * TranslationMatrix(-eye);
}

template <typename T>
ION_API Matrix<4, T> OrthographicMatrixFromFrustum(T x_left, T x_right,
                                                   T y_bottom, T y_top,
                                                   T z_near, T z_far) {
  if (x_left == x_right || y_bottom == y_top || z_near == z_far) {
    return Matrix<4, T>::Identity();
  }

  const T X = 2 / (x_right - x_left);
  const T Y = 2 / (y_top - y_bottom);
  const T Z = 2 / (z_near - z_far);
  const T A = (x_right + x_left) / (x_left - x_right);
  const T B = (y_top + y_bottom) / (y_bottom - y_top);
  const T C = (z_near + z_far) / (z_near - z_far);

  return math::Matrix<4, T>(X, 0, 0, A,
                            0, Y, 0, B,
                            0, 0, Z, C,
                            0, 0, 0, 1);
}

template <typename T>
ION_API Matrix<4, T> PerspectiveMatrixFromFrustum(T x_left, T x_right,
                                                  T y_bottom, T y_top, T z_near,
                                                  T z_far) {
  const T zero = static_cast<T>(0);
  if (x_left == x_right || y_bottom == y_top || z_near == z_far ||
      z_near <= zero || z_far <= zero) {
    return Matrix<4, T>::Identity();
  }

  const T X = (2 * z_near) / (x_right - x_left);
  const T Y = (2 * z_near) / (y_top - y_bottom);
  const T A = (x_right + x_left) / (x_right - x_left);
  const T B = (y_top + y_bottom) / (y_top - y_bottom);
  const T C = (z_near + z_far) / (z_near - z_far);
  const T D = (2 * z_near * z_far) / (z_near - z_far);

  return math::Matrix<4, T>(X, 0, A, 0,
                            0, Y, B, 0,
                            0, 0, C, D,
                            0, 0, -1, 0);
}

template <typename T>
ION_API Matrix<4, T> PerspectiveMatrixFromInfiniteFrustum(T x_left, T x_right,
                                                  T y_bottom, T y_top, T z_near,
                                                  T z_far_epsilon) {
  const T zero = static_cast<T>(0);
  if (x_left == x_right || y_bottom == y_top || z_near <= zero) {
    return Matrix<4, T>::Identity();
  }

  // For derivation, see for example:
  // Lengyel, E. "Projection Matrix Tricks." Game Developers Conference
  // Proceedings, 2007. http://www.terathon.com/gdc07_lengyel.pdf.
  const T X = (2 * z_near) / (x_right - x_left);
  const T Y = (2 * z_near) / (y_top - y_bottom);
  const T A = (x_right + x_left) / (x_right - x_left);
  const T B = (y_top + y_bottom) / (y_top - y_bottom);
  const T C = -1 + z_far_epsilon;
  const T D = (-2 + z_far_epsilon) * z_near;

  return math::Matrix<4, T>(X, 0, A, 0,
                            0, Y, B, 0,
                            0, 0, C, D,
                            0, 0, -1, 0);
}

template <typename T>
ION_API Matrix<4, T> PerspectiveMatrixFromView(const Angle<T>& fovy, T aspect,
                                               T z_near, T z_far) {
  const T zero = static_cast<T>(0);
  if (fovy.Radians() <= zero || aspect <= zero ||
      z_near <= zero || z_far <= zero || z_near == z_far) {
    return Matrix<4, T>::Identity();
  }

  const T tan_fov = Tangent(fovy / 2) * z_near;
  const T x_left = -tan_fov * aspect;
  const T x_right = tan_fov * aspect;
  const T y_bottom = -tan_fov;
  const T y_top = tan_fov;
  return PerspectiveMatrixFromFrustum<T>(
      x_left, x_right, y_bottom, y_top, z_near, z_far);
}

template <typename T>
ION_API Matrix<4, T> PerspectiveMatrixInverse(const Matrix<4, T>& m) {
  static const T kZero = static_cast<T>(0);
  static const T kOne = static_cast<T>(1);
  // We assume that the matrix M has the following form:
  //
  //         [X  0  A  0]                   [1/X  0   0   A/X]
  //         [0  Y  B  0]                   [ 0  1/Y  0   B/Y]
  //     M = [0  0  C  D]     with inv(M) = [ 0   0   0   -1 ]
  //         [0  0 -1  0]                   [ 0   0  1/D  C/D]
  //
  // Verify that M has the assumed form.
  DCHECK_LE(fabs(m(0, 1)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(0, 3)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(1, 0)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(1, 3)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(2, 0)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(2, 1)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(3, 0)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(3, 1)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(3, 3)), std::numeric_limits<T>::epsilon());
  DCHECK_LE(fabs(m(3, 2) + kOne), std::numeric_limits<T>::epsilon());

  // Now compute its inverse.
  const T a = m(0, 2);
  const T b = m(1, 2);
  const T c = m(2, 2);
  const T inv_d = kOne / m(2, 3);
  const T inv_x = kOne / m(0, 0);
  const T inv_y = kOne / m(1, 1);
  return math::Matrix<4, T>(inv_x, kZero, kZero, a * inv_x,
                            kZero, inv_y, kZero, b * inv_y,
                            kZero, kZero, kZero, -kOne,
                            kZero, kZero, inv_d, c * inv_d);
}

template <typename T>
ION_API Matrix<4, T> Interpolate(const Matrix<4, T>& from,
                                 const Matrix<4, T>& to, float percentage) {
  if (percentage <= EPSILON) {
    return from;
  } else if (percentage >= 1.0f - EPSILON) {
    return to;
  }

  Vector<3, T> from_translation = GetTranslationVector<4, T>(from);
  Vector<3, T> to_translation = GetTranslationVector<4, T>(to);

  Rotation<T> from_rotation =
      Rotation<T>::FromRotationMatrix(GetRotationMatrix<4, T>(from));
  Rotation<T> to_rotation =
      Rotation<T>::FromRotationMatrix(GetRotationMatrix<4, T>(to));

  Vector<3, T> from_scale = GetScaleVector<4, T>(from);
  Vector<3, T> to_scale = GetScaleVector<4, T>(to);

  Vector<3, T> new_translation;
  Vector<3, T> new_scale;
  for (int i = 0; i < 3; ++i) {
    new_translation[i] =
        Lerp(from_translation[i], to_translation[i], percentage);
    new_scale[i] = Lerp(from_scale[i], to_scale[i], percentage);
  }

  Rotation<T> new_rotation =
      Rotation<T>::Slerp(from_rotation, to_rotation, percentage);

  return TranslationMatrix(new_translation) * RotationMatrixH(new_rotation) *
         ScaleMatrixH(new_scale);
}

//-----------------------------------------------------------------------------
// Instantiate functions for supported types.
// If you add any instantiations, please also add explicit instantiation
// declarations to the section in transformutils.h. Otherwise, ClangTidy may
// complain when people try to use these templates. (See
// http://g3doc/devtools/cymbal/clang_tidy/g3doc/checks/clang-diagnostic-undefined-func-template.md)
// -----------------------------------------------------------------------------

#define ION_INSTANTIATE_FUNCTIONS(type)                                       \
  template Matrix<4, type> ION_API OrthoInverseH(const Matrix<4, type>& r);   \
  template Matrix<4, type> ION_API RotationMatrixH(const Rotation<type>& r);  \
  template Matrix<3, type> ION_API RotationMatrixNH(const Rotation<type>& r); \
  template Matrix<4, type> ION_API LookAtMatrixFromCenter(                    \
      const Point<3, type>& eye, const Point<3, type>& center,                \
      const Vector<3, type>& up);                                             \
  template Matrix<4, type> ION_API LookAtMatrixFromDir(                       \
      const Point<3, type>& eye, const Vector<3, type>& dir,                  \
      const Vector<3, type>& up);                                             \
  template Matrix<4, type> ION_API OrthographicMatrixFromFrustum(             \
      type x_left, type x_right, type y_bottom, type y_top, type z_near,      \
      type z_far);                                                            \
  template Matrix<4, type> ION_API PerspectiveMatrixFromFrustum(              \
      type x_left, type x_right, type y_bottom, type y_top, type z_near,      \
      type z_far);                                                            \
  template Matrix<4, type> ION_API PerspectiveMatrixFromInfiniteFrustum(      \
      type x_left, type x_right, type y_bottom, type y_top, type z_near,      \
      type z_far_epsilon);                                                    \
  template Matrix<4, type> ION_API PerspectiveMatrixFromView(                 \
      const Angle<type>& fovy, type aspect, type z_near, type z_far);         \
  template Matrix<4, type> ION_API PerspectiveMatrixInverse(                  \
      const Matrix<4, type>& m);                                              \
  template Matrix<4, type> ION_API Interpolate(const Matrix<4, type>& from,   \
                                               const Matrix<4, type>& to,     \
                                               float percentage)

ION_INSTANTIATE_FUNCTIONS(double);  // NOLINT
ION_INSTANTIATE_FUNCTIONS(float);   // NOLINT

#undef ION_INSTANTIATE_FUNCTIONS

}  // namespace math
}  // namespace ion
