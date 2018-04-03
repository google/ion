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

#include "ion/gfx/computeprogram.h"

#include <memory>

#include "ion/base/logchecker.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/tests/mockresource.h"
#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

typedef testing::MockResource<
  ComputeProgram::kNumChanges> MockShaderProgramResource;

class ComputeProgramTest : public ::testing::Test {
 protected:
  void SetUp() override {
    registry_.Reset(new ShaderInputRegistry());
    resource_ = absl::make_unique<MockShaderProgramResource>();
    compute_.Reset(new Shader());
    program_.Reset(new ComputeProgram(registry_));
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
    program_->SetResource(0U, 0, resource_.get());
    EXPECT_EQ(resource_.get(), program_->GetResource(0U, 0));
    EXPECT_TRUE(resource_->AnyModifiedBitsSet());
    resource_->ResetModifiedBits();
    EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  }

  // This is to ensure that the resource holder goes away before the resource.
  void TearDown() override { program_.Reset(nullptr); }

  ShaderInputRegistryPtr registry_;
  std::unique_ptr<MockShaderProgramResource> resource_;
  ShaderPtr compute_;
  ComputeProgramPtr program_;
};

TEST_F(ComputeProgramTest, SetRegistry) {
  EXPECT_EQ(registry_.Get(), program_->GetRegistry().Get());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(ComputeProgramTest, SetLabel) {
  // Check that the initial id is empty.
  EXPECT_TRUE(program_->GetLabel().empty());

  program_->SetLabel("myId");
  // Check that the id is set.
  EXPECT_EQ("myId", program_->GetLabel());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(ResourceHolder::kLabelChanged));
}

TEST_F(ComputeProgramTest, SetDocString) {
  // Check that the initial doc is empty.
  EXPECT_TRUE(program_->GetDocString().empty());

  program_->SetDocString("myDoc");
  // Check that the doc is set.
  EXPECT_EQ("myDoc", program_->GetDocString());
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
}

TEST_F(ComputeProgramTest, SetComputeShader) {
  // Check that the initial shader is NULL.
  EXPECT_FALSE(program_->GetComputeShader());

  EXPECT_FALSE(resource_->AnyModifiedBitsSet());
  program_->SetComputeShader(compute_);
  // Check that the proper bit is set.
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      ComputeProgram::kComputeShaderChanged));
  resource_->ResetModifiedBit(ComputeProgram::kComputeShaderChanged);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Modifying the shader should also trigger a Notifier change.
  compute_->SetSource("new source");
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      ComputeProgram::kComputeShaderChanged));
  resource_->ResetModifiedBit(ComputeProgram::kComputeShaderChanged);
  ShaderPtr new_shader(new Shader());
  EXPECT_EQ(1U, compute_->GetReceiverCount());
  program_->SetComputeShader(new_shader);
  EXPECT_EQ(0U, compute_->GetReceiverCount());
  EXPECT_TRUE(resource_->TestOnlyModifiedBit(
      ComputeProgram::kComputeShaderChanged));
  resource_->ResetModifiedBit(ComputeProgram::kComputeShaderChanged);
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // Check that the shader is set.
  EXPECT_EQ(new_shader.Get(), program_->GetComputeShader().Get());
  // Check that this did not change a bit.
  EXPECT_FALSE(resource_->AnyModifiedBitsSet());

  // The program should remove itself as a receiver when it goes away.
  EXPECT_EQ(1U, new_shader->GetReceiverCount());
  program_.Reset();
  EXPECT_EQ(0U, new_shader->GetReceiverCount());
}

TEST_F(ComputeProgramTest, SetConcurrent) {
  base::LogChecker log_checker;
  program_->SetConcurrent(true);
  EXPECT_TRUE(program_->IsConcurrent());
  EXPECT_FALSE(log_checker.HasAnyMessages());
  program_->SetConcurrent(false);
  EXPECT_TRUE(program_->IsConcurrent());
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "cannot change concurrency"));
}

TEST_F(ComputeProgramTest, SetInfoLog) {
  // Check that the initial log is empty.
  EXPECT_TRUE(program_->GetInfoLog().empty());

  program_->SetInfoLog("Link OK");
  EXPECT_EQ("Link OK", program_->GetInfoLog());
}

TEST_F(ComputeProgramTest, BuildFromStrings) {
  program_ = ComputeProgram::BuildFromStrings(
      "program", registry_, "dummy shader source",
      ion::base::AllocatorPtr());
  EXPECT_TRUE(program_->GetComputeShader());
  EXPECT_EQ(registry_, program_->GetRegistry());
  EXPECT_EQ("program", program_->GetLabel());
  EXPECT_EQ("program compute shader", program_->GetComputeShader()->GetLabel());
  EXPECT_EQ("dummy shader source", program_->GetComputeShader()->GetSource());
}

}  // namespace gfx
}  // namespace ion
