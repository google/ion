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

#include "ion/base/allocator.h"

#include "ion/base/allocationmanager.h"
#include "ion/base/logchecker.h"
#include "ion/base/tests/testallocator.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace {

// A derived Allocator class used only for testing SetTracker()/GetTracker().
class DummyAllocator : public ion::base::Allocator {
 public:
  // Implementations don't matter - they will never be called.
  void* Allocate(size_t size) override { return nullptr; }
  void Deallocate(void* p) override {}
};

}  // anonymous namespace

TEST(Allocator, RefCount) {
  using ion::base::testing::TestAllocator;
  using ion::base::testing::TestAllocatorPtr;

  TestAllocator::ClearNumCreations();
  TestAllocator::ClearNumDeletions();

  {
    TestAllocatorPtr p;
  }
  EXPECT_EQ(0U, TestAllocator::GetNumCreations());
  EXPECT_EQ(0U, TestAllocator::GetNumDeletions());

  {
    TestAllocatorPtr p(new TestAllocator);
    EXPECT_EQ(1, p->GetRefCount());
    TestAllocatorPtr p2 = p;
    EXPECT_EQ(2, p->GetRefCount());
    EXPECT_EQ(1U, TestAllocator::GetNumCreations());
    EXPECT_EQ(0U, TestAllocator::GetNumDeletions());
  }
  EXPECT_EQ(1U, TestAllocator::GetNumCreations());
  EXPECT_EQ(1U, TestAllocator::GetNumDeletions());

  TestAllocator::ClearNumCreations();
  TestAllocator::ClearNumDeletions();
}

TEST(Allocator, GetAllocatorForLifetime) {
  using ion::base::AllocationManager;
  using ion::base::kShortTerm;
  using ion::base::kMediumTerm;
  using ion::base::kLongTerm;

  ion::base::testing::TestAllocatorPtr p(new ion::base::testing::TestAllocator);

  // The base Allocator class should return the same Allocators as the
  // AllocationManager.
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm),
            p->GetAllocatorForLifetime(kShortTerm));
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kMediumTerm),
            p->GetAllocatorForLifetime(kMediumTerm));
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kLongTerm),
            p->GetAllocatorForLifetime(kLongTerm));

  // Override Allocators.
  p->SetAllocatorForLifetime(kMediumTerm, p);
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm),
            p->GetAllocatorForLifetime(kShortTerm));
  EXPECT_EQ(p.Get(), p->GetAllocatorForLifetime(kMediumTerm).Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kLongTerm),
            p->GetAllocatorForLifetime(kLongTerm));

  p->SetAllocatorForLifetime(kShortTerm, p);
  EXPECT_EQ(p.Get(), p->GetAllocatorForLifetime(kShortTerm).Get());
  EXPECT_EQ(p.Get(), p->GetAllocatorForLifetime(kMediumTerm).Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kLongTerm),
            p->GetAllocatorForLifetime(kLongTerm));

  // p refs itself as a default allocator, so cleanup is necessary to avoid
  // a memory leak report.
  p->SetAllocatorForLifetime(kShortTerm, ion::base::AllocatorPtr());
  p->SetAllocatorForLifetime(kMediumTerm, ion::base::AllocatorPtr());
}

TEST(Allocator, Tracker) {
  ion::base::AllocatorPtr al(new DummyAllocator);
  EXPECT_FALSE(al->GetTracker());
  ion::base::AllocationTrackerPtr tr(new ion::base::FullAllocationTracker);
  al->SetTracker(tr);
  EXPECT_EQ(tr, al->GetTracker());
  al->SetTracker(ion::base::AllocationTrackerPtr());
  EXPECT_FALSE(al->GetTracker());
}
