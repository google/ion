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

#ifndef ION_BASE_ONCE_H_
#define ION_BASE_ONCE_H_

#include <functional>
#include <mutex>  // NOLINT(build/c++11)

#include "base/integral_types.h"
#include "base/macros.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/port/atomic.h"
#include "ion/port/macros.h"

namespace ion {
namespace base {

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
    std::call_once(flag_, &Lazy::Populate, this);
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
  mutable std::once_flag flag_;
  ION_DISALLOW_ASSIGN_ONLY(Lazy<T>);
};

inline void CallChecked(const std::function<bool()>& target) {
  if (!target()) {
    LOG(ERROR) << "CallOnce target returned false.";
  }
}

}  // namespace base
}  // namespace ion

// Executes a given static bool() function exactly once.
// Logs an error if the function returns false.
#define ION_STATIC_ONCE_CHECKED(function) \
  do { \
    static std::once_flag flag; \
    std::call_once(flag, ::ion::base::CallChecked, function); \
  } while (0)

// Executes a given static void() or T() function exactly once.
// Any return type is ignored.
#define ION_STATIC_ONCE(function) \
  do { \
    static std::once_flag flag; \
    std::call_once(flag, function); \
  } while (0)

#endif  // ION_BASE_ONCE_H_
