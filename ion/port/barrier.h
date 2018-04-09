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

#ifndef ION_PORT_BARRIER_H_
#define ION_PORT_BARRIER_H_

#if defined(ION_PLATFORM_WINDOWS)
#  include <windows.h>
#else
#  include <pthread.h>
#include <unistd.h>
#endif

#include "base/integral_types.h"
#include "base/macros.h"
#include "ion/port/atomic.h"

namespace ion {
namespace port {

// The Barrier class defines a multi-thread barrier that allows N threads to
// synchronize execution. For example, if you create a Barrier for 3 threads
// and have each of the three threads call Wait() on it, then execution of each
// waiting thread will proceed once all 3 have called Wait().
class Barrier {
 public:
  // Constructs an instance that will wait for thread_count threads. If
  // thread_count is not positive, the Barrier will do nothing and IsValid()
  // will return false.
  explicit Barrier(uint32 thread_count);
  ~Barrier();

  // Returns true if a valid barrier was created by the constructor. If this
  // returns false, the Wait() function is a no-op.
  bool IsValid() const { return is_valid_; }

  // Causes the current thread to wait at the barrier.
  void Wait();

 private:
#if defined(ION_PLATFORM_WINDOWS)
  // See section 3.6.5-3.6.7 of The Little Book of Semaphores
  // http://greenteapress.com/semaphores/downey08semaphores.pdf
  void WaitInternal(int32 increment, int32 limit, HANDLE turnstile);

  const int32 thread_count_;
  std::atomic<int32> wait_count_;
  HANDLE turnstile1_;
  HANDLE turnstile2_;
  // Counts threads inside the turnstile to prevent handle destruction race.
  HANDLE exit_turnstile_;

#elif defined(_POSIX_BARRIERS) && (_POSIX_BARRIERS > 0)
  pthread_barrier_t barrier_;
  // pthread_barrier_wait() modifies state after unblocking, so we need a bit
  // more synchronization to make sure we don't destroy the barrier too soon.
  std::atomic<int32> ref_count_;

#else
  // See comments in source code.
  pthread_cond_t condition1_;
  pthread_cond_t condition2_;
  pthread_cond_t exit_condition_;
  pthread_mutex_t mutex_;
  const int32 thread_count_;
  int32 wait_count1_;
  int32 wait_count2_;
  int32 ref_count_;
#endif

  // This is set to true if a valid barrier was created in the constructor.
  const bool is_valid_;

  DISALLOW_COPY_AND_ASSIGN(Barrier);
};

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_BARRIER_H_
