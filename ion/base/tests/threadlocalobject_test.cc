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

#include "ion/base/threadlocalobject.h"

#include "base/integral_types.h"
#include "ion/base/tests/testallocator.h"
#include "ion/base/threadspawner.h"
#include "ion/port/atomic.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

//-----------------------------------------------------------------------------
//
// Classes wrapped by the ThreadLocalObject for testing.
//
//-----------------------------------------------------------------------------

namespace {

// An instance of this class is created per thread in the test. Each instance
// has a unique integer ID. It also maintains a static count of the number of
// instances that currently exist.
class PerThread {
 public:
  PerThread() : id_(10U + next_id_++) { ++instance_count_; }
  ~PerThread() { --instance_count_; }
  int32 GetId() const { return id_; }
  static int32 GetInstanceCount() { return instance_count_; }

 private:
  static std::atomic<int32> next_id_;
  static std::atomic<int32> instance_count_;
  int32 id_;
};

std::atomic<int32> PerThread::next_id_(0);
std::atomic<int32> PerThread::instance_count_(0);

// This class is a singleton that stores a unique PerThread instance per thread.
class Singleton {
 public:
  Singleton() : id_sum_(0) {}

  const ThreadLocalObject<PerThread>& GetThreadLocalObject() const {
    return tlo_;
  }

  // This is the function that executes in each thread. It gets the unique
  // PerThread instance for the thread and adds its ID to the ID sum.
  bool TestPerThread() {
    PerThread* PerThread = tlo_.Get();
    id_sum_ += PerThread->GetId();

    // Verify that accessing the PerThread again gets the same pointer.
    EXPECT_EQ(PerThread, tlo_.Get());
    return true;
  }

  // Returns the sum of the PerThread IDs that were accumulated.
  int32 GetIdSum() const { return id_sum_; }

 private:
  ThreadLocalObject<PerThread> tlo_;
  std::atomic<int32> id_sum_;
};

// Simple derived Allocatable class.
class DerivedAllocatable : public Allocatable {
 public:
  ~DerivedAllocatable() override {}
};

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Tests.
//
//-----------------------------------------------------------------------------

TEST(ThreadLocalObject, InstancePerThread) {
  {
    // Verify that each instance of a Singleton::PerThread is different in each
    // thread.
    Singleton singleton;
    EXPECT_NE(port::kInvalidThreadLocalStorageKey,
              singleton.GetThreadLocalObject().GetKey());
    EXPECT_EQ(0, PerThread::GetInstanceCount());
    {
      using std::bind;
      ThreadSpawner t1("thread1", bind(&Singleton::TestPerThread, &singleton));
      ThreadSpawner t2("thread2", bind(&Singleton::TestPerThread, &singleton));
      ThreadSpawner t3("thread3", bind(&Singleton::TestPerThread, &singleton));
      ThreadSpawner t4("thread4", bind(&Singleton::TestPerThread, &singleton));
    }
    // There should have been 4 instances created, 1 per thread.
    EXPECT_EQ(4, PerThread::GetInstanceCount());
    // The sum of the IDs should be 10+11+12+13.
    EXPECT_EQ(10 + 11 + 12 + 13, singleton.GetIdSum());
  }
  // All PerThread instances should be destroyed along with the singleton.
  EXPECT_EQ(0, PerThread::GetInstanceCount());
}

TEST(ThreadLocalObject, Allocator) {
  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  {
    ThreadLocalObject<DerivedAllocatable> tl(allocator);
    EXPECT_NE(port::kInvalidThreadLocalStorageKey, tl.GetKey());

    EXPECT_EQ(0U, allocator->GetNumAllocated());
    EXPECT_EQ(0U, allocator->GetNumDeallocated());

    // Make sure the Allocator is used to create the instance in this thread.
    DerivedAllocatable* da = tl.Get();
    EXPECT_EQ(1U, allocator->GetNumAllocated());
    EXPECT_EQ(0U, allocator->GetNumDeallocated());

    // Calling Get() again should return the same instance and not allocate.
    EXPECT_EQ(da, tl.Get());
    EXPECT_EQ(1U, allocator->GetNumAllocated());
    EXPECT_EQ(0U, allocator->GetNumDeallocated());
  }
  EXPECT_EQ(1U, allocator->GetNumAllocated());
  EXPECT_EQ(1U, allocator->GetNumDeallocated());
}

TEST(ThreadLocalObject, NullAllocator) {
  // Make sure using a NULL Allocator pointer for an Allocatable is OK.
  AllocatorPtr null_allocator;
  ThreadLocalObject<DerivedAllocatable> tl(null_allocator);
  EXPECT_NE(port::kInvalidThreadLocalStorageKey, tl.GetKey());
  DerivedAllocatable* da = tl.Get();
  // Calling Get() again should return the same instance.
  EXPECT_EQ(da, tl.Get());
}

}  // namespace base
}  // namespace ion
