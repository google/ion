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

#include "ion/port/semaphore.h"

#include "ion/port/timer.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(Semaphore, PostAndWait) {
  ion::port::Semaphore semaphore;
  // Post once to let the wait pass so the test won't block.
  EXPECT_TRUE(semaphore.Post());
  EXPECT_TRUE(semaphore.Wait());

  // Trying to wait now should fail since there have not been any posts.
  EXPECT_FALSE(semaphore.TryWait());
  // Posting will allow the next TryWait to succeed.
  EXPECT_TRUE(semaphore.Post());
  EXPECT_TRUE(semaphore.TryWait());

  // Trying to wait now should now fail.
  EXPECT_FALSE(semaphore.TryWait());

  ion::port::Timer timer;
  EXPECT_FALSE(semaphore.TimedWaitMs(300));
  EXPECT_FALSE(semaphore.TimedWaitMs(200));
  // Some platforms don't wait the full amount of time, and asmjs doesn't
  // wait at all.
#if !defined(ION_PLATFORM_ASMJS)
  EXPECT_GE(timer.GetInMs(), 450.);
#endif

  EXPECT_TRUE(semaphore.Post());
  timer.Reset();
  EXPECT_TRUE(semaphore.TimedWaitMs(500));
#if !defined(ION_PLATFORM_ASMJS)
  EXPECT_LT(timer.GetInMs(), 500.);
#endif

  EXPECT_TRUE(semaphore.Post());
  timer.Reset();
  // This will just execute Wait().
  EXPECT_TRUE(semaphore.TimedWaitMs(-1));
  EXPECT_LT(timer.GetInMs(), 500.);
}

TEST(Semaphore, InitialValue) {
  ion::port::Semaphore semaphore1(1);
  EXPECT_TRUE(semaphore1.Wait());

  ion::port::Semaphore semaphore2(2);
  EXPECT_TRUE(semaphore2.Wait());
  EXPECT_TRUE(semaphore2.Wait());

  ion::port::Semaphore semaphore3(3);
  EXPECT_TRUE(semaphore3.Wait());
  EXPECT_TRUE(semaphore3.Wait());
}

// 
