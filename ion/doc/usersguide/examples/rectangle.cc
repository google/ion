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

#include <memory>

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
#include "absl/memory/memory.h"

// This has to be included last or bad things happen on Windows.
#include "GL/freeglut.h"

namespace {

//-----------------------------------------------------------------------------
//
// Global state to make this program easier.
//
//-----------------------------------------------------------------------------

struct GlobalState {
  int window_width;
  int window_height;
  ion::gfx::NodePtr scene_root;
  ion::gfx::RendererPtr renderer;
};

static std::unique_ptr<GlobalState> s_global_state;

//-----------------------------------------------------------------------------
//
// Scene graph construction.
//
//-----------------------------------------------------------------------------

static const ion::gfx::NodePtr BuildGraph(int window_width, int window_height) {
  ion::gfx::NodePtr root(new ion::gfx::Node);

  ion::gfxutils::RectangleSpec rect_spec;
  rect_spec.vertex_type = ion::gfxutils::ShapeSpec::kPosition;
  rect_spec.size.Set(2.f, 2.f);
  root->AddShape(ion::gfxutils::BuildRectangleShape(rect_spec));

  ion::gfx::StateTablePtr state_table(
      new ion::gfx::StateTable(window_width, window_height));
  state_table->SetViewport(
      ion::math::Range2i::BuildWithSize(ion::math::Point2i(0, 0),
                                        ion::math::Vector2i(window_width,
                                                            window_height)));
  state_table->SetClearColor(ion::math::Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  root->SetStateTable(state_table);

  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();
  const ion::math::Matrix4f proj(1.732f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.732f, 0.0f, 0.0f,
                                 0.0f, 0.0f, -1.905f, -13.798f,
                                 0.0f, 0.0f, -1.0f, 0.0f);
  const ion::math::Matrix4f view(1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, -5.0f,
                                 0.0f, 0.0f, 0.0f, 1.0f);
  root->AddUniform(global_reg->Create<ion::gfx::Uniform>(
      "uProjectionMatrix", proj));
  root->AddUniform(global_reg->Create<ion::gfx::Uniform>(
      "uModelviewMatrix", view));
  root->AddUniform(global_reg->Create<ion::gfx::Uniform>(
      "uBaseColor", ion::math::Vector4f(1.f, 1.f, 0.f, 1.f)));

  return root;
}

//-----------------------------------------------------------------------------
//
// FreeGLUT callback functions.
//
//-----------------------------------------------------------------------------

static void Resize(int w, int h) {
  s_global_state->window_width = w;
  s_global_state->window_height = h;
  glutPostRedisplay();
}

static void Render() {
  if (s_global_state)
    s_global_state->renderer->DrawScene(s_global_state->scene_root);
  glutSwapBuffers();
}

static void Update() {
  glutPostRedisplay();
}

static void Keyboard(unsigned char key, int x, int y) {
  glutPostRedisplay();
}

static void KeyboardUp(unsigned char key, int x, int y) {
  switch (key) {
    case 27:  // Escape.
      s_global_state.reset(nullptr);
      glutLeaveMainLoop();
      break;
  }
  glutPostRedisplay();
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Mainline.
//
//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  glutInit(&argc, argv);

  s_global_state = absl::make_unique<GlobalState>();
  s_global_state->window_width = s_global_state->window_height = 800;
  s_global_state->scene_root = BuildGraph(s_global_state->window_width,
                                          s_global_state->window_height);

  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
  glutSetOption(GLUT_MULTISAMPLE, 16);
  glutInitWindowSize(s_global_state->window_width,
                     s_global_state->window_height);

  glutCreateWindow("Ion rectangle example");
  glutDisplayFunc(Render);
  glutReshapeFunc(Resize);
  glutKeyboardFunc(Keyboard);
  glutKeyboardUpFunc(KeyboardUp);
  glutIdleFunc(Update);

  // Can't do this before GLUT creates the OpenGL context.
  ion::gfx::GraphicsManagerPtr graphics_manager(new ion::gfx::GraphicsManager);
  s_global_state->renderer.Reset(new ion::gfx::Renderer(graphics_manager));

  glutMainLoop();
}
