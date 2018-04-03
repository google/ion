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

#ifndef ION_MATH_ROTATION_H_
#define ION_MATH_ROTATION_H_

#include <istream>  // NOLINT
#include <ostream>  // NOLINT
#include <string>

#include "ion/base/stringutils.h"
#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"

namespace ion {
namespace math {

// The Rotation class represents a rotation around a 3-dimensional axis. It
// uses normalized quaternions internally to make the math robust.
template <typename T>
class Rotation {
 public:
  // Convenience typedefs for Angle and 3D vector of the correct type.
  typedef Angle<T> AngleType;
  typedef Vector<3, T> VectorType;
  typedef Vector<4, T> QuaternionType;

  // The default constructor creates an identity Rotation, which has no effect.
  Rotation() {
    quat_.Set(static_cast<T>(0), static_cast<T>(0), static_cast<T>(0),
              static_cast<T>(1));
  }

  // Copy constructor from an instance of any value type that is compatible (via
  // static_cast) with this instance's type.
  template <typename U>
  explicit Rotation(const Rotation<U> other)
      : quat_(other.GetQuaternion()) {}

  // Returns an identity Rotation, which has no effect.
  static Rotation Identity() {
    return Rotation();
  }

  // Returns true if this represents an identity Rotation.
  bool IsIdentity() const {
    return quat_[3] == static_cast<T>(1) ||
           quat_[3] == static_cast<T>(-1);
  }

  // Sets the Rotation from a quaternion (4D vector), which is first normalized.
  void SetQuaternion(const QuaternionType& quaternion) {
    quat_ = Normalized(quaternion);
  }

  // Returns the Rotation as a normalized quaternion (4D vector).
  const QuaternionType& GetQuaternion() const { return quat_; }

  // Sets the Rotation to rotate by the given angle around the given axis,
  // following the right-hand rule. The axis does not need to be unit
  // length. If it is zero length, this results in an identity Rotation.
  void SetAxisAndAngle(const VectorType& axis, const AngleType& angle);

  // Returns the right-hand rule axis and angle corresponding to the
  // Rotation. If the Rotation is the identity rotation, this returns the +X
  // axis and an angle of 0.
  void GetAxisAndAngle(VectorType* axis, AngleType* angle) const;

  // Returns the Euler angles which would result in this rotation if done in
  // the order of (rotate-Z by roll) * (rotate-X by pitch) * (rotate-Y by yaw) *
  // point.
  void GetRollPitchYaw(AngleType* roll, AngleType* pitch, AngleType* yaw) const;

  // Returns the Euler angles which would result in this rotation if done in
  // the order of (rotate-Y by yaw) * (rotate-X by pitch) * (rotate-Z by roll) *
  // point.
  void GetYawPitchRoll(AngleType* yaw, AngleType* pitch, AngleType* roll) const;

  // This function is a legacy alias for GetRollPitchYaw(), which is what
  // GetEulerAngles() actually did despite its documentation saying the
  // decomposition was yaw, pitch, roll order.
  //
  // Where possible, switch to using GetYawPitchRoll() or GetRollPitchYaw().
  void GetEulerAngles(AngleType* yaw, AngleType* pitch, AngleType* roll) const {
    GetRollPitchYaw(roll, pitch, yaw);
  }

  // Convenience function that constructs and returns a Rotation given an axis
  // and angle.
  static Rotation FromAxisAndAngle(const VectorType& axis,
                                   const AngleType& angle) {
    Rotation r;
    r.SetAxisAndAngle(axis, angle);
    return r;
  }

  // Convenience function that constructs and returns a Rotation given a
  // quaternion.
  static Rotation FromQuaternion(const QuaternionType& quat) {
    Rotation r;
    r.SetQuaternion(quat);
    return r;
  }

  // Convenience function that constructs and returns a Rotation given a
  // rotation matrix R with $R^\top R = I && det(R) = 1$.
  static Rotation FromRotationMatrix(const Matrix<3, T>& mat);

  // Convenience function that constructs and returns a Rotation given Euler
  // angles that are applied in the order of rotate-Z by roll, rotate-X by
  // pitch, rotate-Y by yaw (same as GetRollPitchYaw).
  static Rotation FromRollPitchYaw(const AngleType& roll,
                                   const AngleType& pitch,
                                   const AngleType& yaw) {
    VectorType x(1, 0, 0), y(0, 1, 0), z(0, 0, 1);
    return FromAxisAndAngle(z, roll) *
           (FromAxisAndAngle(x, pitch) * FromAxisAndAngle(y, yaw));
  }

  // Convenience function that constructs and returns a Rotation given Euler
  // angles that are applied in the order of rotate-Y by yaw, rotate-X by
  // pitch, rotate-Z by roll (same as GetYawPitchRoll).
  static Rotation FromYawPitchRoll(const AngleType& yaw, const AngleType& pitch,
                                   const AngleType& roll) {
    VectorType x(1, 0, 0), y(0, 1, 0), z(0, 0, 1);
    return FromAxisAndAngle(y, yaw) *
           (FromAxisAndAngle(x, pitch) * FromAxisAndAngle(z, roll));
  }

  // This function is a legacy alias for FromRollPitchYaw(), which is what
  // FromEulerAngles() actually did despite its documentation saying the
  // rotation order was yaw, pitch, roll.
  //
  // Where possible, switch to using FromYawPitchRoll() or FromRollPitchYaw().
  static Rotation FromEulerAngles(
      const AngleType& yaw, const AngleType& pitch, const AngleType& roll) {
    return FromRollPitchYaw(roll, pitch, yaw);
  }

  // Constructs and returns a Rotation that rotates one vector to another along
  // the shortest arc. This returns an identity rotation if either vector has
  // zero length.
  static Rotation RotateInto(const VectorType& from,
                             const VectorType& to);

  // The negation operator returns the inverse rotation.
  friend Rotation operator-(const Rotation& r) {
    // Because we store normalized quaternions, the inverse is found by
    // negating the vector part.
    return Rotation(-r.quat_[0], -r.quat_[1], -r.quat_[2], r.quat_[3]);
  }

  // Appends a rotation to this one.
  Rotation& operator*=(const Rotation& r) {
    const QuaternionType &qr = r.quat_;
    QuaternionType &qt = quat_;
    SetQuaternion(QuaternionType(
        qr[3] * qt[0] + qr[0] * qt[3] + qr[2] * qt[1] - qr[1] * qt[2],
        qr[3] * qt[1] + qr[1] * qt[3] + qr[0] * qt[2] - qr[2] * qt[0],
        qr[3] * qt[2] + qr[2] * qt[3] + qr[1] * qt[0] - qr[0] * qt[1],
        qr[3] * qt[3] - qr[0] * qt[0] - qr[1] * qt[1] - qr[2] * qt[2]));
    return *this;
  }

  // Binary multiplication operator - returns a composite Rotation.
  friend const Rotation operator*(const Rotation& r0, const Rotation& r1) {
    Rotation r = r0;
    r *= r1;
    return r;
  }

  // Multiply a Rotation and a Vector/Point to get a Vector/Point.
  VectorType operator*(const VectorType& v) const;
  Point<3, T> operator*(const Point<3, T>& p) const;

  // Exact equality and inequality comparisons.
  friend bool operator==(const Rotation& v0, const Rotation& v1) {
    return v0.quat_ == v1.quat_ || v0.quat_ == -v1.quat_;
  }
  friend bool operator!=(const Rotation& v0, const Rotation& v1) {
    return v0.quat_ != v1.quat_ && v0.quat_ != -v1.quat_;
  }

  // Performs spherical linear interpolation between two Rotation instances.
  // This returns r0 when t is 0 and r1 when t is 1; all other values of t
  // interpolate appropriately.
  static Rotation Slerp(const Rotation& r0, const Rotation& r1, T t);

 private:
  // Private constructor that builds a Rotation from quaternion components.
  Rotation(T q0, T q1, T q2, T q3) : quat_(q0, q1, q2, q3) {}

  // Applies a Rotation to a Vector to rotate the Vector. Method borrowed from:
  // http://blog.molecular-matters.com/2013/05/24/a-faster-quaternion-vector-multiplication/
  VectorType ApplyToVector(const VectorType& v) const {
    VectorType im(quat_[0], quat_[1], quat_[2]);
    VectorType temp = static_cast<T>(2) * Cross(im, v);
    return v + quat_[3] * temp + Cross(im, temp);
  }

  // The rotation represented as a normalized quaternion. (Unit quaternions are
  // required for constructing rotation matrices, so it makes sense to always
  // store them that way.) The vector part is in the first 3 elements, and the
  // scalar part is in the last element.
  QuaternionType quat_;
};

template <typename T>
typename Rotation<T>::VectorType Rotation<T>::operator*(
    const typename Rotation<T>::VectorType& v) const {
  return ApplyToVector(v);
}

template <typename T>
Point<3, T> Rotation<T>::operator*(const Point<3, T>& p) const {
  return ApplyToVector(p - Point<3, T>::Zero()) + Point<3, T>::Zero();
}

// Prints a Rotation to a stream.
template <typename T>
std::ostream& operator<<(std::ostream& out, const Rotation<T>& a) {
  Vector<3, T> axis;
  Angle<T> angle;
  a.GetAxisAndAngle(&axis, &angle);
  return out << "ROT[" << axis << ": " << angle << "]";
}

// Reads a Rotation from a stream.
template <typename T>
std::istream& operator>>(std::istream& in, Rotation<T>& a) {
  if (base::GetExpectedString(in, "ROT[")) {
    Vector<3, T> axis;
    Angle<T> angle;
    if (in >> axis >> base::GetExpectedChar<':'> >> angle
           >> base::GetExpectedChar<']'>)
      a.SetAxisAndAngle(axis, angle);
  }

  return in;
}

//------------------------------------------------------------------------------
// Type-specific typedefs.
//------------------------------------------------------------------------------

typedef Rotation<float> Rotationf;
typedef Rotation<double> Rotationd;

//------------------------------------------------------------------------------
// Explicit instantiation declarations
// These are to make ClangTidy happy, since the definitions are in a CC file
// instead of this header. (See
// http://g3doc/devtools/cymbal/clang_tidy/g3doc/checks/clang-diagnostic-undefined-func-template.md)
//------------------------------------------------------------------------------

extern template void ION_API
Rotation<float>::SetAxisAndAngle(const Vector<3, float>&, const Angle<float>&);
extern template void ION_API
    Rotation<float>::GetAxisAndAngle(Vector<3, float>*, Angle<float>*) const;
extern template void ION_API Rotation<float>::GetYawPitchRoll(
    Angle<float>*, Angle<float>*, Angle<float>*) const;
extern template void ION_API Rotation<float>::GetRollPitchYaw(
    Angle<float>*, Angle<float>*, Angle<float>*) const;
extern template void ION_API Rotation<float>::GetEulerAngles(
    Angle<float>*, Angle<float>*, Angle<float>*) const;
extern template Rotation<float> ION_API
Rotation<float>::RotateInto(const Vector<3, float>&, const Vector<3, float>&);
extern template Rotation<float> ION_API Rotation<float>::Slerp(const Rotation&,
                                                               const Rotation&,
                                                               float);
extern template Rotation<float> ION_API
Rotation<float>::FromRotationMatrix(const Matrix<3, float>&);

extern template void ION_API Rotation<double>::SetAxisAndAngle(
    const Vector<3, double>&, const Angle<double>&);
extern template void ION_API
    Rotation<double>::GetAxisAndAngle(Vector<3, double>*, Angle<double>*) const;
extern template void ION_API Rotation<double>::GetYawPitchRoll(
    Angle<double>*, Angle<double>*, Angle<double>*) const;
extern template void ION_API Rotation<double>::GetRollPitchYaw(
    Angle<double>*, Angle<double>*, Angle<double>*) const;
extern template void ION_API Rotation<double>::GetEulerAngles(
    Angle<double>*, Angle<double>*, Angle<double>*) const;
extern template Rotation<double> ION_API Rotation<double>::RotateInto(
    const Vector<3, double>&, const Vector<3, double>&);
extern template Rotation<double> ION_API
Rotation<double>::Slerp(const Rotation&, const Rotation&, double);
extern template Rotation<double> ION_API
Rotation<double>::FromRotationMatrix(const Matrix<3, double>&);

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_ROTATION_H_
