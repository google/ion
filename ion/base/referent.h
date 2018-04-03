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

#ifndef ION_BASE_REFERENT_H_
#define ION_BASE_REFERENT_H_

#include <algorithm>

#include "base/macros.h"
#include "ion/base/allocatable.h"
#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/shareable.h"
#include "ion/base/sharedptr.h"
#include "ion/port/atomic.h"

namespace ion {
namespace base {

// Thread-safe abstract base class. Anything derived from Referent can be stored
// in a ReferentPtr for shared ownership.  The ReferentPtr class manages the
// reference count.
//
// Referent is derived from Allocatable, so memory can be managed for any
// instance by passing an Allocator to operator new.
//
// Copying of instances of classes derived from Referent is not allowed
// because reference-counted objects should not need to be copied. A derived
// class can implement a function to create and return a copy in special
// circumstances.
//
// Note: All derived classes should declare their destructor as protected or
// private so that they cannot be created on the stack (which could easily
// result in double deletions).
class Referent : public Allocatable, public Shareable {
 protected:
  Referent() {}
  // Constructor for Referents which can live on the stack or other
  // non-Allocator-supplied memory.
  explicit Referent(const AllocatorPtr& allocator) : Allocatable(allocator) {}
  ~Referent() override {}
};

// A ReferentPtr is a smart shared pointer to an instance of some class derived
// from Referent. It manages the reference count so that the instance is deleted
// when the last pointer to it goes away.  Since we can't use template
// typedefs yet, this struct is a workaround.
template <typename T> struct ReferentPtr {
  typedef SharedPtr<T> Type;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_REFERENT_H_
