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

#ifndef ION_BASE_LOCKGUARDS_H_
#define ION_BASE_LOCKGUARDS_H_

#include "base/macros.h"
#include "ion/base/readwritelock.h"
#include "ion/base/spinmutex.h"
#include "ion/port/mutex.h"

namespace ion {
namespace base {

// This file contains utility classes for automatically locking and unlocking
// mutexes.

// Base class of guards that lock a mutex when created and unlock when
// destroyed.
template <class MutexT>
class ION_API GenericLockGuardBase {
 public:
  // Returns whether this guard has locked the mutex; returns false even if
  // another guard has it locked. Use MutexT::IsLocked() to determine if
  // the mutex itself is locked.
  bool IsLocked() const { return is_locked_; }

  // Locks the mutex if it is not already locked by this guard. This function
  // blocks if the mutex is locked elsewhere (e.g., by another guard).
  void Lock() {
    if (!is_locked_) {
      mutex_.Lock();
      is_locked_ = true;
    }
  }

  // Attempts to lock the mutex if the mutex is not already locked by this
  // guard. This function never blocks. Returns true if the mutex was
  // successfully locked or if this guard has already locked it, and false
  // otherwise.
  bool TryLock() {
    if (!is_locked_) {
      is_locked_ = mutex_.TryLock();
    }
    return is_locked_;
  }

  // Releases a lock on the mutex if it was previously locked by this guard.
  void Unlock() {
    if (is_locked_) {
      mutex_.Unlock();
      is_locked_ = false;
    }
  }

 protected:
  // The constructor is protected since this is an abstract base class.
  explicit GenericLockGuardBase(MutexT* m) : mutex_(*m), is_locked_(false) {}

  // This destructor intentionally non-virtual for speed.
  // Any subclasses should also call Unlock() in their destructors.
  ~GenericLockGuardBase() { Unlock(); }

  // The mutex used for locking.
  MutexT& mutex_;
  // Whether the mutex is currently locked by this guard.
  bool is_locked_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GenericLockGuardBase);
};

// A LockGuard locks a mutex when created, and unlocks it when destroyed. The
// LockGuard constructor may block waiting to acquire the mutex lock.
template <class MutexT>
class ION_API GenericLockGuard : public GenericLockGuardBase<MutexT> {
 public:
  // The passed pointer must be non-NULL.
  explicit GenericLockGuard(MutexT* m) : GenericLockGuardBase<MutexT>(m) {
    this->mutex_.Lock();
    this->is_locked_ = true;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GenericLockGuard);
};

// A TryLockGuard attempts to lock a mutex when created, and if successful,
// will unlock it when destroyed. Use IsLocked() to determine if the initial
// lock was successful. A TryLockGuard never blocks.
template <class MutexT>
class ION_API GenericTryLockGuard : public GenericLockGuardBase<MutexT> {
 public:
  // The passed pointer must be non-NULL.
  explicit GenericTryLockGuard(MutexT* m) : GenericLockGuardBase<MutexT>(m) {
    this->TryLock();
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GenericTryLockGuard);
};

// An UnlockGuard is the reverse of a LockGuard; it unlocks a mutex when created
// and locks it when destroyed. Note that the destructor of an UnlockGuard may
// block waiting to acquire the mutex lock.
template <class MutexT>
class ION_API GenericUnlockGuard {
 public:
  // The passed pointer must be non-NULL.
  explicit GenericUnlockGuard(MutexT* m) : mutex_(*m) {
    mutex_.Unlock();
  }

  ~GenericUnlockGuard() {
    mutex_.Lock();
  }

 private:
  MutexT& mutex_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GenericUnlockGuard);
};

// A ManualLockGuard can be used to protect a variable with a mutex in
// situations where it is not possible for scoping to be used. The mutex is
// built into the class.
template <typename T>
class ION_API ManualLockGuard : public GenericLockGuardBase<port::Mutex> {
 public:
  // The constructor is passed the initial value of the variable being
  // protected.
  explicit ManualLockGuard(const T& initial_value)
      : GenericLockGuardBase<port::Mutex>(&local_mutex_),
        initial_value_(initial_value),
        current_value_(initial_value) {
    is_locked_ = false;
  }

  // The destructor unlocks the mutex if necessary.
  // This destructor is intentionally non-virtual for speed.  It duplicates what
  // the base class destructor does.
  ~ManualLockGuard() { Unlock(); }

  // Sets the variable's value and locks the mutex. This blocks while the mutex
  // is locked.
  void SetAndLock(const T& new_value) {
    Lock();
    current_value_ = new_value;
  }

  // Resets the variable's value to the initial value (passed to the
  // constructor), unlocks the mutex, and returns the previous value.
  const T ResetAndUnlock() {
    const T ret = current_value_;
    current_value_ = initial_value_;
    Unlock();
    return ret;
  }

  // Returns the current value of the variable. This blocks if the mutex is
  // already locked.
  const T GetCurrentValue() {
    Lock();
    T ret = current_value_;
    Unlock();
    return ret;
  }

 private:
  port::Mutex local_mutex_;
  T initial_value_;
  T current_value_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ManualLockGuard);
};

// Convenient typedefs for ion::port::Mutex.
typedef GenericLockGuard<port::Mutex> LockGuard;
typedef GenericUnlockGuard<port::Mutex> UnlockGuard;
typedef GenericTryLockGuard<port::Mutex> TryLockGuard;

// Convenient typedefs for SpinMutex.
typedef GenericLockGuard<SpinMutex> SpinLockGuard;
typedef GenericUnlockGuard<SpinMutex> SpinUnlockGuard;
typedef GenericTryLockGuard<SpinMutex> SpinTryLockGuard;

// Convenient typedefs for ReadWriteLock.
typedef GenericLockGuard<ReadLock> ReadGuard;
typedef GenericLockGuard<WriteLock> WriteGuard;

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_LOCKGUARDS_H_
