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

#ifndef ION_BASE_SPINMUTEX_H_
#define ION_BASE_SPINMUTEX_H_

#include "ion/port/atomic.h"

namespace ion {
namespace base {

// SpinMutex exposes the same interface as ion::port::Mutex, but implements
// locking via a simple atomic CAS.  This gives it two advantages:
//   - higher performance (when used in appropriate situations)
//   - smaller memory footprint
// The downside is that waiters continue to consume CPU cycles as they wait.
//
// This implementation is extremely simple, and makes no attempt at fairness.
// Starvation is possible; when the mutex is already locked, there is no
// guarantee that a thread that begins to wait for the mutex will become
// unblocked before subsequent blocking threads.
class ION_API SpinMutex {
 public:
  SpinMutex() : locked_(false) {}
  ~SpinMutex();

  // Returns whether the Mutex is currently locked. Does not block.
  bool IsLocked() const { return locked_; }

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
  std::atomic<bool> locked_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SPINMUTEX_H_
