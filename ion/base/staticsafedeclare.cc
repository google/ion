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

#include "ion/base/staticsafedeclare.h"

#include "ion/base/logging.h"

namespace ion {
namespace base {

StaticDeleterDeleter::StaticDeleterDeleter() {
  // The logging system uses StaticDeleterDeleter so to allow classes registered
  // with StaticDeleterDeleter to treat logging as an always-available facility,
  // we have to ensure that the LogEntryWriter is registered first (so it is
  // deleted last).  Force this via the call below.
  ion::base::GetDefaultLogEntryWriter();
}

StaticDeleterDeleter::~StaticDeleterDeleter() {
  // Delete all pointers in reverse order of construction.
  LockGuard locker(&mutex_);
  // Static cast the vector size to int to catch empty vectors.
  for (int i = static_cast<int>(deleters_.size()) - 1; i >= 0; --i)
    delete deleters_[i];
}

void StaticDeleterDeleter::SetInstancePtr(
  const std::string&, StaticDeleterDeleter* instance) {
  // This static will be destroyed at exit, and will trigger deleting the held
  // StaticDeleters.
  // We need this in a separate function because static local initialization is
  // not thread-safe in C++03 (it is in C++11). Therefore, we cannot declare it
  // in GetInstance() which may be called from anywhere. The
  // AtomicCompareAndSwap logic in GetInstance() ensures this method will only
  // be called by one thread.
  static SharedPtr<StaticDeleterDeleter> s_singleton_holder;
  s_singleton_holder.Reset(instance);
}

StaticDeleterDeleter* StaticDeleterDeleter::GetInstance() {
  ION_DECLARE_SAFE_STATIC(StaticDeleterDeleter*, singleton_ptr,
                          new StaticDeleterDeleter, SetInstancePtr,
                          delete);
  return singleton_ptr;
}

void StaticDeleterDeleter::DestroyInstance() {
  SetInstancePtr("StaticDeleterDeleter", NULL);
}

}  // namespace base
}  // namespace ion
