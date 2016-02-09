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

#include "ion/base/threadspawner.h"

#include "ion/port/barrier.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

//-----------------------------------------------------------------------------
//
// Simple thread callback function that sets a static variable.
//
//-----------------------------------------------------------------------------

static bool s_sample_func_was_called = false;

static bool SampleFunc() {
  s_sample_func_was_called = true;
  return true;
}

//-----------------------------------------------------------------------------
//
// Thread callback helper. This uses port::Barriers to allow the calling thread
// to step through the various stages of execution.
//
//-----------------------------------------------------------------------------

class ThreadCallbackHelper {
 public:
  ThreadCallbackHelper()
      : barrier1_(2),
        barrier2_(2),
        id_(port::kInvalidThreadId) {}

  port::Barrier* GetBarrier1() { return &barrier1_; }
  port::Barrier* GetBarrier2() { return &barrier2_; }

  port::ThreadId GetId() const { return id_; }

  bool Run() {
    EXPECT_EQ(port::kInvalidThreadId, id_);
    id_ = port::GetCurrentThreadId();

    // Wait for the barrier before finishing.
    barrier2_.Wait();
    return true;
  }

 private:
  port::Barrier barrier1_;
  port::Barrier barrier2_;
  port::ThreadId id_;
};

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Tests.
//
//-----------------------------------------------------------------------------

TEST(ThreadSpawner, SpawnPtr) {
  EXPECT_FALSE(s_sample_func_was_called);
  {
    // Create a thread and execute SampleFunc().
    ThreadSpawner ts("threaddy", SampleFunc);
    EXPECT_EQ("threaddy", ts.GetName());
    EXPECT_NE(port::kInvalidThreadId, ts.GetId());
  }
  EXPECT_TRUE(s_sample_func_was_called);
  s_sample_func_was_called = false;
}

TEST(ThreadSpawner, SpawnNull) {
  // Create a thread with a NULL function. The thread ID should be invalid.
  ThreadSpawner ts("Nully", NULL);
  EXPECT_EQ("Nully", ts.GetName());
  EXPECT_EQ(port::kInvalidThreadId, ts.GetId());
}

TEST(ThreadSpawner, SpawnStd) {
  ThreadCallbackHelper tch;
  EXPECT_EQ(port::kInvalidThreadId, tch.GetId());
  {
    // Create a thread and execute it.
    ThreadSpawner ts("Stupid Name",
                     std::bind(&ThreadCallbackHelper::Run, &tch));

    // Once the thread hits the barrier, the ID should be set.
    tch.GetBarrier2()->Wait();
    EXPECT_EQ(tch.GetId(), ts.GetId());
  }
}

TEST(ThreadSpawner, Join) {
  // Same as above test, but call Join() before the thread goes away.
  ThreadCallbackHelper tch;
  {
    ThreadSpawner ts("Thread to join",
                     std::bind(&ThreadCallbackHelper::Run, &tch));

    // Let the thread get past the barrier so it can be joined.
    tch.GetBarrier2()->Wait();
    EXPECT_EQ(tch.GetId(), ts.GetId());

    // Join the ts. It should then have an invalid ID.
    ts.Join();
    EXPECT_EQ(port::kInvalidThreadId, ts.GetId());
  }
}

}  // namespace base
}  // namespace ion
