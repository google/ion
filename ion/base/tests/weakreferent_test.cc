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

#include "ion/base/weakreferent.h"

#include "ion/base/logchecker.h"
#include "ion/base/threadspawner.h"
#include "ion/port/atomic.h"
#include "ion/port/barrier.h"
#include "ion/port/nullptr.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

// Concurrent tests are intrinsically unreliable and need to be run many
// times to get a decent statistical chance of detecting bad code.
static const int kConcurrentTestRepeats = 1000;

// Derived Referent class that allows testing of reference count management.
class TestRef : public ion::base::WeakReferent {
 public:
  TestRef() {}
  static void ClearNumDestroys() { num_destroys_ = 0; }
  static size_t GetNumDestroys() { return num_destroys_; }
 protected:
  ~TestRef() override { ++num_destroys_; }
  static std::atomic<int> num_destroys_;
};

// Child class that allows testing of compatible pointers.
class DerivedTestRef : public TestRef {
 public:
  DerivedTestRef() {}
  static void ClearNumDestroys() { num_destroys_ = 0; }
  static size_t GetNumDestroys() { return num_destroys_; }
 protected:
  ~DerivedTestRef() override {
    ++num_destroys_;
    // Cancel increment in superclass destructor.
    --TestRef::num_destroys_;
  }
 private:
  static size_t num_destroys_;
};

using TestRefPtr = ion::base::SharedPtr<TestRef>;
using DerivedTestRefPtr = ion::base::SharedPtr<DerivedTestRef>;
typedef ion::base::WeakReferentPtr<TestRef> TestWeakRefPtr;

// Thread helper for testing weak reference actions from a second thread.
class ConcurrentWeakRefHelper {
 public:
  ConcurrentWeakRefHelper(const TestWeakRefPtr& weak_ref,
                          bool drop_fast)
      : num_acquires_(0),
        barrier_(2),
        weak_ref_(weak_ref),
        drop_fast_(drop_fast) {}

  ion::port::Barrier* GetBarrier() { return &barrier_; }

  int GetNumAcquires() const { return num_acquires_; }

  bool Run() {
    // Wait to start to give maximal chance of concurrency.
    // This also implies a memory barrier so non-thread safe members created in
    // construction on one thread should be safely usable from the second
    // thread.
    barrier_.Wait();

    // The goal of this test is to have some Acquire()s succeed and some fail.
    // Without the yielding code below, the Acquire() nearly always succeeds,
    // thus defeating the purpose of the test.  The reason is that the barrier
    // results in one thread falling through the barrier, making it hot, while
    // the other thread was waiting for that event to happen, making it cold.
    int yields = 10;
    while (yields--) std::this_thread::yield();

    // Unlike ConcurrentStrongWeakRefHelper::Run(), we don't desire/expect
    // the Acquire() to fail 100% of the time.
    TestRefPtr ptr = weak_ref_.Acquire();
    if (ptr.Get()) {
      ++num_acquires_;
    }

    if (drop_fast_) {
      // Immediately drop the ref for checking missed resurrection duplicate
      // dismantle.
      ptr.Reset(nullptr);
    }

    barrier_.Wait();
    // Nothing to do here, just need a safe place to guarantee this thread
    // is not doing anything before the potentially acquired reference is
    // destroyed.
    barrier_.Wait();
    return true;
  }

 private:
  std::atomic<int> num_acquires_;
  ion::port::Barrier barrier_;
  TestWeakRefPtr weak_ref_;
  bool drop_fast_;
};

// Thread helper for testing strong to weak conversion actions from a second
// thread.
class ConcurrentStrongWeakRefHelper {
 public:
  explicit ConcurrentStrongWeakRefHelper(const TestRefPtr& ref)
      : num_acquires_(0),
        barrier_(2),
        ref_(ref) {}

  ion::port::Barrier* GetBarrier() { return &barrier_; }

  int GetNumAcquires() const { return num_acquires_; }

  bool Run() {
    // Wait to start to give maximal chance of concurrency.
    // This also implies a memory barrier so non-thread safe members created in
    // construction on one thread should be safely usable from the second
    // thread.
    barrier_.Wait();

    TestWeakRefPtr weak_ref(ref_);
    ref_.Reset(nullptr);

    barrier_.Wait();

    // This should never work as both threads have released and barrier waits
    // observed.
    ref_ = weak_ref.Acquire();
    if (ref_.Get()) {
      ++num_acquires_;
    }

    barrier_.Wait();

    // Nothing to do here, just need a safe place to guarantee this thread
    // is not doing anything before the potentially acquired reference is
    // destroyed.

    barrier_.Wait();

    return true;
  }

 private:
  std::atomic<int> num_acquires_;
  ion::port::Barrier barrier_;
  TestRefPtr ref_;
};

std::atomic<int> TestRef::num_destroys_(0);
size_t DerivedTestRef::num_destroys_ = 0;

TEST(Referent, Constructors) {
  TestRef::ClearNumDestroys();
  DerivedTestRef::ClearNumDestroys();

  {
    // Default ReferentPtr construction should have a null pointer.
    TestRefPtr p;
    EXPECT_FALSE(p);
  }

  {
    // Constructor taking a raw pointer.
    TestRef* t = new TestRef;
    TestRefPtr p(t);
    EXPECT_EQ(t, p.Get());
    EXPECT_EQ(1, t->GetRefCount());
  }

  {
    // Constructor taking a compatible raw pointer.
    DerivedTestRef* d = new DerivedTestRef;
    TestRefPtr p(d);
    EXPECT_EQ(d, p.Get());
    EXPECT_EQ(1, d->GetRefCount());
  }

  {
    // Constructor taking a compatible ReferentPtr.
    DerivedTestRef* d = new DerivedTestRef;
    DerivedTestRefPtr dp(d);
    TestRefPtr p(dp);
    EXPECT_EQ(d, p.Get());
    EXPECT_EQ(2, d->GetRefCount());
  }

  {
    // Copy constructor.
    DerivedTestRef* d = new DerivedTestRef;
    DerivedTestRefPtr dp(d);
    TestRefPtr p(dp);
    EXPECT_EQ(d, p.Get());
    EXPECT_EQ(2, d->GetRefCount());
  }

  // All of the above pointers should have been dismantled.
  EXPECT_EQ(1U, TestRef::GetNumDestroys());
  EXPECT_EQ(3U, DerivedTestRef::GetNumDestroys());
}

TEST(Referent, Dismantle) {
  TestRef::ClearNumDestroys();
  DerivedTestRef::ClearNumDestroys();

  // Default (nullptr) pointer should not dismantle anything.
  {
    TestRefPtr p;
  }
  EXPECT_EQ(0U, TestRef::GetNumDestroys());
  EXPECT_EQ(0U, DerivedTestRef::GetNumDestroys());

  {
    // Constructors taking pointers should dismantle.
    TestRef* t = new TestRef;
    EXPECT_EQ(0, t->GetRefCount());
    TestRefPtr p1(t);
    {
      TestRefPtr p2(p1);
      EXPECT_EQ(2, t->GetRefCount());
    }
    // Losing one pointer should change refcount but not cause dismantling.
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(0U, TestRef::GetNumDestroys());
    EXPECT_EQ(0U, DerivedTestRef::GetNumDestroys());
  }
  // Losing the other pointer should cause dismantling.
  EXPECT_EQ(1U, TestRef::GetNumDestroys());
  EXPECT_EQ(0U, DerivedTestRef::GetNumDestroys());

  TestRef::ClearNumDestroys();
  DerivedTestRef::ClearNumDestroys();
  {
    // Test with derived class to make sure the right class is dismantled.
    DerivedTestRef* d = new DerivedTestRef;
    EXPECT_EQ(0, d->GetRefCount());
    DerivedTestRefPtr p1(d);
    {
      TestRefPtr p2(p1);
      EXPECT_EQ(2, d->GetRefCount());
    }
    EXPECT_EQ(1, d->GetRefCount());
    EXPECT_EQ(0U, TestRef::GetNumDestroys());
    EXPECT_EQ(0U, DerivedTestRef::GetNumDestroys());
  }
  // Losing the other pointer should cause dismantling.
  EXPECT_EQ(0U, TestRef::GetNumDestroys());
  EXPECT_EQ(1U, DerivedTestRef::GetNumDestroys());
}

TEST(Referent, Assignment) {
  TestRef* t = new TestRef;
  DerivedTestRef* d = new DerivedTestRef;

  // These guarantee t and d do not get deleted.
  TestRefPtr keep_t(t);
  DerivedTestRefPtr keep_d(d);

  TestRefPtr tp;
  DerivedTestRefPtr dp;
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

  // Assignment to a ReferentPtr of the same type.
  TestRefPtr tp2;
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

  // Assignment to compatible ReferentPtr.
  dp = d;
  tp = dp;
  EXPECT_EQ(d, tp.Get());
  EXPECT_EQ(1, t->GetRefCount());
  EXPECT_EQ(3, d->GetRefCount());
}

TEST(Referent, Operators) {
  TestRefPtr tp1;

  // -> and * operators.
  TestRef* t1 = new TestRef;
  tp1 = t1;
  EXPECT_EQ(t1, tp1.operator->());
  EXPECT_EQ(t1, &(tp1.operator*()));

  // == and != operators.
  TestRef* t2 = new TestRef;
  TestRefPtr tp2;
  // Pointer vs. nullptr.
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
  // Null pointers.
  tp1 = tp2 = nullptr;
  EXPECT_TRUE(tp1 == tp2);
  EXPECT_FALSE(tp1 != tp2);

#if !ION_PRODUCTION
  // operator->() should DCHECK on nullptr in debug mode.
  {
    TestRefPtr tp3;
    EXPECT_DEATH_IF_SUPPORTED(tp3.operator->(), "ptr_");
  }
#endif
}

TEST(Referent, Swap) {
  TestRef::ClearNumDestroys();

  TestRef* t1 = new TestRef;
  TestRef* t2 = new TestRef;
  TestRefPtr tp1(t1);
  TestRefPtr tp2(t2);
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
  EXPECT_EQ(0U, TestRef::GetNumDestroys());

  // Swap back. (Also restores pointer order for clarity below.)
  tp1.swap(tp2);
  EXPECT_EQ(t1, tp1.Get());
  EXPECT_EQ(t2, tp2.Get());
  EXPECT_EQ(1, t1->GetRefCount());
  EXPECT_EQ(1, t2->GetRefCount());
  EXPECT_EQ(0U, TestRef::GetNumDestroys());

  // Swap pointer with nullptr.
  TestRefPtr tp3;
  tp1.swap(tp3);
  EXPECT_FALSE(tp1);
  EXPECT_EQ(t1, tp3.Get());
  EXPECT_EQ(0U, TestRef::GetNumDestroys());

  // Swap nullptr with pointer.
  tp1.swap(tp2);
  EXPECT_EQ(t2, tp1.Get());
  EXPECT_FALSE(tp2);
  EXPECT_EQ(0U, TestRef::GetNumDestroys());
}

TEST(WeakReferent, Constructors) {
  TestRef::ClearNumDestroys();

  {
    // Default ReferentPtr construction should have a null pointer.
    TestRefPtr p;
    EXPECT_FALSE(p);
  }

  {
    // Constructor taking a raw pointer.
    TestRef* t = new TestRef;
    TestRefPtr p(t);
    EXPECT_EQ(t, p.Get());
    EXPECT_EQ(1, t->GetRefCount());
  }
  EXPECT_EQ(1U, TestRef::GetNumDestroys());

  {
    // Out of order constructor; a WeakReferentPtr must be created after the
    // ReferentPtr.
    ion::base::LogChecker log_checker;
    EXPECT_FALSE(log_checker.HasAnyMessages());
    TestRef* t = new TestRef;
    TestWeakRefPtr wp(t);
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Input pointer was not owned"));
    // t should have been destroyed.
    EXPECT_EQ(2U, TestRef::GetNumDestroys());
  }

  {
    // Constructor taking the same raw pointer.
    TestRef* t = new TestRef;
    TestRefPtr p(t);
    TestWeakRefPtr wp(t);
    EXPECT_EQ(p, wp.Acquire());
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(1, wp.GetUnderlyingRefCountUnsynchronized());
  }

  {
    // Constructor taking a compatible referent pointer.
    TestRef* t = new TestRef;
    TestRefPtr p(t);
    TestWeakRefPtr w(p);
    EXPECT_EQ(p, w.Acquire());
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(1, w.GetUnderlyingRefCountUnsynchronized());
  }

  {
    // Copy constructor.
    TestRef* t = new TestRef;
    TestRefPtr p(t);
    TestWeakRefPtr w1(t);
    TestWeakRefPtr w2(w1);
    EXPECT_EQ(w1.Acquire(), w2.Acquire());
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(1, w1.GetUnderlyingRefCountUnsynchronized());
    EXPECT_EQ(1, w2.GetUnderlyingRefCountUnsynchronized());
  }

  {
    TestWeakRefPtr wp;
    {
      // Test that a weak referent that lives beyond the original pointer can
      // still attempt to get a ref count and not crash.
      TestRef* t = new TestRef;
      TestRefPtr p(t);
      wp = TestWeakRefPtr(t);
      EXPECT_EQ(p, wp.Acquire());
      EXPECT_EQ(1, t->GetRefCount());
      EXPECT_EQ(1, wp.GetUnderlyingRefCountUnsynchronized());
    }
    EXPECT_EQ(0, wp.GetUnderlyingRefCountUnsynchronized());
  }

  // All of the above pointers should have been dismantled.
  EXPECT_EQ(6U, TestRef::GetNumDestroys());
}

TEST(WeakReferent, Reset) {
  TestRefPtr p(new TestRef);
  TestWeakRefPtr wp(p);

  EXPECT_TRUE(wp.Acquire().Get());
  wp.Reset();
  EXPECT_FALSE(wp.Acquire().Get());
}

TEST(WeakReferent, Dismantle) {
  TestRef::ClearNumDestroys();

  // Default (nullptr) pointer should not dismantle anything.
  {
    TestRefPtr p;
  }
  EXPECT_EQ(0U, TestRef::GetNumDestroys());

  {
    TestRef* t = new TestRef;
    EXPECT_EQ(0, t->GetRefCount());
    TestRefPtr p(t);
    {
      TestWeakRefPtr w(p);
      // Only the ReferentPtr increases the ref count.
      EXPECT_EQ(1, t->GetRefCount());
    }
    // Losing the weak pointer should not change the refcount or cause
    // dismantling.
    EXPECT_EQ(1, t->GetRefCount());
    EXPECT_EQ(0U, TestRef::GetNumDestroys());
  }
  // Losing the ReferentPtrr should cause dismantling.
  EXPECT_EQ(1U, TestRef::GetNumDestroys());
  TestRef::ClearNumDestroys();

  {
    TestRef* t = new TestRef;
    EXPECT_EQ(0, t->GetRefCount());
    TestRefPtr* p = new TestRefPtr(t);
    TestWeakRefPtr w1(*p);
    TestWeakRefPtr w2(*p);
    // Only the ReferentPtr increases the ref count.
    EXPECT_EQ(1, t->GetRefCount());
    // The WeakReferentPtr should point to t.
    EXPECT_TRUE(w1.Acquire().Get() == t);

    // Destroying the ReferentPtr should cause dismantling.
    delete p;
    EXPECT_EQ(1U, TestRef::GetNumDestroys());
    // The WeakReferentPtrs should point to nullptr.
    EXPECT_FALSE(w1.Acquire());
    EXPECT_FALSE(w2.Acquire());
  }

  {
    TestWeakRefPtr w(nullptr);
    EXPECT_FALSE(w.Acquire());
  }
}

TEST(WeakReferent, Operators) {
  TestRef* t1 = new TestRef;
  TestRef* t2 = new TestRef;
  TestRefPtr tp1(t1);
  TestRefPtr tp2(t2);

  // == and != operators.
  TestWeakRefPtr wp1(t1);
  {
    TestWeakRefPtr wp2(nullptr);
    // Pointer vs. nullptr.
    EXPECT_FALSE(wp1 == wp2);
    EXPECT_TRUE(wp1 != wp2);
  }
  TestWeakRefPtr wp2(t2);
  // Pointer vs. pointer.
  EXPECT_FALSE(wp1 == wp2);
  EXPECT_TRUE(wp1 != wp2);
  // Identical pointers.
  wp1 = wp2;
  EXPECT_TRUE(wp1 == wp2);
  EXPECT_FALSE(wp1 != wp2);

  // Equality from ReferentPtrs.
  TestRef* t3 = new TestRef;
  TestRef* t4 = new TestRef;
  TestRefPtr tp3(t3);
  TestRefPtr tp4(t4);

  TestWeakRefPtr wp3(t3);
  TestWeakRefPtr wp4(t4);
  // Referent pointers.
  EXPECT_FALSE(wp3 == wp4);
  EXPECT_TRUE(wp3 != wp4);

  TestWeakRefPtr wp5(t3);
  TestWeakRefPtr wp6(t3);
  EXPECT_TRUE(wp5 == wp5);
  EXPECT_FALSE(wp6 != wp6);

  // Assignment from SharedPtr.
  wp6 = tp4;
  EXPECT_FALSE(wp5 == wp6);
  wp6 = tp3;
  EXPECT_TRUE(wp5 == wp6);
}

// 

#if !defined(ION_PLATFORM_ASMJS)
TEST(WeakReferent, ConcurrentAcquireWithRelease) {
  int acquires = 0;
  for (int i = 0; i < kConcurrentTestRepeats; i++) {
    TestRef::ClearNumDestroys();
    TestRef* raw = new TestRef;
    TestRefPtr ptr(raw);
    TestWeakRefPtr weak_ptr(ptr);
    ConcurrentWeakRefHelper helper(weak_ptr, false);
    {
      ion::base::ThreadSpawner spawner(
          "WeakBackground",
          std::bind(&ConcurrentWeakRefHelper::Run, &helper));
      helper.GetBarrier()->Wait();
      // Drop the reference while the other is attempting to acquire.
      ptr.Reset(nullptr);
      helper.GetBarrier()->Wait();
      // At this point the second thread either acquired the reference or did
      // not.
      // Therefore either dismantle was called, or the reference count should be
      // one.  Not both as could happen if race conditions were not handled.
      EXPECT_FALSE(helper.GetNumAcquires() == 1 &&
                   TestRef::GetNumDestroys() == 1U);
      EXPECT_TRUE(helper.GetNumAcquires() == 1 ||
                  TestRef::GetNumDestroys() == 1U);
      helper.GetBarrier()->Wait();
    }
    // Once both threads have completed ref count should be 0 and dismantle
    // should have occurred, regardless of path.
    EXPECT_EQ(1U, TestRef::GetNumDestroys());

    acquires += helper.GetNumAcquires();
  }

  // If acquisitions always fail or always succeed, then this test isn't working
  // as intended.  It's OK to see this warning infrequently, but it's a problem
  // if it happens almost every time the test runs.
  if (acquires == kConcurrentTestRepeats)
    LOG(WARNING) << "SharedPtr acquisition always succeeded";
  else if (acquires == 0)
    LOG(WARNING) << "SharedPtr acquisition never succeeded";
}

TEST(WeakReferent, ConcurrentAcquireReleaseWithRelease) {
  int acquires = 0;
  for (int i = 0; i < kConcurrentTestRepeats; i++) {
    TestRef::ClearNumDestroys();
    TestRef* raw = new TestRef;
    TestRefPtr ptr(raw);
    TestWeakRefPtr weak_ptr(ptr);
    ConcurrentWeakRefHelper helper(weak_ptr, true);
    {
      ion::base::ThreadSpawner spawner(
          "WeakBackground",
          std::bind(&ConcurrentWeakRefHelper::Run, &helper));
      helper.GetBarrier()->Wait();
      // Drop the reference while the other is attempting to acquire.
      ptr.Reset(nullptr);
      helper.GetBarrier()->Wait();
      // At this point the second thread acquired and released, so ref count
      // should now be 0 and dismantle should have happened once.
      // Not twice as can happen if the acquire/release thread races the
      // normal release, and the normal release 'misses' the resurrection.
      EXPECT_EQ(1U, TestRef::GetNumDestroys());
      helper.GetBarrier()->Wait();
    }

    acquires += helper.GetNumAcquires();
  }

  // If acquisitions always fail or always succeed, then this test isn't working
  // as intended.  It's OK to see this warning infrequently, but it's a problem
  // if it happens almost every time the test runs.
  if (acquires == kConcurrentTestRepeats)
    LOG(WARNING) << "SharedPtr acquisition always succeeded";
  else if (acquires == 0)
    LOG(WARNING) << "SharedPtr acquisition never succeeded";
}

TEST(WeakReferent, ConcurrentConstructWithRelease) {
  for (int i = 0; i < kConcurrentTestRepeats; i++) {
    TestRef::ClearNumDestroys();
    TestRef* raw = new TestRef;
    TestRefPtr ptr(raw);
    ConcurrentStrongWeakRefHelper helper(ptr);
    {
      ion::base::ThreadSpawner spawner(
          "WeakCopyBackground",
          std::bind(&ConcurrentStrongWeakRefHelper::Run, &helper));
      helper.GetBarrier()->Wait();
      // Drop the reference while the other is attempting to construct weak
      // reference.
      ptr.Reset(nullptr);
      helper.GetBarrier()->Wait();
      // Wait for other thread to try and acquire now that release should
      // have been guaranteed.
      helper.GetBarrier()->Wait();
      // Without locks implying memory barriers, this thread may miss the
      // fact that the proxy has been created, and hence not clear it.
      helper.GetBarrier()->Wait();
    }

    EXPECT_EQ(0, helper.GetNumAcquires());
  }
}
#endif  // !ION_PLATFORM_ASMJS
