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

#include <algorithm>
#include <cmath>
#include <limits>

#include "ion/base/logging.h"
#include "ion/math/angleutils.h"
#include "ion/math/utils.h"
#include "ion/math/vectorutils.h"

namespace ion {
namespace math {

//-----------------------------------------------------------------------------
// Rotation functions.
// -----------------------------------------------------------------------------

template <typename T>
void Rotation<T>::SetAxisAndAngle(const VectorType& axis,
                                  const AngleType& angle) {
  VectorType unit_axis = axis;
  if (!Normalize(&unit_axis)) {
    *this = Identity();
  } else {
    AngleType a = angle / 2;
    const T s = static_cast<T>(Sine(a));
    SetQuaternion(QuaternionType(unit_axis * s, static_cast<T>(Cosine(a))));
  }
}

template <typename T>
Rotation<T> Rotation<T>::FromRotationMatrix(const Matrix<3, T>& mat) {
  static const T kOne = static_cast<T>(1.0);
  static const T kFour = static_cast<T>(4.0);

  const T d0 = mat(0, 0), d1 = mat(1, 1), d2 = mat(2, 2);
  const T ww = kOne + d0 + d1 + d2;
  const T xx = kOne + d0 - d1 - d2;
  const T yy = kOne - d0 + d1 - d2;
  const T zz = kOne - d0 - d1 + d2;

  const T max = std::max(ww, std::max(xx, std::max(yy, zz)));
  if (ww == max) {
    const T w4 = Sqrt(ww * kFour);
    return Rotation::FromQuaternion(QuaternionType(
        (mat(2, 1) - mat(1, 2)) / w4,
        (mat(0, 2) - mat(2, 0)) / w4,
        (mat(1, 0) - mat(0, 1)) / w4,
        w4 / kFour));
  }

  if (xx == max) {
    const T x4 = Sqrt(xx * kFour);
    return Rotation::FromQuaternion(QuaternionType(
        x4 / kFour,
        (mat(0, 1) + mat(1, 0)) / x4,
        (mat(0, 2) + mat(2, 0)) / x4,
        (mat(2, 1) - mat(1, 2)) / x4));
  }

  if (yy == max) {
    const T y4 = Sqrt(yy * kFour);
    return Rotation::FromQuaternion(QuaternionType(
        (mat(0, 1) + mat(1, 0)) / y4,
        y4 / kFour,
        (mat(1, 2) + mat(2, 1)) / y4,
        (mat(0, 2) - mat(2, 0)) / y4));
  }

  // zz is the largest component.
  const T z4 = Sqrt(zz * kFour);
  return Rotation::FromQuaternion(QuaternionType(
      (mat(0, 2) + mat(2, 0)) / z4,
      (mat(1, 2) + mat(2, 1)) / z4,
      z4 / kFour,
      (mat(1, 0) - mat(0, 1)) / z4));
}

template <typename T>
void Rotation<T>::GetAxisAndAngle(VectorType* axis, AngleType* angle) const {
  DCHECK(axis);
  DCHECK(angle);
  if (IsIdentity()) {
    *axis = VectorType(1, 0, 0);
    *angle = AngleType();
  } else {
    DCHECK_NE(quat_[3], static_cast<T>(1));
    *angle = 2 * ArcCosine(quat_[3]);
    const T s = static_cast<T>(1) / Sqrt(static_cast<T>(1) - Square(quat_[3]));
    *axis = VectorType(quat_[0], quat_[1], quat_[2]) * s;
  }
}

template <typename T>
void Rotation<T>::GetRollPitchYaw(AngleType* roll, AngleType* pitch,
                                  AngleType* yaw) const {
  DCHECK(yaw);
  DCHECK(pitch);
  DCHECK(roll);
  const T& qx = quat_[0];
  const T& qy = quat_[1];
  const T& qz = quat_[2];
  const T& qw = quat_[3];

  const T test = qz * qy + qx * qw;
  if (test > static_cast<T>(0.5) - std::numeric_limits<T>::epsilon()) {
    // There is a singularity when the pitch is directly up, so calculate the
    // angles another way.
    *yaw = AngleType::FromRadians(static_cast<T>(2. * atan2(qz, qw)));
    *pitch = AngleType::FromRadians(static_cast<T>(M_PI_2));
    *roll = AngleType::FromRadians(static_cast<T>(0.));
  } else if (test < static_cast<T>(-0.5) + std::numeric_limits<T>::epsilon()) {
    // There is a singularity when the pitch is directly down, so calculate the
    // angles another way.
    *yaw = AngleType::FromRadians(static_cast<T>(-2. * atan2(qz, qw)));
    *pitch = AngleType::FromRadians(static_cast<T>(-M_PI_2));
    *roll = AngleType::FromRadians(static_cast<T>(0.));
  } else {
    // There is no singularity, so calculate angles normally.
    *yaw = AngleType::FromRadians(static_cast<T>(atan2(
        2. * qy * qw - 2. * qz * qx, 1. - 2. * qy * qy - 2. * qx * qx)));
    *pitch = AngleType::FromRadians(static_cast<T>(asin(
        2. * test)));
    *roll = AngleType::FromRadians(static_cast<T>(atan2(
        2. * qz * qw - 2. * qy * qx , 1. - 2. * qz * qz - 2. * qx * qx)));
  }
}

template <typename T>
void Rotation<T>::GetYawPitchRoll(AngleType* yaw, AngleType* pitch,
                                  AngleType* roll) const {
  DCHECK(yaw);
  DCHECK(pitch);
  DCHECK(roll);

  // Rotate vector <0, 0, -1> by the quaternion v' = q * v * q_conjugate, so let
  //   v' = q * v * qc
  //   v  = 0 + 0i + 0j -  k
  //   q  = w + xi + yj + zk
  //   qc = w - xi - yj - zk
  //
  // Which means:
  //   v * qc = (-k) * (w - xi - yj - zk)
  //          = -wk + xki + ykj + zkk
  //          = -wk +  xj -  yi - z
  //          = -z  -  yi + xj  - wk
  //
  // And:
  //   q * v * qc = (w + xi + yj + zk) * (-z - yi + xj - wk)
  //              = -  wz  -  wyi +  wxj -  wwk
  //                - xiz  - xiyi + xixj - xiwk
  //                - yjz  - yjyi + yjxj - yjwk
  //                - zkz  - zkyi + zkxj - zkwk
  //
  // By the properties of i, j, and k (i.e. ii = jj = kk = ijk = -1;
  //                                        ij =  k; jk =  i; ki =  j;
  //                                        ji = -k; kj = -i; ik = -j;):
  //   q * v * qc = - wz  - wyi + wxj - wwk
  //                - xzi + xy  + xxk + xwj
  //                - yzj + yyk - yx  - ywi
  //                - zzk - zyj - zxi + zw
  //
  // Grouping by quaternion unit and simplifying gives us:
  //   q * v * qc = - wz  + xy  - yx  + zw
  //                - wyi - xzi - ywi - zxi
  //                + wxj + xwj - yzj - zyj
  //                - wwk + xxk + yyk - zzk
  //              = 0
  //                - 2(xz + wy)i
  //                + 2(wx - yz)j
  //                (- ww + xx + yy - zz)k
  const T& qx = quat_[0];
  const T& qy = quat_[1];
  const T& qz = quat_[2];
  const T& qw = quat_[3];

  const T vx = -2 * (qx * qz + qw * qy);
  const T vy = 2 * (qw * qx - qy * qz);
  const T vz = (-qw * qw + qx * qx + qy * qy - qz * qz);

  if (vy > static_cast<T>(1.) - std::numeric_limits<T>::epsilon()) {
    *yaw = AngleType::FromRadians(static_cast<T>(2. * atan2(qz, qw)));
    *pitch = AngleType::FromRadians(static_cast<T>(M_PI_2));
    *roll = AngleType::FromRadians(static_cast<T>(0.));
  } else if (vy < static_cast<T>(-1.) + std::numeric_limits<T>::epsilon()) {
    *yaw = AngleType::FromRadians(static_cast<T>(-2. * atan2(qz, qw)));
    *pitch = AngleType::FromRadians(static_cast<T>(-M_PI_2));
    *roll = AngleType::FromRadians(static_cast<T>(0.));
  } else {
    *yaw = AngleType::FromRadians(static_cast<T>(atan2(-vx, -vz)));
    *pitch = AngleType::FromRadians(static_cast<T>(asin(vy)));
    *roll = AngleType::FromRadians(static_cast<T>(
        atan2(2. * qw * qz + 2. * qx * qy, 1. - 2. * qx * qx - 2. * qz * qz)));
  }
}

template <typename T>
Rotation<T> Rotation<T>::RotateInto(const VectorType& from,
                                    const VectorType& to) {
  static const T kTolerance = std::numeric_limits<T>::epsilon() * 100;

  // Directly build the quaternion using the following technique:
  // http://lolengine.net/blog/2014/02/24/quaternion-from-two-vectors-final
  const T norm_u_norm_v = Sqrt(LengthSquared(from) * LengthSquared(to));
  T real_part = norm_u_norm_v + Dot(from, to);
  VectorType w;
  if (real_part < kTolerance * norm_u_norm_v) {
    // If |from| and |to| are exactly opposite, rotate 180 degrees around an
    // arbitrary orthogonal axis. Axis normalization can happen later, when we
    // normalize the quaternion.
    real_part = 0.0;
    w = (Abs(from[0]) > Abs(from[2])) ?
        VectorType(-from[1], from[0], 0) :
        VectorType(0, -from[2], from[1]);
  } else {
    // Otherwise, build the quaternion the standard way.
    w = Cross(from, to);
  }

  // Build and return a normalized quaternion.
  // Note that Rotation::FromQuaternion automatically performs normalization.
  return Rotation::FromQuaternion(QuaternionType(w[0], w[1], w[2], real_part));
}

template <typename T>
Rotation<T> Rotation<T>::Slerp(const Rotation& r0,
                               const Rotation& r1, T t) {
  const QuaternionType& q0 = r0.GetQuaternion();
  QuaternionType q1 = r1.GetQuaternion();
  QuaternionType q_result;

  // Compute the cosine of the angle between the quaternions and clamp it for
  // arithmetic robustness.
  T dot = Clamp(Dot(q0, q1), static_cast<T>(-1), static_cast<T>(1));

  // We should be robust to the case where our two rotations fall on opposite
  // sides of the quaternionic unit sphere.
  if (dot < static_cast<T>(0.0)) {
    dot = -dot;
    q1 = -q1;
  }

  // If the quaternions are too similar, just use linear interpolation.
  static const T kMinDotForSlerp = static_cast<T>(1.0 - 1e-5);
  if (dot > kMinDotForSlerp) {
    q_result = q0 + t * (q1 - q0);
  } else {
    // Compute theta, the angle between q0 and the result quaternion.
    const Angle<T> theta = t * ArcCosine(dot);
    const QuaternionType q2 = Normalized(q1 - q0 * dot);

    // q0 and q2 now form an orthonormal basis; interpolate using it.
    q_result = q0 * Cosine(theta) + q2 * Sine(theta);
  }
  Rotation<T> result;
  result.SetQuaternion(q_result);
  return result;
}

//-----------------------------------------------------------------------------
// Explicit instantiations.
// If you add any instantiations, please also add explicit instantiation
// declarations to the section in rotations.h. Otherwise, ClangTidy may complain
// when people try to use these templates. (See
// http://g3doc/devtools/cymbal/clang_tidy/g3doc/checks/clang-diagnostic-undefined-func-template.md)
// -----------------------------------------------------------------------------

#define ION_INSTANTIATE_ROTATION_FUNCTIONS(type)                      \
  template void ION_API Rotation<type>::SetAxisAndAngle(              \
      const Vector<3, type>& axis, const Angle<type>& angle);         \
  template void ION_API Rotation<type>::GetAxisAndAngle(              \
      Vector<3, type>* axis, Angle<type>* angle) const;               \
  template void ION_API Rotation<type>::GetYawPitchRoll(              \
      Angle<type>* yaw, Angle<type>* pitch, Angle<type>* roll) const; \
  template void ION_API Rotation<type>::GetRollPitchYaw(              \
      Angle<type>* roll, Angle<type>* pitch, Angle<type>* yaw) const; \
  template void ION_API Rotation<type>::GetEulerAngles(               \
      Angle<type>* yaw, Angle<type>* pitch, Angle<type>* roll) const; \
  template Rotation<type> ION_API Rotation<type>::RotateInto(         \
      const Vector<3, type>& from, const Vector<3, type>& to);        \
  template Rotation<type> ION_API Rotation<type>::Slerp(              \
      const Rotation& r0, const Rotation& r1, type t);                \
  template Rotation<type> ION_API Rotation<type>::FromRotationMatrix( \
      const Matrix<3, type>& mat)

ION_INSTANTIATE_ROTATION_FUNCTIONS(double);  // NOLINT; thinks it's a function.
ION_INSTANTIATE_ROTATION_FUNCTIONS(float);   // NOLINT; thinks it's a function.

#undef ION_INSTANTIATE_ROTATION_FUNCTIONS

}  // namespace math
}  // namespace ion
