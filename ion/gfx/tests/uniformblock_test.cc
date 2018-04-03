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

#include "ion/gfx/uniformblock.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

TEST(UniformBlockTest, SetLabel) {
  UniformBlockPtr block(new UniformBlock);

  // Check that the initial label is empty.
  EXPECT_TRUE(block->GetLabel().empty());

  block->SetLabel("myLabel");
  // Check that the label is set.
  EXPECT_EQ("myLabel", block->GetLabel());
}

}  // namespace gfx
}  // namespace ion
