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

#ifndef ION_BASE_NOTIFIER_H_
#define ION_BASE_NOTIFIER_H_

#include <algorithm>

#include "ion/base/lockguards.h"
#include "ion/base/readwritelock.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/base/weakreferent.h"

namespace ion {
namespace base {

// A Notifier both sends notifications to and receives notifications from other
// Notifiers. This is accomplished through the Notify() function, which calls
// OnNotify() on all held Notifiers.
class ION_API Notifier : public WeakReferent {
 public:
  // Adds a Notifier to be notified. Does nothing if the receiver is NULL or is
  // already in the receiver vector.
  void AddReceiver(Notifier* receiver);

  // Removes a Notifier to be notified. Does nothing if the receiver is NULL or
  // not in the set of receivers.
  void RemoveReceiver(Notifier* receiver);

  // Returns the number of Notifiers that will be notified.
  size_t GetReceiverCount() const;

 protected:
  typedef WeakReferentPtr<Notifier> NotifierPtr;
  typedef AllocVector<NotifierPtr> NotifierPtrVector;

  // The constructor is protected because this is a base class and should not be
  // created on its own.
  Notifier() : receivers_(*this) {}
  // The destructor is protected because this is derived from Referent.
  ~Notifier() override;

  // Returns the set of Notifiers that will be notified.
  const NotifierPtrVector& GetReceivers() const;

  // Notifies all contained Notifiers by calling their OnNotify(). Any receivers
  // that have been destroyed will be removed from the vector of receivers.
  void Notify() const;

  // Subclasses can override this to provide custom behavior on notifications.
  virtual void OnNotify(const Notifier* notifier);

 private:
  // This is mutable so that Notify() can be called even on const instances.
  mutable NotifierPtrVector receivers_;
  // Protect access to receivers_.
  mutable base::ReadWriteLock mutex_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_NOTIFIER_H_
