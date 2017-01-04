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

#include "ion/gfxutils/commandlistgenerator.h"

#include "ion/gfx/renderer.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfxutils/shadersourcecomposer.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfxutils {

namespace {


}  // anonymous namespace

class CommandListGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    clg_.reset(new CommandListGenerator);
  }

  void TearDown() override {
  }

  std::unique_ptr<CommandListGenerator> clg_;
};

TEST_F(CommandListGeneratorTest, Functions) {

}

}  // namespace gfxutils
}  // namespace ion
