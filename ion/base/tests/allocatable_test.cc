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

#include "ion/base/allocatable.h"

#include <memory>
#include <vector>

#include "ion/base/allocator.h"
#include "ion/base/datacontainer.h"
#include "ion/base/fullallocationtracker.h"
#include "ion/base/logchecker.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/base/stlalloc/allocset.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/base/stlalloc/stlallocator.h"
#include "ion/base/tests/testallocator.h"
#include "ion/port/nullptr.h"  // For kNullFunction.
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

// Simple derived Allocatable class that can test whether its constructor was
// invoked.
class ATest : public Allocatable {
 public:
  ATest() : value_(kSpecialNumber) {}
  ~ATest() override { value_ = 0; }
  bool WasConstructorCalled() const { return value_ == kSpecialNumber; }
 private:
  static const int kSpecialNumber = 123545435;
  int value_;
};

// Derived Allocatable class that contains an Allocatable that always uses the
// default allocator.
class ExplicitAllocatorTest : public Allocatable {
 public:
  ExplicitAllocatorTest()
      : contained_value_(new (AllocationManager::GetDefaultAllocator())
                             ATest()) {}
  ~ExplicitAllocatorTest() override {}
  ExplicitAllocatorTest(const ExplicitAllocatorTest& other)
      : intrusive_value_(other.intrusive_value_),
        contained_value_(new (AllocationManager::GetDefaultAllocator())
                             ATest()) {}
  void operator=(const ExplicitAllocatorTest& other) {
    intrusive_value_ = other.intrusive_value_;
    contained_value_.reset(new (AllocationManager::GetDefaultAllocator())
                               ATest());
  }
  const ATest& GetIntrusive() { return intrusive_value_; }
  const ATest& GetContained() { return *contained_value_; }

 private:
  ATest intrusive_value_;
  std::unique_ptr<ATest> contained_value_;
};

// Derived Allocatable class that contains an Allocatable that always uses the
// current allocator.
class StlAllocatorTest : public Allocatable {
 public:
  StlAllocatorTest() : contained_value_(new (GetAllocator()) ATest()) {}
  ~StlAllocatorTest() override {}
  StlAllocatorTest(const StlAllocatorTest& other)
      : intrusive_value_(other.intrusive_value_),
        contained_value_(new (GetAllocator()) ATest()) {}
  void operator=(const StlAllocatorTest& other) {
    intrusive_value_ = other.intrusive_value_;
    contained_value_.reset(new (GetAllocator()) ATest());
  }
  const ATest& GetIntrusive() { return intrusive_value_; }
  const ATest& GetContained() { return *contained_value_; }

 private:
  ATest intrusive_value_;
  std::unique_ptr<ATest> contained_value_;
};

// Simple derived Allocatable class that can test whether its copy constructor
// was invoked.
class CopyTest : public Allocatable {
 public:
  CopyTest() : value_(kSpecialNumber), was_copied_(false) {}
  CopyTest(const CopyTest& other)
      : Allocatable(other), value_(other.value_), was_copied_(true) {}
  ~CopyTest() override { value_ = 0; }
  bool WasConstructorCalled() const { return value_ == kSpecialNumber; }
  bool WasCopyConstructed() const { return was_copied_; }
 private:
  static const int kSpecialNumber = 123545435;
  int value_;
  bool was_copied_;
};

// Simple derived Allocatable class that can only be created on the stack.
class StackTest : public Allocatable {
 public:
  StackTest()
      : Allocatable(AllocationManager::GetDefaultAllocator()),
        value_(kSpecialNumber) {}
  ~StackTest() override { value_ = 0; }
  bool WasConstructorCalled() const { return value_ == kSpecialNumber; }
  // Override operator delete to use the default allocator since this should
  // only be stack allocated. This prevents the allocator from searching for a
  // non-existent allocation data struct, which would DCHECK and then
  // dereference NULL. This is only needed so that we can test for the DCHECK in
  // the Allocatable constructor but not leak memory by not deleting the
  // instance.
  void operator delete(void* ptr) {
    AllocationManager::GetDefaultAllocator()->DeallocateMemory(ptr);
  }

 private:
  static const int kSpecialNumber = 123545435;
  int value_;
};

// Derived Allocatable class with unwrapped STL data members.
class StlTest : public Allocatable {
 public:
  StlTest() : vec_(StlAllocator<int>(GetAllocator())) {}
  ~StlTest() override {}
  // Resizes the vector to force allocation.
  void ResizeVector(size_t size) { vec_.resize(size); }
 private:
  // STL vector that should use the same allocator as this.
  std::vector<int, StlAllocator<int> > vec_;
};

// Derived Allocatable class with public STL wrapper data members.
struct StlWrapperTest : public Allocatable {
 public:
  StlWrapperTest()
      : alloc_map(*this),
        alloc_set(*this),
        alloc_vec(*this) {}
  ~StlWrapperTest() override {}
  // STL members that should use the same allocator as this.
  AllocMap<int, float> alloc_map;
  AllocSet<int> alloc_set;
  AllocVector<int> alloc_vec;
};

// These structs are used for testing nested allocations.
class Nested1 : public Allocatable {
 public:
  Nested1() {}
};
class Nested2 : public Nested1 {
 public:
  explicit Nested2(Nested1* n1) : n1_(n1) {}
 private:
  std::unique_ptr<Nested1> n1_;
};
class Nested3 : public Nested2 {
 public:
  Nested3() : Nested2(new Nested1) {}
};

// Defines a test framework that installs a TrackingAllocator in the
// AllocationManager for all allocations so that errors can be detected.
// It also ensures that the proper allocators are installed in the
// AllocationManager on teardown so that the state is consistent.
class AllocatableTest : public ::testing::Test {
 protected:
  void SetUp() override {
    saved_[0] = AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm);
    saved_[1] = AllocationManager::GetDefaultAllocatorForLifetime(kMediumTerm);
    saved_[2] = AllocationManager::GetDefaultAllocatorForLifetime(kLongTerm);

    testing::TestAllocatorPtr ta(new testing::TestAllocator());
    AllocationManager::SetDefaultAllocatorForLifetime(kShortTerm, ta);
    AllocationManager::SetDefaultAllocatorForLifetime(kMediumTerm, ta);
    AllocationManager::SetDefaultAllocatorForLifetime(kLongTerm, ta);
  }
  void TearDown() override {
    // Restore the previous allocator instances. This should also cause the
    // TrackingAllocator to be deleted, which will report errors for any active
    // allocations.
    AllocationManager::SetDefaultAllocatorForLifetime(kShortTerm, saved_[0]);
    AllocationManager::SetDefaultAllocatorForLifetime(kMediumTerm, saved_[1]);
    AllocationManager::SetDefaultAllocatorForLifetime(kLongTerm, saved_[2]);
  }
 private:
  AllocatorPtr saved_[3];
};

}  // anonymous namespace

// Simple class for testing that trivially-copyable types do not set a placement
// allocator. This is outside of the anonymous namespace since it must be a
// friend of Allocatable.
class AllocatableTest_TrivialType_Test {
 public:
  AllocatableTest_TrivialType_Test()
      : placement_allocator_set_(Allocatable::GetPlacementAllocator() !=
                                 nullptr) {
  }
  bool WasPlacementAllocatorSet() { return placement_allocator_set_; }

 private:
  bool placement_allocator_set_;
};

TEST_F(AllocatableTest, StackAllocation) {
  // Allocating on stack should work with no problems for ATest.
  ATest a0;
  ATest a1;
  EXPECT_TRUE(a0.WasConstructorCalled());
  EXPECT_TRUE(a1.WasConstructorCalled());
  EXPECT_FALSE(a0.GetAllocator());
  EXPECT_FALSE(a1.GetAllocator());

  // Allocating StackTest should also work.
  StackTest s0;
  StackTest s1;
  EXPECT_TRUE(s0.WasConstructorCalled());
  EXPECT_TRUE(s1.WasConstructorCalled());
  EXPECT_TRUE(s0.GetAllocator().Get() ==
              AllocationManager::GetDefaultAllocator().Get());
  EXPECT_TRUE(s1.GetAllocator().Get() ==
              AllocationManager::GetDefaultAllocator().Get());

  // Newing a StackTest should DCHECK.
  {
// Running this test without the DCHECK corrupts the thread's allocation helper
// so it has to be disabled entirely.
#if !ION_PRODUCTION
    {
      EXPECT_DEATH_IF_SUPPORTED(std::unique_ptr<StackTest>(new StackTest()),
                                "only when created on the stack");
    }
#endif
  }
}

TEST_F(AllocatableTest, DefaultAllocation) {
  // These allocations use the default allocator.
  std::unique_ptr<ATest> a0(new ATest);
  std::unique_ptr<ATest> a1(new ATest);
  EXPECT_TRUE(a0->WasConstructorCalled());
  EXPECT_TRUE(a1->WasConstructorCalled());

  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            a0->GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            a1->GetAllocator().Get());
}

TEST_F(AllocatableTest, PlacementAllocation) {
  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  void* m0 = allocator->AllocateMemory(sizeof(ATest));
  void* m1 = allocator->AllocateMemory(sizeof(ATest));

  // These allocations use the default allocator.
  std::unique_ptr<ATest> a0(new(allocator, m0) ATest);
  std::unique_ptr<ATest> a1(new(allocator, m1) ATest);
  EXPECT_TRUE(a0->WasConstructorCalled());
  EXPECT_TRUE(a1->WasConstructorCalled());

  EXPECT_EQ(allocator.Get(), a0->GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), a1->GetAllocator().Get());
}

TEST_F(AllocatableTest, CopyConstruction) {
  // These allocations use the default allocator.
  std::unique_ptr<CopyTest> c0(new CopyTest);
  std::unique_ptr<CopyTest> c1(new CopyTest);
  CopyTest c2(*c0);
  EXPECT_TRUE(c0->WasConstructorCalled());
  EXPECT_TRUE(c1->WasConstructorCalled());
  EXPECT_TRUE(c2.WasConstructorCalled());

  EXPECT_FALSE(c0->WasCopyConstructed());
  EXPECT_FALSE(c1->WasCopyConstructed());
  EXPECT_TRUE(c2.WasCopyConstructed());

  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            c0->GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            c1->GetAllocator().Get());
  // Since c2 was allocated on the stack, it should have a NULL allocator.
  EXPECT_FALSE(c2.GetAllocator());

  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  std::unique_ptr<CopyTest> c3(new(allocator) CopyTest(*c1));
  std::unique_ptr<CopyTest> c4(new(allocator) CopyTest(c2));
  EXPECT_EQ(2U, allocator->GetNumAllocated());

  EXPECT_TRUE(c3->WasConstructorCalled());
  EXPECT_TRUE(c4->WasConstructorCalled());
  EXPECT_TRUE(c3->WasCopyConstructed());
  EXPECT_TRUE(c4->WasCopyConstructed());
  EXPECT_EQ(allocator.Get(), c3->GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), c4->GetAllocator().Get());
}

TEST_F(AllocatableTest, Assignment) {
  // These allocations use the default allocator.
  std::unique_ptr<CopyTest> c0(new CopyTest);
  std::unique_ptr<CopyTest> c1(new CopyTest);
  CopyTest c2(*c0);
  EXPECT_TRUE(c0->WasConstructorCalled());
  EXPECT_TRUE(c1->WasConstructorCalled());
  EXPECT_TRUE(c2.WasConstructorCalled());

  EXPECT_FALSE(c0->WasCopyConstructed());
  EXPECT_FALSE(c1->WasCopyConstructed());
  EXPECT_TRUE(c2.WasCopyConstructed());

  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            c0->GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            c1->GetAllocator().Get());
  // Since c2 was allocated on the stack, it should have a NULL allocator.
  EXPECT_FALSE(c2.GetAllocator());

  // Copying instances will copy their internal boolean value, but not their
  // allocators.
  *c0 = c2;
  EXPECT_TRUE(c0->WasCopyConstructed());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            c0->GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            c1->GetAllocator().Get());
  EXPECT_FALSE(c2.GetAllocator());

  c2 = *c1;
  EXPECT_FALSE(c2.WasCopyConstructed());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            c0->GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            c1->GetAllocator().Get());
  EXPECT_FALSE(c2.GetAllocator());
}

TEST_F(AllocatableTest, StlMap) {
  std::map<int, ATest> a_container;
  EXPECT_FALSE(a_container[0].GetAllocator());
  EXPECT_FALSE(a_container[1].GetAllocator());

  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  AllocMap<int, ATest> b_container(allocator);
  EXPECT_TRUE(b_container[0].WasConstructorCalled());
  EXPECT_TRUE(b_container[1].WasConstructorCalled());
  EXPECT_EQ(allocator.Get(), b_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), b_container[1].GetAllocator().Get());

  // Check assignment of an element in an STL container.
  a_container[0] = b_container[0];
  b_container[1] = a_container[1];
  EXPECT_FALSE(a_container[0].GetAllocator());
  EXPECT_FALSE(a_container[1].GetAllocator());
  EXPECT_EQ(allocator.Get(), b_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), b_container[1].GetAllocator().Get());

  // This map will use allocator of the source map.
  AllocMap<int, ATest> c_container = b_container;
  EXPECT_EQ(allocator.Get(), c_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), c_container[1].GetAllocator().Get());

  testing::TestAllocatorPtr allocator2(new testing::TestAllocator);
  // This map will always use the allocator passed to the constructor.
  AllocMap<int, ATest> d_container(allocator2, b_container);
  EXPECT_EQ(allocator2.Get(), d_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(), d_container[1].GetAllocator().Get());

  // This map will use the default allocator for its elements.
  AllocMap<int, ATest> e_container(AllocatorPtr(), a_container);
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            e_container[0].GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            e_container[1].GetAllocator().Get());

  // This map will also use the default allocator.
  AllocMap<int, ATest> f_container = e_container;
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            f_container[0].GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            f_container[1].GetAllocator().Get());

  // Assignment should not change allocators.
  b_container = e_container;
  EXPECT_EQ(allocator.Get(), b_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), b_container[1].GetAllocator().Get());
  d_container = b_container;
  EXPECT_EQ(allocator2.Get(), d_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(), d_container[1].GetAllocator().Get());

  // Try some internal allocations.
  AllocMap<int, ExplicitAllocatorTest> explicit_container(allocator2);
  EXPECT_EQ(allocator2.Get(), explicit_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(), explicit_container[1].GetAllocator().Get());
  EXPECT_TRUE(explicit_container[0].GetIntrusive().WasConstructorCalled());
  EXPECT_TRUE(explicit_container[1].GetIntrusive().WasConstructorCalled());
  EXPECT_TRUE(explicit_container[0].GetContained().WasConstructorCalled());
  EXPECT_TRUE(explicit_container[1].GetContained().WasConstructorCalled());
  // The intrusive member should have the same allocator as the owning instance,
  // which is the container's allocator.
  EXPECT_EQ(allocator2.Get(),
            explicit_container[0].GetIntrusive().GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(),
            explicit_container[1].GetIntrusive().GetAllocator().Get());
  // The contained member should use whatever it was new'd with, which was the
  // default allocator.
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            explicit_container[0].GetContained().GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            explicit_container[1].GetContained().GetAllocator().Get());

  AllocMap<int, StlAllocatorTest> stl_container(allocator2);
  EXPECT_EQ(allocator2.Get(), stl_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(), stl_container[1].GetAllocator().Get());
  EXPECT_TRUE(stl_container[0].GetIntrusive().WasConstructorCalled());
  EXPECT_TRUE(stl_container[1].GetIntrusive().WasConstructorCalled());
  EXPECT_TRUE(stl_container[0].GetContained().WasConstructorCalled());
  EXPECT_TRUE(stl_container[1].GetContained().WasConstructorCalled());
  // The intrusive member should have the same allocator as the owning instance,
  // which is the container's allocator.
  EXPECT_EQ(allocator2.Get(),
            stl_container[0].GetIntrusive().GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(),
            stl_container[1].GetIntrusive().GetAllocator().Get());
  // The contained member should use whatever it was new'd with, which was the
  // owning instance's allocator.
  EXPECT_EQ(allocator2.Get(),
            stl_container[0].GetContained().GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(),
            stl_container[1].GetContained().GetAllocator().Get());

  // Check that an STL container that contains a trivial type does not set the
  // placement Allocator.
  AllocMap<int, AllocatableTest_TrivialType_Test> trivial(allocator);
  EXPECT_FALSE(trivial[0].WasPlacementAllocatorSet());
  EXPECT_FALSE(trivial[1].WasPlacementAllocatorSet());
}

TEST_F(AllocatableTest, StlVector) {
  std::vector<ATest> a_container;
  a_container.resize(2);
  EXPECT_TRUE(a_container[0].WasConstructorCalled());
  EXPECT_TRUE(a_container[1].WasConstructorCalled());

  EXPECT_FALSE(a_container[0].GetAllocator());
  EXPECT_FALSE(a_container[1].GetAllocator());

  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  AllocVector<ATest> b_container(allocator);
  b_container.resize(2);
  EXPECT_TRUE(b_container[0].WasConstructorCalled());
  EXPECT_TRUE(b_container[1].WasConstructorCalled());
  EXPECT_EQ(allocator.Get(), b_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), b_container[1].GetAllocator().Get());

  // Check assignment of an element in an STL container.
  a_container[0] = b_container[0];
  b_container[1] = a_container[1];
  EXPECT_FALSE(a_container[0].GetAllocator());
  EXPECT_FALSE(a_container[1].GetAllocator());
  EXPECT_EQ(allocator.Get(), b_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), b_container[1].GetAllocator().Get());

  // This vector will use allocator of the source vector.
  AllocVector<ATest> c_container = b_container;
  EXPECT_EQ(allocator.Get(), c_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), c_container[1].GetAllocator().Get());

  testing::TestAllocatorPtr allocator2(new testing::TestAllocator);
  // This vector will always use the allocator passed to the constructor.
  AllocVector<ATest> d_container(allocator2, b_container.begin(),
                                 b_container.end());
  EXPECT_EQ(allocator2.Get(), d_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(), d_container[1].GetAllocator().Get());

  // This vector will use the default allocator for its elements.
  AllocVector<ATest> e_container(AllocatorPtr(), a_container.begin(),
                                 a_container.end());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            e_container[0].GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            e_container[1].GetAllocator().Get());

  // This vector will also use the default allocator.
  AllocVector<ATest> f_container = e_container;
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            f_container[0].GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            f_container[1].GetAllocator().Get());

  // Assignment should not change allocators.
  b_container = e_container;
  EXPECT_EQ(allocator.Get(), b_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator.Get(), b_container[1].GetAllocator().Get());
  d_container = b_container;
  EXPECT_EQ(allocator2.Get(), d_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(), d_container[1].GetAllocator().Get());

  // Try some internal allocations.
  AllocVector<ExplicitAllocatorTest> explicit_container(allocator2);
  explicit_container.resize(2);
  EXPECT_EQ(allocator2.Get(), explicit_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(), explicit_container[1].GetAllocator().Get());
  EXPECT_TRUE(explicit_container[0].GetIntrusive().WasConstructorCalled());
  EXPECT_TRUE(explicit_container[1].GetIntrusive().WasConstructorCalled());
  EXPECT_TRUE(explicit_container[0].GetContained().WasConstructorCalled());
  EXPECT_TRUE(explicit_container[1].GetContained().WasConstructorCalled());
  // The intrusive member should have the same allocator as the owning instance,
  // which is the container's allocator.
  EXPECT_EQ(allocator2.Get(),
            explicit_container[0].GetIntrusive().GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(),
            explicit_container[1].GetIntrusive().GetAllocator().Get());
  // The contained member should use whatever it was new'd with, which was the
  // default allocator.
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            explicit_container[0].GetContained().GetAllocator().Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            explicit_container[1].GetContained().GetAllocator().Get());

  AllocVector<StlAllocatorTest> stl_container(allocator2);
  stl_container.resize(2);
  EXPECT_EQ(allocator2.Get(), stl_container[0].GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(), stl_container[1].GetAllocator().Get());
  EXPECT_TRUE(stl_container[0].GetIntrusive().WasConstructorCalled());
  EXPECT_TRUE(stl_container[1].GetIntrusive().WasConstructorCalled());
  EXPECT_TRUE(stl_container[0].GetContained().WasConstructorCalled());
  EXPECT_TRUE(stl_container[1].GetContained().WasConstructorCalled());
  // The intrusive member should have the same allocator as the owning instance,
  // which is the container's allocator.
  EXPECT_EQ(allocator2.Get(),
            stl_container[0].GetIntrusive().GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(),
            stl_container[1].GetIntrusive().GetAllocator().Get());
  // The contained member should use whatever it was new'd with, which was the
  // owning instance's allocator.
  EXPECT_EQ(allocator2.Get(),
            stl_container[0].GetContained().GetAllocator().Get());
  EXPECT_EQ(allocator2.Get(),
            stl_container[1].GetContained().GetAllocator().Get());

  // Check that an STL container that contains a trivial type does not set the
  // placement Allocator.
  AllocVector<AllocatableTest_TrivialType_Test> trivial(allocator);
  trivial.resize(2);
  EXPECT_FALSE(trivial[0].WasPlacementAllocatorSet());
  EXPECT_FALSE(trivial[1].WasPlacementAllocatorSet());
}

TEST_F(AllocatableTest, CustomAllocation) {
  testing::TestAllocatorPtr allocator0(new testing::TestAllocator);
  testing::TestAllocatorPtr allocator1(new testing::TestAllocator);
  EXPECT_EQ(0U, allocator0->GetNumAllocated());
  EXPECT_EQ(0U, allocator1->GetNumAllocated());
  EXPECT_EQ(0U, allocator0->GetNumDeallocated());
  EXPECT_EQ(0U, allocator1->GetNumDeallocated());

  ATest* a0 = new (allocator0) ATest;
  EXPECT_EQ(1U, allocator0->GetNumAllocated());
  EXPECT_EQ(sizeof(ATest), allocator0->GetBytesAllocated());
  ATest* a1 = new (allocator1) ATest;
  EXPECT_EQ(1U, allocator1->GetNumAllocated());
  EXPECT_EQ(sizeof(ATest), allocator1->GetBytesAllocated());
  EXPECT_EQ(0U, allocator0->GetNumDeallocated());
  EXPECT_EQ(0U, allocator1->GetNumDeallocated());

  EXPECT_EQ(allocator0.Get(), a0->GetAllocator().Get());
  EXPECT_EQ(allocator1.Get(), a1->GetAllocator().Get());

  delete a0;
  EXPECT_EQ(1U, allocator0->GetNumAllocated());
  EXPECT_EQ(1U, allocator1->GetNumAllocated());
  EXPECT_EQ(1U, allocator0->GetNumDeallocated());
  EXPECT_EQ(0U, allocator1->GetNumDeallocated());
  delete a1;
  EXPECT_EQ(1U, allocator0->GetNumAllocated());
  EXPECT_EQ(1U, allocator1->GetNumAllocated());
  EXPECT_EQ(1U, allocator0->GetNumDeallocated());
  EXPECT_EQ(1U, allocator1->GetNumDeallocated());
  EXPECT_EQ(sizeof(ATest), allocator0->GetBytesAllocated());
  EXPECT_EQ(sizeof(ATest), allocator1->GetBytesAllocated());
}

TEST_F(AllocatableTest, AllocationByLifetime) {
  const AllocationLifetime lifetime = kLongTerm;

  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  EXPECT_EQ(0U, allocator->GetNumAllocated());
  EXPECT_EQ(0U, allocator->GetNumDeallocated());

  AllocationManager::SetDefaultAllocatorForLifetime(lifetime, allocator);
  ATest* a = new (lifetime) ATest;
  delete a;
  EXPECT_EQ(1U, allocator->GetNumAllocated());
  EXPECT_EQ(1U, allocator->GetNumDeallocated());
}

TEST_F(AllocatableTest, GetAllocatorForLifetime) {
  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  std::unique_ptr<ATest> a(new (allocator) ATest);

  // Should use default allocators by default.
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm),
            a->GetAllocatorForLifetime(kShortTerm));
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kMediumTerm),
            a->GetAllocatorForLifetime(kMediumTerm));
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kShortTerm),
            a->GetAllocatorForLifetime(kShortTerm));

  // Install allocator as the correct allocator for one lifetime.
  allocator->SetAllocatorForLifetime(kShortTerm, allocator);
  EXPECT_EQ(allocator.Get(), a->GetAllocatorForLifetime(kShortTerm).Get());
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kMediumTerm),
            a->GetAllocatorForLifetime(kMediumTerm));
  EXPECT_EQ(AllocationManager::GetDefaultAllocatorForLifetime(kLongTerm),
            a->GetAllocatorForLifetime(kLongTerm));

  // Try the other two.
  allocator->SetAllocatorForLifetime(kMediumTerm, allocator);
  allocator->SetAllocatorForLifetime(kLongTerm, allocator);
  EXPECT_EQ(allocator.Get(), a->GetAllocatorForLifetime(kShortTerm).Get());
  EXPECT_EQ(allocator.Get(), a->GetAllocatorForLifetime(kMediumTerm).Get());
  EXPECT_EQ(allocator.Get(), a->GetAllocatorForLifetime(kLongTerm).Get());

  // Clean up to avoid circular reference leak
  allocator->SetAllocatorForLifetime(kShortTerm, AllocatorPtr());
  allocator->SetAllocatorForLifetime(kMediumTerm, AllocatorPtr());
  allocator->SetAllocatorForLifetime(kLongTerm, AllocatorPtr());
}

TEST_F(AllocatableTest, StlAllocation) {
  testing::TestAllocatorPtr allocator0(new testing::TestAllocator);
  testing::TestAllocatorPtr allocator1(new testing::TestAllocator);

  StlTest* sm = new StlTest;
  StlTest* s0 = new (allocator0) StlTest;
  StlTest* s1 = new (allocator1) StlTest;
  EXPECT_EQ(AllocationManager::GetDefaultAllocator().Get(),
            sm->GetAllocator().Get());
  delete sm;
  EXPECT_EQ(allocator0.Get(), s0->GetAllocator().Get());
  EXPECT_EQ(allocator1.Get(), s1->GetAllocator().Get());

  // Should be 1 allocation for the instance.
  //
  // Note that debug-mode STL on Windows performs an extra allocation of a
  // proxy object for every STL allocator. Therefore, all of the allocation
  // checks here use EXPECT_LE rather than EXPECT_EQ.
  EXPECT_LE(1U, allocator0->GetNumAllocated());
  EXPECT_LE(1U, allocator1->GetNumAllocated());
  EXPECT_LE(sizeof(StlTest), allocator0->GetBytesAllocated());
  EXPECT_LE(sizeof(StlTest), allocator1->GetBytesAllocated());

  // Resize the vector, causing reallocation,
  s0->ResizeVector(100U);
  EXPECT_LE(2U, allocator0->GetNumAllocated());
  EXPECT_LE(sizeof(StlTest) + 100U * sizeof(int),
            allocator0->GetBytesAllocated());
  EXPECT_LE(1U, allocator1->GetNumAllocated());

  s1->ResizeVector(100U);
  EXPECT_LE(2U, allocator1->GetNumAllocated());
  EXPECT_LE(sizeof(StlTest) + 100U * sizeof(int),
            allocator1->GetBytesAllocated());

  // There is no way to guarantee that the vector deallocates any memory except
  // by deleting it. Deleting the StlTest instance should cause a deletion for
  // the instance and for the vector's memory.
  EXPECT_EQ(0U, allocator0->GetNumDeallocated());
  EXPECT_EQ(0U, allocator1->GetNumDeallocated());
  delete s0;
  EXPECT_LE(2U, allocator0->GetNumDeallocated());
  EXPECT_EQ(0U, allocator1->GetNumDeallocated());
  delete s1;
  EXPECT_LE(2U, allocator0->GetNumDeallocated());
  EXPECT_LE(2U, allocator1->GetNumDeallocated());
}

TEST_F(AllocatableTest, StlWrappedAllocation) {
  // Similar to the above test, but uses StlWrapperTest, which employs the STL
  // container wrappers.
  testing::TestAllocatorPtr allocator(new testing::TestAllocator);

  StlWrapperTest* s = new (allocator) StlWrapperTest;
  EXPECT_EQ(allocator.Get(), s->GetAllocator().Get());

  // Should be 1 allocation for the instance.
  //
  // Note that debug-mode STL on Windows performs an extra allocation of a
  // proxy object for every STL allocator. Therefore, all of the allocation
  // checks here use EXPECT_LE rather than EXPECT_EQ.
  EXPECT_LE(1U, allocator->GetNumAllocated());
  EXPECT_LE(sizeof(StlWrapperTest), allocator->GetBytesAllocated());

  // Cause allocation in all of the members.
  s->alloc_map[3] = 12.1f;
  s->alloc_set.insert(16);
  s->alloc_vec.resize(100U);
  EXPECT_LE(4U, allocator->GetNumAllocated());

  // Deleting the StlWrapperTest instance should cause a deletion for the
  // instance and for the 3 wrapped members' memory.
  EXPECT_EQ(0U, allocator->GetNumDeallocated());
  delete s;
  EXPECT_LE(4U, allocator->GetNumDeallocated());
}

TEST_F(AllocatableTest, StlWrappedAllocationCopy) {
  // Test constructors that copy values from standard containers.
  std::vector<int> v;
  v.push_back(14);
  v.push_back(97);
  std::set<int> s;
  s.insert(4);
  s.insert(15);
  std::map<int, float> m;
  m[31] = 15.5f;
  m[6] = 12.0f;

  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  std::unique_ptr<StlWrapperTest> t(new (allocator) StlWrapperTest);
  EXPECT_LE(1U, allocator->GetNumAllocated());

  AllocVector<int> av(*t, v);
  EXPECT_LE(2U, allocator->GetNumAllocated());
  EXPECT_EQ(2U, av.size());
  EXPECT_EQ(14, av[0]);
  EXPECT_EQ(97, av[1]);

  AllocSet<int> as(*t, s);
  EXPECT_LE(3U, allocator->GetNumAllocated());
  EXPECT_EQ(2U, as.size());
  EXPECT_EQ(1U, as.count(4));
  EXPECT_EQ(1U, as.count(15));

  AllocMap<int, float> am(*t, m);
  EXPECT_LE(4U, allocator->GetNumAllocated());
  EXPECT_EQ(2U, am.size());
  EXPECT_EQ(15.5f, am.find(31)->second);
  EXPECT_EQ(12.0f, am.find(6)->second);
}

TEST_F(AllocatableTest, ListInitalization) {
  testing::TestAllocatorPtr allocator(new testing::TestAllocator);
  std::unique_ptr<StlWrapperTest> t(new (allocator) StlWrapperTest);
  EXPECT_LE(1U, allocator->GetNumAllocated());

  AllocSet<int> as(*t, {1, 2});
  EXPECT_LE(2U, allocator->GetNumAllocated());
  EXPECT_EQ(2U, as.size());
  EXPECT_EQ(1U, as.count(1));
  EXPECT_EQ(1U, as.count(2));

  AllocVector<int> av(*t, {10, 20, 30});
  EXPECT_LE(3U, allocator->GetNumAllocated());
  EXPECT_EQ(3U, av.size());
  EXPECT_EQ(10, av[0]);
  EXPECT_EQ(20, av[1]);
  EXPECT_EQ(30, av[2]);

  AllocMap<int, float> am(*t, { {1, 10.0f }, { 2, 20.0f } });
  EXPECT_LE(4U, allocator->GetNumAllocated());
  EXPECT_EQ(2U, am.size());
  EXPECT_EQ(10.0f, am.find(1)->second);
  EXPECT_EQ(20.0f, am.find(2)->second);
}

TEST_F(AllocatableTest, NestedAllocation) {
  // This exercises the condition of having more than one construction at the
  // same time, because there are allocations and constructor calls that occur
  // between the call to operator new for Nested3 and its constructor.
  std::unique_ptr<Nested3> n3(new Nested3);
}

TEST_F(AllocatableTest, NonNullAllocator) {
  // Test GetNonNullAllocator().
  AllocatorPtr default_allocator = AllocationManager::GetDefaultAllocator();
  AllocatorPtr allocator(new testing::TestAllocator);
  std::unique_ptr<ATest> heap_alloced(new(allocator) ATest);
  ATest stack_alloced;
  EXPECT_TRUE(heap_alloced->GetAllocator());
  EXPECT_NE(default_allocator.Get(), heap_alloced->GetAllocator().Get());

  EXPECT_FALSE(stack_alloced.GetAllocator());
  EXPECT_TRUE(stack_alloced.GetNonNullAllocator());
  EXPECT_EQ(default_allocator.Get(),
            stack_alloced.GetNonNullAllocator().Get());
}

TEST_F(AllocatableTest, DestroyAllocator) {
  // Create the Allocator.
  AllocatorPtr allocator(new testing::TestAllocator);
  // Create an Atest that uses it.
  ATest* a = new (allocator) ATest;
  // Reset the Allocator pointer so that the ATest holds the last reference to
  // the Allocator.
  allocator = nullptr;
  // Delete the ATest. This should not crash; the Allocator should not be
  // destroyed before the ATest instance is completely deleted.
  delete a;
}

// Referent which allows placement new creation within a DataContainer for
// performance.
class ContainerReferent
// NOTE: DataContainerPtr must be inherited first so it is destructed last
// because ~DataContainerPtr might delete the memory containing this object.
    : private DataContainerPtr,
      public Referent {
 public:
  const DataContainerPtr& GetDataContainer() const { return *this; }

 protected:
  ContainerReferent() {}
  explicit ContainerReferent(const DataContainerPtr& data)
      : ion::base::DataContainerPtr(data),
        Referent(data->GetAllocator()) {
  }
  ~ContainerReferent() override {}
  void OnZeroRefCount() const override {
    // If this object is within a DataContainer, manually invoke destructor.
    if (Get())
      this->~ContainerReferent();
    else
      delete this;
  }
};

class TestNode;
using TestNodePtr = base::SharedPtr<TestNode>;
class TestNode : public ContainerReferent {
 public:
  TestNode() : ContainerReferent(), ints_(GetAllocator()) {}
  TestNode(const DataContainerPtr& data, TestNode* parent)
      : ContainerReferent(data), ints_(GetAllocator()) {
    if (parent)
      parent->child_.Reset(this);
  }
  const AllocVector<int>& GetInts() const { return  ints_; }
  const TestNodePtr& GetChild() const { return child_; }

 protected:
  ~TestNode() override {}

 private:
  AllocVector<int> ints_;
  TestNodePtr child_;
};

TEST_F(AllocatableTest, ContainerReferent) {
  FullAllocationTrackerPtr tracker(new FullAllocationTracker);
  AllocatorPtr allocator = AllocationManager::GetDefaultAllocator();
  allocator->SetTracker(tracker);

  // Create DataContainer large enough to contain kNumTestNodes.
  const int kNumTestNodes = 33;
  DataContainerPtr data = DataContainer::CreateOverAllocated<uint8>(
      sizeof(TestNode) * kNumTestNodes, nullptr, allocator);
  uint8* mem = data->GetMutableData<uint8>();
  // Create TestNodes using placement new to create them in the DataContainer.
  TestNode* parent = nullptr;
  for (int i = 0; i < kNumTestNodes; ++i) {
    parent = new(mem) TestNode(data, parent);
    mem += sizeof(TestNode);
  }
  // Each TestNode should reference the DataContainer.
  EXPECT_EQ(kNumTestNodes + 1, data->GetRefCount());

  // The Allocator used for TestNode's members is the same used to create
  // the DataContainer.
  TestNode* tnode = data->GetMutableData<TestNode>();
  EXPECT_EQ(0, tnode->GetRefCount());
  EXPECT_TRUE(allocator == tnode->GetAllocator());
  EXPECT_TRUE(allocator == tnode->GetInts().get_allocator().GetAllocator());
  // Test ref counts.
  TestNodePtr ptr(tnode);
  EXPECT_EQ(1, tnode->GetRefCount());
  data.Reset();
  EXPECT_EQ(kNumTestNodes, ptr->GetDataContainer()->GetRefCount());
  TestNodePtr child = tnode->GetChild()->GetChild()->GetChild();
  ptr = TestNodePtr();
  EXPECT_EQ(kNumTestNodes - 3, child->GetDataContainer()->GetRefCount());
  // Release last ref to a TestNode to tear down the remaining node hierarchy.
  // The last ~TestNode will release the last ref to the DataContainer and then
  // delete it.
  child.Reset();
  EXPECT_EQ(0U, tracker->GetActiveAllocationBytesCount());

  // Allocate TestNode in Allocator, not DataContainer.
  ptr.Reset(new(allocator) TestNode);
  EXPECT_LE(sizeof(TestNode), tracker->GetActiveAllocationBytesCount());
  EXPECT_EQ(1, ptr->GetRefCount());
  ptr.Reset();
  EXPECT_EQ(0U, tracker->GetActiveAllocationBytesCount());
}

}  // namespace base
}  // namespace ion
