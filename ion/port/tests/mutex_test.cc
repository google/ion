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

#include "ion/port/mutex.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(Mutex, LockUnlockMutex) {
  ion::port::Mutex mutex;

  EXPECT_FALSE(mutex.IsLocked());
  mutex.Lock();
  EXPECT_TRUE(mutex.IsLocked());
  EXPECT_FALSE(mutex.TryLock());
  mutex.Unlock();
  EXPECT_FALSE(mutex.IsLocked());
  EXPECT_TRUE(mutex.TryLock());
  EXPECT_TRUE(mutex.IsLocked());
  mutex.Unlock();
  EXPECT_FALSE(mutex.IsLocked());
}

// TODO(user): Add better tests when we have threads.
