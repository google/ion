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

#ifndef ION_BASE_WEAKREFERENT_H_
#define ION_BASE_WEAKREFERENT_H_

#include <algorithm>

#include "base/macros.h"
#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/referent.h"
#include "ion/base/sharedptr.h"
#include "ion/port/atomic.h"

namespace ion {
namespace base {

// Abstract base class that inherits from Referent, and adds the ability
// for instances to be referenced by a WeakReferentPtr.  Any WeakReferentPtr
// will Acquire only nullptr after the last ReferentPtr (a.k.a. "strong
// reference") to the instance goes away.
//
// All Referent usage guidelines (regarding copying, private/protected
// destructors, etc.) also apply to WeakReferent.
class WeakReferent : public Referent {
 protected:
  WeakReferent() : proxy_(nullptr) {}

  // The destructor is protected to disallow creating instances on the stack.
  ~WeakReferent() override {
    DCHECK(proxy_.load() == nullptr);
  }

 private:
  // A Proxy accesses the strong reference count for a WeakReferent, and also
  // has a reference count for itself. When the reference count of the held
  // WeakReferent goes from 1 to 0, it notifies the proxy that it has been
  // orphaned through SetOrphaned(). This sets the proxy's pointer to nullptr,
  // so any WeakReferentPtrs that hold the proxy will be notified that the
  // instance is no longer available. The WeakReferent pointer is actually
  // deleted by the WeakReferent, below. Proxy contains a reference count is so
  // that it will be deleted when all references to it (both from
  // WeakReferentPtrs and from Referents) are destroyed.
  class Proxy : public Allocatable, public Shareable {
   public:
    explicit Proxy(WeakReferent* ptr) : ptr_(ptr) { DCHECK(ptr_); }

    // Gets the underlying WeakReferent.
    WeakReferent* Get() const {
      DCHECK(mutex_.IsLocked());
      return ptr_;
    }

    // Gets the underlying WeakReferent without locking a mutex.
    WeakReferent* GetUnsynchronized() const {
      return ptr_;
    }

    // Sets the pointer to nullptr, as all strong references (SharedPtrs) have
    // gone away.
    void SetOrphaned() {
      DCHECK(mutex_.IsLocked());
      ptr_ = nullptr;
    }

    // Mutex which must be locked while calling Get() or SetOrphaned().
    SpinMutex* GetMutex() const { return &mutex_; }

   private:
    // The actual WeakReferent.
    WeakReferent* ptr_;

    // Used to synchronize access to the Proxy during WeakReferentPtr::Acquire()
    // and Referent::DecrementRefCount().
    mutable SpinMutex mutex_;
  };

  // Return a lazily-created Proxy.
  Proxy* GetProxy() const {
    // If there is already a proxy, use it.
    Proxy* proxy = proxy_.load(std::memory_order_acquire);
    if (proxy) {
      return proxy;
    }

    // Instantiate a new Proxy, and try to atomically swap it into proxy_.
    // This can fail if someone else does it first.
    proxy = new(GetAllocator()) Proxy(const_cast<WeakReferent*>(this));
    // Because we can't do the atomic CAS below directly into a SharedPtr,
    // we instead manage the ref-count of the Proxy manually (however, each
    // WeakReferentPtr uses a SharedPtr as usual).
    proxy->IncrementRefCount(this);
    Proxy* null = nullptr;
    if (proxy_.compare_exchange_strong(null, proxy)) {
      // Success!  Return the newly-instantiated Proxy.
      DCHECK(proxy == proxy_);
      return proxy;
    } else {
#if !defined(ION_COVERAGE)  // COV_NF_START
      // Couldn't swap in a new Proxy because someone on a different thread beat
      // us to it, so use theirs and destroy the one we created.  Since the
      // Proxy refs itself in the constructor, we unref it to destroy it.
      proxy->DecrementRefCount(this);
      proxy = proxy_;
      // Since our atomic CAS is a full memory-barrier...
      DCHECK(proxy_.load() != nullptr) << "proxy should not be NULL after CAS";
      return proxy;
#endif  // COV_NF_END
    }
  }

  // Reference counting interface. DecrementRefCount is different enough from
  // the Shareable implementation that we don't call back to it, just override.
  // Note that this function is private because only SharedPtr
  // and WeakReferentPtr should modify the reference count.
  void OnZeroRefCount() const override {
    // No more strong references to this WeakReferent exist, so we can go ahead
    // and destroy it.  However, if there are any WeakReferentPtrs that refer
    // to it, we must take additional care.
    Proxy* proxy = proxy_.exchange(nullptr, std::memory_order_acquire);
    if (proxy) {
      // The test shows that proxy_ was not null, which implies that there may
      // exist WeakReferentPtrs to us.
      // Also see WeakReferentPtr::Acquire(), which guarantees that no SharedPtr
      // can be acquired once the ref-count reaches zero.  Therefore, it's safe
      // for us to destroy ourself, unless someone deliberately tries to break
      // things, such as by creating a SharedPtr from a raw pointer to this
      // WeakReferent.
      {
        SpinLockGuard proxy_guard(proxy->GetMutex());
        // It should not be possible for the proxy to already be orphaned.
        DCHECK(proxy->Get() != nullptr) << "SetOrphaned() already called.";
        // Ref-count must be zero while we hold the mutex.
        DCHECK_EQ(ref_count_.load(), 0);
        proxy->SetOrphaned();
      }
      proxy->DecrementRefCount(this);
    }
    delete this;
  }

  // Make this available to Referent's friends.
  std::atomic<int>& GetRefCountRef() { return ref_count_; }

  // A wrapper around the proxy for weak references.
  mutable std::atomic<Proxy*> proxy_;

  // Allow ReferentPtr (via SharedPtr) and WeakReferentPtr to modify the
  // reference count.
  template <typename T> friend class SharedPtr;
  template <typename T> friend class WeakReferentPtr;

  DISALLOW_COPY_AND_ASSIGN(WeakReferent);
};

// A WeakReferentPtr is a weak reference to an instance of some class derived
// from Referent. It returns a ReferentPtr instance through the Acquire() call,
// which is the only way to access the underlying instance. This ReferentPtr
// will only be valid (non-null) if there are valid ReferentPtrs that still
// point to the underlying instance. Note that a WeakReferentPtr requires a
// ReferentPtr to the instance to already exist; WeakReferentPtrs are useless on
// their own, and will free any non-ref'd pointers passed to them (see Reset(),
// below).
//
// Like SharedPtr, WeakReferentPtr is not thread-safe; synchronization must be
// used to ensure that an instance can only be accessed by one thread at a time.
template <typename T>
class WeakReferentPtr {
 public:
  using ReferentPtrType = SharedPtr<T>;

  WeakReferentPtr() { Reset(nullptr); }
  explicit WeakReferentPtr(T* ptr) { Reset(ptr); }
  explicit WeakReferentPtr(const ReferentPtrType& ref_ptr) {
    Reset(ref_ptr.Get());
  }
  // Copy constructor.
  WeakReferentPtr(const WeakReferentPtr& other)
      : proxy_(other.proxy_) {}

  // Attempts to construct a ReferentPtr of the reference. This is required to
  // perform operations on the pointer. The construction is successful only if
  // there are already existing ReferentPtrs to the instance. If successful, the
  // returned ReferentPtr will have the normal lifetime guarantees. If it fails,
  // it returns a null ReferentPtr, meaning that the Referent has already been
  // deleted.
  ReferentPtrType Acquire() const {
    // This will be returned at the end after the code below sets it or doesn't.
    ReferentPtrType result;

    // We may not have a proxy, and if we do, that proxy may be orphaned.
    if (proxy_.Get()) {
      // We have a proxy; now we need to determine whether it is orphaned.
      // This guard ensures that we don't create a strong SharedPtr to a
      // Referent that is about to be destroyed.
      SpinLockGuard proxy_guard(proxy_->GetMutex());
      // Attempt to get the Referent from the Proxy; it will be null if the
      // Proxy has already been orphaned.
      WeakReferent* referent = proxy_->Get();
      if (!referent) {
        // The proxy was already orphaned, so acquisition fails.
      } else {
        // The proxy isn't yet orphaned (and it can't be until we release the
        // mutex), but we still need to check if the ref-count is zero... if so,
        // we infer that the Referent has started to destroy itself in
        // DecrementRefCount(), so we'll return nullptr and allow it to finish.
        //
        // Note: the following reasoning proves that the Referent hasn't
        // finished destroying itself...
        //   - we hold the mutex
        //   - the Referent can't finish without grabbing the mutex
        //   - the Referent hasn't grabbed it yet; if it had then the Proxy
        //     would already have been orphaned, but we already checked for
        //     that above.

        // Increment the ref-count directly to atomically obtain the old value,
        // and also to prevent the Referent from being destroyed (or more
        // precisely, to establish agreement with the Referent about whether
        // it is to be destroyed or not).
        if (referent->GetRefCountRef()++) {
          // There are other references to the Referent, so it isn't in the
          // process of being destroyed.  Assign the pointer that will be
          // returned.
          result = referent;
        }
        // Regardless of whether we successfully acquired the Referent, we must
        // balance the ref-count.
        --referent->GetRefCountRef();
      }
    }

    return result;
  }

  // Set this WeakPointer to not refer to anything.
  void Reset() {
    Reset(nullptr);
  }

  // Allow assignment from a SharedPtr of the same type.
  WeakReferentPtr<T>& operator=(const ReferentPtrType& p) {
    Reset(p.Get());
    return *this;
  }

  // The equality operator returns true if the proxies are the same.
  bool operator==(const WeakReferentPtr& p) const {
    return p.proxy_ == proxy_;
  }

  // The inequality operator returns true if the proxies differ.
  bool operator!=(const WeakReferentPtr& p) const {
    return p.proxy_ != proxy_;
  }

  // This function returns the reference count of the underlying referent that
  // points to. This function performs no synchronization, and should only be
  // considered accurate when called from the underlying referent's destructor.
  int GetUnderlyingRefCountUnsynchronized() const {
    if (WeakReferent::Proxy* proxy = proxy_.Get()) {
      if (WeakReferent* referent = proxy->GetUnsynchronized()) {
        return referent->GetRefCount();
      }
    }
    return 0;
  }

 private:
  // Changes the pointer to point to the given referent, which may be null.
  void Reset(T* new_referent) {
    // Potentially free previous value.
    proxy_ = nullptr;
    if (new_referent) {
      if (new_referent->GetRefCount() == 0)
        LOG(ERROR) << "Input pointer was not owned by a ReferentPtr and will "
                   << "be deleted";
      ReferentPtrType ref_p(new_referent);
      proxy_ = ref_p->GetProxy();
    }
  }

  mutable SharedPtr<WeakReferent::Proxy> proxy_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_WEAKREFERENT_H_
