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

#include <functional>

#include "ion/base/logchecker.h"
#include "ion/base/tests/testallocator.h"
#include "ion/port/nullptr.h"  // For kNullFunction.

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

using ion::base::testing::TestAllocator;
using ion::base::testing::TestAllocatorPtr;

static size_t s_num_destroys = 0;
static const int kDataCount = 1024;

struct Data {
  float f;
  int i;
};

static void DeleteData(void* data) {
  delete [] reinterpret_cast<Data*>(data);
  s_num_destroys++;
}

static Data* InitData() {
  Data* data = new Data[kDataCount];
  for (int i = 0; i < kDataCount; ++i) {
    const float f = static_cast<float>(i);
    data[i].f = f + 0.1f * f;
    data[i].i = i;
  }
  return data;
}

void CheckData(const Data* data, const Data* copied_data) {
  EXPECT_NE(data, copied_data);
  for (int i = 0; i < kDataCount; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(data[i].f, copied_data[i].f);
    EXPECT_EQ(data[i].i, copied_data[i].i);
  }
}

class MyNotifier : public Notifier {
 public:
  MyNotifier() : notifications_(0U) {}
  void OnNotify(const Notifier* notifier) override { notifications_++; }
  size_t GetNotificationCount() const { return notifications_; }

  // Expose Notify() for tests.
  using Notifier::Notify;

 private:
  ~MyNotifier() override {}
  size_t notifications_;
};
using MyNotifierPtr = base::SharedPtr<MyNotifier>;

}  // anonymous namespace

TEST(DataContainerTest, Create) {
  s_num_destroys = 0;
  EXPECT_EQ(0U, s_num_destroys);

  DataContainer::Deleter deleter =
      std::bind(&DeleteData, std::placeholders::_1);

  // Check that DeleteData is not called if data is NULL.
  {
    DataContainerPtr container(
        DataContainer::Create<Data>(nullptr, deleter, false, AllocatorPtr()));
    EXPECT_TRUE(container->GetData() == nullptr);
    EXPECT_FALSE(container->IsWipeable());
  }
  EXPECT_EQ(0U, s_num_destroys);

  // Check that DeleteData is called if data is not NULL.
  {
    Data* data = new Data[kDataCount];
    DataContainerPtr container(
        DataContainer::Create<Data>(data, deleter, false, AllocatorPtr()));
    EXPECT_EQ(data, container->GetData());
  }
  EXPECT_EQ(1U, s_num_destroys);

#if ION_DEBUG
  s_num_destroys = 0;
  // Check that calling Create() on the same pointer twice is an error in
  // debug mode if both have deleters.
  {
    base::LogChecker log_checker;
    Data* data = new Data[kDataCount];
    DataContainerPtr container(
        DataContainer::Create<Data>(data, deleter, false, AllocatorPtr()));
    EXPECT_EQ(data, container->GetData());

    DataContainerPtr container2(
        DataContainer::Create<Data>(data, deleter, false, AllocatorPtr()));
    EXPECT_TRUE(container2.Get() == nullptr);

    EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                       "Duplicate client-space pointer"));

    DataContainerPtr container3(
        DataContainer::Create<Data>(data, kNullFunction, false,
                                    AllocatorPtr()));
    EXPECT_EQ(data, container->GetData());
    EXPECT_FALSE(log_checker.HasAnyMessages());
}
  EXPECT_EQ(1U, s_num_destroys);
#endif

  // Check that we can pass a NULL deleter.
  {
    Data* data = new Data[kDataCount];
    DataContainerPtr container(
        DataContainer::Create<Data>(data, kNullFunction, false,
                                    AllocatorPtr()));
    EXPECT_EQ(data, container->GetData());
    delete [] data;
  }

  s_num_destroys = 0;
  // Check that DeleteData is called if data is not NULL, even if is_wipeable
  // is set.
  {
    Data* data = new Data[kDataCount];
    DataContainerPtr container(
        DataContainer::Create<Data>(data, deleter, true, AllocatorPtr()));
    EXPECT_TRUE(container->IsWipeable());
    EXPECT_EQ(data, container->GetData());
  }
  EXPECT_EQ(1U, s_num_destroys);

  base::LogChecker log_checker;
  s_num_destroys = 0;
  // Check that DeleteData is called after the data is used if is_wipeable is
  // set.
  {
    Data* data = new Data[kDataCount];
    DataContainerPtr container(
        DataContainer::Create<Data>(data, deleter, true, AllocatorPtr()));
    EXPECT_EQ(data, container->GetData());
    EXPECT_EQ(0U, s_num_destroys);
    Data* ptr = container->GetMutableData<Data>();
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    container->WipeData();
    EXPECT_EQ(1U, s_num_destroys);
    ptr = container->GetMutableData<Data>();
    EXPECT_TRUE(ptr == nullptr);
    EXPECT_TRUE(log_checker.HasMessage(
        "ERROR", "GetMutableData() called on NULL (or wiped) DataContainer"));
  }
  // Check that the data is not destroyed twice.
  EXPECT_EQ(1U, s_num_destroys);

  s_num_destroys = 0;
  // Check that DeleteData is not called after the data is used if is_wipeable
  // is not set.
  {
    Data* data = new Data[kDataCount];
    DataContainerPtr container(
        DataContainer::Create<Data>(data, deleter, false, AllocatorPtr()));
    EXPECT_EQ(data, container->GetData());
    EXPECT_EQ(0U, s_num_destroys);
    Data* ptr = container->GetMutableData<Data>();
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    container->WipeData();
    EXPECT_EQ(0U, s_num_destroys);
  }
  // Check that the data is not destroyed twice.
  EXPECT_EQ(1U, s_num_destroys);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that an allocator can be used to deallocate the data.
  TestAllocatorPtr allocator(new TestAllocator);
  {
    Data* data = static_cast<Data*>(
        allocator->AllocateMemory(sizeof(Data) * kDataCount));
    DataContainerPtr container(DataContainer::Create<Data>(
        data, std::bind(DataContainer::AllocatorDeleter, allocator,
                        std::placeholders::_1),
        true, AllocatorPtr()));
    EXPECT_EQ(data, container->GetData());
    EXPECT_GE(allocator->GetNumAllocated(), 1U);
    EXPECT_EQ(allocator->GetNumDeallocated(), 0U);
    EXPECT_GE(allocator->GetBytesAllocated(),
              sizeof(Data) * kDataCount);
    container->WipeData();
    EXPECT_GE(allocator->GetNumAllocated(), 1U);
    EXPECT_EQ(allocator->GetNumDeallocated(), 1U);
  }
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(DataContainerTest, CreateAndCopy) {
  base::LogChecker log_checker;
  // Check that data is copied correctly.
  {
    Data* data = InitData();
    DataContainerPtr container(DataContainer::CreateAndCopy<Data>(
        data, kDataCount, false, AllocatorPtr()));
    CheckData(data, container->GetData<Data>());
    delete[] data;
  }
  {
    Data* data = InitData();
    DataContainerPtr container(DataContainer::CreateAndCopy<Data>(
        data, kDataCount, true, AllocatorPtr()));
    CheckData(data, container->GetData<Data>());
    delete [] data;
  }
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that data is destroyed after WipeData is called if wipe_data is set.
  {
    Data* data = InitData();
    DataContainerPtr container(DataContainer::CreateAndCopy<Data>(
        data, kDataCount, true, AllocatorPtr()));
    CheckData(data, container->GetData<Data>());
    Data* ptr = container->GetMutableData<Data>();
    EXPECT_TRUE(ptr != nullptr);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    container->WipeData();
    EXPECT_TRUE(container->GetData<Data>() == nullptr);
    delete [] data;
    ptr = container->GetMutableData<Data>();
    EXPECT_TRUE(ptr == nullptr);
    EXPECT_TRUE(log_checker.HasMessage(
        "ERROR", "GetMutableData() called on NULL (or wiped) DataContainer"));
  }

  // Check that data is not destroyed after WipeData is called if wipe_data is
  // not set.
  {
    Data* data = InitData();
    DataContainerPtr container(DataContainer::CreateAndCopy<Data>(
        data, kDataCount, false, AllocatorPtr()));
    CheckData(data, container->GetData<Data>());
    container->WipeData();
    CheckData(data, container->GetData<Data>());
    delete [] data;
    Data* ptr = container->GetMutableData<Data>();
    EXPECT_TRUE(ptr != nullptr);
    // There should be no error since the data was not wiped.
  }
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(DataContainerTest, CreateOverAllocated) {
  base::LogChecker log_checker;
  // Check that the container has data.
  {
    DataContainerPtr container(DataContainer::CreateOverAllocated<Data>(
        kDataCount, nullptr, AllocatorPtr()));
    EXPECT_GE(container->GetData(), container.Get() + 1);
    // Check that the data pointer is 16-byte aligned.
    EXPECT_EQ(0U, reinterpret_cast<size_t>(container->GetData<Data>()) % 16U);
    // Check that WipeData does nothing.
    container->WipeData();
    EXPECT_GE(container->GetData(), container.Get() + 1);
    Data* ptr = container->GetMutableData<Data>();
    EXPECT_TRUE(ptr != nullptr);
  }

  // Check that the data is copied correctly.
  {
    Data* data = InitData();
    DataContainerPtr container(DataContainer::CreateOverAllocated<Data>(
        kDataCount, data, AllocatorPtr()));
    CheckData(data, container->GetData<Data>());
    // Check that WipeData does nothing.
    container->WipeData();
    CheckData(data, container->GetData<Data>());
    delete [] data;
    Data* ptr = container->GetMutableData<Data>();
    EXPECT_TRUE(ptr != nullptr);
  }
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(DataContainerTest, DefaultDestructors) {
  // These tests are primarily to improve code coverage.
  s_num_destroys = 0;
  // Array deleter.
  {
    Data* data = new Data[kDataCount];
    DataContainerPtr container(DataContainer::Create<Data>(
        data, &DataContainer::ArrayDeleter<Data>, false, AllocatorPtr()));
  }

  // Single pointer deleter.
  {
    Data* data = new Data;
    DataContainerPtr container(DataContainer::Create<Data>(
        data, DataContainer::PointerDeleter<Data>, false, AllocatorPtr()));
  }
  EXPECT_EQ(0U, s_num_destroys);
}

TEST(DataContainerTest, Allocator) {
  // Check that a DataContainer can be created using a non-default Allocator.
  {
    TestAllocatorPtr allocator(new TestAllocator);
    // Check that the container has data.
    {
      DataContainerPtr container(DataContainer::CreateOverAllocated<Data>(
          kDataCount, nullptr, allocator));
      EXPECT_GE(container->GetData(), container.Get() + 1);
      // Check that the data pointer is 16-byte aligned.
      EXPECT_EQ(0U, reinterpret_cast<size_t>(container->GetData<Data>()) % 16U);
      // Check that WipeData does nothing.
      container->WipeData();
      EXPECT_GE(container->GetData(), container.Get() + 1);
    }
    EXPECT_GE(allocator->GetNumAllocated(), 1U);
    EXPECT_GE(allocator->GetNumDeallocated(), 1U);
    EXPECT_GE(allocator->GetBytesAllocated(),
              sizeof(DataContainer) + kDataCount);
  }

  {
    TestAllocatorPtr allocator(new TestAllocator);
    // Check that the container has data.
    {
      Data* data = InitData();
      DataContainerPtr container(DataContainer::CreateAndCopy<Data>(
          data, kDataCount, false, allocator));
      CheckData(data, container->GetData<Data>());
      delete[] data;
    }
    EXPECT_GE(allocator->GetNumAllocated(), 2U);
    EXPECT_GE(allocator->GetNumDeallocated(), 2U);
    EXPECT_GE(allocator->GetBytesAllocated(),
              sizeof(DataContainer) + kDataCount);
  }
}

TEST(DataContainerTest, Notifications) {
  base::LogChecker log_checker;
  MyNotifierPtr n(new MyNotifier);

  DataContainerPtr container(DataContainer::CreateOverAllocated<Data>(
      kDataCount, nullptr, AllocatorPtr()));
  container->AddReceiver(n.Get());

  EXPECT_EQ(0U, n->GetNotificationCount());
  container->GetData();
  EXPECT_EQ(0U, n->GetNotificationCount());
  container->GetMutableData<void*>();
  EXPECT_EQ(1U, n->GetNotificationCount());
  container->GetMutableData<void*>();
  EXPECT_EQ(2U, n->GetNotificationCount());
  container->GetData();
  EXPECT_EQ(2U, n->GetNotificationCount());

  Data* data = InitData();
  container = DataContainer::CreateAndCopy<Data>(data, kDataCount, true,
                                                 AllocatorPtr());
  delete[] data;
  container->AddReceiver(n.Get());
  EXPECT_EQ(2U, n->GetNotificationCount());
  container->GetMutableData<void*>();
  EXPECT_EQ(3U, n->GetNotificationCount());
  container->WipeData();
  EXPECT_EQ(3U, n->GetNotificationCount());
  EXPECT_FALSE(log_checker.HasAnyMessages());
  container->GetMutableData<void*>();
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "called on NULL"));
  // There should not be a notification since the data is NULL.
  EXPECT_EQ(3U, n->GetNotificationCount());
}

}  // namespace base
}  // namespace ion
