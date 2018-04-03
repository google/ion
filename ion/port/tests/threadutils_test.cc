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

#include "ion/port/threadutils.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace port {

// This ThreadId is set by SpawnedFunc to allow the main thread to verify that
// it received the correct ThreadId.
static std::thread::id s_spawned_id;

//-----------------------------------------------------------------------------
//
// Thread callback functions.
//
//-----------------------------------------------------------------------------

// Tests thread naming.
static bool NamingFunc() {
  EXPECT_TRUE(IsThreadNamingSupported());

  const std::string name("Some Name");
  EXPECT_TRUE(SetThreadName(name));

  s_spawned_id = std::this_thread::get_id();
  return true;
}

// Tests thread-local storage. It is passed the key created for the
// thread-local storage of the calling thread.
static bool LocalStorageFunc(const ThreadLocalStorageKey& key) {
  // Storage for this thread should start as NULL.
  EXPECT_TRUE(GetThreadLocalStorage(key) == nullptr);

  // Create storage for this thread.
  int my_storage;
  EXPECT_TRUE(SetThreadLocalStorage(key, &my_storage));
  EXPECT_EQ(&my_storage, GetThreadLocalStorage(key));

  // Reset to NULL.
  EXPECT_TRUE(SetThreadLocalStorage(key, nullptr));
  EXPECT_TRUE(GetThreadLocalStorage(key) == nullptr);

  s_spawned_id = std::this_thread::get_id();
  return true;
}

//-----------------------------------------------------------------------------
//
// Tests.
//
//-----------------------------------------------------------------------------

TEST(ThreadUtils, Naming) {
  if (IsThreadNamingSupported()) {
    // Spawn a thread that names itself and tests the name.
    std::thread naming_thread(NamingFunc);
    EXPECT_NE(std::thread::id(), naming_thread.get_id());
    naming_thread.join();
  } else {
    EXPECT_FALSE(SetThreadName("Does not matter"));

    // For coverage.
    EXPECT_EQ(0U, GetMaxThreadNameLength());
  }

  s_spawned_id = std::thread::id();
}

TEST(ThreadUtils, LocalStorage) {
  int storage;

  // Test main thread.
  ThreadLocalStorageKey key = CreateThreadLocalStorageKey();
  EXPECT_NE(kInvalidThreadLocalStorageKey, key);
  EXPECT_TRUE(SetThreadLocalStorage(key, &storage));
  EXPECT_EQ(&storage, GetThreadLocalStorage(key));

  // Test another thread.
  std::thread thread(LocalStorageFunc, key);
  thread.join();

  // Local storage for this thread should not have changed.
  EXPECT_EQ(&storage, GetThreadLocalStorage(key));

  // Reset to NULL.
  EXPECT_TRUE(SetThreadLocalStorage(key, nullptr));
  EXPECT_TRUE(GetThreadLocalStorage(key) == nullptr);

  // Delete the key.
  EXPECT_TRUE(DeleteThreadLocalStorageKey(key));

  // These should fail.
  EXPECT_FALSE(DeleteThreadLocalStorageKey(kInvalidThreadLocalStorageKey));
  EXPECT_FALSE(SetThreadLocalStorage(kInvalidThreadLocalStorageKey, nullptr));
}

}  // namespace port
}  // namespace ion
