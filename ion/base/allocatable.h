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

#ifndef ION_BASE_ALLOCATABLE_H_
#define ION_BASE_ALLOCATABLE_H_

#include "base/macros.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/allocator.h"
#include "ion/base/logging.h"
#include "ion/external/gtest/gunit_prod.h"  // For FRIEND_TEST().

namespace ion {
namespace base {

// Allocatable is an abstract base class for classes whose memory is managed by
// an Allocator. This class defines new and delete operators that force the use
// of an Allocator to produce and reclaim the memory used by the instance. An
// Allocatable also makes an Allocator available via GetAllocator(). If the
// default constructor is used to construct the Allocatable, the returned
// Allocator is either the Allocator used to create the Allocatable or NULL if
// the Allocatable was created on the stack. If the stack-allocation-specific
// constructor taking an Allocator is used, then GetAllocator() returns that
// Allocator.
//
// Allocatables may be copy-constructed or assigned. Note that the internal
// Allocator of the instance is _not_ copied, however. This ensures that each
// Allocatable points to the Allocator that created it. Allocatables may also
// be created with placement new; if a particular Allocator is desired in the
// instance, use the special "new(allocator, memory_ptr)" version. Any
// Allocatable created with normal placement new ("new(memory_ptr) Type") will
// not contain an Allocator.
//
// Allocatables may be used in STL or Alloc-STL containers (e.g., AllocVector).
// The rules for how the Allocator of a contained Allocatable is set are as
// follows:
//   1) Allocatables stored directly in the container as keys or values, or
//      direct member variables of those, will return the container's Allocator
//      when GetAllocator() is called on them.
//   2) Any Allocatable that is new'd uses whatever Allocator is provided to new
//      as normal. For example, if a contained Allocatable creates another one
//      with "new(an_allocator) AllocatableType" then that instance will have
//      an_allocator as its Allocator. If no Allocator is passed to new then the
//      default Allocator is used. This is consistent with non-contained
//      Allocatable behavior.
class ION_API Allocatable {
 public:
  // The destructor clears the reference to the allocator.
  virtual ~Allocatable();

  // Returns the Allocator that was used for the instance. This will be NULL if
  // the instance was declared on the stack or created with normal placement
  // new.
  const AllocatorPtr& GetAllocator() const { return allocator_; }

  // Return our allocator, or the default allocator if the instance
  // was declared on the stack.
  const AllocatorPtr& GetNonNullAllocator() const {
    return AllocationManager::GetNonNullAllocator(allocator_);
  }

  // Convenience function that returns the Allocator to use to allocate an
  // object with a specific lifetime.
  const AllocatorPtr& GetAllocatorForLifetime(
      AllocationLifetime lifetime) const {
    return GetAllocator()->GetAllocatorForLifetime(lifetime);
  }

  // The standard no-parameter new operator uses the default Allocator.
  void* operator new(size_t size) { return New(size, GetDefaultAllocator()); }

  // This overloaded version of the new operator uses the AllocationManager's
  // default Allocator for the specified lifetime.
  void* operator new(size_t size, AllocationLifetime lifetime) {
    return New(size, GetDefaultAllocatorForLifetime(lifetime));
  }

  // This overloaded version of the new operator takes the Allocator to use
  // directly as a parameter. If the Allocator pointer is NULL, this uses the
  // default Allocator.
  void* operator new(size_t size, const AllocatorPtr& allocator) {
    return New(size, allocator);
  }

  // Special operator new for using placement new with Allocatables.
  void* operator new(size_t size, const AllocatorPtr& allocator, void* ptr) {
    return PlacementNew(size, allocator, ptr);
  }

  // The placement new operator is defined conventionally.
  void* operator new(size_t size, void* ptr) { return ptr; }

  // Define the delete operator to use specialized functions dealing with an
  // Allocator.
  void operator delete(void* ptr) { Delete(ptr); }

  // Windows requires these (or it issues C4291 warnings).
  void operator delete(void* ptr, AllocationLifetime lifetime) { Delete(ptr); }
  void operator delete(void* ptr, const AllocatorPtr& allocator) {
    Delete(ptr);
  }

  // The placement delete operator does nothing, as usual.
  void operator delete(void* ptr, void* ptr2) {}

 protected:
  // This constructor sets up the Allocator pointer. If this instance is created
  // on the stack then GetAllocator() will return a NULL Allocator since the
  // allocation and deallocation is performed by the compiler.
  Allocatable();

  // The copy constructor works similarly to the default constructor. It does
  // not, however, copy any members from the other Allocatable, since these are
  // intrinsically tied to a particular allocation.
  Allocatable(const Allocatable& other);

  // The assignment operator does nothing, since all members are intrinsically
  // tied to a particular allocation.
  void operator=(const Allocatable& other) {}

  // This constructor may only be used for Allocatables constructed on the
  // stack; this is enforced via a DCHECK. It allocates an Allocatable and
  // stores the passed allocator to use in subsequent calls to GetAllocator().
  // This is useful when a derived class has members that need a non-NULL
  // Allocator, for example to instantiate other Allocator-using objects.
  explicit Allocatable(const AllocatorPtr& allocator_in);

 private:
  // Allow StlAllocator to set the placement Allocator.
  template <typename T> friend class StlAllocator;

  // Internal singleton helper class.
  class Helper;

  // The array operators are private because there is no good way to set up
  // multiple Allocatable instances correctly. See the comment for
  // Allocatable::Helper in the source file.
  void* operator new[](size_t size);
  void* operator new[](size_t size, AllocationLifetime lifetime);
  void* operator new[](size_t size, const AllocatorPtr& allocator);
  void* operator new[](size_t size, void* ptr);
  void operator delete[](void* ptr);
  void operator delete[](void* ptr, AllocationLifetime lifetime);
  void operator delete[](void* ptr, const AllocatorPtr& allocator);
  void operator delete[](void* ptr, void* ptr2);

  // Constructs a valid instance of this for all constructors.
  void Construct();

  // Convenience functions to access default allocators.
  static const AllocatorPtr& GetDefaultAllocator() {
    return AllocationManager::GetDefaultAllocator();
  }
  static const AllocatorPtr& GetDefaultAllocatorForLifetime(
      AllocationLifetime lifetime) {
    return AllocationManager::GetDefaultAllocatorForLifetime(lifetime);
  }

  // These implement the new and delete operators.
  static void* New(size_t size, const AllocatorPtr& allocator);
  static void Delete(void* memory_ptr);
  static void* PlacementNew(size_t size, const AllocatorPtr& allocator,
                            void* memory_ptr);

  // Sets the Allocator to use for all allocations of Allocatables on this
  // thread until the next call to SetPlacementAllocator(NULL). This is required
  // for placement new constructions initiated by STL containers to inform an
  // about-to-be-constructed Allocatable what Allocator created it. The
  // Allocator must have a lifetime at least as long as the next call to
  // SetPlacementAllocator(NULL).
  //
  // This function is private since it is fairly dangerous. If used improperly
  // it could set the wrong Allocator for an Allocatable.
  static void SetPlacementAllocator(Allocator* allocator);

  // Returns the current placement Allocator. This is private for the same
  // reasons as SetPlacementAllocator().
  static Allocator* GetPlacementAllocator();

  // Returns the static Helper instance, creating it first if necessary in a
  // thread-safe way.
  static Helper* GetHelper();

  // The allocator that was used to get memory for this instance. This same
  // allocator is used to deallocate the memory.
  AllocatorPtr allocator_;

  // Address of the memory chunk from which this instance was allocated. For
  // non-array allocations, this is the same as "this". This pointer is needed
  // to tell the Delete() function which Allocator to use.
  const void* memory_ptr_;

  // Allows tests to check placement allocations.
  FRIEND_TEST(AllocatableTest, TrivialType);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_ALLOCATABLE_H_
