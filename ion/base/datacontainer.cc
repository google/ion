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

#include "ion/base/datacontainer.h"

#include <mutex>  // NOLINT(build/c++11)

#include "ion/base/allocationmanager.h"
#include "ion/base/staticsafedeclare.h"

namespace ion {
namespace base {

DataContainer::DataContainer(const Deleter& deleter, bool is_wipeable)
    : data_(nullptr),
      is_wipeable_(is_wipeable),
      deleter_(deleter) {
}

DataContainer::~DataContainer() {
  AddOrRemoveDataFromCheck(data_, false);
  InternalWipeData();
}

void DataContainer::WipeData() {
  AddOrRemoveDataFromCheck(data_, false);
  if (is_wipeable_)
    InternalWipeData();
}

DataContainer* DataContainer::Allocate(
    size_t extra_bytes, const Deleter& deleter, bool is_wipeable,
    const AllocatorPtr& allocator) {
  const AllocatorPtr& alloc = allocator.Get()
      ? allocator
      : AllocationManager::GetDefaultAllocatorForLifetime(kMediumTerm);

  void* ptr =
      DataContainer::operator new(sizeof(DataContainer) + extra_bytes, alloc);
  DataContainer* container = new(ptr) DataContainer(deleter, is_wipeable);
  return container;
}

bool DataContainer::AddOrRemoveDataFromCheck(void* data, bool is_add) {
#if ION_DEBUG
  // Set of client-created pointers that are contained by DataContainers.
  ION_DECLARE_SAFE_STATIC_POINTER(std::set<void*>, client_pointers_used);

  // Protect access to client_pointers_used.
  ION_DECLARE_SAFE_STATIC_POINTER(std::mutex, mutex);
  std::lock_guard<std::mutex> guard(*mutex);

  // If another DataContainer already uses this pointer log an error and
  // return a null container.
  if (is_add) {
    if (client_pointers_used->count(data)) {
      LOG(ERROR) << "Duplicate client-space pointer passed to "
                 << "DataContainer::Create(). This is very dangerous and may "
                 << "result in double-deletion! It is much safer to simply "
                 << "use the same DataContainerPtr.";
      return false;
    }
    client_pointers_used->insert(data);
  } else {
    client_pointers_used->erase(data);
  }
#endif
  return true;
}

void DataContainer::InternalWipeData() {
  if (data_ != nullptr && deleter_) {
    deleter_(data_);
    data_ = nullptr;
  }
}

}  // namespace base
}  // namespace ion
