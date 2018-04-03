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

#ifndef ION_PORT_ANDROID_TRACE_H_
#define ION_PORT_ANDROID_TRACE_H_

#include <atomic>
#include <thread>  // NOLINT(build/c++11)

// This file originates from:
// /platform/system/core/libcutils/include/cutils/trace.h

// ION_ATRACE_PROD_INIT readies the process for tracing by opening the
// trace_marker file. Calling any trace function causes this to be run, so
// calling it is optional. This can be explicitly run to avoid setup delay on
// first trace function.
#define ION_ATRACE_PROD_INIT() ion::port::android::Tracer::Init()

// ION_ATRACE_PROD_CALL traces the beginning and end of the current function. To
// trace the correct start and end times this macro should be the first line of
// the function body.
#define ION_ATRACE_PROD_CALL() \
  ion::port::android::ScopedTrace ___tracer(ION_ATRACE_TAG, __FUNCTION__)

// ION_ATRACE_PROD_NAME traces the beginning and end of the current function. To
// trace the correct start and end times this macro should be the first line of
// the function body.
#define ION_ATRACE_PROD_NAME(name) \
  ion::port::android::ScopedTrace ___tracer(ION_ATRACE_TAG, name)

// ION_ATRACE_PROD_INT traces a named integer value.  This can be used to track
// how the value changes over time in a trace.
#define ION_ATRACE_PROD_INT(name, value) \
  ion::port::android::Tracer::Counter(ION_ATRACE_TAG, name, value)

// ION_ATRACE_PROD_INT64 traces a named 64-bit integer counter value. This can
// be used to track how a 64-bit value changes over time in a trace.
#define ION_ATRACE_PROD_INT64(name, value) \
  ion::port::android::Tracer::Counter64(ION_ATRACE_TAG, name, value)

// Get the mask of all tags currently enabled.
// It can be used as a guard condition around more expensive trace calculations.
// Every trace function calls this, which ensures init is run.
#define ION_ATRACE_PROD_GET_ENABLED_TAGS() \
  ion::port::android::Tracer::GetEnabledTags()

// ION_ATRACE_PROD_ENABLED returns true if the trace tag is enabled.  It can be
// used as a guard condition around more expensive trace calculations.
#define ION_ATRACE_PROD_ENABLED() \
  ion::port::android::Tracer::IsTagEnabled(ION_ATRACE_TAG)

// ION_ATRACE_PROD_BEGIN traces the beginning of a context.  The name is used to
// identify the context. This is often used to time function execution.
#define ION_ATRACE_PROD_BEGIN(name) \
  ion::port::android::Tracer::Begin(ION_ATRACE_TAG, name)

// ION_ATRACE_PROD_END traces the end of a context.  This should match up (and
// occur after) a corresponding ION_ATRACE_PROD_BEGIN.
#define ION_ATRACE_PROD_END() ion::port::android::Tracer::End(ION_ATRACE_TAG)

// ION_ATRACE_PROD_ASYNC_BEGIN traces the beginning of an asynchronous event.
// Asynchronous events do not need to be nested. The name describes the event,
// and the cookie provides a unique identifier for distinguishing simultaneous
// events. The name and cookie used to begin an event must be used to end it.
#define ION_ATRACE_PROD_ASYNC_BEGIN(name, cookie) \
  ion::port::android::Tracer::AsyncBegin(ION_ATRACE_TAG, name, cookie)

// ION_ATRACE_PROD_ASYNC_END traces the end of an asynchronous event. This
// should have a corresponding ION_ATRACE_PROD_ASYNC_BEGIN.
#define ION_ATRACE_PROD_ASYNC_END(name, cookie) \
  ion::port::android::Tracer::AsyncEnd(ION_ATRACE_TAG, name, cookie)

struct prop_info;

namespace ion {
namespace port {
namespace android {

class Tracer {
 public:
  // The below are the functions used in the above macros.
  static inline void Init() {
    // It is OK if multiple threads enter TraceSetup. It is also OK if it takes
    // a while for all threads to see needs_setup_ change from true to false.
    if (needs_setup_) TraceSetup();
  }
  static inline uint64_t GetEnabledTags() {
    Init();
    return enabled_tags_;
  }
  static inline bool IsTagEnabled(uint64_t tag) {
    return static_cast<bool>(GetEnabledTags() & tag);
  }
  static inline void Begin(uint64_t tag, const char* name) {
    if (IsTagEnabled(tag)) BeginImpl(name);
  }
  static inline void End(uint64_t tag) {
    if (IsTagEnabled(tag)) EndImpl();
  }
  static inline void AsyncBegin(uint64_t tag, const char* name,
                                int32_t cookie) {
    if (IsTagEnabled(tag)) AsyncBeginImpl(name, cookie);
  }
  static inline void AsyncEnd(uint64_t tag, const char* name, int32_t cookie) {
    if (IsTagEnabled(tag)) AsyncEndImpl(name, cookie);
  }
  static inline void Counter(uint64_t tag, const char* name, int32_t value) {
    if (IsTagEnabled(tag)) CounterImpl(name, value);
  }
  static inline void Counter64(uint64_t tag, const char* name,
                                  int64_t value) {
    if (IsTagEnabled(tag)) Counter64Impl(name, value);
  }

 private:
  // Opens the trace file for writing and reads the property for initial tags.
  // The atrace.tags.enableflags property sets the tags to trace.
  static void TraceSetup();

  // Reads the sysprop and return the value tags should be set to.
  static uint64_t GetProperty();

  // Updates the tags used for tracing.
  static void UpdateTags();

  // Initializes tracing.
  static void InitOnce();

  // Implementations of the above functions.
  static void BeginImpl(const char* name);
  static void EndImpl();
  static void AsyncBeginImpl(const char* name, int32_t cookie);
  static void AsyncEndImpl(const char* name, int32_t cookie);
  static void CounterImpl(const char* name, int32_t value);
  static void Counter64Impl(const char* name, int64_t value);

  // Flag indicating whether setup is needed, initialized to true. False
  // indicates setup has completed. Note: This does NOT indicate whether or not
  // setup was successful.
  static bool needs_setup_;

  // Set of ION_ATRACE_TAG flags to trace for, initialized to
  // ION_ATRACE_TAG_NOT_READY. A value of zero indicates setup has failed.
  // Any other nonzero value indicates setup has succeeded, and tracing is on.
  static uint64_t enabled_tags_;

  // Handle to the kernel's trace buffer, initialized to -1. Any other value
  // indicates setup has succeeded, and is a valid fd for tracing.
  static int marker_fd_;

  // System property holding enabled trace flags.
  static const prop_info* enable_flags_property_;
};

class ScopedTrace {
 public:
  inline ScopedTrace(uint64_t tag, const char* name) : mTag(tag) {
    Tracer::Begin(mTag, name);
  }

  inline virtual ~ScopedTrace() { Tracer::End(mTag); }

 private:
  uint64_t mTag;
};

};  // namespace android
};  // namespace port
};  // namespace ion

#endif  // ION_PORT_ANDROID_TRACE_H_
