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

#include "ion/port/semaphore.h"

#include <thread>  // NOLINT(build/c++11)

#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
#include <mach/mach_init.h>
#include <mach/task.h>
#include <time.h>
#elif defined(ION_PLATFORM_ASMJS)
#include <cassert>
#elif !defined(ION_PLATFORM_WINDOWS)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#endif

namespace ion {
namespace port {

Semaphore::Semaphore() {
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  semaphore_ = dispatch_semaphore_create(0);
#elif defined(ION_PLATFORM_WINDOWS)
  // Creates a semaphore with a max value of 0x7fffffff.
  semaphore_ = CreateSemaphore(nullptr, 0, 0x7fffffff, nullptr);
  value_.store(0);
#elif defined(ION_PLATFORM_ASMJS)
  value_ = 0;
#else
  sem_init(&semaphore_, 0, 0);
#endif
}

Semaphore::Semaphore(uint32 initial_value) {
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  // Dispatch semaphores must start at 0 or they will die when released if the
  // final value is not equal to the initial value.
  semaphore_ = dispatch_semaphore_create(0);
  while (initial_value--)
    Post();
#elif defined(ION_PLATFORM_WINDOWS)
  // Creates a semaphore with a max value of 0x7fffffff.
  // The initial count is stored in value_, so the underlying semaphore starts
  // with an inital count of zero.
  semaphore_ = CreateSemaphore(nullptr, 0, 0x7fffffff, nullptr);
  value_.store(initial_value);
#elif defined(ION_PLATFORM_ASMJS)
  // Asmjs lacks threads, so the best we can do is keep a simple counter and
  // assert() on invalid calls (a Wait() will never succeed if it cannot
  // succeed immediately).
  value_ = initial_value;
#else
  sem_init(&semaphore_, 0, initial_value);
#endif
}

Semaphore::~Semaphore() {
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  // In ARC mode, semaphore_ is released automatically.
#if !defined(__has_feature) || !__has_feature(objc_arc)
  dispatch_release(semaphore_);
#endif
#elif defined(ION_PLATFORM_WINDOWS)
  CloseHandle(semaphore_);
#elif defined(ION_PLATFORM_ASMJS)
  // Nothing to do.
#else
  sem_destroy(&semaphore_);
#endif
}

bool Semaphore::Post() {
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  // Ensure semaphore is valid before trying to signal it.
  return semaphore_
    ? (dispatch_semaphore_signal(semaphore_) || true)
    : false;
#elif defined(ION_PLATFORM_WINDOWS)
  if (++value_ > 0) {
    // Fast path.  There are no waiters, so incrementing the value is enough.
    return true;
  } else {
    // Slow path.  There are some waiters... notify one of them.
    return ReleaseSemaphore(semaphore_, 1, nullptr) != 0;
  }
#elif defined(ION_PLATFORM_ASMJS)
  ++value_;
  return true;
#else
  return sem_post(&semaphore_) == 0;
#endif
}

bool Semaphore::TryWait() {
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  // Ensure semaphore is valid before trying to wait on it.
  return semaphore_
    ? (0 == dispatch_semaphore_wait(semaphore_, DISPATCH_TIME_NOW))
    : false;
#elif defined(ION_PLATFORM_WINDOWS)
  int val = value_.load(std::memory_order_acquire);
  while (val > 0) {
    // Fast path.  Attempt to decrement value by 1; this may fail if another
    // thread has used the semaphore, causing value_ to change.  Keep trying
    // until we sucessfully decrement the value.
    // NOTE: due to a bug in msvc12, we use the 4-param version of the function,
    // and explicitly pass "acquire" as the failure_order.  The semantics are
    // unchanged: according to the C++ standard, when "acq_rel" is passed to the
    // 3-param version, the failure_order is implicitly "acquire".
    if (value_.compare_exchange_weak(val, val - 1,
                                     std::memory_order_acq_rel,
                                     std::memory_order_acquire)) {
      return true;
    }
  }
  // If the value is <= 0, waiting for the semaphore would require blocking.
  return false;
#elif defined(ION_PLATFORM_ASMJS)
  if (value_ > 0) {
    --value_;
    return true;
  } else {
    return false;
  }
#else
  return sem_trywait(&semaphore_) == 0;
#endif
}

bool Semaphore::TimedWaitMs(int64 timeout_in_ms) {
  if (timeout_in_ms < 0)
    return Wait();

#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  // Compute deadline as nanosecond offset from current time.
  static const int64 kMsecToNsec = 1000000;
  const dispatch_time_t deadline = dispatch_time(DISPATCH_TIME_NOW,
                                                 timeout_in_ms * kMsecToNsec);
  // Ensure semaphore is valid before trying to wait on it.
  return semaphore_
    ? (0 == dispatch_semaphore_wait(semaphore_, deadline))
    : false;
#elif defined(ION_PLATFORM_WINDOWS)
  if (value_-- > 0) {
    // Fast path.  There was a surplus of signals, so there is no need to wait.
    return true;
  }
  // Slow path.  Wait until signalled, or timeout expires.
  if (WaitForSingleObject(semaphore_, static_cast<DWORD>(timeout_in_ms)) ==
          WAIT_OBJECT_0) {
    // Semaphore was signalled within time limit.
    return true;
  } else {
    // Timeout.  Undo initial predecrement, since we're no longer waiting.
    ++value_;
    return false;
  }
#elif defined(ION_PLATFORM_ASMJS)
  // Waiting doesn't make sense in asmjs (no threads) so just do a TryWait.
  return TryWait();
#else
  static const int64 kMsecToNsec = 1000000;
  static const int64 kNsecPerSec = 1000000000;

  // Timeout is in seconds and nanoseconds, relative to epoch.
  timeval now;
  ::gettimeofday(&now, nullptr);
  // The number of ns is the number requested plus the current time in ns.
  const uint64 timeout_in_ns = now.tv_usec * 1000 + timeout_in_ms * kMsecToNsec;

  timespec timeout;
  // Add additional seconds if the time has more ns than kNsecPerSec.
  timeout.tv_nsec = static_cast<int32>(timeout_in_ns % kNsecPerSec);
  timeout.tv_sec = static_cast<int32>(now.tv_sec + timeout_in_ns / kNsecPerSec);
#if !defined(ION_PLATFORM_PNACL) && !defined(ION_PLATFORM_NACL)
  return sem_timedwait(&semaphore_, &timeout) == 0;
#else
  // NaCl doesn't support sem_timedwait() for now, so spin-wait.
  while (!TryWait()) {
    std::this_thread::yield();
    ::gettimeofday(&now, nullptr);
    if (now.tv_sec > timeout.tv_sec ||
        (now.tv_sec == timeout.tv_sec &&
         now.tv_usec * 1000 > timeout.tv_nsec)) {
      return false;
    }
  }
  return true;
#endif
#endif
}

bool Semaphore::Wait() {
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  // Ensure semaphore is valid before trying to wait on it.
  return semaphore_
    ? (dispatch_semaphore_wait(semaphore_, DISPATCH_TIME_FOREVER) || true)
    : false;
#elif defined(ION_PLATFORM_WINDOWS)
  // Spin on TryWait() before waiting on heavyweight semaphore.
  int spin_count = 2;
  while (spin_count--) {
    if (TryWait()) {
      // Fast path.
      return true;
    }
  }
  // Spinning on TryWait() failed, so check once more if we need to wait.
  if (value_-- > 0) {
    // Fast path.  There is a surplus of signals, therefore no need to wait.
    return true;
  } else {
    // Slow path.
    if (WaitForSingleObject(semaphore_, INFINITE) == WAIT_OBJECT_0) {
      return true;
    } else {
      // Wait failed.  Undo the decremented |value_|, since the underlying
      // Windows semaphore was not affected.
      ++value_;       // COV_NF_LINE
      return false;   // COV_NF_LINE
    }
  }
#elif defined(ION_PLATFORM_ASMJS)
  if (value_ > 0) {
    --value_;
    return true;
  } else {
    assert(false &&
        "Semaphore::Wait cannot block on a platform without threads.");
    return false;
  }
#else
  return sem_wait(&semaphore_) == 0;
#endif
}
}  // namespace port
}  // namespace ion
