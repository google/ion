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

#ifndef ION_BASE_DATACONTAINER_H_
#define ION_BASE_DATACONTAINER_H_

#include <cstring>
#include <functional>

#if ION_DEBUG
#include <set>
#endif

#include "base/integral_types.h"
#include "base/macros.h"
#include "ion/base/logging.h"
#include "ion/base/notifier.h"
#include "ion/port/nullptr.h"  // For kNullFunction.

namespace ion {
namespace base {

// Convenience typedefs for shared pointers to a DataContainer.
class DataContainer;
using DataContainerPtr = base::SharedPtr<DataContainer>;
typedef base::WeakReferentPtr<DataContainer> DataContainerWeakPtr;

// The DataContainer class encapsulates arbitrary user data passed to Ion. It
// can only be created using one of the static Create() functions, templatized
// on a type T. The data in the DataContainer is created and destroyed depending
// on which Create() function is used. The three possibilities are:
//
// Create(T* data, Deleter data_deleter, bool is_wipeable,
//        const AllocatorPtr& container_allocator)
//   The internal data pointer is set to data and the DataContainer is allocated
//   using the passed Allocator. If data_deleter is NULL, then the caller is
//   responsible for deleting data, and is_wipeable is ignored. If data_deleter
//   is non-NULL, then the data will be deleted by calling data_deleter on the
//   pointer when WipeData() is called if is_wipeable is set; otherwise
//   data_deleter is called on the pointer only when the DataContainer is
//   destroyed. The deleter can be any std::function that properly cleans up
//   data; ArrayDeleter, PointerDeleter, and AllocatorDeleter static functions
//   are provided for convenience.
//
// CreateAndCopy(const T* data, size_t count, bool is_wipeable,
//               const AllocatorPtr& container_and_data_allocator)
//   The DataContainer and its internal data pointer are allocated from the
//   passed Allocator and count elements are copied from data if data is
//   non-NULL. The data will be deleted when WipeData() is called if
//   is_wipeable is set; otherwise it is deleted only when the DataContainer is
//   destroyed. The deleter function is always AllocatorDeleter (see below).
//
// CreateOverAllocated(size_t count, T* data,
//                     const AllocatorPtr& container_allocator)
//   Over-allocates the DataContainer by count elements of type T (i.e., this is
//   essentially malloc(sizeof(DataContainer) + sizeof(T) * count)) and copies
//   data into the DataContainer's data if data is non-NULL. The memory is
//   allocated using the passed Allocator. The new data is destroyed only when
//   the DataContainer is destroyed.
class ION_API DataContainer : public base::Notifier {
 public:
  // Generic delete function.
  typedef std::function<void(void* data_to_delete)> Deleter;

  // Generic deleters that perform the most common deletion operations. These
  // deleters may all be passed to Create() (see above).
  template <typename T>
  static void ArrayDeleter(void* data_to_delete) {
    T* data = reinterpret_cast<T*>(data_to_delete);
    delete [] data;
  }
  template <typename T>
  static void PointerDeleter(void* data_to_delete) {
    T* data = reinterpret_cast<T*>(data_to_delete);
    delete data;
  }
  // A deleter for data allocated by an Allocator. The AllocatorPtr is passed by
  // value so that a std::bind that invokes the deleter will hold a strong
  // reference to it.
  static void AllocatorDeleter(const AllocatorPtr& allocator,
                               void* data_to_delete) {
    DCHECK(allocator.Get());
    allocator->DeallocateMemory(data_to_delete);
  }

  // Returns the is_wipeable setting passed to the constructor.
  bool IsWipeable() const { return is_wipeable_; }

  // Returns a const data pointer.
  template <typename T>
  const T* GetData() const {
    return reinterpret_cast<const T*>(GetDataPtr());
  }

  // Default GetData() returns a const void pointer.
  const void* GetData() const {
    return reinterpret_cast<const void*>(GetDataPtr());
  }

  // Returns a non-const data pointer.
  template <typename T>
  T* GetMutableData() const {
    T* data = reinterpret_cast<T*>(GetDataPtr());
    if (data)
      this->Notify();
    else
      LOG(ERROR) << "GetMutableData() called on NULL (or wiped) DataContainer. "
                    "The contents of the original buffer will not be returned "
                    "and any data in GPU memory will likely be cleared. This "
                    "is probably not what you want.";
    return data;
  }

  // See class comment for documentation.
  template <typename T>
  static DataContainerPtr Create(
      T* data, const Deleter& data_deleter, bool is_wipeable,
      const AllocatorPtr& container_allocator) {
    if (data_deleter && !AddOrRemoveDataFromCheck(data, true))
      return DataContainerPtr(nullptr);
    DataContainer* container =
        Allocate(0, data_deleter, is_wipeable, container_allocator);
    container->data_ = data;
    return DataContainerPtr(container);
  }

  // See class comment for documentation.
  template <typename T>
  static DataContainerPtr CreateAndCopy(
      const T* data, size_t count, bool is_wipeable,
      const AllocatorPtr& container_and_data_allocator) {
    return CreateAndCopy(data, sizeof(T), count, is_wipeable,
                         container_and_data_allocator);
  }

  // Overload of CreateAndCopy for use when the size of the
  // datatype of the pointer does not correspond to the size of each element.
  template <typename T>
  static DataContainerPtr CreateAndCopy(
      const T* data, size_t element_size, size_t count, bool is_wipeable,
      const AllocatorPtr& container_and_data_allocator) {
    DataContainer* container =
        Allocate(0, kNullFunction, is_wipeable, container_and_data_allocator);
    // If the data is wipeable then the allocation should be short term,
    // otherwise it should have the same lifetime as this.
    if (is_wipeable) {
      container->data_allocator_ =
          container->GetAllocator().Get()
              ? container->GetAllocator()->GetAllocatorForLifetime(kShortTerm)
              : AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm);
    } else {
      container->data_allocator_ = container->GetAllocator();
    }
    container->deleter_ =
        std::bind(DataContainer::AllocatorDeleter, container->data_allocator_,
                  std::placeholders::_1);
    container->data_ =
        container->data_allocator_->AllocateMemory(element_size * count);
    // Copy the input data to the container.
    if (data) memcpy(container->data_, data, element_size * count);
    return DataContainerPtr(container);
  }

  // See class comment for documentation.
  template <typename T>
  static DataContainerPtr CreateOverAllocated(
      size_t count, const T* data, const AllocatorPtr& container_allocator) {
    // Allocate an additional 16 bytes to ensure that the data pointer can be
    // 16-byte aligned.
    DataContainer* container = Allocate(sizeof(T) * count + 16, kNullFunction,
                                        false, container_allocator);
    uint8* ptr = reinterpret_cast<uint8*>(container) + sizeof(DataContainer);
    // Offset the pointer by the right amount to make it 16-byte aligned.
    container->data_ = ptr + 16 - (reinterpret_cast<size_t>(ptr) % 16);
    // Copy the input data to the container.
    if (data)
      memcpy(container->data_, data, sizeof(T) * count);
    return DataContainerPtr(container);
  }

  // Informs the DataContainer that the data is no longer needed and can be
  // deleted. It does this only if is_wipeable=true was passed to the
  // constructor and there is a non-NULL deleter; otherwise, it has no effect.
  void WipeData();

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~DataContainer() override;

  // The constructor is protected because all allocation of DataContainers
  // should be through the Create() functions.
  DataContainer(const Deleter& deleter, bool is_wipeable);

 private:
  // Allocates space for a DataContainer with extra_bytes, and initializes
  // the DataContainer with deleter and is_wipeable (see constructor).
  static DataContainer* Allocate(size_t extra_bytes, const Deleter& deleter,
                                 bool is_wipeable,
                                 const AllocatorPtr& allocator);

  // In debug mode, adds or removes data to or from a set of client-passed
  // pointers that are used by DataContainers. An error message is printed if
  // if the same pointer is passed to Create() before destroying the
  // DataContainer that already contains the pointer.
  static bool AddOrRemoveDataFromCheck(void* data, bool is_add);

  // Actually delete data.
  virtual void InternalWipeData();

  // Returns the data pointer of this instance.
  virtual void* GetDataPtr() const { return data_; }

  // The actual data;
  void* data_;
  // Whether the data should be destroyed when WipeData() is called and there
  // is a deleter available.
  bool is_wipeable_;
  // Function to use to destroy data_. NULL if the DataContainer does not own
  // the pointer.
  Deleter deleter_;

  // The allocator used to allocate data when CreateAndCopy() is used.
  base::AllocatorPtr data_allocator_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DataContainer);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_DATACONTAINER_H_
