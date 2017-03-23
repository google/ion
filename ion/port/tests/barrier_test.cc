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

#include "ion/port/barrier.h"

#include <functional>

#include "base/integral_types.h"
#include "ion/port/atomic.h"
#include "ion/port/threadutils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace port {

// Counter used by ThreadCallback.
static std::atomic<int32> count(0);

// Callback function for multi-thread barrier test. This increments a static
// counter atomically by the given amount and then calls Wait() on the Barrier.
static bool ThreadCallback(Barrier* barrier, int n) {
  count += n;
  barrier->Wait();
  return true;
}

TEST(Barrier, Invalid) {
  // thread_count has to be positive to be valid.
  Barrier bad_barrier(0);
  EXPECT_FALSE(bad_barrier.IsValid());
}

TEST(Barrier, OneThread) {
  Barrier barrier(1);
  EXPECT_TRUE(barrier.IsValid());
  // This should return immediately.
  barrier.Wait();
  EXPECT_TRUE(barrier.IsValid());
}

#if !defined(ION_PLATFORM_ASMJS)
TEST(Barrier, MultiThreads) {
  count = 0;
  EXPECT_EQ(0, count);
  Barrier barrier(4);
  EXPECT_TRUE(barrier.IsValid());

  // Spawn three threads and have them all wait for the barrier. Also have this
  // calling thread wait for the barrier.
  std::thread t1(ThreadCallback, &barrier, 10);
  std::thread t2(ThreadCallback, &barrier, 20);
  std::thread t3(ThreadCallback, &barrier, 30);
  barrier.Wait();

  // When all 4 threads have called Wait(), this will execute.
  EXPECT_EQ(60, count);

  std::thread t4(ThreadCallback, &barrier, 10);
  std::thread t5(ThreadCallback, &barrier, 20);
  std::thread t6(ThreadCallback, &barrier, 30);
  barrier.Wait();

  // When all 4 threads have called Wait(), this will execute.
  EXPECT_EQ(120, count);

  t1.join();
  t2.join();
  t3.join();
  t4.join();
  t5.join();
  t6.join();
}
#endif  // !ION_PLATFORM_ASMJS

}  // namespace port
}  // namespace ion
