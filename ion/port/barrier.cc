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

#include "ion/port/barrier.h"

#if defined(ION_PLATFORM_WINDOWS)
#  include <assert.h>  // For checking return values since port has no logging.
#  include <algorithm>  // for swap
#endif

namespace ion {
namespace port {

//-----------------------------------------------------------------------------
//
// Linux and QNX are the only platforms that support barriers in pthreads. Both
// the windows and non-Linux/QNX implementations below may seem rather complex.
// This is because they guard against two potential errors:
//   - Deadlock between a wait and a broadcast/set event, ensuring that all
//     threads have entered the wait branch before the broadcast.
//   - An issue where the Barrier destructor is called before all threads are
//     done using the synchronization objects, causing an intermittent crash.
//     For example, a mutex may be destroyed while it is still being held by
//     pthread_cond_wait(), or a handle can be reset after it has been closed
//     in the destructor.
//
//-----------------------------------------------------------------------------

#if defined(ION_PLATFORM_WINDOWS)
//-----------------------------------------------------------------------------
//
// Windows version.
//
// Windows 8 introduces real barrier synchronization functions
// (InitializeSynchronizationBarrier, EnterSynchronizationBarrier, and
// DeleteSynchronizationBarrier). Unfortunately, we're stuck with older APIs,
// which don't have great alternatives.
//
// Our solution is based on sections 3.6.5-3.6.7 of The Little Book of
// Semaphores by Allen B. Downey.
//   http://greenteapress.com/semaphores/downey08semaphores.pdf
//
// We use a turnstile to wait until all the threads have arrived, then we use a
// second turnstile to make sure they've all passed through and that the first
// is ready for re-use.
//
//-----------------------------------------------------------------------------

Barrier::Barrier(uint32 thread_count)
    : thread_count_(thread_count),
      wait_count_(0),
      turnstile1_(nullptr),
      turnstile2_(nullptr),
      exit_turnstile_(nullptr),
      is_valid_(thread_count_ > 0) {
  if (IsValid()) {
    turnstile1_ = CreateSemaphore(nullptr, 0, thread_count_, nullptr);
    turnstile2_ = CreateSemaphore(nullptr, 0, thread_count_, nullptr);
    exit_turnstile_ = CreateSemaphore(nullptr, 0, thread_count_, nullptr);
    ::ReleaseSemaphore(exit_turnstile_, thread_count_, nullptr);
  }
}

Barrier::~Barrier() {
  if (IsValid()) {
    // Drain any remaining Wait calls by consuming all the semaphore slots. This
    // waits efficiently until the threads exit calls to the Wait function and
    // are thus done with the turnstile semaphores.
    for (int semaphore_slot = 0; semaphore_slot < thread_count_;
         ++semaphore_slot) {
      ::WaitForSingleObject(exit_turnstile_, INFINITE);
    }

    CloseHandle(turnstile2_);
    CloseHandle(turnstile1_);
    CloseHandle(exit_turnstile_);
  }
}

void Barrier::Wait() {
  if (IsValid() && thread_count_ > 1) {
    // Consume one turnstile in-use slot.
    DWORD acquire_status = WaitForSingleObject(exit_turnstile_, INFINITE);
    assert(acquire_status == WAIT_OBJECT_0);

    // Wait for all the threads to come in.
    WaitInternal(1, thread_count_, turnstile1_);

    // And then wait for them all to go out, which ensures that the barrier is
    // ready for another round.
    WaitInternal(-1, 0, turnstile2_);

    // Release one turnstile in-use slot.
    BOOL release_status = ::ReleaseSemaphore(exit_turnstile_, 1, nullptr);
    assert(release_status != 0);
  }
}

void Barrier::WaitInternal(int32 increment, int32 limit, HANDLE turnstile) {
  // This object may begin destruction almost as soon as the increment is
  // applied. The case is where multiple threads are Wait'ing to finish a
  // Barrier Wait operation, after which the owner destroys the Barrier. If a
  // background thread is interrupted after applying the increment, the thread
  // owning the Barrier may then proceed destructing it. Then this thread races
  // the owning thread to call WaitForSingleObject before the owning thread
  // calls CloseHandle.
  if ((wait_count_ += increment) == limit) {
    // Last thread is in.  Release the hounds.
    BOOL status = ReleaseSemaphore(turnstile, thread_count_ - 1, nullptr);
    (void)status;  // Avoid warnings when the assertion is optimized out.
    assert(status != 0);
  } else {
    DWORD status = WaitForSingleObject(turnstile, INFINITE);
    (void)status;  // Avoid warnings when the assertion is optimized out.
    assert(status == WAIT_OBJECT_0);
  }
}

#elif defined(_POSIX_BARRIERS) && (_POSIX_BARRIERS > 0)
//-----------------------------------------------------------------------------
//
// Barrier pthreads version.
//
//-----------------------------------------------------------------------------

Barrier::Barrier(uint32 thread_count)
    : ref_count_(0), is_valid_(thread_count > 0) {
  if (IsValid()) {
    pthread_barrier_init(&barrier_, nullptr, thread_count);
  }
}

Barrier::~Barrier() {
  if (IsValid()) {
    // Block until no thread is in the Wait() method, meaning we know that
    // pthread_barrier_wait() has returned.
    while (ref_count_.load(std::memory_order::memory_order_acquire) != 0) {
    }
    pthread_barrier_destroy(&barrier_);
  }
}

void Barrier::Wait() {
  if (IsValid()) {
    ref_count_.fetch_add(1, std::memory_order::memory_order_acquire);
    pthread_barrier_wait(&barrier_);
    ref_count_.fetch_add(-1, std::memory_order::memory_order_release);
  }
}

#else
//-----------------------------------------------------------------------------
//
// Non-barrier pthreads version.
//
// Pthread barriers are an optional part of the Posix spec, and Mac, iOS, and
// Android do not support them, so this version is used on those platforms.
//
//-----------------------------------------------------------------------------

Barrier::Barrier(uint32 thread_count)
    : thread_count_(thread_count),
      wait_count1_(0),
      wait_count2_(0),
      ref_count_(0),
      is_valid_(thread_count_ > 0) {
  if (IsValid()) {
    pthread_cond_init(&condition1_, nullptr);
    pthread_cond_init(&condition2_, nullptr);
    pthread_cond_init(&exit_condition_, nullptr);
    pthread_mutex_init(&mutex_, nullptr);
  }
}

Barrier::~Barrier() {
  if (IsValid()) {
    pthread_mutex_lock(&mutex_);
    while (ref_count_ > 0) {
      pthread_cond_wait(&exit_condition_, &mutex_);
    }
    pthread_mutex_unlock(&mutex_);
    pthread_cond_destroy(&condition1_);
    pthread_cond_destroy(&condition2_);
    pthread_cond_destroy(&exit_condition_);
    pthread_mutex_destroy(&mutex_);
  }
}

void Barrier::Wait() {
  if (IsValid()) {
    pthread_mutex_lock(&mutex_);
    ++ref_count_;

    // Wait at the first condition.
    if (++wait_count1_ == thread_count_) {
      wait_count2_ = 0;
      pthread_cond_broadcast(&condition1_);
    } else {
      do {
        // Wait for the last thread to wait.
        pthread_cond_wait(&condition1_, &mutex_);
      } while (wait_count1_ != thread_count_);
    }

    // Wait at the second condition.
    if (++wait_count2_ == thread_count_) {
      wait_count1_ = 0;
      pthread_cond_broadcast(&condition2_);
    } else {
      do {
        // Wait for the last thread to wait.
        pthread_cond_wait(&condition2_, &mutex_);
      } while (wait_count2_ != thread_count_);
    }

    // If the destructor has already been entered and this is the last thread
    // then the return value of the decrement will be 0. Signal the condition to
    // allow the destructor to proceed.
    if (--ref_count_ == 0) {
      pthread_cond_broadcast(&exit_condition_);
    }
    pthread_mutex_unlock(&mutex_);
  }
}

#endif

}  // namespace port
}  // namespace ion
