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

#ifndef ION_MATH_VECTOR_H_
#define ION_MATH_VECTOR_H_

//
// This file defines geometric N-dimensional Vector and Point classes. Each
// class in this file is templatized on dimension (number of elements) and
// scalar value type.
//

#include <functional>
#include <type_traits>

#include "base/integral_types.h"
#include "ion/base/logging.h"
#include "ion/base/scalarsequence.h"
#include "ion/base/static_assert.h"
#include "ion/base/stringutils.h"

namespace ion {
namespace math {

//-----------------------------------------------------------------------------
// VectorBase.
//-----------------------------------------------------------------------------

// VectorBase is a base class for the Vector and Point classes.
template <int Dimension, typename T>
class VectorBase {
 public:
  // The dimension of the vector (number of elements).
  enum { kDimension = Dimension };
  typedef T ValueType;

  // Sets the vector values.
  void Set(T e0);                    // Only when Dimension == 1.
  void Set(T e0, T e1);              // Only when Dimension == 2.
  void Set(T e0, T e1, T e2);        // Only when Dimension == 3.
  void Set(T e0, T e1, T e2, T e3);  // Only when Dimension == 4.

  // Mutable element accessor.
  T& operator[](int index) {
    // Check that the index is in range. Use a single DCHECK statement with a
    // conjunction rather than multiple DCHECK_GE and DCHECK_LT statements,
    // since the latter seem to occasionally prevent Visual C++ from inlining
    // the operator in opt builds.
    DCHECK(index >= 0 && index < Dimension);
    return elem_[index];
  }

  // Read-only element accessor.
  const T& operator[](int index) const {
    // Check that the index is in range. Use a single DCHECK statement with a
    // conjunction rather than multiple DCHECK_GE and DCHECK_LT statements,
    // since the latter seem to occasionally prevent Visual C++ from inlining
    // the operator in opt builds.
    DCHECK(index >= 0 && index < Dimension);
    return elem_[index];
  }

  // Returns true if all values in two instances are equal.
  static bool AreValuesEqual(const VectorBase& v0, const VectorBase& v1);

  // Returns a pointer to the data for interfacing with other libraries.
  T* Data() { return &elem_[0]; }
  const T* Data() const { return &elem_[0]; }

  // This is used for printing Vectors and Points to a stream.
  void Print(std::ostream& out, const char tag) const {  // NOLINT
    out << tag << "[";
    for (int i = 0; i < Dimension; ++i) {
      out << +elem_[i];
      if (i != Dimension - 1)
        out << ", ";
    }
    out << "]";
  }

  // This is used for reading Vectors and Points from a stream.
  template <char tag>
  void Read(std::istream& in) {  // NOLINT
    VectorBase v;
    if (base::GetExpectedChar<tag>(in) && base::GetExpectedChar<'['>(in)) {
      for (int i = 0; i < Dimension; ++i) {
        in >> v.elem_[i];
        if (i != Dimension - 1 && !base::GetExpectedChar<','>(in))
          return;
      }
      if (base::GetExpectedChar<']'>(in))
        *this = v;
    }
  }

 protected:
  // The default constructor zero-initializes all elements.
  VectorBase() noexcept : elem_() {}

  constexpr explicit VectorBase(T e0);           // Only when Dimension == 1.
  constexpr VectorBase(T e0, T e1);              // Only when Dimension == 2.
  constexpr VectorBase(T e0, T e1, T e2);        // Only when Dimension == 3.
  constexpr VectorBase(T e0, T e1, T e2, T e3);  // Only when Dimension == 4.

  // Constructor for an instance of dimension N from an instance of dimension
  // N-1 and a scalar of the correct type. This is defined only when Dimension
  // is at least 2.
  constexpr VectorBase(const VectorBase<Dimension - 1, T>& v, T s);

  // Copy constructor from an instance of the same Dimension and any value type
  // that is compatible (via static_cast) with this instance's type.
  template <typename U>
  constexpr explicit VectorBase(const VectorBase<Dimension, U>& v);

  // Visual C++ 2015 Update 3 appears to have problems with overloaded
  // constructors with variadic template parameters.  By separating out the
  // constructors into separate initialization scopes, we avoid the problems
  // with that compiler.

  // Constructor helper to create a VectorBase of a different type, but same
  // dimensionality.
  template <typename U>
  struct InitCast {
    static constexpr VectorBase<Dimension, T> Invoke(
        const VectorBase<Dimension, U>& source) {
      return Invoke(
          typename base::ScalarSequenceGenerator<size_t, Dimension>::Sequence(),
          source);
    }

    template <size_t... index>
    static constexpr VectorBase<Dimension, T> Invoke(
        base::ScalarSequence<size_t, index...>,
        const VectorBase<Dimension, U>& source) {
      return VectorBase<Dimension, T>(static_cast<T>(source[index])...);
    }
  };

  // Constructor helper to create a VectorBase from a lower-demension vector of
  // the same type, with an extra parameter.
  struct InitRaiseDimension {
    static constexpr VectorBase<Dimension, T> Invoke(
        const VectorBase<Dimension - 1, T>& source, T s) {
      return Invoke(
          typename base::ScalarSequenceGenerator<size_t,
                                                 Dimension - 1>::Sequence(),
          source, s);
    }

    template <size_t... index>
    static constexpr VectorBase<Dimension, T> Invoke(
        base::ScalarSequence<size_t, index...>,
        const VectorBase<Dimension - 1, T>& source, T s) {
      return VectorBase<Dimension, T>(source[index]..., s);
    }
  };

  // Returns an instance containing all zeroes.
  static VectorBase Zero();

  // Returns an instance with all elements set to the given value.
  static VectorBase Fill(T value);

  //
  // Derived classes use these protected functions to implement type-safe
  // functions and operators.
  //

  // Self-modifying addition.
  void Add(const VectorBase& v);
  // Self-modifying subtraction.
  void Subtract(const VectorBase& v);
  // Self-modifying multiplication by a scalar.
  void Multiply(T s);
  // Self-modifying division by a scalar.
  void Divide(T s);

  // Unary negation.
  VectorBase Negation() const;

  // Binary component-wise multiplication.
  static VectorBase Product(const VectorBase& v0, const VectorBase& v1);
  // Binary component-wise division.
  static VectorBase Quotient(const VectorBase& v0, const VectorBase& v1);
  // Binary component-wise addition.
  static VectorBase Sum(const VectorBase& v0, const VectorBase& v1);
  // Binary component-wise subtraction.
  static VectorBase Difference(const VectorBase& v0, const VectorBase& v1);
  // Binary multiplication by a scalar.
  static VectorBase Scale(const VectorBase& v, T s);
  // Binary division by a scalar.
  static VectorBase Divide(const VectorBase& v, T s);
  // Reciprocal and multiplication by a scalar, i.e., s / v.
  static VectorBase LeftDivide(T s, const VectorBase& v);

  // Helper struct to aid in Axis functions. It is a struct to allow partial
  // template specialization.
  template <int Dim, typename U>
  struct StaticHelper;

 private:
  // Constructor taking elements from a lower dimension vector, and adding an
  // extra dimension supplied by a parameter.  This constructor is used by the
  // regular dimesional conversion constructor, as a utility for constexpr
  // support.
  template <size_t... index>
  constexpr explicit VectorBase(base::ScalarSequence<size_t, index...>,
                                const VectorBase<Dimension - 1, T>& v, T last);

  // Constructor converting a indexable type to a vector of another type, using
  // a ScalarSequence as a variadic index into the source vector.
  template <typename U, size_t... index>
  constexpr VectorBase(base::ScalarSequence<size_t, index...>, const U& v);

  // Element storage.
  T elem_[Dimension];
};

//-----------------------------------------------------------------------------
// Vector.
//-----------------------------------------------------------------------------

// Geometric N-dimensional Vector class.
template <int Dimension, typename T>
class Vector : public VectorBase<Dimension, T> {
 public:
  // The default constructor zero-initializes all elements.
  Vector() noexcept : BaseType() {}

  // Dimension-specific constructors that are passed individual element values.
  constexpr explicit Vector(T e0) : BaseType(e0) {}
  constexpr Vector(T e0, T e1) : BaseType(e0, e1) {}
  constexpr Vector(T e0, T e1, T e2) : BaseType(e0, e1, e2) {}
  constexpr Vector(T e0, T e1, T e2, T e3) : BaseType(e0, e1, e2, e3) {}

  // Constructor for a Vector of dimension N from a Vector of dimension N-1 and
  // a scalar of the correct type, assuming N is at least 2.
  constexpr Vector(const Vector<Dimension - 1, T>& v, T s) : BaseType(v, s) {}

  // Copy constructor from a Vector of the same Dimension and any value type
  // that is compatible (via static_cast) with this Vector's type.
  template <typename U>
  constexpr explicit Vector(const VectorBase<Dimension, U>& v) : BaseType(v) {}

  // Returns a Vector containing all zeroes.
  static Vector Zero() { return ToVector(BaseType::Zero()); }

  // Returns a Vector with all elements set to the given value.
  static Vector Fill(T value) { return ToVector(BaseType::Fill(value)); }

  // Returns a Vector representing the X axis.
  static Vector AxisX();
  // Returns a Vector representing the Y axis if it exists.
  static Vector AxisY();
  // Returns a Vector representing the Z axis if it exists.
  static Vector AxisZ();
  // Returns a Vector representing the W axis if it exists.
  static Vector AxisW();

  // Self-modifying operators.
  void operator+=(const Vector& v) { BaseType::Add(v); }
  void operator-=(const Vector& v) { BaseType::Subtract(v); }
  void operator*=(T s) { BaseType::Multiply(s); }
  void operator/=(T s) { BaseType::Divide(s); }

  // Unary negation operator.
  Vector operator-() const { return ToVector(BaseType::Negation()); }

  // Binary operators.
  friend Vector operator+(const Vector& v0, const Vector& v1) {
    return ToVector(BaseType::Sum(v0, v1));
  }
  friend Vector operator-(const Vector& v0, const Vector& v1) {
    return ToVector(BaseType::Difference(v0, v1));
  }
  friend Vector operator*(const Vector& v, T s) {
    return ToVector(BaseType::Scale(v, s));
  }
  friend Vector operator*(T s, const Vector& v) {
    // Assume the commutative property holds for type T.
    return ToVector(BaseType::Scale(v, s));
  }
  friend Vector operator*(const Vector& v, const Vector& s) {
    return ToVector(BaseType::Product(v, s));
  }
  friend Vector operator/(const Vector& v, const Vector& s) {
    return ToVector(BaseType::Quotient(v, s));
  }
  friend Vector operator/(const Vector& v, T s) {
    return ToVector(BaseType::Divide(v, s));
  }
  friend Vector operator/(T s, const Vector& v) {
    return ToVector(BaseType::LeftDivide(s, v));
  }

  // Exact equality and inequality comparisons.
  friend bool operator==(const Vector& v0, const Vector& v1) {
    return BaseType::AreValuesEqual(v0, v1);
  }
  friend bool operator!=(const Vector& v0, const Vector& v1) {
    return !BaseType::AreValuesEqual(v0, v1);
  }

 private:
  // Type this is derived from.
  typedef VectorBase<Dimension, T> BaseType;

  // Converts a VectorBase of the correct type to a Vector.
  static Vector ToVector(const BaseType& b) {
    // This is safe because Vector is the same size as VectorBase and has no
    // virtual functions. It's ugly, but better than the alternatives.
    return static_cast<const Vector&>(b);
  }

  // Allow Point to convert BaseType to Vector.
  template<int D, typename U> friend class Point;
};

// Prints a Vector to a stream.
template <int Dimension, typename T>
std::ostream& operator<<(std::ostream& out, const Vector<Dimension, T>& v) {
  v.Print(out, 'V');
  return out;
}

// Reads a Vector from a stream.
template <int Dimension, typename T>
std::istream& operator>>(std::istream& in, Vector<Dimension, T>& v) {
  v. template Read<'V'>(in);
  return in;
}

//-----------------------------------------------------------------------------
// Point.
//-----------------------------------------------------------------------------

// Geometric N-dimensional Point class.
template <int Dimension, typename T>
class Point : public VectorBase<Dimension, T> {
 public:
  // Convenience typedef for the corresponding Vector type.
  typedef Vector<Dimension, T> VectorType;

  // The default constructor zero-intializes all elements.
  Point() noexcept : BaseType() {}

  // Dimension-specific constructors that are passed individual element values.
  constexpr explicit Point(T e0) : BaseType(e0) {}
  constexpr Point(T e0, T e1) : BaseType(e0, e1) {}
  constexpr Point(T e0, T e1, T e2) : BaseType(e0, e1, e2) {}
  constexpr Point(T e0, T e1, T e2, T e3) : BaseType(e0, e1, e2, e3) {}

  // Constructor for a Point of dimension N from a Point of dimension N-1 and
  // a scalar of the correct type, assuming N is at least 2.
  constexpr Point(const Point<Dimension - 1, T>& p, T s) : BaseType(p, s) {}

  // Copy constructor from a Point of the same Dimension and any value type
  // that is compatible (via static_cast) with this Point's type.
  template <typename U>
  constexpr explicit Point(const VectorBase<Dimension, U>& p) : BaseType(p) {}

  // Returns a Point containing all zeroes.
  static Point Zero() { return ToPoint(BaseType::Zero()); }

  // Returns a Point with all elements set to the given value.
  static Point Fill(T value) { return ToPoint(BaseType::Fill(value)); }

  // Self-modifying operators.
  void operator+=(const Point& v) { BaseType::Add(v); }
  void operator+=(const VectorType& v) { BaseType::Add(v); }
  void operator-=(const VectorType& v) { BaseType::Subtract(v); }
  void operator*=(T s) { BaseType::Multiply(s); }
  void operator/=(T s) { BaseType::Divide(s); }

  // Unary operators.
  Point operator-() const { return ToPoint(BaseType::Negation()); }

  // Adding two Points produces another Point.
  friend Point operator+(const Point& p0, const Point& p1) {
    return ToPoint(BaseType::Sum(p0, p1));
  }
  // Adding a Vector to a Point produces another Point.
  friend Point operator+(const Point& p, const VectorType& v) {
    return ToPoint(BaseType::Sum(p, v));
  }
  friend Point operator+(const VectorType& v, const Point& p) {
    return ToPoint(BaseType::Sum(p, v));
  }

  // Subtracting a Vector from a Point produces another Point.
  friend Point operator-(const Point& p, const VectorType& v) {
    return ToPoint(BaseType::Difference(p, v));
  }
  // Subtracting two Points results in a Vector.
  friend VectorType operator-(const Point& p0, const Point& p1) {
    return ToVector(BaseType::Difference(p0, p1));
  }

  // Binary scale and division operators.
  friend Point operator*(const Point& p, T s) {
    return ToPoint(BaseType::Scale(p, s));
  }
  friend Point operator*(T s, const Point& p) {
    // Assume the commutative property holds for type T.
    return ToPoint(BaseType::Scale(p, s));
  }
  friend Point operator*(const Point& v, const Point& s) {
    return ToPoint(BaseType::Product(v, s));
  }
  friend Point operator/(const Point& v, const Point& s) {
    return ToPoint(BaseType::Quotient(v, s));
  }
  friend Point operator/(const Point& p, T s) {
    return ToPoint(BaseType::Divide(p, s));
  }

  // Exact equality and inequality comparisons.
  friend bool operator==(const Point& p0, const Point& p1) {
    return BaseType::AreValuesEqual(p0, p1);
  }
  friend bool operator!=(const Point& p0, const Point& p1) {
    return !BaseType::AreValuesEqual(p0, p1);
  }

 private:
  // Type this is derived from.
  typedef VectorBase<Dimension, T> BaseType;

  // Converts a VectorBase of the correct type to a Point.
  static Point ToPoint(const BaseType& b) {
    // This is safe because Point is the same size as VectorBase and has no
    // virtual functions. It's ugly, but better than the alternatives.
    return *static_cast<const Point*>(&b);
  }
  // Converts a VectorBase of the correct type to the corresponding Vector type.
  static VectorType ToVector(const BaseType& b) {
    return VectorType::ToVector(b);
  }
};

// Prints a Point to a stream.
template <int Dimension, typename T>
std::ostream& operator<<(std::ostream& out, const Point<Dimension, T>& p) {
  p.Print(out, 'P');
  return out;
}

// Reads a Point from a stream.
template <int Dimension, typename T>
std::istream& operator>>(std::istream& in, Point<Dimension, T>& v) {
  v. template Read<'P'>(in);
  return in;
}

//------------------------------------------------------------------------------
// Optimization helpers.
//------------------------------------------------------------------------------
namespace vector_internal {

// Unroller is a helper struct for compile-type force-unrolling of loops, using
// indexed recursion.
template <typename T>
struct Unroller {
  // General operators.
  template <typename Op>
  static void ScalarOp(T* a, const T* b, const T v, base::ScalarSequence<int>) {
  }
  template <typename Op, int I, int... Is>
  static void ScalarOp(T* a, const T* b, const T v,
                       base::ScalarSequence<int, I, Is...>) {
    a[I] = Op{}(b[I], v);
    ScalarOp<Op>(a, b, v, base::ScalarSequence<int, Is...>{});
  }

  template <typename Op>
  static void ScalarLeftOp(T* a, const T* b, const T v,
                           base::ScalarSequence<int>) {}
  template <typename Op, int I, int... Is>
  static void ScalarLeftOp(T* a, const T* b, const T v,
                           base::ScalarSequence<int, I, Is...>) {
    a[I] = Op{}(v, b[I]);
    ScalarLeftOp<Op>(a, b, v, base::ScalarSequence<int, Is...>{});
  }

  template <typename Op>
  static void VectorOp(T* a, const T* b, const T* c,
                       base::ScalarSequence<int>) {}
  template <typename Op, int I, int... Is>
  static void VectorOp(T* a, const T* b, const T* c,
                       base::ScalarSequence<int, I, Is...>) {
    a[I] = Op{}(b[I], c[I]);
    VectorOp<Op>(a, b, c, base::ScalarSequence<int, Is...>{});
  }

  template <typename Op>
  static void UnaryOp(T* a, const T* b, base::ScalarSequence<int>) {}
  template <typename Op, int I, int... Is>
  static void UnaryOp(T* a, const T* b, base::ScalarSequence<int, I, Is...>) {
    a[I] = Op{}(b[I]);
    UnaryOp<Op>(a, b, base::ScalarSequence<int, Is...>{});
  }

  template <typename Op>
  static bool BooleanOp(const T* a, const T* b, base::ScalarSequence<int>) {
    return true;
  }
  template <typename Op, int I, int... Is>
  static bool BooleanOp(const T* a, const T* b,
                        base::ScalarSequence<int, I, Is...>) {
    if (Op{}(a[I], b[I])) return false;
    return BooleanOp<Op>(a, b, base::ScalarSequence<int, Is...>{});
  }

  // Specialized operators that don't fit the above pattern.
  static T Dot(T sum, const T* a, const T* b, base::ScalarSequence<int>) {
    return sum;
  }
  template <int I, int... Is>
  static T Dot(T sum, const T* a, const T* b,
               base::ScalarSequence<int, I, Is...>) {
    return Dot(sum + a[I] * b[I], a, b, base::ScalarSequence<int, Is...>{});
  }

  static void Fill(T* a, const T b, base::ScalarSequence<int>) {}
  template <int I, int... Is>
  static void Fill(T* a, const T b, base::ScalarSequence<int, I, Is...>) {
    a[I] = b;
    Fill(a, b, base::ScalarSequence<int, Is...>{});
  }

  // Helper structs for use with the above.
  struct Smaller {
    constexpr const T& operator()(const T& a, const T& b) const {
      return std::min(a, b);
    }
  };
  struct Larger {
    constexpr const T& operator()(const T& a, const T& b) const {
      return std::max(a, b);
    }
  };
  struct IsNotFinite {
    constexpr const T& operator()(const T& a) const { return !IsFinite(a); }
  };
};

}  // namespace vector_internal

//------------------------------------------------------------------------------
// VectorBase implementation.
//------------------------------------------------------------------------------

template <int Dimension, typename T>
constexpr VectorBase<Dimension, T>::VectorBase(T e0) : elem_{e0} {
  static_assert(Dimension == 1, "Bad Dimension in VectorBase constructor");
}

template <int Dimension, typename T>
constexpr VectorBase<Dimension, T>::VectorBase(T e0, T e1) : elem_{e0, e1} {
  static_assert(Dimension == 2, "Bad Dimension in VectorBase constructor");
}

template <int Dimension, typename T>
constexpr VectorBase<Dimension, T>::VectorBase(T e0, T e1, T e2)
    : elem_{e0, e1, e2} {
  static_assert(Dimension == 3, "Bad Dimension in VectorBase constructor");
}

template <int Dimension, typename T>
constexpr VectorBase<Dimension, T>::VectorBase(T e0, T e1, T e2, T e3)
    : elem_{e0, e1, e2, e3} {
  static_assert(Dimension == 4, "Bad Dimension in VectorBase constructor");
}

template <int Dimension, typename T>
constexpr VectorBase<Dimension, T>::VectorBase(
    const VectorBase<Dimension - 1, T>& v, T s)
    : VectorBase(InitRaiseDimension::Invoke(v, s)) {
  static_assert(Dimension >= 2, "Bad Dimension in VectorBase constructor");
}

template <int Dimension, typename T>
template <typename U>
constexpr VectorBase<Dimension, T>::VectorBase(
    const VectorBase<Dimension, U>& v)
    : VectorBase(InitCast<U>::Invoke(v)) {}

template <int Dimension, typename T>
void VectorBase<Dimension, T>::Set(T e0) {
  static_assert(Dimension == 1, "Bad Dimension in VectorBase::Set");
  elem_[0] = e0;
}

template <int Dimension, typename T>
void VectorBase<Dimension, T>::Set(T e0, T e1) {
  static_assert(Dimension == 2, "Bad Dimension in VectorBase::Set");
  elem_[0] = e0;
  elem_[1] = e1;
}

template <int Dimension, typename T>
void VectorBase<Dimension, T>::Set(T e0, T e1, T e2) {
  static_assert(Dimension == 3, "Bad Dimension in VectorBase::Set");
  elem_[0] = e0;
  elem_[1] = e1;
  elem_[2] = e2;
}

template <int Dimension, typename T>
void VectorBase<Dimension, T>::Set(T e0, T e1, T e2, T e3) {
  static_assert(Dimension == 4, "Bad Dimension in VectorBase::Set");
  elem_[0] = e0;
  elem_[1] = e1;
  elem_[2] = e2;
  elem_[3] = e3;
}

// Specializations to help with static functions.
template <int Dimension, typename T>
template <typename U>
struct VectorBase<Dimension, T>::StaticHelper<1, U> {
  typedef math::Vector<1, U> Vector;
  static Vector AxisX() { return Vector(static_cast<U>(1)); }
};

template <int Dimension, typename T>
template <typename U>
struct VectorBase<Dimension, T>::StaticHelper<2, U> {
  typedef math::Vector<2, U> Vector;
  static Vector AxisX() { return Vector(static_cast<U>(1), static_cast<U>(0)); }
  static Vector AxisY() { return Vector(static_cast<U>(0), static_cast<U>(1)); }
};

template <int Dimension, typename T>
template <typename U>
struct VectorBase<Dimension, T>::StaticHelper<3, U> {
  typedef math::Vector<3, U> Vector;
  static Vector AxisX() {
    return Vector(static_cast<U>(1), static_cast<U>(0), static_cast<U>(0));
  }
  static Vector AxisY() {
    return Vector(static_cast<U>(0), static_cast<U>(1), static_cast<U>(0));
  }
  static Vector AxisZ() {
    return Vector(static_cast<U>(0), static_cast<U>(0), static_cast<U>(1));
  }
};

template <int Dimension, typename T>
template <typename U>
struct VectorBase<Dimension, T>::StaticHelper<4, U> {
  typedef math::Vector<4, U> Vector;
  static Vector AxisX() {
    return Vector(static_cast<U>(1), static_cast<U>(0), static_cast<U>(0),
                  static_cast<U>(0));
  }
  static Vector AxisY() {
    return Vector(static_cast<U>(0), static_cast<U>(1), static_cast<U>(0),
                  static_cast<U>(0));
  }
  static Vector AxisZ() {
    return Vector(static_cast<U>(0), static_cast<U>(0), static_cast<U>(1),
                  static_cast<U>(0));
  }
  static Vector AxisW() {
    return Vector(static_cast<U>(0), static_cast<U>(0), static_cast<U>(0),
                  static_cast<U>(1));
  }
};

// The Vector::Axis? functions use the above static methods.
template <int Dimension, typename T>
Vector<Dimension, T> Vector<Dimension, T>::AxisX() {
  static const Vector<Dimension, T> v =
      BaseType::template StaticHelper<Dimension, T>::AxisX();
  return v;
}

template <int Dimension, typename T>
Vector<Dimension, T> Vector<Dimension, T>::AxisY() {
  static const Vector<Dimension, T> v =
      BaseType::template StaticHelper<Dimension, T>::AxisY();
  return v;
}

template <int Dimension, typename T>
Vector<Dimension, T> Vector<Dimension, T>::AxisZ() {
  static const Vector<Dimension, T> v =
      BaseType::template StaticHelper<Dimension, T>::AxisZ();
  return v;
}

template <int Dimension, typename T>
Vector<Dimension, T> Vector<Dimension, T>::AxisW() {
  static const Vector<Dimension, T> v =
      BaseType::template StaticHelper<Dimension, T>::AxisW();
  return v;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Zero() {
  static VectorBase<Dimension, T> v;
  return v;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Fill(T value) {
  VectorBase<Dimension, T> result;
  vector_internal::Unroller<T>::Fill(
      result.Data(), value,
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
void VectorBase<Dimension, T>::Add(const VectorBase& v) {
  vector_internal::Unroller<T>::template VectorOp<std::plus<T>>(
      Data(), Data(), v.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
}

template <int Dimension, typename T>
void VectorBase<Dimension, T>::Subtract(const VectorBase& v) {
  vector_internal::Unroller<T>::template VectorOp<std::minus<T>>(
      Data(), Data(), v.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
}

template <int Dimension, typename T>
void VectorBase<Dimension, T>::Multiply(T s) {
  vector_internal::Unroller<T>::template ScalarOp<std::multiplies<T>>(
      Data(), Data(), s,
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
}

template <int Dimension, typename T>
void VectorBase<Dimension, T>::Divide(T s) {
  vector_internal::Unroller<T>::template ScalarOp<std::divides<T>>(
      Data(), Data(), s,
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Negation() const {
  VectorBase<Dimension, T> result;
  vector_internal::Unroller<T>::template UnaryOp<std::negate<T>>(
      result.Data(), Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Product(
    const VectorBase& v0, const VectorBase& v1) {
  VectorBase result;
  vector_internal::Unroller<T>::template VectorOp<std::multiplies<T>>(
      result.Data(), v0.Data(), v1.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Quotient(
    const VectorBase& v0, const VectorBase& v1) {
  VectorBase result;
  vector_internal::Unroller<T>::template VectorOp<std::divides<T>>(
      result.Data(), v0.Data(), v1.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Sum(const VectorBase& v0,
                                                       const VectorBase& v1) {
  VectorBase result;
  vector_internal::Unroller<T>::template VectorOp<std::plus<T>>(
      result.Data(), v0.Data(), v1.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Difference(
    const VectorBase& v0, const VectorBase& v1) {
  VectorBase result;
  vector_internal::Unroller<T>::template VectorOp<std::minus<T>>(
      result.Data(), v0.Data(), v1.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Scale(const VectorBase& v,
                                                         T s) {
  VectorBase result;
  vector_internal::Unroller<T>::template ScalarOp<std::multiplies<T>>(
      result.Data(), v.Data(), s,
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::Divide(const VectorBase& v,
                                                          T s) {
  VectorBase result;
  vector_internal::Unroller<T>::template ScalarOp<std::divides<T>>(
      result.Data(), v.Data(), s,
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
VectorBase<Dimension, T> VectorBase<Dimension, T>::LeftDivide(
    T s, const VectorBase& v) {
  VectorBase result;
  vector_internal::Unroller<T>::template ScalarLeftOp<std::divides<T>>(
      result.Data(), v.Data(), s,
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
  return result;
}

template <int Dimension, typename T>
bool VectorBase<Dimension, T>::AreValuesEqual(const VectorBase& v0,
                                              const VectorBase& v1) {
  return vector_internal::Unroller<T>::template BooleanOp<std::not_equal_to<T>>(
      v0.Data(), v1.Data(),
      typename base::ScalarSequenceGenerator<int, Dimension>::Sequence{});
}

//------------------------------------------------------------------------------
// Dimension- and type-specific typedefs.
//------------------------------------------------------------------------------
#define ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type)              \
  static_assert(std::is_trivially_destructible<type>::value, \
                "Ion math type should be trivially destructible.");

#define ION_INSTANTIATE_VECTOR_TYPE(type)        \
  typedef type<1, int8> type##1i8;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##1i8)   \
  typedef type<1, uint8> type##1ui8;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##1ui8)  \
  typedef type<1, int16> type##1i16;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##1i16)  \
  typedef type<1, uint16> type##1ui16;           \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##1ui16) \
  typedef type<1, int32> type##1i;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##1i)    \
  typedef type<1, uint32> type##1ui;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##1ui)   \
  typedef type<1, float> type##1f;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##1f)    \
  typedef type<1, double> type##1d;              \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##1d)    \
  typedef type<2, int8> type##2i8;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##2i8)   \
  typedef type<2, uint8> type##2ui8;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##2ui8)  \
  typedef type<2, int16> type##2i16;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##2i16)  \
  typedef type<2, uint16> type##2ui16;           \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##2ui16) \
  typedef type<2, int32> type##2i;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##2i)    \
  typedef type<2, uint32> type##2ui;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##2ui)   \
  typedef type<2, float> type##2f;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##2f)    \
  typedef type<2, double> type##2d;              \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##2d)    \
  typedef type<3, int8> type##3i8;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##3i8)   \
  typedef type<3, uint8> type##3ui8;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##3ui8)  \
  typedef type<3, int16> type##3i16;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##3i16)  \
  typedef type<3, uint16> type##3ui16;           \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##3ui16) \
  typedef type<3, int32> type##3i;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##3i)    \
  typedef type<3, uint32> type##3ui;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##3ui)   \
  typedef type<3, float> type##3f;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##3f)    \
  typedef type<3, double> type##3d;              \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##3d)    \
  typedef type<4, int8> type##4i8;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##4i8)   \
  typedef type<4, uint8> type##4ui8;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##4ui8)  \
  typedef type<4, int16> type##4i16;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##4i16)  \
  typedef type<4, uint16> type##4ui16;           \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##4ui16) \
  typedef type<4, int32> type##4i;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##4i)    \
  typedef type<4, uint32> type##4ui;             \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##4ui)   \
  typedef type<4, float> type##4f;               \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##4f)    \
  typedef type<4, double> type##4d;              \
  ION_ENSURE_TRIVIALLY_DESTRUCTIBLE(type##4d)

ION_INSTANTIATE_VECTOR_TYPE(VectorBase);
ION_INSTANTIATE_VECTOR_TYPE(Vector);
ION_INSTANTIATE_VECTOR_TYPE(Point);

#undef ION_INSTANTIATE_VECTOR_TYPE
#undef ION_ENSURE_TRIVIALLY_DESTRUCTIBLE

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_VECTOR_H_
