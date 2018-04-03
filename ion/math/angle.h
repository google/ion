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

#ifndef ION_MATH_ANGLE_H_
#define ION_MATH_ANGLE_H_

#include <cmath>
#include <istream>  // NOLINT
#include <ostream>  // NOLINT

#include "ion/base/stringutils.h"
#include "ion/math/utils.h"

namespace ion {
namespace math {

// A simple class to represent angles. The fundamental angular unit is radians,
// with conversion provided to and from degrees.
template <typename T>
class Angle {
 public:
  // The default constructor creates an angle of 0 (in any unit).
  Angle() : radians_(0) {}

  // Copy constructor from an instance of any value type that is compatible (via
  // static_cast) with this instance's type.
  template <typename U>
  explicit Angle(const Angle<U> other)
      : radians_(static_cast<T>(other.Radians())) {}

  // Create a angle from radians (no conversion).
  static Angle FromRadians(const T& angle) {
    return Angle(angle);
  }

  // Create a angle from degrees (requires conversion).
  static Angle FromDegrees(const T& angle) {
    return Angle(DegreesToRadians(angle));
  }

  // Get the angle in degrees or radians.
  T Radians() const { return radians_; }
  T Degrees() const {  return RadiansToDegrees(radians_); }

  // Unary negation operator.
  const Angle operator-() const { return Angle::FromRadians(-radians_); }

  // Self-modifying operators.
  void operator+=(const Angle& a) { radians_ += a.radians_; }
  void operator-=(const Angle& a) { radians_ -= a.radians_; }
  void operator*=(T s) { radians_ *= s; }
  void operator/=(T s) { radians_ /= s; }

  // Binary operators.
  friend Angle operator+(const Angle& a0, const Angle& a1) {
    return FromRadians(a0.radians_ + a1.radians_);
  }
  friend Angle operator-(const Angle& a0, const Angle& a1) {
    return FromRadians(a0.radians_ - a1.radians_);
  }
  friend Angle operator*(const Angle& a, T s) {
    return FromRadians(a.radians_ * s);
  }
  friend Angle operator*(T s, const Angle& a) {
    return FromRadians(s * a.radians_);
  }
  friend Angle operator/(const Angle& a, T s) {
    return FromRadians(a.radians_ / s);
  }

  // Exact equality and inequality comparisons.
  friend bool operator==(const Angle& a0, const Angle& a1) {
    return a0.radians_ == a1.radians_;
  }
  friend bool operator!=(const Angle& a0, const Angle& a1) {
    return a0.radians_ != a1.radians_;
  }

  // Comparisons.
  friend bool operator<(const Angle& a0, const Angle& a1) {
    return a0.radians_ < a1.radians_;
  }
  friend bool operator>(const Angle& a0, const Angle& a1) {
    return a0.radians_ > a1.radians_;
  }
  friend bool operator<=(const Angle& a0, const Angle& a1) {
    return a0.radians_ <= a1.radians_;
  }
  friend bool operator>=(const Angle& a0, const Angle& a1) {
    return a0.radians_ >= a1.radians_;
  }

 private:
  explicit Angle(const T angle_rad) {
    radians_ = angle_rad;
  }

  static T RadiansToDegrees(const T& radians) {
    static const T kRadToDeg = 180 / static_cast<T>(M_PI);
    return radians * kRadToDeg;
  }

  static T DegreesToRadians(const T& degrees) {
    static const T kDegToRad = static_cast<T>(M_PI) / 180;
    return degrees * kDegToRad;
  }

  T radians_;
  // DISALLOW_IMPLICIT_CONSTRUCTORS(Angle)
};


// An Angle is streamed as degrees.
template <typename T>
std::ostream& operator<<(std::ostream& out, const Angle<T>& a) {
  return out << +a.Degrees() << " deg";
}

template <typename T>
std::istream& operator>>(std::istream& in, Angle<T>& a) {
  T angle;
  if (in >> angle) {
    if (base::GetExpectedString(in, "deg")) {
      a = Angle<T>::FromDegrees(angle);
    } else {
      in.clear();  // Resets "deg" failure.
      if (base::GetExpectedString(in, "rad"))
        a = Angle<T>::FromRadians(angle);
    }
  }

  return in;
}

// Tests whether two angles are close enough.
template <typename T>
bool AlmostEqual(const Angle<T>& a, const Angle<T>& b,
                 const Angle<T>& tolerance) {
  Angle<T> difference = a - b;
  T difference_radians =
      std::fmod(Abs(difference.Radians()), static_cast<T>(2*M_PI));
  if (difference_radians > static_cast<T>(M_PI)) {
    difference_radians = static_cast<T>(2*M_PI) - difference_radians;
  }
  return Abs(difference_radians) <= Abs(tolerance.Radians());
}

//------------------------------------------------------------------------------
// Type-specific typedefs.
//------------------------------------------------------------------------------

typedef Angle<float> Anglef;
typedef Angle<double> Angled;

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_ANGLE_H_
