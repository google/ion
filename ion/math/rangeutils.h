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

#ifndef ION_MATH_RANGEUTILS_H_
#define ION_MATH_RANGEUTILS_H_

//
// This file contains free functions that define generic operations on the
// Range class.
//

#include <algorithm>

#include "ion/math/range.h"

namespace ion {
namespace math {

// Returns the union of two Range instances. If either Range is empty, this
// returns the other range. Otherwise, it returns the smallest Range containing
// both.
template <int Dimension, typename T>
Range<Dimension, T> RangeUnion(const Range<Dimension, T>& r0,
                               const Range<Dimension, T>& r1) {
  Range<Dimension, T> result(r0);
  result.ExtendByRange(r1);
  return result;
}

// Returns the intersection of two Range instances. If either Range is empty,
// this returns an empty range. Otherwise, it returns the largest Range
// contained by both.
template <int Dimension, typename T>
Range<Dimension, T> RangeIntersection(const Range<Dimension, T>& r0,
                                      const Range<Dimension, T>& r1) {
  if (r0.IsEmpty() || r1.IsEmpty()) {
    return Range<Dimension, T>();
  } else {
    typename Range<Dimension, T>::Endpoint result_min;
    typename Range<Dimension, T>::Endpoint result_max;
    for (int i = 0; i < Dimension; ++i) {
      result_min[i] = std::max(r0.GetMinPoint()[i], r1.GetMinPoint()[i]);
      result_max[i] = std::min(r0.GetMaxPoint()[i], r1.GetMaxPoint()[i]);
      // Stop if the result is empty.
      if (result_min[i] > result_max[i])
        return Range<Dimension, T>();
    }
    return Range<Dimension, T>(result_min, result_max);
  }
}

// Returns the NVolume of a Range, which is the product of its sizes in all
// dimensions. Returns 0 if the Range is empty.
template <int Dimension, typename T>
T NVolume(const Range<Dimension, T>& r) {
  if (r.IsEmpty()) {
    return T(0);
  } else {
    T result(1);
    typename Range<Dimension, T>::Size edges = r.GetSize();
    for (int i = 0; i < Dimension; ++i) {
      result *= edges[i];
    }
    return result;
  }
}

// Returns true if all dimensions of the two ranges are equal within the
// threshold.
template <int Dimension, typename T>
bool RangesAlmostEqual(const Range<Dimension, T>& r0,
                       const Range<Dimension, T>& r1,
                       const T threshold) {
  if (r0.IsEmpty() || r1.IsEmpty())
    return false;
  const typename Range<Dimension, T>::Endpoint& r0_min = r0.GetMinPoint();
  const typename Range<Dimension, T>::Endpoint& r0_max = r0.GetMaxPoint();
  const typename Range<Dimension, T>::Endpoint& r1_min = r1.GetMinPoint();
  const typename Range<Dimension, T>::Endpoint& r1_max = r1.GetMaxPoint();

  for (int i = 0; i < Dimension; ++i) {
    if (Abs(r0_min[i] - r1_min[i]) > Abs(threshold) ||
        Abs(r0_max[i] - r1_max[i]) > Abs(threshold))
      return false;
  }
  return true;
}

// Returns a Range that is the input Range scaled uniformly about its center by
// the given factor. If the factor is not positive, this returns an empty
// Range.
template <int Dimension, typename T>
Range<Dimension, T> ScaleRange(const Range<Dimension, T>& r, T scale_factor) {
  typedef Range<Dimension, T> RangeType;
  RangeType scaled;
  if (!r.IsEmpty() && scale_factor > static_cast<T>(0)) {
    const typename RangeType::Endpoint center = r.GetCenter();
    scaled.Set(center + scale_factor * (r.GetMinPoint() - center),
               center + scale_factor * (r.GetMaxPoint() - center));
  }
  return scaled;
}

// Returns a Range that is the input Range scaled nonuniformly about its center
// by the given per-dimension factors. If any factor is not positive, this
// returns an empty Range.
template <int Dimension, typename T>
Range<Dimension, T> ScaleRangeNonUniformly(
    const Range<Dimension, T>& r, const Vector<Dimension, T> scale_factors) {
  typedef Range<Dimension, T> RangeType;
  RangeType scaled;
  if (!r.IsEmpty()) {
    const typename RangeType::Endpoint center = r.GetCenter();
    typename RangeType::Endpoint min_pt = r.GetMinPoint();
    typename RangeType::Endpoint max_pt = r.GetMaxPoint();
    for (int i = 0; i < Dimension; ++i) {
      if (scale_factors[i] <= static_cast<T>(0))
        return scaled;
      min_pt[i] = center[i] + scale_factors[i] * (min_pt[i] - center[i]);
      max_pt[i] = center[i] + scale_factors[i] * (max_pt[i] - center[i]);
    }
    scaled.Set(min_pt, max_pt);
  }
  return scaled;
}

// Modulate a Range by a Vector.  Each dimension of the Range is modulated
// by the corresponding dimension in the Vector.  The modulation is done using
// the multiplication operator defined for the Vector, in the space of the
// Vector's type.  For example, if an integral Range is scaled by a floating
// point Vector, the modulation will occur using floating point multiply.  If
// any factor is not positive, this returns an empty Range.
template <int Dimension, typename T1, typename T2>
Range<Dimension, T1> ModulateRange(const Range<Dimension, T1>& r,
                                   const Vector<Dimension, T2> modulation) {
  typedef Range<Dimension, T1> RangeType;
  RangeType modulated;
  if (!r.IsEmpty()) {
    typename RangeType::Endpoint min_pt = r.GetMinPoint();
    typename RangeType::Endpoint max_pt = r.GetMaxPoint();
    for (int i = 0; i < Dimension; ++i) {
      if (modulation[i] <= static_cast<T2>(0))
        return modulated;
      min_pt[i] = static_cast<T1>(static_cast<T2>(min_pt[i]) * modulation[i]);
      max_pt[i] = static_cast<T1>(static_cast<T2>(max_pt[i]) * modulation[i]);
    }
    modulated.Set(min_pt, max_pt);
  }
  return modulated;
}

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_RANGEUTILS_H_
