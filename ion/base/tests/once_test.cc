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

#include "ion/base/once.h"

#include "ion/port/timer.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

class SpinBarrier {
 public:
  explicit SpinBarrier(int32 size) : counter_(0), loops_(0), size_(size) {}
  void Wait() {
    int32 before = loops_;
    counter_++;
    int expected = size_;
    if (counter_.compare_exchange_strong(expected, 0)) loops_++;
    while (loops_ == before) {}
  }

 private:
  std::atomic<int32> counter_;
  std::atomic<int32> loops_;
  int32 size_;
};

// Deliberately not atomic - CallOnce must ensure the thread safety for us.
static int32 s_flag_count;

static void Increment() {
  s_flag_count++;
}

static void IncrementSlow() {
  port::Timer::SleepNSeconds(1);
  s_flag_count++;
}

// Callback function for multi-thread once test. Waits for alignment, attempts
// to race CallOnce, then waits again.
static void ThreadCallback(const std::function<void()>& target,
                           SpinBarrier* barrier, OnceFlag* flag) {
  barrier->Wait();
  flag->CallOnce(target);
  barrier->Wait();
}

static std::atomic<int32> s_counter;
// Simple populator function for testing lazy.
static int GetThree() {
  s_counter++;
  return 3;
}

TEST(Once, BasicOnce) {
  s_flag_count = 0;
  OnceFlag flag;
  flag.CallOnce(&Increment);
  EXPECT_EQ(1, s_flag_count);
  flag.CallOnce(&Increment);
  EXPECT_EQ(1, s_flag_count);
  OnceFlag flag2;
  flag2.CallOnce(&Increment);
  EXPECT_EQ(2, s_flag_count);
  flag2.CallOnce(&Increment);
  EXPECT_EQ(2, s_flag_count);
  flag.CallOnce(&Increment);
  EXPECT_EQ(2, s_flag_count);
}

TEST(Once, BasicLazy) {
  s_counter = 0;
  Lazy<int> lazy(&GetThree);
  EXPECT_EQ(0, s_counter);
  EXPECT_EQ(3, lazy.Get());
  EXPECT_EQ(1, s_counter);
  EXPECT_EQ(3, lazy.Get());
  EXPECT_EQ(1, s_counter);
}

TEST(Once, VectorLazy) {
  s_counter = 0;
  std::vector<Lazy<int> > lazy_vector;
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  lazy_vector.push_back(Lazy<int>(&GetThree));
  EXPECT_EQ(0, s_counter);
  for (size_t i = 0; i < lazy_vector.size(); i++) {
    EXPECT_EQ(3, lazy_vector[i].Get());
    EXPECT_EQ(i + 1, static_cast<size_t>(s_counter));
  }
  for (size_t i = 0; i < lazy_vector.size(); i++) {
    EXPECT_EQ(3, lazy_vector[i].Get());
    EXPECT_EQ(lazy_vector.size(), static_cast<size_t>(s_counter));
  }
}

#if !defined(ION_PLATFORM_ASMJS)
// Test race conditions.
TEST(Once, ThreadedOnce) {
  s_flag_count = 0;
  EXPECT_EQ(0, s_flag_count);
  SpinBarrier barrier(4);
  OnceFlag flag;
  std::function<void()> f1(
      std::bind(ThreadCallback, &Increment, &barrier, &flag));

  // Spawn three threads and have them all wait for the barrier. Also have this
  // calling thread wait for the barrier.
  std::thread t1(f1);
  std::thread t2(f1);
  std::thread t3(f1);
  EXPECT_EQ(0, s_flag_count);
  barrier.Wait();
  flag.CallOnce(&Increment);
  EXPECT_EQ(1, s_flag_count);
  barrier.Wait();
  EXPECT_EQ(1, s_flag_count);

  t1.join();
  t2.join();
  t3.join();
}

// Ensure that the spin wait code path is covered regardless of race conditions.
TEST(Once, ThreadedOnceSlowTarget) {
  s_flag_count = 0;
  EXPECT_EQ(0, s_flag_count);
  SpinBarrier barrier(4);
  OnceFlag flag;
  std::function<void()> f1(
      std::bind(ThreadCallback, &IncrementSlow, &barrier, &flag));

  // Spawn three threads and have them all wait for the barrier. Also have this
  // calling thread wait for the barrier.
  std::thread t1(f1);
  std::thread t2(f1);
  std::thread t3(f1);
  EXPECT_EQ(0, s_flag_count);
  barrier.Wait();
  flag.CallOnce(&IncrementSlow);
  EXPECT_EQ(1, s_flag_count);
  barrier.Wait();
  EXPECT_EQ(1, s_flag_count);

  t1.join();
  t2.join();
  t3.join();
}
#endif  // !ION_PLATFORM_ASMJS

}  // namespace base
}  // namespace ion
