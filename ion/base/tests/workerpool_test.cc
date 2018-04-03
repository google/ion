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

#include "ion/base/workerpool.h"

#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <random>

#include "ion/base/threadspawner.h"
#include "ion/port/atomic.h"
#include "ion/port/barrier.h"
#include "ion/port/timer.h"
#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

// Used to wait for |barrier| on another thread.
std::unique_ptr<ion::base::ThreadSpawner> WaitForBarrier(
    ion::port::Barrier* barrier) {
  return absl::make_unique<ion::base::ThreadSpawner>(
      "WaitForBarrier", [=]() -> bool {
        ion::port::Timer::SleepNMilliseconds(1);
        barrier->Wait();
        return true;
      });
}

// Simple implementation of Worker that allows us to pause/resume in the
// middle of work, and track the amount of work done.
class TestWorker : public WorkerPool::Worker {
 public:
  TestWorker()
      : current_work_count_(0), total_work_count_(0), available_work_count_(0),
        barrier_one_(nullptr), barrier_two_(nullptr), work_sema_(nullptr) {}

  void SetBarriers(ion::port::Barrier* one, ion::port::Barrier* two) {
    std::lock_guard<std::mutex> lock(work_mutex_);
    DCHECK((one && two) || (one == two))
        << "Barriers must both be NULL or both be non-NULL";
    barrier_one_ = one;
    barrier_two_ = two;
  }

  void WaitUntilDoneWithBarriers() {
    alldone_sema_.Wait();
    SetBarriers(nullptr, nullptr);
  }

  void DoWork() override {
    ion::port::Barrier* one = nullptr;
    ion::port::Barrier* two = nullptr;

    {
      std::lock_guard<std::mutex> lock(work_mutex_);
      one = barrier_one_;
      two = barrier_two_;

      if (available_work_count_ == 0) {
        // There was no actual work to do, so exit early.
        return;
      }
      --available_work_count_;
    }

    ++current_work_count_;
    ++total_work_count_;

    // So we can stop "in the middle of working".
    if (one) {
      one->Wait();
    }

    // So we can stop "after work is done".
    if (two) {
      two->Wait();
    }

    if (--current_work_count_ == 0) {
      alldone_sema_.Post();
    }
  }

  // Return the worker's name.
  const std::string& GetName() const override {
    static const std::string kName("TestWorker");
    return kName;
  }

  int32 GetCurrentWorkCount() {
    return current_work_count_;
  }

  int32 GetTotalWorkCount() {
    return total_work_count_;
  }

  int32 GetAvailableWorkCount() {
    return available_work_count_;
  }

  void AddWork() {
    std::lock_guard<std::mutex> lock(work_mutex_);
    ++available_work_count_;
    work_sema_->Post();
  }

  void SetWorkSemaphore(ion::port::Semaphore* work_sema) {
    work_sema_ = work_sema;
  }

 private:
  std::atomic<int32> current_work_count_;
  std::atomic<int32> total_work_count_;
  int available_work_count_;
  ion::port::Barrier* barrier_one_;
  ion::port::Barrier* barrier_two_;
  ion::port::Semaphore alldone_sema_;
  ion::port::Semaphore* work_sema_;
  std::mutex work_mutex_;
};

// Verify that we don't deadlock if there is no work, then Suspend() is called.
TEST(WorkerPoolTest, SuspendWithNoWorkToDo) {
  TestWorker worker;
  WorkerPool pool(&worker);

  pool.ResizeThreadPool(10);
  pool.Resume();
  ion::port::Timer::SleepNMilliseconds(10);
  pool.Suspend();
}

// Verify that growing the number of threads in the pool allows more work
// to be done simultaneously.
TEST(WorkerPoolTest, GrowThreadPool) {
  TestWorker worker;
  WorkerPool pool(&worker);
  worker.SetWorkSemaphore(pool.GetWorkSemaphore());

  // The pool is currently suspended.  Grow to two threads, and verify that
  // we can do two work-items at once.
  int total_work = 0;
  int32 num_threads = 2;
  pool.ResizeThreadPool(num_threads);
  {
    pool.Resume();
    ion::port::Barrier one(num_threads + 1);
    ion::port::Barrier two(num_threads + 1);
    worker.SetBarriers(&one, &two);
    for (int32 i = 0; i < num_threads; i++) {
      total_work++;
      worker.AddWork();
    }
    one.Wait();
    EXPECT_EQ(num_threads, worker.GetCurrentWorkCount());
    EXPECT_EQ(total_work, worker.GetTotalWorkCount());
    two.Wait();
    worker.WaitUntilDoneWithBarriers();
  }

  // Verify that we can grow the pool while resumed.
  num_threads = 3;
  pool.ResizeThreadPool(num_threads);
  {
    ion::port::Barrier one(num_threads + 1);
    ion::port::Barrier two(num_threads + 1);
    worker.SetBarriers(&one, &two);
    for (int32 i = 0; i < num_threads; i++) {
      total_work++;
      worker.AddWork();
    }
    one.Wait();
    EXPECT_EQ(num_threads, worker.GetCurrentWorkCount());
    EXPECT_EQ(total_work, worker.GetTotalWorkCount());
    two.Wait();

    worker.WaitUntilDoneWithBarriers();
  }

  // Verify that we can grow after re-suspending.
  num_threads = 4;
  pool.Suspend();
  pool.ResizeThreadPool(num_threads);
  {
    pool.Resume();
    ion::port::Barrier one(num_threads + 1);
    ion::port::Barrier two(num_threads + 1);
    worker.SetBarriers(&one, &two);
    for (int32 i = 0; i < num_threads; i++) {
      total_work++;
      worker.AddWork();
    }
    one.Wait();
    EXPECT_EQ(num_threads, worker.GetCurrentWorkCount());
    EXPECT_EQ(total_work, worker.GetTotalWorkCount());
    two.Wait();

    worker.WaitUntilDoneWithBarriers();
  }
}

// Test that we can repeatedly grow and shrink the number of threads, both
// while suspended and while resumed.
TEST(WorkerPoolTest, GrowAndShrinkThreadPool) {
  TestWorker worker;
  WorkerPool pool(&worker);
  worker.SetWorkSemaphore(pool.GetWorkSemaphore());

  int total_work = 0;
  int32 num_threads = 20;
  pool.ResizeThreadPool(num_threads);
  {
    pool.Resume();
    ion::port::Barrier one(num_threads + 1);
    ion::port::Barrier two(num_threads + 1);
    worker.SetBarriers(&one, &two);
    for (int32 i = 0; i < num_threads; i++) {
      total_work++;
      worker.AddWork();
    }
    one.Wait();
    // Verify that each thread is stopped in the middle of work, and that
    // the total amount of work is as expected.  Repeated below.
    EXPECT_EQ(num_threads, worker.GetCurrentWorkCount());
    EXPECT_EQ(total_work, worker.GetTotalWorkCount());
    // Verify that no extra work is available, i.e. there is exactly as many
    // "work-items" as there are threads.
    EXPECT_EQ(0, worker.GetAvailableWorkCount());
    // Change pool size while each thread is in the middle of doing work.
    // Have another thread release the barrier.
    auto thread = WaitForBarrier(&two);
    num_threads = 10;
    pool.ResizeThreadPool(num_threads);

    worker.WaitUntilDoneWithBarriers();
  }

  // Verify that we can shrink.
  {
    ion::port::Barrier one(num_threads + 1);
    ion::port::Barrier two(num_threads + 1);
    worker.SetBarriers(&one, &two);
    for (int32 i = 0; i < num_threads; i++) {
      total_work++;
      worker.AddWork();
    }
    one.Wait();
    EXPECT_EQ(num_threads, worker.GetCurrentWorkCount());
    EXPECT_EQ(total_work, worker.GetTotalWorkCount());
    EXPECT_EQ(0, worker.GetAvailableWorkCount());
    two.Wait();

    worker.WaitUntilDoneWithBarriers();
  }

  // Verify that we can grow after shrinking.
  num_threads = 20;
  pool.ResizeThreadPool(num_threads);
  {
    ion::port::Barrier one(num_threads + 1);
    ion::port::Barrier two(num_threads + 1);
    worker.SetBarriers(&one, &two);
    for (int32 i = 0; i < num_threads; i++) {
      total_work++;
      worker.AddWork();
    }
    one.Wait();
    EXPECT_EQ(num_threads, worker.GetCurrentWorkCount());
    EXPECT_EQ(total_work, worker.GetTotalWorkCount());
    EXPECT_EQ(0, worker.GetAvailableWorkCount());
    two.Wait();

    worker.WaitUntilDoneWithBarriers();
  }

  // Verify that we can shrink while suspended.
  num_threads = 10;
  pool.Suspend();
  pool.ResizeThreadPool(num_threads);
  {
    pool.Resume();
    ion::port::Barrier one(num_threads + 1);
    ion::port::Barrier two(num_threads + 1);
    worker.SetBarriers(&one, &two);
    for (int32 i = 0; i < num_threads; i++) {
      total_work++;
      worker.AddWork();
    }
    one.Wait();
    EXPECT_EQ(num_threads, worker.GetCurrentWorkCount());
    EXPECT_EQ(total_work, worker.GetTotalWorkCount());
    EXPECT_EQ(0, worker.GetAvailableWorkCount());
    two.Wait();

    worker.WaitUntilDoneWithBarriers();
  }

  // Verify that growing and shrinking don't change the number of available
  // work-items.
  num_threads = 20;
  pool.ResizeThreadPool(num_threads);
  {
    ion::port::Barrier one(num_threads + 1);
    ion::port::Barrier two(num_threads + 1);
    worker.SetBarriers(&one, &two);
    for (int32 i = 0; i < num_threads; i++) {
      total_work++;
      worker.AddWork();
    }
    one.Wait();
    EXPECT_EQ(num_threads, worker.GetCurrentWorkCount());
    EXPECT_EQ(total_work, worker.GetTotalWorkCount());
    EXPECT_EQ(0, worker.GetAvailableWorkCount());
    // Change pool size while each thread is in the middle of doing work.
    // Have another thread release the barrier.
    auto thread = WaitForBarrier(&two);
    num_threads = 10;
    pool.ResizeThreadPool(num_threads);
    pool.Suspend();

    worker.WaitUntilDoneWithBarriers();
  }
}

// Verify that we don't deadlock if there is no work, then Suspend() is called.
TEST(WorkerPoolTest, StressTest) {
  TestWorker worker;
  WorkerPool pool(&worker);
  worker.SetWorkSemaphore(pool.GetWorkSemaphore());

  std::random_device dev;
  std::default_random_engine random(dev());
  std::uniform_real_distribution<float> chance(0, 1.0);

  // Chance of generating some work, resuming/suspending threads, and
  // increasing/decreasing the number of threads.
  double kWorkChance = 0.2;
  double kResumeChance = 0.3;
  double kSuspendChance = 0.1;
  double kResizePoolChance = 0.05;
  double kResizePoolToZeroChance = 0.01;

  int work_signals = 0;

  ion::port::Timer timer;
  while (timer.GetInS() < 1.0) {
    if (worker.GetTotalWorkCount() - work_signals < 1000) {
      if (chance(random) < kWorkChance) {
        int kMinWorkGenerated = 30;
        int kMaxWorkGenerated = 150;
        std::uniform_int_distribution<int> dist(
            kMinWorkGenerated, kMaxWorkGenerated);
        int count = dist(random);
        while (count--) {
          worker.AddWork();
          ++work_signals;
        }
      }
    }

    if (pool.IsSuspended()) {
      if (chance(random) < kResumeChance) {
        pool.Resume();
      }
    } else if (chance(random) < kSuspendChance) {
      pool.Suspend();
    }

    if (chance(random) < kResizePoolToZeroChance) {
      pool.ResizeThreadPool(0);
    } else if (chance(random) < kResizePoolChance) {
      size_t kMinThreads = 2;
      size_t kMaxThreads = 10;
      std::uniform_int_distribution<size_t> dist(kMinThreads, kMaxThreads);
      size_t new_thread_count = dist(random);
      pool.ResizeThreadPool(new_thread_count);
    }

    std::this_thread::yield();
  }
}

}  // namespace base
}  // namespace ion
