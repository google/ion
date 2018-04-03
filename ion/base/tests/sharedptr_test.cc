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

#include "ion/base/sharedptr.h"

#include "ion/base/shareable.h"
#include "ion/base/tests/incompletetype.h"
#include "ion/port/nullptr.h"
#include "ion/port/timer.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

// Simple class that allows testing of SharedPtr reference counting.
class TestCounter : public ion::base::Shareable {
 public:
  TestCounter() {}
  static size_t GetNumDeletions() { return s_num_deletions_; }
  static void ClearNumDeletions() { s_num_deletions_ = 0; }
  ~TestCounter() override { ++s_num_deletions_; }

 private:
  static size_t s_num_deletions_;
};

// Child class that allows testing of compatible pointers.
class DerivedTestCounter : public TestCounter {
 public:
  DerivedTestCounter() {}
  static size_t GetNumDeletions() { return s_num_deletions_; }
  static void ClearNumDeletions() { s_num_deletions_ = 0; }
 protected:
  ~DerivedTestCounter() override { ++s_num_deletions_; }

 private:
  static size_t s_num_deletions_;
};

typedef ion::base::SharedPtr<TestCounter> TestCounterPtr;
typedef ion::base::SharedPtr<DerivedTestCounter> DerivedTestCounterPtr;
size_t TestCounter::s_num_deletions_ = 0;
size_t DerivedTestCounter::s_num_deletions_ = 0;

#if defined(ION_TRACK_SHAREABLE_REFERENCES)
class Trackable : public ion::base::Shareable {
 public:
  Trackable(bool tracking_enabled) {
    SetTrackReferencesEnabled(tracking_enabled);
  }
};
typedef ion::base::SharedPtr<Trackable> TrackablePtr;
#endif

#if !ION_NO_RTTI
using ion::base::DynamicPtrCast;
#endif

TEST(SharedPtr, Constructors) {
  {
    // Default SharedPtr construction should have a null pointer.
    TestCounterPtr p;
    EXPECT_TRUE(p.Get() == nullptr);
  }

  {
    // Constructor taking a raw pointer.
    TestCounter* t = new TestCounter;
    TestCounterPtr p(t);
    EXPECT_EQ(t, p.Get());
    EXPECT_EQ(1, t->GetRefCount());
  }

  {
    // Constructor taking a compatible raw pointer.
    DerivedTestCounter* d = new DerivedTestCounter;
    TestCounterPtr p(d);
    EXPECT_EQ(d, p.Get());
    EXPECT_EQ(1, d->GetRefCount());
  }

  {
    // Constructor taking a compatible SharedPtr.
    DerivedTestCounter* d = new DerivedTestCounter;
    DerivedTestCounterPtr dp(d);
    TestCounterPtr p(dp);
    EXPECT_EQ(d, p.Get());
    EXPECT_EQ(2, d->GetRefCount());
  }

  {
    // Copy constructor.
    DerivedTestCounter* d = new DerivedTestCounter;
    DerivedTestCounterPtr dp(d);
    TestCounterPtr p(dp);
    EXPECT_EQ(d, p.Get());
    EXPECT_EQ(2, d->GetRefCount());
  }

  // All of the above pointers should have been deleted. The TestCounter
  // deletion counter is incremented when either class is deleted.
  EXPECT_EQ(4U, TestCounter::GetNumDeletions());
  EXPECT_EQ(3U, DerivedTestCounter::GetNumDeletions());

#if !defined(ION_TRACK_SHAREABLE_REFERENCES)
  {
    // Move constructor.
    TestCounter *t = new TestCounter;
    TestCounterPtr p1(t);
    EXPECT_EQ(1, t->GetRefCount());
    TestCounterPtr p2(std::move(p1));
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(p2.Get(), t);
  }
  {
    // Move constructor with derived type.
    DerivedTestCounter *d = new DerivedTestCounter;
    DerivedTestCounterPtr p1(d);
    EXPECT_EQ(1, d->GetRefCount());
    TestCounterPtr p2(std::move(p1));
    EXPECT_EQ(1, d->GetRefCount());
    EXPECT_EQ(p2.Get(), d);
  }

  EXPECT_EQ(6U, TestCounter::GetNumDeletions());
  EXPECT_EQ(4U, DerivedTestCounter::GetNumDeletions());
#endif
}

TEST(SharedPtr, Delete) {
  TestCounter::ClearNumDeletions();
  DerivedTestCounter::ClearNumDeletions();

  // Default (null) pointer should not delete anything.
  {
    TestCounterPtr p;
  }
  EXPECT_EQ(0U, TestCounter::GetNumDeletions());
  EXPECT_EQ(0U, DerivedTestCounter::GetNumDeletions());

  {
    // Constructors taking pointers should delete.
    TestCounter* t = new TestCounter;
    EXPECT_EQ(0, t->GetRefCount());
    TestCounterPtr p1(t);
    {
      TestCounterPtr p2(p1);
      EXPECT_EQ(2, t->GetRefCount());
    }
    // Losing one pointer should change refcount but not cause deletion.
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(0U, TestCounter::GetNumDeletions());
    EXPECT_EQ(0U, DerivedTestCounter::GetNumDeletions());
  }
  // Losing the other pointer should cause deletion.
  EXPECT_EQ(1U, TestCounter::GetNumDeletions());
  EXPECT_EQ(0U, DerivedTestCounter::GetNumDeletions());

  TestCounter::ClearNumDeletions();
  DerivedTestCounter::ClearNumDeletions();
  {
    // Test with derived class to make sure the right class is deleted.
    DerivedTestCounter* d = new DerivedTestCounter;
    EXPECT_EQ(0, d->GetRefCount());
    DerivedTestCounterPtr p1(d);
    {
      TestCounterPtr p2(p1);
      EXPECT_EQ(2, d->GetRefCount());
    }
    EXPECT_EQ(1, d->GetRefCount());
    EXPECT_EQ(0U, TestCounter::GetNumDeletions());
    EXPECT_EQ(0U, DerivedTestCounter::GetNumDeletions());
  }
  // Losing the other pointer should cause deletion.
  EXPECT_EQ(1U, TestCounter::GetNumDeletions());
  EXPECT_EQ(1U, DerivedTestCounter::GetNumDeletions());
}

TEST(SharedPtr, Assignment) {
  TestCounter* t = new TestCounter;
  DerivedTestCounter* d = new DerivedTestCounter;

  // These guarantee t and d do not get deleted.
  TestCounterPtr keep_t(t);
  DerivedTestCounterPtr keep_d(d);

  TestCounterPtr tp;
  DerivedTestCounterPtr dp;
  EXPECT_FALSE(tp);
  EXPECT_FALSE(dp);

  // Assignment to raw pointer.
  tp = t;
  EXPECT_EQ(t, tp.Get());
  EXPECT_EQ(2, t->GetRefCount());

  // Assignment to same pointer should have no effect.
  tp = t;
  EXPECT_EQ(t, tp.Get());
  EXPECT_EQ(2, t->GetRefCount());

  // Assignment to a SharedPtr of the same type.
  TestCounterPtr tp2;
  tp2 = tp;
  EXPECT_EQ(t, tp2.Get());
  EXPECT_EQ(3, t->GetRefCount());
  tp2 = nullptr;
  EXPECT_EQ(2, t->GetRefCount());

  // Assignment to compatible raw pointer.
  tp = d;
  EXPECT_EQ(d, tp.Get());
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_EQ(2, d->GetRefCount());

  // Assignment to nullptr.
  tp = nullptr;
  EXPECT_FALSE(tp);
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_EQ(1, d->GetRefCount());

  // Assignment to compatible SharedPtr.
  dp = d;
  tp = dp;
  EXPECT_EQ(d, tp.Get());
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_EQ(3, d->GetRefCount());

#if !defined(ION_TRACK_SHAREABLE_REFERENCES)
  {
    // Move assign.
    TestCounter *t = new TestCounter;
    TestCounterPtr p1(t);
    TestCounterPtr p2;
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(p2.Get(), nullptr);
    p2 = std::move(p1);
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(p1.Get(), nullptr);
    EXPECT_EQ(p2.Get(), t);
  }
  {
    // Move assign with derived type.
    DerivedTestCounter *d = new DerivedTestCounter;
    DerivedTestCounterPtr p1(d);
    EXPECT_EQ(1, d->GetRefCount());
    TestCounterPtr p2;
    EXPECT_EQ(p2.Get(), nullptr);
    p2 = std::move(p1);
    EXPECT_EQ(1, d->GetRefCount());
    EXPECT_EQ(p1.Get(), nullptr);
    EXPECT_EQ(p2.Get(), d);
  }
#endif
}

TEST(SharedPtr, Operators) {
  TestCounterPtr tp1;

  // -> and * operators.
  TestCounter* t1 = new TestCounter;
  tp1 = t1;
  EXPECT_EQ(t1, tp1.operator->());
  EXPECT_EQ(t1, &(tp1.operator*()));

  // == and != operators.
  TestCounter* t2 = new TestCounter;
  TestCounterPtr tp2;

  // Pointer vs. nullptr.
  EXPECT_FALSE(tp1 == tp2);
  EXPECT_TRUE(tp1 != tp2);
  EXPECT_TRUE(tp1);
  EXPECT_FALSE(tp2);

  // Pointer vs. pointer.
  tp2 = t2;
  EXPECT_FALSE(tp1 == tp2);
  EXPECT_TRUE(tp1 != tp2);

  // Identical pointers.
  tp1 = tp2;
  EXPECT_TRUE(tp1 == tp2);
  EXPECT_FALSE(tp1 != tp2);

  // Null pointers.
  tp1 = tp2 = nullptr;
  EXPECT_TRUE(tp1 == tp2);
  EXPECT_FALSE(tp1 != tp2);

  // operator->() should DCHECK on nullptr in debug mode.
  TestCounterPtr tp3;
#if !ION_PRODUCTION
  EXPECT_DEATH_IF_SUPPORTED(tp3.operator->(), "ptr_");
#endif
}

TEST(SharedPtr, Swap) {
  TestCounter::ClearNumDeletions();

  TestCounter* t1 = new TestCounter;
  TestCounter* t2 = new TestCounter;
  TestCounterPtr tp1(t1);
  TestCounterPtr tp2(t2);
  EXPECT_EQ(t1, tp1.Get());
  EXPECT_EQ(t2, tp2.Get());
  EXPECT_EQ(1, t1->GetRefCount());
  EXPECT_EQ(1, t2->GetRefCount());

  // Swap pointers.
  tp1.swap(tp2);
  EXPECT_EQ(t2, tp1.Get());
  EXPECT_EQ(t1, tp2.Get());
  EXPECT_EQ(1, t1->GetRefCount());
  EXPECT_EQ(1, t2->GetRefCount());
  EXPECT_EQ(0U, TestCounter::GetNumDeletions());

  // Swap back. (Also restores pointer order for clarity below.)
  tp1.swap(tp2);
  EXPECT_EQ(t1, tp1.Get());
  EXPECT_EQ(t2, tp2.Get());
  EXPECT_EQ(1, t1->GetRefCount());
  EXPECT_EQ(1, t2->GetRefCount());
  EXPECT_EQ(0U, TestCounter::GetNumDeletions());

  // Swap pointer with nullptr.
  TestCounterPtr tp3;
  tp1.swap(tp3);
  EXPECT_FALSE(tp1);
  EXPECT_EQ(t1, tp3.Get());
  EXPECT_EQ(0U, TestCounter::GetNumDeletions());

  // Swap nullptr with pointer.
  tp1.swap(tp2);
  EXPECT_EQ(t2, tp1.Get());
  EXPECT_FALSE(tp2);
  EXPECT_EQ(0U, TestCounter::GetNumDeletions());
}

#if !ION_NO_RTTI

TEST(SharedPtr, DynamicPtrCast) {
  // Test DynamicPtrCast works for downcasting in a valid case.
  {
    DerivedTestCounter *d = new DerivedTestCounter;
    TestCounterPtr bp(d);
    DerivedTestCounterPtr dp = DynamicPtrCast<DerivedTestCounter>(bp);
    EXPECT_EQ(d, dp.Get());
    EXPECT_EQ(2, d->GetRefCount());
  }

  // Test DynamicPtrCast fails when there is no relationship.
  {
    TestCounter *b = new TestCounter;
    TestCounterPtr bp(b);
    DerivedTestCounterPtr dp = DynamicPtrCast<DerivedTestCounter>(bp);
    EXPECT_EQ(nullptr, dp.Get());
    EXPECT_EQ(1, b->GetRefCount());
  }
}

#endif

TEST(SharedPtr, IncompleteType) {
  ion::base::SharedPtr<Incomplete> ptr = MakeIncomplete();

  // 
  // verify incompleteness.

  // These operations should work with an incomplete type.
  Incomplete* raw = ptr.Get();
  EXPECT_NE(raw, reinterpret_cast<Incomplete*>(0U));

  ion::base::SharedPtr<Incomplete> ptr2 = ptr;
  EXPECT_EQ(ptr2.Get(), ptr.Get());
  EXPECT_EQ(ptr2, ptr2);
  EXPECT_FALSE(ptr2 != ptr);

  ion::base::SharedPtr<Incomplete> ptr3;
  ptr.swap(ptr3);

  EXPECT_FALSE(ptr.Get());
  EXPECT_EQ(ptr2, ptr3);

  ptr2.Reset();

  // And finally, destruction of all ptrs should work.
}

#if defined(ION_TRACK_SHAREABLE_REFERENCES)
TEST(SharedPtr, TrackReferences) {
  // Test default operation with reference tracking disabled.
  Trackable* t = new Trackable(false);
  EXPECT_EQ(0, t->GetRefCount());
  EXPECT_TRUE(t->GetReferencesDebugString().empty());
  TrackablePtr p(t);
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_TRUE(t->GetReferencesDebugString().empty());
  p.Reset();

  // Test operation with reference tracking enabled.
  t = new Trackable(true);
  EXPECT_EQ(0, t->GetRefCount());
  EXPECT_TRUE(t->GetReferencesDebugString().empty());
  p.Reset(t);
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_FALSE(t->GetReferencesDebugString().empty());
  // Add a second reference.
  TrackablePtr p2(p);
  EXPECT_EQ(2, t->GetRefCount());
  EXPECT_FALSE(t->GetReferencesDebugString().empty());
  // Remove a reference.
  p.Reset();
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_FALSE(t->GetReferencesDebugString().empty());
  p2.Reset();
}
#endif

TEST(SharedPtr, ConstructionPerfTest) {
  ion::port::Timer tmr;
  const unsigned iterations = 100000;
  for (unsigned i = 0; i < iterations; ++i) {
    TestCounterPtr ptr(new TestCounter);
  }
  LOG(INFO) << "Time per SharedPtr construction/destruction: "
            << tmr.GetInMs() * 1000 / iterations << "us";
}

TEST(stdshared_ptr, ConstructionPerfTest) {
  ion::port::Timer tmr;
  const unsigned iterations = 100000;
  for (unsigned i = 0; i < iterations; ++i) {
    std::shared_ptr<TestCounter> ptr = std::make_shared<TestCounter>();
  }
  LOG(INFO) << "Time per std::shared_ptr construction/destruction: "
            << tmr.GetInMs() * 1000 / iterations << "us";
}

TEST(SharedPtr, AssignmentPerfTest) {
  ion::port::Timer tmr;
  const unsigned iterations = 100000;
  TestCounterPtr ptr(new TestCounter);
  for (unsigned i = 0; i < iterations; ++i) {
    TestCounterPtr ptr2 = ptr;
  }
  LOG(INFO) << "Time per SharedPtr increment/decrement: "
            << tmr.GetInMs() * 1000 / iterations << "us";
}

TEST(stdshared_ptr, AssignmentPerfTest) {
  ion::port::Timer tmr;
  const unsigned iterations = 100000;
  std::shared_ptr<TestCounter> ptr = std::make_shared<TestCounter>();
  for (unsigned i = 0; i < iterations; ++i) {
    std::shared_ptr<TestCounter> ptr2 = ptr;
  }
  LOG(INFO) << "Time per std::shared_ptr increment/decrement: "
            << tmr.GetInMs() * 1000 / iterations << "us";
}
