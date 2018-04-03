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

#ifndef ION_BASE_STATIC_ASSERT_H_
#define ION_BASE_STATIC_ASSERT_H_

// Defines ION_STATIC_ASSERT(expr, message) to produce a compile-time error if
// expr evaluates to false. This uses native support for static assertions if
// it is available (as is true in gcc with version >= 4.6 or the Microsoft
// compiler with version > 1600).  In this case, the resulting error message
// should include the message passed to the constructor. If there is no native
// support, we fall back to a macro defined here that causes a compilation
// error something like this:
//
//   error: size of array 'static_assert_failed_at_lineXXX' is negative
//
// Note that if the macro is used, the expr argument may have to be
// parenthesized if it contains commas.

#include "ion/port/static_assert.h"  // For ION_PORT_STATIC_ASSERT().
#define ION_STATIC_ASSERT(expr, message) ION_PORT_STATIC_ASSERT(expr, message)

#endif  // ION_BASE_STATIC_ASSERT_H_
