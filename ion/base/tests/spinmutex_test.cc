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

#include <functional>
#include <thread>  // NOLINT(build/c++11)

#include "ion/base/spinmutex.h"
#include "ion/port/barrier.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

#if !defined(ION_PLATFORM_ASMJS)

template <class MutexT>
class GenericMutexTest : public ::testing::Test {
 public:
  void TestExclusion(int iterations);

 protected:
  MutexT mutex_;
};

template <class MutexT>
void GenericMutexTest<MutexT>::TestExclusion(int iterations) {
  for (int i = 0; i < iterations; ++i) {
    port::Barrier barrier(2);

    EXPECT_FALSE(mutex_.IsLocked());
    mutex_.Lock();
    EXPECT_TRUE(mutex_.IsLocked());

    std::thread thread([&]() {
      EXPECT_FALSE(mutex_.TryLock());

      // Wait twice, so that the main thread knows we reached this point,
      // and we know that it responded by unlocking.
      barrier.Wait();
      barrier.Wait();
      EXPECT_TRUE(mutex_.TryLock());

      // Wait twice, so that the main thread knows we reached this point,
      // and we know that it tried and failed to lock.
      barrier.Wait();
      barrier.Wait();
      mutex_.Unlock();

      // Wait so that the main thread knows that locking should succeed.
      barrier.Wait();

      return true;
    });

    barrier.Wait();  // Thread failed to lock.
    mutex_.Unlock();
    barrier.Wait();  // Thread knows that we unlocked.

    barrier.Wait();  // Thread finished locking.
    EXPECT_FALSE(mutex_.TryLock());
    barrier.Wait();  // Thread knows that we failed to lock.

    barrier.Wait();  // Thread unlocked.
    EXPECT_TRUE(mutex_.TryLock());

    EXPECT_TRUE(mutex_.IsLocked());
    mutex_.Unlock();
    EXPECT_FALSE(mutex_.IsLocked());

    thread.join();
  }
}

typedef GenericMutexTest<SpinMutex> SpinMutexTest;

TEST_F(SpinMutexTest, Exclusion) {
  TestExclusion(100);  // iterations
}

#endif  // !ION_PLATFORM_ASMJS

}  // namespace base
}  // namespace ion
