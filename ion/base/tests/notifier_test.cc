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

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

class MyNotifier : public Notifier {
 public:
  MyNotifier() : notifications_(0U) {}
  void OnNotify(const Notifier* notifier) override {
    Notifier::OnNotify(notifier);
    notifications_++;
  }
  size_t GetNotificationCount() const { return notifications_; }

  // Expose vector for tests.
  using Notifier::NotifierPtrVector;

  // Expose protected functions for tests.
  using Notifier::GetReceivers;
  using Notifier::Notify;

 private:
  ~MyNotifier() override {}
  size_t notifications_;
};
using MyNotifierPtr = base::SharedPtr<MyNotifier>;

}  // anonymous namespace

TEST(Notifier, AddRemoveReceivers) {
  MyNotifierPtr n(new MyNotifier);

  // The notifier should initially be empty.
  const MyNotifier::NotifierPtrVector& notifiers = n->GetReceivers();
  EXPECT_TRUE(notifiers.empty());
  EXPECT_EQ(0U, n->GetNotificationCount());
  EXPECT_EQ(0U, n->GetReceiverCount());

  MyNotifierPtr n2(new MyNotifier);
  MyNotifierPtr n3(new MyNotifier);
  MyNotifierPtr n4(new MyNotifier);
  MyNotifierPtr n5(new MyNotifier);
  n->AddReceiver(nullptr);
  EXPECT_TRUE(notifiers.empty());
  n->AddReceiver(n2.Get());
  n->AddReceiver(n3.Get());
  n->AddReceiver(n4.Get());
  n->AddReceiver(n5.Get());
  EXPECT_EQ(4U, notifiers.size());
  EXPECT_EQ(4U, n->GetReceiverCount());

  // Check that duplicates are ok.
  n->AddReceiver(n2.Get());
  n->AddReceiver(n3.Get());
  n->AddReceiver(n4.Get());
  n->AddReceiver(n5.Get());
  EXPECT_EQ(4U, notifiers.size());

  n->RemoveReceiver(nullptr);
  EXPECT_EQ(4U, notifiers.size());
  EXPECT_EQ(4U, n->GetReceiverCount());
  n->RemoveReceiver(n5.Get());
  EXPECT_EQ(3U, notifiers.size());
  EXPECT_EQ(3U, n->GetReceiverCount());
  n->RemoveReceiver(n5.Get());
  EXPECT_EQ(3U, notifiers.size());
  EXPECT_EQ(3U, n->GetReceiverCount());
  n->RemoveReceiver(n4.Get());
  EXPECT_EQ(2U, notifiers.size());
  EXPECT_EQ(2U, n->GetReceiverCount());
  n->RemoveReceiver(n.Get());
  EXPECT_EQ(2U, notifiers.size());
  EXPECT_EQ(2U, n->GetReceiverCount());
  n->RemoveReceiver(n2.Get());
  n->RemoveReceiver(n3.Get());
  EXPECT_EQ(0U, notifiers.size());
  EXPECT_EQ(0U, n->GetReceiverCount());
}

TEST(Notifier, NotifiersCalled) {
  MyNotifierPtr n(new MyNotifier);

  // The notifier should initially be empty.
  const MyNotifier::NotifierPtrVector& notifiers = n->GetReceivers();
  EXPECT_TRUE(notifiers.empty());
  EXPECT_EQ(0U, n->GetNotificationCount());

  MyNotifierPtr n2(new MyNotifier);
  {
    // Add a Notifier.
    MyNotifierPtr n3(new MyNotifier);
    n->AddReceiver(n3.Get());
    EXPECT_EQ(1U, notifiers.size());
    EXPECT_EQ(n3.Get(), notifiers[0].Acquire().Get());

    // Trigger a call to OnNotify().
    n->Notify();
    EXPECT_EQ(0U, n2->GetNotificationCount());
    EXPECT_EQ(1U, n3->GetNotificationCount());

    // Trigger a couple more calls to OnNotify().
    n->Notify();
    n->Notify();
    EXPECT_EQ(0U, n2->GetNotificationCount());
    EXPECT_EQ(3U, n3->GetNotificationCount());

    // Add another Notifier.
    n->AddReceiver(n2.Get());
    EXPECT_EQ(2U, notifiers.size());
    EXPECT_EQ(n3.Get(), notifiers[0].Acquire().Get());
    EXPECT_EQ(n2.Get(), notifiers[1].Acquire().Get());
    n->Notify();
    EXPECT_EQ(1U, n2->GetNotificationCount());
    EXPECT_EQ(4U, n3->GetNotificationCount());
  }
  // n3 was just destroyed, so the next call to Notify should remove it from the
  // notification list.
  EXPECT_EQ(2U, notifiers.size());
  n->Notify();
  // Now there is only one Notifier, and it should be n2.
  EXPECT_EQ(1U, notifiers.size());
  EXPECT_EQ(n2.Get(), notifiers[0].Acquire().Get());
  EXPECT_EQ(2U, n2->GetNotificationCount());
}

}  // namespace base
}  // namespace ion
