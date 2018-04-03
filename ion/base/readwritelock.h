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

#ifndef ION_BASE_READWRITELOCK_H_
#define ION_BASE_READWRITELOCK_H_

#include <mutex>  // NOLINT(build/c++11)

#include "ion/port/atomic.h"
#include "ion/port/semaphore.h"

namespace ion {
namespace base {

// The ReadWriteLock class defines a non-promotable lock that is very fast when
// only readers try to obtain the lock, but slower than a Mutex when there are
// writers. A ReadWriteLock allows any number of readers to enter the lock as
// long as there are no writers, but each writer obtains exclusive access to the
// lock. At a high level, a ReadWriteLock behaves like an atomic integer under
// no or reader-only contention, and like a Mutex when there are any writers.
// Note that since this implementation is non-promotable, attempting to acquire
// a write lock while holding a read lock will block the caller.
//
// This implementation is based on sections 4.2.5 of The Little Book of
// Semaphores by Allen B. Downey.
//   http://greenteapress.com/semaphores/downey08semaphores.pdf
//
// This particular implementation has the following behaviors:
// - If there are no writers, then only the first reader to obtain the read lock
//   Wait()s on a Semaphore.
// - If there are no writers, then only the last reader to unlock the read lock
//   actually Post()s a Semaphore.
// - Writers cannot obtain a lock until all readers have exited, but while
//   writers are waiting readers Lock() and Unlock() a Mutex in addition to the
//   above semaphore behavior. This prevents writers them from starving at the
//   expense of readers. If writing is more common than reading, however, it is
//   faster to use a regular Mutex.
class ION_API ReadWriteLock {
 public:
  ReadWriteLock();
  ~ReadWriteLock();

  // Locks the ReadWriteLock for reading. This will block other readers if there
  // is a writer is in the lock, and will cause LockForWrite() to block until
  // the last reader that has entered the lock calls UnlockForRead().
  void LockForRead();
  // Unlocks the ReadWriteLock for reading, which will allow writers to obtain a
  // lock once the last reader has exited.
  void UnlockForRead();
  // Locks the ReadWriteLock for writing. This will cause any callers of
  // LockFor*() to block until the caller calls UnlockForWrite().
  void LockForWrite();
  // Unlocks the ReadWriteLock for writing. This will allow other callers to
  // obtain a read or write lock.
  void UnlockForWrite();

  // Returns the number of readers in this lock.
  int GetReaderCount() const { return reader_count_; }
  // Returns the number of writers in this lock.
  int GetWriterCount() const { return writer_count_; }

 private:
  std::atomic<int> reader_count_;
  std::atomic<int> writer_count_;
  port::Semaphore room_empty_;
  std::mutex turnstile_;

  DISALLOW_COPY_AND_ASSIGN(ReadWriteLock);
};

// A ReadLock obtains a read lock, but has a similar interface to a Mutex and
// can be used with a ReadGuard.
class ION_API ReadLock {
 public:
  // The passed pointer must be non-NULL.
  explicit ReadLock(ReadWriteLock* lock) : lock_(*lock) {}
  ~ReadLock() {}
  // "Locks" the lock for reading, which may or may not block. See the comments
  // for ReadWriteLock.
  void Lock() { lock_.LockForRead(); }
  // "Unlocks" the lock for reading, which may allow other writers to proceed.
  // See the comments for ReadWriteLock.
  void Unlock() { lock_.UnlockForRead(); }
  // Returns whether there are any readers in the lock. This is not the same as
  // what it means for a Mutex to be locked, but is a convenient way of
  // determining whether any readers are currently active though a Mutex-like
  // interface that LockGuards can understand.
  bool IsLocked() const { return lock_.GetReaderCount() > 0; }

 private:
  ReadWriteLock& lock_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReadLock);
};

// A WriteLock obtains a write lock, but has a similar interface to a Mutex and
// can be used with a WriteGuard.
class ION_API WriteLock {
 public:
  // The passed pointer must be non-NULL.
  explicit WriteLock(ReadWriteLock* lock) : lock_(*lock) {}
  ~WriteLock() {}
  // "Locks" the lock for writing, which blocks if there are any readers or
  // writers holding the lock. See the comments for ReadWriteLock.
  void Lock() { lock_.LockForWrite(); }
  // "Unlocks" the lock for writing, which may allow other readers or writers to
  // proceed. See the comments for ReadWriteLock.
  void Unlock() { lock_.UnlockForWrite(); }
  // Returns whether there are any writers in the lock. This is similar to what
  // it means for a Mutex to be locked.
  bool IsLocked() const { return lock_.GetWriterCount() > 0; }

 private:
  ReadWriteLock& lock_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WriteLock);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_READWRITELOCK_H_
