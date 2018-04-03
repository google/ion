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

#include "ion/base/datacontainer.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/shapeutils.h"
#include "ion/math/angle.h"
#include "ion/math/matrix.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
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
// Shader program strings.
//
//-----------------------------------------------------------------------------

static const char* kVertexShaderString = (
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "uniform mat4 uTextureMatrix;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec2 aTexCoords;\n"
    "varying vec3 vPosition;\n"
    "varying vec2 vTexCoords;\n"
    "\n"
    "void main(void) {\n"
    "  vTexCoords = (uTextureMatrix * vec4(aTexCoords, 0., 1.)).st;\n"
    "  vPosition = aVertex;\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
    "      vec4(aVertex, 1.);\n"
    "}\n");

static const char* kFragmentShaderString = (
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "uniform sampler2D uSampler;\n"
    "uniform float uWaveFrequency;\n"
    "varying vec3 vPosition;\n"
    "varying vec2 vTexCoords;\n"
    "\n"
    "void main(void) {\n"
    "  float nx = sin(uWaveFrequency * radians(90.) * vPosition.x);\n"
    "  vec3 normal = normalize(vec3(nx, 0., .5));\n"
    "  vec3 dir_to_light = normalize(vec3(1., 2., 10.));\n"
    "  float intensity = max(0.0, dot(dir_to_light, normal));\n"
    "  gl_FragColor = intensity * texture2D(uSampler, vTexCoords);\n"
    "}\n");

//-----------------------------------------------------------------------------
//
// Scene graph construction.
//
//-----------------------------------------------------------------------------

static const ion::math::Matrix4f BuildTextureRotationMatrix(float degrees) {
  return
      ion::math::TranslationMatrix(ion::math::Vector3f(.5f, .5f, 0.f)) *
      ion::math::RotationMatrixAxisAngleH(
          ion::math::Vector3f::AxisZ(),
          ion::math::Anglef::FromDegrees(degrees)) *
      ion::math::TranslationMatrix(ion::math::Vector3f(-.5f, -.5f, 0.f));
}

static ion::gfx::TexturePtr BuildTexture() {
  // 2x2 RGB pixels.  Note that OpenGL defines images with the bottom row first.
  static const int kWidth = 2;
  static const int kHeight = 2;
  static const int kRowSize = kWidth * 3;
  static const uint8 pixels[kHeight * kRowSize] = {
    0xee, 0x22, 0xee,  0x00, 0x55, 0xdd,  // Bottom row : magenta, blue.
    0x00, 0xdd, 0xaa,  0xdd, 0xcc, 0x33,  // Top row: green, yellow.
  };

  ion::gfx::ImagePtr image(new ion::gfx::Image);
  ion::base::DataContainerPtr data_container =
      ion::base::DataContainer::CreateAndCopy<uint8>(
          pixels, sizeof(pixels), true, ion::base::AllocatorPtr());
  image->Set(ion::gfx::Image::kRgb888, kWidth, kHeight, data_container);

  ion::gfx::SamplerPtr sampler(new ion::gfx::Sampler);
  // This is required for textures on iOS. No other texture wrap mode seems to
  // be supported.
  sampler->SetWrapS(ion::gfx::Sampler::kClampToEdge);
  sampler->SetWrapT(ion::gfx::Sampler::kClampToEdge);

  ion::gfx::TexturePtr texture(new ion::gfx::Texture);
  texture->SetImage(0U, image);
  texture->SetSampler(sampler);
  return texture;
}

static const ion::gfx::NodePtr BuildGraph(int window_width, int window_height) {
  ion::gfx::NodePtr root(new ion::gfx::Node);

  ion::gfxutils::RectangleSpec rect_spec;
  rect_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionTexCoords;
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

  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uTextureMatrix", ion::gfx::kMatrix4x4Uniform,
      "Matrix applied to texture coordinates"));
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uSampler", ion::gfx::kTextureUniform,
      "Texture sampler"));
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uWaveFrequency", ion::gfx::kFloatUniform,
      "Frequency of the sine wave applied to the rectangle normal"));
  root->SetShaderProgram(
      ion::gfx::ShaderProgram::BuildFromStrings(
          "Example shader", reg, kVertexShaderString,
          kFragmentShaderString, ion::base::AllocatorPtr()));

  const ion::math::Matrix4f proj(1.732f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.732f, 0.0f, 0.0f,
                                 0.0f, 0.0f, -1.905f, -13.798f,
                                 0.0f, 0.0f, -1.0f, 0.0f);
  const ion::math::Matrix4f view(1.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f, 0.0f, 0.0f,
                                 0.0f, 0.0f, 1.0f, -5.0f,
                                 0.0f, 0.0f, 0.0f, 1.0f);
  const ion::math::Matrix4f tex_mtx = BuildTextureRotationMatrix(30.f);

  root->AddUniform(reg->Create<ion::gfx::Uniform>("uProjectionMatrix", proj));
  root->AddUniform(reg->Create<ion::gfx::Uniform>("uModelviewMatrix", view));
  root->AddUniform(reg->Create<ion::gfx::Uniform>("uTextureMatrix", tex_mtx));
  root->AddUniform(reg->Create<ion::gfx::Uniform>("uWaveFrequency", 5.f));
  root->AddUniform(reg->Create<ion::gfx::Uniform>("uSampler", BuildTexture()));

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

  glutCreateWindow("Ion texture example");
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
