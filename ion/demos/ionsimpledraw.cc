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

#include "ion/demos/demobase.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/math/matrix.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"

using ion::math::Point2i;
using ion::math::Point3f;
using ion::math::Range2i;
using ion::math::Vector2i;
using ion::math::Vector4f;
using ion::math::Matrix4f;

namespace {

//-----------------------------------------------------------------------------
//
// Scene graph construction.
//
//-----------------------------------------------------------------------------

static const ion::gfx::NodePtr BuildRectangle(const Point3f& center,
                                              const Vector4f& color) {
  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();

  ion::gfx::NodePtr node(new ion::gfx::Node);
  node->AddUniform(global_reg->Create<ion::gfx::Uniform>("uBaseColor", color));

  ion::gfxutils::RectangleSpec rect_spec;
  rect_spec.vertex_type = ion::gfxutils::ShapeSpec::kPosition;
  rect_spec.translation = center;
  node->AddShape(ion::gfxutils::BuildRectangleShape(rect_spec));
  return node;
}

static const ion::gfx::NodePtr BuildGraph(int width, int height) {
  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();

  const Matrix4f proj(1.515f, 0.0f, 0.0f, 0.0f,
                      0.0f, 1.732f, 0.0f, 0.0f,
                      0.0f, 0.0f, -1.905f, -13.798f,
                      0.0f, 0.0f, -1.0f, 0.0f);
  const Matrix4f view(1.0f, 0.0f, 0.0f, 0.0f,
                      0.0f, 1.0f, 0.0f, 0.0f,
                      0.0f, 0.0f, 1.0f, -5.0f,
                      0.0f, 0.0f, 0.0f, 1.0f);

  ion::gfx::NodePtr root(new ion::gfx::Node);
  root->AddUniform(global_reg->Create<ion::gfx::Uniform>("uProjectionMatrix",
                                                         proj));
  root->AddUniform(global_reg->Create<ion::gfx::Uniform>("uModelviewMatrix",
                                                         view));
  // Set up global state.
  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable(width, height));
  state_table->SetViewport(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
  state_table->SetClearColor(Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  root->SetStateTable(state_table);

  // Yellow rect on left.
  root->AddChild(BuildRectangle(Point3f(-0.6f, 0.0f, 0.0f),
                                Vector4f(0.8f, 0.8f, 0.2f, 1.0f)));

  // Red rect on right.
  root->AddChild(BuildRectangle(Point3f(0.6f, 0.0f, 0.0f),
                                Vector4f(0.9f, 0.3f, 0.2f, 1.0f)));

  return root;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// IonSimpleDraw class.
//
//-----------------------------------------------------------------------------

class IonSimpleDraw : public DemoBase {
 public:
  IonSimpleDraw(int width, int height)
      : width_(width),
        height_(height) {
    graphics_manager_.Reset(new ion::gfx::GraphicsManager);
    renderer_.Reset(new ion::gfx::Renderer(graphics_manager_));
    root_ = BuildGraph(width, height);
  }
  ~IonSimpleDraw() override {}

  void Resize(int width, int height) override {
    DCHECK(root_->GetStateTable().Get());
    root_->GetStateTable()->SetViewport(
        Range2i::BuildWithSize(Point2i(0, 0), Vector2i(width, height)));
    width_ = width;
    height_ = height;
  }
  void Render() override {
    renderer_->DrawScene(root_);
  }
  void Update() override {}
  void Keyboard(int key, int x, int y, bool is_press) override {}
  void ProcessMotion(float x, float y, bool is_press) override {}
  void ProcessScale(float scale) override {}
  std::string GetDemoClassName() const override { return "IonSimpleDraw"; }

 private:
  ion::gfx::GraphicsManagerPtr graphics_manager_;
  ion::gfx::RendererPtr renderer_;
  ion::gfx::NodePtr root_;
  ion::gfx::NodePtr plane_particles_;
  int width_;
  int height_;
};

std::unique_ptr<DemoBase> CreateDemo(int width, int height) {
  return std::unique_ptr<DemoBase>(new IonSimpleDraw(width, height));
}
