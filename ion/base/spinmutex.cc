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

#include "ion/base/spinmutex.h"

#include <thread>  // NOLINT(build/c++11)

#include "ion/base/logging.h"

namespace ion {
namespace base {

SpinMutex::~SpinMutex() { DCHECK(!IsLocked()); }

void SpinMutex::Lock() {
  unsigned int try_count = 0;
  bool already_locked = false;
  while (!locked_.compare_exchange_weak(
             already_locked, true, std::memory_order_acquire)) {
    already_locked = false;
    if (++try_count > 1000) {
      std::this_thread::yield();
    }
  }
}

bool SpinMutex::TryLock() {
  bool already_locked = false;
  return locked_.compare_exchange_strong(
      already_locked, true, std::memory_order_acquire);
}

void SpinMutex::Unlock() {
  DCHECK(IsLocked());
  locked_.store(false, std::memory_order_release);
}

}  // namespace base
}  // namespace ion
