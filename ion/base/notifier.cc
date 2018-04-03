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

#include "ion/base/notifier.h"

namespace ion {
namespace base {

Notifier::~Notifier() {}

void Notifier::AddReceiver(Notifier* receiver) {
  if (receiver) {
    NotifierPtr ptr(receiver);
    WriteLock lock(&mutex_);
    WriteGuard guard(&lock);
    const size_t count = receivers_.size();
    for (size_t i = 0; i < count; ++i)
      if (ptr == receivers_[i])
        return;
    receivers_.push_back(ptr);
  }
}

void Notifier::RemoveReceiver(Notifier* receiver) {
  if (receiver) {
    WriteLock lock(&mutex_);
    WriteGuard guard(&lock);
    const size_t count = receivers_.size();
    if (receiver->GetRefCount()) {
      NotifierPtr ptr(receiver);
      for (size_t i = 0; i < count; ++i) {
        if (receivers_[i] == ptr) {
          receivers_[i] = receivers_[receivers_.size() - 1U];
          receivers_.pop_back();
          break;
        }
      }
    } else {
      // We are being called from receiver's destructor. Short of using an
      // additional map from Notifier* to NotifierPtr, we have to check every
      // weak pointer in the vector. Because the destructor can only be called
      // from one thread when the object is going way, there are no concurrency
      // issues.
      for (size_t i = 0; i < receivers_.size();) {
        if (receivers_[i].GetUnderlyingRefCountUnsynchronized()) {
          ++i;
        } else {
          receivers_[i] = receivers_[receivers_.size() - 1U];
          receivers_.pop_back();
          break;
        }
      }
    }
  }
}

size_t Notifier::GetReceiverCount() const {
  ReadLock lock(&mutex_);
  ReadGuard guard(&lock);
  return receivers_.size();
}

const Notifier::NotifierPtrVector& Notifier::GetReceivers() const {
  return receivers_;
}

void Notifier::Notify() const {
  ReadLock lock(&mutex_);
  ReadGuard guard(&lock);
  for (size_t i = 0; i < receivers_.size();) {
    SharedPtr<Notifier> receiver = receivers_[i].Acquire();
    if (receiver.Get()) {
      receiver->OnNotify(this);
      ++i;
    } else {
      receivers_[i] = receivers_[receivers_.size() - 1U];
      receivers_.pop_back();
    }
  }
}

void Notifier::OnNotify(const Notifier* notifier) {}

}  // namespace base
}  // namespace ion
