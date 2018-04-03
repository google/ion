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

#include "ion/base/stlalloc/stlallocator.h"
#include "ion/base/stlalloc/allocdeque.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/base/stlalloc/allocset.h"
#include "ion/base/stlalloc/allocunorderedmap.h"
#include "ion/base/stlalloc/allocunorderedset.h"
#include "ion/base/stlalloc/allocvector.h"

#include "ion/base/allocationmanager.h"
#include "ion/base/logchecker.h"
#include "ion/base/referent.h"
#include "ion/base/tests/testallocator.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace {

// Used to exercise various constructors which take an "owner" argument.
class TestAllocatable : public ion::base::Allocatable {
 public:
  TestAllocatable() {}
};

// Used to test that allocated containers can properly handle reference counted
// objects.
class TestReferent : public ion::base::Referent {
 public:
  explicit TestReferent(int n) : val_(n) {}
  int GetValue() const { return val_; }
  static void ClearNumDestroys() { num_destroys_ = 0; }
  static int GetNumDestroys() { return num_destroys_; }
 private:
  TestReferent() : val_(0) {}
  ~TestReferent() override { ++num_destroys_; }
  int val_;
  static int num_destroys_;
};

int TestReferent::num_destroys_ = 0;

using TestReferentPtr = ion::base::SharedPtr<TestReferent>;

}  // anonymous namespace

TEST(StlAllocator, AllocVector) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocVector uses an StlAllocator wrapping the TestAllocator.
    ion::base::AllocVector<int> vec(a);
    vec.push_back(15);
    EXPECT_EQ(1U, vec.size());
    EXPECT_EQ(15, vec[0]);
    EXPECT_LE(2U, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(a.Get(), vec.get_allocator().GetAllocator().Get());

    // Add elements up to the current capacity and make sure no more
    // allocations were made.
    const size_t prev_capacity = vec.capacity();
    const size_t prev_allocated = a->GetNumAllocated();
    for (size_t i = vec.size(); i < prev_capacity; ++i)
      vec.push_back(111);
    EXPECT_EQ(prev_capacity, vec.size());
    EXPECT_EQ(15, vec[0]);
    EXPECT_EQ(prev_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());

    // Exceed the capacity and make sure at least one more allocation was made.
    vec.push_back(99);
    EXPECT_EQ(prev_capacity + 1, vec.size());
    EXPECT_EQ(15, vec[0]);
    EXPECT_EQ(99, vec[prev_capacity]);
    EXPECT_LT(prev_allocated, a->GetNumAllocated());

    // Exercise the other constructors.
    ion::base::AllocVector<int> vec1(a, vec);
    ion::base::AllocVector<int> vec2(a, vec.begin(), vec.end());
    ion::base::AllocVector<int> vec3(*(owner.get()), vec);
    ion::base::AllocVector<int> vec4(*(owner.get()), vec.begin(), vec.end());
    EXPECT_EQ(vec, vec1);
    EXPECT_EQ(vec1, vec2);
    EXPECT_EQ(vec2, vec3);
    EXPECT_EQ(vec3, vec4);
    EXPECT_EQ(a.Get(), vec1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec2.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec3.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec4.get_allocator().GetAllocator().Get());
  }

  // The AllocVectors have been destroyed, which should have deallocated memory.
  EXPECT_LE(6U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

TEST(StlAllocator, AllocVectorBool) {
  // Test that the bool specialization of std::vector works with AllocVector.
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocVector uses an StlAllocator wrapping the TestAllocator.
    ion::base::AllocVector<bool> vec(a);
    vec.push_back(false);
    EXPECT_EQ(1U, vec.size());
    EXPECT_FALSE(vec[0]);
    EXPECT_LE(2U, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(a.Get(), vec.get_allocator().GetAllocator().Get());

    // Add elements up to the current capacity and make sure no more
    // allocations were made.
    const size_t prev_capacity = vec.capacity();
    const size_t prev_allocated = a->GetNumAllocated();
    for (size_t i = vec.size(); i < prev_capacity; ++i)
      vec.push_back(true);
    EXPECT_EQ(prev_capacity, vec.size());
    EXPECT_FALSE(vec[0]);
    EXPECT_EQ(prev_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());

    // Exceed the capacity and make sure at least one more allocation was made.
    vec.push_back(true);
    EXPECT_EQ(prev_capacity + 1, vec.size());
    EXPECT_FALSE(vec[0]);
    EXPECT_TRUE(vec[prev_capacity]);
    EXPECT_LT(prev_allocated, a->GetNumAllocated());

    // Exercise the other constructors.
    ion::base::AllocVector<bool> vec1(a, vec);
    ion::base::AllocVector<bool> vec2(a, vec.begin(), vec.end());
    ion::base::AllocVector<bool> vec3(*(owner.get()), vec);
    ion::base::AllocVector<bool> vec4(*(owner.get()), vec.begin(), vec.end());
    EXPECT_EQ(vec, vec1);
    EXPECT_EQ(vec1, vec2);
    EXPECT_EQ(vec2, vec3);
    EXPECT_EQ(vec3, vec4);
    EXPECT_EQ(a.Get(), vec1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec2.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec3.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec4.get_allocator().GetAllocator().Get());
  }

  // The AllocVectors have been destroyed, which should have deallocated memory.
  EXPECT_LE(6U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

TEST(StlAllocator, InlinedAllocVector) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocVector uses an StlAllocator wrapping the TestAllocator.
    ion::base::InlinedAllocVector<int, 4> vec(a);
    const size_t initial_allocated = a->GetNumAllocated();

    vec.push_back(15);
    EXPECT_EQ(1U, vec.size());
    EXPECT_EQ(15, vec[0]);
    // Only the vector itself was allocated.
    EXPECT_EQ(initial_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(a.Get(), vec.get_allocator().GetAllocator().Get());

    vec.push_back(20);
    EXPECT_EQ(initial_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(15, vec[0]);
    EXPECT_EQ(20, vec[1]);

    vec.push_back(50);
    EXPECT_EQ(initial_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(15, vec[0]);
    EXPECT_EQ(20, vec[1]);
    EXPECT_EQ(50, vec[2]);

    vec.push_back(100);
    EXPECT_EQ(initial_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(15, vec[0]);
    EXPECT_EQ(20, vec[1]);
    EXPECT_EQ(50, vec[2]);
    EXPECT_EQ(100, vec[3]);

    // There has now been an allocation for the new space.
    vec.push_back(200);
    EXPECT_EQ(initial_allocated + 1, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(15, vec[0]);
    EXPECT_EQ(20, vec[1]);
    EXPECT_EQ(50, vec[2]);
    EXPECT_EQ(100, vec[3]);
    EXPECT_EQ(200, vec[4]);

    // Add elements up to the current capacity and make sure no more
    // allocations were made.
    const size_t prev_capacity = vec.capacity();
    const size_t prev_allocated = a->GetNumAllocated();
    for (size_t i = vec.size(); i < prev_capacity; ++i)
      vec.push_back(111);
    EXPECT_EQ(prev_capacity, vec.size());
    EXPECT_EQ(15, vec[0]);
    EXPECT_EQ(prev_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());

    // Exceed the capacity and make sure at least one more allocation was made.
    vec.push_back(99);
    EXPECT_EQ(prev_capacity + 1, vec.size());
    EXPECT_EQ(15, vec[0]);
    EXPECT_EQ(99, vec[prev_capacity]);
    EXPECT_LT(prev_allocated, a->GetNumAllocated());

    // Exercise the other constructors.
    ion::base::InlinedAllocVector<int, 4> vec1(a, vec);
    ion::base::InlinedAllocVector<int, 4> vec2(a, vec.begin(), vec.end());
    ion::base::InlinedAllocVector<int, 4> vec3(*(owner.get()), vec);
    ion::base::InlinedAllocVector<int, 4> vec4(*(owner.get()), vec.begin(),
                                               vec.end());
    EXPECT_EQ(vec, vec1);
    EXPECT_EQ(vec1, vec2);
    EXPECT_EQ(vec2, vec3);
    EXPECT_EQ(vec3, vec4);
    EXPECT_EQ(a.Get(), vec1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec2.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec3.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec4.get_allocator().GetAllocator().Get());
  }

  // The AllocVectors have been destroyed, which should have deallocated memory.
  EXPECT_LE(6U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

TEST(StlAllocator, InlinedAllocVectorOfReferentPtrs) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocVector uses an StlAllocator wrapping the TestAllocator.
    ion::base::InlinedAllocVector<TestReferentPtr, 4> vec(a);
    size_t num_allocated = a->GetNumAllocated();

    vec.push_back(TestReferentPtr(new TestReferent(15)));
    EXPECT_EQ(1U, vec.size());
    EXPECT_TRUE(vec[0].Get() != nullptr);
    EXPECT_EQ(15, vec[0]->GetValue());
    // Only the vector itself was allocated.
    EXPECT_EQ(num_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(a.Get(), vec.get_allocator().GetAllocator().Get());
    EXPECT_EQ(1, vec[0]->GetRefCount());

    vec.push_back(TestReferentPtr(new TestReferent(20)));
    EXPECT_EQ(num_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_TRUE(vec[0]);
    EXPECT_TRUE(vec[1]);
    EXPECT_EQ(15, vec[0]->GetValue());
    EXPECT_EQ(20, vec[1]->GetValue());
    EXPECT_EQ(1, vec[0]->GetRefCount());
    EXPECT_EQ(1, vec[1]->GetRefCount());

    vec.push_back(TestReferentPtr(new TestReferent(50)));
    EXPECT_EQ(num_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_TRUE(vec[0]);
    EXPECT_TRUE(vec[1]);
    EXPECT_TRUE(vec[2]);
    EXPECT_EQ(15, vec[0]->GetValue());
    EXPECT_EQ(20, vec[1]->GetValue());
    EXPECT_EQ(50, vec[2]->GetValue());
    EXPECT_EQ(1, vec[0]->GetRefCount());
    EXPECT_EQ(1, vec[1]->GetRefCount());
    EXPECT_EQ(1, vec[2]->GetRefCount());

    vec.push_back(TestReferentPtr(new TestReferent(100)));
    EXPECT_EQ(num_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_TRUE(vec[0]);
    EXPECT_TRUE(vec[1]);
    EXPECT_TRUE(vec[2]);
    EXPECT_TRUE(vec[3]);
    EXPECT_EQ(15, vec[0]->GetValue());
    EXPECT_EQ(20, vec[1]->GetValue());
    EXPECT_EQ(50, vec[2]->GetValue());
    EXPECT_EQ(100, vec[3]->GetValue());
    // Check the ref counts.
    EXPECT_EQ(1, vec[0]->GetRefCount());
    EXPECT_EQ(1, vec[1]->GetRefCount());
    EXPECT_EQ(1, vec[2]->GetRefCount());
    EXPECT_EQ(1, vec[3]->GetRefCount());

    // Check that copies work.
    ion::base::InlinedAllocVector<TestReferentPtr, 4> copy = vec;
    // Windows uses the allocator when allocating the vector itself, so we must
    // update the value.
    num_allocated = a->GetNumAllocated();
    EXPECT_EQ(num_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_TRUE(vec[0]);
    EXPECT_TRUE(vec[1]);
    EXPECT_TRUE(vec[2]);
    EXPECT_TRUE(vec[3]);
    EXPECT_EQ(15, vec[0]->GetValue());
    EXPECT_EQ(20, vec[1]->GetValue());
    EXPECT_EQ(50, vec[2]->GetValue());
    EXPECT_EQ(100, vec[3]->GetValue());
    EXPECT_EQ(num_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_TRUE(copy[0]);
    EXPECT_TRUE(copy[1]);
    EXPECT_TRUE(copy[2]);
    EXPECT_TRUE(copy[3]);
    EXPECT_EQ(15, copy[0]->GetValue());
    EXPECT_EQ(20, copy[1]->GetValue());
    EXPECT_EQ(50, copy[2]->GetValue());
    EXPECT_EQ(100, copy[3]->GetValue());

    // Check that popping and resizing works as expected.
    copy.pop_back();
    EXPECT_TRUE(copy[0]);
    EXPECT_TRUE(copy[1]);
    EXPECT_TRUE(copy[2]);
    copy.pop_back();
    EXPECT_TRUE(copy[0]);
    EXPECT_TRUE(copy[1]);
    copy.resize(1U);
    EXPECT_TRUE(copy[0]);
    // There are no deallocations since the original vector still holds refs.
    EXPECT_EQ(0U, a->GetNumDeallocated());

    // This should not destroy any of the existing elements.
    copy.push_back(TestReferentPtr(new TestReferent(20)));
    copy.push_back(TestReferentPtr(new TestReferent(50)));
    copy.push_back(TestReferentPtr(new TestReferent(100)));
    EXPECT_TRUE(copy[0]);
    EXPECT_TRUE(copy[1]);
    EXPECT_TRUE(copy[2]);
    EXPECT_TRUE(copy[3]);
    EXPECT_EQ(15, copy[0]->GetValue());
    EXPECT_EQ(20, copy[1]->GetValue());
    EXPECT_EQ(50, copy[2]->GetValue());
    EXPECT_EQ(100, copy[3]->GetValue());
    // The 0th element is shared by both vectors.
    EXPECT_EQ(2, copy[0]->GetRefCount());
    EXPECT_EQ(1, copy[1]->GetRefCount());
    EXPECT_EQ(1, copy[2]->GetRefCount());
    EXPECT_EQ(1, copy[3]->GetRefCount());

    copy.clear();
    EXPECT_EQ(num_allocated, a->GetNumAllocated());
    // There are no deallocations since the original vector still holds refs.
    EXPECT_EQ(0U, a->GetNumDeallocated());

    // None of this should have affected the original vector.
    EXPECT_TRUE(vec[0].Get() != nullptr);
    EXPECT_TRUE(vec[1].Get() != nullptr);
    EXPECT_TRUE(vec[2].Get() != nullptr);
    EXPECT_TRUE(vec[3].Get() != nullptr);
    EXPECT_EQ(15, vec[0]->GetValue());
    EXPECT_EQ(20, vec[1]->GetValue());
    EXPECT_EQ(50, vec[2]->GetValue());
    EXPECT_EQ(100, vec[3]->GetValue());
    EXPECT_EQ(1, vec[0]->GetRefCount());
    EXPECT_EQ(1, vec[1]->GetRefCount());
    EXPECT_EQ(1, vec[2]->GetRefCount());
    EXPECT_EQ(1, vec[3]->GetRefCount());

    // There has now been an allocation for the new space.
    vec.push_back(TestReferentPtr(new TestReferent(200)));
    EXPECT_EQ(num_allocated + 1, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_TRUE(vec[0]);
    EXPECT_TRUE(vec[1]);
    EXPECT_TRUE(vec[2]);
    EXPECT_TRUE(vec[3]);
    EXPECT_TRUE(vec[4]);
    EXPECT_EQ(15, vec[0]->GetValue());
    EXPECT_EQ(20, vec[1]->GetValue());
    EXPECT_EQ(50, vec[2]->GetValue());
    EXPECT_EQ(100, vec[3]->GetValue());
    EXPECT_EQ(200, vec[4]->GetValue());
    EXPECT_EQ(1, vec[0]->GetRefCount());
    EXPECT_EQ(1, vec[1]->GetRefCount());
    EXPECT_EQ(1, vec[2]->GetRefCount());
    EXPECT_EQ(1, vec[3]->GetRefCount());
    EXPECT_EQ(1, vec[4]->GetRefCount());

    // Test that popping elements calls their destructors.
    ion::base::InlinedAllocVector<TestReferentPtr, 4> pop_vec(a);
    pop_vec.push_back(TestReferentPtr(new TestReferent(15)));
    pop_vec.push_back(TestReferentPtr(new TestReferent(20)));
    pop_vec.push_back(TestReferentPtr(new TestReferent(50)));
    pop_vec.push_back(TestReferentPtr(new TestReferent(100)));
    TestReferent::ClearNumDestroys();
    pop_vec.pop_back();
    EXPECT_EQ(1, TestReferent::GetNumDestroys());
    pop_vec.pop_back();
    EXPECT_EQ(2, TestReferent::GetNumDestroys());
    pop_vec.pop_back();
    EXPECT_EQ(3, TestReferent::GetNumDestroys());
    pop_vec.pop_back();
    EXPECT_EQ(4, TestReferent::GetNumDestroys());

    // Add elements up to the current capacity and make sure no more
    // allocations were made.
    const size_t prev_capacity = vec.capacity();
    const size_t prev_allocated = a->GetNumAllocated();
    for (size_t i = vec.size(); i < prev_capacity; ++i)
      vec.push_back(TestReferentPtr(new TestReferent(111)));
    EXPECT_EQ(prev_capacity, vec.size());
    EXPECT_TRUE(vec[0]);
    EXPECT_EQ(15, vec[0]->GetValue());
    EXPECT_EQ(prev_allocated, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());

    // Exceed the capacity and make sure at least one more allocation was made.
    vec.push_back(TestReferentPtr(new TestReferent(99)));
    EXPECT_EQ(prev_capacity + 1, vec.size());
    EXPECT_TRUE(vec[0]);
    EXPECT_EQ(15, vec[0]->GetValue());
    EXPECT_TRUE(vec[prev_capacity]);
    EXPECT_EQ(99, vec[prev_capacity]->GetValue());
    EXPECT_LT(prev_allocated, a->GetNumAllocated());

    // Exercise the other constructors.
    ion::base::InlinedAllocVector<TestReferentPtr, 4> vec1(a, vec);
    ion::base::InlinedAllocVector<TestReferentPtr, 4> vec2(a, vec.begin(),
                                                           vec.end());
    ion::base::InlinedAllocVector<TestReferentPtr, 4> vec3(*(owner.get()), vec);
    ion::base::InlinedAllocVector<TestReferentPtr, 4> vec4(
        *(owner.get()), vec.begin(), vec.end());
    EXPECT_EQ(vec, vec1);
    EXPECT_EQ(vec1, vec2);
    EXPECT_EQ(vec2, vec3);
    EXPECT_EQ(vec3, vec4);
    EXPECT_EQ(a.Get(), vec1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec2.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec3.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), vec4.get_allocator().GetAllocator().Get());
  }

  // The AllocVectors have been destroyed, which should have deallocated memory.
  EXPECT_LE(6U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

TEST(StlAllocator, AllocDeque) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocDeque uses an StlAllocator wrapping the TestAllocator.
    ion::base::AllocDeque<int> q(a);
    q.push_back(102);
    q.push_back(103);
    q.push_back(104);
    EXPECT_EQ(3U, q.size());
    EXPECT_EQ(102, q.front());
    EXPECT_EQ(104, q.back());
    EXPECT_LE(2U, a->GetNumAllocated());
    EXPECT_EQ(a.Get(), q.get_allocator().GetAllocator().Get());

    // Exercise the other constructors.
    ion::base::AllocDeque<int> q1(a, q);
    ion::base::AllocDeque<int> q2(*(owner.get()), q);
    EXPECT_EQ(3U, q1.size());
    EXPECT_EQ(3U, q2.size());
    q1.pop_front();
    q1.pop_back();
    EXPECT_EQ(103, q1.front());
    EXPECT_EQ(103, q1.back());
    EXPECT_LE(4U, a->GetNumAllocated());
    EXPECT_EQ(a.Get(), q1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), q2.get_allocator().GetAllocator().Get());
  }

  // The AllocDeques have been destroyed, which should have deallocated memory.
  EXPECT_LE(4U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

TEST(StlAllocator, AllocSet) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocSet uses an StlAllocator wrapping the TestAllocator.
    ion::base::AllocSet<int> s(a);
    s.insert(102);
    s.insert(102);
    s.insert(103);
    EXPECT_EQ(2U, s.size());
    EXPECT_EQ(1U, s.count(102));
    EXPECT_LE(2U, a->GetNumAllocated());
    EXPECT_EQ(a.Get(), s.get_allocator().GetAllocator().Get());

    // Exercise the other constructors.
    ion::base::AllocSet<int> s1(a, s);
    ion::base::AllocSet<int> s2(*(owner.get()), s);
    EXPECT_EQ(2U, s1.size());
    EXPECT_EQ(2U, s2.size());
    EXPECT_LE(4U, a->GetNumAllocated());
    EXPECT_EQ(a.Get(), s1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), s2.get_allocator().GetAllocator().Get());
  }

  // The AllocSets have been destroyed, which should have deallocated memory.
  EXPECT_LE(4U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

TEST(StlAllocator, AllocMap) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocMap uses an StlAllocator wrapping the TestAllocator.
    ion::base::AllocMap<int, float> m(a);
    m[42] = 3.0f;
    EXPECT_EQ(1U, m.size());
    EXPECT_EQ(3.0f, m.find(42)->second);
    EXPECT_LE(2U, a->GetNumAllocated());
    EXPECT_EQ(0U, a->GetNumDeallocated());
    EXPECT_EQ(a.Get(), m.get_allocator().GetAllocator().Get());

    // Exercise the other constructors.
    ion::base::AllocMap<int, float> m1(a, m);
    ion::base::AllocMap<int, float> m2(*(owner.get()), m);
    EXPECT_EQ(1U, m1.size());
    EXPECT_EQ(1U, m2.size());
    EXPECT_EQ(3.0f, m1[42]);
    EXPECT_LE(4U, a->GetNumAllocated());
    EXPECT_EQ(a.Get(), m1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), m2.get_allocator().GetAllocator().Get());
  }

  // The AllocMaps have been destroyed, which should have deallocated memory.
  EXPECT_LE(4U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

TEST(StlAllocator, AllocUnorderedMap) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocUnorderedMap uses an StlAllocator wrapping the TestAllocator.
    ion::base::AllocUnorderedMap<int, float> m(a);
    m[42] = 3.0f;
    EXPECT_EQ(1U, m.size());
    EXPECT_EQ(3.0f, m.find(42)->second);
    EXPECT_LE(2U, a->GetNumAllocated());
    EXPECT_GE(1U, a->GetNumDeallocated());
    EXPECT_EQ(a.Get(), m.get_allocator().GetAllocator().Get());

    // Exercise the other constructors.
    ion::base::AllocUnorderedMap<int, float> m1(a, m);
    ion::base::AllocUnorderedMap<int, float> m2(*(owner.get()), m);
    EXPECT_EQ(1U, m1.size());
    EXPECT_EQ(1U, m2.size());
    EXPECT_EQ(3.0f, m1[42]);
    EXPECT_LE(4U, a->GetNumAllocated());
    EXPECT_EQ(a.Get(), m1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), m2.get_allocator().GetAllocator().Get());
  }

  // The AllocUnorderedMaps have been destroyed,
  // which should have deallocated memory.
  EXPECT_LE(4U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

TEST(StlAllocator, AllocUnorderedSet) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  EXPECT_EQ(0U, a->GetNumAllocated());
  EXPECT_EQ(0U, a->GetNumDeallocated());

  {
    // This Allocatable will be used in the "owner" constructors.
    std::unique_ptr<TestAllocatable> owner(new(a) TestAllocatable);
    EXPECT_EQ(1U, a->GetNumAllocated());

    // This AllocUnorderedSet uses an StlAllocator wrapping the TestAllocator.
    ion::base::AllocUnorderedSet<int> s(a);
    s.insert(102);
    EXPECT_EQ(1U, s.size());
    EXPECT_EQ(1U, s.count(102));
    EXPECT_LE(2U, a->GetNumAllocated());
    EXPECT_GE(1U, a->GetNumDeallocated());
    EXPECT_EQ(a.Get(), s.get_allocator().GetAllocator().Get());

    // Exercise the other constructors.
    ion::base::AllocUnorderedSet<int> s1(a, s);
    ion::base::AllocUnorderedSet<int> s2(*(owner.get()), s);
    EXPECT_EQ(1U, s1.size());
    EXPECT_EQ(1U, s2.size());
    EXPECT_LE(4U, a->GetNumAllocated());
    EXPECT_EQ(a.Get(), s1.get_allocator().GetAllocator().Get());
    EXPECT_EQ(a.Get(), s2.get_allocator().GetAllocator().Get());
  }

  // The AllocUnorderedSets have been destroyed,
  // which should have deallocated memory.
  EXPECT_LE(4U, a->GetNumDeallocated());
  EXPECT_EQ(a->GetNumAllocated(), a->GetNumDeallocated());
}

// Only StlAllocator objects with the same type and allocator are equal.
TEST(StlAllocator, Equality) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  ion::base::testing::TestAllocatorPtr b(new ion::base::testing::TestAllocator);

  ion::base::StlAllocator<int> stlallocator_int_a1(a);
  ion::base::StlAllocator<int> stlallocator_int_a2(a);
  ion::base::StlAllocator<int> stlallocator_int_b1(b);

  ion::base::StlAllocator<float> stlallocator_float_a1(a);
  ion::base::StlAllocator<float> stlallocator_float_a2(a);
  ion::base::StlAllocator<float> stlallocator_float_b1(b);

  // Same StlAllocator == equal.
  EXPECT_TRUE(stlallocator_int_a1 == stlallocator_int_a1);
  EXPECT_FALSE(stlallocator_int_a1 != stlallocator_int_a1);

  // Same type, same allocator == equal.
  EXPECT_TRUE(stlallocator_int_a1 == stlallocator_int_a2);
  EXPECT_TRUE(stlallocator_float_a1 == stlallocator_float_a2);
  EXPECT_FALSE(stlallocator_int_a1 != stlallocator_int_a2);
  EXPECT_FALSE(stlallocator_float_a1 != stlallocator_float_a2);

  // Same type, different allocator == not equal.
  EXPECT_TRUE(stlallocator_int_a1 != stlallocator_int_b1);
  EXPECT_TRUE(stlallocator_int_a1 != stlallocator_float_a1);
  EXPECT_FALSE(stlallocator_int_a1 == stlallocator_int_b1);
  EXPECT_FALSE(stlallocator_int_a1 == stlallocator_float_a1);

  // Different type, same allocator == not equal.
  EXPECT_TRUE(stlallocator_int_a1 != stlallocator_float_a1);
  EXPECT_FALSE(stlallocator_int_a1 == stlallocator_float_a1);
}

// StlInlinedAllocator is always not equal.
TEST(StlInlinedAllocator, Equality) {
  ion::base::testing::TestAllocatorPtr a(new ion::base::testing::TestAllocator);
  ion::base::StlInlinedAllocator<int, 5> alloc1(a);
  ion::base::StlInlinedAllocator<int, 5> alloc2(a);
  // Same StlInlinedAllocator == equal.
  EXPECT_TRUE(alloc1 == alloc1);
  EXPECT_FALSE(alloc1 != alloc1);
  // Anything else == not equal.
  EXPECT_FALSE(alloc1 == alloc2);
  EXPECT_TRUE(alloc1 != alloc2);
}
