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

#include "ion/base/calllist.h"

namespace ion {
namespace base {

CallList::CallList() : calls_(*this) {}

CallList::~CallList() {}

// Executes the stored calls.
void CallList::Execute() {
  const size_t count = calls_.size();
  for (size_t i = 0; i < count; ++i)
    (*calls_[i])();
}

// Clears the set of calls.
void CallList::Clear() {
  calls_.clear();
}

}  // namespace base
}  // namespace ion
