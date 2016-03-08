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

#ifndef ION_PORT_ANDROID_TRACE_H_
#define ION_PORT_ANDROID_TRACE_H_

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android/log.h>

// This file originates from /platform/frameworks/native/include/utils/Trace.h
//
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
#define ION_ATRACE_TAG_NEVER            0       // The "never" tag is never
                                                // enabled.
#define ION_ATRACE_TAG_ALWAYS           (1<<0)  // The "always" tag is always
                                                // enabled.
#define ION_ATRACE_TAG_GRAPHICS         (1<<1)
#define ION_ATRACE_TAG_INPUT            (1<<2)
#define ION_ATRACE_TAG_VIEW             (1<<3)
#define ION_ATRACE_TAG_WEBVIEW          (1<<4)
#define ION_ATRACE_TAG_WINDOW_MANAGER   (1<<5)
#define ION_ATRACE_TAG_ACTIVITY_MANAGER (1<<6)
#define ION_ATRACE_TAG_SYNC_MANAGER     (1<<7)
#define ION_ATRACE_TAG_AUDIO            (1<<8)
#define ION_ATRACE_TAG_VIDEO            (1<<9)
#define ION_ATRACE_TAG_CAMERA           (1<<10)
#define ION_ATRACE_TAG_HAL              (1<<11)
#define ION_ATRACE_TAG_APP              (1<<12)
#define ION_ATRACE_TAG_RESOURCES        (1<<13)
#define ION_ATRACE_TAG_DALVIK           (1<<14)
#define ION_ATRACE_TAG_RS               (1<<15)
#define ION_ATRACE_TAG_BIONIC           (1<<16)
#define ION_ATRACE_TAG_POWER            (1<<17)
#define ION_ATRACE_TAG_PACKAGE_MANAGER  (1<<18)
#define ION_ATRACE_TAG_SYSTEM_SERVER    (1<<19)
#define ION_ATRACE_TAG_LAST             ION_ATRACE_TAG_SYSTEM_SERVER

#define ION_ATRACE_TAG_NOT_READY        (1LL << 63)  // Reserved for use during
                                                     // init

#define ION_ATRACE_TAG_VALID_MASK \
  ((ION_ATRACE_TAG_LAST - 1) | ION_ATRACE_TAG_LAST)

#ifndef ION_ATRACE_TAG
#define ION_ATRACE_TAG ION_ATRACE_TAG_NEVER
#elif ION_ATRACE_TAG > ION_ATRACE_TAG_LAST
#error ION_ATRACE_TAG must be defined to be one of the tags defined in \
  utils/Trace.h
#endif

// ION_ATRACE_CALL traces the beginning and end of the current function.  To
// trace the correct start and end times this macro should be the first line of
// the function body.
#define ION_ATRACE_CALL() \
  ion::port::android::ScopedTrace ___tracer(ION_ATRACE_TAG, __FUNCTION__)

// ION_ATRACE_NAME traces the beginning and end of the current function.  To
// trace the correct start and end times this macro should be the first line of
// the function body.
#define ION_ATRACE_NAME(name) \
  ion::port::android::ScopedTrace ___tracer(ION_ATRACE_TAG, name)

// ION_ATRACE_INT traces a named integer value.  This can be used to track how
// the value changes over time in a trace.
#define ION_ATRACE_INT(name, value) \
  ion::port::android::Tracer::traceCounter(ION_ATRACE_TAG, name, value)

// ION_ATRACE_ENABLED returns true if the trace tag is enabled.  It can be used
// as a guard condition around more expensive trace calculations.
#define ION_ATRACE_ENABLED() \
  ion::port::android::Tracer::isTagEnabled(ION_ATRACE_TAG)

namespace ion {
namespace port {
namespace android {

class Tracer {
 public:
  static uint64_t getEnabledTags() {
    initIfNeeded();
    return sEnabledTags;
  }

  static inline bool isTagEnabled(uint64_t tag) {
    initIfNeeded();
    return true;  // sEnabledTags & tag;
  }

  static inline void traceCounter(uint64_t tag, const char* name,
                                  int32_t value) {
    if (isTagEnabled(tag)) {
      char buf[1024];
      snprintf(buf, 1024, "C|%d|%s|%d", getpid(), name, value);  // NOLINT
      write(sTraceFD, buf, strlen(buf));
    }
  }

  static inline void traceBegin(uint64_t tag, const char* name) {
    if (isTagEnabled(tag)) {
      char buf[1024];
      size_t len = snprintf(buf, 1024, "B|%d|%s", getpid(), name);  // NOLINT
      write(sTraceFD, buf, len);
    }
  }

  static inline void traceEnd(uint64_t tag) {
    if (isTagEnabled(tag)) {
      char buf = 'E';
      write(sTraceFD, &buf, 1);
    }
  }

 private:
  static inline void initIfNeeded() {
    init();
  }

  static void changeCallback();

  // init opens the trace marker file for writing and reads these
  // atrace.tags.enableflags system property.  It does this only the first
  // time it is run, using sMutex for synchronization.
  static void init();

  // retrieve the current value of the system property.
  static void loadSystemProperty();

  // sIsReady is a boolean value indicating whether a call to init() has
  // completed in this process.  It is initialized to 0 and set to 1 when the
  // first init() call completes.  It is set to 1 even if a failure occurred
  // in init (e.g. the trace marker file couldn't be opened).
  //
  // This should be checked by all tracing functions using an atomic acquire
  // load operation before calling init().  This check avoids the need to lock
  // a mutex each time a trace function gets called.
  static volatile int32_t sIsReady;

  // sTraceFD is the file descriptor used to write to the kernel's trace
  // buffer.  It is initialized to -1 and set to an open file descriptor in
  // init() while a lock on sMutex is held.
  //
  // This should only be used by a trace function after init() has
  // successfully completed.
  static int sTraceFD;

  // sEnabledTags is the set of tag bits for which tracing is currently
  // enabled.  It is initialized to 0 and set based on the
  // atrace.tags.enableflags system property in init() while a lock on sMutex
  // is held.
  //
  // This should only be used by a trace function after init() has
  // successfully completed.
  //
  // This value is only ever non-zero when tracing is initialized and sTraceFD
  // is not -1.
  static uint64_t sEnabledTags;
};

class ScopedTrace {
 public:
  inline ScopedTrace(uint64_t tag, const char* name)
      : mTag(tag) {
    Tracer::traceBegin(mTag, name);
  }

  inline virtual ~ScopedTrace() {
    Tracer::traceEnd(mTag);
  }

 private:
  uint64_t mTag;
};

};  // namespace android
};  // namespace port
};  // namespace ion

#endif  // ION_PORT_ANDROID_TRACE_H_
