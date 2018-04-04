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

#define ION_ATRACE_TAG ION_ATRACE_TAG_NEVER
#include "ion/port/trace.h"  // NOLINT

#define LOG_TAG "ATRACE"
#include <android/log.h>
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <unistd.h>

#include <climits>
#include <mutex>  // NOLINT(build/c++11)

#include "base/strtoint.h"

// This file originates in part from /platform/system/core/libcutils/trace-dev.c

using PFsystem_property_wait =
    bool (*)(const prop_info* pi, uint32_t old_serial, uint32_t* new_serial_ptr,
             const struct timespec* relative_timeout);

PFsystem_property_wait system_property_wait;

namespace ion {
namespace port {
namespace android {

// Maximum size of a message that can be logged to the trace buffer.
// Note this message includes a tag, the pid, and the string given as the name.
// Names should be kept short to get the most use of the trace buffer.
#define ION_ATRACE_MESSAGE_LENGTH 1024
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

// Reads an Android system property named |key| and returns its string value in
// |value|. If the key does not exist then |default_value| is used instead.
static int GetSystemProperty(const prop_info* info, char* key, char* value,
                             const char* default_value) {
  int len = __system_property_read(info, key, value);
  if (len > 0) {
    return len;
  }
  // Use the default value.
  if (default_value) {
    len = static_cast<int>(strnlen(default_value, PROP_VALUE_MAX - 1));
    memcpy(value, default_value, len);
    value[len] = '\0';
  }
  return len;
}

// Reads an Android system property named |key| and returns its value as a
// uint64_t. If the key does not exist then |default_value| is used instead.
static uint64_t ReadInt64SystemProperty(const prop_info* info,
                                        const char* default_value) {
  char key[PROP_NAME_MAX];
  char value[PROP_VALUE_MAX];
  char* endptr;

  GetSystemProperty(info, key, value, default_value);
  errno = 0;
  uint64_t number = strtou64(value, &endptr, 0);
  if (value[0] == '\0' || *endptr != '\0') {
    ALOGE("Error parsing trace property %s: Not a number: %s", key, value);
    return 0;
  } else if (errno == ERANGE || number == ULLONG_MAX) {
    ALOGE("Error parsing trace property %s: Number too large: %s", key, value);
    return 0;
  }
  return number;
}

}  // anonymous namespace

// Statics from Tracer.
bool Tracer::needs_setup_ = true;
int Tracer::marker_fd_ = -1;
uint64_t Tracer::enabled_tags_ = ION_ATRACE_TAG_NOT_READY;
const prop_info* Tracer::enable_flags_property_ = nullptr;

// Read the sysprop and return the value tags should be set to
uint64_t Tracer::GetProperty() {
  uint64_t tags = 0;
  if (enable_flags_property_) {
    tags = ReadInt64SystemProperty(enable_flags_property_, "0");
  }
  return (tags | ION_ATRACE_TAG_ALWAYS | ION_ATRACE_TAG_APP) &
         ION_ATRACE_TAG_VALID_MASK;
}

void Tracer::UpdateTags() {
  uint32_t serial = 0;
  while (true) {
    uint32_t old_serial = serial;
    bool ret = system_property_wait(enable_flags_property_, old_serial, &serial,
                                    nullptr);
    assert(ret && "Unable to read system trace property");
    enabled_tags_ = GetProperty();
    ALOGI("Updated trace tags to %" PRId64, enabled_tags_);
  }
}

void Tracer::InitOnce() {
  marker_fd_ =
      open("/sys/kernel/debug/tracing/trace_marker", O_WRONLY | O_CLOEXEC);
  if (marker_fd_ == -1) {
    ALOGE("Error opening trace file: %s (%d)", strerror(errno), errno);
    enabled_tags_ = 0;
    needs_setup_ = false;
    return;
  }

  enable_flags_property_ =
      __system_property_find("debug.atrace.tags.enableflags");
  // Emulators to not have the system property, but that's ok since we don't
  // need to profile on them.
  if (!enable_flags_property_) {
    ALOGW("Unable to find system trace flags property, tracing may not work");
  }

  enabled_tags_ = GetProperty();

  needs_setup_ = false;

  if (void* libc = dlopen("libc.so", RTLD_NOW)) {
    *reinterpret_cast<void**>(&system_property_wait) =
        dlsym(libc, "__system_property_wait");
  }

  if (enable_flags_property_) {
    if (system_property_wait) {
      // Start monitor thread.
      std::thread update_thread(UpdateTags);
      update_thread.detach();
      ALOGI("Using __system_property_wait to update tags");
    } else {
      ALOGW(
          "Unable to find __system_property_wait, trace tags will not update "
          "unless the app is restarted");
    }
  }
}

void Tracer::TraceSetup() {
  static std::once_flag once_control;
  std::call_once(once_control, InitOnce);
}

void Tracer::BeginImpl(const char* name) {
  char buf[ION_ATRACE_MESSAGE_LENGTH];

  size_t len = snprintf(buf, sizeof(buf), "B|%d|%s", getpid(), name);
  if (len >= sizeof(buf)) {
    ALOGW("Truncated name in %s: %s\n", __FUNCTION__, name);
    len = sizeof(buf) - 1;
  }
  write(marker_fd_, buf, len);
}

void Tracer::EndImpl() {
  char c = 'E';
  write(marker_fd_, &c, 1);
}

#define ION_WRITE_MSG(format_begin, format_end, pid, name, value)              \
  {                                                                            \
    char buf[ION_ATRACE_MESSAGE_LENGTH];                                       \
    size_t len = snprintf(buf, sizeof(buf), format_begin "%s" format_end, pid, \
                          name, value);                                        \
    if (len >= sizeof(buf)) {                                                  \
      /* Given the sizeof(buf), and all of the current format buffers,  */     \
      /* it is impossible for name_len to be < 0 if len >= sizeof(buf). */     \
      size_t name_len = strlen(name) - (len - sizeof(buf)) - 1;                \
      /* Truncate the name to make the message fit. */                         \
      ALOGW("Truncated name in %s: %s\n", __FUNCTION__, name);                 \
      len = snprintf(buf, sizeof(buf), format_begin "%.*s" format_end, pid,    \
                     name_len, name, value);                                   \
    }                                                                          \
    write(marker_fd_, buf, len);                                               \
  }

void Tracer::AsyncBeginImpl(const char* name, int32_t cookie) {
  ION_WRITE_MSG("S|%d|", "|%" PRId32, getpid(), name, cookie);
}

void Tracer::AsyncEndImpl(const char* name, int32_t cookie) {
  ION_WRITE_MSG("F|%d|", "|%" PRId32, getpid(), name, cookie);
}

void Tracer::CounterImpl(const char* name, int32_t value) {
  ION_WRITE_MSG("C|%d|", "|%" PRId32, getpid(), name, value);
}

void Tracer::Counter64Impl(const char* name, int64_t value) {
  ION_WRITE_MSG("C|%d|", "|%" PRId64, getpid(), name, value);
}

#undef ION_ATRACE_MESSAGE_LENGTH
#undef ION_WRITE_MSG

}  // namespace android
}  // namespace port
}  // namespace ion
