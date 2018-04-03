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

#include "ion/text/basicbuilder.h"

#include <string>

#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/node.h"
#include "ion/gfxutils/shadermanager.h"
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

class BasicBuilderTest : public testing::BuilderTestBase<BasicBuilder> {
 protected:
  const std::string GetShaderId() const override {
    return std::string("Basic Text Shader");
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
        "  }\n");
  }
};

//-----------------------------------------------------------------------------
//
// The tests.
//
//-----------------------------------------------------------------------------

TEST_F(BasicBuilderTest, BuildSuccess) {
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
  BasicBuilder* bb = GetBuilder();
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  gfx::NodePtr node = bb->GetNode();
  EXPECT_TRUE(node);
  EXPECT_TRUE(
      math::RangesAlmostEqual(math::Range3f(math::Point3f(-7.f, -7.f, 0.f),
                                            math::Point3f(12.f, 13.f, 0.f)),
      bb->GetExtents(), kEpsilon));

  static const char kExpectedAttributeArrayString[] =
      "    ION AttributeArray {\n"
      "      Buffer Values: {\n"
      "        v 0: [-7, -4, 0], [98, 99]\n"
      "        v 1: [4, -4, 0], [99, 99]\n"
      "        v 2: [4, 13, 0], [99, 98]\n"
      "        v 3: [-7, 13, 0], [98, 98]\n"
      "        v 4: [0, -7, 0], [103, 104]\n"
      "        v 5: [12, -7, 0], [104, 104]\n"
      "        v 6: [12, 10, 0], [104, 103]\n"
      "        v 7: [0, 10, 0], [103, 103]\n"
      "        v 8: [0, 0, 0], [0, 0]\n"
      "        v 9: [0, 0, 0], [0, 0]\n"
      "        v 10: [0, 0, 0], [0, 0]\n"
      "        v 11: [0, 0, 0], [0, 0]\n"
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

TEST_F(BasicBuilderTest, BuildFailure) {
  // A valid Layout.
  Layout layout = BuildLayout("bg");
  gfxutils::ShaderManagerPtr sm;

  {
    // Null FontImagePtr, valid Layout..
    BasicBuilderPtr bb(new BasicBuilder(FontImagePtr(nullptr), sm,
                                        base::AllocatorPtr()));
    EXPECT_FALSE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  }

  {
    // Valid FontImagePtr, bad Layout.
    FontImagePtr font_image(new testing::MockFontImage());
    BasicBuilderPtr bb(new BasicBuilder(font_image, sm, base::AllocatorPtr()));
    EXPECT_FALSE(bb->Build(Layout(), ion::gfx::BufferObject::kStreamDraw));
  }
}

TEST_F(BasicBuilderTest, Rebuild) {
  Layout layout = BuildLayout("bg");

  // Build a Node containing the text.
  BasicBuilder* bb = GetBuilder();
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  gfx::NodePtr node = bb->GetNode();
  EXPECT_TRUE(node);
  EXPECT_TRUE(
      math::RangesAlmostEqual(math::Range3f(math::Point3f(-7.f, -7.f, 0.f),
                                            math::Point3f(12.f, 13.f, 0.f)),
      bb->GetExtents(), kEpsilon));

  // Rebuild using a different layout with the same number of glyphs.
  layout = BuildLayout("gb");
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  static const char kExpectedAttributeArrayString1[] =
      "    ION AttributeArray {\n"
      "      Buffer Values: {\n"
      "        v 0: [-8, -7, 0], [103, 104]\n"
      "        v 1: [4, -7, 0], [104, 104]\n"
      "        v 2: [4, 10, 0], [104, 103]\n"
      "        v 3: [-8, 10, 0], [103, 103]\n"
      "        v 4: [1, -4, 0], [98, 99]\n"
      "        v 5: [12, -4, 0], [99, 99]\n"
      "        v 6: [12, 13, 0], [99, 98]\n"
      "        v 7: [1, 13, 0], [98, 98]\n"
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
      "    }\n";
  static const char kExpectedIndexBufferString1[] =
      "    ION IndexBuffer {\n"
      "      Type: Unsigned Short\n"
      "      Target: Elementbuffer\n"
      "      Indices: [0 - 9: 0, 1, 2, 0, 2, 3, 4, 5, 6, 4,\n"
      "                10 - 11: 6, 7]\n"
      "    }\n";
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      BuildExpectedNodeString(kExpectedAttributeArrayString1,
                              kExpectedIndexBufferString1),
      BuildNodeString(node))) << BuildNodeString(node);
  EXPECT_TRUE(
      math::RangesAlmostEqual(math::Range3f(math::Point3f(-8.f, -7.f, 0.f),
                                            math::Point3f(12.f, 13.f, 0.f)),
      bb->GetExtents(), kEpsilon));

  // Rebuild using a layout with a different number of glyphs.
  // Note that the string has to be chosen carefully as this test is quite
  // fragile - the ASMJS build will produce different values within floating
  // point epsilon for (for example) the string "bgb".
  layout = BuildLayout("agb");
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  static const char kExpectedAttributeArrayString2[] =
      "    ION AttributeArray {\n"
      "      Buffer Values: {\n"
      "        v 0: [-12, -4, 0], [97, 98]\n"
      "        v 1: [-1, -4, 0], [98, 98]\n"
      "        v 2: [-1, 10, 0], [98, 97]\n"
      "        v 3: [-12, 10, 0], [97, 97]\n"
      "        v 4: [-4, -7, 0], [103, 104]\n"
      "        v 5: [8, -7, 0], [104, 104]\n"
      "        v 6: [8, 10, 0], [104, 103]\n"
      "        v 7: [-4, 10, 0], [103, 103]\n"
      "        v 8: [5, -4, 0], [98, 99]\n"
      "        v 9: [16, -4, 0], [99, 99]\n"
      "        v 10: [16, 13, 0], [99, 98]\n"
      "        v 11: [5, 13, 0], [98, 98]\n"
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
      "    }\n";
  static const char kExpectedIndexBufferString2[] =
      "    ION IndexBuffer {\n"
      "      Type: Unsigned Short\n"
      "      Target: Elementbuffer\n"
      "      Indices: [0 - 9: 0, 1, 2, 0, 2, 3, 4, 5, 6, 4,\n"
      "                10 - 17: 6, 7, 8, 9, 10, 8, 10, 11]\n"
      "    }\n";
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      BuildExpectedNodeString(kExpectedAttributeArrayString2,
                              kExpectedIndexBufferString2),
      BuildNodeString(node))) << BuildNodeString(node);
  EXPECT_TRUE(
      math::RangesAlmostEqual(math::Range3f(math::Point3f(-12.f, -7.f, 0.f),
                                            math::Point3f(16.f, 13.f, 0.f)),
      bb->GetExtents(), kEpsilon));
}

TEST_F(BasicBuilderTest, RebuildAfterChanges) {
  // Build and save the results.
  Layout layout = BuildLayout("bg");
  BasicBuilder* bb = GetBuilder();
  ASSERT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  gfx::NodePtr node = bb->GetNode();
  const std::string expected = BuildNodeString(node);

  // Rebuild after removing all uniforms. They should be restored.
  node->ClearUniforms();
  ASSERT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected, BuildNodeString(node)));
  EXPECT_EQ(3U, node->GetUniforms().size());

  // Rebuild after removing one uniform. They should be restored.
  EXPECT_EQ(3U, node->GetUniforms().size());
  base::AllocVector<gfx::Uniform> uniforms = node->GetUniforms();
  node->ClearUniforms();
  node->AddUniform(uniforms[0]);
  ASSERT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected, BuildNodeString(node)));
  EXPECT_EQ(3U, node->GetUniforms().size());

  // Clear the texture and rebuild. It should come back.
  node->SetUniformValue<gfx::TexturePtr>(1U, gfx::TexturePtr());
  ASSERT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected, BuildNodeString(node)));
}

TEST_F(BasicBuilderTest, BuildWithShaderManager) {
  // Build with no ShaderManager.
  Layout layout = BuildLayout("bg");
  BasicBuilder* bb = GetBuilder();
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  const std::string expected = BuildNodeString(bb->GetNode());

  // Use a ShaderManager to compose the ShaderProgram and build again.
  UseBuilderWithShaderManager();
  bb = GetBuilder();
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected, BuildNodeString(bb->GetNode())));
}

TEST_F(BasicBuilderTest, BuildWithExistingShader) {
  gfxutils::ShaderManagerPtr sm = UseBuilderWithShaderManagerAndShader();
  // Create a trivial shader with the same name used by the BasicBuilder.
  const std::string dummy_source = "#version 100\nvoid main(void) { }";
  gfx::ShaderInputRegistryPtr registry(new gfx::ShaderInputRegistry());
  gfx::ShaderProgramPtr shader = sm->CreateShaderProgram(
      GetShaderId(), registry,
      gfxutils::ShaderSourceComposerPtr(
          new gfxutils::StringComposer("vertex shader", dummy_source)),
      gfxutils::ShaderSourceComposerPtr(
          new ion::gfxutils::StringComposer("fragment shader", dummy_source)));
  // Verify the shader has been registered with the ShaderManager.
  EXPECT_EQ(1U, sm->GetShaderProgramNames().size());
  // Create a shader with the expected shader ID before building.
  Layout layout = BuildLayout("bg");
  BasicBuilder* bb = GetBuilder();
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  // Ensure the existing shader is used and a second has not been rebuilt.
  EXPECT_EQ(1U, sm->GetShaderProgramNames().size());
  // Ensure that the correct ShaderInputRegistry is used.
  const auto& uniforms = bb->GetNode()->GetUniforms();
  EXPECT_LT(0U, uniforms.size());
  EXPECT_EQ(&uniforms[0].GetRegistry(), registry.Get());
}

TEST_F(BasicBuilderTest, SetFontImage) {
  // Build as usual.
  BasicBuilder* bb = GetBuilder();
  Layout layout = BuildLayout("bg");
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  const std::string expected = BuildNodeString(bb->GetNode());

  // Replace the FontImage with another MockFontImage instance. The resulting
  // data should be identical since the MockFontImages contain the same data.
  bb->SetFontImage(BuildMockFontImage());
  EXPECT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      expected, BuildNodeString(bb->GetNode())));
}

TEST_F(BasicBuilderTest, ModifyUniforms) {
  // Build normally.
  Layout layout = BuildLayout("bg");
  BasicBuilder* bb = GetBuilder();

  // Modifying uniforms should fail before Build() is called.
  EXPECT_FALSE(bb->SetSdfPadding(12.5f));
  EXPECT_FALSE(bb->SetTextColor(math::Point4f(.5f, 0.f, .5f, 1.f)));

  // Build.
  ASSERT_TRUE(bb->Build(layout, ion::gfx::BufferObject::kStreamDraw));

  // Test default uniform values.
  gfx::NodePtr node = bb->GetNode();
  EXPECT_EQ(3U, node->GetUniforms().size());
  EXPECT_EQ(2.f, node->GetUniforms()[0].GetValue<float>());  // uSdfPadding
  EXPECT_TRUE(math::VectorBase4f::AreValuesEqual(            // uTextColor
      math::Point4f(1.f, 1.f, 1.f, 1.f),
      node->GetUniforms()[2].GetValue<math::VectorBase4f>()));

  // Modify the ones that can change.
  EXPECT_TRUE(bb->SetSdfPadding(12.5f));
  EXPECT_TRUE(bb->SetTextColor(math::Point4f(.5f, 0.f, .5f, 1.f)));

  // Test resulting uniform values.
  EXPECT_EQ(3U, node->GetUniforms().size());
  EXPECT_EQ(12.5f, node->GetUniforms()[0].GetValue<float>());  // uSdfPadding
  EXPECT_TRUE(math::VectorBase4f::AreValuesEqual(              // uTextColor
      math::Point4f(.5f, 0.f, .5f, 1.f),
      node->GetUniforms()[2].GetValue<math::VectorBase4f>()));
}

TEST_F(BasicBuilderTest, FontDataSubImages) {
  EXPECT_TRUE(TestDynamicFontSubImages());
}

TEST_F(BasicBuilderTest, LayoutMetrics) {
  Layout layout = BuildLayout("bg");

  EXPECT_EQ(19.f, layout.GetLineAdvanceHeight());

  EXPECT_EQ(2U, layout.GetGlyphCount());
  const Layout::Glyph& g0 = layout.GetGlyph(0);
  const Layout::Glyph& g1 = layout.GetGlyph(1);
  EXPECT_EQ(math::Vector2f(1.f, -1.f), g0.offset);
  EXPECT_EQ(math::Vector2f(0.f, -4.f), g1.offset);
  EXPECT_EQ(math::Range2f(math::Point2f(-5.f, -2.f), math::Point2f(2.f, 11.f)),
            g0.bounds);
  EXPECT_EQ(math::Range2f(math::Point2f(2.f, -5.f), math::Point2f(10.f, 8.f)),
            g1.bounds);
}

}  // namespace text
}  // namespace ion
