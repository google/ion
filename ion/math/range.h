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

#ifndef ION_MATH_RANGE_H_
#define ION_MATH_RANGE_H_

#include <algorithm>
#include <istream>  // NOLINT
#include <ostream>  // NOLINT

#include "ion/base/logging.h"
#include "ion/base/static_assert.h"
#include "ion/base/stringutils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"

namespace ion {
namespace math {

// This struct allows the Endpoint and Size types in a Range<1, T> to be
// treated like those of higher-dimension Range classes (specifically the use
// of index operators) to simplify templated functions that use them.  It is
// essentially an implicit wrapper around a value of type T.
template <typename T, int N> struct Range1TWrapper {
  Range1TWrapper() {}
  Range1TWrapper(const T& t_in) : t(t_in) {}  // NOLINT - needs to be implicit.
  operator T() const { return t; }
  T& operator[](int index) {
    DCHECK_EQ(index, 0);
    return t;
  }
  const T& operator[](int index) const {
    DCHECK_EQ(index, 0);
    return t;
  }

  static T Zero() { return T(0); }

  // Allows reading wrapped value from a stream.
  template <typename U> friend std::istream& operator>>(
      std::istream& in, Range1TWrapper<U, 0>& w);

  T t;
};

template <typename T>
std::istream& operator>>(std::istream& in, Range1TWrapper<T, 0>& w) {
  return in >> w.t;
}

// The RangeBase class makes it possible to treat Ranges with Dimension=1
// specially. For example, a Range<1, int> will store two ints instead of two
// Point<1, int> instances, which would then require callers to index (always
// with 0) into the points.
template <int Dimension, typename T>
class RangeBase {
 public:
  // The dimension of the Range.
  enum { kDimension = Dimension };

  // Convenience typedef for a Range endpoint type.
  typedef Point<Dimension, T> Endpoint;

  // Convenience typedef for the size of a Range.
  typedef Vector<Dimension, T> Size;
};

// Specialize for Dimension=1.
template <typename T>
class RangeBase<1, T> {
 public:
  enum { kDimension = 1 };
  typedef Range1TWrapper<T, 0> Endpoint;
  typedef Range1TWrapper<T, 1> Size;
};

// The Range class defines an N-dimensional interval defined by minimum and
// maximum N-dimensional endpoints. The geometric interpretation of a Range is:
//   1D: A segment of the number line.
//   2D: An axis-aligned rectangle.
//   3D: An axis-aligned box.
//
// A Range is considered to be empty if the minimum value is strictly greater
// than the maximum value in any dimension.
template <int Dimension, typename T>
class Range : public RangeBase<Dimension, T> {
 public:
  typedef RangeBase<Dimension, T> BaseType;
  typedef typename BaseType::Endpoint Endpoint;
  typedef typename BaseType::Size Size;

  // The default constructor defines an empty Range.
  Range() { MakeEmpty(); }

  // Constructor that takes the minimum and maximum points. This does not check
  // that the values form a valid Range, so the resulting instance might be
  // considered empty.
  Range(const Endpoint& min_point, const Endpoint& max_point) {
    Set(min_point, max_point);
  }

  // Copy constructor from an instance of the same Dimension and any value type
  // that is compatible (via static_cast) with this instance's type.
  template <typename U>
  explicit Range(const Range<Dimension, U>& range);

  // Convenience function that returns a Range from a minimum point and the
  // Range size. This does not check that the values form a valid Range, so the
  // resulting instance might be considered empty.
  static Range BuildWithSize(const Endpoint& min_point, const Size& size) {
    Range r;
    r.SetWithSize(min_point, size);
    return r;
  }

  // Sets the Range to be empty.
  void MakeEmpty();

  // Returns true if the Range is empty, meaning that the minimum value is
  // strictly greater than the maximum value in some dimension.
  bool IsEmpty() const;

  // Returns the minimum or maximum endpoint. If the Range is empty, these
  // points will likely not be very useful.
  const Endpoint& GetMinPoint() const { return min_point_; }
  const Endpoint& GetMaxPoint() const { return max_point_; }

  // Modifies the minimum or maximum endpoint, or both. This does not check
  // that the values form a valid Range, so the resulting instance might be
  // considered empty.
  void SetMinPoint(const Endpoint& p) { min_point_ = p; }
  void SetMaxPoint(const Endpoint& p) { max_point_ = p; }
  void Set(const Endpoint& min_point, const Endpoint& max_point) {
    min_point_ = min_point;
    max_point_ = max_point;
  }

  // Modifies a single element for the minimum or maximum endpoint.
  void SetMinComponent(int i, T value);
  void SetMaxComponent(int i, T value);

  // Sets the Range from the minimum point and Range size. This does not check
  // that the values form a valid Range, so the resulting instance might be
  // considered empty.
  void SetWithSize(const Endpoint& min_point, const Size& size) {
    min_point_ = min_point;
    max_point_ = min_point + size;
  }

  // Returns the size of the range, or the zero vector if the Range is empty.
  const Size GetSize() const {
    return IsEmpty() ? Size::Zero() : max_point_ - min_point_;
  }

  // Returns the point at the center of the range, or the origin if the range
  // is empty.
  const Endpoint GetCenter() const {
    return IsEmpty() ? Endpoint::Zero() :
        min_point_ + ((max_point_ - min_point_) / static_cast<T>(2));
  }

  // Extends the Range if necessary to contain the given point. If the Range is
  // empty, it will be modified to contain just the point.
  void ExtendByPoint(const Endpoint& p);

  // Extends the Range if necessary to contain the given Range. If this Range
  // is empty, it becomes r. If r is empty, the Range is untouched.
  void ExtendByRange(const Range& r);

  // Returns true if the Range contains the given point. This is true if the
  // point lies between or on the Range extents.
  bool ContainsPoint(const Endpoint& p) const;

  // Returns true if this Range fully contains the given Range by
  // testing both min/max points of input Range.
  bool ContainsRange(const Range& r) const;

  // Returns true if this Range overlaps the given Range, i.e., there exists at
  // least one point contained in both ranges.
  bool IntersectsRange(const Range& r) const;

  // Exact equality and inequality comparisons.
  friend bool operator==(const Range& m0, const Range& m1) {
    // All empty ranges are equal.
    const bool m0_empty = m0.IsEmpty();
    const bool m1_empty = m1.IsEmpty();
    if (m0_empty || m1_empty)
      return m0_empty == m1_empty;
    else
      return m0.min_point_ == m1.min_point_ && m0.max_point_ == m1.max_point_;
  }
  friend bool operator!=(const Range& m0, const Range& m1) {
    return !(m0 == m1);
  }

 private:
  // For each dimension, if the value of p for that dimension is smaller than
  // the corresponding value in min-point, use that value instead.
  void ExtendMinByPoint(const Endpoint& p);
  // For each dimension, if the value of p for that dimension is larger than
  // the corresponding value in min-point, use that value instead.
  void ExtendMaxByPoint(const Endpoint& p);

  Endpoint min_point_;
  Endpoint max_point_;
};

// Prints a Range to a stream.
template <int Dimension, typename T>
std::ostream& operator<<(std::ostream& out, const Range<Dimension, T>& r) {
  if (r.IsEmpty())
    return out << "R[EMPTY]";
  else
    return out << "R[" << r.GetMinPoint() << ", " << r.GetMaxPoint() << "]";
}

// Reads a Range from a stream.
template <int Dimension, typename T>
std::istream& operator>>(std::istream& in, Range<Dimension, T>& r) {
  if (base::GetExpectedString(in, "R[")) {
    if (base::GetExpectedString(in, "EMPTY]")) {
      r = Range<Dimension, T>();
    } else {
      // Clear the error flag set by the check for EMPTY].
      in.clear();
      typename Range<Dimension, T>::Endpoint min_point, max_point;
      if (in >> min_point >> base::GetExpectedChar<','> >> max_point
             >> base::GetExpectedChar<']'>)
        r.Set(min_point, max_point);
    }
  }

  return in;
}

//------------------------------------------------------------------------------
// Implementation.
//------------------------------------------------------------------------------

// Default implementation; faster specialized versions are found below.
template <int Dimension, typename T>
void Range<Dimension, T>::MakeEmpty() {
  for (int i = 0; i < Dimension; ++i) {
    // Any values will do, as long as min > max.
    min_point_[i] = static_cast<T>(1);
    max_point_[i] = static_cast<T>(0);
  }
}

template <int Dimension, typename T>
template <typename U>
Range<Dimension, T>::Range(const Range<Dimension, U>& range)
    : min_point_(range.GetMinPoint()), max_point_(range.GetMaxPoint()) {}

// Default implementation; faster specialized versions are found below.
template <int Dimension, typename T>
bool Range<Dimension, T>::IsEmpty() const {
  for (int i = 0; i < Dimension; ++i) {
    if (min_point_[i] > max_point_[i])
      return true;
  }
  return false;
}

template <int Dimension, typename T>
void Range<Dimension, T>::SetMinComponent(int i, T value) {
  DCHECK_LE(0, i);
  DCHECK_GT(Dimension, i);
  if (0 <= i && i < Dimension)
    min_point_[i] = value;
}

template <int Dimension, typename T>
void Range<Dimension, T>::SetMaxComponent(int i, T value) {
  DCHECK_LE(0, i);
  DCHECK_GT(Dimension, i);
  if (0 <= i && i < Dimension)
    max_point_[i] = value;
}

template <int Dimension, typename T>
void Range<Dimension, T>::ExtendByPoint(const Endpoint& p) {
  if (IsEmpty()) {
    min_point_ = max_point_ = p;
  } else {
    ExtendMinByPoint(p);
    ExtendMaxByPoint(p);
  }
}

template <int Dimension, typename T>
void Range<Dimension, T>::ExtendByRange(const Range& r) {
  // Extending by an empty range has no effect.
  if (!r.IsEmpty()) {
    if (IsEmpty()) {
      *this = r;
    } else {
      ExtendMinByPoint(r.GetMinPoint());
      ExtendMaxByPoint(r.GetMaxPoint());
    }
  }
}

// Default implementation; faster specialized versions are found below.
template <int Dimension, typename T>
bool Range<Dimension, T>::ContainsPoint(const Endpoint& p) const {
  for (int i = 0; i < Dimension; ++i) {
    if (p[i] < min_point_[i] || p[i] > max_point_[i])
      return false;
  }
  return true;
}

template <int Dimension, typename T>
bool Range<Dimension, T>::ContainsRange(const Range& r) const {
  return ContainsPoint(r.GetMinPoint()) && ContainsPoint(r.GetMaxPoint());
}

template <int Dimension, typename T>
bool Range<Dimension, T>::IntersectsRange(const Range& r) const {
  const Endpoint& r_min_point = r.GetMinPoint();
  const Endpoint& r_max_point = r.GetMaxPoint();
  for (int i = 0; i < Dimension; ++i) {
    if (min_point_[i] > r_max_point[i]) return false;
    if (max_point_[i] < r_min_point[i]) return false;
  }
  return true;
}

// Default implementation; faster specialized versions are found below.
template <int Dimension, typename T>
void Range<Dimension, T>::ExtendMinByPoint(const Endpoint& p) {
  for (int i = 0; i < Dimension; ++i) {
    min_point_[i] = std::min(p[i], min_point_[i]);
  }
}

// Default implementation; faster specialized versions are found below.
template <int Dimension, typename T>
void Range<Dimension, T>::ExtendMaxByPoint(const Endpoint& p) {
  for (int i = 0; i < Dimension; ++i) {
    max_point_[i] = std::max(p[i], max_point_[i]);
  }
}

//------------------------------------------------------------------------------
// Dimension- and type-specific typedefs.
//------------------------------------------------------------------------------

typedef Range<1, int8> Range1i8;
typedef Range<1, uint8> Range1ui8;
typedef Range<1, int16> Range1i16;
typedef Range<1, uint16> Range1ui16;
typedef Range<1, int32> Range1i;
typedef Range<1, uint32> Range1ui;
typedef Range<1, float> Range1f;
typedef Range<1, double> Range1d;
typedef Range<2, int8> Range2i8;
typedef Range<2, uint8> Range2ui8;
typedef Range<2, int16> Range2i16;
typedef Range<2, uint16> Range2ui16;
typedef Range<2, int32> Range2i;
typedef Range<2, uint32> Range2ui;
typedef Range<2, float> Range2f;
typedef Range<2, double> Range2d;
typedef Range<3, int8> Range3i8;
typedef Range<3, uint8> Range3ui8;
typedef Range<3, int16> Range3i16;
typedef Range<3, uint16> Range3ui16;
typedef Range<3, int32> Range3i;
typedef Range<3, uint32> Range3ui;
typedef Range<3, float> Range3f;
typedef Range<3, double> Range3d;
typedef Range<4, int8> Range4i8;
typedef Range<4, uint8> Range4ui8;
typedef Range<4, int16> Range4i16;
typedef Range<4, uint16> Range4ui16;
typedef Range<4, int32> Range4i;
typedef Range<4, uint32> Range4ui;
typedef Range<4, float> Range4f;
typedef Range<4, double> Range4d;

//------------------------------------------------------------------------------
// Specializations for higher performance.
//------------------------------------------------------------------------------

template<>
inline void Range3d::MakeEmpty() {
  min_point_[0] = 1.0;
  min_point_[1] = 1.0;
  min_point_[2] = 1.0;
  max_point_[0] = -1.0;
  max_point_[1] = -1.0;
  max_point_[2] = -1.0;
}

template<>
inline bool Range3d::IsEmpty() const {
  return
    min_point_[0] > max_point_[0] ||
    min_point_[1] > max_point_[1] ||
    min_point_[2] > max_point_[2];
}

template<>
inline bool Range3d::ContainsPoint(const Point3d& p) const {
  return
    p[0] >= min_point_[0] && p[0] <= max_point_[0] &&
    p[1] >= min_point_[1] && p[1] <= max_point_[1] &&
    p[2] >= min_point_[2] && p[2] <= max_point_[2];
}

template<>
inline void Range3d::ExtendMinByPoint(const Point3d& p) {
  min_point_[0] = std::min(p[0], min_point_[0]);
  min_point_[1] = std::min(p[1], min_point_[1]);
  min_point_[2] = std::min(p[2], min_point_[2]);
}

template<>
inline void Range3d::ExtendMaxByPoint(const Point3d& p) {
  max_point_[0] = std::max(p[0], max_point_[0]);
  max_point_[1] = std::max(p[1], max_point_[1]);
  max_point_[2] = std::max(p[2], max_point_[2]);
}

template<>
inline void Range3f::MakeEmpty() {
  min_point_[0] = 1.0f;
  min_point_[1] = 1.0f;
  min_point_[2] = 1.0f;
  max_point_[0] = -1.0f;
  max_point_[1] = -1.0f;
  max_point_[2] = -1.0f;
}

template<>
inline bool Range3f::IsEmpty() const {
  return
    min_point_[0] > max_point_[0] ||
    min_point_[1] > max_point_[1] ||
    min_point_[2] > max_point_[2];
}

template<>
inline bool Range3f::ContainsPoint(const Point3f& p) const {
  return
    p[0] >= min_point_[0] && p[0] <= max_point_[0] &&
    p[1] >= min_point_[1] && p[1] <= max_point_[1] &&
    p[2] >= min_point_[2] && p[2] <= max_point_[2];
}

template<>
inline void Range3f::ExtendMinByPoint(const Point3f& p) {
  min_point_[0] = std::min(p[0], min_point_[0]);
  min_point_[1] = std::min(p[1], min_point_[1]);
  min_point_[2] = std::min(p[2], min_point_[2]);
}

template<>
inline void Range3f::ExtendMaxByPoint(const Point3f& p) {
  max_point_[0] = std::max(p[0], max_point_[0]);
  max_point_[1] = std::max(p[1], max_point_[1]);
  max_point_[2] = std::max(p[2], max_point_[2]);
}

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_RANGE_H_
