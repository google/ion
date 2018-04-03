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

/*
 * scalblnf() is only available on Android-x86 starting with API 18 but HarfBuzz
 * wants it and we build against API 16, so provide it.
 */

#if !defined(__i386__)
#error This file should only be included for android-x86 builds!
#endif

#include <limits.h>
#include <math.h>

float scalblnf(float x, long n) {
  if (n > INT_MAX)
    return x > 0 ? HUGE_VALF : -HUGE_VALF;
  if (n < INT_MIN)
    return x > 0 ? 0.f : -0.f;
  return scalbnf(x, (int)n);
}
