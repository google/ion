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

#include "ion/gfxutils/shadermanager.h"

#include "ion/base/logchecker.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfxutils/shadersourcecomposer.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfxutils {

namespace {

using gfx::RendererPtr;
using gfx::ShaderInputRegistry;
using gfx::ShaderInputRegistryPtr;
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

using ComposerPtr = base::SharedPtr<Composer>;

}  // anonymous namespace

class ShaderManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    manager_.Reset(new ShaderManager());
    registry_.Reset(new ShaderInputRegistry());
    vertex_composer_.Reset(new Composer("vertex", "vertex"));
    fragment_composer_.Reset(new Composer("fragment", "fragment"));
    geometry_composer_.Reset(new Composer("geometry", "geometry"));
    program_ = manager_->CreateShaderProgram(
        "program", registry_, vertex_composer_, fragment_composer_,
        geometry_composer_);
  }

  void TearDown() override {
    program_.Reset(nullptr);
    fragment_composer_.Reset(nullptr);
    vertex_composer_.Reset(nullptr);
    registry_.Reset(nullptr);
    manager_.Reset(nullptr);
  }

  ShaderProgramPtr program_;
  ShaderInputRegistryPtr registry_;
  ComposerPtr vertex_composer_;
  ComposerPtr fragment_composer_;
  ComposerPtr geometry_composer_;
  ShaderManagerPtr manager_;
};

TEST_F(ShaderManagerTest, CreateAndGetShaderProgram) {
  EXPECT_EQ("vertex", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());
  EXPECT_EQ("geometry", program_->GetGeometryShader()->GetSource());
  std::vector<std::string> names = manager_->GetShaderProgramNames();
  EXPECT_EQ(1U, names.size());
  EXPECT_EQ("program", names[0]);

  EXPECT_EQ(nullptr, manager_->GetShaderProgram("does not exist").Get());
  EXPECT_EQ(program_.Get(), manager_->GetShaderProgram("program").Get());

  // The program_ should not be gettable after the reference goes away.
  program_.Reset(nullptr);
  EXPECT_EQ(nullptr, manager_->GetShaderProgram("program").Get());
  names = manager_->GetShaderProgramNames();
  EXPECT_EQ(0U, names.size());
}

TEST_F(ShaderManagerTest, CreateShaderProgramWithExistingNameWarns) {
  base::LogChecker log_checker;
  manager_->CreateShaderProgram(
      "new_program", registry_, vertex_composer_, fragment_composer_,
      geometry_composer_);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  manager_->CreateShaderProgram(
      "program", registry_, vertex_composer_, fragment_composer_,
      geometry_composer_);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "Overriding existing ShaderProgram"));
}

TEST_F(ShaderManagerTest, GetShaderProgramComposers) {
  ShaderSourceComposerPtr composer1, composer2, composer3;
  manager_->GetShaderProgramComposers("program", &composer1, &composer2,
                                      &composer3);
  EXPECT_EQ(vertex_composer_.Get(), composer1.Get());
  EXPECT_EQ(fragment_composer_.Get(), composer2.Get());
  EXPECT_EQ(geometry_composer_.Get(), composer3.Get());
  composer1.Reset(nullptr);
  composer2.Reset(nullptr);

  manager_->GetShaderProgramComposers("does not exist", &composer1, &composer2,
                                      &composer3);
  EXPECT_EQ(nullptr, composer1.Get());
  EXPECT_EQ(nullptr, composer2.Get());
  EXPECT_EQ(nullptr, composer3.Get());

  // Check that it is ok for arguments to be nullptr.
  manager_->GetShaderProgramComposers("program", &composer1, nullptr);
  manager_->GetShaderProgramComposers("program", nullptr, &composer2);
  EXPECT_EQ(vertex_composer_.Get(), composer1.Get());
  EXPECT_EQ(fragment_composer_.Get(), composer2.Get());

  program_.Reset(nullptr);
  manager_->GetShaderProgramComposers("program", &composer1, &composer2);
  EXPECT_EQ(nullptr, composer1.Get());
  EXPECT_EQ(nullptr, composer2.Get());
}

TEST_F(ShaderManagerTest, RecreateAllShaderPrograms) {
  EXPECT_EQ("vertex", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());
  EXPECT_EQ("geometry", program_->GetGeometryShader()->GetSource());

  {
    vertex_composer_->SetSource("vertex2");
    manager_->RecreateAllShaderPrograms();
    EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
    EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());
    EXPECT_EQ("geometry", program_->GetGeometryShader()->GetSource());
  }

  {
    fragment_composer_->SetSource("fragment2");
    manager_->RecreateAllShaderPrograms();
    EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
    EXPECT_EQ("fragment2", program_->GetFragmentShader()->GetSource());
    EXPECT_EQ("geometry", program_->GetGeometryShader()->GetSource());
  }

  {
    geometry_composer_->SetSource("geometry2");
    manager_->RecreateAllShaderPrograms();
    EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
    EXPECT_EQ("fragment2", program_->GetFragmentShader()->GetSource());
    EXPECT_EQ("geometry2", program_->GetGeometryShader()->GetSource());
  }

  // Test that we can recreate all programs properly.
  ComposerPtr vertex_composer(new Composer("vertex3", "vertex3"));
  ComposerPtr fragment_composer(new Composer("fragment3", "fragment3"));
  ComposerPtr geometry_composer(new Composer("geometry3", "geometry3"));
  ShaderProgramPtr program = manager_->CreateShaderProgram(
      "program3", registry_, vertex_composer, fragment_composer,
      geometry_composer);

  manager_->RecreateAllShaderPrograms();
  EXPECT_EQ("vertex3", program->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment3", program->GetFragmentShader()->GetSource());
  EXPECT_EQ("geometry3", program->GetGeometryShader()->GetSource());

  // Check that we can still recreate programs after destroying one.
  program_.Reset(nullptr);
  manager_->RecreateAllShaderPrograms();
  EXPECT_EQ("vertex3", program->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment3", program->GetFragmentShader()->GetSource());
  EXPECT_EQ("geometry3", program->GetGeometryShader()->GetSource());
}

TEST_F(ShaderManagerTest, RecreateShaderProgramThatDependOn) {
  EXPECT_EQ("vertex", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());
  EXPECT_EQ("geometry", program_->GetGeometryShader()->GetSource());

  manager_->RecreateShaderProgramsThatDependOn("no dependency");
  // Nothing should change.
  EXPECT_EQ("vertex", program_->GetVertexShader()->GetSource());
  EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());
  EXPECT_EQ("geometry", program_->GetGeometryShader()->GetSource());

  // Only the vertex shader source should change.
  {
    vertex_composer_->SetSource("vertex2");
    manager_->RecreateShaderProgramsThatDependOn("vertex");
    EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
    EXPECT_EQ("fragment", program_->GetFragmentShader()->GetSource());
    EXPECT_EQ("geometry", program_->GetGeometryShader()->GetSource());
  }

  // Only the fragment shader source should change.
  {
    fragment_composer_->SetSource("fragment2");
    manager_->RecreateShaderProgramsThatDependOn("fragment");
    EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
    EXPECT_EQ("fragment2", program_->GetFragmentShader()->GetSource());
    EXPECT_EQ("geometry", program_->GetGeometryShader()->GetSource());
  }

  // Finally, only the geometry shader source should change.
  {
    geometry_composer_->SetSource("geometry2");
    manager_->RecreateShaderProgramsThatDependOn("geometry");
    EXPECT_EQ("vertex2", program_->GetVertexShader()->GetSource());
    EXPECT_EQ("fragment2", program_->GetFragmentShader()->GetSource());
    EXPECT_EQ("geometry2", program_->GetGeometryShader()->GetSource());
  }
}

}  // namespace gfxutils
}  // namespace ion
