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

#include "ion/base/readwritelock.h"

namespace ion {
namespace base {

// The semaphore is initialized to 1 so that the first Wait() succeeds.
ReadWriteLock::ReadWriteLock()
    : reader_count_(0), writer_count_(0), room_empty_(1) {}

ReadWriteLock::~ReadWriteLock() {}

void ReadWriteLock::LockForRead() {
  // Prevent readers from proceeding if a writer is waiting. This makes the lock
  // function more like a Mutex under write contention.
  if (writer_count_) {
    turnstile_.lock();
    turnstile_.unlock();
  }
  // The first reader prevents writers from obtaining the lock, or blocks until
  // a writer has finished.
  if (++reader_count_ == 1)
    room_empty_.Wait();
}

void ReadWriteLock::UnlockForRead() {
  // The last reader allows writers to proceed.
  if (--reader_count_ == 0)
    room_empty_.Post();
}

void ReadWriteLock::LockForWrite() {
  ++writer_count_;
  turnstile_.lock();
  room_empty_.Wait();
}

void ReadWriteLock::UnlockForWrite() {
  // Allow everyone to proceed.
  turnstile_.unlock();
  room_empty_.Post();
  --writer_count_;
}

}  // namespace base
}  // namespace ion
