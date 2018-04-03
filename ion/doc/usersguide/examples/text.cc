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
#include <string>

#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/math/matrix.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
#include "ion/math/vector.h"
#include "ion/text/fontimage.h"
#include "ion/text/freetypefont.h"
#include "ion/text/layout.h"
#include "ion/text/outlinebuilder.h"
#include "absl/memory/memory.h"

// This has to be included last or bad things happen on Windows.
#include "GL/freeglut.h"

namespace {

//-----------------------------------------------------------------------------
//
// Font data (public domain TTF) is stored as an array in a header file to
// avoid having to load a file on all platforms at run-time.
//
//-----------------------------------------------------------------------------

static unsigned char kFontData[] = {
#include "./fontdata.h"
};

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

static const ion::text::FontPtr CreateFont() {
  static const char kFontName[] = "ExampleFont";
  static const size_t kFontSizeInPixels = 64U;
  static const size_t kSdfPadding = 8U;
  ion::text::FontPtr font(new ion::text::FreeTypeFont(
      kFontName, kFontSizeInPixels, kSdfPadding, kFontData, sizeof(kFontData)));
  return font;
}

static const ion::gfx::NodePtr BuildTextNode(
    const ion::text::FontImagePtr& font_image) {
  ion::text::LayoutOptions options;
  options.target_size.Set(0.f, 2.f);
  options.horizontal_alignment = ion::text::kAlignHCenter;
  options.vertical_alignment = ion::text::kAlignVCenter;
  options.line_spacing = 1.5f;
  const ion::text::Layout layout =
      font_image->GetFont()->BuildLayout("Hello,\nWorld!", options);

  ion::text::OutlineBuilderPtr outline_builder(new ion::text::OutlineBuilder(
      font_image, ion::gfxutils::ShaderManagerPtr(),
      ion::base::AllocatorPtr()));
  outline_builder->Build(layout, ion::gfx::BufferObject::kStreamDraw);
  outline_builder->SetTextColor(ion::math::Vector4f(1.f, 1.f, .4f, 1.f));
  outline_builder->SetOutlineColor(ion::math::Vector4f(.1f, .1f, .1f, 1.f));
  outline_builder->SetHalfSmoothWidth(2.f);
  outline_builder->SetOutlineWidth(6.f);
  return outline_builder->GetNode();
}

static const ion::gfx::NodePtr BuildScreenAlignedTextNode(
    const ion::text::FontImagePtr& font_image) {
  ion::text::LayoutOptions options;
  options.target_point.Set(0.1f, 0.f);
  options.target_size.Set(0.f, .06f);
  options.horizontal_alignment = ion::text::kAlignLeft;
  options.vertical_alignment = ion::text::kAlignBottom;
  const ion::text::Layout layout =
      font_image->GetFont()->BuildLayout("Screen-Aligned text", options);

  ion::text::OutlineBuilderPtr outline_builder(new ion::text::OutlineBuilder(
      font_image, ion::gfxutils::ShaderManagerPtr(),
      ion::base::AllocatorPtr()));
  outline_builder->Build(layout, ion::gfx::BufferObject::kStreamDraw);
  outline_builder->SetTextColor(ion::math::Vector4f(1.f, .8f, .8f, 1.f));
  outline_builder->SetOutlineColor(ion::math::Vector4f(.2f, .2f, .2f, 1.f));
  outline_builder->SetHalfSmoothWidth(2.f);
  outline_builder->SetOutlineWidth(6.f);
  return outline_builder->GetNode();
}

static const ion::gfx::NodePtr BuildGraph(int window_width, int window_height) {
  ion::gfx::NodePtr root(new ion::gfx::Node);

  const ion::math::Vector2i window_size(window_width, window_height);

  ion::gfx::StateTablePtr state_table(
      new ion::gfx::StateTable(window_width, window_height));
  state_table->SetViewport(
      ion::math::Range2i::BuildWithSize(ion::math::Point2i(0, 0), window_size));
  state_table->SetClearColor(ion::math::Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, true);
  root->SetStateTable(state_table);

  const ion::gfx::ShaderInputRegistryPtr& global_reg =
      ion::gfx::ShaderInputRegistry::GetGlobalRegistry();

  root->AddUniform(global_reg->Create<ion::gfx::Uniform>(
      "uViewportSize", window_size));

  ion::text::FontPtr font = CreateFont();
  static const size_t kFontImageSize = 256U;
  ion::text::DynamicFontImagePtr font_image(
      new ion::text::DynamicFontImage(font, kFontImageSize));

  ion::gfx::NodePtr text_node = BuildTextNode(font_image);
  text_node->AddUniform(global_reg->Create<ion::gfx::Uniform>(
      "uProjectionMatrix",
      ion::math::PerspectiveMatrixFromView(ion::math::Anglef::FromDegrees(60.f),
                                           1.f, .1f, 10.f)));
  text_node->AddUniform(global_reg->Create<ion::gfx::Uniform>(
      "uModelviewMatrix",
      ion::math::LookAtMatrixFromCenter(ion::math::Point3f(2.f, 2.f, 4.f),
                                        ion::math::Point3f::Zero(),
                                        ion::math::Vector3f::AxisY())));
  root->AddChild(text_node);

  ion::gfx::NodePtr aligned_text_node = BuildScreenAlignedTextNode(font_image);
  aligned_text_node->AddUniform(global_reg->Create<ion::gfx::Uniform>(
      "uProjectionMatrix",
      ion::math::OrthographicMatrixFromFrustum(0.f, 1.f, 0.f, 1.f, -1.f, 1.f)));
  aligned_text_node->AddUniform(global_reg->Create<ion::gfx::Uniform>(
      "uModelviewMatrix", ion::math::Matrix4f::Identity()));
  root->AddChild(aligned_text_node);

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
