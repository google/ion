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

#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL)
# define THREAD_NAMING_SUPPORTED 0
#else
# define THREAD_NAMING_SUPPORTED 1
#endif

namespace ion {
namespace port {

namespace {

//-----------------------------------------------------------------------------
//
// Platform-independent helper functions.
//
//-----------------------------------------------------------------------------

// Sets the ThreadId of the main thread and returns it.  This must be called
// before any new thread is spawned. If id is kInvalidThreadId, this sets the
// main thread ID to the current thread ID unless the main thread ID was
// already set.
static ThreadId InitMainThreadId(ThreadId id) {
  static ThreadId main_thread_id = kInvalidThreadId;
  if (id == kInvalidThreadId) {
    if (main_thread_id == kInvalidThreadId)
      main_thread_id = GetCurrentThreadId();
  } else {
    main_thread_id = id;
  }
  return main_thread_id;
}

// Thread callback functions return a DWORD on Windows and void* with pthreads.
// Casting a Boolean success code to a void* requires a reinterpret_cast, but
// that causes an error with DWORD on Windows. This specialized little adapter
// function fixes this problem.
template <typename ReturnType> static ReturnType CastSuccessCode(bool success) {
  return static_cast<ReturnType>(success ? 0 : 1);
}
template <> void* CastSuccessCode(bool success) {
  return reinterpret_cast<void*>(static_cast<size_t>(success ? 0 : 1));
}

// Spawns a new thread, invoking the function pointer passed via arg.
template <typename ReturnType>
static ReturnType API_DECL InvokeThreadFuncPtr(void* arg) {
  ThreadFuncPtr func_ptr = reinterpret_cast<ThreadFuncPtr>(arg);

  // The return value does not matter as long as it is zero when successful.
  const bool success = (*func_ptr)();
  return CastSuccessCode<ReturnType>(success);
}

// Spawns a new thread, invoking the std::function passed via arg.
template <typename ReturnType>
static ReturnType API_DECL InvokeThreadFuncStd(void* arg) {
  ThreadStdFunc* func = reinterpret_cast<ThreadStdFunc*>(arg);

  // The return value does not matter as long as it is zero when successful.
  const bool success = (*func)();
  return CastSuccessCode<ReturnType>(success);
}

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

// This is called by SpawnThread() to do the platform-specific part.
static void CreateThread(const ThreadFuncPtr func_ptr, void* arg,
                         ThreadId* id) {
  if (!::CreateThread(NULL, 0, InvokeThreadFuncPtr<DWORD>, arg, 0, id))
    std::cerr << "***ION error: Unable to create thread: " << GetLastError()
              << "\n";
}

// This is called by SpawnThreadStd() to do the platform-specific part.
static void CreateThreadStd(const ThreadStdFunc* func, void* arg,
                            ThreadId* id) {
  if (!::CreateThread(NULL, 0, InvokeThreadFuncStd<DWORD>, arg, 0, id))
    std::cerr << "***ION error: Unable to create thread: " << GetLastError()
              << "\n";
}

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
    std::cerr << "Pthread error: " << what << " returned "
              << result << ": " << strerror(result) << "\n";
    return false;
#endif  // COV_NF_END
  }
  return true;
}

// This is called by SpawnThread() to do the platform-specific part.
static void CreateThread(const ThreadFuncPtr func_ptr, void* arg,
                         ThreadId* id) {
  CheckPthreadSuccess(
      "Creating thread",
      ::pthread_create(id, NULL, InvokeThreadFuncPtr<void*>, arg));
}

// This is called by SpawnThreadStd() to do the platform-specific part.
static void CreateThreadStd(const ThreadStdFunc* func, void* arg,
                            ThreadId* id) {
  CheckPthreadSuccess(
      "Creating thread",
      ::pthread_create(id, NULL, InvokeThreadFuncStd<void*>, arg));
}

#endif

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Platform-independent public functions.
//
//-----------------------------------------------------------------------------

ThreadId SpawnThread(const ThreadFuncPtr func_ptr) {
  // A non-main thread may be created, so make sure the main thread ID is set.
  // This will set it to the current thread unless it was already set.
  InitMainThreadId(kInvalidThreadId);

  ThreadId id = kInvalidThreadId;
  if (func_ptr) {
    void* arg = const_cast<void*>(reinterpret_cast<const void*>(func_ptr));
    CreateThread(func_ptr, arg, &id);
  }
  return id;
}

ThreadId SpawnThreadStd(const ThreadStdFunc* func) {
  // A non-main thread may be created, so make sure the main thread ID is set.
  // This will set it to the current thread unless it was already set.
  InitMainThreadId(kInvalidThreadId);

  ThreadId id = kInvalidThreadId;
  if (func && *func) {
    void* arg = const_cast<void*>(reinterpret_cast<const void*>(func));
    CreateThreadStd(func, arg, &id);
  }
  return id;
}

bool IsThreadNamingSupported() {
  return THREAD_NAMING_SUPPORTED;
}

bool IsMainThread() {
  return GetCurrentThreadId() == InitMainThreadId(kInvalidThreadId);
}

void SetMainThreadId(ThreadId id) {
  InitMainThreadId(id);
}

//-----------------------------------------------------------------------------
//
// Windows-specific public functions.
//
//-----------------------------------------------------------------------------
#if defined(ION_PLATFORM_WINDOWS)

bool JoinThread(ThreadId id) {
  if (id != kInvalidThreadId) {
    if (const HANDLE handle = ::OpenThread(THREAD_ALL_ACCESS, 0, id)) {
      ::WaitForSingleObject(handle, INFINITE);
      ::CloseHandle(handle);
      return true;
    }
    std::cerr << "***ION error: Could not join thread with ID " << id
              << ": error " << GetLastError() << "\n";
  }
  return false;
}

size_t GetMaxThreadNameLength() {
  return 0;
}

bool SetThreadName(const std::string& name) {
  bool success = false;
  // Search MSDN for "How to Set a Thread Name in Native Code" for the source
  // of this ridiculous implementation.
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

void YieldThread() {
  ::SwitchToThread();
}

ThreadId GetCurrentThreadId() {
  return ::GetCurrentThreadId();
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
  return key == kInvalidThreadLocalStorageKey ? NULL : ::TlsGetValue(key);
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

bool JoinThread(ThreadId id) {
  if (id != kInvalidThreadId) {
    return CheckPthreadSuccess("Joining thread", ::pthread_join(id, NULL));
  }
  return false;
}

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
      ::pthread_setname_np(GetCurrentThreadId(), truncated_name.c_str()));
#  endif
#else
  return false;
#endif
}

void YieldThread() {
  CheckPthreadSuccess("Yielding thread", sched_yield());
}

ThreadId GetCurrentThreadId() {
  return ::pthread_self();
}

ThreadLocalStorageKey CreateThreadLocalStorageKey() {
  ThreadLocalStorageKey key = kInvalidThreadLocalStorageKey;
  CheckPthreadSuccess("Creating thread-local storage key",
                      ::pthread_key_create(&key, NULL));
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
  return key == kInvalidThreadLocalStorageKey ? NULL :
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
