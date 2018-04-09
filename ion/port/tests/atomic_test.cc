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

#include <cstdio>
#include <functional>
#include <mutex>  // NOLINT(build/c++11)

#include "ion/base/logging.h"
#include "ion/base/threadspawner.h"
#include "ion/port/atomic.h"
#include "ion/port/barrier.h"
#include "ion/port/timer.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace port {

using base::ThreadSpawner;

static bool Incrementer(Barrier *barrier,
                        std::atomic<int> *atomicval, int count) {
  barrier->Wait();
  for (int i = 0; i < count; ++i) {
    ++(*atomicval);
  }
  return true;
}

static bool MutexDoubleIncrementer(int *val, std::mutex *mutex,
                                   int start, int count) {
  for (int i = start; i < count;) {
    mutex->lock();
    if (*val == i) {
      ++(*val);
      i += 2;
    }
    mutex->unlock();
  }
  return true;
}

static bool AtomicDoubleIncrementer(std::atomic<int> *atomicval,
                                    int start, int count) {
  for (int i = start; i < count;) {
    int expected = i;
    if (atomicval->compare_exchange_strong(expected, i + 1)) {
      i += 2;  // i always even or always odd
    }
  }
  return true;
}

// Make sure std::atomic compiles on all platforms.
TEST(Atomic, IntegerFunctionality) {
  std::atomic<int> aval(0);

  EXPECT_EQ(aval.load(), 0);

  EXPECT_TRUE(aval.is_lock_free());

  aval.store(10);
  aval = 5;

  EXPECT_EQ(aval.load(), 5);

  int prev = aval.exchange(15);

  EXPECT_EQ(prev, 5);

  EXPECT_FALSE(aval.compare_exchange_strong(prev, 25));
  EXPECT_EQ(prev, 15);
  EXPECT_EQ(aval, 15);
  EXPECT_TRUE(aval.compare_exchange_strong(prev, 25));
  EXPECT_EQ(aval.load(), 25);
  EXPECT_EQ(prev, 15);

  ++aval;
  EXPECT_EQ(aval.load(), 26);

  prev = aval.fetch_add(4);
  EXPECT_EQ(prev, 26);
  EXPECT_EQ(aval.load(), 30);

  prev = aval.fetch_sub(10);
  EXPECT_EQ(prev, 30);
  EXPECT_EQ(aval.load(), 20);

  aval -= 10;
  EXPECT_EQ(aval.load(), 10);
}

TEST(Atomic, BoolFunctionality) {
  std::atomic<bool> abool(false);

  EXPECT_EQ(static_cast<bool>(abool), false);
  EXPECT_FALSE(abool.exchange(true));
  EXPECT_TRUE(abool);
  abool.store(false);
  EXPECT_FALSE(abool.load());
  bool val = true;
  EXPECT_FALSE(abool.compare_exchange_strong(val, true));
  EXPECT_FALSE(val);
  EXPECT_FALSE(abool.load());
  EXPECT_TRUE(abool.compare_exchange_strong(val, true));
  EXPECT_TRUE(abool);
}

TEST(Atomic, PointerFunctionality) {
  std::atomic<int*> aptr(nullptr);
  int val1 = 5, val2 = 10;

  aptr = &val1;
  EXPECT_EQ(aptr.load(), &val1);

  aptr.store(new int(1));

  int* needs_delete = aptr.exchange(&val2);
  EXPECT_TRUE(needs_delete);
  EXPECT_NE(needs_delete, &val2);
  EXPECT_NE(needs_delete, &val1);
  delete needs_delete;

  EXPECT_EQ(aptr, &val2);

  int* expected = &val1;
  EXPECT_FALSE(aptr.compare_exchange_strong(expected, &val1));
  EXPECT_EQ(expected, &val2);
  EXPECT_TRUE(aptr.compare_exchange_strong(expected, &val1));
  EXPECT_EQ(aptr, &val1);
}

// Unfortunately gcc 4.6 doesn't support atomic enums. We won't use them for
// now, but keep the test in case we upgrade later.
#if !((__GNUC__ == 4 && __GNUC_MINOR_ <= 6) || __GNUC__ < 4)
enum TestAnimal { kMouse, kRat, kRabbit };

TEST(Atomic, EnumFunctionality) {
  std::atomic<TestAnimal> aenum(kMouse);

  EXPECT_EQ(aenum.load(), kMouse);
  EXPECT_EQ(aenum.exchange(kRat), kMouse);
  TestAnimal expected = kMouse;
  EXPECT_FALSE(aenum.compare_exchange_strong(expected, kRabbit));
  EXPECT_EQ(expected, kRat);
  EXPECT_TRUE(aenum.compare_exchange_strong(expected, kRabbit));
  EXPECT_EQ(aenum.load(), kRabbit);
}
#endif  // GCC <= 4.6

// Make sure that incrementing on a bunch of threads doesn't cause writes
// to get lost. Note: Writes are observed to be lost if this test is modified
// to use normal, unprotected ints.
#if !defined(ION_PLATFORM_ASMJS)
TEST(Atomic, MultiThreadedIncrement) {
  std::atomic<int> aval(0);
  const int iterations_per_thread = 500;
  const int num_threads = 5;
  Barrier barrier(num_threads);

  // ThreadSpawner isn't movable, so can't use it to create threads in a loop.
  ThreadSpawner thread1("Increment thread 1",
      std::bind(Incrementer, &barrier, &aval, iterations_per_thread));
  ThreadSpawner thread2("Increment thread 2",
      std::bind(Incrementer, &barrier, &aval, iterations_per_thread));
  ThreadSpawner thread3("Increment thread 3",
      std::bind(Incrementer, &barrier, &aval, iterations_per_thread));
  ThreadSpawner thread4("Increment thread 4",
      std::bind(Incrementer, &barrier, &aval, iterations_per_thread));
  Incrementer(&barrier, &aval, iterations_per_thread);
  thread1.Join();
  thread2.Join();
  thread3.Join();
  thread4.Join();

  EXPECT_EQ(aval.load(), iterations_per_thread * num_threads);
}

// Force two threads to heavily contend by only allowing one to increment
// even numbers and the other to increment odd numbers. This is a very
// artificial case, but it verifies atomics have advantages under heavy
// contention.
TEST(Atomic, SpeedHeavyContention) {
#if defined(ION_PLATFORM_ANDROID)
  // This is far too slow on emulators.
  static const int kIterations = 200;
#else
  static const int kIterations = 10000;
#endif
  std::atomic<int> aval(0);

  Timer timer;
  // Increment even numbers in the background thread.
  ThreadSpawner aincthread("Atomic increment thread",
      std::bind(AtomicDoubleIncrementer, &aval, 0, kIterations));
  // Increment odd numbers in the main thread.
  AtomicDoubleIncrementer(&aval, 1, kIterations);
  aincthread.Join();
  double a_ms = timer.GetInMs();

  std::mutex mutex;
  int val = 0;

  timer.Reset();
  // Increment even numbers in the background thread.
  ThreadSpawner mincthread("Mutex increment thread", std::bind(
      MutexDoubleIncrementer, &val, &mutex, 0, kIterations));
  // Increment odd numbers in the main thread.
  MutexDoubleIncrementer(&val, &mutex, 1, kIterations);
  mincthread.Join();
  double m_ms = timer.GetInMs();

  LOG(INFO) << "SpeedHeavyContention mutex to atomic running time ratio: "
            << m_ms/a_ms << ".";

  // Something is probably wrong if atomics aren't faster.
  if (m_ms/a_ms <= 1.0) {
    LOG(WARNING) << "SpeedNoContention shows mutexes faster than atomics!";
  }
  // 
  // Find a better way to report this.
}
#endif  // !ION_PLATFORM_AMSJS

// Test the relative speed of mutexes and atomics when there is no contention
// at all.
TEST(Atomic, SpeedNoContention) {
#if defined(ION_PLATFORM_ANDROID)
  static const int kIterations = 100000;
#else
  const int kIterations = 1000000;
#endif
  std::atomic<int> aval(0);

  Timer timer;
  for (int i = 0; i < kIterations; ++i) {
    ++aval;
  }
  double a_ms = timer.GetInMs();

  std::mutex mutex;
  int val = 0;

  timer.Reset();
  for (int i = 0; i < kIterations; ++i) {
    std::lock_guard<std::mutex> guard(mutex);
    ++val;
  }
  double m_ms = timer.GetInMs();

  LOG(INFO) << "SpeedNoContention mutex to atomic running time ratio: "
            << m_ms/a_ms << ".";

  // Atomics should be faster, but maybe not by much in this case.
  if (m_ms/a_ms <= 1.0) {
    LOG(WARNING) << "SpeedNoContention shows mutexes faster than atomics!";
  }
}

}  // namespace port
}  // namespace ion
