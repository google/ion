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

/* jconfigint.h.  Generated from jconfigint.h.in by configure.  */
/* libjpeg-turbo build number */
#define BUILD "20171114"

/* Compiler's inline keyword */
/* #undef inline */

/* How to obtain function inlining. */
/* BEGIN GOOGLE MODIFICATIONS */
#ifdef _MSC_VER  /* Windows */
#define INLINE __inline
#else
#define INLINE inline __attribute__((always_inline))
#endif
/* END GOOGLE MODIFICATIONS */

/* Define to the full name of this package. */
#define PACKAGE_NAME "libjpeg-turbo"

/* Version number of package */
#define VERSION "1.5.2"

/* The size of `size_t', as computed by sizeof. */
/* BEGIN GOOGLE MODIFICATIONS */
#if (__WORDSIZE==64 && !defined(__native_client__)) || defined(_WIN64)
/* END GOOGLE MODIFICATIONS */
#define SIZEOF_SIZE_T 8
#else
#define SIZEOF_SIZE_T 4
#endif
