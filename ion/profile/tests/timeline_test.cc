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
#include "ion/profile/timelineevent.h"
#include "ion/profile/timelinenode.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"
#include "third_party/jsoncpp/include/json/value.h"

TimelineEvent* AddTimelineEvent(const uint32 start, const uint32 end,
                                const char* name, TimelineNode* parent) {
  TimelineEvent* event = new TimelineEvent(
      name, start, end - start, Json::nullValue);
  parent->AddChild(std::unique_ptr<TimelineEvent>(event));
  return event;
}

TEST(Timeline, NodeTimeMethods) {
  std::unique_ptr<TimelineNode> root(new TimelineNode("root"));
  TimelineEvent* x0 = AddTimelineEvent(7000, 10000, "A", root.get());
  EXPECT_EQ(7000U, x0->GetBegin());
  EXPECT_EQ(10000U, x0->GetEnd());
  EXPECT_EQ(3000U, x0->GetDuration());
  EXPECT_EQ(7.0, x0->GetBeginMs());
  EXPECT_EQ(10.0, x0->GetEndMs());
  EXPECT_EQ(3.0, x0->GetDurationMs());
}

TEST(Timeline, IteratorEmptyTimeline) {
  Timeline timeline;
  auto event = timeline.begin();
  EXPECT_EQ(event, timeline.end());
}

TEST(Timeline, IteratorComplexTimeline) {
  // 0         1         2         3         4
  // 01234567890123456789012345678901234567890
  // [             X0             ] A [  X7  ]
  //  [     X1     ] [   X4    ] A     C [X8]
  //   [X2] A [X3]    [X5] [X6]
  //                   B
  std::unique_ptr<TimelineNode> root(new TimelineNode("root"));
  TimelineEvent* x0 = AddTimelineEvent(0, 29, "X0", root.get());
  TimelineEvent* x1 = AddTimelineEvent(1, 14, "X1", x0);
  TimelineEvent* x2 = AddTimelineEvent(2, 5, "X2", x1);
  TimelineEvent* a0 = AddTimelineEvent(7, 7, "A", x1);
  TimelineEvent* x3 = AddTimelineEvent(9, 12, "X3", x1);
  TimelineEvent* x4 = AddTimelineEvent(16, 26, "X4", x0);
  TimelineEvent* x5 = AddTimelineEvent(17, 20, "X5", x4);
  TimelineEvent* b = AddTimelineEvent(18, 18, "B", x5);
  TimelineEvent* x6 = AddTimelineEvent(22, 25, "X6", x4);
  TimelineEvent* a1 = AddTimelineEvent(28, 28, "A", x0);
  TimelineEvent* a2 = AddTimelineEvent(31, 31, "A", root.get());
  TimelineEvent* x7 = AddTimelineEvent(33, 40, "X6", root.get());
  TimelineEvent* c = AddTimelineEvent(34, 34, "C", x7);
  TimelineEvent* x8 = AddTimelineEvent(36, 39, "X8", x7);
  Timeline timeline(std::move(root));

  TimelineEvent* expected_order[] = {x0, x1, x2, a0, x3, x4, x5,
                                     b,  x6, a1, a2, x7, c,  x8};
  int index = 0;
  for (auto event : timeline) EXPECT_EQ(event, expected_order[index++]);
}
