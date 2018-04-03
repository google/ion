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

#include "ion/gfxutils/frame.h"

#include <functional>

#include "ion/base/logchecker.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfxutils {

namespace {

// Simple class for testing callbacks.
class CallbackTester {
 public:
  CallbackTester() : call_count_(0) {}
  void Callback(const Frame&) { ++call_count_; }
  size_t GetCallCount() const { return call_count_; }
  void Reset() { call_count_ = 0; }

 private:
  size_t call_count_;
};

}  // anonymous namespace

TEST(FrameTest, BeginEnd) {
  base::LogChecker log_checker;

  // Matched Begin/End.
  FramePtr frame(new Frame);
  frame->Begin();
  frame->End();
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Unmatched End.
  frame->End();
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "End() called while not in a frame"));

  // Nested Begin.
  frame->Begin();
  frame->Begin();
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "Begin() called while already in a frame"));

  // Back to normal.
  frame->End();
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(FrameTest, IsInFrame) {
  FramePtr frame(new Frame);
  EXPECT_FALSE(frame->IsInFrame());
  frame->Begin();
  EXPECT_TRUE(frame->IsInFrame());
  frame->End();
  EXPECT_FALSE(frame->IsInFrame());
  frame->Begin();
  EXPECT_TRUE(frame->IsInFrame());
  frame->End();
  EXPECT_FALSE(frame->IsInFrame());
}

TEST(FrameTest, Counter) {
  FramePtr frame(new Frame);
  EXPECT_EQ(0U, frame->GetCounter());

  frame->Begin();
  EXPECT_EQ(0U, frame->GetCounter());
  frame->End();
  EXPECT_EQ(1U, frame->GetCounter());

  frame->Begin();
  frame->End();
  frame->Begin();
  frame->End();
  EXPECT_EQ(3U, frame->GetCounter());

  frame->ResetCounter();
  EXPECT_EQ(0U, frame->GetCounter());

  frame->Begin();
  frame->End();
  EXPECT_EQ(1U, frame->GetCounter());
}

TEST(FrameTest, Callbacks) {
  using std::bind;
  using std::placeholders::_1;

  CallbackTester c1, c2;

  FramePtr frame(new Frame);
  frame->AddPreFrameCallback("pre1", bind(&CallbackTester::Callback, &c1, _1));
  EXPECT_EQ(0U, c1.GetCallCount());
  frame->Begin();
  EXPECT_EQ(1U, c1.GetCallCount());
  frame->End();
  frame->Begin();
  EXPECT_EQ(2U, c1.GetCallCount());
  frame->End();

  c1.Reset();
  frame->AddPreFrameCallback("pre2", bind(&CallbackTester::Callback, &c2, _1));
  EXPECT_EQ(0U, c1.GetCallCount());
  EXPECT_EQ(0U, c2.GetCallCount());
  frame->Begin();
  EXPECT_EQ(1U, c1.GetCallCount());
  EXPECT_EQ(1U, c2.GetCallCount());
  frame->End();
  frame->Begin();
  EXPECT_EQ(2U, c1.GetCallCount());
  EXPECT_EQ(2U, c2.GetCallCount());
  frame->End();
  EXPECT_TRUE(frame->RemovePreFrameCallback("pre1"));
  frame->Begin();
  EXPECT_EQ(2U, c1.GetCallCount());
  EXPECT_EQ(3U, c2.GetCallCount());
  frame->End();
  EXPECT_FALSE(frame->RemovePreFrameCallback("not_there"));
  EXPECT_FALSE(frame->RemovePreFrameCallback("pre1"));
  EXPECT_TRUE(frame->RemovePreFrameCallback("pre2"));
  frame->Begin();
  EXPECT_EQ(2U, c1.GetCallCount());
  EXPECT_EQ(3U, c2.GetCallCount());
  frame->End();

  c1.Reset();
  c2.Reset();
  frame->AddPostFrameCallback("post1",
                              bind(&CallbackTester::Callback, &c1, _1));
  EXPECT_EQ(0U, c1.GetCallCount());
  frame->Begin();
  EXPECT_EQ(0U, c1.GetCallCount());
  frame->End();
  EXPECT_EQ(1U, c1.GetCallCount());
  frame->Begin();
  EXPECT_EQ(1U, c1.GetCallCount());
  frame->End();
  EXPECT_EQ(2U, c1.GetCallCount());

  c1.Reset();
  c2.Reset();
  frame->AddPostFrameCallback("post2",
                              bind(&CallbackTester::Callback, &c2, _1));
  EXPECT_EQ(0U, c1.GetCallCount());
  EXPECT_EQ(0U, c2.GetCallCount());
  frame->Begin();
  EXPECT_EQ(0U, c1.GetCallCount());
  EXPECT_EQ(0U, c2.GetCallCount());
  frame->End();
  EXPECT_EQ(1U, c1.GetCallCount());
  EXPECT_EQ(1U, c2.GetCallCount());
  frame->Begin();
  frame->End();
  EXPECT_EQ(2U, c1.GetCallCount());
  EXPECT_EQ(2U, c2.GetCallCount());
  EXPECT_TRUE(frame->RemovePostFrameCallback("post1"));
  frame->Begin();
  frame->End();
  EXPECT_EQ(2U, c1.GetCallCount());
  EXPECT_EQ(3U, c2.GetCallCount());
  EXPECT_FALSE(frame->RemovePostFrameCallback("not_there"));
  EXPECT_FALSE(frame->RemovePostFrameCallback("post1"));
  EXPECT_TRUE(frame->RemovePostFrameCallback("post2"));
  frame->Begin();
  frame->End();
  EXPECT_EQ(2U, c1.GetCallCount());
  EXPECT_EQ(3U, c2.GetCallCount());
}

}  // namespace gfxutils
}  // namespace ion
