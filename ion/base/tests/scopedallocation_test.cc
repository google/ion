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

#include "ion/base/scopedallocation.h"

#include "ion/base/allocationmanager.h"
#include "ion/base/datacontainer.h"
#include "ion/base/tests/testallocator.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace {

// A class not derived from Allocatable that tracks construction and
// destruction.
class TestClass {
 public:
  TestClass() : a_(123) { ++num_constructors_; }
  ~TestClass() { ++num_destructors_; }
  int GetA() const { return a_; }

  static size_t GetNumConstructors() { return num_constructors_; }
  static size_t GetNumDestructors() { return num_destructors_; }

  static void ClearCounts() { num_constructors_ = num_destructors_ = 0; }

 private:
  int a_;
  static size_t num_constructors_;
  static size_t num_destructors_;
};

size_t TestClass::num_constructors_ = 0;
size_t TestClass::num_destructors_ = 0;

}  // anonymous namespace

TEST(ScopedAllocation, NoInstances) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  {
    ion::base::ScopedAllocation<TestClass> scoped_tc(a, 0);

    // Nothing should have been allocated and the pointer should be null.
    EXPECT_EQ(0U, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_TRUE(scoped_tc.Get() == nullptr);
    EXPECT_EQ(0U, TestClass::GetNumConstructors());
    EXPECT_EQ(0U, TestClass::GetNumDestructors());
  }

  // Nothing should have been deallocated.
  EXPECT_EQ(0U, TestClass::GetNumConstructors());
  EXPECT_EQ(0U, TestClass::GetNumDestructors());
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  TestClass::ClearCounts();
}

TEST(ScopedAllocation, OneInstance) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  {
    ion::base::ScopedAllocation<TestClass> scoped_tc(a);

    // The Allocator should have been used for the TestClass.
    EXPECT_EQ(1U, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());

    // Make sure the TestClass was allocated and constructed properly.
    EXPECT_EQ(1U, TestClass::GetNumConstructors());
    EXPECT_EQ(0U, TestClass::GetNumDestructors());
    EXPECT_FALSE(scoped_tc.Get() == nullptr);
    EXPECT_EQ(123, scoped_tc.Get()->GetA());
  }
  // The TestClass should have been destroyed.
  EXPECT_EQ(1U, TestClass::GetNumConstructors());
  EXPECT_EQ(1U, TestClass::GetNumDestructors());
  EXPECT_EQ(1U, a->GetNumAllocated());
  EXPECT_EQ(1U, a->GetNumDeallocated());

  TestClass::ClearCounts();
}

TEST(ScopedAllocation, NInstances) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  {
    ion::base::ScopedAllocation<TestClass> scoped_tc(a, 4);

    // The Allocator should have been used for the TestClass.
    EXPECT_EQ(1U, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());

    // Make sure the TestClass instances were allocated and constructed
    // properly.
    EXPECT_EQ(4U, TestClass::GetNumConstructors());
    EXPECT_EQ(0U, TestClass::GetNumDestructors());
    EXPECT_FALSE(scoped_tc.Get() == nullptr);
    for (int i = 0; i < 4; ++i)
      EXPECT_EQ(123, scoped_tc.Get()[i].GetA());
  }
  // All TestClass instances should have been destroyed.
  EXPECT_EQ(4U, TestClass::GetNumConstructors());
  EXPECT_EQ(4U, TestClass::GetNumDestructors());
  EXPECT_EQ(1U, a->GetNumAllocated());
  EXPECT_EQ(1U, a->GetNumDeallocated());

  TestClass::ClearCounts();
}

TEST(ScopedAllocation, Pods) {
  // Test ScopedAllocation with PODs, just to make sure they compile ok.
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  {
    ion::base::ScopedAllocation<int> scoped_tc(a, 32);

    // The Allocator should have been used for the ints.
    EXPECT_EQ(1U, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
  }
  // The ints should have been destroyed.
  EXPECT_EQ(1U, a->GetNumAllocated());
  EXPECT_EQ(1U, a->GetNumDeallocated());

  TestClass::ClearCounts();
}

TEST(ScopedAllocation, HeapAllocation) {
  // Test using a ScopedAllocation instance created on the heap. This is the
  // same as the NInstances test in all other respects.
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  ion::base::ScopedAllocation<TestClass>* scoped_tc =
      new ion::base::ScopedAllocation<TestClass>(a, 4);

  // The Allocator should have been used for the TestClass.
  EXPECT_EQ(1U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  // Make sure the TestClass instances were allocated and constructed
  // properly.
  EXPECT_EQ(4U, TestClass::GetNumConstructors());
  EXPECT_EQ(0U, TestClass::GetNumDestructors());
  EXPECT_FALSE(scoped_tc->Get() == nullptr);
  for (int i = 0; i < 4; ++i)
    EXPECT_EQ(123, scoped_tc->Get()[i].GetA());

  delete scoped_tc;

  // All TestClass instances should have been destroyed.
  EXPECT_EQ(4U, TestClass::GetNumConstructors());
  EXPECT_EQ(4U, TestClass::GetNumDestructors());
  EXPECT_EQ(1U, a->GetNumAllocated());
  EXPECT_EQ(1U, a->GetNumDeallocated());

  TestClass::ClearCounts();
}

TEST(ScopedAllocation, TransferToDataContainer) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);

  ion::base::DataContainerPtr dc;
  {
    ion::base::ScopedAllocation<TestClass> scoped_tc(a, 4);
    TestClass* ptr = scoped_tc.Get();
    EXPECT_FALSE(ptr == nullptr);

    // This should empty out the ScopedAllocation instance.
    dc = scoped_tc.TransferToDataContainer(false);
    EXPECT_TRUE(scoped_tc.Get() == nullptr);
    ASSERT_FALSE(dc.Get() == nullptr);
    EXPECT_EQ(ptr, dc->GetData());

    // Both the TestClass array and DataContainer should have been allocated.
    // No TestClass instances should have been destroyed yet.
    EXPECT_LE(2U, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(4U, TestClass::GetNumConstructors());
    EXPECT_EQ(0U, TestClass::GetNumDestructors());
  }
  // The ScopedAllocation destructor should have been called when the above
  // scope ended. It should not have tried to delete anything.
  EXPECT_LE(2U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());
  EXPECT_EQ(4U, TestClass::GetNumConstructors());
  EXPECT_EQ(0U, TestClass::GetNumDestructors());

  // Delete the DataContainer, which should cause the instances to be destroyed
  // and deleted as well.
  dc = nullptr;

  // Now all TestClass instances should have been destroyed.
  EXPECT_EQ(4U, TestClass::GetNumConstructors());
  EXPECT_EQ(4U, TestClass::GetNumDestructors());
  EXPECT_LE(2U, a->GetNumAllocated());
  EXPECT_LE(2U, a->GetNumDeallocated());

  TestClass::ClearCounts();
}

TEST(ScopedAllocation, TransferEmptyToDataContainer) {
  // An empty ScopedAllocation should transfer with no problems.
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);

  ion::base::DataContainerPtr dc;
  {
    ion::base::ScopedAllocation<TestClass> scoped_tc(a, 0);
    EXPECT_TRUE(scoped_tc.Get() == nullptr);
    dc = scoped_tc.TransferToDataContainer(false);
    ASSERT_FALSE(dc.Get() == nullptr);
    EXPECT_TRUE(scoped_tc.Get() == nullptr);
    EXPECT_TRUE(dc->GetData() == nullptr);
  }
  dc = nullptr;
  EXPECT_EQ(0U, TestClass::GetNumConstructors());
  EXPECT_EQ(0U, TestClass::GetNumDestructors());
  // Only the DataContainer itself should have been allocated and deallocated
  // with the Allocator.
  EXPECT_LE(1U, a->GetNumAllocated());
  EXPECT_LE(1U, a->GetNumDeallocated());

  TestClass::ClearCounts();
}
