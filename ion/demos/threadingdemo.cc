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

#include <list>
#include <thread>  // NOLINT(build/c++11)

#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/demos/utils.h"
#include "ion/demos/viewerdemobase.h"
#include "ion/gfxutils/shadersourcecomposer.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/math/transformutils.h"
#include "ion/port/barrier.h"
#include "ion/port/timer.h"
#include "ion/portgfx/glcontext.h"

ION_REGISTER_ASSETS(IonThreadingResources);

using ion::math::Anglef;
using ion::math::Matrix4f;
using ion::math::Point2i;
using ion::math::Point3f;
using ion::math::Range2i;
using ion::math::Vector2i;
using ion::math::Vector3f;
using ion::math::Vector4f;

namespace {

static const int kCubeMapFaces = 6;
static const int kReflectionMapResolution = 256;

#if defined(ION_PLATFORM_LINUX) || defined(ION_PLATFORM_MAC) || \
    defined(ION_PLATFORM_WINDOWS)
static const auto depth_format = ion::gfx::Image::kRenderbufferDepth24;
#else
static const auto depth_format = ion::gfx::Image::kRenderbufferDepth16;
#endif

struct ReflectionMapFaceData {
  ion::gfx::FramebufferObjectPtr fbo;
  Matrix4f view_matrix;
};

static bool ReflectionThread(const ion::portgfx::GlContextPtr& gl_context,
                             const ion::gfx::RendererPtr& renderer,
                             const ion::gfx::NodePtr& scene,
                             const ion::gfx::CubeMapTexturePtr& reflection_map,
                             Vector3f* sphere_position, bool* finished,
                             ion::port::Barrier* start_barrier,
                             ion::port::Barrier* reflection_barrier) {
  LOG(INFO) << "Spawned reflection map thread, ID: "
            << std::this_thread::get_id() << std::endl;
  if (!ion::portgfx::GlContext::MakeCurrent(gl_context)) {
    LOG(ERROR) << "Could not make GL context current" << std::endl;
    std::exit(1);
  }

  std::vector<ReflectionMapFaceData> data;

  for (int i = 0; i < kCubeMapFaces; ++i) {
    ion::gfx::CubeMapTexture::CubeFace face =
        static_cast<ion::gfx::CubeMapTexture::CubeFace>(
            ion::gfx::CubeMapTexture::kNegativeX + i);

    // Set up the view matrices used to render the cubemap faces.
    // The correct matrices for each side were determined empirically.
    Matrix4f view;
    Matrix4f rot = ion::math::RotationMatrixAxisAngleH(
        Vector3f::AxisZ(), Anglef::FromDegrees(180.f));
    switch (face) {
      case ion::gfx::CubeMapTexture::kNegativeX:
        view = rot * ion::math::RotationMatrixAxisAngleH(
            Vector3f::AxisY(), Anglef::FromDegrees(-90.f));
        break;
      case ion::gfx::CubeMapTexture::kNegativeY:
        view = ion::math::RotationMatrixAxisAngleH(
            Vector3f::AxisX(), Anglef::FromDegrees(90.f));
        break;
      case ion::gfx::CubeMapTexture::kNegativeZ:
        view = rot;
        break;
      case ion::gfx::CubeMapTexture::kPositiveX:
        view = rot * ion::math::RotationMatrixAxisAngleH(
            Vector3f::AxisY(), Anglef::FromDegrees(90.f));
        break;
      case ion::gfx::CubeMapTexture::kPositiveY:
        view = ion::math::RotationMatrixAxisAngleH(
            Vector3f::AxisX(), Anglef::FromDegrees(-90.f));
        break;
      case ion::gfx::CubeMapTexture::kPositiveZ:
        view = rot * ion::math::RotationMatrixAxisAngleH(
            Vector3f::AxisY(), Anglef::FromDegrees(180.f));
        break;
      default:
        LOG(ERROR) << "Unknown cubemap face" << std::endl;
        std::exit(1);
    }

    ReflectionMapFaceData face_data;
    face_data.view_matrix = view;
    face_data.fbo = ion::gfx::FramebufferObjectPtr(
        new ion::gfx::FramebufferObject(kReflectionMapResolution,
                                        kReflectionMapResolution));
    face_data.fbo->SetColorAttachment(0U,
        ion::gfx::FramebufferObject::Attachment(reflection_map, face, 0U));
    face_data.fbo->SetDepthAttachment(
        ion::gfx::FramebufferObject::Attachment(depth_format));
    data.emplace_back(face_data);
  }

  size_t view_matrix_index = scene->GetUniformIndex("uModelviewMatrix");
  renderer->BindFramebuffer(data[0].fbo);
  renderer->CreateOrUpdateResources(scene);
  start_barrier->Wait();

  while (true) {
    // Wait until drawing or quitting is requested.
    start_barrier->Wait();
    if (*finished) {
      ion::portgfx::GlContext::CleanupThread();
      return true;
    }

    for (int i = 0; i < kCubeMapFaces; ++i) {
      Vector3f offset = *sphere_position;
      Matrix4f current_view = data[i].view_matrix *
          ion::math::TranslationMatrix(-offset);
      scene->SetUniformValue(view_matrix_index, current_view);
      renderer->BindFramebuffer(data[i].fbo);
      renderer->DrawScene(scene);
    }

    // Wait until the cubemap is ready.
    renderer->GetGraphicsManager()->Finish();
    reflection_barrier->Wait();
  }

  return true;
}

// Builds a "temple" made out of two slabs and ten columns.
// This shape was chosen to allow nice reflections.
static ion::gfx::NodePtr BuildTemple() {
  ion::gfx::NodePtr temple(new ion::gfx::Node);

  // Build top and bottom slab.
  ion::gfxutils::BoxSpec slab_spec;
  slab_spec.size = Vector3f(2.f, 0.1f, 2.f);
  slab_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionTexCoords;
  slab_spec.translation = Point3f(0.f, -1.f, 0.f);
  temple->AddShape(ion::gfxutils::BuildBoxShape(slab_spec));
  slab_spec.translation = Point3f(0.f, 1.f, 0.f);
  temple->AddShape(ion::gfxutils::BuildBoxShape(slab_spec));

  // Build columns.
  ion::gfxutils::CylinderSpec column_spec;
  column_spec.top_radius = 0.1f;
  column_spec.bottom_radius = 0.1f;
  column_spec.height = 2.f;
  column_spec.has_top_cap = false;
  column_spec.has_bottom_cap = false;
  column_spec.sector_count = 16;
  column_spec.shaft_band_count = 1;
  column_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionTexCoords;
  column_spec.translation = Point3f::Zero();
  for (int side = 0; side < 2; ++side) {
    column_spec.translation[0] = side ? 0.8f : -0.8f;
    for (int row = -2; row <= 2; ++row) {
      column_spec.translation[2] = static_cast<float>(row) * 0.4f;
      column_spec.rotation = ion::math::RotationMatrixAxisAngleNH(
          Vector3f::AxisY(),
          Anglef::FromDegrees((360.f / 7.f * 3.f) *
                              static_cast<float>(5 * side + row + 2)));
      temple->AddShape(ion::gfxutils::BuildCylinderShape(column_spec));
    }
  }

  return temple;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// ThreadingDemo class.
//
//-----------------------------------------------------------------------------

class IonThreadingDemo : public ViewerDemoBase {
 public:
  IonThreadingDemo(int width, int height);
  ~IonThreadingDemo() override;
  void Update() override;
  void RenderFrame() override;
  void Keyboard(int key, int x, int y, bool is_press) override {}
  std::string GetDemoClassName() const override { return "ThreadingDemo"; }

 private:
  ion::gfx::NodePtr draw_root_;
  ion::gfx::NodePtr scene_;
  ion::gfx::NodePtr sphere_;
  Vector3f sphere_position_;
  size_t sphere_position_index_;
  ion::port::Barrier start_barrier_;
  ion::port::Barrier reflection_barrier_;
  bool finished_ = false;

  std::vector<std::thread> threads_;
};

IonThreadingDemo::IonThreadingDemo(int width, int height)
    : ViewerDemoBase(width, height),
      draw_root_(new ion::gfx::Node),
      scene_(new ion::gfx::Node),
      sphere_(new ion::gfx::Node),
      sphere_position_(Vector3f::Zero()),
      sphere_position_index_(0),
      start_barrier_(2U),
      reflection_barrier_(2U) {
  IonThreadingResources::RegisterAssets();
  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  ion::gfx::SamplerPtr sampler(new ion::gfx::Sampler);
  // This is required for textures on iOS. No other texture wrap mode seems to
  // be supported.
  sampler->SetWrapS(ion::gfx::Sampler::kClampToEdge);
  sampler->SetWrapT(ion::gfx::Sampler::kClampToEdge);

  ion::gfx::CubeMapTexturePtr sky_map =
      demoutils::LoadCubeMapAsset("shapes_cubemap_image", ".jpg");
  sky_map->SetSampler(sampler);

  // Set up reflection cube map.
  ion::gfx::CubeMapTexturePtr reflection_map(new ion::gfx::CubeMapTexture);
  ion::gfx::ImagePtr empty_image(new ion::gfx::Image);
  empty_image->Set(ion::gfx::Image::kRgba8, kReflectionMapResolution,
      kReflectionMapResolution, ion::base::DataContainerPtr());
  for (int i = 0; i < kCubeMapFaces; ++i) {
    reflection_map->SetImage(
        static_cast<ion::gfx::CubeMapTexture::CubeFace>(
            ion::gfx::CubeMapTexture::kNegativeX + i),
        0U, empty_image);
  }
  reflection_map->SetSampler(sampler);

  // Build a box that renders the environment cube map.
  ion::gfx::NodePtr skybox(new ion::gfx::Node);
  ion::gfxutils::BoxSpec box_spec;
  box_spec.size = Vector3f::Fill(1.f);
  box_spec.vertex_type = ion::gfxutils::ShapeSpec::kPosition;
  skybox->AddShape(ion::gfxutils::BuildBoxShape(box_spec));
  skybox->SetShaderProgram(demoutils::LoadShaderProgramAsset(
      GetShaderManager(), "Environment cube", reg, "skybox"));
  skybox->GetShaderProgram()->SetConcurrent(true);
  skybox->AddUniform(reg->Create<ion::gfx::Uniform>("uCubeMap", sky_map));

  ion::gfx::StateTablePtr cube_state(new ion::gfx::StateTable);
  cube_state->SetDepthFunction(ion::gfx::StateTable::kDepthLessOrEqual);
  cube_state->SetCullFaceMode(ion::gfx::StateTable::kCullFront);
  skybox->SetStateTable(cube_state);

  // Build a "temple" made out of 4 columns and 2 flat boxes.
  ion::gfx::NodePtr temple = BuildTemple();
  ion::gfx::TexturePtr marble_texture =
      demoutils::LoadTextureAsset("marble.jpg");
  marble_texture->SetSampler(sampler);
  temple->SetShaderProgram(demoutils::LoadShaderProgramAsset(
      GetShaderManager(), "Texture shader", reg, "texture"));
  temple->GetShaderProgram()->SetConcurrent(true);
  temple->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uTexture", marble_texture));
  temple->AddUniform(reg->Create<ion::gfx::Uniform>("uFlip", 0.f));

  // Build reflective sphere.
  ion::gfxutils::EllipsoidSpec sphere_spec;
  sphere_spec.band_count = 16;
  sphere_spec.sector_count = 32;
  sphere_spec.size = Vector3f::Fill(0.3f);
  sphere_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionNormal;
  sphere_->AddShape(ion::gfxutils::BuildEllipsoidShape(sphere_spec));
  sphere_->SetShaderProgram(demoutils::LoadShaderProgramAsset(
      GetShaderManager(), "Sphere shader", reg, "reflective_sphere"));
  sphere_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uReflectionCubeMap", reflection_map));
  sphere_position_index_ = sphere_->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uSpherePosition", Vector3f::Zero()));
  sphere_->SetLabel("Reflective sphere");

  // Set up a node representing the scene without the reflective sphere.
  ion::gfx::StateTablePtr scene_state(new ion::gfx::StateTable(width, height));
  scene_state->SetClearColor(Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  scene_state->SetClearDepthValue(2.f);
  scene_->SetStateTable(scene_state);
  scene_->SetLabel("Scene without sphere");
  scene_->AddChild(temple);
  scene_->AddChild(skybox);

  // Set up a node for the main view.
  ion::gfx::StateTablePtr draw_state(new ion::gfx::StateTable(width, height));
  draw_state->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
  draw_state->Enable(ion::gfx::StateTable::kDepthTest, true);
  draw_state->Enable(ion::gfx::StateTable::kCullFace, true);
  draw_state->SetCullFaceMode(ion::gfx::StateTable::kCullBack);
  draw_root_->SetStateTable(draw_state);
  draw_root_->SetLabel("Main view drawing node");
  draw_root_->AddChild(scene_);

  // Set up a node for reflection map views.
  // Same as above, but different viewport size.
  ion::gfx::StateTablePtr reflection_state(new ion::gfx::StateTable(
      kReflectionMapResolution, kReflectionMapResolution));
  reflection_state->CopyFrom(*draw_state);
  reflection_state->SetViewport(Range2i::BuildWithSize(Point2i(0, 0),
      Vector2i::Fill(kReflectionMapResolution)));
  ion::gfx::NodePtr reflection_root(new ion::gfx::Node);
  reflection_root->SetStateTable(reflection_state);
  reflection_root->AddChild(scene_);
  reflection_root->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uProjectionMatrix", ion::math::PerspectiveMatrixFromView(
      Anglef::FromDegrees(90.f), 1.0f, 0.1f, 10.f)));
  reflection_root->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uModelviewMatrix", Matrix4f::Identity()));

  SetTrackballRadius(2.0f);
  SetNodeWithViewUniforms(draw_root_);

  // The shared resources must be created before starting the thread.
  const ion::gfx::RendererPtr& renderer = GetRenderer();
  renderer->CreateOrUpdateResources(draw_root_);
  renderer->CreateOrUpdateResources(sphere_);

  // Ensure that the very first frame is reasonable.
  UpdateViewUniforms();

  // Create reflection map rendering thread.
  threads_.emplace_back(
      ReflectionThread,
      ion::portgfx::GlContext::CreateGlContextInCurrentShareGroup(),
      renderer, reflection_root, reflection_map, &sphere_position_, &finished_,
      &start_barrier_, &reflection_barrier_);

  InitRemoteHandlers({reflection_root, draw_root_});
  start_barrier_.Wait();
}

IonThreadingDemo::~IonThreadingDemo() {
  finished_ = true;
  start_barrier_.Wait();
  for (auto& thread : threads_) {
    thread.join();
  }
}

void IonThreadingDemo::Update() {
  static ion::port::Timer timer;
  const double delta = timer.GetInS();

  float x = static_cast<float>(std::sin(delta * 0.741) * 0.4);
  float y = static_cast<float>(std::sin(delta * 0.687) * 0.5);
  float z = static_cast<float>(std::sin(delta * 0.639) * 0.8);
  sphere_position_ = Vector3f(x, y, z);
}

void IonThreadingDemo::RenderFrame() {
  // Tell the auxiliary threads to start rendering.
  start_barrier_.Wait();

  // Render everything except the reflective sphere.
  const ion::gfx::RendererPtr& renderer = GetRenderer();
  draw_root_->ReplaceChild(0U, scene_);
  renderer->DrawScene(draw_root_);

  // Wait until cubemap threads are finished.
  reflection_barrier_.Wait();

  // Render the sphere.
  draw_root_->ReplaceChild(0U, sphere_);
  sphere_->SetUniformValue(sphere_position_index_, sphere_position_);
  renderer->DrawScene(draw_root_);
}

std::unique_ptr<DemoBase> CreateDemo(int width, int height) {
  return std::unique_ptr<DemoBase>(new IonThreadingDemo(width, height));
}
