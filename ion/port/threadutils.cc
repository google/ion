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

#include "ion/port/threadutils.h"

#include <cstring>  // NOLINT
#include <iostream>  // NOLINT

#if defined(ION_PLATFORM_WINDOWS)
#  define API_DECL WINAPI
#else
#  include <sched.h>
#  include <stdio.h>
#  define API_DECL
#endif

#if defined(ION_PLATFORM_ANDROID)
#  include <android/log.h>
#endif

#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL) || \
    defined(__myriad2__)
# define THREAD_NAMING_SUPPORTED 0
#else
# define THREAD_NAMING_SUPPORTED 1
#endif

namespace ion {
namespace port {

namespace {

//-----------------------------------------------------------------------------
//
// Windows-specific types and helper functions.
//
//-----------------------------------------------------------------------------
#if defined(ION_PLATFORM_WINDOWS)

// This struct is used for naming threads.
struct ThreadNameInfo {
  ThreadNameInfo() : type(0x1000), flags(0) {}
  DWORD type;        // Must be 0x1000.
  LPCSTR name;       // Pointer to name.
  DWORD  thread_id;  // Thread ID, or -1 to indicate the calling thread.
  DWORD flags;       // Reserved for future use; must be zero.
};

#endif

//-----------------------------------------------------------------------------
//
// pthreads-specific types and helper functions. (All non-Windows platforms use
// pthreads.)
//
//-----------------------------------------------------------------------------
#if !defined(ION_PLATFORM_WINDOWS)

// Checks the return value of a pthread function call for success and prints an
// error if it failed. Returns true on success.
static bool CheckPthreadSuccess(const char* what, int result) {
  if (result) {
#if !defined(ION_COVERAGE)  // COV_NF_START
    // All pthread functions return 0 when successful, so non-zero means error.
    // Note that because this code is in port, we don't have access to
    // base::LogChecker, which means that there is no good way to trap error
    // messages in tests.

#if defined(ION_PLATFORM_ANDROID)
    // If 'result' is ENOMEM, writing to std::cerr would cause an exception,
    // so this function calls __android_log_print instead to prevent a crash.
    __android_log_print(ANDROID_LOG_ERROR, "Ion",
                        "Pthread error %s returned %d: %s\n", what, result,
                        strerror(result));
#else
    std::cerr << "Pthread error: " << what << " returned "
              << result << ": " << strerror(result) << "\n";
#endif
    return false;
#endif  // COV_NF_END
  }
  return true;
}

#endif

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Platform-independent public functions.
//
//-----------------------------------------------------------------------------

bool IsThreadNamingSupported() {
  return THREAD_NAMING_SUPPORTED;
}

//-----------------------------------------------------------------------------
//
// Windows-specific public functions.
//
//-----------------------------------------------------------------------------
#if defined(ION_PLATFORM_WINDOWS)

size_t GetMaxThreadNameLength() {
  return 0;
}

bool SetThreadName(const std::string& name) {
  bool success = false;
  // Search MSDN for "How to Set a Thread Name in Native Code" for the source
  // of these incantations. Basically, the officially blessed way to set the
  // thread name is to raise a special SEH exception.
  ThreadNameInfo info;
  info.name = name.c_str();
  info.thread_id = GetCurrentThreadId();
  __try {
    static const DWORD kMsVcException = 0x406d1388;
    ::RaiseException(kMsVcException, 0, sizeof(info) / sizeof(DWORD),
                     reinterpret_cast<ULONG_PTR*>(&info));
    success = true;
  } __except(EXCEPTION_CONTINUE_EXECUTION) {
    std::cerr << "***ION error: Unable to name thread with ID "
              << info.thread_id << "\n";
  }
  return success;
}

ThreadLocalStorageKey CreateThreadLocalStorageKey() {
  ThreadLocalStorageKey key = ::TlsAlloc();
  if (key == TLS_OUT_OF_INDEXES) {
    std::cerr << "***ION error: Unable to create thread-local storage key: "
              << GetLastError() << "\n";
    key = kInvalidThreadLocalStorageKey;
  }
  return key;
}

bool SetThreadLocalStorage(ThreadLocalStorageKey key, void* ptr) {
  if (key != kInvalidThreadLocalStorageKey) {
    if (::TlsSetValue(key, ptr))
      return true;
    std::cerr << "***ION error: Unable to set thread-local storage pointer: "
              << GetLastError() << "\n";
  }
  return false;
}

void* GetThreadLocalStorage(ThreadLocalStorageKey key) {
  return key == kInvalidThreadLocalStorageKey ? nullptr : ::TlsGetValue(key);
}

bool DeleteThreadLocalStorageKey(ThreadLocalStorageKey key) {
  if (key != kInvalidThreadLocalStorageKey) {
    if (::TlsFree(key))
      return true;
    std::cerr << "***ION error: Unable to delete thread-local storage key: "
              << GetLastError() << "\n";
  }
  return false;
}
#endif

//-----------------------------------------------------------------------------
//
// pthreads-specific public functions. (All non-Windows platforms use pthreads.)
//
//-----------------------------------------------------------------------------
#if !defined(ION_PLATFORM_WINDOWS)

size_t GetMaxThreadNameLength() {
#if THREAD_NAMING_SUPPORTED
  // The pthread library restricts thread name length. This length includes the
  // NUL byte at the end.
  static const size_t kMaxThreadNameLength = 16;
  return kMaxThreadNameLength - 1U;
#else
  return 0;
#endif
}

bool SetThreadName(const std::string& name) {
#if THREAD_NAMING_SUPPORTED
  std::string truncated_name = name;
  if (const size_t max_len = GetMaxThreadNameLength())
    truncated_name = truncated_name.substr(0, max_len);
#  if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  // On Apple platforms the ThreadId is assumed to be the current thread.
  return CheckPthreadSuccess(
      "Naming thread", ::pthread_setname_np(truncated_name.c_str()));
#  else
  return CheckPthreadSuccess(
      "Naming thread",
      ::pthread_setname_np(::pthread_self(), truncated_name.c_str()));
#  endif
#else
  return false;
#endif
}

ThreadLocalStorageKey CreateThreadLocalStorageKey() {
  ThreadLocalStorageKey key = kInvalidThreadLocalStorageKey;
  CheckPthreadSuccess("Creating thread-local storage key",
                      ::pthread_key_create(&key, nullptr));
  return key;
}

bool SetThreadLocalStorage(ThreadLocalStorageKey key, void* ptr) {
  if (key != kInvalidThreadLocalStorageKey) {
    return CheckPthreadSuccess("Setting thread-local storage area",
                               ::pthread_setspecific(key, ptr));
  }
  return false;
}

void* GetThreadLocalStorage(ThreadLocalStorageKey key) {
  return key == kInvalidThreadLocalStorageKey ? nullptr :
      ::pthread_getspecific(key);
}

bool DeleteThreadLocalStorageKey(ThreadLocalStorageKey key) {
  if (key != kInvalidThreadLocalStorageKey) {
    return CheckPthreadSuccess("Deleting thread-local storage key",
                               ::pthread_key_delete(key));
  }
  return false;
}

#endif

#undef API_DECL
#undef THREAD_NAMING_SUPPORTED

}  // namespace port
}  // namespace ion
