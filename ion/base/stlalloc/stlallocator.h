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

#ifndef ION_BASE_STLALLOC_STLALLOCATOR_H_
#define ION_BASE_STLALLOC_STLALLOCATOR_H_

#include <memory>
#include <type_traits>

#include "ion/base/allocatable.h"
#include "ion/base/allocator.h"
#include "ion/base/type_structs.h"
#include "ion/port/align.h"

namespace ion {
namespace base {

// StlAllocator is derived std::allocator class that allows an Ion Allocator to
// be used for STL containers. There are derived container classes (such as
// AllocVector and AllocMap) that make it easier to use this allocator.
template <typename T> class StlAllocator : public std::allocator<T> {
 public:
  typedef typename std::allocator<T>::pointer Pointer;
  typedef typename std::allocator<T>::const_pointer ConstPointer;
  typedef typename std::allocator<T>::size_type SizeType;

  // These allocator traits affect the behavior of containers such as
  // AllocVector. During an assignment or swap, the StlAllocators will be
  // swapped along with the container's memory.
  typedef std::true_type propagate_on_container_move_assignment;
  typedef std::true_type propagate_on_container_swap;

  explicit StlAllocator(const AllocatorPtr& allocator)
      : allocator_(allocator) {}
  // This copy constructor cannot be explicit because older versions of STL
  // seem to implicitly cast between allocators of different types.
  template <typename U> StlAllocator(const StlAllocator<U>& a)
      : allocator_(a.allocator_) {}

  StlAllocator(const StlAllocator&) = default;
  StlAllocator(StlAllocator&& other) { *this = std::move(other); }
  StlAllocator& operator=(StlAllocator&& other) {
    // NOTE: we _deliberately_ don't move from the other allocator's pointer.
    // We want the other StlAllocator to still have its underlying Allocator
    // available.
    // This is to address a problem where, when moving from a STL container,
    // the allocators also get moved. This can lead to bad results when the
    // dtor of the moved-from container is invoked, and attempts to free
    // something, using an allocator object that is no longer there.
    allocator_ = other.allocator_;
    return *this;
  }

  // Returns the Allocator used by this.
  const AllocatorPtr& GetAllocator() const { return allocator_; }

  Pointer allocate(SizeType n, ConstPointer hint = nullptr) {
    DCHECK(allocator_.Get());
    void* p = allocator_->AllocateMemory(n * sizeof(T));
    return reinterpret_cast<Pointer>(p);
  }
  void deallocate(Pointer p, SizeType n) {
    allocator_->DeallocateMemory(p);
  }

  // Replace the construct() functions so that we can pass an Allocator to
  // Allocatable types. The construct() functions inform the allocation system
  // that there is a placement new allocation that might be an Allocatable. If
  // the constructed type is trivially destructible, then construct() is
  // forwarded to std::allocator, which just calls placement new(). Any
  // non-trivially-destructible type that is default, copy, or move constructed,
  // and thus might contain an Allocatable, will receive this container's
  // Allocator.
  void construct(Pointer p, const T& val) {
    // Type that is void if T is trivially destructible, and T otherwise.
    // Allocatables are not trivially destructible. This is a local type so that
    // it is only instantiated when the function (as opposed to the class) is
    // instantiated.
    typedef typename std::conditional<HasTrivialDestructor<T>::value, void,
                                      T>::type VoidOrT;
    const VoidOrT* select_overload = nullptr;
    construct_impl(select_overload, p, val);
  }
  template <typename... Args>
  void construct(Pointer p, Args&&... args) {  // NOLINT
    // Type that is void if T is trivially destructible, and T otherwise.
    // Allocatables are not trivially destructible. This is a local type so that
    // it is only instantiated when the function (as opposed to the class) is
    // instantiated.
    typedef typename std::conditional<HasTrivialDestructor<T>::value, void,
                                      T>::type VoidOrT;
    const VoidOrT* select_overload = nullptr;
    construct_impl(select_overload, p, std::forward<Args>(args)...);  // NOLINT
  }
  template <class U, class... Args>
  void construct(U* p, Args&&... args) {  // NOLINT
    // Type that is void if U is trivially destructible, and U otherwise.
    // Allocatables are not trivially destructible. This is a local type so that
    // it
    // is only instantiated when the function (as opposed to the class) is
    // instantiated.
    typedef typename std::conditional<HasTrivialDestructor<U>::value, void,
                                      U>::type VoidOrU;
    const VoidOrU* select_overload = nullptr;
    construct_impl(select_overload, p, std::forward<Args>(args)...);  // NOLINT
  }

  template <typename U>
  struct rebind {
    typedef StlAllocator<U> other;
  };

 private:
  StlAllocator();  // Not default constructible.

  // These just call the default construct function from std::allocator.
  void construct_impl(const void* dummy, Pointer p, const T& val) {
    std::allocator<T>::construct(p, val);
  }
  template <typename... Args>
  void construct_impl(const void* dummy, Pointer p,
                      Args&&... args) {  // NOLINT
    std::allocator<T>::construct(p, std::forward<Args>(args)...);  // NOLINT
  }
  template <class U, class... Args>
  void construct_impl(const void* dummy, U* p, Args&&... args) {  // NOLINT
    std::allocator<T>::construct(p, std::forward<Args>(args)...);  // NOLINT
  }

  // These set allocator_ as the placement Allocator to be used by
  // Allocatable-derived types.
  void construct_impl(const T* dummy, Pointer p, const T& val) {
    Allocatable::SetPlacementAllocator(allocator_.Get());
    std::allocator<T>::construct(p, val);
    Allocatable::SetPlacementAllocator(nullptr);
  }
  template <typename... Args>
  void construct_impl(const T* dummy, Pointer p, Args&&... args) {  // NOLINT
    Allocatable::SetPlacementAllocator(allocator_.Get());
    std::allocator<T>::construct(p, std::forward<Args>(args)...);  // NOLINT
    Allocatable::SetPlacementAllocator(nullptr);
  }
  template <class U, class... Args>
  void construct_impl(const U* dummy, U* p, Args&&... args) {  // NOLINT
    Allocatable::SetPlacementAllocator(allocator_.Get());
    std::allocator<T>::construct(p, std::forward<Args>(args)...);  // NOLINT
    Allocatable::SetPlacementAllocator(nullptr);
  }

  AllocatorPtr allocator_;
  template <typename U> friend class StlAllocator;
};

// StlInlinedAllocator is a derived StlAllocator class that uses inlined storage
// for its first N elements, then uses an Ion Allocator if that size is
// exceeded.
template <typename T, int N>
class StlInlinedAllocator : public StlAllocator<T> {
 public:
  typedef typename StlAllocator<T>::Pointer Pointer;
  typedef typename StlAllocator<T>::ConstPointer ConstPointer;
  typedef typename StlAllocator<T>::SizeType SizeType;
  explicit StlInlinedAllocator(const AllocatorPtr& allocator)
      : StlAllocator<T>(allocator), current_(nullptr), inlined_(true) {}
  // Copy constructor. This needs to be explicitly listed so that inlined
  // storage is not copied.
  explicit StlInlinedAllocator(const StlInlinedAllocator<T, N>& a)
      : StlAllocator<T>(a.GetAllocator()),
        current_(nullptr),
        inlined_(a.inlined_) {}
  // This copy constructor cannot be explicit because older versions of STL
  // seem to implicitly cast between allocators of different types.
  template <typename U>
  StlInlinedAllocator(const StlInlinedAllocator<U, N>& a)
      : StlAllocator<T>(a.GetAllocator()),
        current_(nullptr),
        inlined_(a.inlined_) {}
  Pointer allocate(SizeType n, ConstPointer hint = nullptr) {
    // Return the local storage if we can, otherwise mark this as not inlined
    // and do a normal allocation.
    if (n <= N && inlined_) {
      if (current_ == reinterpret_cast<T*>(&storage_[0]))
        current_ = reinterpret_cast<T*>(&storage_[sizeof(T) * N]);
      else
        current_ = reinterpret_cast<T*>(&storage_[0]);
      return reinterpret_cast<Pointer>(current_);
    } else {
      inlined_ = false;
      return this->StlAllocator<T>::allocate(n, hint);
    }
  }
  void deallocate(Pointer p, SizeType n) {
    // If we are deallocating an allocated pointer then we can reuse the inlined
    // storage.
    if (p != reinterpret_cast<Pointer>(&storage_) &&
        p != reinterpret_cast<Pointer>(&storage_[sizeof(T) * N])) {
      inlined_ = true;
      this->StlAllocator<T>::deallocate(p, n);
    }
  }

  // Windows uses transient proxy allocators. If the other type (U) is not the
  // local type (T), then just use a normal StlAllocator as the allocator.
  // RebindHelper enforces this with a struct specialization.
  template <typename U, typename V>
  struct RebindHelper {
    typedef typename StlAllocator<V>::template rebind<U>::other other;
  };
  // Specialize for when the types are the same, that is, for the primary T
  // allocator.
  template <typename U>
  struct RebindHelper<U, U> {
    typedef StlInlinedAllocator<U, N> other;
  };
  template <typename U> struct rebind {
    typedef typename RebindHelper<U, T>::other other;
  };

 private:
  // We ping-pong between the two halves of space in storage_. STL
  // implementations sometimes use one allocator to destroy the memory of
  // another, and require the source and target memory spaces of a copy
  // operation to be distinct. This ensures proper usage semantics during an
  // erase, push, pop, or insert operation. The alternative to this is a complex
  // re-implementation of classes that desire inlined storage (see e.g.,
  // util/gtl/inlinedvector.h). Since inlined classes typically will only use
  // small amounts of inlined elements, this is not too much wasted space.
  ION_ALIGN(16) char storage_[sizeof(T) * N * 2];
  T* current_;
  bool inlined_;
  template <typename U, int M> friend class StlInlinedAllocator;
};

// Each StlAllocator holds an Allocator. StlAllocator objects are only
// equal if they refer to the same Allocator and are allocating the
// same type T.
template <class T1, class T2>
bool operator==(const StlAllocator<T1>& lhs, const StlAllocator<T2>& rhs) {
  return false;
}

template <class T1, class T2>
bool operator!=(const StlAllocator<T1>& lhs, const StlAllocator<T2>& rhs) {
  return true;
}

template <class T>
bool operator==(const StlAllocator<T>& lhs, const StlAllocator<T>& rhs) {
  return lhs.GetAllocator() == rhs.GetAllocator();
}

template <class T>
bool operator!=(const StlAllocator<T>& lhs, const StlAllocator<T>& rhs) {
  return lhs.GetAllocator() != rhs.GetAllocator();
}

// StlInlinedAllocator instances are always unique because of inlined storage.
template <class T1, class T2, int N1, int N2>
bool operator==(const StlInlinedAllocator<T1, N1>& lhs,
                const StlInlinedAllocator<T2, N2>& rhs) {
  return &lhs == &rhs;
}

template <class T1, class T2, int N1, int N2>
bool operator!=(const StlInlinedAllocator<T1, N1>& lhs,
                const StlInlinedAllocator<T2, N2>& rhs) {
  return &lhs != &rhs;
}

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_STLALLOC_STLALLOCATOR_H_
