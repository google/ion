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

// Copyright 2008 Google Inc. All Rights Reserved.
//
// Various Google-specific macros.
//
// This code is compiled directly on many platforms, including client
// platforms like Windows, Mac, and embedded systems.  Before making
// any changes here, make sure that you're not breaking any platforms.

#ifndef BASE_MACROS_H_
#define BASE_MACROS_H_

// Users should still #include "base/macros.h".  Code in //absl/base
// // is not visible for general use.

#include "base/port.h"
#include "absl/base/macros.h"  // IWYU pragma: export

#ifdef SWIG
%include "absl/base/macros.h"
#endif  // SWIG

// ABSTRACT is equivalent to "= 0" in most builds.  Expands to {} in SWIG
// builds only.  Avoid this macro in APIs that are not SWIG wrapped.
#ifndef SWIG
#define ABSTRACT = 0
#endif

// COMPILE_ASSERT is provided for backwards compatibility.  New code
// should use static_assert instead.
#define COMPILE_ASSERT(expr, msg) static_assert((expr), #msg)

// DEPRECATED: Prefer the language-supported `= delete` syntax in the `public:`
// section of the class over these macros.
// More info: http://go/cppstyle#Copyable_Movable_Types
//
// DISALLOW_COPY_AND_ASSIGN disallows the copy constructor and copy assignment
// operator. DISALLOW_IMPLICIT_CONSTRUCTORS is like DISALLOW_COPY_AND_ASSIGN,
// but also disallows the default constructor, intended to help make a
// class uninstantiable.
//
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  TypeName& operator=(const TypeName&) = delete
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

// DEPRECATED: ABSL_ARRAYSIZE() is prefered over ARRAYSIZE().
//
// ARRAYSIZE performs essentially the same calculation as arraysize,
// but can be used on anonymous types or types defined inside
// functions.  It's less safe than arraysize as it accepts some
// (although not all) pointers.  Therefore, you should use arraysize
// whenever possible.
//
// The expression ARRAYSIZE(a) is a compile-time constant of type
// size_t.
//
// ARRAYSIZE catches a few type errors.  If you see a compiler error
//
//   "warning: division by zero in ..."
//
// when using ARRAYSIZE, you are (wrongfully) giving it a pointer.
// You should only use ARRAYSIZE on statically allocated arrays.
//
// The following comments are on the implementation details, and can
// be ignored by the users.
//
// ARRAYSIZE(arr) works by inspecting sizeof(arr) (the # of bytes in
// the array) and sizeof(*(arr)) (the # of bytes in one array
// element).  If the former is divisible by the latter, perhaps arr is
// indeed an array, in which case the division result is the # of
// elements in the array.  Otherwise, arr cannot possibly be an array,
// and we generate a compiler error to prevent the code from
// compiling.
//
// Since the size of bool is implementation-defined, we need to cast
// !(sizeof(a) & sizeof(*(a))) to size_t in order to ensure the final
// result has type size_t.
//
// This macro is not perfect as it wrongfully accepts certain
// pointers, namely where the pointer size is divisible by the pointee
// size.  For code that goes through a 32-bit compiler, where a pointer
// is 4 bytes, this means all pointers to a type whose size is 3 or
// greater than 4 will be (righteously) rejected.
//
// Kudos to Jorg Brown for this simple and elegant implementation.
//
// - wan 2005-11-16
//
// Starting with Visual C++ 2005, WinNT.h includes ARRAYSIZE.
#if !defined(COMPILER_MSVC)
#define ARRAYSIZE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#endif

// A macro to turn a symbol into a string
// These should be used judiciously in modern code. Prefer raw-string literal
// should you simply need to specify a large string.
#define AS_STRING(x)   AS_STRING_INTERNAL(x)
#define AS_STRING_INTERNAL(x)   #x

// The CLANG_WARN_UNUSED_RESULT macro can be used on a class or struct to mark
// the class or struct as one that should never be ignored when it is a return
// value. Which is to say, any function or method that returns an instance of
// the marked class/struct and is not a member function of the class/struct
// will be treated as if it has the "warn_unused_result" attribute. The macro
// is only expanded into an attribute annotation when compiling with clang, as
// the use of the "warn_unused_result" attribute on a class or struct is a
// clang-specific extension of the eponymous function attribute.
//
// For example, in:
//   class CLANG_WARN_UNUSED_RESULT Status {
//     ...
//     void CheckSuccess();
//     Status StripMessage() const;
//   };
//
//   Status CreateResource();
//
//   void DoIt() {
//     Status s = CreateResource();
//     s.StripMessage();
//     CreateResource();
//     CreateResource().CheckSuccess();
//   }
//
// The first call to CreateResource in DoIt will not trigger a warning because
// the returned Status object was assigned to a variable. The call to
// Status::StripMessage also won't raise a warning despite the returned Status
// object being unused because the method is a member of the Status class.
// The second call to CreateResource will raise a warning because CreateResource
// returns a Status object and that object is unused (even though CreateResource
// was not explicitly declared with the "warn_unused_result" attribute). The
// final call to CreateResource is fine since the CheckSuccess method is called
// for the returned Status object.
#if defined(__clang__)
# if defined(LANG_CXX11) && __has_feature(cxx_attributes)
#  define CLANG_WARN_UNUSED_RESULT [[clang::warn_unused_result]]  // NOLINT
# else
#  define CLANG_WARN_UNUSED_RESULT __attribute__((warn_unused_result))  // NOLINT
# endif
#else
# define CLANG_WARN_UNUSED_RESULT
#endif

#endif  // BASE_MACROS_H_
