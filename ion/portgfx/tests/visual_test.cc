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

#include "ion/portgfx/visual.h"

#include "ion/base/logging.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(Visual, Visual) {
  // Get the current GL context for coverage.
  ion::portgfx::Visual::GetCurrent();
  // Get an ID without a Visual for coverage.
  ion::portgfx::Visual::GetCurrentId();

  // Create an initial context.
  std::unique_ptr<ion::portgfx::Visual> visual(
      ion::portgfx::Visual::CreateVisual());
  ion::portgfx::Visual::MakeCurrent(visual.get());
  const size_t id = ion::portgfx::Visual::GetCurrentId();
  if (visual->IsValid())
    EXPECT_EQ(visual.get(), ion::portgfx::Visual::GetCurrent());
  EXPECT_EQ(id, visual->GetId());
  EXPECT_GE(id, 0U);

  // Share the context.
  std::unique_ptr<ion::portgfx::Visual> share_visual(
      ion::portgfx::Visual::CreateVisualInCurrentShareGroup());
  // Creating the visual doesn't make it current.
  if (share_visual.get()) {
    EXPECT_EQ(visual.get(), ion::portgfx::Visual::GetCurrent());
    EXPECT_GE(ion::portgfx::Visual::GetCurrentId(), 0U);

    ion::portgfx::Visual::MakeCurrent(share_visual.get());
    const size_t new_id = ion::portgfx::Visual::GetCurrentId();
    EXPECT_EQ(share_visual.get(), ion::portgfx::Visual::GetCurrent());
    EXPECT_EQ(new_id, share_visual->GetId());
    EXPECT_GE(new_id, 0U);
  }

  // Create another share context in the same group.
  std::unique_ptr<ion::portgfx::Visual> share_visual2(
      visual->CreateVisualInCurrentShareGroup());
  if (share_visual2.get()) {
    // Creating the visual doesn't make it current.
    EXPECT_EQ(share_visual.get(), ion::portgfx::Visual::GetCurrent());
    EXPECT_GE(ion::portgfx::Visual::GetCurrentId(), 0U);

    ion::portgfx::Visual::MakeCurrent(share_visual2.get());
    const size_t new_id2 = ion::portgfx::Visual::GetCurrentId();
    EXPECT_EQ(share_visual2.get(), ion::portgfx::Visual::GetCurrent());
    EXPECT_EQ(new_id2, share_visual2->GetId());
    EXPECT_TRUE(share_visual2->IsCurrent());

    // Clearing a non-kCurrent Visual should clear the OpenGL context.
    ion::portgfx::Visual::MakeCurrent(NULL);
    EXPECT_FALSE(share_visual2->IsCurrent());
    EXPECT_NE(new_id2, ion::portgfx::Visual::GetCurrentId());
    ion::portgfx::Visual::MakeCurrent(share_visual2.get());
    EXPECT_EQ(new_id2, ion::portgfx::Visual::GetCurrentId());
    ion::portgfx::Visual::MakeCurrent(NULL);
  }
}
