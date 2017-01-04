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

#include "ion/gfx/texturemanager.h"

#include <limits>
#include <memory>

#include "ion/base/logchecker.h"
#include "ion/math/range.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

using math::Range1i;

TEST(TextureManagerTest, ProperUnitAssignment) {
  TextureManager tm(4);

  // Check front and pack pointers are set correctly.
  EXPECT_EQ(0, tm.GetFrontIndex());
  EXPECT_EQ(3, tm.GetBackIndex());

  int ptr1 = 1;
  int ptr2 = 2;
  int ptr3 = 3;
  int ptr4 = 4;
  int ptr5 = 5;
  int ptr6 = 6;
  int ptr7 = 7;

  // Add a texture.
  EXPECT_EQ(0, tm.GetUnit(&ptr1, -1));
  EXPECT_EQ(1, tm.GetFrontIndex());
  EXPECT_EQ(&ptr1, tm.GetTexture(0));
  EXPECT_EQ(0, tm.GetBackIndex());

  // Add another texture.
  EXPECT_EQ(1, tm.GetUnit(&ptr2, -1));
  EXPECT_EQ(2, tm.GetFrontIndex());
  EXPECT_EQ(&ptr2, tm.GetTexture(1));
  EXPECT_EQ(1, tm.GetBackIndex());

  // Check that adding a texture with a wrong index is like using -1.
  EXPECT_EQ(2, tm.GetUnit(&ptr3, 0));
  EXPECT_EQ(3, tm.GetFrontIndex());
  EXPECT_EQ(&ptr3, tm.GetTexture(2));
  EXPECT_EQ(2, tm.GetBackIndex());

  // Check that touching a previously used unit reuses it and moves it to the
  // back.
  EXPECT_EQ(1, tm.GetUnit(&ptr2, 1));
  EXPECT_EQ(3, tm.GetFrontIndex());
  EXPECT_EQ(&ptr2, tm.GetTexture(1));
  EXPECT_EQ(1, tm.GetBackIndex());

  // Fill up the manager.
  EXPECT_EQ(3, tm.GetUnit(&ptr4, -1));
  EXPECT_EQ(0, tm.GetFrontIndex());
  EXPECT_EQ(&ptr4, tm.GetTexture(3));
  EXPECT_EQ(3, tm.GetBackIndex());
  EXPECT_EQ(0, tm.GetUnit(&ptr5, -1));
  EXPECT_EQ(2, tm.GetFrontIndex());
  EXPECT_EQ(&ptr5, tm.GetTexture(0));
  EXPECT_EQ(0, tm.GetBackIndex());

  // Check that index 1 is not returned (since it was touched recently), but is
  // now the front.
  EXPECT_EQ(2, tm.GetUnit(&ptr6, -1));
  EXPECT_EQ(1, tm.GetFrontIndex());
  EXPECT_EQ(&ptr6, tm.GetTexture(2));
  EXPECT_EQ(2, tm.GetBackIndex());

  // Make sure we get new unit from front.
  int front = tm.GetFrontIndex();
  EXPECT_EQ(front, tm.GetUnit(&ptr7, tm.GetBackIndex()));
  EXPECT_EQ(front, tm.GetBackIndex());  // Front is new back.
  EXPECT_EQ(&ptr7, tm.GetTexture(front));
  // Make sure getting the back changes nothing.
  int unit = tm.GetUnit(&ptr7, tm.GetBackIndex());
  EXPECT_EQ(tm.GetBackIndex(), unit);
  EXPECT_EQ(&ptr7, tm.GetTexture(unit));
  // Make sure getting the front moves front to back.
  front = tm.GetFrontIndex();
  unit = tm.GetUnit(tm.GetTexture(front), front);
  EXPECT_EQ(front, unit);
  EXPECT_EQ(tm.GetBackIndex(), unit);
}

TEST(TextureManagerTest, SetUnitRange) {
  base::LogChecker log_checker;

  TextureManager tm(4);
  EXPECT_EQ(0, tm.GetFrontIndex());
  EXPECT_EQ(3, tm.GetBackIndex());
  tm.SetUnitRange(Range1i(-1, 2));
  EXPECT_EQ(0, tm.GetFrontIndex());
  EXPECT_EQ(3, tm.GetBackIndex());
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "minimum unit for TextureManager to use must be >= 0"));

  tm.SetUnitRange(Range1i(1, 4));
  EXPECT_EQ(1, tm.GetFrontIndex());
  EXPECT_EQ(3, tm.GetBackIndex());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  tm.SetUnitRange(Range1i(1, 2));
  EXPECT_EQ(1, tm.GetFrontIndex());
  EXPECT_EQ(2, tm.GetBackIndex());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  tm.SetUnitRange(Range1i(0, 0));
  EXPECT_EQ(0, tm.GetFrontIndex());
  EXPECT_EQ(0, tm.GetBackIndex());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  tm.SetUnitRange(Range1i(0, std::numeric_limits<int>::max()));
  EXPECT_EQ(0, tm.GetFrontIndex());
  EXPECT_EQ(3, tm.GetBackIndex());
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(TextureManagerTest, ChangeUnitRange) {
  // Check that changing the unit range after units are assigned reassigns
  // everything.
  TextureManager tm(4);
  int ptr1 = 1;
  int ptr2 = 2;

  // Add a texture.
  EXPECT_EQ(0, tm.GetUnit(&ptr1, -1));
  EXPECT_EQ(1, tm.GetFrontIndex());
  EXPECT_EQ(&ptr1, tm.GetTexture(0));
  EXPECT_EQ(0, tm.GetBackIndex());

  // Add another texture.
  EXPECT_EQ(1, tm.GetUnit(&ptr2, -1));
  EXPECT_EQ(2, tm.GetFrontIndex());
  EXPECT_EQ(&ptr2, tm.GetTexture(1));
  EXPECT_EQ(1, tm.GetBackIndex());
  EXPECT_EQ(0, tm.GetUnit(&ptr1, 0));

  // Change the range to be the highest two units.
  tm.SetUnitRange(Range1i(2, 3));
  EXPECT_EQ(2, tm.GetUnit(&ptr1, 0));
  EXPECT_EQ(3, tm.GetFrontIndex());
  EXPECT_EQ(nullptr, tm.GetTexture(0));
  EXPECT_EQ(&ptr1, tm.GetTexture(2));
  EXPECT_EQ(2, tm.GetBackIndex());

  // Check ptr2.
  EXPECT_EQ(3, tm.GetUnit(&ptr2, 1));
  EXPECT_EQ(2, tm.GetFrontIndex());
  EXPECT_TRUE(tm.GetTexture(1) == nullptr);
  EXPECT_EQ(&ptr2, tm.GetTexture(3));
  EXPECT_EQ(3, tm.GetBackIndex());
  EXPECT_EQ(2, tm.GetUnit(&ptr1, 2));
  EXPECT_EQ(3, tm.GetUnit(&ptr2, 3));

  // Go down to a single unit.
  tm.SetUnitRange(Range1i(1, 1));
  EXPECT_EQ(nullptr, tm.GetTexture(0));
  EXPECT_EQ(nullptr, tm.GetTexture(1));
  EXPECT_EQ(nullptr, tm.GetTexture(2));
  EXPECT_EQ(nullptr, tm.GetTexture(3));

  EXPECT_EQ(1, tm.GetUnit(&ptr1, 2));
  EXPECT_EQ(1, tm.GetFrontIndex());
  EXPECT_EQ(&ptr1, tm.GetTexture(1));
  EXPECT_EQ(1, tm.GetBackIndex());

  EXPECT_EQ(1, tm.GetUnit(&ptr2, 3));
  EXPECT_EQ(1, tm.GetFrontIndex());
  EXPECT_EQ(&ptr2, tm.GetTexture(1));
  EXPECT_EQ(1, tm.GetBackIndex());

  // Go down to a single unit, but use the highest
  // unit available by selecting an out-of-range value.
  tm.SetUnitRange(Range1i(std::numeric_limits<int>::max(),
                          std::numeric_limits<int>::max()));
  EXPECT_EQ(nullptr, tm.GetTexture(0));
  EXPECT_EQ(nullptr, tm.GetTexture(1));
  EXPECT_EQ(nullptr, tm.GetTexture(2));
  EXPECT_EQ(nullptr, tm.GetTexture(3));

  EXPECT_EQ(3, tm.GetUnit(&ptr1, 2));
  EXPECT_EQ(3, tm.GetFrontIndex());
  EXPECT_EQ(&ptr1, tm.GetTexture(3));
  EXPECT_EQ(3, tm.GetBackIndex());

  EXPECT_EQ(3, tm.GetUnit(&ptr2, 3));
  EXPECT_EQ(3, tm.GetFrontIndex());
  EXPECT_EQ(&ptr2, tm.GetTexture(3));
  EXPECT_EQ(3, tm.GetBackIndex());
}

}  // namespace gfx
}  // namespace ion
