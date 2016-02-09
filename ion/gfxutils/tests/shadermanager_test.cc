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

#include "ion/gfxutils/shadermanager.h"

#include "ion/gfx/renderer.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfxutils/shadersourcecomposer.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfxutils {

namespace {

using gfx::Renderer;
using gfx::RendererPtr;
using gfx::ShaderInputRegistry;
using gfx::ShaderInputRegistryPtr;
using gfx::ShaderProgram;
using gfx::ShaderProgramPtr;

// Simple composer that fakes a dependency.
class Composer : public ShaderSourceComposer {
 public:
  Composer(const std::string& source, const std::string& dependency)
      : source_(source),
        dependency_(dependency) {}
  ~Composer() override {}
  const std::string GetSource() override { return source_; }
  const std::string GetDependencySource(
      const std::string& dependency) const override {
    return std::string();
  }
  bool SetDependencySource(const std::string& dependency,
                           const std::string& source) override {
    return false;
  }
  void SetSource(const std::string& source) { source_ = source; }
  bool DependsOn(const std::string& resource) const override {
    return resource == dependency_;
  }
  const std::string GetDependencyName(unsigned int id) const override {
    if (id == 0)
      return dependency_;
    else
      return std::string();
  }
  const std::vector<std::string> GetDependencyNames() const override {
    return std::vector<std::string>(1, dependency_);
  }
  const std::vector<std::string> GetChangedDependencies() override {
    return std::vector<std::string>();
  }

 private:
  std::string source_;
  std::string dependency_;
};

typedef base::ReferentPtr<Composer>::Type ComposerPtr;

}  // anonymous namespace

class ShaderManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    manager_.Reset(new ShaderManager());
    registry_.Reset(new ShaderInputRegistry());
    vertex_composer_.Reset(new Composer("vertex", "vertex"));
    fragment_composer_.Reset(new Composer("fragment", "fragment"));
    program_ = manager_->CreateShaderProgram(
        "program", registry_, vertex_composer_, fragment_composer_);
  }

  void TearDown() override {
    program_.Reset(NULL);
    fragment_composer_.Reset(NULL);
    vertex_composer_.Reset(NULL);
    registry_.Reset(NULL);
    manager_.Reset(NULL);
  }

  ShaderProgramPtr program_;
  ShaderInputRegistryPtr registry_;
  ComposerPtr vertex_composer_;
  ComposerPtr fragment_composer_;
  ShaderManagerPtr manager_;
};

TEST_F(ShaderManagerTest, CreateAndGetShaderProgram) {
  EXPECT_EQ("vertex", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());
  std::vector<std::string> names = manager_->GetShaderProgramNames();
  EXPECT_EQ(1U, names.size());
  EXPECT_EQ("program", names[0]);

  EXPECT_EQ(NULL, manager_->GetShaderProgram("does not exist").Get());
  EXPECT_EQ(program_.Get(), manager_->GetShaderProgram("program").Get());

  // The program_ should not be gettable after the reference goes away.
  program_.Reset(NULL);
  EXPECT_EQ(NULL, manager_->GetShaderProgram("program").Get());
  names = manager_->GetShaderProgramNames();
  EXPECT_EQ(0U, names.size());
}

TEST_F(ShaderManagerTest, GetShaderProgramComposers) {
  ShaderSourceComposerPtr composer1, composer2;
  manager_->GetShaderProgramComposers("program", &composer1, &composer2);
  EXPECT_EQ(vertex_composer_.Get(), composer1.Get());
  EXPECT_EQ(fragment_composer_.Get(), composer2.Get());
  composer1.Reset(NULL);
  composer2.Reset(NULL);

  manager_->GetShaderProgramComposers("does not exist", &composer1, &composer2);
  EXPECT_EQ(NULL, composer1.Get());
  EXPECT_EQ(NULL, composer2.Get());

  // Check that it is ok for arguments to be NULL.
  manager_->GetShaderProgramComposers("program", &composer1, NULL);
  manager_->GetShaderProgramComposers("program", NULL, &composer2);
  EXPECT_EQ(vertex_composer_.Get(), composer1.Get());
  EXPECT_EQ(fragment_composer_.Get(), composer2.Get());

  program_.Reset(NULL);
  manager_->GetShaderProgramComposers("program", &composer1, &composer2);
  EXPECT_EQ(NULL, composer1.Get());
  EXPECT_EQ(NULL, composer2.Get());
}

TEST_F(ShaderManagerTest, RecreateAllShaderPrograms) {
  EXPECT_EQ("vertex", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());

  vertex_composer_->SetSource("vertex2");
  manager_->RecreateAllShaderPrograms();
  EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());

  fragment_composer_->SetSource("fragment2");
  manager_->RecreateAllShaderPrograms();
  EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment2", program_->GetFragmentShader()->GetSource());


  // Test that we can recreate all programs properly.
  ComposerPtr vertex_composer(new Composer("vertex3", "vertex3"));
  ComposerPtr fragment_composer(new Composer("fragment3", "fragment3"));
  ShaderProgramPtr program = manager_->CreateShaderProgram(
      "program3", registry_, vertex_composer, fragment_composer);

  manager_->RecreateAllShaderPrograms();
  EXPECT_EQ("vertex3", program->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment3", program->GetFragmentShader()->GetSource());

  // Check that we can still recreate programs after destroying one.
  program_.Reset(NULL);
  manager_->RecreateAllShaderPrograms();
  EXPECT_EQ("vertex3", program->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment3", program->GetFragmentShader()->GetSource());
}

TEST_F(ShaderManagerTest, RecreateShaderProgramThatDependOn) {
  EXPECT_EQ("vertex", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());

  manager_->RecreateShaderProgramsThatDependOn("no dependency");
  // Nothing should change.
  EXPECT_EQ("vertex", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());

  // Only the vertex shader source should change.
  vertex_composer_->SetSource("vertex2");
  manager_->RecreateShaderProgramsThatDependOn("vertex");
  EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());

  // Only the fragment shader source should change.
  fragment_composer_->SetSource("fragment2");
  manager_->RecreateShaderProgramsThatDependOn("fragment");
  EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment2", program_->GetFragmentShader()->GetSource());
}

}  // namespace gfxutils
}  // namespace ion
