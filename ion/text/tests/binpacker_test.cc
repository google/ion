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

#include "ion/text/binpacker.h"

#include "ion/math/vector.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

TEST(BinPackerTest, OneBin) {
  BinPacker bp;
  bp.AddRectangle(0, math::Vector2ui(10, 10));
  EXPECT_TRUE(bp.Pack(math::Vector2ui(10, 10)));
  const std::vector<BinPacker::Rectangle>& rects = bp.GetRectangles();
  ASSERT_EQ(1U, rects.size());
  EXPECT_EQ(0U, rects[0].id);
  EXPECT_EQ(math::Vector2ui(10, 10), rects[0].size);
  EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
}

TEST(BinPackerTest, TwoRectsHorizontal) {
  BinPacker bp;
  bp.AddRectangle(0, math::Vector2ui(10, 10));
  bp.AddRectangle(1, math::Vector2ui(10, 10));
  EXPECT_TRUE(bp.Pack(math::Vector2ui(20, 10)));
  const std::vector<BinPacker::Rectangle>& rects = bp.GetRectangles();
  ASSERT_EQ(2U, rects.size());
  EXPECT_EQ(0U, rects[0].id);
  EXPECT_EQ(math::Vector2ui(10, 10), rects[0].size);
  EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
  EXPECT_EQ(1U, rects[1].id);
  EXPECT_EQ(math::Vector2ui(10, 10), rects[1].size);
  EXPECT_EQ(math::Point2ui(10, 0), rects[1].bottom_left);
}

TEST(BinPackerTest, TwoRectsVertical) {
  BinPacker bp;
  bp.AddRectangle(0, math::Vector2ui(10, 10));
  bp.AddRectangle(1, math::Vector2ui(10, 10));
  EXPECT_TRUE(bp.Pack(math::Vector2ui(10, 20)));
  const std::vector<BinPacker::Rectangle>& rects = bp.GetRectangles();
  ASSERT_EQ(2U, rects.size());
  EXPECT_EQ(0U, rects[0].id);
  EXPECT_EQ(math::Vector2ui(10, 10), rects[0].size);
  EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
  EXPECT_EQ(1U, rects[1].id);
  EXPECT_EQ(math::Vector2ui(10, 10), rects[1].size);
  EXPECT_EQ(math::Point2ui(0, 10), rects[1].bottom_left);
}

TEST(BinPackerTest, FourRects) {
  // Looks something like this (not to scale):
  //        8      12
  //      _______________
  //    2 |____|        |
  //      |    |        | 8
  //   10 |    |________|
  //      |    |        | 4
  //      ---------------
  //
  // Bin 0 is the bottom left, 1 is to the right of it, 2 is above 1, and 3 is
  // above 0.
  BinPacker bp;
  bp.AddRectangle(0, math::Vector2ui(8, 10));
  bp.AddRectangle(1, math::Vector2ui(12, 4));
  bp.AddRectangle(2, math::Vector2ui(12, 8));
  bp.AddRectangle(3, math::Vector2ui(8, 2));
  EXPECT_TRUE(bp.Pack(math::Vector2ui(20, 12)));
  const std::vector<BinPacker::Rectangle>& rects = bp.GetRectangles();
  ASSERT_EQ(4U, rects.size());
  EXPECT_EQ(0U, rects[0].id);
  EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
  EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
  EXPECT_EQ(1U, rects[1].id);
  EXPECT_EQ(math::Vector2ui(12, 4), rects[1].size);
  EXPECT_EQ(math::Point2ui(8, 0), rects[1].bottom_left);
  EXPECT_EQ(2U, rects[2].id);
  EXPECT_EQ(math::Vector2ui(12, 8), rects[2].size);
  EXPECT_EQ(math::Point2ui(8, 4), rects[2].bottom_left);
  EXPECT_EQ(3U, rects[3].id);
  EXPECT_EQ(math::Vector2ui(8, 2), rects[3].size);
  EXPECT_EQ(math::Point2ui(0, 10), rects[3].bottom_left);
}

TEST(BinPackerTest, FourRectsIncremental) {
  // This is the same as the FourRects test but with incremental packing.
  BinPacker bp;
  bp.AddRectangle(0, math::Vector2ui(8, 10));
  EXPECT_TRUE(bp.Pack(math::Vector2ui(20, 12)));
  {
    const std::vector<BinPacker::Rectangle>& rects = bp.GetRectangles();
    ASSERT_EQ(1U, rects.size());
    EXPECT_EQ(0U, rects[0].id);
    EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
    EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
  }
  bp.AddRectangle(1, math::Vector2ui(12, 4));
  EXPECT_TRUE(bp.Pack(math::Vector2ui(20, 12)));
  {
    const std::vector<BinPacker::Rectangle>& rects = bp.GetRectangles();
    ASSERT_EQ(2U, rects.size());
    EXPECT_EQ(0U, rects[0].id);
    EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
    EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
    EXPECT_EQ(1U, rects[1].id);
    EXPECT_EQ(math::Vector2ui(12, 4), rects[1].size);
    EXPECT_EQ(math::Point2ui(8, 0), rects[1].bottom_left);
  }
  bp.AddRectangle(2, math::Vector2ui(12, 8));
  EXPECT_TRUE(bp.Pack(math::Vector2ui(20, 12)));
  {
    const std::vector<BinPacker::Rectangle>& rects = bp.GetRectangles();
    ASSERT_EQ(3U, rects.size());
    EXPECT_EQ(0U, rects[0].id);
    EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
    EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
    EXPECT_EQ(1U, rects[1].id);
    EXPECT_EQ(math::Vector2ui(12, 4), rects[1].size);
    EXPECT_EQ(math::Point2ui(8, 0), rects[1].bottom_left);
    EXPECT_EQ(2U, rects[2].id);
    EXPECT_EQ(math::Vector2ui(12, 8), rects[2].size);
    EXPECT_EQ(math::Point2ui(8, 4), rects[2].bottom_left);
  }
  bp.AddRectangle(3, math::Vector2ui(8, 2));
  EXPECT_TRUE(bp.Pack(math::Vector2ui(20, 12)));
  {
    const std::vector<BinPacker::Rectangle>& rects = bp.GetRectangles();
    ASSERT_EQ(4U, rects.size());
    EXPECT_EQ(0U, rects[0].id);
    EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
    EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
    EXPECT_EQ(1U, rects[1].id);
    EXPECT_EQ(math::Vector2ui(12, 4), rects[1].size);
    EXPECT_EQ(math::Point2ui(8, 0), rects[1].bottom_left);
    EXPECT_EQ(2U, rects[2].id);
    EXPECT_EQ(math::Vector2ui(12, 8), rects[2].size);
    EXPECT_EQ(math::Point2ui(8, 4), rects[2].bottom_left);
    EXPECT_EQ(3U, rects[3].id);
    EXPECT_EQ(math::Vector2ui(8, 2), rects[3].size);
    EXPECT_EQ(math::Point2ui(0, 10), rects[3].bottom_left);
  }
}

TEST(BinPackerTest, NoFit) {
  BinPacker bp;

  // Single rectangle. The rectangle will not fit in either packing area.
  bp.AddRectangle(0, math::Vector2ui(10, 10));
  EXPECT_FALSE(bp.Pack(math::Vector2ui(10, 9)));
  EXPECT_FALSE(bp.Pack(math::Vector2ui(9, 10)));

  // Add another rectangle and try to pack both into too-small areas.
  bp.AddRectangle(0, math::Vector2ui(20, 20));
  EXPECT_FALSE(bp.Pack(math::Vector2ui(9, 30)));
  EXPECT_FALSE(bp.Pack(math::Vector2ui(29, 10)));
}

TEST(BinPackerTest, CopyAndAssign) {
  // This uses the same data as the FourRects test.
  BinPacker bp;
  bp.AddRectangle(0, math::Vector2ui(8, 10));
  bp.AddRectangle(1, math::Vector2ui(12, 4));
  bp.AddRectangle(2, math::Vector2ui(12, 8));
  bp.AddRectangle(3, math::Vector2ui(8, 2));

  // Test before packing.
  {
    BinPacker bp_copy(bp);
    EXPECT_TRUE(bp_copy.Pack(math::Vector2ui(20, 12)));
    const std::vector<BinPacker::Rectangle>& rects = bp_copy.GetRectangles();
    ASSERT_EQ(4U, rects.size());
    EXPECT_EQ(0U, rects[0].id);
    EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
    EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
    EXPECT_EQ(1U, rects[1].id);
    EXPECT_EQ(math::Vector2ui(12, 4), rects[1].size);
    EXPECT_EQ(math::Point2ui(8, 0), rects[1].bottom_left);
    EXPECT_EQ(2U, rects[2].id);
    EXPECT_EQ(math::Vector2ui(12, 8), rects[2].size);
    EXPECT_EQ(math::Point2ui(8, 4), rects[2].bottom_left);
    EXPECT_EQ(3U, rects[3].id);
    EXPECT_EQ(math::Vector2ui(8, 2), rects[3].size);
    EXPECT_EQ(math::Point2ui(0, 10), rects[3].bottom_left);
  }
  {
    BinPacker bp_assign;
    bp_assign = bp;
    EXPECT_TRUE(bp_assign.Pack(math::Vector2ui(20, 12)));
    const std::vector<BinPacker::Rectangle>& rects = bp_assign.GetRectangles();
    ASSERT_EQ(4U, rects.size());
    EXPECT_EQ(0U, rects[0].id);
    EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
    EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
    EXPECT_EQ(1U, rects[1].id);
    EXPECT_EQ(math::Vector2ui(12, 4), rects[1].size);
    EXPECT_EQ(math::Point2ui(8, 0), rects[1].bottom_left);
    EXPECT_EQ(2U, rects[2].id);
    EXPECT_EQ(math::Vector2ui(12, 8), rects[2].size);
    EXPECT_EQ(math::Point2ui(8, 4), rects[2].bottom_left);
    EXPECT_EQ(3U, rects[3].id);
    EXPECT_EQ(math::Vector2ui(8, 2), rects[3].size);
    EXPECT_EQ(math::Point2ui(0, 10), rects[3].bottom_left);
  }

  // Test after packing.
  EXPECT_TRUE(bp.Pack(math::Vector2ui(20, 12)));
  {
    BinPacker bp_copy(bp);
    const std::vector<BinPacker::Rectangle>& rects = bp_copy.GetRectangles();
    ASSERT_EQ(4U, rects.size());
    EXPECT_EQ(0U, rects[0].id);
    EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
    EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
    EXPECT_EQ(1U, rects[1].id);
    EXPECT_EQ(math::Vector2ui(12, 4), rects[1].size);
    EXPECT_EQ(math::Point2ui(8, 0), rects[1].bottom_left);
    EXPECT_EQ(2U, rects[2].id);
    EXPECT_EQ(math::Vector2ui(12, 8), rects[2].size);
    EXPECT_EQ(math::Point2ui(8, 4), rects[2].bottom_left);
    EXPECT_EQ(3U, rects[3].id);
    EXPECT_EQ(math::Vector2ui(8, 2), rects[3].size);
    EXPECT_EQ(math::Point2ui(0, 10), rects[3].bottom_left);
  }
  {
    BinPacker bp_assign;
    bp_assign = bp;
    const std::vector<BinPacker::Rectangle>& rects = bp_assign.GetRectangles();
    ASSERT_EQ(4U, rects.size());
    EXPECT_EQ(0U, rects[0].id);
    EXPECT_EQ(math::Vector2ui(8, 10), rects[0].size);
    EXPECT_EQ(math::Point2ui(0, 0), rects[0].bottom_left);
    EXPECT_EQ(1U, rects[1].id);
    EXPECT_EQ(math::Vector2ui(12, 4), rects[1].size);
    EXPECT_EQ(math::Point2ui(8, 0), rects[1].bottom_left);
    EXPECT_EQ(2U, rects[2].id);
    EXPECT_EQ(math::Vector2ui(12, 8), rects[2].size);
    EXPECT_EQ(math::Point2ui(8, 4), rects[2].bottom_left);
    EXPECT_EQ(3U, rects[3].id);
    EXPECT_EQ(math::Vector2ui(8, 2), rects[3].size);
    EXPECT_EQ(math::Point2ui(0, 10), rects[3].bottom_left);
  }
}

}  // namespace text
}  // namespace ion
