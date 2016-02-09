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

#ifndef ION_BASE_ONCE_H_
#define ION_BASE_ONCE_H_

#include <functional>

#include "base/integral_types.h"
#include "base/macros.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/port/atomic.h"
#include "ion/port/macros.h"
#include "ion/port/threadutils.h"

namespace ion {
namespace base {

// OnceFlag ensures that a target function is only evaluated once.
// TODO(user): Reimplement in terms of std::call_once once c++11
// is available on all platforms, and/or deprecate.  Note that even once it
// is available, caution may be advised as some platform implementations
// may be very bad...
class ION_API OnceFlag {
 public:
  OnceFlag() : value_(init_value_) {}

  // If this OnceFlag instance has never executed a target function, CallOnce
  // calls the target function. Thread safe, and all side effects of a call to
  // a target function are guaranteed to be visible when the call returns. This
  // call is not exception safe - if a target throws or the thread terminates
  // this instance becomes invalid.
  void CallOnce(const std::function<void()>& target);

  static void CallChecked(const std::function<bool()>& target) {
    if (!target()) {
      LOG(ERROR) << "CallOnce target returned false.";
    }
  }

 private:
  std::atomic<int32> value_;
  static const int32 init_value_ = 0;
  DISALLOW_COPY_AND_ASSIGN(OnceFlag);
};

// Lazily populates a value. Supports copy construct for use in resizable
// containers. Note that the creator function will be called once for each copy
// even if the value has been evaluated, so container resizing should be avoided
// once values have started to be populated.
template <typename T> class Lazy {
 public:
  explicit Lazy(const std::function<T()>& creator) : creator_(creator) {}

  // Copy constructor, only copies the creator.
  Lazy(const Lazy<T>& other) : creator_(other.creator_) {}
  const T& Get() const {
    flag_.CallOnce(std::bind(&Lazy::Populate, this));
    return value_;
  }

 private:
  void Populate() const { value_ = creator_(); }

  // Function to create a T value to use for lazy population.
  std::function<T()> creator_;

  // Value which is to be populated using lazy evaluation.  Mutable due to the
  // lazy evaulation.
  mutable T value_;

  // Once flag to ensure Populate is only called once.  Mutable due to the lazy
  // evaluation changing the flag value to mark it as completed.
  mutable OnceFlag flag_;
  ION_DISALLOW_ASSIGN_ONLY(Lazy<T>);
};

// Inlines.

inline void OnceFlag::CallOnce(const std::function<void()>& target) {
  // Some random values which should have low probability of being found by
  // accident.
  const int32 running_value = 0x325ad493;
  const int32 done_value = 0x46f36511;
  // Shortcut path, if already done only need an acquire to ensure that
  // all writes done in the target() call by any thread are visible.
  if (value_.load(std::memory_order_acquire) == done_value) {
    return;
  }
  int32 expected_value = init_value_;
  if (value_.compare_exchange_strong(expected_value, running_value)) {
    target();
    // Storing done is a release, so any other threads doing an acquire and
    // finding done_value can be safe knowing that side effects of the target()
    // call are available.
    value_ = done_value;
  } else if (expected_value != done_value) {
    // Another thread is running, spin.
    // TODO(user): Optimize for case where target function evaluates
    // in less time than it takes to yield.  Also optimize for case where
    // it takes a very long time...
    // These atomic reads must be at least an acquire.
    while (value_ != done_value) {
      port::YieldThread();
    }
  }
}

}  // namespace base
}  // namespace ion

// Executes a given static bool() function exactly once.
// Logs an error if the function returns false.
#define ION_STATIC_ONCE_CHECKED(function) \
    ION_STATIC_ONCE(std::bind(::ion::base::OnceFlag::CallChecked, &function));

// Executes a given static void() or T() function exactly once.
// Any return type is ignored.
#define ION_STATIC_ONCE(function) \
  do { \
    ION_DECLARE_SAFE_STATIC_POINTER(::ion::base::OnceFlag, s_once_flag_macro); \
    s_once_flag_macro->CallOnce(function); \
  } while (0)

#endif  // ION_BASE_ONCE_H_
