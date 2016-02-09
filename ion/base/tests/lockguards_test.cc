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

#include "ion/base/lockguards.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(LockGuards, LockAndUnlock) {
  ion::port::Mutex m;
  {
    ion::base::LockGuard guard(&m);
    EXPECT_TRUE(m.IsLocked());
    EXPECT_TRUE(guard.IsLocked());
  }
  EXPECT_FALSE(m.IsLocked());

  {
    ion::base::TryLockGuard guard(&m);
    EXPECT_TRUE(m.IsLocked());
    EXPECT_TRUE(guard.IsLocked());
  }
  EXPECT_FALSE(m.IsLocked());

  {
    ion::base::LockGuard* guard1 = new ion::base::LockGuard(&m);
    ion::base::TryLockGuard guard2(&m);
    EXPECT_TRUE(m.IsLocked());
    EXPECT_TRUE(guard1->IsLocked());
    EXPECT_FALSE(guard2.IsLocked());
    // Destroying guard1 should release the lock.
    delete guard1;
    EXPECT_FALSE(m.IsLocked());
    // The TryLockGuard can now Lock().
    guard2.Lock();
    EXPECT_TRUE(guard2.IsLocked());
  }
  EXPECT_FALSE(m.IsLocked());

  {
    ion::base::LockGuard guard(&m);
    EXPECT_TRUE(m.IsLocked());
    EXPECT_TRUE(guard.IsLocked());
    {
      ion::base::UnlockGuard guard2(&m);
      EXPECT_FALSE(m.IsLocked());
      // The guard should report locked, even though the mutex is not.
      EXPECT_TRUE(guard.IsLocked());
    }
    EXPECT_TRUE(m.IsLocked());
    EXPECT_TRUE(guard.IsLocked());
  }
  EXPECT_FALSE(m.IsLocked());
}

TEST(LockGuards, ManualLockGuard) {
  {
    ion::base::ManualLockGuard<int> mlg(13);
    EXPECT_FALSE(mlg.IsLocked());
    EXPECT_EQ(13, mlg.GetCurrentValue());
    mlg.SetAndLock(27);
    EXPECT_TRUE(mlg.IsLocked());
    EXPECT_EQ(27, mlg.ResetAndUnlock());
    EXPECT_FALSE(mlg.IsLocked());
    EXPECT_EQ(13, mlg.GetCurrentValue());
  }

  {
    // Test destruction while locked for coverage.
    ion::base::ManualLockGuard<int> mlg(16);
    EXPECT_FALSE(mlg.IsLocked());
    mlg.SetAndLock(42);
    EXPECT_TRUE(mlg.IsLocked());
  }
}

// TODO(user): Add more tests when threading is available.
