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

#ifndef ION_MATH_VECTORUTILS_H_
#define ION_MATH_VECTORUTILS_H_

//
// This file contains free functions that operate on Vector and Point
// instances.
//

#include <algorithm>
#include <limits>

#include "ion/base/logging.h"
#include "ion/base/scalarsequence.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"

namespace ion {
namespace math {

// Returns a vector with the specified coordinate removed, yielding a vector
// that is one dimension smaller.
template <int Dimension, typename T>
Vector<Dimension - 1, T> WithoutDimension(const Vector<Dimension, T>& v,
                                          int dim) {
  Vector<Dimension - 1, T> result;
  for (int i = 0; i < Dimension - 1; ++i) {
    result[i] = v[i + (i < dim ? 0 : 1)];
  }
  return result;
}

// Returns the dot (inner) product of two Vectors.
template <int Dimension, typename T>
T Dot(const Vector<Dimension, T>& v0, const Vector<Dimension, T>& v1) {
  return vector_internal::Unroller<T>::Dot(
      static_cast<T>(0), v0.Data(), v1.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
}

// Returns the 3-dimensional cross product of 2 Vectors. Note that this is
// defined only for 3-dimensional Vectors.
template <typename T>
Vector<3, T> Cross(const Vector<3, T>& v0, const Vector<3, T>& v1) {
  return Vector<3, T>(v0[1] * v1[2] - v0[2] * v1[1],
                      v0[2] * v1[0] - v0[0] * v1[2],
                      v0[0] * v1[1] - v0[1] * v1[0]);
}

// Returns the cross product of |v0| and |v1| aka the determinant of the 2x2
// matrix described by |v0.x v0.y|
//                     |v1.x v1.y|.
template <typename T>
T Cross(const Vector<2, T>& v0, const Vector<2, T>& v1) {
  return (v0[0] * v1[1] - v0[1] * v1[0]);
}

// Returns the square of the length of a Vector.
template <int Dimension, typename T>
T LengthSquared(const Vector<Dimension, T>& v) {
  return Dot(v, v);
}

// Returns the geometric length of a Vector.
template <int Dimension, typename T>
T Length(const Vector<Dimension, T>& v) {
  return static_cast<T>(Sqrt(LengthSquared(v)));
}

// Returns the square of the distance between two Points.
template <int Dimension, typename T>
T DistanceSquared(const Point<Dimension, T>& p0,
                  const Point<Dimension, T>& p1) {
  return LengthSquared(p0 - p1);
}

// Returns the geometric distance between two Points.
template <int Dimension, typename T>
T Distance(const Point<Dimension, T>& p0, const Point<Dimension, T>& p1) {
  return Length(p0 - p1);
}

// Normalizes a Vector to unit length. If the vector has no length, this leaves
// the Vector untouched and returns false.
template <int Dimension, typename T>
bool Normalize(Vector<Dimension, T>* v) {
  DCHECK(v);
  const T len = Length(*v);
  if (len == static_cast<T>(0)) {
    return false;
  } else {
    (*v) /= len;
    return true;
  }
}

// Returns a unit-length version of a Vector. If the given Vector has no
// length, this returns a Zero() Vector.
template <int Dimension, typename T>
Vector<Dimension, T> Normalized(const Vector<Dimension, T>& v) {
  Vector<Dimension, T> result = v;
  if (Normalize(&result))
    return result;
  else
    return Vector<Dimension, T>::Zero();
}

// Returns an unnormalized Vector2 that is orthonormal to the passed one. If the
// passed vector has length 0, then a zero-length vector is returned.
template <typename T>
Vector<2, T> Orthogonal(const Vector<2, T>& v) {
  return Vector<2, T>(-v[1], v[0]);
}

// Returns an unnormalized Vector3 that is orthonormal to the passed one. If the
// passed vector has length 0, then a zero-length vector is returned. The
// returned vector is not guaranteed to be in any particular direction, just
// that it is perpendicular to v.
template <typename T>
Vector<3, T> Orthogonal(const Vector<3, T>& v) {
  static const T kTolerance = static_cast<T>(0.0001);
  Vector<3, T> n = Cross(v, Vector<3, T>::AxisX());
  if (Length(n) < kTolerance) {
    n = Cross(v, Vector<3, T>::AxisY());
    if (Length(n) < kTolerance)
      n = Cross(v, Vector<3, T>::AxisZ());
  }
  return n;
}

// Returns a normalized Vector that is orthonormal to the passed one. If the
// passed vector has length 0, then a zero-length vector is returned. The
// returned vector is not guaranteed to be in any particular direction, just
// that it is perpendicular to v.
template <int Dimension, typename T>
Vector<Dimension, T> Orthonormal(const Vector<Dimension, T>& v) {
  return Normalized(Orthogonal(v));
}

// Returns the Vector resulting from projecting of one Vector onto another.
// This will return a Zero() Vector if onto_v has zero length.
template <int Dimension, typename T>
Vector<Dimension, T> Projection(const Vector<Dimension, T>& v,
                                const Vector<Dimension, T>& onto_v) {
  const T len_squared = LengthSquared(onto_v);
  return len_squared == static_cast<T>(0) ? Vector<Dimension, T>::Zero() :
      (Dot(v, onto_v) / len_squared) * onto_v;
}

// Returns a Vector in the same direction as the passed vector but with the
// passed length. If the input vector has zero length, however, then the
// returned vector also has zero length.
template <int Dimension, typename T>
Vector<Dimension, T> Rescale(const Vector<Dimension, T>& v, T length) {
  return Normalized(v) * length;
}

// Returns true if all elements of two Vectors are equal within a tolerance.
template <int Dimension, typename T>
bool AlmostEqual(const Vector<Dimension, T>& v0,
                 const Vector<Dimension, T>& v1, T tolerance) {
  for (int i = 0; i < Dimension; ++i) {
    if (!AlmostEqual(v0[i], v1[i], tolerance)) return false;
  }
  return true;
}

// Deprecated alias for AlmostEqual with vector parameters.
template <int Dimension, typename T>
bool VectorsAlmostEqual(const Vector<Dimension, T>& v0,
                        const Vector<Dimension, T>& v1, T tolerance) {
  return AlmostEqual(v0, v1, tolerance);
}

// Returns a Point in which each element is the minimum of the corresponding
// elements of two Points. This is useful for computing bounding boxes.
template <int Dimension, typename T>
Point<Dimension, T> MinBoundPoint(const Point<Dimension, T>& p0,
                                  const Point<Dimension, T>& p1) {
  Point<Dimension, T> min_point;
  vector_internal::Unroller<T>::template VectorOp<
      typename vector_internal::Unroller<T>::Smaller>(
      min_point.Data(), p0.Data(), p1.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return min_point;
}

// Returns a Point in which each element is the maximum of the corresponding
// elements of two Points. This is useful for computing bounding boxes.
template <int Dimension, typename T>
Point<Dimension, T> MaxBoundPoint(const Point<Dimension, T>& p0,
                                  const Point<Dimension, T>& p1) {
  Point<Dimension, T> max_point;
  vector_internal::Unroller<T>::template VectorOp<
      typename vector_internal::Unroller<T>::Larger>(
      max_point.Data(), p0.Data(), p1.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return max_point;
}

// Returns the closest point to p on the line segment defined by start and end.
template <int Dimension, typename T>
Point<Dimension, T> ClosestPointOnSegment(const Point<Dimension, T>& p,
                                          const Point<Dimension, T>& start,
                                          const Point<Dimension, T>& end) {
  const Vector<Dimension, T> diff = end - start;
  if (LengthSquared(diff) == static_cast<T>(0))
    return start;

  const Vector<Dimension, T> to_min = p - start;
  const T projection = Dot(to_min, diff);
  const T length_squared = Dot(diff, diff);
  if (projection <= static_cast<T>(0)) {
    return start;
  } else if (length_squared <= projection) {
    return end;
  } else {
    const T t = projection / length_squared;
    return start + t * diff;
  }
}

// Returns the squared distance from a Point to a line segment described by two
// Points.
template <int Dimension, typename T>
T DistanceSquaredToSegment(const Point<Dimension, T>& p,
                           const Point<Dimension, T>& start,
                           const Point<Dimension, T>& end) {
  return DistanceSquared(p, ClosestPointOnSegment(p, start, end));
}

// Returns the distance from a Point to a line segment described by two Points.
template <int Dimension, typename T>
T DistanceToSegment(const Point<Dimension, T>& p,
                    const Point<Dimension, T>& start,
                    const Point<Dimension, T>& end) {
  return Distance(p, ClosestPointOnSegment(p, start, end));
}

// Returns true if all elements of two Points are equal within a tolerance.
template <int Dimension, typename T>
bool AlmostEqual(const Point<Dimension, T>& v0,
                 const Point<Dimension, T>& v1, T tolerance) {
  for (int i = 0; i < Dimension; ++i) {
    if (!AlmostEqual(v0[i], v1[i], tolerance)) return false;
  }
  return true;
}

// Alias for AlmostEqual with point parameters.
template <int Dimension, typename T>
bool PointsAlmostEqual(const Point<Dimension, T>& v0,
                       const Point<Dimension, T>& v1, T tolerance) {
  return AlmostEqual(v0, v1, tolerance);
}

// Computes the result of swizzling a Vector or Point (or anything else derived
// from VectorBase). The swizzle_string determines the contents of the output
// vector. Each character in the string must be one of {x,y,z,w} or {r,g,b,a}
// or {s,t,p,q}, upper- or lower-case, specifying a component of the input
// vector. Extra components in the string are ignored; missing components in
// the string result in an error. This returns false on error.
//
// For example:
//   Vec3d v3(1.0, 2.0, 3.0);
//   Vec2d v2;
//   Vec4d v4;
//   Swizzle(v3, "xz", &v2);    // v2 is set to (1.0, 3.0).
//   Swizzle(v3, "BBYX", &v4);  // v4 is set to (3.0, 3.0, 2.0, 1.0).
//   Swizzle(v3, "xyz", &v2);   // v2 is set to (1.0, 2.0); 'z' is ignored.
//   Swizzle(v3, "xw", &v2);    // Returns false: v3 has no "w" component.
//   Swizzle(v3, "x", &v2);     // Returns false: string is missing a component.
template <int InDimension, int OutDimension, typename T>
bool Swizzle(const VectorBase<InDimension, T>& input,
             const char* swizzle_string, VectorBase<OutDimension, T>* output) {
  for (int i = 0; i < OutDimension; ++i) {
    int dim;
    switch (swizzle_string[i]) {
      case 'x': case 'r' : case 's':
      case 'X': case 'R' : case 'S':
        dim = 0;
        break;
      case 'y': case 'g' : case 't':
      case 'Y': case 'G' : case 'T':
        dim = 1;
        break;
      case 'z': case 'b' : case 'p':
      case 'Z': case 'B' : case 'P':
        dim = 2;
        break;
      case 'w': case 'a' : case 'q':
      case 'W': case 'A' : case 'Q':
        dim = 3;
        break;
      default:
        // This handles the NUL character and avoids reading past the end of
        // the string.
        return false;
    }
    if (dim >= InDimension)
      return false;
    (*output)[i] = input[dim];
  }
  return true;
}

// Returns true if all components of VectorBase v are finite, otherwise false.
template <int Dimension, typename T>
bool IsVectorFinite(const VectorBase<Dimension, T>& v) {
  for (int i = 0; i < Dimension; ++i) {
    if (!IsFinite(v[i]))
      return false;
  }
  return true;
}

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_VECTORUTILS_H_
