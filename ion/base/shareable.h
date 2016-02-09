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

#ifndef ION_BASE_SHAREABLE_H_
#define ION_BASE_SHAREABLE_H_

#include "base/macros.h"
#include "ion/base/logging.h"
#include "ion/port/atomic.h"

namespace ion {
namespace base {

// Shareable is an abstract base class for any object that can be shared via
// the SharedPtr class. It supports the reference counting interface required
// by SharedPtr.
class Shareable {
 public:
  // GetRefCount() is part of the interface necessary for SharedPtr.
  int GetRefCount() const { return ref_count_; }

 protected:
  Shareable(): ref_count_(0) {}

  // The destructor is protected because all instances should be managed
  // through SharedPtr.
  virtual ~Shareable() { DCHECK_EQ(ref_count_.load(), 0); }

 private:
  // These are part of the interface necessary for SharedPtr. They are private
  // to prevent anyone messing with the reference count.
  void IncrementRefCount() const { ++ref_count_; }
  void DecrementRefCount() const {
    const int new_count = --ref_count_;
    DCHECK_GE(new_count, 0);
    if (new_count == 0) {
      OnZeroRefCount();
    }
  }

  virtual void OnZeroRefCount() const { delete this; }

  // The reference count is atomic to provide thread safety. It is mutable so
  // that const instances can be managed.
  mutable std::atomic<int> ref_count_;

  // Allow SharedPtr to modify the reference count.
  template <typename T> friend class SharedPtr;
  friend class WeakReferent;

  DISALLOW_COPY_AND_ASSIGN(Shareable);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SHAREABLE_H_
