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

#ifndef ION_PORT_THREADUTILS_H_
#define ION_PORT_THREADUTILS_H_

#if defined(ION_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <functional>
#include <string>

namespace ion {
namespace port {

#if defined(ION_PLATFORM_WINDOWS)
// Defines a type that can be used to identify a thread.
typedef DWORD ThreadId;
// Defines a type that is used to access thread-local storage.
typedef DWORD ThreadLocalStorageKey;
#else
// Defines a type that can be used to identify a thread.
typedef pthread_t ThreadId;
// Defines a type that is used to access thread-local storage.
typedef pthread_key_t ThreadLocalStorageKey;
#endif

//-----------------------------------------------------------------------------
//
// Thread types and constants.
//
//-----------------------------------------------------------------------------

// These types define a function that is called when a thread is spawned. They
// are supposed to return false if an error occurred. One is easiest to use if
// you have pointer to a simple function that does not take any arguments. The
// std::function version can be useful to package up a function that takes
// arbitrary arguments.
typedef bool (*ThreadFuncPtr)();
typedef std::function<bool()> ThreadStdFunc;

// Defines an invalid thread ID that can be used as an initial value or to
// indicate an error.
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC) || \
    defined(ION_PLATFORM_NACL)
static const ThreadId kInvalidThreadId = reinterpret_cast<ThreadId>(-1LL);
#else
static const ThreadId kInvalidThreadId = static_cast<ThreadId>(-1);
#endif

// Defines an invalid thread-local storage key that can be used as an initial
// value or to indicate an error.
static const ThreadLocalStorageKey kInvalidThreadLocalStorageKey =
    static_cast<ThreadLocalStorageKey>(-1);

//-----------------------------------------------------------------------------
//
// Thread lifetime functions.
//
//-----------------------------------------------------------------------------

// Spawns a new named thread that invokes the given function. Returns
// kInvalidThreadId on error. The second version takes an std::function,
// passed by pointer because the caller is responsible for ensuring that it
// persists through the creation of the new thread.
ION_API ThreadId SpawnThread(const ThreadFuncPtr func_ptr);
ION_API ThreadId SpawnThreadStd(const ThreadStdFunc* func);

// Waits for a thread to terminate. Returns immediately if the thread has
// already terminated.
ION_API bool JoinThread(ThreadId id);

// Causes the calling thread to relinquish the CPU if there are other threads
// waiting to execute.
ION_API void YieldThread();

//-----------------------------------------------------------------------------
//
// Thread naming functions.
//
//-----------------------------------------------------------------------------

// Returns true if the platform supports named threads.
// TODO(user): Remove this if LSB builds go away or support named threads.
ION_API bool IsThreadNamingSupported();

// Returns the maximum length of a thread name if restricted by the platform.
// Otherwise, returns 0.
ION_API size_t GetMaxThreadNameLength();

// Sets the name of the current thread. Does nothing but return false if
// IsThreadNamingSupported() returns false.  The thread name will be truncated
// if GetMaxThreadNameLength() is non-zero and the length of the name exceeds
// it.
ION_API bool SetThreadName(const std::string& name);

//-----------------------------------------------------------------------------
//
// Thread ID functions.
//
//-----------------------------------------------------------------------------

// Returns the ID of the currently-running thread.
ION_API ThreadId GetCurrentThreadId();

// Returns true if the current thread is the main thread.
ION_API bool IsMainThread();

// Sets the given thread ID to be considered the main thread; IsMainThread()
// will return true only for this thread. This is useful primarily for testing
// but can also be useful in situations where there is no persistent main
// thread, and one thread should be considered the main thread. This does
// nothing if id is kInvalidThreadId.
ION_API void SetMainThreadId(ThreadId id);

//-----------------------------------------------------------------------------
//
// Thread-local storage functions.
//
//-----------------------------------------------------------------------------

// Creates and returns a key that can be used to define thread-local storage
// areas. This returns kInvalidThreadLocalStorageKey if an error occurs.
ION_API ThreadLocalStorageKey CreateThreadLocalStorageKey();

// Associates ptr with the thread-local storage area indicated by key. Returns
// false on error.
ION_API bool SetThreadLocalStorage(ThreadLocalStorageKey key, void* ptr);

// Returns the pointer to the thread-local storage area indicated by
// key. Returns nullptr on error or if no thread-local storage was set.
ION_API void* GetThreadLocalStorage(ThreadLocalStorageKey key);

// Deletes a key returned by CreateThreadLocalStorageKey(). Returns false on
// error.
ION_API bool DeleteThreadLocalStorageKey(ThreadLocalStorageKey key);

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_THREADUTILS_H_
