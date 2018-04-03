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

#ifndef ION_BASE_SHAREDPTR_H_
#define ION_BASE_SHAREDPTR_H_

#include <algorithm>

#include "base/macros.h"
#include "ion/base/logging.h"
#include "ion/base/shareable.h"

namespace ion {
namespace base {

// A SharedPtr is a smart shared pointer to an instance of some class that
// implements reference counting. The class passed as the template parameter
// must support the following interface:
//
//   void IncrementRefCount() const;
//   void DecrementRefCount() const;
//
// The DecrementRefCount() function should typically delete the instance when
// the count goes to zero.
//
// Note that the Shareable class supports exactly this interface, so any class
// derived from Shareable can be used automatically with SharedPtr.
//
// If the type instantiating the SharedPtr is incomplete, only the following
// operations are supported:
// - Default construction.
// - Copy construction from a SharedPtr of the same type.
// - Move construction from a SharedPtr of the same type.
// - Move construction from a SharedPtr of a compatible type.
// - Destruction.
// - Get().
// - Reset() without arguments.
// - Assignment from a SharedPtr of the same type.
// - Move assignment from a SharedPtr of the same type.
// - Move assignment from a SharedPtr of a compatible type.
// - Operator* and Operator-> (although the latter is unlikely to be useful).
// - Operator== and Operator!=.
// - Operator bool().
// - swap().
// The remaining operators require a complete type:
// - Construction from a T*.
// - Copy construction requiring a type conversion.
// - Reset(args) with any arguments, even nullptr.
// - Assignment from a T*.
// - Assignment from a SharedPtr requiring a type conversion.
template <typename T> class SharedPtr {
  // The following declaration needed for move ctor/assignment to work, this is
  // the way it's done in std::shared_ptr as well.
  template<typename U> friend class SharedPtr;

 public:
  // The default constructor initializes the pointer to nullptr.
  SharedPtr() : ptr_(nullptr), shr_(nullptr) {}

  // Constructor that takes a raw shared pointer.
  explicit SharedPtr(T* shared) : ptr_(shared), shr_(shared) {
    AddReference();
  }

  // Constructor that allows sharing of a pointer to a type that is compatible
  // with T*.
  template <typename U> SharedPtr(const SharedPtr<U>& p)
      : ptr_(p.Get()), shr_(p.Get()) {
    AddReference();
  }

  // The copy constructor shares the instance from the other pointer.
  SharedPtr(const SharedPtr<T>& p) : ptr_(p.Get()), shr_(p.shr_) {
    AddReference();
  }

  ~SharedPtr() { Reset(); }

  // Returns a raw pointer to the instance, which may be nullptr.
  T* Get() const { return ptr_; }

  // Changes the pointer to point to the given shared, which may be nullptr.
  void Reset(T* new_shared) {
    if (new_shared != ptr_) {
      const Shareable* shr = shr_;
      ptr_ = new_shared;
      shr_ = new_shared;
      AddReference();
      // Call RemoveReference() last in case it deletes memory containing this.
      RemoveReference(shr);
    }
  }

  // Make the SharedPtr point to nullptr.
  void Reset() {
    const Shareable* shr = shr_;
    ptr_ = nullptr;
    shr_ = nullptr;
    // Call RemoveReference() last in case it deletes memory containing this.
    RemoveReference(shr);
  }

  // Assignment to a raw pointer is the same as Reset().
  SharedPtr<T>& operator=(T* new_shared) {
    Reset(new_shared);
    return *this;
  }

  // Allow assignment to a compatible raw pointer.
  template <typename U> SharedPtr<T>& operator=(U* new_shared) {
    Reset(static_cast<T*>(new_shared));
    return *this;
  }

  // Allow assignment to a SharedPtr of the same type.
  // Don't delegate to Reset() here because Reset() requires a complete type.
  // We can do this version without that requirement.
  SharedPtr<T>& operator=(const SharedPtr<T>& p) {
    if (p.Get() != ptr_) {
      const Shareable* shr = shr_;
      ptr_ = p.Get();
      shr_ = p.shr_;
      AddReference();
      // Call RemoveReference() last in case it deletes memory containing this.
      RemoveReference(shr);
    }
    return *this;
  }

  // Allow assignment to a compatible SharedPtr.
  template <typename U> SharedPtr<T>& operator=(const SharedPtr<U>& p) {
    Reset(p.Get());
    return *this;
  }

#if !defined(ION_TRACK_SHAREABLE_REFERENCES)
  // Move assignment. Don't call Reset here because it increments ref counter of
  // the object pointed to by `other', and we don't want that: the ref counter
  // of that object must stay exactly the same.
  template <typename U, typename = typename std::enable_if<
                            std::is_base_of<T, U>::value>::type>
  SharedPtr& operator=(SharedPtr<U>&& other) {
    const Shareable* shr = shr_;

    // Simply assign ptrs, no ref count manipulation necessary.
    ptr_ = other.ptr_;
    shr_ = other.shr_;
    other.ptr_ = nullptr;
    other.shr_ = nullptr;

    // If this shared pointer was previously referring to something, remove
    // that reference. Otherwise, this is a no-op.
    RemoveReference(shr);

    return *this;
  }

  // This is technically a style violation, but we do want implicit move
  // construction from SharedPtrs to derived types, to match the behavior of raw
  // pointers and std::shared_ptr.
  template <typename U, typename = typename std::enable_if<
                            std::is_base_of<T, U>::value>::type>
  SharedPtr(SharedPtr<U>&& other) : ptr_(nullptr), shr_(nullptr) {  // NOLINT
    *this = std::move(other);
  }
#endif

  // The -> operator forwards to the raw pointer.
  T* operator->() const {
    DCHECK(ptr_);
    return ptr_;
  }

  // The * operator returns the underlying instance.
  T& operator*() const {
    DCHECK(ptr_);
    return *ptr_;
  }

  // Check that the pointer is non-null.
  explicit operator bool() const { return ptr_ != nullptr; }

  // The equality operator returns true if the raw pointers are the same.
  bool operator==(const SharedPtr<T>& p) const {
    return p.ptr_ == ptr_;
  }

  // The inequality operator returns true if the raw pointers differ.
  bool operator!=(const SharedPtr<T>& p) const {
    return p.ptr_ != ptr_;
  }

  // STL-style function to swap the raw pointer with another SharedPtr
  // without the need for copying.
  void swap(SharedPtr<T>& p) {
    std::swap(ptr_, p.ptr_);
    std::swap(shr_, p.shr_);

#if defined(ION_TRACK_SHAREABLE_REFERENCES)
    // Needed so that stack-traces in the Shareable are correct:
    if (shr_) {
      shr_->IncrementRefCount(this);
      shr_->DecrementRefCount(&p);
    }
    if (p.shr_) {
      p.shr_->IncrementRefCount(&p);
      p.shr_->DecrementRefCount(this);
    }
#endif
  }

 private:
  // Increments the reference count in the instance if it is not null.
  void AddReference() {
    if (shr_)
      shr_->IncrementRefCount(this);
  }

  // Decrements the reference count in the instance, if it is not null.
  void RemoveReference(const Shareable* shr) {
    if (shr)
      shr->DecrementRefCount(this);
  }

  T* ptr_;
  // shr_ points into the same object as ptr_, but is not necessarily the same
  // address in the multiple-inheritance case. We store it separately so that
  // T's complete type is not needed except when it is first assigned into the
  // SharedPtr.
  const Shareable* shr_;
};

#if !ION_NO_RTTI
// Allows casting SharedPtrs down a type hierarchy.
template <typename To, typename From>
SharedPtr<To> DynamicPtrCast(const SharedPtr<From>& orig) {
  return SharedPtr<To>(dynamic_cast<To*>(orig.Get()));
}
#endif

// A similar StaticPtrCast could be written, but it is not clear
// if this would ever make sense for intrusive reference counting.
// Static_casts are typically for conversions between related, but
// distinct, objects. Such objects probably shouldn't be sharing
// a reference count. For now, StaticPtrCast will be omitted.

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SHAREDPTR_H_
