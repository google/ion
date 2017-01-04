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
  ThreadStdFunc f1(std::bind(ThreadCallback, &barrier, 10));
  ThreadStdFunc f2(std::bind(ThreadCallback, &barrier, 20));
  ThreadStdFunc f3(std::bind(ThreadCallback, &barrier, 30));

  // Spawn three threads and have them all wait for the barrier. Also have this
  // calling thread wait for the barrier.
  ThreadId t1 = SpawnThreadStd(&f1);
  ThreadId t2 = SpawnThreadStd(&f2);
  ThreadId t3 = SpawnThreadStd(&f3);
  barrier.Wait();

  // When all 4 threads have called Wait(), this will execute.
  EXPECT_EQ(60, count);

  ThreadId t4 = SpawnThreadStd(&f1);
  ThreadId t5 = SpawnThreadStd(&f2);
  ThreadId t6 = SpawnThreadStd(&f3);
  barrier.Wait();

  // When all 4 threads have called Wait(), this will execute.
  EXPECT_EQ(120, count);

  JoinThread(t1);
  JoinThread(t2);
  JoinThread(t3);
  JoinThread(t4);
  JoinThread(t5);
  JoinThread(t6);
}
#endif  // !ION_PLATFORM_ASMJS

}  // namespace port
}  // namespace ion
