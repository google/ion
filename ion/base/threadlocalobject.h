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

#ifndef ION_BASE_THREADLOCALOBJECT_H_
#define ION_BASE_THREADLOCALOBJECT_H_

#include <mutex>  // NOLINT(build/c++11)
#include <vector>

#include "ion/base/allocatable.h"
#include "ion/base/allocator.h"
#include "ion/base/lockguards.h"
#include "ion/base/type_structs.h"
#include "ion/port/threadutils.h"

namespace ion {
namespace base {

// This templated class makes it easy to create an instance of an object in
// thread-local storage. It obtains and manages a TLS key that can be used in
// all threads and sets the TLS pointer in each thread to the object using that
// key.
//
// The wrapped object must be default-constructable for this class to compile.
//
// For example, consider a singleton "Manager" class that needs to store a
// unique "Info" instance per thread:
//
//   class Manager {
//    public:
//     // This returns a Info instance that is specific to the current thread.
//     const Info& GetInfo() const { return *tl_.Get(); }
//     ...
//    private:
//     ThreadLocalObject<Info> tl_;
//   };
//
//
// This example shows how a non-singleton "Thing" class can use a static
// ThreadLocalObject to create a different singleton "Thing::Helper" class
// instance per thread.
//
//   class Thing {
//     ...
//    private:
//     // This returns a static Helper instance that is specific to the current
//     // thread.
//     Helper* GetHelper();
//   };
//
//   // In the source file:
//   Thing::Helper* Thing::GetHelper() {
//     ION_DECLARE_SAFE_STATIC_POINTER(ThreadLocalObject<Helper>, s_helper);
//     return s_helper->Get();
//   }
//
template <typename T> class ThreadLocalObject {
 public:
  // The default constructor will use global operator new() to construct T
  // instances.
  ThreadLocalObject()
      : key_(port::CreateThreadLocalStorageKey()) {}

  // This constructor uses the given Allocator to construct T instances. This
  // will compile only if T is derived from Allocatable.
  explicit ThreadLocalObject(const AllocatorPtr& allocator)
      : key_(port::CreateThreadLocalStorageKey()),
        allocator_(allocator) {}

  ~ThreadLocalObject() {
    // Destroy all T instances created by this.
    DestroyAllInstances();
    // Delete the key, which also invalidates all thread-local storage pointers
    // associated with it.
    port::DeleteThreadLocalStorageKey(key_);
    // Resetting the key_ instance variable in case the
    // ThreadLocalObject is used after it has been deallocated.
    key_ = port::kInvalidThreadLocalStorageKey;
  }

  // Returns the ThreadLocalStorageKey created by the instance. This will be
  // port::kInvalidThreadLocalStorageKey if anything went wrong.
  const port::ThreadLocalStorageKey& GetKey() const { return key_; }

  // Returns a T instance for the current thread, creating it first if
  // necessary. All subsequent calls on the same thread will return the same
  // instance.
  T* Get() {
    if (void* ptr = port::GetThreadLocalStorage(key_))
      return static_cast<T*>(ptr);
    else
      return CreateAndStoreInstance();
  }

 private:
  // Helper struct used to allocate a T instance. This is partially specialized
  // for classes derived from Allocatable to use the Allocator.
  template <typename InstanceType, bool IsAllocatable = false>
  struct AllocationHelper {
    static InstanceType* Allocate(const AllocatorPtr& allocator) {
      return new InstanceType();
    }
  };
  template <typename InstanceType> struct AllocationHelper<InstanceType, true> {
    static InstanceType* Allocate(const AllocatorPtr& allocator) {
      return new(allocator) InstanceType();
    }
  };

  // Creates an instance of a T with the default constructor and puts it in
  // thread-local storage. Returns NULL if the ThreadLocalStorageKey is invalid.
  T* CreateAndStoreInstance() {
    T* instance = nullptr;
    if (key_ != port::kInvalidThreadLocalStorageKey) {
      instance = AllocateInstance(allocator_);
      port::SetThreadLocalStorage(key_, instance);
      std::lock_guard<std::mutex> guard(mutex_);
      instances_.push_back(instance);
    }
    return instance;
  }

  // Allocates an instance of a T using the Allocator only if T is derived from
  // Allocatable.
  static T* AllocateInstance(const AllocatorPtr& allocator) {
    typedef AllocationHelper<T, IsBaseOf<Allocatable, T>::value> HelperType;
    return HelperType::Allocate(allocator);
  }

  // Destroys all T instances created by this.
  void DestroyAllInstances() {
    std::lock_guard<std::mutex> guard(mutex_);
    const size_t num_instances = instances_.size();
    for (size_t i = 0; i < num_instances; ++i)
      delete instances_[i];
    instances_.clear();
  }

  // Key used to associate the storage with all threads.
  port::ThreadLocalStorageKey key_;
  // Allocator used to create instances (if T is derived from Allocatable).
  AllocatorPtr allocator_;
  // Vector of all T instances created by this.
  std::vector<T*> instances_;
  // Mutex protecting the instances_ vector.
  std::mutex mutex_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_THREADLOCALOBJECT_H_
