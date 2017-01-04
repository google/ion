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

#ifndef ION_PORT_MUTEX_H_
#define ION_PORT_MUTEX_H_

#if defined(ION_PLATFORM_WINDOWS)
#  include <windows.h>
#else
#  include <pthread.h>
#endif

#include "base/macros.h"

namespace ion {
namespace port {

// A Mutex is used to ensure that only one thread or process can access a block
// of code at one time.
class ION_API Mutex {
 public:
  Mutex();
  ~Mutex();

  // Returns whether the Mutex is currently locked. Does not block.
  bool IsLocked();
  // Locks the Mutex. Blocks the calling thread or process until the lock is
  // available; no thread or process can return from Lock() until the lock owner
  // Unlock()s.
  void Lock();
  // Returns true if the mutex was successfully locked, and false otherwise.
  // Does not block.
  bool TryLock();
  // Unlocks the Mutex. Any thread or process can now return from Lock(). Does
  // nothing if the Mutex was not locked by the calling thread or at all. Does
  // not block.
  void Unlock();

 private:
  // Platform specific mutex implementation.
#if defined(ION_PLATFORM_WINDOWS)
  CRITICAL_SECTION critical_section_;
  LONG blockers_;
  HANDLE event_;
  DWORD thread_id_;
#elif defined(ION_PLATFORM_ASMJS)
  bool locked_;
#else
  pthread_mutex_t pthread_mutex_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Mutex);
};

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_MUTEX_H_
