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

#ifndef ION_PORT_USERESULT_H_
#define ION_PORT_USERESULT_H_

// Helper macro to make compiler emit a warning when the result of a function
// goes unused. To use, declare start your function declaration with the macro.
// Example:
//   ION_USE_RESULT int FunctionReturningImportantResult();
// This should work with GCC, Clang and MSVC (note:clang defines __GNUC__).
#if defined(__GNUC__)
#define ION_USE_RESULT __attribute__ ((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#define ION_USE_RESULT _Check_return_
#else
#define ION_USE_RESULT
#endif

#endif  // ION_PORT_USERESULT_H_
