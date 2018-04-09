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

// This is not a unit test - it uses OpenGL to render an ion scene graph in a
// window.

#include <algorithm>
#include <chrono>  // NOLINT
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ion/base/logging.h"
#include "ion/base/setting.h"
#include "ion/base/settingmanager.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/demos/hud.h"
#include "ion/demos/utils.h"
#include "ion/demos/viewerdemobase.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/image.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/node.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/gfxutils/shadersourcecomposer.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/math/matrix.h"
#include "ion/math/matrixutils.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
#include "ion/math/vector.h"
#include "ion/math/vectorutils.h"
#include "ion/port/timer.h"
#include "ion/portgfx/setswapinterval.h"

ION_REGISTER_ASSETS(IonSkinDataResources);
ION_REGISTER_ASSETS(IonSkinResources);

using ion::math::Anglef;
using ion::math::Matrix4f;
using ion::math::Point2f;
using ion::math::Point2i;
using ion::math::Point3f;
using ion::math::Range1i;
using ion::math::Range2i;
using ion::math::Vector2f;
using ion::math::Vector2i;
using ion::math::Vector3f;
using ion::math::Vector4f;

struct Vertex {
  Vector3f position;
  Vector2f tex_coords;
};

struct LightInfo {
  LightInfo(float tilt, float rot, float dist)
      : tilt_angle(Anglef::FromDegrees(tilt)),
        rotation_angle(Anglef::FromDegrees(rot)),
        distance(dist),
        last_mouse_pos(Vector2f::Zero()) {}

  Anglef field_of_view_angle;
  Anglef tilt_angle;
  Anglef rotation_angle;
  float distance;
  Vector2f last_mouse_pos;
};

static bool s_update_depth_map = true;
static const float kBaseDistance = 500.f;
static LightInfo s_light_info(-30.f, 30.f, kBaseDistance);
static int kFboSize = 2048;
static int kHudOffset = 25;

static const ion::gfx::NodePtr BuildRectangle() {
  ion::gfx::NodePtr node(new ion::gfx::Node);
  node->SetLabel("Rectangle");

  ion::gfxutils::RectangleSpec rect_spec;
  rect_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionTexCoords;
  rect_spec.plane_normal = ion::gfxutils::RectangleSpec::kNegativeY;
  rect_spec.translation.Set(0.f, -0.1f, 0.f);
  rect_spec.size.Set(20.f, 20.f);
  ion::gfx::ShapePtr shape = ion::gfxutils::BuildRectangleShape(rect_spec);
  shape->SetLabel("Rectangle");
  node->AddShape(shape);

  return node;
}

static void LightingChanged(ion::base::SettingBase* setting) {
  s_update_depth_map = true;
}

//-----------------------------------------------------------------------------
//
// SkinDemo class.
//
//-----------------------------------------------------------------------------

class IonSkinDemo : public ViewerDemoBase {
 public:
  IonSkinDemo(int width, int height);
  ~IonSkinDemo() override {}
  void Keyboard(int key, int x, int y, bool is_press) override {}
  void Resize(int width, int height) override;
  void Update() override;
  void RenderFrame() override;
  void ProcessMotion(float x, float y, bool is_press) override;
  void ProcessScale(float scale) override;
  std::string GetDemoClassName() const override { return "SkinDemo"; }

 private:
  void UpdateView();
  void UpdateDepthMap();
  void InitScreenSizedFbos(int width, int height);

  ion::gfx::NodePtr head_;
  ion::gfx::NodePtr draw_root_;
  ion::gfx::NodePtr texture_display_root_;
  ion::gfx::NodePtr depth_map_root_;
  ion::gfx::NodePtr blur_root_;
  ion::gfx::NodePtr irradiance_root_;
  ion::gfx::NodePtr clear_root_;

  // Illumination.
  ion::gfx::TexturePtr depth_map_;
  ion::gfx::TexturePtr blurred_depth_map_;
  ion::gfx::TexturePtr irradiance_map_;
  ion::gfx::FramebufferObjectPtr irradiance_fbo_;
  ion::gfx::ShaderProgramPtr accumulate_;
  ion::gfx::ShaderProgramPtr blur_horizontally_;
  ion::gfx::ShaderProgramPtr blur_vertically_;
  ion::gfx::ShaderProgramPtr blur_vertically_and_accumulate_;
  ion::gfx::FramebufferObjectPtr blur_fbo_;
  ion::gfx::FramebufferObjectPtr depth_fbo_;

  // Skin.
  ion::gfx::TexturePtr scatter_horizontal_tex_;
  ion::gfx::TexturePtr scatter_vertical_tex_;
  ion::gfx::TexturePtr accumulated_tex_;
  ion::gfx::FramebufferObjectPtr skin_vertical_fbo_;
  ion::gfx::FramebufferObjectPtr skin_horizontal_fbo_;
  ion::gfx::FramebufferObjectPtr accumulate_fbo_;

  ion::gfx::SamplerPtr sampler_;
  ion::gfx::Image::Format fbo_format_;
  ion::gfx::Image::Format depth_format_;

  // Settings.
  ion::base::Setting<bool> move_light_;
  ion::base::Setting<bool> multisample_;
  ion::base::Setting<bool> show_depth_;
  ion::base::Setting<bool> show_hud_;
  ion::base::Setting<bool> show_irrad_;
  ion::base::Setting<bool> show_trans_;
  ion::base::Setting<bool> auto_rotate_light_;
  ion::base::Setting<int> blur_passes_;
  ion::base::Setting<double> roughness_;
  ion::base::Setting<double> specular_intensity_;
  ion::base::Setting<double> translucency_;
  ion::base::Setting<double> translucency_fade_;
  ion::base::Setting<double> rim_power_;
  ion::base::Setting<double> auto_rotation_speed_;
  ion::base::Setting<double> auto_rotation_size_;
  ion::base::Setting<double> auto_rotation_tilt_;
  ion::base::Setting<std::vector<ion::math::Vector3f> > profile_weights_;
  ion::base::Setting<double> exposure_;

  float model_radius_;

  Hud hud_;
};

IonSkinDemo::IonSkinDemo(int width, int height)
    : ViewerDemoBase(width, height),
#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_MAC) || \
    defined(ION_PLATFORM_WINDOWS)
      fbo_format_(demoutils::RendererSupportsRgb16fHalf(GetGraphicsManager()) ?
                  ion::gfx::Image::kRgb16fHalf : ion::gfx::Image::kRgba8888),
      depth_format_(ion::gfx::Image::kRenderbufferDepth24),
#else
      fbo_format_(ion::gfx::Image::kRgba8888),
      depth_format_(ion::gfx::Image::kRenderbufferDepth16),
#endif
      move_light_("SkinDemo/move_light",
                  false,
                  "Move the light rather than the camera"),
      multisample_("SkinDemo/multisample", true, "Use OpenGL multisampling"),
      show_depth_("SkinDemo/show depth",
                  false,
                  "Display the depth map on-screen"),
      show_hud_("SkinDemo/show HUD", false, "Display the HUD and FPS counter"),
      show_irrad_("SkinDemo/show irrad",
                  false,
                  "Display the irradiance map on-screen"),
      show_trans_("SkinDemo/show trans",
                  false,
                  "Display the translucency map on-screen"),
      auto_rotate_light_("SkinDemo/auto rotate light",
                         false,
                         "Automatically rotate the light around the head"),
      blur_passes_("SkinDemo/blur passes", 1, "Number of depth blur passes"),
      roughness_("SkinDemo/roughness", .2, "Roughness of the skin surface"),
      specular_intensity_("SkinDemo/specular intensity",
                          .35,
                          "Strength of the specular BRDF"),
      translucency_("SkinDemo/translucency", 2., "Skin translucency"),
      translucency_fade_("SkinDemo/translucency fade",
                         0.75,
                         "Skin translucency fade"),
      rim_power_("SkinDemo/rim power", 0.4, "Rim lighting power"),
      auto_rotation_speed_("SkinDemo/auto rotation speed",
                           1.,
                           "How fast the auto rotate light moves"),
      auto_rotation_size_("SkinDemo/auto rotation size",
                          0.3,
                          "How large of a circle the light rotates in"),
      auto_rotation_tilt_("SkinDemo/auto rotation tilt (deg)",
                          -60.,
                          "How much tilt the light rotation has (degrees)"),
      profile_weights_("SkinDemo/profile weights",
                       std::vector<Vector3f>(),
                       "Profile weights"),
      exposure_("SkinDemo/exposure", 1.5f, "Exposure"),
      hud_(ion::text::FontManagerPtr(new ion::text::FontManager),
           GetShaderManager(),
           width,
           height) {
  std::vector<Vector3f> weights;
  // See http://http.developer.nvidia.com/GPUGems3/gpugems3_ch14.html
  weights.push_back(Vector3f(0.233f, 0.455f, 0.649f));
  weights.push_back(Vector3f(0.100f, 0.336f, 0.344f));
  weights.push_back(Vector3f(0.118f, 0.198f, 0.f));
  weights.push_back(Vector3f(0.113f, 0.007f, 0.007f));
  weights.push_back(Vector3f(0.358f, 0.004f, 0.f));
  // The red component here was originally 0.078, but it looks better this way.
  weights.push_back(Vector3f(0.118f, 0.f, 0.f));
  profile_weights_ = weights;

  blur_passes_.RegisterListener("blur listener", LightingChanged);
  roughness_.RegisterListener("roughness listener", LightingChanged);
  specular_intensity_.RegisterListener("specular listener", LightingChanged);
  exposure_.RegisterListener("specular listener", LightingChanged);

  // Load data assets.
  IonSkinDataResources::RegisterAssets();
  // Load shader assets.
  IonSkinResources::RegisterAssets();

  GetGraphicsManager()->EnableErrorChecking(true);

  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  ion::gfxutils::ExternalShapeSpec head_spec;
  head_spec.scale = 1000.f;
  ion::gfx::ShapePtr shape =
      demoutils::LoadShapeAsset("head.obj", head_spec, &model_radius_);
  shape->SetLabel("Head Shape");

  head_.Reset(new ion::gfx::Node);
  head_->AddShape(shape);
  head_->SetLabel("Head Node");

  // --------------------------------------------------------------------------
  // Set up viewing.
  // --------------------------------------------------------------------------
  draw_root_.Reset(new ion::gfx::Node);
  irradiance_root_.Reset(new ion::gfx::Node);
  SetTrackballRadius(model_radius_ * 2);
  SetNodeWithViewUniforms(draw_root_);
  UpdateViewUniforms();

  // --------------------------------------------------------------------------
  // Set up HUD for FPS display.
  // --------------------------------------------------------------------------
  Hud::TextRegion fps_region;
  fps_region.resize_policy = Hud::TextRegion::kFixedSize;
  fps_region.target_point.Set(0.5f, 0.02f);
  fps_region.target_size.Set(0.15f, 0.025f);
  fps_region.horizontal_alignment = ion::text::kAlignHCenter;
  fps_region.vertical_alignment = ion::text::kAlignBottom;
  hud_.InitFps(4, 2, fps_region);
  hud_.GetRootNode()->SetLabel("HUD FPS");
  hud_.EnableFps(true);
  ion::portgfx::SetSwapInterval(0);

  // --------------------------------------------------------------------------
  // Textures
  // --------------------------------------------------------------------------
  sampler_.Reset(new ion::gfx::Sampler);
  sampler_->SetMinFilter(ion::gfx::Sampler::kLinear);
  sampler_->SetMagFilter(ion::gfx::Sampler::kLinear);
  // This is required for textures on iOS. No other texture wrap mode seems to
  // be supported.
  sampler_->SetWrapS(ion::gfx::Sampler::kClampToEdge);
  sampler_->SetWrapT(ion::gfx::Sampler::kClampToEdge);

  accumulated_tex_.Reset(new ion::gfx::Texture);
  accumulated_tex_->SetLabel("accumulated texture");
  accumulated_tex_->SetSampler(sampler_);
  scatter_horizontal_tex_.Reset(new ion::gfx::Texture);
  scatter_horizontal_tex_->SetLabel("scatter 1 texture");
  scatter_horizontal_tex_->SetSampler(sampler_);
  scatter_vertical_tex_.Reset(new ion::gfx::Texture);
  scatter_vertical_tex_->SetLabel("scatter 2 texture");
  scatter_vertical_tex_->SetSampler(sampler_);
  irradiance_map_.Reset(new ion::gfx::Texture);
  irradiance_map_->SetLabel("irradiance texture");
  irradiance_map_->SetSampler(sampler_);

  ion::gfx::ImagePtr fbo_image(new ion::gfx::Image);
  fbo_image->Set(fbo_format_,
                    kFboSize,
                    kFboSize,
                    ion::base::DataContainerPtr());

  depth_map_.Reset(new ion::gfx::Texture);
  depth_map_->SetSampler(sampler_);
  depth_map_->SetLabel("shadow tex");
  depth_map_->SetImage(0U, fbo_image);

  blurred_depth_map_.Reset(new ion::gfx::Texture);
  blurred_depth_map_->SetSampler(sampler_);
  blurred_depth_map_->SetLabel("blurred tex");
  blurred_depth_map_->SetImage(0U, fbo_image);

  ion::gfx::TexturePtr diffuse = demoutils::LoadTextureAsset("diffuse.jpg");
  diffuse->SetLabel("diffuse tex");
  diffuse->SetSampler(sampler_);
  ion::gfx::TexturePtr normal = demoutils::LoadTextureAsset("normal.jpg");
  normal->SetLabel("normal tex");
  normal->SetSampler(sampler_);

  // --------------------------------------------------------------------------
  // Other state.
  // --------------------------------------------------------------------------
  const Matrix4f ortho_proj = ion::math::OrthographicMatrixFromFrustum(
      -10.f, 10.f, -10.f, 10.f, -1.f, 1.f);
  const Matrix4f ortho_view = ion::math::RotationMatrixAxisAngleH(
      Vector3f::AxisX(), Anglef::FromDegrees(90.f));
  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();

  // --------------------------------------------------------------------------
  // Draw Root
  // --------------------------------------------------------------------------
  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable(width, height));
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
  state_table->SetClearColor(Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  state_table->SetCullFaceMode(ion::gfx::StateTable::kCullBack);
  draw_root_->SetStateTable(state_table);
  draw_root_->SetShaderProgram(demoutils::LoadShaderProgramAsset(
      GetShaderManager(), "Skin shader", reg, "skin"));

  draw_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uBiasMatrix", Matrix4f::Identity()));
  draw_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uLightPos", Vector3f::AxisY()));
  draw_root_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uDepthAndRanges", Vector3f(0.f, 1.f, 1.f)));
  const Matrix4f modelview_matrix = ion::math::Inverse(GetModelviewMatrix());
  const Point3f camera_pos(
      modelview_matrix[0][3], modelview_matrix[1][3], modelview_matrix[2][3]);
  draw_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uCameraPos", camera_pos));
  draw_root_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uSkinParams",
      Vector4f(static_cast<float>(roughness_),
               static_cast<float>(specular_intensity_), 0.f, 0.f)));
  draw_root_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uInvWindowDims", Vector2f(1.f / static_cast<float>(width),
                                 1.f / static_cast<float>(height))));
  draw_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uExposure", 1.f));

  draw_root_->AddUniform(reg->Create<ion::gfx::Uniform>("uDiffuse", diffuse));
  draw_root_->AddUniform(reg->Create<ion::gfx::Uniform>("uNormalMap", normal));
  draw_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uIrradianceMap", irradiance_map_));
  draw_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uScattered", accumulated_tex_));

  draw_root_->AddChild(head_);

  // --------------------------------------------------------------------------
  // Clear Root.
  // --------------------------------------------------------------------------
  clear_root_.Reset(new ion::gfx::Node);
  clear_root_->SetLabel("Clear node");
  state_table.Reset(new ion::gfx::StateTable(width, height));
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
  state_table->SetClearColor(Vector4f::Zero());
  clear_root_->SetStateTable(state_table);

  // --------------------------------------------------------------------------
  // Irradiance Root.
  // --------------------------------------------------------------------------
  state_table.Reset(new ion::gfx::StateTable(width, height));
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
  const float rim_power = static_cast<float>(rim_power_);
  state_table->SetClearColor(Vector4f(rim_power, rim_power, rim_power, 1.));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  state_table->SetCullFaceMode(ion::gfx::StateTable::kCullBack);
  irradiance_root_->SetStateTable(state_table);
  irradiance_root_->SetShaderProgram(demoutils::LoadShaderProgramAsset(
      GetShaderManager(), "Irradiance Shader", reg, "irrad"));

  irradiance_root_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uProjectionMatrix", GetProjectionMatrix()));
  irradiance_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uModelviewMatrix", GetModelviewMatrix()));
  irradiance_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uBiasMatrix", Matrix4f::Identity()));
  irradiance_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uLightPos", Vector3f::AxisY()));
  irradiance_root_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uDepthAndRanges", Vector3f(0.f, 1.f, 1.f)));
  irradiance_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uDepthMap", depth_map_));
  irradiance_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uDiffuse", diffuse));
  irradiance_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uNormalMap", normal));
  irradiance_root_->AddChild(head_);

  // --------------------------------------------------------------------------
  // Shadow Root
  // --------------------------------------------------------------------------
  depth_map_root_.Reset(new ion::gfx::Node);
  state_table.Reset(new ion::gfx::StateTable(kFboSize, kFboSize));
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(kFboSize, kFboSize)));
  state_table->SetClearColor(Vector4f(1.f, 1.f, 1.f, 1.f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  state_table->SetCullFaceMode(ion::gfx::StateTable::kCullBack);
  depth_map_root_->SetStateTable(state_table);

  depth_map_root_->SetShaderProgram(demoutils::LoadShaderProgramAsset(
      GetShaderManager(), "Depth shader", reg, "depth"));
  depth_map_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uBiasMatrix", Matrix4f::Identity()));
  depth_map_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uLightPos", Vector3f::AxisY()));
  depth_map_root_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uDepthAndInverseRange", Vector2f(0.f, 1.f)));
  depth_map_root_->AddChild(head_);

  // --------------------------------------------------------------------------
  // Blur Root
  // --------------------------------------------------------------------------
  blur_root_ = BuildRectangle();
  blur_root_->SetLabel("Blur Root");
  state_table.Reset(new ion::gfx::StateTable(kFboSize, kFboSize));
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(kFboSize, kFboSize)));
  blur_root_->SetStateTable(state_table);

  ion::gfxutils::ShaderSourceComposerPtr blur_composer(
      new ion::gfxutils::ZipAssetComposer("blur.fp", false));
  ion::gfxutils::ShaderSourceComposerPtr blur_vertical_composer(
      new ion::gfxutils::ZipAssetComposer("blur_vertical.vp", false));
  ion::gfxutils::ShaderSourceComposerPtr blur_accum_composer(
      new ion::gfxutils::ZipAssetComposer("blur_accum.fp", false));
  blur_horizontally_ = GetShaderManager()->CreateShaderProgram(
      "Blur horizontally",
      reg,
      ion::gfxutils::ShaderSourceComposerPtr(
          new ion::gfxutils::ZipAssetComposer("blur_horizontal.vp", false)),
      blur_composer);
  blur_vertically_ = GetShaderManager()->CreateShaderProgram(
      "Blur vertically", reg, blur_vertical_composer, blur_composer);
  blur_vertically_and_accumulate_ = GetShaderManager()->CreateShaderProgram(
      "Blur vertically and accumulate", reg,
      blur_vertical_composer, blur_accum_composer);
  accumulate_ = demoutils::LoadShaderProgramAsset(
      GetShaderManager(), "Accumulate", reg, "accum");

  blur_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uTexture", depth_map_));
  blur_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uInverseSize",
                                     1.f / static_cast<float>(kFboSize)));
  blur_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uAccumWeights", Vector3f::Fill(1.f)));
  blur_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uCameraPos", camera_pos));
  blur_root_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uLastPass", accumulated_tex_));
  blur_root_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uProjectionMatrix", ortho_proj));
  blur_root_->AddUniform(
      reg->Create<ion::gfx::Uniform>("uModelviewMatrix", ortho_view));

  depth_fbo_.Reset(new ion::gfx::FramebufferObject(
      kFboSize,
      kFboSize));
  depth_fbo_->SetColorAttachment(
      0U, ion::gfx::FramebufferObject::Attachment(depth_map_));
  depth_fbo_->SetDepthAttachment(
      ion::gfx::FramebufferObject::Attachment(depth_format_));
  depth_fbo_->SetLabel("Depth FBO");

  blur_fbo_.Reset(new ion::gfx::FramebufferObject(kFboSize, kFboSize));
  blur_fbo_->SetColorAttachment(
      0U, ion::gfx::FramebufferObject::Attachment(blurred_depth_map_));
  blur_fbo_->SetLabel("Blur FBO");

  // --------------------------------------------------------------------------
  // Hud
  // --------------------------------------------------------------------------
  texture_display_root_.Reset(new ion::gfx::Node);
  texture_display_root_->SetLabel("Texture Display Root");
  const int hud_size = std::min(width >> 2, height >> 2);
  state_table.Reset(new ion::gfx::StateTable(hud_size, hud_size));
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(hud_size, hud_size)));
  texture_display_root_->SetStateTable(state_table);

  ion::gfx::NodePtr rect = BuildRectangle();
  rect->SetLabel("Rect Node");
  demoutils::AddUniformToNode(
      reg, "uTexture", depth_map_, texture_display_root_);
  demoutils::AddUniformToNode(reg, "uFlip", 0.f, texture_display_root_);
  rect->SetShaderProgram(demoutils::LoadShaderProgramAsset(
      GetShaderManager(), "Texture shader", reg, "texture"));

  demoutils::AddUniformToNode(
      global_reg, "uProjectionMatrix", ortho_proj, rect);
  demoutils::AddUniformToNode(global_reg, "uModelviewMatrix", ortho_view, rect);
  texture_display_root_->AddChild(rect);

  // --------------------------------------------------------------------------
  // Screen-sized framebuffer objects.
  // --------------------------------------------------------------------------
  InitScreenSizedFbos(width, height);

  // --------------------------------------------------------------------------
  // Remote handlers.
  // --------------------------------------------------------------------------
  std::vector<ion::gfx::NodePtr> tracked_nodes;
  tracked_nodes.push_back(draw_root_);
  tracked_nodes.push_back(blur_root_);
  tracked_nodes.push_back(depth_map_root_);
  tracked_nodes.push_back(irradiance_root_);
  tracked_nodes.push_back(hud_.GetRootNode());
  InitRemoteHandlers(tracked_nodes);
}

void IonSkinDemo::Resize(int width, int height) {
  ViewerDemoBase::Resize(width, height);

  hud_.Resize(width, height);

  const Range2i viewport =
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height));
  draw_root_->GetStateTable()->SetViewport(viewport);
  draw_root_->SetUniformByName("uInvWindowDims",
                               Vector2f(1.f / static_cast<float>(width),
                                        1.f / static_cast<float>(height)));
  irradiance_root_->GetStateTable()->SetViewport(viewport);
  clear_root_->GetStateTable()->SetViewport(viewport);
  InitScreenSizedFbos(width, height);
}

void IonSkinDemo::InitScreenSizedFbos(int width, int height) {
  ion::gfx::ImagePtr screen_sized_image(new ion::gfx::Image);
  screen_sized_image->Set(fbo_format_,
                    width,
                    height,
                    ion::base::DataContainerPtr());
  accumulated_tex_->SetImage(0U, screen_sized_image);
  scatter_horizontal_tex_->SetImage(0U, screen_sized_image);
  scatter_vertical_tex_->SetImage(0U, screen_sized_image);
  irradiance_map_->SetImage(0U, screen_sized_image);
  accumulate_fbo_.Reset(new ion::gfx::FramebufferObject(width, height));
  accumulate_fbo_->SetColorAttachment(
      0U, ion::gfx::FramebufferObject::Attachment(accumulated_tex_));
  accumulate_fbo_->SetLabel("Accumulate FBO");
  skin_horizontal_fbo_.Reset(new ion::gfx::FramebufferObject(width, height));
  skin_horizontal_fbo_->SetColorAttachment(
      0U, ion::gfx::FramebufferObject::Attachment(scatter_horizontal_tex_));
  skin_horizontal_fbo_->SetLabel("Horizontal blur FBO");
  skin_vertical_fbo_.Reset(new ion::gfx::FramebufferObject(width, height));
  skin_vertical_fbo_->SetColorAttachment(
      0U, ion::gfx::FramebufferObject::Attachment(scatter_vertical_tex_));
  skin_vertical_fbo_->SetLabel("Vertical blur FBO");
  irradiance_fbo_.Reset(new ion::gfx::FramebufferObject(width, height));
  irradiance_fbo_->SetColorAttachment(
      0U, ion::gfx::FramebufferObject::Attachment(irradiance_map_));
  irradiance_fbo_->SetDepthAttachment(
      ion::gfx::FramebufferObject::Attachment(depth_format_));
  irradiance_fbo_->SetLabel("Irradiance FBO");
}

void IonSkinDemo::Update() {
  if (show_hud_)
    hud_.Update();
  if (auto_rotate_light_) {
    static ion::port::Timer timer;
    const double delta = timer.GetInS() * auto_rotation_speed_;

    Vector3f light_pos = Normalized(
        Vector3f(static_cast<float>(sin(delta) * auto_rotation_size_),
                 static_cast<float>(cos(delta) * auto_rotation_size_), -1.f));
    light_pos =
        ion::math::RotationMatrixAxisAngleH(
            Vector3f::AxisX(),
            Anglef::FromDegrees(static_cast<float>(auto_rotation_tilt_))) *
        light_pos;

    const float theta = acosf(light_pos[1] / Length(light_pos));
    float phi = atan2f(light_pos[0], light_pos[2]);
    if (phi < 0.f)
      phi += 2.f * static_cast<float>(M_PI);

    s_light_info.rotation_angle = Anglef::FromRadians(phi);
    s_light_info.tilt_angle = Anglef::FromRadians(theta);
    s_update_depth_map = true;
  }
}

void IonSkinDemo::UpdateDepthMap() {
  Point3f light_pos(0.f, 0.f, s_light_info.distance);
  light_pos = ion::math::RotationMatrixAxisAngleH(Vector3f::AxisX(),
                                                  s_light_info.tilt_angle) *
              ion::math::RotationMatrixAxisAngleH(Vector3f::AxisY(),
                                                  s_light_info.rotation_angle) *
              light_pos;

  Vector3f light_to_model = Point3f::Zero() - light_pos;
  const float distance_to_model = ion::math::Length(light_to_model);
  light_to_model /= distance_to_model;
  const float radius = 1.01f * model_radius_;
  const float min_depth = distance_to_model - 0.99f * radius;
  const float max_depth = distance_to_model + 1.01f * radius;
  const float inv_depth_range = 1.f / (max_depth - min_depth);

  const Anglef fov =
      Anglef::FromRadians(2.f * atan2f(radius, distance_to_model));
  const Matrix4f scale = ion::math::ScaleMatrixH(Vector3f(.5f, .5f, .5f));
  const Matrix4f trans = ion::math::TranslationMatrix(Vector3f(.5f, .5f, .5f));
  const Matrix4f pmat = ion::math::PerspectiveMatrixFromView(
      fov, 1.f, min_depth, max_depth);
  const Matrix4f mmat = ion::math::LookAtMatrixFromCenter(
      light_pos, Point3f::Zero(), Vector3f::AxisY());
  // Projection matrix for light view.
  const Matrix4f proj_mat = pmat * mmat;
  // Projection matrix for looking up depth values in the light's space.
  const Matrix4f bias_mat = trans * (scale * proj_mat);

  depth_map_root_->SetUniformByName("uBiasMatrix", proj_mat);
  depth_map_root_->SetUniformByName("uLightPos", light_pos);
  depth_map_root_->SetUniformByName("uDepthAndInverseRange",
                                    Vector2f(min_depth, inv_depth_range));

  Vector3f ranges(min_depth, inv_depth_range, max_depth - min_depth);
  draw_root_->SetUniformByName("uBiasMatrix", bias_mat);
  draw_root_->SetUniformByName("uLightPos", light_pos);
  draw_root_->SetUniformByName("uDepthAndRanges", ranges);
  draw_root_->SetUniformByName("uExposure",
                               static_cast<float>(exposure_.GetValue()));

  irradiance_root_->SetUniformByName("uBiasMatrix", bias_mat);
  irradiance_root_->SetUniformByName("uLightPos", light_pos);
  irradiance_root_->SetUniformByName("uDepthAndRanges", ranges);

  // Draw the depth map.
  const ion::gfx::RendererPtr& renderer = GetRenderer();
  renderer->BindFramebuffer(depth_fbo_);
  renderer->DrawScene(depth_map_root_);

  blur_root_->GetStateTable()->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(kFboSize, kFboSize)));
  // No variance scaling for the depth blur.
  blur_root_->SetUniformByName("uInverseSize",
                               1.f / static_cast<float>(kFboSize));
  for (int i = 0; i < blur_passes_; ++i) {
    // Blur the depth map in depth space using separable convolution. First blur
    // horizontally, reading from depth_map_ and writing into blurred_depth_map_
    // via blur_fbo_.
    renderer->BindFramebuffer(blur_fbo_);
    blur_root_->SetUniformByName("uTexture", depth_map_);
    blur_root_->SetShaderProgram(blur_horizontally_);
    renderer->DrawScene(blur_root_);
    // Now blur vertically, reading from blurred_depth_map_ and writing into
    // depth_map_ via depth_fbo_.
    renderer->BindFramebuffer(depth_fbo_);
    blur_root_->SetShaderProgram(blur_vertically_);
    blur_root_->SetUniformByName("uTexture", blurred_depth_map_);
    renderer->DrawScene(blur_root_);
  }
}

void IonSkinDemo::RenderFrame() {
#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_MAC) || \
    (defined(ION_PLATFORM_WINDOWS) && !defined(ION_ANGLE))
  if (multisample_)
    GetGraphicsManager()->Enable(GL_MULTISAMPLE);
  else
    GetGraphicsManager()->Disable(GL_MULTISAMPLE);
#endif

  if (s_update_depth_map) {
    s_update_depth_map = false;
    UpdateDepthMap();
  }

  // See http://http.developer.nvidia.com/GPUGems3/gpugems3_ch14.html
  static const float kVariances[] = {0.0064f, 0.0484f, 0.187f,
                                     0.567f,  1.99f,   7.41f};

  // Clear the accumulated texture.
  const ion::gfx::RendererPtr& renderer = GetRenderer();
  renderer->BindFramebuffer(accumulate_fbo_);
  renderer->DrawScene(clear_root_);

  // Compute irradiance in screen-space from depth.
  irradiance_root_->SetUniformByName("uProjectionMatrix",
                                     GetProjectionMatrix());
  irradiance_root_->SetUniformByName("uModelviewMatrix", GetModelviewMatrix());
  const float rim_power = static_cast<float>(rim_power_);
  irradiance_root_->GetStateTable()->SetClearColor(
      Vector4f(rim_power, rim_power, rim_power, 1.));
  renderer->BindFramebuffer(irradiance_fbo_);
  renderer->DrawScene(irradiance_root_);

  // Blur translucency.
  // No variance scaling for the depth blur.
  const float inv_width = 1.f / static_cast<float>(GetViewportSize()[0]);
  const float inv_height = 1.f / static_cast<float>(GetViewportSize()[1]);
  blur_root_->GetStateTable()->SetViewport(Range2i::BuildWithSize(
      Point2i(0, 0), Vector2i(GetViewportSize()[0], GetViewportSize()[1])));

  const Matrix4f modelview_matrix = ion::math::Inverse(GetModelviewMatrix());
  const Vector3f camera_pos(
      modelview_matrix[0][3], modelview_matrix[1][3], modelview_matrix[2][3]);
  const float camera_dist = ion::math::Length(camera_pos);
  const float dist_scale =
      static_cast<float>(pow(translucency_ * log(camera_dist),
                             translucency_fade_));
  for (int i = 0; i < 6; ++i) {
    // Blur the irradiance map in screen space using separable convolution.
    // First blur horizontally, reading from irradiance_map_ and writing into
    // scatter_horizontal_tex_ via skin_horizontal_fbo_.
    renderer->BindFramebuffer(skin_horizontal_fbo_);
    blur_root_->SetShaderProgram(blur_horizontally_);
    blur_root_->SetUniformByName("uTexture", irradiance_map_);
    blur_root_->SetUniformByName("uInverseSize",
                                 dist_scale * inv_width * kVariances[i]);
    renderer->DrawScene(blur_root_);
    // Now blur vertically, reading from scatter_horizontal_tex_ and
    // accumulated_tex_ and writing into scatter_vertical_tex_ via
    // skin_vertical_fbo_.
    renderer->BindFramebuffer(skin_vertical_fbo_);
    blur_root_->SetShaderProgram(blur_vertically_and_accumulate_);
    blur_root_->SetUniformByName("uTexture", scatter_horizontal_tex_);
    blur_root_->SetUniformByName("uInverseSize",
                                 dist_scale * inv_height * kVariances[i]);
    blur_root_->SetUniformByName("uAccumWeights",
                                 profile_weights_.GetValue()[i]);
    renderer->DrawScene(blur_root_);
    // Copy the accumulated result so that it can be read in the next pass.
    renderer->BindFramebuffer(accumulate_fbo_);
    blur_root_->SetShaderProgram(accumulate_);
    blur_root_->SetUniformByName("uTexture", scatter_vertical_tex_);
    renderer->DrawScene(blur_root_);
  }
  // Unbind framebuffer and draw main scene.
  renderer->BindFramebuffer(ion::gfx::FramebufferObjectPtr());
  draw_root_->SetUniformByName("uSkinParams",
      Vector4f(static_cast<float>(roughness_),
               static_cast<float>(specular_intensity_), 0.f, 0.f));
  renderer->DrawScene(draw_root_);

  // Show textures around the window if requested.
  const int hud_size =
      std::min(GetViewportSize()[0] >> 2, GetViewportSize()[1] >> 2);
  if (show_irrad_) {
    texture_display_root_->SetUniformByName("uTexture", irradiance_map_);
    texture_display_root_->SetUniformByName("uFlip", 1.f);
    texture_display_root_->GetStateTable()->SetViewport(
        Range2i::BuildWithSize(Point2i(kHudOffset, kHudOffset),
                               Vector2i(hud_size, hud_size)));
    renderer->DrawScene(texture_display_root_);
  }

  if (show_trans_) {
    texture_display_root_->SetUniformByName("uTexture", accumulated_tex_);
    texture_display_root_->SetUniformByName("uFlip", 0.f);
    texture_display_root_->GetStateTable()->SetViewport(Range2i::BuildWithSize(
        Point2i(GetViewportSize()[0] - hud_size - kHudOffset, kHudOffset),
        Vector2i(hud_size, hud_size)));
    renderer->DrawScene(texture_display_root_);
  }

  if (show_depth_) {
    texture_display_root_->SetUniformByName("uTexture", depth_map_);
    texture_display_root_->SetUniformByName("uFlip", 1.f);
    texture_display_root_->GetStateTable()->SetViewport(Range2i::BuildWithSize(
        Point2i(kHudOffset, GetViewportSize()[1] - hud_size - kHudOffset),
        Vector2i(hud_size, hud_size)));
    renderer->DrawScene(texture_display_root_);
  }

  if (show_hud_)
    renderer->DrawScene(hud_.GetRootNode());
}

void IonSkinDemo::ProcessMotion(float x, float y, bool is_press) {
  if (move_light_) {
    const Vector2f new_pos(x, y);
    if (!is_press) {
      static const Anglef kRotationAngle = Anglef::FromDegrees(.25f);
      const Vector2f delta = new_pos - s_light_info.last_mouse_pos;
      s_light_info.rotation_angle += delta[0] * kRotationAngle;
      s_light_info.rotation_angle =
          ion::math::Clamp(s_light_info.rotation_angle,
                           Anglef::FromRadians(static_cast<float>(-M_PI)),
                           Anglef::FromRadians(static_cast<float>(M_PI)));
      s_light_info.tilt_angle += delta[1] * kRotationAngle;
      s_light_info.tilt_angle =
          ion::math::Clamp(s_light_info.tilt_angle,
                           Anglef::FromRadians(static_cast<float>(-M_PI_2)),
                           Anglef::FromRadians(static_cast<float>(M_PI_2)));
      s_update_depth_map = true;
    }
    s_light_info.last_mouse_pos = new_pos;
  } else {
    ViewerDemoBase::ProcessMotion(x, y, is_press);
    const Matrix4f modelview_matrix = ion::math::Inverse(GetModelviewMatrix());
    const Point3f camera_pos(
        modelview_matrix[0][3], modelview_matrix[1][3], modelview_matrix[2][3]);
    blur_root_->SetUniformByName("uCameraPos", camera_pos);
    draw_root_->SetUniformByName("uCameraPos", camera_pos);
  }
}

void IonSkinDemo::ProcessScale(float scale) {
  if (move_light_) {
    s_light_info.distance = kBaseDistance * scale;
    s_update_depth_map = true;
  } else {
    ViewerDemoBase::ProcessScale(scale);
  }
}

std::unique_ptr<DemoBase> CreateDemo(int width, int height) {
  return std::unique_ptr<DemoBase>(new IonSkinDemo(width, height));
}
