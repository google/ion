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

#include "ion/text/outlinebuilder.h"

#include <string>

#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/node.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/math/range.h"
#include "ion/math/rangeutils.h"
#include "ion/math/vector.h"
#include "ion/text/font.h"
#include "ion/text/fontmanager.h"
#include "ion/text/layout.h"
#include "ion/text/tests/buildertestbase.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {

static const float kEpsilon = 1e-5f;

//-----------------------------------------------------------------------------
//
// Test harness that adds some convenience functions.
//
//-----------------------------------------------------------------------------

class OutlineBuilderTest : public testing::BuilderTestBase<OutlineBuilder> {
 protected:
  const std::string GetShaderId() const override {
    return std::string("Outline Text Shader");
  }

  const std::string GetUniformString() const override {
    return std::string(
        "  ION Uniform {\n"
        "    Name: \"uSdfPadding\"\n"
        "    Type: Float\n"
        "    Value: 2\n"
        "  }\n"
        "  ION Uniform {\n"
        "    Name: \"uSdfSampler\"\n"
        "    Type: Texture\n"
        "    Value: ION Texture {\n"
        "      Image: Face=None, Format=Rgb888, Width=64, Height=64, Depth=1, "
        "Type=Dense, Dimensions=2\n"
        "      Level range: R[0, 1000]\n"
        "      Multisampling: Samples=0, Fixed sample locations=true\n"
        "      Swizzles: R=Red, G=Green, B=Blue, A=Alpha\n"
        "      Sampler: ION Sampler {\n"
        "        Autogenerating mipmaps: false\n"
        "        Texture compare mode: None\n"
        "        Texture compare function: Less\n"
        "        MinFilter mode: Linear\n"
        "        MagFilter mode: Linear\n"
        "        Level-of-detail range: R[-1000, 1000]\n"
        "        Wrap modes: R=Repeat, S=ClampToEdge, T=ClampToEdge\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "  ION Uniform {\n"
        "    Name: \"uTextColor\"\n"
        "    Type: FloatVector4\n"
        "    Value: V[1, 1, 1, 1]\n"
        "  }\n"
        "  ION Uniform {\n"
        "    Name: \"uOutlineColor\"\n"
        "    Type: FloatVector4\n"
        "    Value: V[0, 0, 0, 0]\n"
        "  }\n"
        "  ION Uniform {\n"
        "    Name: \"uOutlineWidth\"\n"
        "    Type: Float\n"
        "    Value: 2\n"
        "  }\n"
        "  ION Uniform {\n"
        "    Name: \"uHalfSmoothWidth\"\n"
        "    Type: Float\n"
        "    Value: 3\n"
        "  }\n");
  }
};

//-----------------------------------------------------------------------------
//
// The tests.
//
//-----------------------------------------------------------------------------

TEST_F(OutlineBuilderTest, BuildSuccess) {
  // Use glyphs that are valid in both the MockFont and MockFontImage.
  Layout layout = BuildLayout("bg");

  const math::Range2f bounds;
  const math::Vector2f offset(math::Vector2f::Zero());

  // Add glyph for a character not in the font. There should be an empty
  // rectangle for it in the resulting data.
  EXPECT_TRUE(layout.AddGlyph(
      Layout::Glyph('@', Layout::Quad(math::Point3f(0.0f, 0.0f, 0.0f),
                                      math::Point3f(1.0f, 0.0f, 0.0f),
                                      math::Point3f(1.0f, 1.0f, 0.0f),
                                      math::Point3f(0.0f, 1.0f, 0.0f)),
                                      bounds, offset)));

  // Build a Node containing the text.
  OutlineBuilder* ob = GetBuilder();
  EXPECT_TRUE(ob->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  gfx::NodePtr node = ob->GetNode();
  EXPECT_TRUE(node);
  EXPECT_TRUE(
      math::RangesAlmostEqual(math::Range3f(math::Point3f(-7.f, -7.f, 0.f),
                                            math::Point3f(12.f, 13.f, 0.f)),
                              ob->GetExtents(), kEpsilon));

  static const char kExpectedAttributeArrayString[] =
      "    ION AttributeArray {\n"
      "      Buffer Values: {\n"
      "        v 0: [-7, -4, 0], [98, 99], [1.57143, 1.30769, 0]\n"
      "        v 1: [4, -4, 0], [99, 99], [1.57143, 1.30769, 0]\n"
      "        v 2: [4, 13, 0], [99, 98], [1.57143, 1.30769, 0]\n"
      "        v 3: [-7, 13, 0], [98, 98], [1.57143, 1.30769, 0]\n"
      "        v 4: [0, -7, 0], [103, 104], [1.5, 1.30769, 0]\n"
      "        v 5: [12, -7, 0], [104, 104], [1.5, 1.30769, 0]\n"
      "        v 6: [12, 10, 0], [104, 103], [1.5, 1.30769, 0]\n"
      "        v 7: [0, 10, 0], [103, 103], [1.5, 1.30769, 0]\n"
      "        v 8: [0, 0, 0], [0, 0], [0.25, 0.0769231, 0]\n"
      "        v 9: [0, 0, 0], [0, 0], [0.25, 0.0769231, 0]\n"
      "        v 10: [0, 0, 0], [0, 0], [0.25, 0.0769231, 0]\n"
      "        v 11: [0, 0, 0], [0, 0], [0.25, 0.0769231, 0]\n"
      "      }\n"
      "      ION Attribute (Buffer) {\n"
      "        Name: \"aVertex\"\n"
      "        Enabled: true\n"
      "        Normalized: false\n"
      "      }\n"
      "      ION Attribute (Buffer) {\n"
      "        Name: \"aTexCoords\"\n"
      "        Enabled: true\n"
      "        Normalized: false\n"
      "      }\n"
      "      ION Attribute (Buffer) {\n"
      "        Name: \"aFontPixelVec\"\n"
      "        Enabled: true\n"
      "        Normalized: false\n"
      "      }\n"
      "    }\n";
  static const char kExpectedIndexBufferString[] =
      "    ION IndexBuffer {\n"
      "      Type: Unsigned Short\n"
      "      Target: Elementbuffer\n"
      "      Indices: [0 - 9: 0, 1, 2, 0, 2, 3, 4, 5, 6, 4,\n"
      "                10 - 17: 6, 7, 8, 9, 10, 8, 10, 11]\n"
      "    }\n";
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      BuildExpectedNodeString(kExpectedAttributeArrayString,
                              kExpectedIndexBufferString),
      BuildNodeString(node))) << BuildNodeString(node);
}

TEST_F(OutlineBuilderTest, BuildFailure) {
  // A valid Layout.
  Layout layout = BuildLayout("bg");
  gfxutils::ShaderManagerPtr sm;

  {
    // Null FontImagePtr, valid Layout.
    OutlineBuilderPtr ob(new OutlineBuilder(FontImagePtr(nullptr), sm,
                                            base::AllocatorPtr()));
    EXPECT_FALSE(ob->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  }

  {
    // Valid FontImagePtr, bad Layout.
    FontImagePtr font_image(new testing::MockFontImage());
    OutlineBuilderPtr ob(new OutlineBuilder(font_image, sm,
                                            base::AllocatorPtr()));
    EXPECT_FALSE(ob->Build(Layout(), ion::gfx::BufferObject::kStreamDraw));
  }
}

TEST_F(OutlineBuilderTest, RebuildAfterChanges) {
  // Build and save the results.
  Layout layout = BuildLayout("bg");
  OutlineBuilder* ob = GetBuilder();
  EXPECT_TRUE(ob->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  gfx::NodePtr node = ob->GetNode();
  EXPECT_TRUE(
      math::RangesAlmostEqual(math::Range3f(math::Point3f(-7.f, -7.f, 0.f),
                                            math::Point3f(12.f, 13.f, 0.f)),
                              ob->GetExtents(), kEpsilon));
  const std::string expected = BuildNodeString(node);

  // Rebuild after removing all uniforms. They should be restored.
  node->ClearUniforms();
  EXPECT_TRUE(ob->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected, BuildNodeString(node)));
  EXPECT_EQ(6U, node->GetUniforms().size());
  EXPECT_TRUE(
      math::RangesAlmostEqual(math::Range3f(math::Point3f(-7.f, -7.f, 0.f),
                                            math::Point3f(12.f, 13.f, 0.f)),
                              ob->GetExtents(), kEpsilon));

  // Rebuild after removing one uniform. All should be restored.
  EXPECT_EQ(6U, node->GetUniforms().size());
  base::AllocVector<gfx::Uniform> uniforms = node->GetUniforms();
  node->ClearUniforms();
  node->AddUniform(uniforms[0]);
  node->AddUniform(uniforms[1]);
  node->AddUniform(uniforms[2]);
  node->AddUniform(uniforms[3]);
  node->AddUniform(uniforms[5]);  // Skipping 4.
  EXPECT_TRUE(ob->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected, BuildNodeString(node)));
  EXPECT_EQ(6U, node->GetUniforms().size());

  // Clear the texture and rebuild. It should come back.
  node->SetUniformValue<gfx::TexturePtr>(1U, gfx::TexturePtr());
  EXPECT_TRUE(ob->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected, BuildNodeString(node)));
}

TEST_F(OutlineBuilderTest, ModifyUniforms) {
  // Build normally.
  Layout layout = BuildLayout("bg");
  OutlineBuilder* ob = GetBuilder();

  // Modifing uniforms should fail before Build() is called.
  EXPECT_FALSE(ob->SetSdfPadding(12.5f));
  EXPECT_FALSE(ob->SetTextColor(math::Point4f(.5f, 0.f, .5f, 1.f)));
  EXPECT_FALSE(ob->SetOutlineColor(math::Point4f(0.f, .5f, 0.f, .5f)));
  EXPECT_FALSE(ob->SetOutlineWidth(3.25f));
  EXPECT_FALSE(ob->SetHalfSmoothWidth(2.5f));

  // Build.
  EXPECT_TRUE(ob->Build(layout, ion::gfx::BufferObject::kStreamDraw));

  // Test default uniform values.
  gfx::NodePtr node = ob->GetNode();
  EXPECT_EQ(6U, node->GetUniforms().size());
  EXPECT_EQ(2.f, node->GetUniforms()[0].GetValue<float>());  // uSdfPadding
  EXPECT_TRUE(math::VectorBase4f::AreValuesEqual(            // uTextColor
      math::Point4f(1.f, 1.f, 1.f, 1.f),
      node->GetUniforms()[2].GetValue<math::VectorBase4f>()));
  EXPECT_TRUE(math::VectorBase4f::AreValuesEqual(            // uOutlineColor
      math::Point4f(0.f, 0.f, 0.f, 0.f),
      node->GetUniforms()[3].GetValue<math::VectorBase4f>()));
  EXPECT_EQ(2.f, node->GetUniforms()[4].GetValue<float>());  // uOutlineWidth
  EXPECT_EQ(3.f, node->GetUniforms()[5].GetValue<float>());  // uHalfSmoothWidth

  // Modify the ones that can change.
  EXPECT_TRUE(ob->SetSdfPadding(12.5f));
  EXPECT_TRUE(ob->SetTextColor(math::Point4f(.5f, 0.f, .5f, 1.f)));
  EXPECT_TRUE(ob->SetOutlineColor(math::Point4f(0.f, .5f, 0.f, .5f)));
  EXPECT_TRUE(ob->SetOutlineWidth(3.25f));
  EXPECT_TRUE(ob->SetHalfSmoothWidth(2.5f));

  // Test resulting uniform values.
  EXPECT_EQ(6U, node->GetUniforms().size());
  EXPECT_EQ(12.5f, node->GetUniforms()[0].GetValue<float>());  // uSdfPadding
  EXPECT_TRUE(math::VectorBase4f::AreValuesEqual(              // uTextColor
      math::Point4f(.5f, 0.f, .5f, 1.f),
      node->GetUniforms()[2].GetValue<math::VectorBase4f>()));
  EXPECT_TRUE(math::VectorBase4f::AreValuesEqual(              // uOutlineColor
      math::Point4f(0.f, .5f, 0.f, .5f),
      node->GetUniforms()[3].GetValue<math::VectorBase4f>()));
  EXPECT_EQ(3.25f, node->GetUniforms()[4].GetValue<float>());  // uOutlineWidth
  EXPECT_EQ(2.5f,                                           // uHalfSmoothWidth
            node->GetUniforms()[5].GetValue<float>());
}

TEST_F(OutlineBuilderTest, FontDataSubImages) {
  EXPECT_TRUE(TestDynamicFontSubImages());
}

}  // namespace text
}  // namespace ion
