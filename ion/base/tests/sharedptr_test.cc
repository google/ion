/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#include "ion/base/logchecker.h"
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

using ion::base::DynamicPtrCast;

TEST(SharedPtr, Constructors) {
  {
    // Default SharedPtr construction should have a NULL pointer.
    TestCounterPtr p;
    EXPECT_TRUE(p.Get() == NULL);
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
}

TEST(SharedPtr, Delete) {
  TestCounter::ClearNumDeletions();
  DerivedTestCounter::ClearNumDeletions();

  // Default (NULL) pointer should not delete anything.
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
  EXPECT_TRUE(tp.Get() == NULL);
  EXPECT_TRUE(dp.Get() == NULL);

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
  tp2 = NULL;
  EXPECT_EQ(2, t->GetRefCount());

  // Assignment to compatible raw pointer.
  tp = d;
  EXPECT_EQ(d, tp.Get());
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_EQ(2, d->GetRefCount());

  // Assignment to NULL.
  tp = NULL;
  EXPECT_TRUE(tp.Get() == NULL);
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_EQ(1, d->GetRefCount());

  // Assignment to compatible SharedPtr.
  dp = d;
  tp = dp;
  EXPECT_EQ(d, tp.Get());
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_EQ(3, d->GetRefCount());
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
  // Pointer vs. NULL.
  EXPECT_FALSE(tp1 == tp2);
  EXPECT_TRUE(tp1 != tp2);
  // Pointer vs. pointer.
  tp2 = t2;
  EXPECT_FALSE(tp1 == tp2);
  EXPECT_TRUE(tp1 != tp2);
  // Identical pointers.
  tp1 = tp2;
  EXPECT_TRUE(tp1 == tp2);
  EXPECT_FALSE(tp1 != tp2);
  // NULL pointers.
  tp1 = tp2 = NULL;
  EXPECT_TRUE(tp1 == tp2);
  EXPECT_FALSE(tp1 != tp2);

  // operator->() should DCHECK on NULL in debug mode.
  {
    ion::base::LogChecker logchecker;
    ion::base::SetBreakHandler(kNullFunction);
    TestCounterPtr tp3;
    EXPECT_TRUE(tp3.operator->() == NULL);
    ion::base::RestoreDefaultBreakHandler();
#if ION_DEBUG
    EXPECT_TRUE(
        logchecker.HasMessage("DFATAL", "ptr_"));
#endif
  }
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

  // Swap pointer with NULL.
  TestCounterPtr tp3;
  tp1.swap(tp3);
  EXPECT_TRUE(tp1.Get() == NULL);
  EXPECT_EQ(t1, tp3.Get());
  EXPECT_EQ(0U, TestCounter::GetNumDeletions());

  // Swap NULL with pointer.
  tp1.swap(tp2);
  EXPECT_EQ(t2, tp1.Get());
  EXPECT_TRUE(tp2.Get() == NULL);
  EXPECT_EQ(0U, TestCounter::GetNumDeletions());
}

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
    EXPECT_EQ(NULL, dp.Get());
    EXPECT_EQ(1, b->GetRefCount());
  }
}

TEST(SharedPtr, IncompleteType) {
  ion::base::SharedPtr<Incomplete> ptr = MakeIncomplete();

  // TODO(user): If we ever write an is_complete type trait, use it here to
  // verify incompleteness.

  // These operations should work with an incomplete type.
  Incomplete* raw = ptr.Get();
  EXPECT_NE(raw, reinterpret_cast<Incomplete*>(NULL));

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
