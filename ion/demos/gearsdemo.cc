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
#include "ion/base/stringutils.h"
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
#include "ion/gfxutils/shadersourcecomposer.h"
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

using ion::math::Point2i;
using ion::math::Range2i;
using ion::math::Vector2i;
using ion::math::Vector3f;
using ion::math::Vector4f;

// Resources for the demo.
ION_REGISTER_ASSETS(IonGearsResources);

namespace {

// Per-instance information for gears.
struct GearInfo {
  Vector3f position;  // Translation of the gear from origin.
  float rotation;     // Angle of rotation about the Y axis.
};

}  // end anonymous namespace

//-----------------------------------------------------------------------------
//
// GearsDemo class.
//
//-----------------------------------------------------------------------------

class IonGearsDemo : public ViewerDemoBase {
 public:
  IonGearsDemo(int width, int height);
  ~IonGearsDemo() override {}
  void Resize(int width, int height) override;
  void Update() override {}
  void RenderFrame() override;
  void Keyboard(int key, int x, int y, bool is_press) override {}
  std::string GetDemoClassName() const override { return "GearsDemo"; }

 private:
  void UpdateGearUniforms(uint64 frame_count);

  ion::gfx::NodePtr root_;
  ion::gfx::NodePtr gear_;
  ion::gfx::ShapePtr gear_shape_;

  std::vector<GearInfo> gear_infos_;
  ion::gfx::BufferObjectPtr gear_info_buffer_;
  size_t gear_count_index_;

  ion::base::Setting<bool> check_errors_;
  ion::base::Setting<int> gear_rows_;
  ion::base::Setting<int> gear_columns_;
};

IonGearsDemo::IonGearsDemo(int width, int height)
    : ViewerDemoBase(width, height),
      root_(new ion::gfx::Node),
      gear_(new ion::gfx::Node),
      gear_info_buffer_(new ion::gfx::BufferObject),
      check_errors_("gearsdemo/check_errors", false,
                    "Enable OpenGL error checking"),
      gear_rows_("gearsdemo/gear_rows", 4, "Number of gear rows"),
      gear_columns_("gearsdemo/gear_columns", 4, "Number of gear columns") {
  if (!IonGearsResources::RegisterAssets()) {
    LOG(FATAL) << "Could not register demo assets";
  }
  if (!GetGraphicsManager()->IsFeatureAvailable(
          ion::gfx::GraphicsManager::kInstancedArrays)) {
    LOG(FATAL) << "IonGearsDemo requires instanced drawing functions, "
               << "but the OpenGL implementation does not support them";
  }

  // Set up global state.
  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable(width, height));
  state_table->SetViewport(Range2i::BuildWithSize(Point2i(0, 0),
                                                  Vector2i(width, height)));
  state_table->SetClearColor(Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  root_->SetStateTable(state_table);

  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  // Gears
  ion::gfxutils::ExternalShapeSpec gear_spec;
  gear_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionNormal;
  gear_shape_ = demoutils::LoadShapeAsset("gear.obj", gear_spec);

  const ion::gfx::GraphicsManagerPtr& gm = GetGraphicsManager();
  ion::gfxutils::FilterComposer::StringFilter vertex_program_filter =
      std::bind(&RewriteShader, std::placeholders::_1, gm->GetGlFlavor(),
                gm->GetGlVersion(), false);
  ion::gfxutils::FilterComposer::StringFilter fragment_program_filter =
      std::bind(&RewriteShader, std::placeholders::_1, gm->GetGlFlavor(),
                gm->GetGlVersion(), true);
  gear_->AddShape(gear_shape_);
  gear_->SetLabel("Gear Shape");
  gear_->SetShaderProgram(GetShaderManager()->CreateShaderProgram(
      "Instanced gears", reg,
      ion::gfxutils::ShaderSourceComposerPtr(
          new ion::gfxutils::FilterComposer(
              ion::gfxutils::ShaderSourceComposerPtr(
                  new ion::gfxutils::ZipAssetComposer("gears.vp", false)),
              vertex_program_filter)),
      ion::gfxutils::ShaderSourceComposerPtr(
          new ion::gfxutils::FilterComposer(
              ion::gfxutils::ShaderSourceComposerPtr(
                  new ion::gfxutils::ZipAssetComposer("gears.fp", false)),
              fragment_program_filter))));

  // gear_info_buffer_ will be filled with GearInfo structures. To bind their
  // fields to attributes in the shader, BufferToAttributeBinder uses the dummy
  // structure below to simplify specifying instance offsets. After specifying
  // the binding, we apply it to a buffer object. Note that the buffer doesn't
  // need to be filled with valid data at the time of binding. We can fill it
  // later, as long as it contains valid data when the draw function is called.
  GearInfo gi;
  ion::gfxutils::BufferToAttributeBinder<GearInfo>(gi)
      .Bind(gi.position, "aInstancePosition", 1)
      .Bind(gi.rotation, "aInstanceRotation", 1)
      .Apply(reg, gear_shape_->GetAttributeArray(), gear_info_buffer_);

  gear_count_index_ = gear_->AddUniform(reg->Create<ion::gfx::Uniform, uint32>(
      "uGearCount", gear_rows_ * gear_columns_));

  root_->AddChild(gear_);

  check_errors_.RegisterListener("check errors listener",
                                 [this](ion::base::SettingBase*) {
    GetGraphicsManager()->EnableErrorChecking(check_errors_);
  });

  UpdateGearUniforms(0);

  // Set up viewing.
  SetTrackballRadius(6.0f);
  SetNodeWithViewUniforms(root_);

  InitRemoteHandlers(std::vector<ion::gfx::NodePtr>(1, root_));

  // Initialize the uniforms and matrices in the graph.
  UpdateViewUniforms();
}

void IonGearsDemo::Resize(int width, int height) {
  ViewerDemoBase::Resize(width, height);

  DCHECK(root_->GetStateTable().Get());
  root_->GetStateTable()->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
}

void IonGearsDemo::RenderFrame() {
  UpdateGearUniforms(GetFrame()->GetCounter());
  GetRenderer()->DrawScene(root_);
}

// Set the uniforms that specify the placement of gear instances.
void IonGearsDemo::UpdateGearUniforms(uint64 frame_count) {
  const int gear_count = gear_rows_ * gear_columns_;
  gear_infos_.resize(gear_count);

  const uint64 frames_per_rev = 240;
  const float angle = static_cast<float>(frame_count % frames_per_rev)
      / frames_per_rev * static_cast<float>(2*M_PI);
  const float spacing = 0.9f;

  // Offsets for zeroth gear in units of spacing.
  const float column_offset = static_cast<float>(gear_columns_ - 1) / 2.f;
  const float row_offset = static_cast<float>(gear_rows_ - 1) / 2.f;

  gear_shape_->SetInstanceCount(gear_count);
  gear_->SetUniformValue<uint32>(gear_count_index_, gear_count);

  // Lay out the gears in a square grid.
  for (int i = 0; i < gear_columns_; ++i) {
    for (int j = 0; j < gear_rows_; ++j) {
      size_t index = gear_rows_ * i + j;
      gear_infos_[index].position =
          Vector3f(spacing * (-row_offset + static_cast<float>(j)),
                   0.f,
                   spacing * (-column_offset + static_cast<float>(i)));

      float rotation = 0;
      if (i % 2 == j % 2) {
        rotation = angle;
      } else {
        rotation = -angle + static_cast<float>(M_PI/12);
      }
      gear_infos_[index].rotation = rotation;
    }
  }

  gear_info_buffer_->SetData(ion::base::DataContainer::Create(
      gear_infos_.data(), nullptr, false, ion::base::AllocatorPtr()),
      sizeof(GearInfo), gear_count, ion::gfx::BufferObject::kDynamicDraw);
}

std::unique_ptr<DemoBase> CreateDemo(int width, int height) {
  return std::unique_ptr<DemoBase>(new IonGearsDemo(width, height));
}
