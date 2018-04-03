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

#include <memory>
#include <string>
#include <vector>

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
#include "ion/gfx/renderer.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/gfxutils/frame.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/gfxutils/shadersourcecomposer.h"
#include "ion/math/matrix.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
#include "ion/math/vector.h"

ION_REGISTER_ASSETS(IonPhysicsShaders);

const float TIMESTEP = 0.1f;

using ion::math::Point3f;
using ion::math::Point2i;
using ion::math::Range2i;
using ion::math::Vector2i;
using ion::math::Vector3f;
using ion::math::Vector4f;
using ion::math::Matrix4f;

// Particle colors: the official Google colors.
static const Vector3f google_colors[] = {
    Vector3f(51.f / 255.f, 105.f / 255.f, 232.f / 255.f),  // Blue
    Vector3f(213.f / 255.f, 15.f / 255.f, 37.f / 255.f),   // Red
    Vector3f(238.f / 255.f, 178.f / 255.f, 17.f / 255.f),  // Yellow
    Vector3f(0.f / 255.f, 153.f / 255.f, 37.f / 255.f),    // Green
};

// 
// portions of the shader text, depending on the platform.  This is really
// awkward and should probably be a feature in Ion.
static ion::gfxutils::ShaderSourceComposerPtr CreateShaderSourceComposer(
    const std::string& name, ion::gfx::GraphicsManager::GlFlavor gl_flavor,
    GLuint gl_version, bool is_fragment_shader) {
  ion::gfxutils::FilterComposer::StringFilter filter =
      std::bind(&RewriteShader, std::placeholders::_1, gl_flavor, gl_version,
                is_fragment_shader);
  return ion::gfxutils::ShaderSourceComposerPtr(
      new ion::gfxutils::FilterComposer(
          ion::gfxutils::ShaderSourceComposerPtr(
              new ion::gfxutils::ZipAssetComposer(name.c_str(), false)),
          filter));
}

//-----------------------------------------------------------------------------
//
// Shape construction.
//
//-----------------------------------------------------------------------------

static void InitializePositionsBuffer(const ion::gfx::BufferObjectPtr& buffer,
                                      size_t particle_count) {
  // Generate starting positions along a diagonal line, with all particles
  // initially moving eastward.
  Vector4f* particles = new Vector4f[particle_count];
  const float velocity_x = 0.01f;
  const float step_x = 20.0f / static_cast<float>(particle_count);
  float x = -10.0f;
  for (size_t i = 0; i < particle_count; ++i) {
    // Each 4-tuple is (previous X, previous Y, current X, current Y).
    particles[i] = Vector4f(x, x, x + velocity_x, x);
    x += step_x;
  }
  ion::base::DataContainerPtr container =
      ion::base::DataContainer::Create<Vector4f>(
          particles, ion::base::DataContainer::ArrayDeleter<Vector4f>,
          true, buffer->GetAllocator());
  buffer->SetData(container, sizeof(particles[0]), particle_count,
                  ion::gfx::BufferObject::kStreamDraw);
}

static void InitializePropertiesBuffer(const ion::gfx::BufferObjectPtr& buffer,
                                       size_t particle_count) {
  Vector4f* properties = new Vector4f[particle_count];
  size_t particles_per_color = particle_count / 4;
  for (size_t i = 0; i < particle_count; ++i) {
    // Each 4-tuple is (R, G, B, GROUP_INDEX)
    size_t group_index = i / particles_per_color;
    float w = static_cast<float>(group_index);
    properties[i] = Vector4f(google_colors[group_index], w);
  }
  ion::base::DataContainerPtr container =
      ion::base::DataContainer::Create<Vector4f>(
          properties, ion::base::DataContainer::ArrayDeleter<Vector4f>,
          true, buffer->GetAllocator());
  buffer->SetData(container, sizeof(properties[0]), particle_count,
                  ion::gfx::BufferObject::kStaticDraw);
}

//-----------------------------------------------------------------------------
//
// Demo app class declaration.
//
//-----------------------------------------------------------------------------

class IonPhysicsDemo : public ViewerDemoBase {
 public:
  IonPhysicsDemo(int width, int height);
  void Resize(int width, int height) override;
  void Update() override;
  void RenderFrame() override;
  void Keyboard(int key, int x, int y, bool is_press) override {}
  std::string GetDemoClassName() const override { return "Physics"; }

 private:
  void BuildGraph(int width, int height);
  void BuildVerletRoot();
  void BuildRenderRoot();
  ion::gfx::NodePtr render_root_;
  ion::gfx::NodePtr verlet_root_;
  ion::gfx::BufferObjectPtr source_buffer_;
  ion::gfx::BufferObjectPtr capture_buffer_;
  ion::gfx::BufferObjectPtr properties_buffer_;
  ion::gfx::AttributeArrayPtr verlet_arrays_[2];
  ion::gfx::AttributeArrayPtr render_arrays_[2];
  ion::gfx::TransformFeedbackPtr transform_feedback_;
  ion::base::Setting<size_t> particle_count_;
};

//-----------------------------------------------------------------------------
//
// Scene graph construction.
//
//-----------------------------------------------------------------------------

void IonPhysicsDemo::BuildGraph(int width, int height) {
  // Set up the simulation node.
  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable(width, height));
  verlet_root_.Reset(new ion::gfx::Node);
  verlet_root_->SetLabel("Verlet Root");
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
  state_table->Enable(ion::gfx::StateTable::kCullFace, false);
  state_table->Enable(ion::gfx::StateTable::kRasterizerDiscard, true);
  verlet_root_->SetStateTable(state_table);
  BuildVerletRoot();

  // Set up the rendering node.
  state_table.Reset(new ion::gfx::StateTable(width, height));
  render_root_.Reset(new ion::gfx::Node);
  render_root_->SetLabel("Render Root");
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
  state_table->SetClearColor(Vector4f(0.0f, 0.0f, 0.0f, 1.0f));
  state_table->Enable(ion::gfx::StateTable::kRasterizerDiscard, false);
  state_table->Enable(ion::gfx::StateTable::kCullFace, false);
  state_table->Enable(ion::gfx::StateTable::kBlend, true);
  state_table->SetBlendFunctions(
      ion::gfx::StateTable::kOne, ion::gfx::StateTable::kOneMinusSrcColor,
      ion::gfx::StateTable::kOne, ion::gfx::StateTable::kOneMinusSrcAlpha);
  render_root_->SetStateTable(state_table);
  BuildRenderRoot();

  // Use the base class to update uProjectionMatrix and uModelviewMatrix.
  SetNodeWithViewUniforms(render_root_);
}

void IonPhysicsDemo::BuildVerletRoot() {
  const ion::gfx::NodePtr& node = verlet_root_;
  const auto& shader_manager = GetShaderManager();
  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uDeltasqr", ion::gfx::kFloatUniform, "TimeStep * TimeStep"));
  demoutils::AddUniformToNode(reg, "uDeltasqr", TIMESTEP * TIMESTEP, node);

  Vector4f p;
  for (int i = 0; i < 2; i++) {
    ion::gfx::AttributeArrayPtr attribute_array(new ion::gfx::AttributeArray);
    ion::gfxutils::BufferToAttributeBinder<Vector4f>(p)
        .Bind(p, "aPositions")
        .Apply(reg, attribute_array, i ? capture_buffer_ : source_buffer_);
    ion::gfxutils::BufferToAttributeBinder<Vector4f>(p)
        .Bind(p, "aProperties")
        .Apply(reg, attribute_array, properties_buffer_);
    verlet_arrays_[i] = attribute_array;
  }

  auto& gm = GetGraphicsManager();
  ion::gfx::GraphicsManager::GlFlavor gl_flavor = gm->GetGlFlavor();
  unsigned int gl_version = gm->GetGlVersion();
  ion::gfx::ShaderProgramPtr shader = shader_manager->CreateShaderProgram(
      "verlet", reg,
      CreateShaderSourceComposer("verlet.vp", gl_flavor, gl_version, false),
      CreateShaderSourceComposer("verlet.fp", gl_flavor, gl_version, true));
  shader->SetCapturedVaryings({"vPositions"});
  node->SetShaderProgram(shader);

  ion::gfx::ShapePtr shape(new ion::gfx::Shape);
  shape->SetLabel("Verlet");
  shape->SetPrimitiveType(ion::gfx::Shape::kPoints);
  shape->SetAttributeArray(verlet_arrays_[0]);
  node->AddShape(shape);
}

void IonPhysicsDemo::BuildRenderRoot() {
  const ion::gfx::NodePtr& node = render_root_;
  const auto& shader_manager = GetShaderManager();
  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  Vector4f p;
  for (int i = 0; i < 2; i++) {
    ion::gfx::AttributeArrayPtr attribute_array(new ion::gfx::AttributeArray);
    ion::gfxutils::BufferToAttributeBinder<Vector4f>(p)
        .Bind(p, "aPositions")
        .Apply(reg, attribute_array, i ? source_buffer_ : capture_buffer_);
    ion::gfxutils::BufferToAttributeBinder<Vector4f>(p)
        .Bind(p, "aProperties")
        .Apply(reg, attribute_array, properties_buffer_);
    render_arrays_[i] = attribute_array;
  }

  auto& gm = GetGraphicsManager();
  ion::gfx::GraphicsManager::GlFlavor gl_flavor = gm->GetGlFlavor();
  unsigned int gl_version = gm->GetGlVersion();
  ion::gfx::ShaderProgramPtr shader = shader_manager->CreateShaderProgram(
      "draw", reg,
      CreateShaderSourceComposer("draw.vp", gl_flavor, gl_version, false),
      CreateShaderSourceComposer("draw.fp", gl_flavor, gl_version, true));
  node->SetShaderProgram(shader);

  ion::gfx::ShapePtr shape(new ion::gfx::Shape);
  shape->SetLabel("Render");
  shape->SetPrimitiveType(ion::gfx::Shape::kPoints);
  shape->SetAttributeArray(render_arrays_[0]);
  node->AddShape(shape);
}

//-----------------------------------------------------------------------------
//
// Demo app class implementation.
//
//-----------------------------------------------------------------------------

IonPhysicsDemo::IonPhysicsDemo(int width, int height)
    : ViewerDemoBase(width, height),
      source_buffer_(new ion::gfx::BufferObject),
      capture_buffer_(new ion::gfx::BufferObject),
      properties_buffer_(new ion::gfx::BufferObject),
      particle_count_("particles/particle count", 5000U, "Particle Count") {
  // Create the ping-pong dynamic vertex buffers.
  InitializePositionsBuffer(source_buffer_, particle_count_);
  InitializePositionsBuffer(capture_buffer_, particle_count_);
  // Create the static properties buffer.
  InitializePropertiesBuffer(properties_buffer_, particle_count_);
  // Load shader assets.
  IonPhysicsShaders::RegisterAssets();
  BuildGraph(width, height);
  SetTrackballRadius(15.0f);
  // Update uProjectionMatrix and uModelviewMatrix
  UpdateViewUniforms();
  transform_feedback_ = new ion::gfx::TransformFeedback(capture_buffer_);
  // Set up the remote handlers.
  InitRemoteHandlers({render_root_, verlet_root_});
}

void IonPhysicsDemo::Resize(int width, int height) {
  ViewerDemoBase::Resize(width, height);
  render_root_->GetStateTable()->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
}

void IonPhysicsDemo::Update() {
  // The simulation is 2D, so disallow tilt / spin.  In a way this defeats the
  // purpose of descending from ViewerDemoBase, but it handles resize / zoom,
  // and allows us to improve the demo in the future.
  SetTiltAngle(ion::math::Anglef());
  SetRotationAngle(ion::math::Anglef());
}

void IonPhysicsDemo::RenderFrame() {
  const ion::gfx::RendererPtr& renderer = GetRenderer();
  // Perform physics simulation with the vertex shader.
  // Note that we could have done physics & rendering in a single pass, but
  // using a physics-only pass lets us test RASTERIZER_DISCARD.
  renderer->BeginTransformFeedback(transform_feedback_);
  renderer->DrawScene(verlet_root_);
  renderer->EndTransformFeedback();
  // Render point sprites.
  renderer->DrawScene(render_root_);
  // Swap the buffer used for transform feedback.
  std::swap(capture_buffer_, source_buffer_);
  transform_feedback_->SetCaptureBuffer(capture_buffer_);
  // Swap the buffers used for draw calls.
  std::swap(verlet_arrays_[0], verlet_arrays_[1]);
  std::swap(render_arrays_[0], render_arrays_[1]);
  verlet_root_->GetShapes()[0]->SetAttributeArray(verlet_arrays_[0]);
  render_root_->GetShapes()[0]->SetAttributeArray(render_arrays_[0]);
}

std::unique_ptr<DemoBase> CreateDemo(int width, int height) {
  return std::unique_ptr<DemoBase>(new IonPhysicsDemo(width, height));
}
