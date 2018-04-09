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

#ifndef ION_MATH_MATRIX_H_
#define ION_MATH_MATRIX_H_

#include <cstring>  // For memcpy().
#include <istream>  // NOLINT
#include <ostream>  // NOLINT

#include "ion/base/logging.h"
#include "ion/base/scalarsequence.h"
#include "ion/base/static_assert.h"
#include "ion/base/stringutils.h"

namespace ion {
namespace math {

// The Matrix class defines a square N-dimensional matrix. Elements are stored
// in row-major order.
template <int Dimension, typename T>
class Matrix {
 public:
  // The dimension of the matrix (number of elements in a row or column).
  enum { kDimension = Dimension };
  typedef T ValueType;

  // The default constructor zero-initializes all elements.
  constexpr Matrix() : elem_() {}

  // Dimension-specific constructors that are passed individual element values.
  constexpr Matrix(T m00, T m01, T m10, T m11);  // Only when Dimension == 2.
  constexpr Matrix(T m00, T m01, T m02,          // Only when Dimension == 3.
                   T m10, T m11, T m12, T m20, T m21, T m22);
  constexpr Matrix(T m00, T m01, T m02, T m03,  // Only when Dimension == 4.
                   T m10, T m11, T m12, T m13, T m20, T m21, T m22, T m23,
                   T m30, T m31, T m32, T m33);

  // Constructor that reads elements from a linear array of the correct size.
  explicit Matrix(const T array[Dimension * Dimension]);

  // Copy constructor from an instance of the same Dimension and any value type
  // that is compatible (via static_cast) with this instance's type.
  template <typename U>
  constexpr explicit Matrix(const Matrix<Dimension, U>& other);

  // Returns a Matrix containing all zeroes.
  static Matrix Zero();

  // Returns an identity Matrix.
  static Matrix Identity();

  // Mutable element accessors.
  T& operator()(int row, int col) {
    CheckIndices(row, col);
    return elem_[row][col];
  }
  T* operator[](int row) {
    CheckIndices(row, 0);
    return elem_[row];
  }

  // Read-only element accessors.
  const T& operator()(int row, int col) const {
    CheckIndices(row, col);
    return elem_[row][col];
  }
  const T* operator[](int row) const {
    CheckIndices(row, 0);
    return elem_[row];
  }

  // Return a pointer to the data for interfacing with libraries.
  T* Data() { return &elem_[0][0]; }
  const T* Data() const { return &elem_[0][0]; }

  // Self-modifying multiplication operators.
  void operator*=(T s) { MultiplyScalar(s); }
  void operator*=(const Matrix& m) { *this = Product(*this, m); }

  // Unary operators.
  Matrix operator-() const { return Negation(); }

  // Binary scale operators.
  friend Matrix operator*(const Matrix& m, T s) { return Scale(m, s); }
  friend Matrix operator*(T s, const Matrix& m) { return Scale(m, s); }

  // Binary matrix addition.
  friend Matrix operator+(const Matrix& lhs, const Matrix& rhs) {
    return Addition(lhs, rhs);
  }

  // Binary matrix subtraction.
  friend Matrix operator-(const Matrix& lhs, const Matrix& rhs) {
    return Subtraction(lhs, rhs);
  }

  // Binary multiplication operator.
  friend Matrix operator*(const Matrix& m0, const Matrix& m1) {
    return Product(m0, m1);
  }

  // Exact equality and inequality comparisons.
  friend bool operator==(const Matrix& m0, const Matrix& m1) {
    return AreEqual(m0, m1);
  }
  friend bool operator!=(const Matrix& m0, const Matrix& m1) {
    return !AreEqual(m0, m1);
  }

 private:
  // Helper constructor that takes a ScalarSequence to index a raw matrix.
  template <typename U, size_t... index>
  constexpr Matrix(base::ScalarSequence<size_t, index...>,
                   const Matrix<Dimension, U>& other);

  // Validates indices for accessors.
  void CheckIndices(int row, int col) const {
    // Check that the indices are in range. Use a single DCHECK statement with a
    // conjunction rather than multiple DCHECK_GE and DCHECK_LT statements,
    // since the latter seem to occasionally prevent Visual C++ from inlining
    // the operator in opt builds.
    DCHECK(row >= 0 && row < Dimension && col >= 0 && col < Dimension);
  }

  // These private functions implement most of the operators.
  void MultiplyScalar(T s);
  Matrix Negation() const;
  static Matrix Addition(const Matrix& lhs, const Matrix& rhs);
  static Matrix Subtraction(const Matrix& lhs, const Matrix& rhs);
  static Matrix Scale(const Matrix& m, T s);
  static Matrix Product(const Matrix& m0, const Matrix& m1);
  static bool AreEqual(const Matrix& m0, const Matrix& m1);

  T elem_[Dimension][Dimension];
};

// Prints a Matrix to a stream.
template <int Dimension, typename T>
std::ostream& operator<<(std::ostream& out, const Matrix<Dimension, T>& m) {
  out << "M[";
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col) {
      out << +m(row, col);
      if (col != Dimension - 1)
        out << ", ";
    }
    if (row < Dimension - 1)
      out << " ; ";
  }
  return out << "]";
}

// Reads a Matrix from a stream.
template <int Dimension, typename T>
std::istream& operator>>(std::istream& in, Matrix<Dimension, T>& m) {
  Matrix<Dimension, T> mm;
  if (base::GetExpectedString(in, "M[")) {
    for (int row = 0; row < Dimension; ++row) {
      for (int col = 0; col < Dimension; ++col) {
        in >> mm[row][col];
        if (col != Dimension - 1 && !base::GetExpectedChar<','>(in))
          return in;
      }
      if (row != Dimension - 1 && !base::GetExpectedChar<';'>(in))
        return in;
    }
    if (base::GetExpectedChar<']'>(in))
      m = mm;
  }
  return in;
}

//------------------------------------------------------------------------------
// Implementation.
//------------------------------------------------------------------------------

template <int Dimension, typename T>
constexpr Matrix<Dimension, T>::Matrix(T m00, T m01, T m10, T m11)
    : elem_{{m00, m01}, {m10, m11}} {
  static_assert(Dimension == 2, "Bad Dimension in Matrix constructor");
}

template <int Dimension, typename T>
constexpr Matrix<Dimension, T>::Matrix(T m00, T m01, T m02, T m10, T m11, T m12,
                                       T m20, T m21, T m22)
    : elem_{{m00, m01, m02}, {m10, m11, m12}, {m20, m21, m22}} {
  static_assert(Dimension == 3, "Bad Dimension in Matrix constructor");
}

template <int Dimension, typename T>
constexpr Matrix<Dimension, T>::Matrix(T m00, T m01, T m02, T m03, T m10, T m11,
                                       T m12, T m13, T m20, T m21, T m22, T m23,
                                       T m30, T m31, T m32, T m33)
    : elem_{{m00, m01, m02, m03},
            {m10, m11, m12, m13},
            {m20, m21, m22, m23},
            {m30, m31, m32, m33}} {
  static_assert(Dimension == 4, "Bad Dimension in Matrix constructor");
}

template <int Dimension, typename T>
Matrix<Dimension, T>::Matrix(const T array[Dimension * Dimension]) {
#if defined(ION_PLATFORM_WINDOWS) && defined(ION_ARCH_X86_64)
  // 
  // memory alignment with x64_opt build using MSVC. Please see the bug
  // report for more detail and the bug reproduction steps.
  if ((reinterpret_cast<size_t>(&array[0]) & 0xF) != 0) {
    // |array| is NOT 16-byte aligned, avoid calling memcpy().
    for (int i = 0; i < Dimension; ++i) {
      for (int j = 0; j < Dimension; ++j) {
        elem_[i][j] = array[i*Dimension+j];
      }
    }
    return;
  }
#endif
  memcpy(elem_, array, sizeof(elem_));
}

template <int Dimension, typename T>
template <typename U>
constexpr Matrix<Dimension, T>::Matrix(const Matrix<Dimension, U>& other)
    : Matrix(typename base::ScalarSequenceGenerator<
                 size_t, Dimension * Dimension>::Sequence(),
             other) {}

template <int Dimension, typename T>
template <typename U, size_t... index>
constexpr Matrix<Dimension, T>::Matrix(base::ScalarSequence<size_t, index...>,
                                       const Matrix<Dimension, U>& other)
    : Matrix(static_cast<T>(other[index / Dimension][index % Dimension])...) {}

template <int Dimension, typename T>
Matrix<Dimension, T> Matrix<Dimension, T>::Zero() {
  Matrix result;
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result.elem_[row][col] = static_cast<T>(0);
  }
  return result;
}

template <int Dimension, typename T>
Matrix<Dimension, T> Matrix<Dimension, T>::Identity() {
  Matrix result;
  for (int row = 0; row < Dimension; ++row) {
    result.elem_[row][row] = static_cast<T>(1);
  }
  return result;
}

template <int Dimension, typename T>
void Matrix<Dimension, T>::MultiplyScalar(T s) {
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      elem_[row][col] *= s;
  }
}

template <int Dimension, typename T>
Matrix<Dimension, T> Matrix<Dimension, T>::Negation() const {
  Matrix result;
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result.elem_[row][col] = -elem_[row][col];
  }
  return result;
}

template <int Dimension, typename T>
Matrix<Dimension, T> Matrix<Dimension, T>::Scale(const Matrix& m, T s) {
  Matrix result;
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result.elem_[row][col] = m.elem_[row][col] * s;
  }
  return result;
}

template<int Dimension, typename T>
Matrix<Dimension, T> Matrix<Dimension, T>::Addition(
  const Matrix<Dimension, T> &lhs, const Matrix<Dimension, T> &rhs) {
  Matrix<Dimension, T> result;
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result.elem_[row][col] = lhs.elem_[row][col] + rhs.elem_[row][col];
  }
  return result;
}

template<int Dimension, typename T>
Matrix<Dimension, T> Matrix<Dimension, T>::Subtraction(
    const Matrix<Dimension, T> &lhs, const Matrix<Dimension, T> &rhs) {
  Matrix<Dimension, T> result;
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col)
      result.elem_[row][col] = lhs.elem_[row][col] - rhs.elem_[row][col];
  }
  return result;
}

template <int Dimension, typename T>
Matrix<Dimension, T> Matrix<Dimension, T>::Product(const Matrix& m0,
                                                   const Matrix& m1) {
  Matrix result = Matrix::Zero();
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col) {
      for (int i = 0; i < Dimension; ++i)
        result.elem_[row][col] += m0.elem_[row][i] * m1.elem_[i][col];
    }
  }
  return result;
}

template <int Dimension, typename T>
bool Matrix<Dimension, T>::AreEqual(const Matrix& m0, const Matrix& m1) {
  for (int row = 0; row < Dimension; ++row) {
    for (int col = 0; col < Dimension; ++col) {
      if (m0.elem_[row][col] != m1.elem_[row][col])
        return false;
    }
  }
  return true;
}

//------------------------------------------------------------------------------
// Dimension- and type-specific typedefs.
//------------------------------------------------------------------------------

typedef Matrix<2, float> Matrix2f;
typedef Matrix<2, double> Matrix2d;
typedef Matrix<3, float> Matrix3f;
typedef Matrix<3, double> Matrix3d;
typedef Matrix<4, float> Matrix4f;
typedef Matrix<4, double> Matrix4d;

}  // namespace math
}  // namespace ion

#endif  // ION_MATH_MATRIX_H_
