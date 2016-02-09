/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#ifndef ION_PORT_TRACE_H_
#define ION_PORT_TRACE_H_

// If the target platform is android, use the ION_ATRACE_* macros to assist in
// performing a systrace, otherwise define them as no-ops.

#if defined(ION_PLATFORM_ANDROID)
  #include "ion/port/android/trace.h"
#else
  #define ION_ATRACE_CALL()
  #define ION_ATRACE_NAME(name)
  #define ION_ATRACE_INT(name, value)
  #define ION_ATRACE_ENABLED() false
#endif

#endif  // ION_PORT_TRACE_H_
