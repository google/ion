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

#ifndef ION_PORT_SEMAPHORE_H_
#define ION_PORT_SEMAPHORE_H_

#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
#include <dispatch/dispatch.h>
#elif defined(ION_PLATFORM_WINDOWS)
#include <windows.h>
#include <atomic>
#else
#include <semaphore.h>
#endif

#include "base/integral_types.h"
#include "base/macros.h"

namespace ion {
namespace port {

// A Semaphore enables threads and process synchronization. Semaphores block via
// a call to Wait(), and are woken when another thread calls Post() on the same
// Semaphore. If multiple threads are Wait()ing, then a call to Post() will wake
// only one thread.
class ION_API Semaphore {
 public:
  // Initializes a semaphore with an internal value of zero.
  Semaphore();
  // Initializes a semaphore with an explicit initial value.
  explicit Semaphore(uint32 initial_value);
  ~Semaphore();

  // Wakes a single thread that is Wait()ing, or the next thread to call Wait().
  // Returns false if there was an error while trying to post.
  bool Post();
  // Blocks the calling thread until another thread calls Post(). Returns false
  // if there was an error while trying to wait.
  bool Wait();
  // Does not block. Returns whether the semaphore has been Post()ed. A return
  // value of true means that a call to Wait() would not have blocked, while
  // a return value of false means that a call to Wait() would have blocked.
  bool TryWait();
  // Blocks for a maximum of the passed number of milliseconds before returning.
  // Returns whether the semaphore was Post()ed within the timeout. Passing
  // a negative value for the timeout is equivalent to calling Wait();
  bool TimedWaitMs(int64 timeout_in_ms);

 private:
  // Platform specific implementation.
#if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
  dispatch_semaphore_t semaphore_;
#elif defined(ION_PLATFORM_WINDOWS)
  HANDLE semaphore_;
  std::atomic<int> value_;
#elif defined(ION_PLATFORM_ASMJS)
  int value_;
#else
  sem_t semaphore_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Semaphore);
};

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_SEMAPHORE_H_
