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

#include "ion/port/threadutils.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace port {

// This ThreadId is set by SpawnedFunc to allow the main thread to verify that
// it received the correct ThreadId.
static ThreadId s_spawned_id = kInvalidThreadId;

//-----------------------------------------------------------------------------
//
// Thread callback functions.
//
//-----------------------------------------------------------------------------

// Does nothing but return true.
static bool EmptyFunc() {
  EXPECT_FALSE(IsMainThread());
  s_spawned_id = GetCurrentThreadId();
  return true;
}

// Tests thread naming.
static bool NamingFunc() {
  EXPECT_TRUE(IsThreadNamingSupported());
  EXPECT_FALSE(IsMainThread());

  const std::string name("Some Name");
  EXPECT_TRUE(SetThreadName(name));

  s_spawned_id = GetCurrentThreadId();
  return true;
}

// Tests passing an argument to a function.
static bool FuncWithIntArg(int arg) {
  EXPECT_FALSE(IsMainThread());
  const ThreadId my_id = GetCurrentThreadId();
  EXPECT_EQ(42, arg);

  s_spawned_id = my_id;
  return true;
}

// Tests thread-local storage. It is passed the key created for the
// thread-local storage of the calling thread.
static bool LocalStorageFunc(const ThreadLocalStorageKey& key) {
  // Storage for this thread should start as NULL.
  EXPECT_TRUE(GetThreadLocalStorage(key) == NULL);

  // Create storage for this thread.
  int my_storage;
  EXPECT_TRUE(SetThreadLocalStorage(key, &my_storage));
  EXPECT_EQ(&my_storage, GetThreadLocalStorage(key));

  // Reset to NULL.
  EXPECT_TRUE(SetThreadLocalStorage(key, NULL));
  EXPECT_TRUE(GetThreadLocalStorage(key) == NULL);

  s_spawned_id = GetCurrentThreadId();
  return true;
}

//-----------------------------------------------------------------------------
//
// Tests.
//
//-----------------------------------------------------------------------------

TEST(ThreadUtils, MainThread) {
  // For coverage. There is no way to guarantee that the test is run in a main
  // thread, so this may or may not return true.
  IsMainThread();

  // Force the current thread to be the main thread.
  SetMainThreadId(GetCurrentThreadId());
  EXPECT_TRUE(IsMainThread());

  // This should have no effect.
  SetMainThreadId(kInvalidThreadId);
  EXPECT_TRUE(IsMainThread());
}

#if !defined(ION_PLATFORM_ASMJS)
TEST(ThreadUtils, SpawnAndJoin) {
  ThreadId my_id = GetCurrentThreadId();

  EXPECT_TRUE(IsMainThread());
  ThreadId id = SpawnThread(EmptyFunc);
  EXPECT_NE(kInvalidThreadId, id);
  EXPECT_EQ(my_id, GetCurrentThreadId());

  YieldThread();  // For coverage.
  bool join_succeeded = JoinThread(id);
  EXPECT_TRUE(join_succeeded);

  // Now that the spawned thread is finished, make sure the ID's match.
  EXPECT_EQ(id, s_spawned_id);

  EXPECT_EQ(my_id, GetCurrentThreadId());
  s_spawned_id = kInvalidThreadId;
}

TEST(ThreadUtils, Naming) {
  if (IsThreadNamingSupported()) {
    // Spawn a thread that names itself and tests the name.
    ThreadId id = SpawnThread(NamingFunc);
    EXPECT_NE(kInvalidThreadId, id);
    JoinThread(id);
  } else {
    EXPECT_FALSE(SetThreadName("Does not matter"));

    // For coverage.
    EXPECT_EQ(0U, GetMaxThreadNameLength());
  }

  s_spawned_id = kInvalidThreadId;
}

TEST(ThreadUtils, StdFunc) {
  // Spawn a thread with a function that takes an integer.
  ThreadStdFunc func(std::bind(FuncWithIntArg, 42));
  ThreadId id = SpawnThreadStd(&func);
  JoinThread(id);
  s_spawned_id = kInvalidThreadId;
}

TEST(ThreadUtils, LocalStorage) {
  int storage;

  // Test main thread.
  ThreadLocalStorageKey key = CreateThreadLocalStorageKey();
  EXPECT_NE(kInvalidThreadLocalStorageKey, key);
  EXPECT_TRUE(SetThreadLocalStorage(key, &storage));
  EXPECT_EQ(&storage, GetThreadLocalStorage(key));

  // Test another thread.
  ThreadStdFunc func(std::bind(LocalStorageFunc, key));
  ThreadId id = SpawnThreadStd(&func);
  JoinThread(id);

  // Local storage for this thread should not have changed.
  EXPECT_EQ(&storage, GetThreadLocalStorage(key));

  // Reset to NULL.
  EXPECT_TRUE(SetThreadLocalStorage(key, NULL));
  EXPECT_TRUE(GetThreadLocalStorage(key) == NULL);

  // Delete the key.
  EXPECT_TRUE(DeleteThreadLocalStorageKey(key));

  // These should fail.
  EXPECT_FALSE(DeleteThreadLocalStorageKey(kInvalidThreadLocalStorageKey));
  EXPECT_FALSE(SetThreadLocalStorage(kInvalidThreadLocalStorageKey, NULL));
}
#endif  // !ION_PLATFORM_ASMJS

TEST(ThreadUtils, JoinWithInvalid) {
  EXPECT_TRUE(IsMainThread());

  bool join_succeeded = JoinThread(kInvalidThreadId);
  EXPECT_FALSE(join_succeeded);
}

#if !defined(ION_PLATFORM_WINDOWS)
TEST(ThreadUtils, JoinWithSelf) {
  ThreadId my_id = GetCurrentThreadId();
  EXPECT_TRUE(IsMainThread());

  bool join_succeeded = JoinThread(my_id);
  EXPECT_FALSE(join_succeeded);
}
#endif  // !ION_PLATFORM_WINDOWS

}  // namespace port
}  // namespace ion
