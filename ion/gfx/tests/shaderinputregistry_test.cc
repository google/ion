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

#include "ion/gfx/shaderinputregistry.h"

#include <cmath>
#include <vector>

#include "ion/base/logchecker.h"
#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/rotation.h"
#include "ion/math/transformutils.h"
#include "ion/math/vector.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

// For ShaderInputRegistry testing.
static const Uniform DummyCombineFunction(const Uniform& u1, const Uniform&u2) {
  return Uniform();  // Doesn't matter - it's never used.
}

// For ShaderInputRegistry testing.
static std::vector<Uniform> DummyGenerateFunction(const Uniform& u) {
  return std::vector<Uniform>();  // Doesn't matter - it's never used.
}

// Extracts a Vector3f of Euler angles from a 3x3 rotation matrix. Note that
// this is just an illustrative example of a GenerateFunction.
static std::vector<Uniform> ExtractEulerAngles(const Uniform& current) {
  DCHECK_EQ(kMatrix3x3Uniform, current.GetType());

  const math::Matrix3f mat = current.GetValue<math::Matrix3f>();
  math::Vector4f quat;
  float trace = mat[0][0] + mat[1][1] + mat[2][2];
  if (trace > 0) {
    const float scale = 0.5f / sqrtf(trace + 1.0f);
    quat[0] = (mat[2][1] - mat[1][2]) * scale;
    quat[1] = (mat[0][2] - mat[2][0]) * scale;
    quat[2] = (mat[1][0] - mat[0][1]) * scale;
    quat[3] = 0.25f / scale;
  } else {
    if (mat[0][0] > mat[1][1] && mat[0][0] > mat[2][2]) {
      const float scale =
          2.0f * sqrtf(1.0f + mat[0][0] - mat[1][1] - mat[2][2]);
      quat[0] = 0.25f * scale;
      quat[1] = (mat[0][1] + mat[1][0]) / scale;
      quat[2] = (mat[0][2] + mat[2][0]) / scale;
      quat[3] = (mat[2][1] - mat[1][2]) / scale;
    } else if (mat[1][1] > mat[2][2]) {
      const float scale =
          2.0f * sqrtf(1.0f + mat[1][1] - mat[0][0] - mat[2][2]);
      quat[0] = (mat[0][1] + mat[1][0]) / scale;
      quat[1] = 0.25f * scale;
      quat[2] = (mat[1][2] + mat[2][1]) / scale;
      quat[3] = (mat[0][2] - mat[2][0]) / scale;
    } else {
      const float scale =
          2.0f * sqrtf(1.0f + mat[2][2] - mat[0][0] - mat[1][1]);
      quat[0] = (mat[0][2] + mat[2][0]) / scale;
      quat[1] = (mat[1][2] + mat[2][1]) / scale;
      quat[2] = 0.25f * scale;
      quat[3] = (mat[1][0] - mat[0][1]) / scale;
    }
  }

  math::Rotationf rot;
  rot.SetQuaternion(quat);

  math::Anglef yaw;
  math::Anglef pitch;
  math::Anglef roll;
  rot.GetEulerAngles(&yaw, &pitch, &roll);
  const ShaderInputRegistry& reg = current.GetRegistry();
  std::vector<Uniform> uniforms;
  uniforms.push_back(reg.Create<Uniform>(
      "uAngles",
      math::Vector3f(roll.Radians(), pitch.Radians(), yaw.Radians())));
  return uniforms;
}

TEST(ShaderInputRegistryTest, UniqueRegistryId) {
  ShaderInputRegistryPtr registry1(new ShaderInputRegistry);
  ShaderInputRegistryPtr registry2(new ShaderInputRegistry);

  // Check each registry has a unique id.
  EXPECT_NE(registry1->GetId(), registry2->GetId());
}

TEST(ShaderInputRegistryTest, AddToRegistry) {
  base::LogChecker log_checker;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(reg->GetSpecs<Attribute>().empty());
  EXPECT_TRUE(reg->GetSpecs<Uniform>().empty());

  BufferObjectElement element;

  // Check that adding items to registry returns without error.
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::UniformSpec(
      "myInt", kIntUniform, "doc0")));
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::UniformSpec(
      "myFloat", kFloatUniform, "doc1")));
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::UniformSpec(
      "myVec2f", kFloatVector2Uniform, "doc2", DummyCombineFunction,
      DummyGenerateFunction)));

  EXPECT_TRUE(reg->Add(ShaderInputRegistry::AttributeSpec(
      "myVec4f", kFloatVector4Attribute, "doc3")));
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::AttributeSpec(
      "myBufferElement", kBufferObjectElementAttribute, "doc4")));
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::AttributeSpec(
      "myFloatAttrib", kFloatAttribute, "doc5")));
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::AttributeSpec(
      "myVec3f", kFloatVector3Attribute, "doc6")));

  // Try array permutations.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_FALSE(reg->Add(
      ShaderInputRegistry::UniformSpec("myIntArray]2[", kIntUniform, "doc10")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "invalid input name"));
  EXPECT_FALSE(reg->Add(
      ShaderInputRegistry::UniformSpec("myIntArray][", kIntUniform, "doc10")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "invalid input name"));
  EXPECT_FALSE(reg->Add(
      ShaderInputRegistry::UniformSpec("myIntArray2[", kIntUniform, "doc10")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "invalid input name"));
  EXPECT_FALSE(reg->Add(
      ShaderInputRegistry::UniformSpec("myIntArray[2", kIntUniform, "doc10")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "invalid input name"));
  EXPECT_FALSE(reg->Add(
      ShaderInputRegistry::UniformSpec("myIntArray[2", kIntUniform, "doc10")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "invalid input name"));
  EXPECT_FALSE(reg->Add(
      ShaderInputRegistry::UniformSpec("myIntArray[2", kIntUniform, "doc10")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "invalid input name"));
  // None of the above should have successfully parsed the name.
  EXPECT_TRUE(reg->Find<Uniform>("myIntArray") == nullptr);
  EXPECT_TRUE(reg->Add(
      ShaderInputRegistry::UniformSpec("myIntArray[2]", kIntUniform, "doc10")));

  // Check that adding an entry with an existing name returns false and prints
  // an error message.
  EXPECT_FALSE(reg->Add(ShaderInputRegistry::AttributeSpec(
      "myVec2f", kFloatVector2Attribute, "doc7")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "already present in registry"));

  // Check uniform specs.
  const base::AllocDeque<ShaderInputRegistry::UniformSpec>& uniform_specs =
      reg->GetSpecs<Uniform>();
  EXPECT_FALSE(uniform_specs.empty());
  EXPECT_EQ(4U, uniform_specs.size());
  EXPECT_EQ("myInt", uniform_specs[0].name);
  EXPECT_EQ(kIntUniform, uniform_specs[0].value_type);
  EXPECT_EQ("doc0", uniform_specs[0].doc_string);
  EXPECT_TRUE(!uniform_specs[0].combine_function);
  EXPECT_EQ(0U, uniform_specs[0].index);
  EXPECT_EQ("myFloat", uniform_specs[1].name);
  EXPECT_EQ(kFloatUniform, uniform_specs[1].value_type);
  EXPECT_EQ("doc1", uniform_specs[1].doc_string);
  EXPECT_TRUE(!uniform_specs[1].combine_function);
  EXPECT_EQ(1U, uniform_specs[1].index);
  EXPECT_EQ("myVec2f", uniform_specs[2].name);
  EXPECT_EQ(kFloatVector2Uniform, uniform_specs[2].value_type);
  EXPECT_EQ("doc2", uniform_specs[2].doc_string);
  EXPECT_FALSE(!uniform_specs[2].combine_function);
  EXPECT_EQ(2U, uniform_specs[2].index);
  EXPECT_EQ(kIntUniform, uniform_specs[3].value_type);
  EXPECT_EQ("doc10", uniform_specs[3].doc_string);
  EXPECT_TRUE(!uniform_specs[3].combine_function);
  EXPECT_EQ(3U, uniform_specs[3].index);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check attribute specs.
  const base::AllocDeque<ShaderInputRegistry::AttributeSpec>& attribute_specs =
      reg->GetSpecs<Attribute>();
  EXPECT_FALSE(attribute_specs.empty());
  EXPECT_EQ(4U, attribute_specs.size());
  EXPECT_EQ("myVec4f", attribute_specs[0].name);
  EXPECT_EQ(kFloatVector4Attribute, attribute_specs[0].value_type);
  EXPECT_EQ("doc3", attribute_specs[0].doc_string);
  EXPECT_TRUE(!attribute_specs[0].combine_function);
  EXPECT_EQ(0U, attribute_specs[0].index);
  EXPECT_EQ("myBufferElement", attribute_specs[1].name);
  EXPECT_EQ(kBufferObjectElementAttribute, attribute_specs[1].value_type);
  EXPECT_EQ("doc4", attribute_specs[1].doc_string);
  EXPECT_TRUE(!attribute_specs[1].combine_function);
  EXPECT_EQ(1U, attribute_specs[1].index);
  EXPECT_EQ("myFloatAttrib", attribute_specs[2].name);
  EXPECT_EQ(kFloatAttribute, attribute_specs[2].value_type);
  EXPECT_EQ("doc5", attribute_specs[2].doc_string);
  EXPECT_TRUE(!attribute_specs[2].combine_function);
  EXPECT_EQ(2U, attribute_specs[2].index);
  EXPECT_EQ("myVec3f", attribute_specs[3].name);
  EXPECT_EQ(kFloatVector3Attribute, attribute_specs[3].value_type);
  EXPECT_EQ("doc6", attribute_specs[3].doc_string);
  EXPECT_TRUE(!attribute_specs[3].combine_function);
  EXPECT_EQ(3U, attribute_specs[3].index);

  EXPECT_TRUE(reg->Find<Uniform>("myInt") != nullptr);
  EXPECT_TRUE(reg->Find<Uniform>("myFloat") != nullptr);
  EXPECT_TRUE(reg->Find<Uniform>("myVec2f") != nullptr);
  EXPECT_TRUE(reg->Find<Attribute>("myVec4f") != nullptr);
  EXPECT_TRUE(reg->Find<Attribute>("myBufferElement") != nullptr);
  EXPECT_TRUE(reg->Find<Attribute>("myFloatAttrib") != nullptr);
  EXPECT_TRUE(reg->Find<Attribute>("myVec3f") != nullptr);

  // Check that the namespace isn't polluted across types.
  EXPECT_TRUE(reg->Find<Attribute>("myInt") == nullptr);
  EXPECT_TRUE(reg->Find<Uniform>("myVec3f") == nullptr);
  EXPECT_TRUE(reg->Find<Uniform>("noSuchUniform") == nullptr);
  EXPECT_TRUE(reg->Find<Attribute>("noSuchAttribute") == nullptr);
}

TEST(ShaderInputRegistryTest, ConstCreateFailsWhenSpecNotAdded) {
  base::LogChecker log_checker;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(reg->GetSpecs<Attribute>().empty());
  EXPECT_TRUE(reg->GetSpecs<Uniform>().empty());

  // Create on a non-const registry works.
  Uniform u = reg->Create<Uniform>("myInt", 21);
  EXPECT_TRUE(u.IsValid());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Create on a const registry fails if the spec does not exist.
  const ShaderInputRegistry& const_reg = *reg;
  u = const_reg.Create<Uniform>("myIntConst", 42);
  EXPECT_FALSE(u.IsValid());
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "no Spec exists for this name"));
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::UniformSpec(
      "myIntConst", kIntUniform, "doc1")));
  u = const_reg.Create<Uniform>("myIntConst", 42);
  EXPECT_TRUE(u.IsValid());
  EXPECT_EQ(42, u.GetValue<int>());

  // Try the same with an array Uniform.
  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  u = reg->CreateArrayUniform("myIntArray", &ints[0], ints.size(),
                              base::AllocatorPtr());
  EXPECT_TRUE(u.IsValid());
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Create on a const registry fails if the spec does not exist.
  u = const_reg.CreateArrayUniform("myIntArrayConst", &ints[0], ints.size(),
                                   base::AllocatorPtr());
  EXPECT_FALSE(u.IsValid());
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "no Spec exists for this name"));
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::UniformSpec("myIntArrayConst",
                                                        kIntUniform, "doc1")));
  u = const_reg.CreateArrayUniform("myIntArrayConst", &ints[0], ints.size(),
                                   base::AllocatorPtr());
  EXPECT_TRUE(u.IsValid());
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(2U, u.GetCount());
  EXPECT_EQ(1, u.GetValueAt<int>(0));
  EXPECT_EQ(2, u.GetValueAt<int>(1));
}

TEST(ShaderInputRegistryTest, IncludeGlobalRegistry) {
  base::LogChecker log_checker;

  // Ensure the global registry exists.
  const ShaderInputRegistryPtr& global_reg =
      ShaderInputRegistry::GetGlobalRegistry();
  // This test ensures that the above line won't be optimized out.
  EXPECT_GE(global_reg->GetId(), 0U);

  ShaderInputRegistryPtr reg1(new ShaderInputRegistry);
  ShaderInputRegistryPtr reg2(new ShaderInputRegistry);
  EXPECT_TRUE(reg1->GetSpecs<Attribute>().empty());
  EXPECT_TRUE(reg1->GetSpecs<Uniform>().empty());
  EXPECT_TRUE(reg2->GetSpecs<Attribute>().empty());
  EXPECT_TRUE(reg2->GetSpecs<Uniform>().empty());

  // Check that the global registry can be included.
  EXPECT_TRUE(reg1->IncludeGlobalRegistry());
  EXPECT_TRUE(reg1->Find<Uniform>("uViewportSize") != nullptr);
  EXPECT_TRUE(reg1->Find<Uniform>("uProjectionMatrix") != nullptr);
  EXPECT_TRUE(reg1->Find<Uniform>("uModelviewMatrix") != nullptr);
  EXPECT_TRUE(reg1->Find<Uniform>("uBaseColor") != nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("aVertex") != nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("aColor") != nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("aTexCoords") != nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("aNormal") != nullptr);
  // These should not exist.
  EXPECT_TRUE(reg1->Find<Uniform>("myVec3f") == nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("myFloat") == nullptr);

  EXPECT_TRUE(reg2->IncludeGlobalRegistry());
  EXPECT_TRUE(reg2->Find<Uniform>("uViewportSize") != nullptr);
  EXPECT_TRUE(reg2->Find<Uniform>("uProjectionMatrix") != nullptr);
  EXPECT_TRUE(reg2->Find<Uniform>("uModelviewMatrix") != nullptr);
  EXPECT_TRUE(reg2->Find<Uniform>("uBaseColor") != nullptr);
  EXPECT_TRUE(reg2->Find<Attribute>("aVertex") != nullptr);
  EXPECT_TRUE(reg2->Find<Attribute>("aColor") != nullptr);
  EXPECT_TRUE(reg2->Find<Attribute>("aTexCoords") != nullptr);
  EXPECT_TRUE(reg2->Find<Attribute>("aNormal") != nullptr);
  // These should not exist.
  EXPECT_TRUE(reg2->Find<Uniform>("myVec3f") == nullptr);
  EXPECT_TRUE(reg2->Find<Attribute>("myFloat") == nullptr);

  // Now reg1 cannot include reg2, or vice versa.
  EXPECT_FALSE(reg1->Include(reg2));
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     "both define the shader input"));
  EXPECT_FALSE(reg2->Include(reg1));
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     "both define the shader input"));
}

TEST(ShaderInputRegistryTest, CombineFunction) {
  base::LogChecker log_checker;

  // Ensure the global registry exists.
  const ShaderInputRegistryPtr& global_reg =
      ShaderInputRegistry::GetGlobalRegistry();

  EXPECT_TRUE(global_reg->Find<Uniform>("uViewportSize") != nullptr);
  EXPECT_TRUE(global_reg->Find<Uniform>("uModelviewMatrix") != nullptr);
  EXPECT_TRUE(global_reg->Find<Uniform>("uProjectionMatrix") != nullptr);
  EXPECT_TRUE(global_reg->Find<Uniform>("uBaseColor") != nullptr);

  // Check that uModelviewMatrix has a combine function.
  ShaderInputRegistry::CombineFunction<Uniform>::Type combiner =
      global_reg->Find<Uniform>("uModelviewMatrix")->combine_function;
  EXPECT_TRUE(combiner);
  // Check that other uniforms do not.
  EXPECT_FALSE(
      global_reg->Find<Uniform>("uViewportSize")->combine_function);
  EXPECT_FALSE(
      global_reg->Find<Uniform>("uProjectionMatrix")->combine_function);
  EXPECT_FALSE(
      global_reg->Find<Uniform>("uBaseColor")->combine_function);

  // Call the combiner.
  math::Matrix4f m1 = math::TranslationMatrix(math::Vector3f(1.f, 2.3f, -3.f));
  math::Matrix4f m2 = math::TranslationMatrix(math::Vector3f(-3.1f, 0.11f,
                                                             2.f));
  Uniform u1 = global_reg->Create<Uniform>("uModelviewMatrix", m1);
  Uniform u2 = global_reg->Create<Uniform>("uModelviewMatrix", m2);

  math::Matrix4f product = m1 * m2;
  Uniform result = combiner(u1, u2);
  EXPECT_EQ(product, result.GetValue<math::Matrix4f>());
}

TEST(ShaderInputRegistryTest, GenerateFunction) {
  // Ensure the global registry exists.
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::UniformSpec(
      "uRotation", kMatrix3x3Uniform, "doc", nullptr, ExtractEulerAngles)));
  EXPECT_TRUE(reg->Add(ShaderInputRegistry::UniformSpec(
      "uAngles", kFloatVector3Uniform, "doc")));

  // Check that uRotation has a generate function.
  ShaderInputRegistry::GenerateFunction<Uniform>::Type generator =
      reg->Find<Uniform>("uRotation")->generate_function;
  EXPECT_TRUE(generator);

  // Call the generator.
  const math::Angled angle1 = math::Angled::FromDegrees(30.);
  const math::Angled angle2 = math::Angled::FromDegrees(20.);
  const math::Angled angle3 = math::Angled::FromDegrees(10.);
  const math::Matrix3f m(math::RotationMatrixNH(
      math::Rotationd::FromAxisAndAngle(math::Vector3d::AxisZ(), angle1) *
      math::Rotationd::FromAxisAndAngle(math::Vector3d::AxisX(), angle2) *
      math::Rotationd::FromAxisAndAngle(math::Vector3d::AxisY(), angle3)));

  // Read the angles out of the uniform.
  Uniform u = reg->Create<Uniform>("uRotation", m);
  std::vector<Uniform> result = generator(u);
  ASSERT_EQ(1U, result.size());
  EXPECT_TRUE(result[0].IsValid());
  const math::VectorBase3f angles = result[0].GetValue<math::VectorBase3f>();
  const math::Angled uniform_angle1(math::Anglef::FromRadians(angles[0]));
  const math::Angled uniform_angle2(math::Anglef::FromRadians(angles[1]));
  const math::Angled uniform_angle3(math::Anglef::FromRadians(angles[2]));
  static const double kTolerance = 1e-5;
  EXPECT_NEAR(angle1.Radians(), uniform_angle1.Radians(), kTolerance);
  EXPECT_NEAR(angle2.Radians(), uniform_angle2.Radians(), kTolerance);
  EXPECT_NEAR(angle3.Radians(), uniform_angle3.Radians(), kTolerance);
}

TEST(ShaderInputRegistryTest, Include) {
  base::LogChecker log_checker;
  ShaderInputRegistryPtr reg1(new ShaderInputRegistry);
  ShaderInputRegistryPtr reg2(new ShaderInputRegistry);
  ShaderInputRegistryPtr reg3(new ShaderInputRegistry);
  ShaderInputRegistryPtr reg4(new ShaderInputRegistry);

  BufferObjectElement element;

  // Check that adding items to registry returns without error.
  EXPECT_TRUE(reg1->Add(ShaderInputRegistry::UniformSpec(
      "myInt", kIntUniform, "doc0")));
  EXPECT_TRUE(reg2->Add(ShaderInputRegistry::UniformSpec(
      "myFloat", kFloatUniform, "doc1")));
  EXPECT_TRUE(reg3->Add(ShaderInputRegistry::UniformSpec(
      "myVec2f", kFloatVector2Uniform, "doc2")));

  EXPECT_TRUE(reg1->Add(ShaderInputRegistry::AttributeSpec(
      "myVec4f", kFloatVector4Attribute, "doc3")));
  EXPECT_TRUE(reg2->Add(ShaderInputRegistry::AttributeSpec(
      "myBufferElement", kBufferObjectElementAttribute, "doc4")));
  EXPECT_TRUE(reg3->Add(ShaderInputRegistry::AttributeSpec(
      "myFloatAttrib", kFloatAttribute, "doc5")));

  // Conflicts with reg2 (even though the types are different).
  EXPECT_TRUE(reg4->Add(ShaderInputRegistry::AttributeSpec(
      "myFloat", kFloatAttribute, "doc1")));
  EXPECT_TRUE(reg4->Add(ShaderInputRegistry::AttributeSpec(
      "myVec3f", kFloatVector3Attribute, "doc6")));
  // Conflicts with reg3.
  EXPECT_TRUE(reg4->Add(ShaderInputRegistry::UniformSpec(
      "myVec2f", kFloatVector2Uniform, "doc7")));

  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that trying to add a pre-existing entry fails.
  EXPECT_FALSE(reg2->Add(ShaderInputRegistry::UniformSpec(
      "myFloat", kFloatUniform, "doc1")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "already present in registry or its"));

  // Check that registries cannot include themselves.
  EXPECT_FALSE(reg1->Include(reg1));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "cannot include itself"));
  EXPECT_FALSE(reg2->Include(reg2));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "cannot include itself"));
  EXPECT_FALSE(reg3->Include(reg3));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "cannot include itself"));
  EXPECT_FALSE(reg4->Include(reg4));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "cannot include itself"));

  // reg2 cannot include reg4, or vice versa, since they both define myFloat.
  EXPECT_FALSE(reg2->Include(reg4));
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     "both define the shader input 'myFloat'"));
  EXPECT_FALSE(reg4->Include(reg2));
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     "both define the shader input 'myFloat'"));

  // reg3 cannot include reg4, or vice versa, since they both define myVec2f.
  EXPECT_FALSE(reg3->Include(reg4));
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     "both define the shader input 'myVec2f'"));
  EXPECT_FALSE(reg4->Include(reg3));
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     "both define the shader input 'myVec2f'"));
  // Have reg2 include reg3 (recall reg1 includes reg2).
  EXPECT_TRUE(reg2->Include(reg3));

  // Have reg1 include reg2.
  EXPECT_TRUE(reg1->Include(reg2));
  EXPECT_FALSE(reg1->Add(ShaderInputRegistry::AttributeSpec(
      "myVec2f", kFloatVector2Attribute, "doc1")));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "already present in registry or its"));
  EXPECT_FALSE(reg1->Include(reg4));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "both define the shader input"));
  EXPECT_FALSE(reg1->Include(reg3));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "both define the shader input"));
  // reg1 already includes reg2.
  EXPECT_FALSE(reg1->Include(reg2));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "both define the shader input"));

  // Test inclusions.
  EXPECT_EQ(1U, reg1->GetIncludes().size());
  EXPECT_EQ(reg2, reg1->GetIncludes()[0]);
  EXPECT_EQ(1U, reg2->GetIncludes().size());
  EXPECT_EQ(reg3, reg2->GetIncludes()[0]);

  // Check that registries can find specs from includes.
  EXPECT_TRUE(reg1->Find<Uniform>("myInt") != nullptr);
  EXPECT_TRUE(reg1->Find<Uniform>("myFloat") != nullptr);
  EXPECT_TRUE(reg1->Find<Uniform>("myVec2f") != nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("myVec4f") != nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("myBufferElement") != nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("myFloatAttrib") != nullptr);
  // These should not exist.
  EXPECT_TRUE(reg1->Find<Uniform>("myVec3f") == nullptr);
  EXPECT_TRUE(reg1->Find<Attribute>("myFloat") == nullptr);

  // Check that specs from included registries have the right registry and
  // registry id.
  EXPECT_EQ(reg1.Get(), reg1->Find<Uniform>("myInt")->registry);
  EXPECT_EQ(reg2.Get(), reg1->Find<Uniform>("myFloat")->registry);
  EXPECT_EQ(reg3.Get(), reg1->Find<Uniform>("myVec2f")->registry);
  EXPECT_EQ(reg1.Get(), reg1->Find<Attribute>("myVec4f")->registry);
  EXPECT_EQ(reg2.Get(),
            reg1->Find<Attribute>("myBufferElement")->registry);
  EXPECT_EQ(reg3.Get(),
            reg1->Find<Attribute>("myFloatAttrib")->registry);
  EXPECT_EQ(reg1->GetId(), reg1->Find<Uniform>("myInt")->registry_id);
  EXPECT_EQ(reg2->GetId(), reg1->Find<Uniform>("myFloat")->registry_id);
  EXPECT_EQ(reg3->GetId(), reg1->Find<Uniform>("myVec2f")->registry_id);
  EXPECT_EQ(reg1->GetId(), reg1->Find<Attribute>("myVec4f")->registry_id);
  EXPECT_EQ(reg2->GetId(),
            reg1->Find<Attribute>("myBufferElement")->registry_id);
  EXPECT_EQ(reg3->GetId(),
            reg1->Find<Attribute>("myFloatAttrib")->registry_id);

  // Check that a null registry cannot be included.
  ShaderInputRegistryPtr reg5;
  EXPECT_FALSE(reg1->Include(reg5));
  EXPECT_FALSE(reg4->Include(reg5));

  // Check for uniqueness.
  EXPECT_TRUE(reg1->CheckInputsAreUnique());
  EXPECT_TRUE(reg2->CheckInputsAreUnique());
  EXPECT_TRUE(reg3->CheckInputsAreUnique());
  EXPECT_TRUE(reg4->CheckInputsAreUnique());

  // Create an artificial duplicate.
  EXPECT_TRUE(reg3->Add(ShaderInputRegistry::UniformSpec(
      "myInt", kIntUniform, "doc0")));
  EXPECT_FALSE(reg1->CheckInputsAreUnique());
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "duplicate input"));
  EXPECT_TRUE(reg2->CheckInputsAreUnique());
  EXPECT_TRUE(reg3->CheckInputsAreUnique());
  EXPECT_TRUE(reg4->CheckInputsAreUnique());
}

}  // namespace gfx
}  // namespace ion
