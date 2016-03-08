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

#define LOG_TAG "Trace"

#include "ion/port/android/trace.h"

#include <android/log.h>

// This file originates from /platform/frameworks/native/libs/utils/Trace.cpp

namespace ion {
namespace port {
namespace android {

volatile int32_t Tracer::sIsReady = 0;
int Tracer::sTraceFD = -1;
uint64_t Tracer::sEnabledTags = ION_ATRACE_TAG_NOT_READY;

void Tracer::changeCallback() {
  if (sIsReady && sTraceFD >= 0) {
    loadSystemProperty();
  }
}

void Tracer::init() {
  if (sIsReady) return;

  const char* const traceFileName =
      "/sys/kernel/debug/tracing/trace_marker";
  sTraceFD = open(traceFileName, O_WRONLY);
  if (sTraceFD == -1) {
    __android_log_print(ANDROID_LOG_INFO,
                        "TRACE", "error opening trace file: %s (%d)",
                        strerror(errno), errno);

    sEnabledTags = 0;   // no tracing can occur
  } else {
    loadSystemProperty();
  }
  sIsReady = 1;
}

void Tracer::loadSystemProperty() {
  sEnabledTags = (0x400a & ION_ATRACE_TAG_VALID_MASK) | ION_ATRACE_TAG_ALWAYS;
}

}  // namespace android
}  // namespace port
}  // namespace ion
