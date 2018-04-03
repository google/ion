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

#ifndef ION_BASE_TYPE_STRUCTS_H_
#define ION_BASE_TYPE_STRUCTS_H_

// These structs implement various type traits needed by Ion. These are
// typically available in standard header files.  However:
//   - c++11 versions are not yet available everywhere.
//   - The tr1 versions are usable everywhere but do not provide a conditional
//     type implementation.
//
// Rather than use a hodgepodge of different implementations, we just define
// the ones we need here in a fairly straightforward way.

namespace ion {
namespace base {

// BoolType is a struct whose value member is either true or false.
template <bool b> struct BoolType { static const bool value = b; };
template <bool b> const bool BoolType<b>::value;

// IsSameType is similar to std::is_same. IsSameType<T, U>::value is true iff T
// and U refer to the same type.
template<typename T, typename U> struct IsSameType : public BoolType<false> {};
template<typename T> struct IsSameType<T, T> : public BoolType<true> {};

// IsBaseOf is similar to std::is_base_of. IsBaseOf<Base, Derived>::value is
// true iff Base and Derived are different types and Base is a base class of
// Derived.
template <typename Base, typename Derived> struct IsBaseOf {
  // Because the Derived type-cast operator is non-const, it is preferred over
  // the Base operator when calling Test() below.
  struct Helper {
    operator Base*() const;
    operator Derived*();
  };
  // Overloaded function that chooses the Derived version iff Derived is
  // actually derived from Base. The size of the return type is used to tell
  // which version is selected.
  template <typename T> static char Test(Derived*, T);
  static int Test(Base*, int);
  static const bool value = sizeof(Test(Helper(), 0)) == 1;
};
template <typename Base, typename Derived>
const bool IsBaseOf<Base, Derived>::value;

// IsConvertible is similar to std::is_convertible, except that it only looks at
// direct inheritance relationships (e.g. it doesn't account for conversion
// operators). IsConvertible<From, To>::value is true iff either
// IsSameType<To, From>::value or IsBaseOf<To, From>::value is true.
template <typename From, typename To>
struct IsConvertible : public BoolType<IsSameType<To, From>::value ||
                                       IsBaseOf<To, From>::value> {};

// ConditionalType is similar to std::conditional.
// ConditionalType<condition, A, B>::Type is A if condition is true, and B if
// it is false.
template<bool condition, typename A, typename B>
struct ConditionalType { typedef A Type; };
template<typename A, typename B>
struct ConditionalType<false, A, B> { typedef B Type; };

// HasTrivialDestructor is similar to std::has_trivial_destructor or
// std::is_trivially_destructible. Unfortunately, some STL implementations use
// std::has_trivial_destructor while some use std::is_trivially_destructible,
// and still others have neither. This version simply uses the builtin that is
// available on all platforms.
template <typename T> struct HasTrivialDestructor {
  static const bool value = __has_trivial_destructor(T);
};
template <typename T>
const bool HasTrivialDestructor<T>::value;

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_TYPE_STRUCTS_H_
