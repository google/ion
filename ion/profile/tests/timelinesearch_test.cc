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

#include <memory>
#include <string>

#include "ion/profile/timeline.h"
#include "ion/profile/timelinenode.h"
#include "ion/profile/timelinerange.h"
#include "ion/profile/timelinescope.h"
#include "ion/profile/timelinesearch.h"
#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"
#include "third_party/jsoncpp/include/json/value.h"

static std::unique_ptr<Timeline> CreateTimelineWithSingleScope(
    const char* name) {
  std::unique_ptr<TimelineNode> root(new TimelineNode("root"));
  std::unique_ptr<TimelineScope> scope(
      new TimelineScope(name, 0, 0, Json::nullValue));
  root->AddChild(std::move(scope));
  return absl::make_unique<Timeline>(std::move(root));
}

static TimelineRange* AddRange(const uint32 start, const uint32 end,
                               const char* name, TimelineNode* parent) {
  TimelineRange* range =
      new TimelineRange(name, start, end - start, Json::nullValue);
  parent->AddChild(std::unique_ptr<TimelineRange>(range));
  return range;
}

static TimelineScope* AddScope(const uint32 start, const uint32 end,
                               const char* name, TimelineNode* parent) {
  TimelineScope* scope =
      new TimelineScope(name, start, end - start, Json::nullValue);
  parent->AddChild(std::unique_ptr<TimelineScope>(scope));
  return scope;
}

TEST(TimelineSearch, EmptyTimeline) {
  Timeline timeline;
  TimelineSearch search(timeline, TimelineNode::Type::kScope,
                        std::string("NotInTimeline"));
  EXPECT_TRUE(search.empty());
}

TEST(TimelineSearch, DontFindInSimpleTimeline) {
  std::unique_ptr<Timeline> timeline =
      CreateTimelineWithSingleScope("InTimeline");
  TimelineSearch search(*timeline, TimelineNode::Type::kScope,
                        std::string("NotInTimeline"));
  EXPECT_TRUE(search.empty());
}

TEST(TimelineSearch, FindInSimpleTimeline) {
  std::unique_ptr<Timeline> timeline =
      CreateTimelineWithSingleScope("InTimeline");
  TimelineSearch search(*timeline, TimelineNode::Type::kScope,
                        std::string("InTimeline"));
  auto iter = search.begin();
  EXPECT_NE(iter, search.end());
  EXPECT_EQ((*iter)->GetName(), std::string("InTimeline"));
  ++iter;
  EXPECT_EQ(iter, search.end());
}

TEST(TimelineSearch, FindInComplexTimeline) {
  // 0         1         2         3         4
  // 01234567890123456789012345678901234567890
  // [             R0             ] A [  X7  ]
  //  [     X1     ] [   X4    ] A     C [X8]
  //   [X2] A [X3]    [X5] [X6]
  //                   B
  std::unique_ptr<TimelineNode> root(new TimelineNode("root"));
  TimelineRange* r0 = AddRange(0, 29, "R0", root.get());
  TimelineScope* x1 = AddScope(1, 14, "X1", r0);
  TimelineScope* x2 = AddScope(2, 5, "X2", x1);
  TimelineScope* a0 = AddScope(7, 7, "A", x1);
  TimelineScope* x3 = AddScope(9, 12, "X3", x1);
  TimelineScope* x4 = AddScope(16, 26, "X4", r0);
  TimelineScope* x5 = AddScope(17, 20, "X5", x4);
  AddScope(18, 18, "B", x5);
  AddScope(22, 25, "X6", x4);
  TimelineScope* a1 = AddScope(28, 28, "A", r0);
  TimelineScope* a2 = AddScope(31, 31, "A", root.get());
  TimelineScope* x7 = AddScope(33, 40, "X6", root.get());
  AddScope(34, 34, "C", x7);
  AddScope(36, 39, "X8", x7);
  Timeline timeline((std::move(root)));

  TimelineSearch search_range(timeline, TimelineNode::Type::kRange);
  auto iter_range = search_range.begin();
  EXPECT_EQ(*iter_range, r0);
  ++iter_range;
  EXPECT_EQ(iter_range, search_range.end());

  TimelineSearch search_a_scopes(timeline, TimelineNode::Type::kScope,
                                 std::string("A"));
  auto iter_a_scopes = search_a_scopes.begin();
  EXPECT_EQ(*iter_a_scopes, a0);
  ++iter_a_scopes;
  EXPECT_EQ(*iter_a_scopes, a1);
  ++iter_a_scopes;
  EXPECT_EQ(*iter_a_scopes, a2);
  ++iter_a_scopes;
  EXPECT_EQ(iter_a_scopes, search_a_scopes.end());

  TimelineSearch search_scopes_in_range(timeline, TimelineNode::Type::kScope,
                                        x1->GetBegin(), x1->GetEnd());
  auto iter_scopes_in_range = search_scopes_in_range.begin();
  EXPECT_EQ(*iter_scopes_in_range, x1);
  ++iter_scopes_in_range;
  EXPECT_EQ(*iter_scopes_in_range, x2);
  ++iter_scopes_in_range;
  EXPECT_EQ(*iter_scopes_in_range, a0);
  ++iter_scopes_in_range;
  EXPECT_EQ(*iter_scopes_in_range, x3);
  ++iter_scopes_in_range;
  EXPECT_EQ(iter_scopes_in_range, search_scopes_in_range.end());

  TimelineSearch search_named_scopes_in_range(
      timeline, TimelineNode::Type::kScope, std::string("A"), x1->GetBegin(),
      x1->GetEnd());
  auto iter_named_scopes_in_range = search_named_scopes_in_range.begin();
  EXPECT_EQ(*iter_named_scopes_in_range, a0);
  ++iter_named_scopes_in_range;
  EXPECT_EQ(iter_named_scopes_in_range, search_named_scopes_in_range.end());
}
