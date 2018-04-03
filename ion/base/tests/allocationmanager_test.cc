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

#include "ion/base/allocationmanager.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

// A derived Allocator class used for testing. Implementation does not matter.
class TestAllocator : public ion::base::Allocator {
 public:
  void* Allocate(size_t /* size */) override { return nullptr; }
  void Deallocate(void* /* p */) override {}
};

// Defines a test framework that restores the default allocators in the
// AllocationManager on teardown. Since AllocationManager is a singleton, this
// keeps tests from interfering with each other.
class AllocationManagerTest : public ::testing::Test {
 protected:
  void TearDown() override {
    // Use a null pointer to restore the defaults.
    AllocatorPtr a;
    AllocationManager::SetDefaultAllocatorForLifetime(kShortTerm, a);
    AllocationManager::SetDefaultAllocatorForLifetime(kMediumTerm, a);
    AllocationManager::SetDefaultAllocatorForLifetime(kLongTerm, a);
    AllocationManager::SetDefaultAllocationLifetime(kMediumTerm);
  }
};

}  // anonymous namespace

TEST_F(AllocationManagerTest, DefaultAllocators) {
  // The MallocAllocator should always be available.
  const AllocatorPtr& ma = AllocationManager::GetMallocAllocator();
  EXPECT_TRUE(ma);

  // The MallocAllocator should be used by default for all lifetimes.
  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm));
  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocatorForLifetime(kMediumTerm));
  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocatorForLifetime(kLongTerm));
}

TEST_F(AllocationManagerTest, AllocationLifetime) {
  EXPECT_EQ(kMediumTerm, AllocationManager::GetDefaultAllocationLifetime());

  AllocationManager::SetDefaultAllocationLifetime(kLongTerm);
  EXPECT_EQ(kLongTerm, AllocationManager::GetDefaultAllocationLifetime());
}

TEST_F(AllocationManagerTest, DefaultAllocatorForLifetime) {
  const AllocatorPtr& ma = AllocationManager::GetMallocAllocator();

  // Change one default allocator.
  AllocatorPtr a0(new TestAllocator);
  AllocationManager::SetDefaultAllocatorForLifetime(kMediumTerm, a0);

  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm));
  EXPECT_EQ(a0, AllocationManager::GetDefaultAllocatorForLifetime(kMediumTerm));
  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocatorForLifetime(kLongTerm));

  // Change another default allocator.
  AllocatorPtr a1(new TestAllocator);
  AllocationManager::SetDefaultAllocatorForLifetime(kShortTerm, a1);
  EXPECT_EQ(a1, AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm));
  EXPECT_EQ(a0, AllocationManager::GetDefaultAllocatorForLifetime(kMediumTerm));
  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocatorForLifetime(kLongTerm));

  // Setting the default allocator to null should restore the MallocAllocator.
  AllocationManager::SetDefaultAllocatorForLifetime(kShortTerm,
                                                    AllocatorPtr(nullptr));
  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm));
}

TEST_F(AllocationManagerTest, DefaultAllocator) {
  const AllocatorPtr& ma = AllocationManager::GetMallocAllocator();
  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocator());

  AllocatorPtr a(new TestAllocator);
  AllocationManager::SetDefaultAllocatorForLifetime(kMediumTerm, a);
  EXPECT_EQ(a, AllocationManager::GetDefaultAllocator());

  AllocationManager::SetDefaultAllocationLifetime(kShortTerm);
  EXPECT_EQ(ma, AllocationManager::GetDefaultAllocator());
  AllocationManager::SetDefaultAllocatorForLifetime(kShortTerm, a);
  EXPECT_EQ(a, AllocationManager::GetDefaultAllocator());
}

}  // namespace base
}  // namespace ion
