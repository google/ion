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

#include <cmath>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/setting.h"
#include "ion/base/settingmanager.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/demos/utils.h"
#include "ion/demos/viewerdemobase.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/node.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"
#include "ion/text/basicbuilder.h"
#include "ion/text/builder.h"
#include "ion/text/font.h"
#include "ion/text/fontimage.h"
#include "ion/text/fontmanager.h"
#include "ion/text/layout.h"
#include "ion/text/outlinebuilder.h"

using ion::math::Point2f;
using ion::math::Point2i;
using ion::math::Point3f;
using ion::math::Point4f;
using ion::math::Range2i;
using ion::math::Range2f;
using ion::math::Vector2f;
using ion::math::Vector2i;
using ion::math::Vector3f;
using ion::math::Vector4f;
using ion::math::Matrix4f;

// Resources for the demo.
ION_REGISTER_ASSETS(TextDemoAssets);

//-----------------------------------------------------------------------------
//
// Types and constants that cannot be in the anonymous namespace.
//
//-----------------------------------------------------------------------------

// This enum is used to select different text styles (via different Builders).
enum TextStyle {
  kBasic,     // Basic text, uses BasicBuilder.
  kOutlined,  // Outlined text, uses OutlineBuilder.
};

// This struct contains everything needed to build the correct text string(s).
struct TextInfo {
  ion::text::FontPtr font;
  ion::text::FontImagePtr font_image;
  TextStyle style;
  std::vector<std::string> text_strings;
  bool cylindrical_layout;
  ion::text::LayoutOptions layout_options;
};

namespace {

//-----------------------------------------------------------------------------
//
// Types and constants.
//
//-----------------------------------------------------------------------------

// Font info.
static const char kFontName[] = "Tuffy";

// Target text string width.
static const float kTextWidth = 8.0f;

// Default text string to display.
static const char kDefaultString[] =
    "First line_n"
    "Line 2"
    "_s"
    "Second string_n"
    "Unicode: _xc3_xb7_n"
    "Done!_n";

// Multiple strings are separated by these amounts in Y and Z.
static const float kStringOffsetY = 4.f;
static const float kStringOffsetZ = .8f;

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Helper for FixInputString() that converts a hex character to its integer
// equivalent. Returns -1 on error.
static int FromHexChar(const char c) {
  int digit;
  if (c >= '0' && c <= '9')
    digit = c - '0';
  else if (c >= 'a' && c <= 'f')
    digit = 10 + c - 'a';
  else if (c >= 'A' && c <= 'F')
    digit = 10 + c - 'A';
  else
    digit = -1;
  return digit;
}

// Parses text in an input string to process string breaks, line breaks, and
// UTF-8 sequences. The SettingsManager does not pass backslashes through, so
// this uses underscores to signify the following special sequences:
// sequences:
//
//   "_s"    => new string
//   "_n"    => newline
//   "_xNN"  => hex character 0xNN
//
// The "new string" sequence is used to separate strings when using a
// DynamicFontImage. Each string is processed using a potentially different
// FontImage::ImageData instance. It is equivalent to a newline when using a
// StaticFontImage.
//
// 
static const std::vector<std::string> ParseInputStrings(const std::string& s) {
  std::vector<std::string> strings;
  std::string fixed;
  const size_t len = s.size();
  fixed.reserve(len);
  bool error = false;
  for (size_t i = 0; i < len; ++i) {
    if (s[i] == '_') {
      if (++i >= len) {
        LOG(ERROR) << "Missing character after '_'";
        error = true;
        break;
      } else {
        if (s[i] == 's') {          // New string.
          if (!fixed.empty())
            strings.push_back(fixed);
          fixed.clear();
        } else if (s[i] == 'n') {   // Newline.
          fixed += '\n';
        } else if (s[i] == 'x') {   // Hex character.
          if (i + 2 < len) {
            const int digit1 = FromHexChar(s[++i]);
            const int digit2 = FromHexChar(s[++i]);
            if (digit1 >= 0 && digit2 >= 0) {
              fixed += static_cast<char>(digit1 * 16 + digit2);
            } else {
              LOG(ERROR) << "Illegal hex digit characters '"
                         << s[i - 1] << s[i] << "'";
              error = true;
            }
          } else {
            LOG(ERROR) << "Missing digit characters after '_x'";
            error = true;
          }
        }
      }
    } else {
      fixed += s[i];
    }
  }
  if (!fixed.empty())
    strings.push_back(fixed);
  if (error) {
    LOG(ERROR) << "Error processing special characters in string";
  }
  return strings;
}

// Initializes and returns a Font to use for the demo.
static const ion::text::FontPtr CreateFont(
    const ion::text::FontManagerPtr& font_manager, size_t font_size,
    size_t sdf_padding) {
  ion::text::FontPtr font = demoutils::InitFont(
      font_manager, kFontName, font_size, sdf_padding);
  if (!font.Get()) {
    LOG(FATAL) << "Could not initialize font";
  }
  return font;
}

// Initializes and returns a FontImage to use for the demo.
static const ion::text::FontImagePtr CreateFontImage(
    const ion::text::FontPtr& font, ion::text::FontImage::Type type) {
  if (type == ion::text::FontImage::kStatic) {
    static const size_t kMaxFontImageSize = 1024U;
    // Create a GlyphSet containing all ASCII characters.
    ion::text::GlyphSet glyph_set(ion::base::AllocatorPtr(nullptr));
    font->AddGlyphsForAsciiCharacterRange(1, 127, &glyph_set);
    glyph_set.insert(font->GetDefaultGlyphForChar(0xf7));  // Division sign.
    ion::text::StaticFontImagePtr font_image(
        new ion::text::StaticFontImage(font, kMaxFontImageSize, glyph_set));
    if (!font_image->GetImageData().texture.Get()) {
      LOG(FATAL) << "Could not initialize StaticFontImage";
    }
    return font_image;
  } else {
    static const size_t kFontImageSize = 256U;
    ion::text::DynamicFontImagePtr font_image(
        new ion::text::DynamicFontImage(font, kFontImageSize));
    return font_image;
  }
}

static void ModifyLayoutToCylinder(ion::text::Layout* layout) {
  // Center and radius of the cylinder.
  static const ion::math::Point3f kCylinderCenter(0.0f, 0.0f, -5.0f);
  static const float kCylinderRadius = 5.0f;

  const size_t num_glyphs = layout->GetGlyphCount();
  for (size_t i = 0; i < num_glyphs; ++i) {
    ion::text::Layout::Glyph glyph = layout->GetGlyph(i);
    for (int j = 0; j < 4; ++j) {
      Point3f& p = glyph.quad.points[j];
      const float y = p[1];

      // Vector from the point to the center of the cylinder.
      Vector3f v = glyph.quad.points[j] - kCylinderCenter;
      v[1] = 0.0f;

      p = kCylinderCenter + kCylinderRadius * ion::math::Normalized(v);
      p[1] = y;
    }
    layout->ReplaceGlyph(i, glyph);
  }
}

static const std::vector<ion::text::Layout> BuildLayouts(
    const TextInfo& text_info) {
  std::vector<ion::text::Layout> layouts;
  const size_t num_strings = text_info.text_strings.size();
  layouts.reserve(num_strings);
  for (size_t i = 0; i < num_strings; ++i) {
    layouts.push_back(text_info.font->BuildLayout(text_info.text_strings[i],
                                                  text_info.layout_options));
    if (text_info.cylindrical_layout)
      ModifyLayoutToCylinder(&layouts.back());
  }
  return layouts;
}

static bool BuildTextNodes(const std::vector<ion::text::BuilderPtr>& builders,
                           const std::vector<ion::text::Layout>& layouts,
                           const ion::gfx::NodePtr& text_root) {
  DCHECK(text_root.Get());
  text_root->ClearChildren();
  bool ok = true;
  const size_t num_layouts = layouts.size();
  DCHECK_EQ(builders.size(), num_layouts);
  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();
  const Vector3f translation(0.f, -kStringOffsetY, -kStringOffsetZ);
  for (size_t i = 0; i < num_layouts; ++i) {
    DCHECK(builders[i].Get());
    if (builders[i]->Build(layouts[i], ion::gfx::BufferObject::kStreamDraw)) {
      const ion::gfx::NodePtr& text_node = builders[i]->GetNode();
      DCHECK(text_node.Get());
      // Add or update the uModelviewMatrix uniform in the node.
      const Matrix4f m =
          ion::math::TranslationMatrix(static_cast<float>(i) * translation);
      if (!text_node->SetUniformByName("uModelviewMatrix", m))
        demoutils::AddUniformToNode(global_reg, "uModelviewMatrix", m,
                                    text_node);
      text_root->AddChild(text_node);
    } else {
      ok = false;
    }
  }
  return ok;
}

static ion::text::FontImage::Type FontImageTypeFromInt(int i) {
  if (i >= ion::text::FontImage::kStatic &&
      i <= ion::text::FontImage::kDynamic) {
    return static_cast<ion::text::FontImage::Type>(i);
  } else {
    LOG(ERROR) << "Invalid font image type: " << i;
    return ion::text::FontImage::kStatic;
  }
}

static ion::text::HorizontalAlignment HorizontalAlignmentFromInt(int i) {
  if (i >= ion::text::kAlignLeft && i <= ion::text::kAlignRight) {
    return static_cast<ion::text::HorizontalAlignment>(i);
  } else {
    LOG(ERROR) << "Invalid horizontal alignment value: " << i;
    return ion::text::kAlignHCenter;
  }
}

static ion::text::VerticalAlignment VerticalAlignmentFromInt(int i) {
  if (i >= ion::text::kAlignTop && i <= ion::text::kAlignBottom) {
    return static_cast<ion::text::VerticalAlignment>(i);
  } else {
    LOG(ERROR) << "Invalid vertical alignment value: " << i;
    return ion::text::kAlignVCenter;
  }
}

static TextStyle TextStyleFromInt(int i) {
  if (i >= kBasic && i <= kOutlined) {
    return static_cast<TextStyle>(i);
  } else {
    LOG(ERROR) << "Invalid text style value: " << i;
    return kBasic;
  }
}

static ion::gfx::NodePtr BuildOriginNode() {
  // Size and color of the square indicating the origin.
  static const float kSquareHalfSize = 0.04f;
  static const Vector4f kSquareColor(0.9f, 0.3f, 0.8f, 1.0f);

  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();

  Vector3f v[6];
  v[0].Set(-kSquareHalfSize, -kSquareHalfSize, 0.0f);
  v[1].Set(kSquareHalfSize, -kSquareHalfSize, 0.0f);
  v[2].Set(kSquareHalfSize, kSquareHalfSize, 0.0f);
  v[3].Set(-kSquareHalfSize, -kSquareHalfSize, 0.0f);
  v[4].Set(kSquareHalfSize, kSquareHalfSize, 0.0f);
  v[5].Set(-kSquareHalfSize, kSquareHalfSize, 0.0f);
  ion::gfx::BufferObjectPtr buffer_object(new ion::gfx::BufferObject);
  ion::base::DataContainerPtr container =
      ion::base::DataContainer::CreateAndCopy<Vector3f>(
          v, 6U, false, buffer_object->GetAllocator());
  buffer_object->SetData(container, sizeof(v[0]), 6U,
                         ion::gfx::BufferObject::kStaticDraw);

  ion::gfx::AttributeArrayPtr attribute_array(new ion::gfx::AttributeArray);
  ion::gfxutils::BufferToAttributeBinder<Vector3f>(v[0])
      .Bind(v[0], "aVertex")
      .Apply(global_reg, attribute_array, buffer_object);

  ion::gfx::ShapePtr shape(new ion::gfx::Shape);
  shape->SetPrimitiveType(ion::gfx::Shape::kTriangles);
  shape->SetAttributeArray(attribute_array);

  ion::gfx::NodePtr node(new ion::gfx::Node);
  demoutils::AddUniformToNode(global_reg, "uBaseColor", kSquareColor, node);
  node->AddShape(shape);
  return node;
}

// Builds the Ion graph for the demo.
static ion::gfx::NodePtr BuildGraph(int width, int height,
                                    ion::gfx::NodePtr* text_root,
                                    ion::gfx::NodePtr* origin_node) {
  ion::gfx::NodePtr root(new ion::gfx::Node);

  // Set up global state.
  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable(width, height));
  state_table->SetViewport(Range2i::BuildWithSize(Point2i(0, 0),
                                                  Vector2i(width, height)));
  state_table->SetClearColor(Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  root->SetStateTable(state_table);

  // Text.
  (*text_root).Reset(new ion::gfx::Node);
  (*text_root)->SetLabel("Text Root");
  root->AddChild(*text_root);

  // Node displaying origin as a small square, disabled by default.
  *origin_node = BuildOriginNode();
  (*origin_node)->Enable(false);
  root->AddChild(*origin_node);

  return root;
}

// Clamps the value of a modified size_t Setting to [min, max] and updates the
// UI if necessary. Returns the new Setting value.
static size_t ClampSetting(const size_t min, const size_t max,
                           ion::base::Setting<size_t>* setting) {
  // Clamp to the range.
  const size_t clamped = ion::math::Clamp(setting->GetValue(), min, max);

  // Update the UI if clamping changed the value.
  if (clamped != *setting) {
    // Don't want to get notified again for this change.
    setting->EnableListener("TextDemo", false);
    setting->SetValue(clamped);
    setting->EnableListener("TextDemo", true);
  }
  return clamped;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// TextDemo class.
//
//-----------------------------------------------------------------------------

class IonTextDemo : public ViewerDemoBase {
 public:
  IonTextDemo(int width, int height);
  ~IonTextDemo() override;
  void Resize(int width, int height) override;
  void Update() override {}
  void RenderFrame() override;
  void Keyboard(int key, int x, int y, bool is_press) override {}
  std::string GetDemoClassName() const override { return "TextDemo"; }

 private:
  // Class initialization helpers.
  bool InitTextInfo(TextInfo* text_info);
  void InitSettings();

  // Callbacks for settings.
  void UpdateFont(ion::base::SettingBase* setting);
  void UpdateText(ion::base::SettingBase* setting);
  void UpdateFontImageType(ion::base::SettingBase* setting);
  void UpdateTextStyle(ion::base::SettingBase* setting);

  // Other update helper functions.
  void UpdateTextUniforms();
  void UpdateBuilders();
  void UpdateTextNodes();

  ion::text::FontManagerPtr font_manager_;
  ion::gfx::NodePtr root_;
  ion::gfx::NodePtr text_root_;
  ion::gfx::NodePtr origin_node_;
  std::unique_ptr<TextInfo> text_info_;

  // One Builder for each text string to display.
  std::vector<ion::text::BuilderPtr> builders_;

  // Font settings.
  ion::base::Setting<size_t> font_size_;
  ion::base::Setting<size_t> sdf_padding_;
  ion::base::Setting<int> font_image_type_;

  // Layout settings.
  ion::base::Setting<std::string> string_;
  ion::base::Setting<bool> cylindrical_layout_;
  ion::base::Setting<int> horizontal_alignment_;
  ion::base::Setting<int> vertical_alignment_;
  ion::base::Setting<float> line_spacing_;

  // Shader settings.
  ion::base::Setting<ion::math::Point4f> text_color_;
  ion::base::Setting<ion::math::Point4f> outline_color_;
  ion::base::Setting<float> smooth_width_;
  ion::base::Setting<float> outline_width_;

  // Other settings.
  ion::base::Setting<bool> display_origin_;
  ion::base::Setting<int> text_style_;
  ion::base::Setting<bool> check_errors_;
};

IonTextDemo::IonTextDemo(int width, int height)
    : ViewerDemoBase(width, height),
      font_manager_(new ion::text::FontManager),
      text_info_(new TextInfo),

      // Font settings.
      font_size_("textdemo/font/font_size", 32U, "Font size in pixels"),
      sdf_padding_("textdemo/font/sdf_padding", 8U, "SDF font image padding"),
      font_image_type_("textdemo/font_image_type",
                       ion::text::FontImage::kStatic,
                       "FontImage type"),

      // Layout settings.
      string_("textdemo/layout/string", kDefaultString,
              "Text string to display"),
      cylindrical_layout_("textdemo/layout/cylindrical_layout", false,
                          "Lay the text out on the surface of a cylinder"),
      horizontal_alignment_("textdemo/layout/horizontal_alignment",
                            ion::text::kAlignHCenter,
                            "Horizontal alignment of text"),
      vertical_alignment_("textdemo/layout/vertical_alignment",
                          ion::text::kAlignVCenter,
                          "Vertical alignment of text"),
      line_spacing_("textdemo/layout/line_spacing", 1.0f,
                    "Spacing between lines as a fraction of max glyph height"),

      // Shader settings.
      text_color_("textdemo/shader/text_color", Point4f(1.f, 1.f, 1.f, 1.f),
                  "Foreground color of text"),
      outline_color_("textdemo/shader/outline_color",
                     Point4f(0.f, 0.f, 0.f, 1.f), "Outline color of text"),
      smooth_width_("textdemo/shader/smooth_width", 6.f,
                    "Width of edge smoothing band in pixels"),
      outline_width_("textdemo/shader/outline_width", 2.f,
                     "Width of text outline in pixels, or 0 for none;"),

      // Other settings.
      display_origin_("textdemo/display_origin", false,
                      "Display a marker at the world origin"),
      text_style_("textdemo/text_style", kOutlined, "Text rendering style"),
      check_errors_("textdemo/check_errors", false,
                    "Enable OpenGL error checking") {
  if (!TextDemoAssets::RegisterAssets()) {
    LOG(ERROR) << "Could not register demo assets";
    exit(0);
  }

  // Set up the TextInfo with everything that is needed.
  if (!InitTextInfo(text_info_.get()))
    exit(0);

  // Build the Ion graph.
  root_ = BuildGraph(width, height, &text_root_, &origin_node_);

  // Set up viewing. Use a fairly generous view radius so that the default text
  // is not cut off when rotated.
  SetTrackballRadius(1.5f * kTextWidth);
  SetNodeWithViewUniforms(root_);

  // Set up the remote handlers.
  InitRemoteHandlers(std::vector<ion::gfx::NodePtr>(1, root_));

  // Set up the settings.
  InitSettings();

  // Initialize the uniforms and matrices in the graph.
  UpdateViewUniforms();

  // Update the graph to display the correct text.
  UpdateText(nullptr);
}

IonTextDemo::~IonTextDemo() {}

void IonTextDemo::Resize(int width, int height) {
  ViewerDemoBase::Resize(width, height);

  DCHECK(root_->GetStateTable().Get());
  root_->GetStateTable()->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
}

void IonTextDemo::RenderFrame() {
  origin_node_->Enable(display_origin_);
  GetGraphicsManager()->EnableErrorChecking(check_errors_);
  UpdateTextUniforms();
  GetRenderer()->DrawScene(root_);
}

bool IonTextDemo::InitTextInfo(TextInfo* text_info) {
  text_info->font = CreateFont(font_manager_, font_size_, sdf_padding_);
  text_info->font_image =
      CreateFontImage(text_info->font, FontImageTypeFromInt(font_image_type_));
  text_info->style = TextStyleFromInt(text_style_);
  text_info->text_strings = ParseInputStrings(string_);
  text_info->cylindrical_layout = cylindrical_layout_;
  text_info->layout_options.horizontal_alignment =
      HorizontalAlignmentFromInt(horizontal_alignment_);
  text_info->layout_options.vertical_alignment =
      VerticalAlignmentFromInt(vertical_alignment_);
  text_info->layout_options.line_spacing = line_spacing_;

  // Used a fixed target width so that multi-line text stays relatively the
  // same size.
  text_info->layout_options.target_size.Set(kTextWidth, 0.0f);

  // A Font is required.
  return text_info->font.Get();
}

void IonTextDemo::InitSettings() {
  using std::bind;
  using std::placeholders::_1;

  // Set up listeners for settings that require rebuilding.
  ion::base::SettingManager::RegisterGroupListener(
      "textdemo/font", "TextDemo", bind(&IonTextDemo::UpdateFont, this, _1));
  ion::base::SettingManager::RegisterGroupListener(
      "textdemo/layout", "TextDemo", bind(&IonTextDemo::UpdateText, this, _1));
  font_image_type_.RegisterListener(
      "TextDemo", bind(&IonTextDemo::UpdateFontImageType, this, _1));
  text_style_.RegisterListener(
      "TextDemo", bind(&IonTextDemo::UpdateTextStyle, this, _1));

  // Set up strings for enum settings so they use dropboxes.
  font_image_type_.SetTypeDescriptor("enum:Dynamic|Static");
  horizontal_alignment_.SetTypeDescriptor("enum:Left|Center|Right");
  vertical_alignment_.SetTypeDescriptor("enum:Top|Center|Baseline|Bottom");
  text_style_.SetTypeDescriptor("enum:Basic|Outlined");
}

void IonTextDemo::UpdateFont(ion::base::SettingBase*) {
  // Clamp the font size and SDF padding settings to reasonable values.
  static const size_t kMinFontSize = 2U;
  static const size_t kMaxFontSize = 128U;
  static const size_t kMinSdfPadding = 0U;  // Uses 1/8 font size.
  static const size_t kMaxSdfPadding = 32U;
  const size_t size = ClampSetting(kMinFontSize, kMaxFontSize, &font_size_);
  const size_t padding =
      ClampSetting(kMinSdfPadding, kMaxSdfPadding, &sdf_padding_);

  // If the font size or padding changed, create a new Font if necessary.
  TextInfo& text_info = *text_info_;
  if (size != text_info.font->GetSizeInPixels() ||
      padding != text_info.font->GetSdfPadding()) {
    text_info.font = font_manager_->FindFont(kFontName, size, padding);
    if (!text_info.font.Get())
      text_info.font = CreateFont(font_manager_, size, padding);
    text_info.font_image =
        CreateFontImage(text_info.font, FontImageTypeFromInt(font_image_type_));
    UpdateBuilders();
    UpdateTextNodes();
  }
}

void IonTextDemo::UpdateText(ion::base::SettingBase*) {
  TextInfo& text_info = *text_info_;
  text_info.text_strings = ParseInputStrings(string_);

  text_info.cylindrical_layout = cylindrical_layout_;
  text_info.layout_options.horizontal_alignment =
      HorizontalAlignmentFromInt(horizontal_alignment_);
  text_info.layout_options.vertical_alignment =
      VerticalAlignmentFromInt(vertical_alignment_);
  text_info.layout_options.line_spacing = line_spacing_;

  // Update the Builders if the number of strings changed.
  if (text_info.text_strings.size() != builders_.size())
    UpdateBuilders();

  UpdateTextNodes();
}

void IonTextDemo::UpdateTextUniforms() {
  const size_t num_builders = builders_.size();
  if (text_info_->style == kBasic) {
    for (size_t i = 0; i < num_builders; ++i) {
      ion::text::BasicBuilder* bb =
          static_cast<ion::text::BasicBuilder*>(builders_[i].Get());
      bb->SetSdfPadding(static_cast<float>(sdf_padding_));
      bb->SetTextColor(text_color_);
    }
  } else {
    for (size_t i = 0; i < num_builders; ++i) {
      ion::text::OutlineBuilder* ob =
          static_cast<ion::text::OutlineBuilder*>(builders_[i].Get());
      ob->SetSdfPadding(static_cast<float>(sdf_padding_));
      ob->SetTextColor(text_color_);
      ob->SetOutlineColor(outline_color_);
      ob->SetHalfSmoothWidth(0.5f * smooth_width_);
      ob->SetOutlineWidth(outline_width_);
    }
  }
}

void IonTextDemo::UpdateFontImageType(ion::base::SettingBase*) {
  text_info_->font_image =
      CreateFontImage(text_info_->font, FontImageTypeFromInt(font_image_type_));
  UpdateBuilders();
  UpdateTextNodes();
}

void IonTextDemo::UpdateTextStyle(ion::base::SettingBase*) {
  TextInfo& text_info = *text_info_;
  const TextStyle new_style = TextStyleFromInt(text_style_);
  if (text_info.style != new_style) {
    text_info.style = new_style;
    // The Builders need to be recreated.
    builders_.clear();
    UpdateBuilders();
    UpdateTextNodes();
  }
}

void IonTextDemo::UpdateBuilders() {
  TextInfo& text_info = *text_info_;
  const size_t num_builders_needed = text_info_->text_strings.size();
  const size_t num_builders_existing = builders_.size();
  builders_.resize(num_builders_needed);
  ion::text::BuilderPtr builder;
  for (size_t i = num_builders_existing; i < num_builders_needed; ++i) {
    switch (text_info.style) {
      case kBasic:
      default:
        builder.Reset(new ion::text::BasicBuilder(
            text_info.font_image, GetShaderManager(),
            ion::base::AllocatorPtr()));
        break;
      case kOutlined:
        builder.Reset(new ion::text::OutlineBuilder(
            text_info.font_image, GetShaderManager(),
            ion::base::AllocatorPtr()));
        break;
    }
    builders_[i] = builder;
  }
  // Make sure the Builder uses the current FontImage.
  for (size_t i = 0; i < num_builders_needed; ++i)
    builders_[i]->SetFontImage(text_info.font_image);
}

void IonTextDemo::UpdateTextNodes() {
  if (BuildTextNodes(builders_, BuildLayouts(*text_info_), text_root_)) {
    // Since the node changed, have to update the uniform values.
    UpdateViewUniforms();
  } else {
    LOG(ERROR) << "Unable to rebuild text graphics data";
  }
}

std::unique_ptr<DemoBase> CreateDemo(int width, int height) {
  return std::unique_ptr<DemoBase>(new IonTextDemo(width, height));
}
