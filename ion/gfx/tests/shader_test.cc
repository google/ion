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

#include "ion/gfx/shader.h"

#include <memory>

#include "ion/gfx/tests/mockresource.h"
#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

typedef testing::MockResource<Shader::kNumChanges> MockShaderResource;

class ShaderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    resource_ = absl::make_unique<MockShaderResource>();
    shader_.Reset(new Shader());
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    shader_->SetResource(0U, 0U, resource_.get());
    EXPECT_EQ(resource_.get(), shader_->GetResource(0U, 0U));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { shader_.Reset(nullptr); }

  std::unique_ptr<MockShaderResource> resource_;
  ShaderPtr shader_;
};

TEST_F(ShaderTest, SetLabel) {
  // Check that the initial id is empty.
  EXPECT_TRUE(shader_->GetLabel().empty());

  shader_->SetLabel("myId");
  // Check that the id is set.
  EXPECT_EQ("myId", shader_->GetLabel());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(ResourceHolder::kLabelChanged));
}

TEST_F(ShaderTest, SetDocString) {
  // Check that the initial doc is empty.
  EXPECT_TRUE(shader_->GetDocString().empty());

  shader_->SetDocString("myDoc");
  // Check that the doc is set.
  EXPECT_EQ("myDoc", shader_->GetDocString());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(ShaderTest, SetSource) {
  // Check that the initial source is empty.
  EXPECT_TRUE(shader_->GetSource().empty());

  shader_->SetSource("mySource");
  // Check that the proper bit is set.
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(Shader::kSourceChanged));
  // Check that this was the only bit set.
  resource_->ResetModifiedBit(Shader::kSourceChanged);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  // Check that the source is set.
  EXPECT_EQ("mySource", shader_->GetSource());
  // Check that this did not change a bit.
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(ShaderTest, SetInfoLog) {
  // Check that the initial log is empty.
  EXPECT_TRUE(shader_->GetInfoLog().empty());

  shader_->SetInfoLog("Compile OK");
  EXPECT_EQ("Compile OK", shader_->GetInfoLog());
}

}  // namespace gfx
}  // namespace ion
