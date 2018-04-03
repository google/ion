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
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/uniform.h"
#include "ion/gfxutils/buffertoattributebinder.h"
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
    "attribute vec3 aNormal;\n"
    "attribute float aOffsetAlongNormal;\n"
    "varying vec3 vPosition;\n"
    "varying vec2 vTexCoords;\n"
    "varying vec3 vNormal;\n"
    "\n"
    "void main(void) {\n"
    "  vTexCoords = (uTextureMatrix * vec4(aTexCoords, 0., 1.)).st;\n"
    "  vPosition = aVertex + aOffsetAlongNormal * aNormal;\n"
    "  vNormal = aNormal;\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
    "      vec4(vPosition, 1.);\n"
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
    "varying vec3 vPosition;\n"
    "varying vec2 vTexCoords;\n"
    "varying vec3 vNormal;\n"
    "\n"
    "void main(void) {\n"
    "  vec3 dir_to_light = normalize(vec3(6., 3., 10.));\n"
    "  float intensity = .3 * abs(dot(dir_to_light, vNormal));\n"
    "  gl_FragColor = intensity * texture2D(uSampler, vTexCoords);\n"
    "}\n");

//-----------------------------------------------------------------------------
//
// Scene graph construction.
//
//-----------------------------------------------------------------------------

struct Vertex {
  ion::math::Point3f position;
  ion::math::Point2f texture_coords;
  ion::math::Vector3f normal;
  float offset_along_normal;
};

static ion::gfx::BufferObjectPtr BuildPyramidBufferObject() {
  const ion::math::Point3f apex(0.f, 1.f, 0.f);
  const ion::math::Point3f back_left(-1.f, -1.f, -1.f);
  const ion::math::Point3f back_right(1.f, -1.f, -1.f);
  const ion::math::Point3f front_left(-1.f, -1.f, 1.f);
  const ion::math::Point3f front_right(1.f, -1.f, 1.f);

  static const size_t kVertexCount = 12U;  // 3 vertices for each of 4 sides.
  Vertex vertices[kVertexCount];

  // Vertex positions.
  vertices[0].position = front_left;   // Front side.
  vertices[1].position = front_right;
  vertices[2].position = apex;
  vertices[3].position = front_right;  // Right side.
  vertices[4].position = back_right;
  vertices[5].position = apex;
  vertices[6].position = back_right;   // Back side.
  vertices[7].position = back_left;
  vertices[8].position = apex;
  vertices[9].position = back_left;    // Left side.
  vertices[10].position = front_left;
  vertices[11].position = apex;

  // Surface normals and texture coordinates.
  for (int face = 0; face < 4; ++face) {
    Vertex& v0 = vertices[3 * face + 0];
    Vertex& v1 = vertices[3 * face + 1];
    Vertex& v2 = vertices[3 * face + 2];
    v0.normal = v1.normal = v2.normal = ion::math::Cross(
        v1.position - v0.position, v2.position - v0.position);
    v0.texture_coords.Set(0.f, 0.f);
    v1.texture_coords.Set(1.f, 0.f);
    v2.texture_coords.Set(.5f, 1.f);
  }

  // Offsets.
  for (size_t v = 0; v < kVertexCount; ++v)
    vertices[v].offset_along_normal = .05f * static_cast<float>(1U + v % 2U);

  ion::base::DataContainerPtr data_container =
      ion::base::DataContainer::CreateAndCopy<Vertex>(
          vertices, kVertexCount, true, ion::base::AllocatorPtr());
  ion::gfx::BufferObjectPtr buffer_object(new ion::gfx::BufferObject);
  buffer_object->SetData(data_container, sizeof(vertices[0]), kVertexCount,
                         ion::gfx::BufferObject::kStaticDraw);
  return buffer_object;
}

const ion::gfx::AttributeArrayPtr BuildPyramidAttributeArray(
    const ion::gfx::ShaderInputRegistryPtr& reg) {
  ion::gfx::BufferObjectPtr buffer_object = BuildPyramidBufferObject();

  ion::gfx::AttributeArrayPtr attribute_array(new ion::gfx::AttributeArray);
  Vertex v;
  ion::gfxutils::BufferToAttributeBinder<Vertex>(v)
      .Bind(v.position, "aVertex")
      .Bind(v.texture_coords, "aTexCoords")
      .Bind(v.normal, "aNormal")
      .Bind(v.offset_along_normal, "aOffsetAlongNormal")
      .Apply(reg, attribute_array, buffer_object);
  return attribute_array;
}

static const ion::gfx::IndexBufferPtr BuildPyramidIndexBuffer() {
  ion::gfx::IndexBufferPtr index_buffer(new ion::gfx::IndexBuffer);

  static const size_t kIndexCount = 12U;
  uint16 indices[kIndexCount];
  for (size_t i = 0; i < kIndexCount; ++i)
    indices[i] = static_cast<uint16>(i);

  ion::base::DataContainerPtr data_container =
      ion::base::DataContainer::CreateAndCopy<uint16>(
          indices, kIndexCount, true, ion::base::AllocatorPtr());

  index_buffer->AddSpec(ion::gfx::BufferObject::kUnsignedShort, 1, 0);
  index_buffer->SetData(data_container, sizeof(indices[0]), kIndexCount,
                         ion::gfx::BufferObject::kStaticDraw);

  return index_buffer;
}

const ion::gfx::ShapePtr BuildPyramidShape(
    const ion::gfx::ShaderInputRegistryPtr& reg) {
  ion::gfx::ShapePtr shape(new ion::gfx::Shape);
  shape->SetLabel("Pyramid");
  shape->SetPrimitiveType(ion::gfx::Shape::kTriangles);
  shape->SetAttributeArray(BuildPyramidAttributeArray(reg));
  shape->SetIndexBuffer(BuildPyramidIndexBuffer());
  return shape;
}

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

  ion::gfx::StateTablePtr state_table(
      new ion::gfx::StateTable(window_width, window_height));
  state_table->SetViewport(
      ion::math::Range2i::BuildWithSize(ion::math::Point2i(0, 0),
                                        ion::math::Vector2i(window_width,
                                                            window_height)));
  state_table->SetClearColor(ion::math::Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(1.f);
  state_table->Enable(ion::gfx::StateTable::kDepthTest, true);
  state_table->Enable(ion::gfx::StateTable::kCullFace, false);
  root->SetStateTable(state_table);

  ion::gfx::ShaderInputRegistryPtr reg(new ion::gfx::ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ion::gfx::ShaderInputRegistry::AttributeSpec(
      "aOffsetAlongNormal", ion::gfx::kBufferObjectElementAttribute,
      "Offset of each vertex along its surface normal vector"));
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uTextureMatrix", ion::gfx::kMatrix4x4Uniform,
      "Matrix applied to texture coordinates"));
  reg->Add(ion::gfx::ShaderInputRegistry::UniformSpec(
      "uSampler", ion::gfx::kTextureUniform,
      "Texture sampler"));
  root->SetShaderProgram(
      ion::gfx::ShaderProgram::BuildFromStrings(
          "Example shader", reg, kVertexShaderString,
          kFragmentShaderString, ion::base::AllocatorPtr()));

  root->AddShape(BuildPyramidShape(reg));

  const ion::math::Matrix4f proj =
      ion::math::PerspectiveMatrixFromView(ion::math::Anglef::FromDegrees(60.f),
                                           1.f, .1f, 10.f);
  const ion::math::Matrix4f view =
      ion::math::LookAtMatrixFromCenter(ion::math::Point3f(3.f, 2.f, 5.f),
                                        ion::math::Point3f::Zero(),
                                        ion::math::Vector3f::AxisY());
  const ion::math::Matrix4f tex_mtx = BuildTextureRotationMatrix(30.f);

  root->AddUniform(reg->Create<ion::gfx::Uniform>("uProjectionMatrix", proj));
  root->AddUniform(reg->Create<ion::gfx::Uniform>("uModelviewMatrix", view));
  root->AddUniform(reg->Create<ion::gfx::Uniform>("uTextureMatrix", tex_mtx));
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

  glutCreateWindow("Ion shape example");
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
