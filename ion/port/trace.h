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

#ifndef ION_PORT_TRACE_H_
#define ION_PORT_TRACE_H_

// If the target platform is android, use the ION_ATRACE_* macros to assist in
// performing a systrace, otherwise define them as no-ops.

// The ION_ATRACE_TAG macro can be defined before including this header to trace
// using one of the tags defined below.  It must be defined to one of the
// following ION_ATRACE_TAG_* macros.  The trace tag is used to filter tracing
// in userland to avoid some of the runtime cost of tracing when it is not
// desired.
//
// Defining ION_ATRACE_TAG to be ION_ATRACE_TAG_ALWAYS will result in the
// tracing always being enabled - this should ONLY be done for debug code, as
// userland tracing has a performance cost even when the trace is not being
// recorded.  Defining ION_ATRACE_TAG to be ION_ATRACE_TAG_NEVER or leaving
// ION_ATRACE_TAG undefined will result in the tracing always being disabled.
//
// These tags must be kept in sync with
//   frameworks/base/core/java/android/os/Trace.java.
#define ION_ATRACE_TAG_NEVER 0          // This tag is never enabled.
#define ION_ATRACE_TAG_ALWAYS (1 << 0)  // This tag is always enabled.
#define ION_ATRACE_TAG_GRAPHICS (1 << 1)
#define ION_ATRACE_TAG_INPUT (1 << 2)
#define ION_ATRACE_TAG_VIEW (1 << 3)
#define ION_ATRACE_TAG_WEBVIEW (1 << 4)
#define ION_ATRACE_TAG_WINDOW_MANAGER (1 << 5)
#define ION_ATRACE_TAG_ACTIVITY_MANAGER (1 << 6)
#define ION_ATRACE_TAG_SYNC_MANAGER (1 << 7)
#define ION_ATRACE_TAG_AUDIO (1 << 8)
#define ION_ATRACE_TAG_VIDEO (1 << 9)
#define ION_ATRACE_TAG_CAMERA (1 << 10)
#define ION_ATRACE_TAG_HAL (1 << 11)
#define ION_ATRACE_TAG_APP (1 << 12)
#define ION_ATRACE_TAG_RESOURCES (1 << 13)
#define ION_ATRACE_TAG_DALVIK (1 << 14)
#define ION_ATRACE_TAG_RS (1 << 15)
#define ION_ATRACE_TAG_BIONIC (1 << 16)
#define ION_ATRACE_TAG_POWER (1 << 17)
#define ION_ATRACE_TAG_PACKAGE_MANAGER (1 << 18)
#define ION_ATRACE_TAG_SYSTEM_SERVER (1 << 19)
#define ION_ATRACE_TAG_DATABASE (1 << 20)
#define ION_ATRACE_TAG_NETWORK (1 << 21)
#define ION_ATRACE_TAG_ADB (1 << 22)
#define ION_ATRACE_TAG_LAST ION_ATRACE_TAG_ADB

#define ION_ATRACE_TAG_NOT_READY (1LL << 63)  // Reserved for use during init.

#define ION_ATRACE_TAG_VALID_MASK \
  ((ION_ATRACE_TAG_LAST - 1) | ION_ATRACE_TAG_LAST)

#if !defined(ION_ATRACE_TAG) || ION_ATRACE_TAG > ION_ATRACE_TAG_LAST
#error ION_ATRACE_TAG must be defined to one of the tags defined in port/trace.h
#endif

// ION_ATRACE_PROD_* macros are always traced. The non-PROD versions are only
// traced in non-production builds (!ION_PRODUCTION).
#if ION_PRODUCTION
  #define ION_ATRACE_INIT()
  #define ION_ATRACE_CALL()
  #define ION_ATRACE_NAME(name)
  #define ION_ATRACE_INT(name, value)
  #define ION_ATRACE_INT64(name, value)
  #define ION_ATRACE_GET_ENABLED_TAGS() 0
  #define ION_ATRACE_ENABLED() false
  #define ION_ATRACE_BEGIN(name)
  #define ION_ATRACE_END()
  #define ION_ATRACE_ASYNC_BEGIN(name, cookie)
  #define ION_ATRACE_ASYNC_END(name, cookie)
#else
  #define ION_ATRACE_INIT() ION_ATRACE_PROD_INIT()
  #define ION_ATRACE_CALL() ION_ATRACE_PROD_CALL()
  #define ION_ATRACE_NAME(name) ION_ATRACE_PROD_NAME(name)
  #define ION_ATRACE_INT(name, value) ION_ATRACE_PROD_INT(name, value)
  #define ION_ATRACE_INT64(name, value) ION_ATRACE_PROD_INT64(name, value)
  #define ION_ATRACE_GET_ENABLED_TAGS() ION_ATRACE_PROD_GET_ENABLED_TAGS()
  #define ION_ATRACE_ENABLED() ION_ATRACE_PROD_ENABLED()
  #define ION_ATRACE_BEGIN(name) ION_ATRACE_PROD_BEGIN(name)
  #define ION_ATRACE_END() ION_ATRACE_PROD_END()
  #define ION_ATRACE_ASYNC_BEGIN(name, cookie) \
    ION_ATRACE_PROD_ASYNC_BEGIN(name, cookie)
  #define ION_ATRACE_ASYNC_END(name, cookie)     \
    ION_ATRACE_PROD_ASYNC_END(name, cookie)
#endif  // ION_PRODUCTION

#if defined(ION_PLATFORM_ANDROID)
#include "ion/port/android/trace.h"
#else
  #define ION_ATRACE_PROD_INIT()
  #define ION_ATRACE_PROD_CALL()
  #define ION_ATRACE_PROD_NAME(name)
  #define ION_ATRACE_PROD_INT(name, value)
  #define ION_ATRACE_PROD_INT64(name, value)
  #define ION_ATRACE_PROD_GET_ENABLED_TAGS() 0
  #define ION_ATRACE_PROD_ENABLED() false
  #define ION_ATRACE_PROD_BEGIN(name)
  #define ION_ATRACE_PROD_END()
  #define ION_ATRACE_PROD_ASYNC_BEGIN(name, cookie)
  #define ION_ATRACE_PROD_ASYNC_END(name, cookie)
#endif  // defined(ION_PLATFORM_ANDROID)

#endif  // ION_PORT_TRACE_H_
