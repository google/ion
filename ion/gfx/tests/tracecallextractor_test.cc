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

#include "ion/gfx/tracecallextractor.h"

#include <string>

#include "ion/base/invalid.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

TEST(TraceCallExtractorTest, Defaults) {
  TraceCallExtractor tce;
  EXPECT_EQ(0U, tce.GetCallCount());
  EXPECT_EQ(0U, tce.GetCalls().size());
}

TEST(TraceCallExtractorTest, Calls) {
  const std::string calls1(
      "func1(arg1 = horse, arg2 = hippo)\n"
      "func1(arg1 = horse, arg2 = dog)\n"
      "func1(arg1 = horse, arg2 = cat)\n"
      "func1(arg1 = echidna, arg2 = orangutan)\n"
      "func1(arg1 = echidna, arg2 = orangutan)\n"
      "func2(arg3 = hippo)\n"
      "func2(arg3 = emu)\n"
      "func2(arg3 = horse)\n");
  const std::string calls2("third(arg3 = groundhog)\n");
  TraceCallExtractor tce(calls1);
  EXPECT_EQ(8U, tce.GetCallCount());
  EXPECT_EQ(8U, tce.GetCalls().size());
  EXPECT_EQ(8U, tce.GetCountOf("func"));
  EXPECT_EQ(5U, tce.GetCountOf("func1"));
  EXPECT_EQ(3U, tce.GetCountOf("func2"));
  EXPECT_EQ(0U, tce.GetCountOf("thir"));

  tce.SetTrace(calls1 + calls2);
  EXPECT_EQ(9U, tce.GetCallCount());
  EXPECT_EQ(9U, tce.GetCalls().size());
  EXPECT_EQ(8U, tce.GetCountOf("func"));
  EXPECT_EQ(5U, tce.GetCountOf("func1"));
  EXPECT_EQ(3U, tce.GetCountOf("func2"));
  EXPECT_EQ(1U, tce.GetCountOf("thir"));

  EXPECT_EQ(3U, tce.GetCountOf({{0, "func1"}, {1, "horse"}}));
  EXPECT_EQ(1U, tce.GetCountOf({{0, "func1"}, {2, "hippo"}}));
  EXPECT_EQ(1U, tce.GetCountOf({{0, "func2"}, {1, "emu"}}));
  EXPECT_EQ(0U, tce.GetCountOf({{0, "func1"}, {2, "koala"}}));
  EXPECT_EQ(4U, tce.GetCountOf({{1, "horse"}}));
  EXPECT_EQ(1U, tce.GetCountOf({{0, "third"}}));

  EXPECT_EQ(0U, tce.GetNthIndexOf(0U, "func"));
  EXPECT_EQ(1U, tce.GetNthIndexOf(1U, "func"));
  EXPECT_EQ(3U, tce.GetNthIndexOf(0U, {{0, "func1"}, {1, "echidna"}}));
  EXPECT_EQ(4U, tce.GetNthIndexOf(1U, {{0, "func1"}, {1, "echidna"}}));

  tce.SetTrace(calls2 + calls1);
  EXPECT_EQ(1U, tce.GetNthIndexOf(0U, "func"));
  EXPECT_EQ(2U, tce.GetNthIndexOf(1U, "func"));
  EXPECT_EQ(4U, tce.GetNthIndexOf(0U, {{0, "func1"}, {1, "echidna"}}));
  EXPECT_EQ(5U, tce.GetNthIndexOf(1U, {{0, "func1"}, {1, "echidna"}}));

  EXPECT_EQ(base::kInvalidIndex, tce.GetNthIndexOf(8U, "func"));
  EXPECT_EQ(base::kInvalidIndex, tce.GetNthIndexOf(0U, "nosuchcall"));
  EXPECT_EQ(base::kInvalidIndex, tce.GetNthIndexOf(0U, {{0, "nosuchcall"}}));
  EXPECT_EQ(base::kInvalidIndex, tce.GetNthIndexOf(0U, {{1, "porcupine"}}));
}

}  // namespace gfx
}  // namespace ion
