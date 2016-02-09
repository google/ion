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

#include <assert.h>

#include "ion/port/mutex.h"

namespace ion {
namespace port {

// POSIX mutexes are non-recursive, meaning that if the same thread tries to
// Lock() the mutex, it will block indefinitely. On Windows, a CRITICAL_SECTION
// can be entered recursively, so long as the number of EnterCriticalSection()s
// matches the number of LeaveCriticalSection()s, and TryEnterCriticalSection()
// always succeeds if the same thread is already in the critical section. POSIX
// threads can match this behavior if you set them PTHREAD_MUTEX_RECURSIVE.
//
// Recursive locks are in general a bad idea, and lead to poorly written code:
// if a lock is held, there is no reason to try to acquire that lock again.
//
// Windows supports non-recursive Mutexes, but they are very slow. Instead, we
// wrap an Event test around Interlocked(Increment|Decrement), which are
// very fast (faster than (Try)EnterCriticalSection()) atomic operations. A
// slower path is only taken when there are multiple contenders; if a single
// thread repeatedly Lock()s and Unlock()s a mutex, the Event is never tested,
// and the only cost is the (very small) increment and decrement cost plus that
// of an if. Intel has shown that the costs of WaitForSingleObject() and
// EnterCriticalSection() are very similar for multiple threads under high
// contention:
// See http://software.intel.com/en-us/articles/implementing-scalable-atomic-
// locks-for-multi-core-intel-em64t-and-ia32-architectures
//
// To use a CRITICAL_SECTION anyway, uncomment the following line, otherwise the
// above interlocking increment/decrement scheme is used. This will cause tests
// in mutex_tests.cc and lockguards_test.cc to fail; their logic must be
// updated.
// #define ION_MUTEX_USE_CRITICAL_SECTION

Mutex::Mutex() {
#if defined(ION_PLATFORM_WINDOWS)
#  if defined(ION_MUTEX_USE_CRITICAL_SECTION)
    InitializeCriticalSection(&critical_section_);
#  else
    blockers_ = 0;
    thread_id_ = -1;
    event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
#  endif
#elif defined(ION_PLATFORM_ASMJS)
  locked_ = false;
#else
  pthread_mutex_init(&pthread_mutex_, NULL);
#endif
}

Mutex::~Mutex() {
#if defined(ION_PLATFORM_WINDOWS)
#  if defined(ION_MUTEX_USE_CRITICAL_SECTION)
    DeleteCriticalSection(&critical_section_);
#  else
    CloseHandle(event_);
#  endif
#elif defined(ION_PLATFORM_ASMJS)
  locked_ = false;
#else
  pthread_mutex_destroy(&pthread_mutex_);
#endif
}

bool Mutex::IsLocked() {
  if (TryLock()) {
    Unlock();
    return false;
  } else {
    return true;
  }
}

void Mutex::Lock() {
#if defined(ION_PLATFORM_WINDOWS)
#  if defined(ION_MUTEX_USE_CRITICAL_SECTION)
    EnterCriticalSection(&critical_section_);
#  else
    // Increment the counter. If it is greater than 1, then there is another
    // contender already holding the lock, so we must wait for it to release
    // it. The holder may be the current thread, causing an infinite wait. If
    // the increment returns 0, then we are only holder, and there is nothing
    // to do.
    if (InterlockedIncrement(&blockers_) > 1)
      WaitForSingleObject(event_, INFINITE);
    // Store the id of the thread with the lock. Only it can call Unlock(). Only
    // one contender will be able to set thread_id at a time.
    thread_id_ = GetCurrentThreadId();
#  endif
#elif defined(ION_PLATFORM_ASMJS)
  assert(!locked_);
  locked_ = true;
#else
  pthread_mutex_lock(&pthread_mutex_);
#endif
}

bool Mutex::TryLock() {
#if defined(ION_PLATFORM_WINDOWS)
#  if defined(ION_MUTEX_USE_CRITICAL_SECTION)
    return TryEnterCriticalSection(&critical_section_);
#  else
    // Increment the counter. If it is greater than 1, then there is another
    // contender already holding the lock, so we decrement the counter and
    // return false since we could not get the lock. If the increment returns 0,
    // then we have an exclusive lock.
    if (InterlockedIncrement(&blockers_) > 1) {
      InterlockedDecrement(&blockers_);
      return false;
    } else {
      thread_id_ = GetCurrentThreadId();
      return true;
    }
#  endif
#elif defined(ION_PLATFORM_ASMJS)
  if (!locked_) {
    locked_ = true;
    return true;
  } else {
    return false;
  }
#else
  return pthread_mutex_trylock(&pthread_mutex_) == 0;
#endif
}

void Mutex::Unlock() {
#if defined(ION_PLATFORM_WINDOWS)
#  if defined(ION_MUTEX_USE_CRITICAL_SECTION)
    LeaveCriticalSection(&critical_section_);
#  else
    // Only the thread that owns the lock can release it.
    if (thread_id_ == GetCurrentThreadId()) {
      // The next thread to acquire the lock will set its id.
      thread_id_ = -1;
      // Decrement the counter. If it still greater than 0, then there is at
      // least one more contender attempting to obtain the lock. SetEvent() will
      // wake one of them so that they can acquire the lock.
      if (InterlockedDecrement(&blockers_) > 0)
        SetEvent(event_);
    }
#  endif
#elif defined(ION_PLATFORM_ASMJS)
  assert(locked_);
  locked_ = false;
#else
  pthread_mutex_unlock(&pthread_mutex_);
#endif
}

#undef ION_MUTEX_USE_CRITICAL_SECTION

}  // namespace port
}  // namespace ion
