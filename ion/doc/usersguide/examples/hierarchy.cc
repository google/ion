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
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/shapeutils.h"
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

static const char* kNode1VertexShaderString = (
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "uniform vec4 uTopColor;\n"
    "uniform vec4 uBottomColor;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec3 aNormal;\n"
    "varying vec3 vNormal;\n"
    "varying vec4 vColor;\n"
    "\n"
    "void main(void) {\n"
    "  vNormal = aNormal;\n"
    "  vColor = mix(uBottomColor, uTopColor, .5 * (1. + aVertex.y));\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
    "      vec4(aVertex, 1.);\n"
    "}\n");

static const char* kNode1FragmentShaderString = (
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "varying vec3 vNormal;\n"
    "varying vec4 vColor;\n"
    "\n"
    "void main(void) {\n"
    "  vec3 normal = normalize(vNormal);\n"
    "  vec3 dir_to_light = normalize(vec3(1., 4., 8.));\n"
    "  float intensity = min(1., abs(dot(dir_to_light, normal)));\n"
    "  gl_FragColor = intensity * vColor;\n"
    "}\n");

static const char* kNode3VertexShaderString = (
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec3 aNormal;\n"
    "varying vec3 vNormal;\n"
    "\n"
    "void main(void) {\n"
    "  vNormal = aNormal;\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
    "      vec4(aVertex, 1.);\n"
    "}\n");

static const char* kNode3FragmentShaderString = (
    "#ifdef GL_ES\n"
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "#endif\n"
    "\n"
    "uniform vec4 uBaseColor;\n"
    "uniform vec3 uOpenDirection;\n"
    "varying vec3 vNormal;\n"
    "\n"
    "void main(void) {\n"
    "  vec3 normal = normalize(vNormal);\n"
    "  if (dot(vNormal, uOpenDirection) > .9)\n"
    "    discard;\n"
    "  vec3 dir_to_light = normalize(vec3(1., 1., 2.));\n"
    "  float intensity = min(1., abs(dot(dir_to_light, normal)));\n"
    "  gl_FragColor = intensity * uBaseColor;\n"
    "}\n");

//-----------------------------------------------------------------------------
//
// Scene graph construction.
//
//-----------------------------------------------------------------------------

static const ion::gfx::NodePtr BuildNode1(int window_width, int window_height) {
  ion::gfx::NodePtr node1(new ion::gfx::Node);

  ion::gfxutils::EllipsoidSpec sphere_spec;
  sphere_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionNormal;
  sphere_spec.size.Set(2.f, 2.f, 2.f);
  node1->AddShape(ion::gfxutils::BuildEllipsoidShape(sphere_spec));

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
  node1->SetStateTable(state_table);

  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uTopColor", ion::gfx::kFloatVector4Uniform,
      "Color at the top of the rectangle"));
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uBottomColor", ion::gfx::kFloatVector4Uniform,
      "Color at the bottom of the rectangle"));
  node1->SetShaderProgram(
      ion::gfx::ShaderProgram::BuildFromStrings(
          "Node1 shader", reg, kNode1VertexShaderString,
          kNode1FragmentShaderString, ion::base::AllocatorPtr()));

  const ion::math::Matrix4f proj =
      ion::math::PerspectiveMatrixFromView(ion::math::Anglef::FromDegrees(60.f),
                                           1.f, .1f, 10.f);
  const ion::math::Matrix4f view =
      ion::math::LookAtMatrixFromCenter(ion::math::Point3f(1.f, 1.f, 6.f),
                                        ion::math::Point3f(0.f, -1.f, 0.f),
                                        ion::math::Vector3f::AxisY());
  node1->AddUniform(reg->Create<ion::gfx::Uniform>("uProjectionMatrix", proj));
  node1->AddUniform(reg->Create<ion::gfx::Uniform>("uModelviewMatrix", view));
  node1->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uTopColor", ion::math::Vector4f(1.f, .2f, .2f, 1.f)));
  node1->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uBottomColor", ion::math::Vector4f(.2f, 1.f, 1.f, 1.f)));

  return node1;
}

static const ion::gfx::NodePtr BuildNode2(
    const ion::gfx::ShaderInputRegistryPtr& reg) {
  ion::gfx::NodePtr node2(new ion::gfx::Node);

  ion::gfxutils::BoxSpec box_spec;
  box_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionNormal;
  box_spec.size.Set(3.f, 2.f, 1.f);
  node2->AddShape(ion::gfxutils::BuildBoxShape(box_spec));

  ion::gfx::StateTablePtr state_table(new ion::gfx::StateTable);
  state_table->SetCullFaceMode(ion::gfx::StateTable::kCullFront);
  node2->SetStateTable(state_table);

  node2->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uTopColor", ion::math::Vector4f(.9f, .9f, .2f, 1.f)));
  node2->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uBottomColor", ion::math::Vector4f(.9f, .1f, .9f, 1.f)));
  node2->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uModelviewMatrix",
      ion::math::TranslationMatrix(ion::math::Point3f(-2.f, -3.f, 0.f))));

  return node2;
}

static const ion::gfx::NodePtr BuildNode3() {
  ion::gfx::NodePtr node3(new ion::gfx::Node);

  ion::gfxutils::CylinderSpec cylinder_spec;
  cylinder_spec.vertex_type = ion::gfxutils::ShapeSpec::kPositionNormal;
  cylinder_spec.height = 2.f;
  node3->AddShape(ion::gfxutils::BuildCylinderShape(cylinder_spec));

  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uOpenDirection", ion::gfx::kFloatVector3Uniform,
      "Surface normal direction near cut-out"));
  node3->SetShaderProgram(
      ion::gfx::ShaderProgram::BuildFromStrings(
          "Node3 shader", reg, kNode3VertexShaderString,
          kNode3FragmentShaderString, ion::base::AllocatorPtr()));

  node3->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uModelviewMatrix",
      ion::math::TranslationMatrix(ion::math::Point3f(2.f, -3.f, 0.f))));
  node3->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uBaseColor", ion::math::Vector4f(.9f, .9f, .7f, 1.f)));
  node3->AddUniform(reg->Create<ion::gfx::Uniform>(
      "uOpenDirection", ion::math::Vector3f::AxisZ()));

  return node3;
}

static const ion::gfx::NodePtr BuildGraph(int window_width, int window_height) {
  ion::gfx::NodePtr node1 = BuildNode1(window_width, window_height);
  node1->AddChild(BuildNode2(node1->GetShaderProgram()->GetRegistry()));
  node1->AddChild(BuildNode3());
  return node1;
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

  glutCreateWindow("Ion hierarchy example");
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
