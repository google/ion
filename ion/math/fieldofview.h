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

#ifndef ION_MATH_FIELDOFVIEW_H_
#define ION_MATH_FIELDOFVIEW_H_

#include <cmath>

#include "ion/math/angle.h"
#include "ion/math/matrixutils.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
#include "ion/math/vector.h"

namespace ion {
namespace math {

// Encapsulates a generalized, asymmetric field of view with four half angles.
// Each half angle denotes the angle between the corresponding frustum plane.
// Together with a near and far plane, a FieldOfView forms the frustum of an
// off-axis perspective projection.
template <typename T>
class FieldOfView {
 public:
  // The default constructor sets an angle of 0 (in any unit) for all four
  // half-angles.
  FieldOfView();

  // Constructs a FieldOfView from four angles.
  FieldOfView(const Angle<T>& left, const Angle<T>& right,
              const Angle<T>& bottom, const Angle<T>& top);

  // Constructs a FieldOfView by extracting the four frustum planes from the
  // projection matrix.
  static FieldOfView<T> FromProjectionMatrix(const Matrix<4, T>& m);

  // Constructs a FieldOfView from four values tan(alpha) for each half-angle
  // alpha.  Note that these are tangents of signed angles, so to construct
  // a field of view that is 45 degrees from in each direction, you would pass
  // -1, 1, -1, 1.
  static FieldOfView<T> FromTangents(const T& left, const T& right,
                                     const T& bottom, const T& top);

  // Constructs a FieldOfView from four values tan(alpha) for each half-angle
  // alpha, represented as a range.  Note that these are tangents of signed
  // angles, so to construct a field of view that is 45 degrees from in each
  // direction, you would pass Range2f(Point2f(-1, -1), Point2f(1, 1)).
  static FieldOfView<T> FromTangents(const Range<2, T>& tangents);

  // Shorthand for constructing a field of view from four angles in radians.
  static FieldOfView<T> FromRadians(const T& left, const T& right,
                                    const T& bottom, const T& top);

  // Shorthand for constructing a field of view from four angles in degrees.
  static FieldOfView<T> FromDegrees(const T& left, const T& right,
                                    const T& bottom, const T& top);

  // Copy constructor from an instance of the same Dimension and any value type
  // that is compatible (via static_cast) with this instance's type.
  template <typename U>
  explicit FieldOfView(const FieldOfView<U>& fov);

  // Resets the FieldOfView based on a total field of view in both dimensions,
  // and an optical center for the projection. The optical center is defined
  // as the intersection of the optical axis with the image plane. Note that the
  // optical center is invariant in world space. This method sets
  // left/right/up/down so that the optical center appears at the given
  // |optical_center_ndc| with respect to the window defined by those bounds.
  //
  // Returns true if a valid configuration was provided. If the provided
  // configuration was invalid, the FieldOfView object remains unchanged and
  // this method returns false.
  //
  // Note that the aspect ratio implied by the requested fov_x and fov_y will
  // not necessarily be preserved.
  bool SetFromTotalFovAndOpticalCenter(const Angle<T>& fov_x,
                                       const Angle<T>& fov_y,
                                       const Point<2, T>& optical_center_ndc);

  // Constructs a FieldOfView based on a centered field of view and an optical
  // center for the projection. The optical center is defined as the
  // intersection of the optical axis with the image plane. Note that the
  // optical center is invariant in world space. This method sets
  // left/right/up/down so that the optical center appears at the given
  // |optical_center_ndc| with respect to the window defined by those bounds.
  //
  // The centered FOV is not necessarily the actual FOV. It is defined as what
  // the fov would be if the camera were kept the same perpendicular distance
  // from the viewing plane but the optical center were the center of the
  // screen.
  //
  //           -1   p_ndc    0            1
  //            +-----*------*------------+
  //             \ \  |      |            /
  //              \  \|      |        //
  //               \  |\     |    / /
  //                \ |   \  |/  /
  //                 \|   / \|/
  //                  *      *
  //                 eye    eye_centered
  //
  // In the diagram above, the centered_fov is the angle at eye_centered.
  // The centered FOV allows us to maintain the size of objects on the map.
  static FieldOfView<T> FromCenteredFovAndOpticalCenter(
      const Angle<T>& fov_x, const Angle<T>& fov_y,
      const Point<2, T>& optical_center_ndc);

  // Returns the optical center of a projection that is created using this
  // FieldOfView.
  Point<2, T> GetOpticalCenter() const;

  // Computes the projection matrix corresponding to the frustum defined by
  // the four half angles and the two planes |near_p| and |far_p|.
  // 
  Matrix<4, T> GetProjectionMatrix(T near_p, T far_p) const;

  // Computes the projection matrix corresponding to the infinite frustum
  // defined by the four half angles, the near plane |near_p| and the far
  // clip plane at infinity. The optional epsilon |far_epsilon| assists
  // with clipping artifacts when using the matrix with GPU clipping; see
  // PerspectiveMatrixFromInfiniteFrustum.
  Matrix<4, T> GetInfiniteFarProjectionMatrix(T near_p, T far_epsilon) const;

  // Gets the tangents of field of view angles as a range. For the purposes of
  // this method, the left and bottom angles are negated before taking the
  // tangent. For example, a field of view with all angles equal to 45 degrees
  // will return a range from -1 to 1 in both dimensions.
  Range<2, T> GetTangents() const;

  // Accessors for all four half-angles.
  Angle<T> GetLeft() const { return left_; }
  Angle<T> GetRight() const { return right_; }
  Angle<T> GetBottom() const { return bottom_; }
  Angle<T> GetTop() const { return top_; }

  // Setters for all four half-angles.
  void SetLeft(const Angle<T>& left) { left_ = left; }
  void SetRight(const Angle<T>& right) { right_ = right; }
  void SetBottom(const Angle<T>& bottom) { bottom_ = bottom; }
  void SetTop(const Angle<T>& top) { top_ = top; }

  // Gets the centered FOV in each dimension. It is defined as what
  // the FOV would be if the camera were kept the same perpendicular distance
  // from the viewing plane but the optical center were the center of the
  // screen.
  Angle<T> GetCenteredFovX() const;
  Angle<T> GetCenteredFovY() const;

  // This is used for printing FOV objects to a stream.
  void Print(std::ostream& out) const;  // NOLINT

  // This is used for reading FOV objects from a stream.
  void Read(std::istream& in);  // NOLINT

  // Returns true iff all four angles are zero (which is the case after using
  // the default constructor).
  bool IsZero() const;

  // Exact equality and inequality comparisons.
  friend bool operator==(const FieldOfView& fov0, const FieldOfView& fov1) {
    return AreEqual(fov0, fov1);
  }

  friend bool operator!=(const FieldOfView& fov0, const FieldOfView& fov1) {
    return !AreEqual(fov0, fov1);
  }

 private:
  // Computes the two half-angles between the optical axis and the two frustum
  // planes in one dimension. |optical_center_ndc| specifies the location of
  // the optical center on the near plane, and |total_fov| specifies the sum of
  // the half-angles along that dimension.
  static bool ComputeHalfAnglesForTotalFovAndOpticalCenter1d(
      const Angle<T>& total_fov, const T optical_center_ndc,
      Angle<T>* angle1_out, Angle<T>* angle2_out);

  // Computes the two half-angles between the optical axis and the two frustum
  // planes in one dimension. |optical_center_ndc| specifies the location of
  // the optical center on the near plane, and |centered_fov| specifies the
  // centered fov desired (i.e., what the fov would be if the camera were kept
  // the same perpendicular distance from the viewing plane but the optical
  // center were the center of the screen).
  static void ComputeHalfAnglesForCenteredFovAndOpticalCenter1d(
      const Angle<T>& centered_fov, const T optical_center_ndc,
      Angle<T>* angle1_out, Angle<T>* angle2_out);

  static bool AreEqual(const FieldOfView& fov0, const FieldOfView& fov1);

  Angle<T> left_;
  Angle<T> right_;
  Angle<T> bottom_;
  Angle<T> top_;
};

typedef FieldOfView<float> FieldOfViewf;
typedef FieldOfView<double> FieldOfViewd;

template <typename T>
inline FieldOfView<T>::FieldOfView() {}

template <typename T>
inline FieldOfView<T>::FieldOfView(const Angle<T>& left, const Angle<T>& right,
                                   const Angle<T>& bottom, const Angle<T>& top)
    : left_(left), right_(right), bottom_(bottom), top_(top) {}

template <typename T>
inline Matrix<4, T> FieldOfView<T>::GetProjectionMatrix(T near_p,
                                                        T far_p) const {
  const T l = -std::tan(left_.Radians()) * near_p;
  const T r = std::tan(right_.Radians()) * near_p;
  const T b = -std::tan(bottom_.Radians()) * near_p;
  const T t = std::tan(top_.Radians()) * near_p;
  return ion::math::PerspectiveMatrixFromFrustum(l, r, b, t, near_p, far_p);
}

template <typename T>
inline Matrix<4, T> FieldOfView<T>::GetInfiniteFarProjectionMatrix(T near_p,
                                                        T far_epsilon) const {
  const T l = -std::tan(left_.Radians()) * near_p;
  const T r = std::tan(right_.Radians()) * near_p;
  const T b = -std::tan(bottom_.Radians()) * near_p;
  const T t = std::tan(top_.Radians()) * near_p;
  return ion::math::PerspectiveMatrixFromInfiniteFrustum(l, r, b, t, near_p,
                                                         far_epsilon);
}

template <typename T>
inline Range<2, T> FieldOfView<T>::GetTangents() const {
  return Range<2, T>(
      Point<2, T>(-std::tan(left_.Radians()), -std::tan(bottom_.Radians())),
      Point<2, T>(std::tan(right_.Radians()), std::tan(top_.Radians())));
}

template <typename T>
inline FieldOfView<T> FieldOfView<T>::FromTangents(const T& left,
                                                   const T& right,
                                                   const T& bottom,
                                                   const T& top) {
  return FieldOfView<T>(Angle<T>::FromRadians(std::atan(-left)),
                        Angle<T>::FromRadians(std::atan(right)),
                        Angle<T>::FromRadians(std::atan(-bottom)),
                        Angle<T>::FromRadians(std::atan(top)));
}

template <typename T>
inline FieldOfView<T> FieldOfView<T>::FromTangents(
    const Range<2, T>& tangents) {
  return FieldOfView<T>(
      Angle<T>::FromRadians(std::atan(-tangents.GetMinPoint()[0])),
      Angle<T>::FromRadians(std::atan(tangents.GetMaxPoint()[0])),
      Angle<T>::FromRadians(std::atan(-tangents.GetMinPoint()[1])),
      Angle<T>::FromRadians(std::atan(tangents.GetMaxPoint()[1])));
}

template <typename T>
inline FieldOfView<T> FieldOfView<T>::FromRadians(const T& left,
                                                  const T& right,
                                                  const T& bottom,
                                                  const T& top) {
  return FieldOfView<T>(Angle<T>::FromRadians(left),
                        Angle<T>::FromRadians(right),
                        Angle<T>::FromRadians(bottom),
                        Angle<T>::FromRadians(top));
}

template <typename T>
inline FieldOfView<T> FieldOfView<T>::FromDegrees(const T& left,
                                                  const T& right,
                                                  const T& bottom,
                                                  const T& top) {
  return FieldOfView<T>(Angle<T>::FromDegrees(left),
                        Angle<T>::FromDegrees(right),
                        Angle<T>::FromDegrees(bottom),
                        Angle<T>::FromDegrees(top));
}

template <typename T>
inline FieldOfView<T> FieldOfView<T>::FromProjectionMatrix(
    const Matrix<4, T>& m) {
  const T kOne = static_cast<T>(1.0);

  // Compute tangents.
  const T kTanVertFov = kOne / m(1, 1);
  const T kTanHorzFov = kOne / m(0, 0);
  const T t = (m(1, 2) + kOne) * kTanVertFov;
  const T b = (m(1, 2) - kOne) * kTanVertFov;
  const T l = (m(0, 2) - kOne) * kTanHorzFov;
  const T r = (m(0, 2) + kOne) * kTanHorzFov;

  return FromTangents(l, r, b, t);
}

template <typename T>
template <typename U>
FieldOfView<T>::FieldOfView(const FieldOfView<U>& fov)
    : left_(fov.GetLeft()),
      right_(fov.GetRight()),
      bottom_(fov.GetBottom()),
      top_(fov.GetTop()) {}

template <typename T>
inline Point<2, T> FieldOfView<T>::GetOpticalCenter() const {
  const T kOne = static_cast<T>(1.0);
  const T kTwo = static_cast<T>(2.0);
  const T tan_left = std::tan(left_.Radians());
  const T tan_right = std::tan(right_.Radians());
  const T tan_bottom = std::tan(bottom_.Radians());
  const T tan_top = std::tan(top_.Radians());
  const T x_ndc = kTwo * tan_left / (tan_left + tan_right) - kOne;
  const T y_ndc = kTwo * tan_bottom / (tan_bottom + tan_top) - kOne;
  return Point<2, T>(x_ndc, y_ndc);
}

template <typename T>
inline bool FieldOfView<T>::SetFromTotalFovAndOpticalCenter(
    const Angle<T>& fov_x, const Angle<T>& fov_y,
    const Point<2, T>& optical_center_ndc) {
  Angle<T> bottom, top, left, right;
  if (!ComputeHalfAnglesForTotalFovAndOpticalCenter1d(
          fov_x, optical_center_ndc[0], &left, &right))
    return false;
  if (!ComputeHalfAnglesForTotalFovAndOpticalCenter1d(
          fov_y, optical_center_ndc[1], &bottom, &top))
    return false;

  // Only modify internal state once we know that the provided configuration was
  // valid.
  left_ = left;
  right_ = right;
  bottom_ = bottom;
  top_ = top;
  return true;
}

template <typename T>
inline bool FieldOfView<T>::ComputeHalfAnglesForTotalFovAndOpticalCenter1d(
    const Angle<T>& total_fov, const T optical_center_ndc, Angle<T>* angle1_out,
    Angle<T>* angle2_out) {
  const T kOne = static_cast<T>(1.0);
  T p = optical_center_ndc;

  // The problem is symmetric around the center of the near plane. For
  // simplicity, the rest of this method assumes that p lies on the left side or
  // in the center of the near plane. If this is not the case, we need to flip
  // the input and remember to flip the result in the end.
  bool invert = false;
  if (p > 0.0) {
    invert = true;
    p = -p;
  }

  // Here is an illustration of the one-dimensional problem in normalized device
  // coordinates:
  //
  //           -1   p_ndc                  1
  //            +-----*--------------------+
  //             \    |                   /
  //              \   |               /
  //               \  |           /
  //                \ |       /
  //                 \|   /
  //     angle1 ----^ * ^---- angle2
  //                 eye
  //
  // We can derive the following relationship between angle1, angle2 and p_ndc
  // from trigonometric principles:
  //
  //     tan(angle1) / (tan(angle1) + tan(angle2)) = (1 + p_ndc) / 2
  //
  // Since we know total_fov and p_ndc, but not angle1 and angle2, we
  // substitute angle2 := total_fov - angle1:
  //
  //     tan(angle1) / (tan(angle1) + tan(total_fov - angle1)) = (1 + p_ndc) / 2
  //
  // To solve this equation for angle1 (the only remaining unknown), we need to
  // apply the following trigonometric identity:
  //
  //     tan(x - y) = (tan(x) - tan(y)) / (1 + tan(x) * tan(y))
  //
  //              with x := total_fov and y := angle1
  //
  // After reordering terms, we end up with a quadratic equation in tan(alpha):
  //
  //     a * tan(alpha)^2 + b * tan(alpha) + c = 0
  //
  //     a = (p_ndc - 1) / tan(total_fov)
  //     b = -2
  //     c = (p_ndc + 1) / tan(total_fov)
  //
  // Some combinations of total_fov and optical center are not satisfiable and
  // result in a negative discriminant:
  //
  const T inv_tan_total_fov = kOne / tan(total_fov.Radians());
  const T discriminant = kOne + Square(inv_tan_total_fov) - Square(p);
  if (discriminant < 0) return false;

  // The quadratic equation has two solutions for alpha within [-PI, PI], and
  // they are different by PI radians. To choose the correct solution, we
  // distinguish two cases:
  //
  // 1. If the original p is inside [-1, 1], we expect a positive angle. The
  // other solution is negative, hence we choose the bigger solution.
  // 2. If the original p is outside [-1, 1], we expect a negative angle. The
  // other solution is positive, hence we choose the smaller solution.
  //
  const double sign = p < -kOne ? -kOne : kOne;
  *angle1_out = Angle<T>::FromRadians(
      atan((sign * std::sqrt(discriminant) - inv_tan_total_fov) / (kOne - p)));

  // Compute the second half angle (given the total angle) and flip the result
  // if necessary.
  *angle2_out = total_fov - *angle1_out;
  if (invert) std::swap(*angle1_out, *angle2_out);
  return true;
}

template <typename T>
inline FieldOfView<T> FieldOfView<T>::FromCenteredFovAndOpticalCenter(
    const Angle<T>& fov_x, const Angle<T>& fov_y,
    const Point<2, T>& optical_center_ndc) {
  Angle<T> bottom, top, left, right;
  ComputeHalfAnglesForCenteredFovAndOpticalCenter1d(
      fov_x, optical_center_ndc[0], &left, &right);
  ComputeHalfAnglesForCenteredFovAndOpticalCenter1d(
      fov_y, optical_center_ndc[1], &bottom, &top);

  return FieldOfView<T>(left, right, bottom, top);
}

template <typename T>
inline void FieldOfView<T>::ComputeHalfAnglesForCenteredFovAndOpticalCenter1d(
    const Angle<T>& centered_fov, const T optical_center_ndc,
    Angle<T>* angle1_out, Angle<T>* angle2_out) {
  const T kOne = static_cast<T>(1.0);
  const T kTwo = static_cast<T>(2.0);
  const T p = optical_center_ndc;

  // Here is an illustration of the one-dimensional problem in normalized device
  // coordinates:
  //
  //           -1   p_ndc    0            1
  //            +-----*------*------------+
  //             \ \  |      |            /
  //              \  \|      |        //
  //               \  |\     |    / /
  //                \ |   \  |/  /
  //                 \|   / \|/
  //              a1-^*^-a2  *
  //                 eye    eye_centered
  //
  // Let x be the perpendicular distance from the eye to ndc unit plane. Then,
  //
  //     tan(centered_fov / 2) = 1 / x
  //     x = 1 / tan(centered_fov / 2)
  //
  //     tan(a1) = (p_ndc - (-1)) / x
  //     a1 = arctan((p_ndc + 1) / x)
  //
  //     tan(a2) = (1 - p_ndc) / x
  //     a2 = arctan((1 - p_ndc) / x)

  const T x = kOne / std::tan(centered_fov.Radians() / kTwo);

  *angle1_out = Angle<T>::FromRadians(std::atan2(p + kOne, x));
  *angle2_out = Angle<T>::FromRadians(std::atan2(kOne - p, x));
}

template <typename T>
inline Angle<T> FieldOfView<T>::GetCenteredFovX() const {
  // Using the diagram in ComputeHalfAnglesForCenteredFovAndOpticalCenter1d:
  //     p_ndc = x * tan(a1) - 1
  //     p_ndc = 1 - x * tan(a2)
  //
  // Solving simultaneously:
  //     x = 2 / (tan(a1) + tan(a2))
  //
  // Using x = 1 / tan(centered_fov / 2), we get:
  //     tan(centered_fov / 2) = (tan(a1) + tan(a2)) / 2
  //     centered_fov = 2 * arctan((tan(a1) + tan(a2)) / 2)

  const T tan_left = std::tan(left_.Radians());
  const T tan_right = std::tan(right_.Radians());
  return Angle<T>::FromRadians(2 * std::atan((tan_left + tan_right) / 2));
}

template <typename T>
inline Angle<T> FieldOfView<T>::GetCenteredFovY() const {
  // See GetCenteredFovX for derivation.

  const T tan_bottom = std::tan(bottom_.Radians());
  const T tan_top = std::tan(top_.Radians());
  return Angle<T>::FromRadians(2 * std::atan((tan_bottom + tan_top) / 2));
}

template <typename T>
inline void FieldOfView<T>::Print(std::ostream& out) const {  // NOLINT
  out << "FOV[" << left_ << ", " << right_ << ", " << bottom_ << ", " << top_
      << "]";
}

template <typename T>
inline void FieldOfView<T>::Read(std::istream& in) {  // NOLINT
  FieldOfView fov;
  if (base::GetExpectedString(in, "FOV[")) {
    in >> left_;
    if (!base::GetExpectedChar<','>(in)) {
      *this = fov;
      return;
    }
    in >> right_;
    if (!base::GetExpectedChar<','>(in)) {
      *this = fov;
      return;
    }
    in >> bottom_;
    if (!base::GetExpectedChar<','>(in)) {
      *this = fov;
      return;
    }
    in >> top_;
    if (!base::GetExpectedChar<']'>(in)) {
      *this = fov;
      return;
    }
  }
}

template <typename T>
inline std::ostream& operator<<(std::ostream& out,
                                const FieldOfView<T>& f) {  // NOLINT
  f.Print(out);
  return out;
}

template <typename T>
inline std::istream& operator>>(std::istream& in,
                                FieldOfView<T>& f) {  // NOLINT
  f.Read(in);
  return in;
}

template <typename T>
inline bool FieldOfView<T>::IsZero() const {
  const T kZero = static_cast<T>(0.0);
  if (left_.Radians() != kZero || right_.Radians() != kZero ||
      bottom_.Radians() != kZero || top_.Radians() != kZero)
    return false;
  return true;
}

template <typename T>
bool FieldOfView<T>::AreEqual(const FieldOfView& fov0,
                              const FieldOfView& fov1) {
  if (fov0.GetLeft() != fov1.GetLeft() || fov0.GetRight() != fov1.GetRight() ||
      fov0.GetBottom() != fov1.GetBottom() || fov0.GetTop() != fov1.GetTop())
    return false;
  return true;
}

// Tests whether two fields of view are close enough, with tolerance specified
// as an angle.
template <typename T>
bool AlmostEqual(const FieldOfView<T>& a, const FieldOfView<T>& b,
                 const Angle<T>& tolerance) {
  return AlmostEqual(a.GetLeft(), b.GetLeft(), tolerance) &&
         AlmostEqual(a.GetRight(), b.GetRight(), tolerance) &&
         AlmostEqual(a.GetBottom(), b.GetBottom(), tolerance) &&
         AlmostEqual(a.GetTop(), b.GetTop(), tolerance);
}

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_FIELDOFVIEW_H_
