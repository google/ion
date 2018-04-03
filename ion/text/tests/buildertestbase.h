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

#ifndef ION_TEXT_TESTS_BUILDERTESTBASE_H_
#define ION_TEXT_TESTS_BUILDERTESTBASE_H_

#include <sstream>
#include <string>

#include "ion/base/logging.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/texture.h"
#include "ion/gfxutils/printer.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/text/font.h"
#include "ion/text/layout.h"
#include "ion/text/tests/mockfont.h"
#include "ion/text/tests/mockfontimage.h"
#include "ion/text/tests/testfont.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace text {
namespace testing {

// This class defines a test harness that adds some convenience functions to
// simplify testing of derived Builder classes.
template <typename BuilderType> class BuilderTestBase : public ::testing::Test {
 protected:
  // Sets up the BasicBuilder with a MockFont and MockStaticFontImage.
  void SetUp() override {
    builder_ =
        new TestBuilder(gfxutils::ShaderManagerPtr(), FontImage::kStatic);
    EXPECT_TRUE(builder_->GetFont());
  }

  // Returns the Font.
  const Font& GetFont() const { return *builder_->GetFont(); }

  // Returns the Builder instance.
  BuilderType* GetBuilder() { return builder_.Get(); }

  // Builds and returns a MockStaticFontImage instance that can be used for
  // testing.
  static const FontImagePtr BuildMockFontImage() {
    FontPtr font(BuildTestFreeTypeFont("Test", 16U, 2U));
    return FontImagePtr(new MockFontImage(font));
  }

  // Builds and returns a MockDynamicFontImage instance that can be used for
  // testing.
  static const FontImagePtr BuildDynamicFontImage() {
    FontPtr font(BuildTestFreeTypeFont("Test", 16U, 2U));
    return FontImagePtr(new DynamicFontImage(font, 256U));
  }

  // Builds a Layout for a text string using the Font from the Builder. This
  // uses identity scaling to keep the numbers simple.
  const Layout BuildLayout(const std::string& text) {
    const Font& font = GetFont();
    LayoutOptions options;
    options.target_point.Set(2.0f, 3.0f);
    options.target_size.Set(0.0f, static_cast<float>(font.GetSizeInPixels()));
    options.horizontal_alignment = kAlignHCenter;
    options.vertical_alignment = kAlignVCenter;
    return font.BuildLayout(text, options);
  }

  // This uses a Printer to return a string with the contents of a built Node.
  static const std::string BuildNodeString(const gfx::NodePtr& node) {
    std::ostringstream out;
    ion::gfxutils::Printer printer;
    printer.EnableAddressPrinting(false);
    printer.EnableFullShapePrinting(true);
    printer.PrintScene(node, out);
    return out.str();
  }

  // Builds an expected Node contents string.
  const std::string BuildExpectedNodeString(
      const std::string& expected_attribute_array_string,
      const std::string& expected_index_buffer_string) {
    static const char kExpectedNodeStartString[] =
        "ION Node {\n"
        "  Enabled: true\n";
    static const char kExpectedStateTableString[] =
        "  ION StateTable {\n"
        "    Blend: true\n"
        "    CullFace: false\n"
        "    Blend Equations: RGB=Add, Alpha=Add\n"
        "    Blend Functions: RGB-src=One, RGB-dest=OneMinusSrcAlpha, "
        "Alpha-src=One, Alpha-dest=OneMinusSrcAlpha\n"
        "  }\n";
    static const char kExpectedShapeStartString[] =
        "  ION Shape {\n"
        "    Primitive Type: Triangles\n";
    static const char kExpectedEndString[] =
        "  }\n"
        "}\n";
    return
        kExpectedNodeStartString + GetShaderIdString() +
        kExpectedStateTableString + GetUniformString() +
        kExpectedShapeStartString + expected_attribute_array_string +
        expected_index_buffer_string + kExpectedEndString;
  }

  // Replaces the Builder with one that uses a ShaderManager.
  void UseBuilderWithShaderManager() {
    gfxutils::ShaderManagerPtr sm(new gfxutils::ShaderManager());
    builder_ = new TestBuilder(sm, FontImage::kStatic);
  }

  // Replaces the builder with one that uses a ShaderManager that already
  // contains a shader with the same shader name as used by the builder.
  // Returns the newly created ShaderManager.
  gfxutils::ShaderManagerPtr UseBuilderWithShaderManagerAndShader() {
    gfxutils::ShaderManagerPtr sm(new gfxutils::ShaderManager());
    builder_ = new TestBuilder(sm, FontImage::kStatic);
    return sm;
  }

  // Provides a formatted version of the shader id.
  const std::string GetShaderIdString() const {
    return std::string("  Shader ID: \"") + GetShaderId() + "\"\n";
  }

  // Derived classes must implement these functions.
  virtual const std::string GetShaderId() const = 0;
  virtual const std::string GetUniformString() const = 0;

  // Tests that BuilderType properly propagate sub-images to Textures and clears
  // them from ImageData.
  ::testing::AssertionResult TestDynamicFontSubImages() {
    base::SharedPtr<BuilderType> builder(
        new TestBuilder(gfxutils::ShaderManagerPtr(), FontImage::kDynamic));
    Layout layout = BuildLayout("bg");

    // Build.
    if (!builder->Build(layout, ion::gfx::BufferObject::kStreamDraw))
        return ::testing::AssertionFailure() << "Unable to build initial";
    DynamicFontImage* dfi =
        static_cast<DynamicFontImage*>(builder->GetFontImage().Get());
    if (base::IsInvalidReference(dfi->GetImageData(0)))
      return ::testing::AssertionFailure()
             << "Initial ImageData should be valid";
    {
      const FontImage::ImageData& data = dfi->GetImageData(0);
      if (!data.texture->GetSubImages().empty())
        return ::testing::AssertionFailure()
               << "Initial sub-images should be empty";
    }

    // Add some more glyphs to force sub-image creation.
    GlyphSet glyph_set(base::AllocatorPtr(nullptr));
    glyph_set.insert(dfi->GetFont()->GetDefaultGlyphForChar('A'));
    glyph_set.insert(dfi->GetFont()->GetDefaultGlyphForChar('.'));
    const FontImage::ImageData& data = dfi->FindImageData(glyph_set);
    EXPECT_EQ("Test_16_0", data.texture->GetLabel());
    if (data.texture->GetSubImages().size() != 2U)
      return ::testing::AssertionFailure()
             << "ImageData should have 2 sub-images, not "
             << data.texture->GetSubImages().size();

    gfx::NodePtr node = builder->GetNode();
    gfx::TexturePtr tex = node->GetUniforms()[1].GetValue<gfx::TexturePtr>();
    if (tex->GetSubImages().empty())
      return ::testing::AssertionFailure() << "Texture should have sub-images.";

    // Calling Build should transfer the sub-image data to the Font's Texture.
    if (!builder->Build(layout, ion::gfx::BufferObject::kStreamDraw))
      return ::testing::AssertionFailure() << "Unable to build secondary";
    if (data.texture->GetSubImages().empty())
      return ::testing::AssertionFailure()
             << "Secondary data should have sub-images";
    if (tex->GetSubImages().size() != 2U)
      return ::testing::AssertionFailure()
             << "Texture should have 2 sub-images, not "
             << data.texture->GetSubImages().size();
    return ::testing::AssertionSuccess();
  }

 private:
  // Derived Builder class that provides access to the constructor that
  // installs a MockFont and MockFontImage. The MockFontImage returns simple
  // texture coordinates for glyphs so we can avoid precision issues.
  class TestBuilder : public BuilderType {
   public:
    TestBuilder(const gfxutils::ShaderManagerPtr& shader_manager,
                FontImage::Type type)
        : BuilderType(type == FontImage::kStatic ? BuildMockFontImage()
                                                 : BuildDynamicFontImage(),
                      shader_manager, base::AllocatorPtr()) {}
  };

  base::SharedPtr<BuilderType> builder_;
};

}  // namespace testing
}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_TESTS_BUILDERTESTBASE_H_
