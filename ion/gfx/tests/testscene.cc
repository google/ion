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

#include "ion/gfx/tests/testscene.h"

#include "ion/base/serialize.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/image.h"
#include "ion/gfx/indexbuffer.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfx/texture.h"

namespace ion {
namespace gfx {
namespace testing {

namespace {

using math::Matrix2f;
using math::Matrix3f;
using math::Matrix4f;
using math::Point2i;
using math::Range1i;
using math::Range2i;
using math::Vector2f;
using math::Vector2i;
using math::Vector2ui;
using math::Vector3f;
using math::Vector3i;
using math::Vector3ui;
using math::Vector4f;
using math::Vector4i;
using math::Vector4ui;

// The number of indices for index buffers.
static const int kNumIndices = 24;

static const char kVertexShader[] =
    "attribute float aFloat;\n"
    "attribute vec2 aFV2;\n"
    "attribute vec3 aFV3;\n"
    "attribute vec4 aFV4;\n"
    "attribute mat2 aMat2;\n"
    "attribute mat3 aMat3;\n"
    "attribute mat4 aMat4;\n"
    "attribute vec2 aBOE1;\n"
    "attribute vec3 aBOE2;\n"
    "uniform int uInt;\n"
    "uniform float uFloat;\n";
static const char kGeometryShader[] =
    "uniform int uIntGS;\n"
    "uniform uint uUintGS;\n"
    "uniform vec2 uFV2;\n"
    "uniform vec3 uFV3;\n"
    "uniform vec4 uFV4;\n";
static const char kFragmentShader[] =
    "uniform int uInt;\n"
    "uniform uint uUint;\n"
    "uniform float uFloat;\n"
    "uniform samplerCube uCubeMapTex;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec2 uFV2;\n"
    "uniform vec3 uFV3;\n"
    "uniform vec4 uFV4;\n"
    "uniform ivec2 uIV2;\n"
    "uniform ivec3 uIV3;\n"
    "uniform ivec4 uIV4;\n"
    "uniform uvec2 uUV2;\n"
    "uniform uvec3 uUV3;\n"
    "uniform uvec4 uUV4;\n"
    "uniform mat2 uMat2;\n"
    "uniform mat3 uMat3;\n"
    "uniform mat4 uMat4;\n"
    "uniform int uIntArray[2];\n"
    "uniform uint uUintArray[2];\n"
    "uniform float uFloatArray[2];\n"
    "uniform samplerCube uCubeMapTexArray[2];\n"
    "uniform sampler2D uTexArray[2];\n"
    "uniform vec2 uFV2Array[2];\n"
    "uniform vec3 uFV3Array[2];\n"
    "uniform vec4 uFV4Array[2];\n"
    "uniform ivec2 uIV2Array[2];\n"
    "uniform ivec3 uIV3Array[2];\n"
    "uniform ivec4 uIV4Array[2];\n"
    "uniform uvec2 uUV2Array[2];\n"
    "uniform uvec3 uUV3Array[2];\n"
    "uniform uvec4 uUV4Array[2];\n"
    "uniform mat2 uMat2Array[2];\n"
    "uniform mat3 uMat3Array[2];\n"
    "uniform mat4 uMat4Array[2];\n";

// Creates and returns a ShaderInputRegistry with one of each type of uniform
// and attribute in it.
static const ShaderInputRegistryPtr CreateRegistry() {
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->SetLabel("Registry");

  // One of each uniform type.
  reg->Add(ShaderInputRegistry::UniformSpec("uInt", kIntUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uUint", kUnsignedIntUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uFloat", kFloatUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uCubeMapTex", kCubeMapTextureUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uTex", kTextureUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uFV2", kFloatVector2Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uFV3", kFloatVector3Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uFV4", kFloatVector4Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uIV2", kIntVector2Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uIV3", kIntVector3Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uIV4", kIntVector4Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uUV2", kUnsignedIntVector2Uniform,
                                            "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uUV3", kUnsignedIntVector3Uniform,
                                            "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uUV4", kUnsignedIntVector4Uniform,
                                            "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uMat2", kMatrix2x2Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uMat3", kMatrix3x3Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uMat4", kMatrix4x4Uniform, "."));
  // Array uniforms.
  reg->Add(ShaderInputRegistry::UniformSpec("uIntArray", kIntUniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uUintArray", kUnsignedIntUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uFloatArray", kFloatUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uCubeMapTexArray",
                                            kCubeMapTextureUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uTexArray", kTextureUniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uFV2Array", kFloatVector2Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uFV3Array", kFloatVector3Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uFV4Array", kFloatVector4Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uIV2Array", kIntVector2Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uIV3Array", kIntVector3Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uIV4Array", kIntVector4Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uUV2Array",
                                            kUnsignedIntVector2Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uUV3Array",
                                            kUnsignedIntVector3Uniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uUV4Array",
                                            kUnsignedIntVector4Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uMat2Array", kMatrix2x2Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uMat3Array", kMatrix3x3Uniform, "."));
  reg->Add(
      ShaderInputRegistry::UniformSpec("uMat4Array", kMatrix4x4Uniform, "."));

  // One of each non-buffer attribute type.
  reg->Add(ShaderInputRegistry::AttributeSpec("aFloat", kFloatAttribute, "."));
  reg->Add(
      ShaderInputRegistry::AttributeSpec("aFV2", kFloatVector2Attribute, "."));
  reg->Add(
      ShaderInputRegistry::AttributeSpec("aFV3", kFloatVector3Attribute, "."));
  reg->Add(
      ShaderInputRegistry::AttributeSpec("aFV4", kFloatVector4Attribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aMat2", kFloatMatrix2x2Attribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aMat3", kFloatMatrix3x3Attribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aMat4", kFloatMatrix4x4Attribute, "."));

  // A couple of buffer object element attributes.
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aBOE1", kBufferObjectElementAttribute, "."));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aBOE2", kBufferObjectElementAttribute, "."));

  return reg;
}

// Creates and returns a dummy ShaderProgram using a registry.
static const ShaderProgramPtr CreateShaderProgram(
    const ShaderInputRegistryPtr& reg_ptr) {
  ShaderProgramPtr program(new ShaderProgram(reg_ptr));
  program->SetLabel("Dummy Shader");
  program->SetDocString("Program doc string");
  ShaderPtr vertex_shader(new Shader(kVertexShader));
  vertex_shader->SetLabel("Vertex shader");
  vertex_shader->SetDocString("Vertex shader doc string");
  ShaderPtr geometry_shader(new Shader(kGeometryShader));
  geometry_shader->SetLabel("Geometry shader");
  geometry_shader->SetDocString("Geometry shader doc string");
  ShaderPtr fragment_shader(new Shader(kFragmentShader));
  fragment_shader->SetLabel("Fragment shader");
  fragment_shader->SetDocString("Fragment shader doc string");
  program->SetVertexShader(vertex_shader);
  program->SetGeometryShader(geometry_shader);
  program->SetFragmentShader(fragment_shader);
  return program;
}

// Creates and returns a cube map containing 6 Images.
static const CubeMapTexturePtr BuildCubeMapTexture() {
  static const uint8 kPixels[2 * 2 * 3] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b };
  ImagePtr image(new Image);
  image->Set(Image::kRgb888, 2, 2,
             base::DataContainer::CreateAndCopy<uint8>(kPixels, 12, false,
                                                       image->GetAllocator()));

  CubeMapTexturePtr tex(new CubeMapTexture);
  SamplerPtr sampler(new Sampler);
  sampler->SetLabel("Cubemap Sampler");
  sampler->SetCompareFunction(Sampler::kNever);
  sampler->SetCompareMode(Sampler::kCompareToTexture);
  sampler->SetMinLod(-1.5f);
  sampler->SetMaxLod(1.5f);
  sampler->SetMinFilter(Sampler::kLinearMipmapLinear);
  sampler->SetMagFilter(Sampler::kNearest);
  sampler->SetWrapR(Sampler::kClampToEdge);
  sampler->SetWrapS(Sampler::kMirroredRepeat);
  sampler->SetWrapT(Sampler::kClampToEdge);
  tex->SetBaseLevel(10);
  tex->SetMaxLevel(100);
  for (int i = 0; i < 6; ++i)
    tex->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U, image);
  tex->SetSampler(sampler);
  tex->SetLabel("Cubemap");
  tex->SetSwizzleRed(Texture::kAlpha);
  tex->SetSwizzleGreen(Texture::kBlue);
  tex->SetSwizzleBlue(Texture::kGreen);
  tex->SetSwizzleAlpha(Texture::kRed);
  return tex;
}

// Creates and returns a Texture containing an Image.
static const TexturePtr BuildTexture() {
  static const uint8 kPixels[2 * 2 * 3] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b };
  ImagePtr image(new Image);
  image->Set(Image::kRgb888, 2, 2,
             base::DataContainer::CreateAndCopy<uint8>(kPixels, 12, false,
                                                       image->GetAllocator()));

  TexturePtr tex(new Texture);
  SamplerPtr sampler(new Sampler);
  sampler->SetLabel("Sampler");
  sampler->SetCompareFunction(Sampler::kNever);
  sampler->SetCompareMode(Sampler::kCompareToTexture);
  sampler->SetMinLod(-0.5f);
  sampler->SetMaxLod(0.5f);
  sampler->SetMinFilter(Sampler::kLinearMipmapLinear);
  sampler->SetMagFilter(Sampler::kNearest);
  sampler->SetWrapR(Sampler::kMirroredRepeat);
  sampler->SetWrapS(Sampler::kMirroredRepeat);
  sampler->SetWrapT(Sampler::kClampToEdge);
  tex->SetBaseLevel(10);
  tex->SetMaxLevel(100);
  tex->SetImage(0U, image);
  tex->SetSampler(sampler);
  tex->SetLabel("Texture");
  tex->SetSwizzleRed(Texture::kAlpha);
  tex->SetSwizzleGreen(Texture::kBlue);
  tex->SetSwizzleBlue(Texture::kGreen);
  tex->SetSwizzleAlpha(Texture::kRed);
  return tex;
}

// Adds a uniform of each type (including an invalid one) to a node.
static void AddUniformsToNode(const ShaderInputRegistryPtr& reg,
                              const NodePtr& node) {
  node->AddUniform(reg->Create<Uniform>("uInt", 13));
  node->AddUniform(reg->Create<Uniform>("uIntGS", 27));
  node->AddUniform(reg->Create<Uniform>("uUint", 15U));
  node->AddUniform(reg->Create<Uniform>("uUintGS", 33U));
  node->AddUniform(reg->Create<Uniform>("uFloat", 1.5f));
  node->AddUniform(reg->Create<Uniform>("uCubeMapTex", BuildCubeMapTexture()));
  node->AddUniform(reg->Create<Uniform>("uTex", BuildTexture()));
  node->AddUniform(reg->Create<Uniform>("uFV2", Vector2f(2.f, 3.f)));
  node->AddUniform(reg->Create<Uniform>("uFV3", Vector3f(4.f, 5.f, 6.f)));
  node->AddUniform(reg->Create<Uniform>("uFV4", Vector4f(7.f, 8.f, 9.f, 10.f)));
  node->AddUniform(reg->Create<Uniform>("uIV2", Vector2i(2, 3)));
  node->AddUniform(reg->Create<Uniform>("uIV3", Vector3i(4, 5, 6)));
  node->AddUniform(reg->Create<Uniform>("uIV4", Vector4i(7, 8, 9, 10)));
  node->AddUniform(reg->Create<Uniform>("uUV2", Vector2ui(2U, 3U)));
  node->AddUniform(reg->Create<Uniform>("uUV3", Vector3ui(4U, 5U, 6U)));
  node->AddUniform(reg->Create<Uniform>("uUV4", Vector4ui(7U, 8U, 9U, 10U)));
  node->AddUniform(reg->Create<Uniform>("uMat2", Matrix2f(1.f, 2.f,
                                                          3.f, 4.f)));
  node->AddUniform(reg->Create<Uniform>("uMat3", Matrix3f(1.f, 2.f, 3.f,
                                                          4.f, 5.f, 6.f,
                                                          7.f, 8.f, 9.f)));
  node->AddUniform(reg->Create<Uniform>("uMat4", Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                          5.f, 6.f, 7.f, 8.f,
                                                          9.f, 1.f, 2.f, 3.f,
                                                          4.f, 5.f, 6.f, 7.f)));

  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  std::vector<uint32> uints;
  uints.push_back(3U);
  uints.push_back(4U);
  std::vector<float> floats;
  floats.push_back(1.f);
  floats.push_back(2.f);
  std::vector<TexturePtr> textures;
  textures.push_back(BuildTexture());
  textures.push_back(BuildTexture());
  std::vector<CubeMapTexturePtr> cubemaps;
  cubemaps.push_back(BuildCubeMapTexture());
  cubemaps.push_back(BuildCubeMapTexture());
  std::vector<math::Vector2i> vector2is;
  vector2is.push_back(math::Vector2i(1, 2));
  vector2is.push_back(math::Vector2i(3, 4));
  std::vector<math::Vector3i> vector3is;
  vector3is.push_back(math::Vector3i(1, 2, 3));
  vector3is.push_back(math::Vector3i(4, 5, 6));
  std::vector<math::Vector4i> vector4is;
  vector4is.push_back(math::Vector4i(1, 2, 3, 4));
  vector4is.push_back(math::Vector4i(5, 6, 7, 8));
  std::vector<math::Vector2ui> vector2uis;
  vector2uis.push_back(math::Vector2ui(1U, 2U));
  vector2uis.push_back(math::Vector2ui(3U, 4U));
  std::vector<math::Vector3ui> vector3uis;
  vector3uis.push_back(math::Vector3ui(1U, 2U, 3U));
  vector3uis.push_back(math::Vector3ui(4U, 5U, 6U));
  std::vector<math::Vector4ui> vector4uis;
  vector4uis.push_back(math::Vector4ui(1U, 2U, 3U, 4U));
  vector4uis.push_back(math::Vector4ui(5U, 6U, 7U, 8U));
  std::vector<math::Vector2f> vector2fs;
  vector2fs.push_back(math::Vector2f(1.f, 2.f));
  vector2fs.push_back(math::Vector2f(3.f, 4.f));
  std::vector<math::Vector3f> vector3fs;
  vector3fs.push_back(math::Vector3f(1.f, 2.f, 3.f));
  vector3fs.push_back(math::Vector3f(4.f, 5.f, 6.f));
  std::vector<math::Vector4f> vector4fs;
  vector4fs.push_back(math::Vector4f(1.f, 2.f, 3.f, 4.f));
  vector4fs.push_back(math::Vector4f(5.f, 6.f, 7.f, 8.f));
  std::vector<math::Matrix2f> matrix2fs;
  matrix2fs.push_back(math::Matrix2f::Identity());
  matrix2fs.push_back(math::Matrix2f::Identity() * 2.f);
  std::vector<math::Matrix3f> matrix3fs;
  matrix3fs.push_back(math::Matrix3f::Identity());
  matrix3fs.push_back(math::Matrix3f::Identity() * 2.f);
  std::vector<math::Matrix4f> matrix4fs;
  matrix4fs.push_back(math::Matrix4f::Identity());
  matrix4fs.push_back(math::Matrix4f::Identity() * 2.f);

  node->AddUniform(reg->CreateArrayUniform("uIntArray", &ints[0], ints.size(),
                                           base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform("uUintArray", &uints[0],
                                           uints.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uFloatArray", &floats[0], floats.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uCubeMapTexArray", &cubemaps[0], cubemaps.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uTexArray", &textures[0], textures.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uFV2Array", &vector2fs[0], vector3fs.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uFV3Array", &vector3fs[0], vector2fs.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uFV4Array", &vector4fs[0], vector4fs.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uIV2Array", &vector2is[0], vector2is.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uIV3Array", &vector3is[0], vector3is.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uIV4Array", &vector4is[0], vector4is.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uUV2Array", &vector2uis[0], vector2uis.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uUV3Array", &vector3uis[0], vector3uis.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uUV4Array", &vector4uis[0], vector4uis.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uMat2Array", &matrix2fs[0], matrix2fs.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uMat3Array", &matrix3fs[0], matrix3fs.size(), base::AllocatorPtr()));
  node->AddUniform(reg->CreateArrayUniform(
      "uMat4Array", &matrix4fs[0], matrix4fs.size(), base::AllocatorPtr()));
}

// Creates and returns a Shape with the given primitive type.
static const ShapePtr CreateShape(Shape::PrimitiveType prim_type) {
  ShapePtr shape(new Shape);
  shape->SetPrimitiveType(prim_type);
  return shape;
}

// Adds one Shape with each primitive type to a node.
static void AddShapesToNode(const NodePtr& node) {
  node->AddShape(CreateShape(Shape::kLines));
  node->GetShapes()[node->GetShapes().size() - 1U]->SetLabel("Line Shape");
  node->AddShape(CreateShape(Shape::kLineLoop));
  node->GetShapes()[node->GetShapes().size() - 1U]
      ->SetLabel("Line loops Shape");
  node->AddShape(CreateShape(Shape::kLineStrip));
  node->GetShapes()[node->GetShapes().size() - 1U]
      ->SetLabel("Line strips Shape");
  node->AddShape(CreateShape(Shape::kPoints));
  node->GetShapes()[node->GetShapes().size() - 1U]->SetLabel("Points Shape");
  node->AddShape(CreateShape(Shape::kTriangles));
  node->GetShapes()[node->GetShapes().size() - 1U]->SetLabel("Triangles Shape");
  node->AddShape(CreateShape(Shape::kTriangleFan));
  node->GetShapes()[node->GetShapes().size() - 1U]
      ->SetLabel("Triangle fans Shape");
  node->AddShape(CreateShape(Shape::kTriangleStrip));
  node->GetShapes()[node->GetShapes().size() - 1U]
      ->SetLabel("Triangle strips Shape");
}

// Creates and returns an AttributeArray with each type of attribute.
static const AttributeArrayPtr CreateAttributeArray(
    const ShaderInputRegistryPtr& reg) {
  AttributeArrayPtr aa(new AttributeArray);
  aa->SetLabel("Vertex array");
  aa->AddAttribute(reg->Create<Attribute>("aFloat", 1.0f));
  aa->AddAttribute(reg->Create<Attribute>("aFV2", Vector2f(1.f, 2.f)));
  aa->AddAttribute(reg->Create<Attribute>("aFV3", Vector3f(1.f, 2.f, 3.f)));
  aa->AddAttribute(reg->Create<Attribute>("aFV4",
                                          Vector4f(1.f, 2.f, 3.f, 4.f)));
  aa->AddAttribute(reg->Create<Attribute>("aMat2", Matrix2f(1.f, 2.f,
                                                            3.f, 4.f)));
  aa->AddAttribute(reg->Create<Attribute>("aMat3", Matrix3f(1.f, 2.f, 3.f,
                                                            4.f, 5.f, 6.f,
                                                            7.f, 8.f, 9.f)));
  aa->AddAttribute(reg->Create<Attribute>("aMat4",
                                          Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                   5.f, 6.f, 7.f, 8.f,
                                                   9.f, 1.f, 2.f, 3.f,
                                                   4.f, 5.f, 6.f, 7.f)));

  // Add and bind a couple of buffer object elements.
  const TestScene::Vertex vertices[3] = {
    TestScene::Vertex(0),
    TestScene::Vertex(1),
    TestScene::Vertex(2)
  };
  BufferObjectPtr buffer_object(new BufferObject);
  base::DataContainerPtr container =
      base::DataContainer::CreateAndCopy<TestScene::Vertex>(
          vertices, 3, false, buffer_object->GetAllocator());
  buffer_object->SetData(
      container, sizeof(vertices[0]), 3U, BufferObject::kStaticDraw);
  buffer_object->SetLabel("Vertex buffer");

  aa->AddAttribute(reg->Create<Attribute>(
      "aBOE1", BufferObjectElement(buffer_object, buffer_object->AddSpec(
          BufferObject::kFloat, 1, 0))));
  size_t index = aa->AddAttribute(reg->Create<Attribute>(
      "aBOE2", BufferObjectElement(buffer_object, buffer_object->AddSpec(
          BufferObject::kFloat, 2, TestScene::GetSecondBoeAttributeOffset()))));
  aa->GetMutableAttribute(index)->SetFixedPointNormalized(true);
  return aa;
}

// Creates and returns an AttributeArray usable by the default shader.
static const AttributeArrayPtr CreateDefaultAttributeArray() {
  const ShaderInputRegistryPtr& reg = ShaderInputRegistry::GetGlobalRegistry();
  AttributeArrayPtr aa(new AttributeArray);

  // Add and bind a buffer object element.
  const TestScene::Vertex vertices[3] = {
    TestScene::Vertex(0),
    TestScene::Vertex(1),
    TestScene::Vertex(2)
  };
  BufferObjectPtr buffer_object(new BufferObject);
  base::DataContainerPtr container =
      base::DataContainer::CreateAndCopy<TestScene::Vertex>(
          vertices, 3, false, buffer_object->GetAllocator());
  buffer_object->SetData(
      container, sizeof(vertices[0]), 3U, BufferObject::kStaticDraw);
  aa->AddAttribute(reg->Create<Attribute>(
      "aVertex", BufferObjectElement(buffer_object, buffer_object->AddSpec(
          BufferObject::kFloat, 3, 0))));
  return aa;
}

// Creates and returns an IndexBuffer with the given type of indices.
template <typename T>
static IndexBufferPtr CreateIndexBuffer(BufferObject::ComponentType type,
                                        BufferObject::UsageMode usage) {
  // Set up an array of indices of the correct type.
  T indices[kNumIndices];
  for (int i = 0; i < kNumIndices; ++i)
    indices[i] = static_cast<T>(i % 3);

  IndexBufferPtr index_buffer(new IndexBuffer);
  // Copy them into a DataContainer.
  base::DataContainerPtr container =
      base::DataContainer::CreateAndCopy<T>(indices, kNumIndices, false,
                                            index_buffer->GetAllocator());

  // Create an IndexBuffer using them with a couple of ranges.
  index_buffer->AddSpec(type, 1, 0);
  index_buffer->SetData(container, sizeof(indices[0]), kNumIndices, usage);
  return index_buffer;
}

// Creates and returns a test scene for printing.
static const NodePtr BuildTestScene(bool capture_varyings) {
  // Create a registry with one of each type of uniform and attribute in it.
  ShaderInputRegistryPtr reg_ptr(CreateRegistry());
  reg_ptr->IncludeGlobalRegistry();

  // Create a root node and a ShaderProgram to it.
  NodePtr root(new Node);
  root->SetLabel("Root Node");
  ShaderProgramPtr prog = CreateShaderProgram(reg_ptr);
  root->SetShaderProgram(prog);

  // Add one uniform of each supported type to the root.
  AddUniformsToNode(reg_ptr, root);

  // When testing transform feedback, we need a simple scene graph and
  // at least one captured varying.
  if (capture_varyings) {
    prog->SetCapturedVaryings({"gl_Position"});
    ShapePtr shape = CreateShape(Shape::kTriangles);
    shape->SetLabel("Default Shape");
    shape->SetAttributeArray(CreateAttributeArray(reg_ptr));
    root->AddShape(shape);
    return root;
  }

  // Add a child Node with shapes in it.
  NodePtr node_with_shapes(new Node);
  node_with_shapes->SetLabel("Node with Shapes");
  root->AddChild(node_with_shapes);
  AddShapesToNode(node_with_shapes);

  // Add an AttributeArray with one of each attribute type to the first Shape.
  const base::AllocVector<ShapePtr>& shapes = node_with_shapes->GetShapes();
  shapes[0]->SetAttributeArray(CreateAttributeArray(reg_ptr));

  // Add one IndexBuffer of each type to the Shapes.
  DCHECK_GE(shapes.size(), 7U);
  shapes[0]->SetIndexBuffer(CreateIndexBuffer<int8>(BufferObject::kByte,
                                                    BufferObject::kStaticDraw));
  shapes[1]->SetIndexBuffer(
      CreateIndexBuffer<uint8>(BufferObject::kUnsignedByte,
                               BufferObject::kStaticDraw));
  shapes[2]->SetIndexBuffer(
      CreateIndexBuffer<int16>(BufferObject::kShort,
                               BufferObject::kDynamicDraw));
  shapes[3]->SetIndexBuffer(
      CreateIndexBuffer<uint16>(BufferObject::kUnsignedShort,
                                BufferObject::kStreamDraw));
  shapes[4]->SetIndexBuffer(
      CreateIndexBuffer<int32>(BufferObject::kInt, BufferObject::kStaticDraw));
  shapes[5]->SetIndexBuffer(
      CreateIndexBuffer<uint32>(BufferObject::kUnsignedInt,
                                BufferObject::kDynamicDraw));
  shapes[6]->SetIndexBuffer(
      CreateIndexBuffer<float>(BufferObject::kFloat,
                               BufferObject::kStreamDraw));

  // Add a couple of vertex ranges to each Shape.
  for (int i = 0; i < 7; ++i) {
    shapes[i]->AddVertexRange(Range1i(0, 3));
    shapes[i]->AddVertexRange(Range1i(10, kNumIndices - 1));
    shapes[i]->GetIndexBuffer()->SetLabel("Indices #" +
                                          base::ValueToString(i));
  }

  NodePtr default_root(new Node);
  default_root->SetLabel("Real Root Node");

  default_root->AddChild(root);
  default_root->AddUniform(
      reg_ptr->Create<Uniform>("uProjectionMatrix",
                               Matrix4f(1.f, 2.f, 3.f, 4.f,
                                        5.f, 1.f, 7.f, 8.f,
                                        9.f, 1.f, 1.f, 3.f,
                                        4.f, 5.f, 6.f, 1.f)));
  default_root->AddUniform(
      reg_ptr->Create<Uniform>("uModelviewMatrix",
                               Matrix4f(4.f, 2.f, 3.f, 4.f,
                                        5.f, 4.f, 7.f, 8.f,
                                        9.f, 1.f, 4.f, 3.f,
                                        4.f, 5.f, 6.f, 4.f)));
  default_root->AddUniform(
      reg_ptr->Create<Uniform>("uBaseColor", Vector4f(4.f, 3.f, 2.f, 1.f)));
  // Add a shape to the root so that the default program will be used.
  ShapePtr shape = CreateShape(Shape::kTriangles);
  shape->SetLabel("Default Shape");
  shape->SetAttributeArray(CreateDefaultAttributeArray());
  default_root->AddShape(shape);
  return default_root;
}

}  // anonymous namespace

TestScene::TestScene(bool capture_varyings)
    : scene_(BuildTestScene(capture_varyings)) {}

const CubeMapTexturePtr TestScene::CreateCubeMapTexture() const {
  return BuildCubeMapTexture();
}

const TexturePtr TestScene::CreateTexture() const {
  return BuildTexture();
}

size_t TestScene::GetIndexCount() const {
  return kNumIndices;
}

size_t TestScene::GetBufferSize() const {
  return sizeof(TestScene::Vertex) * 3U;
}

size_t TestScene::GetBufferStride() const {
  return sizeof(TestScene::Vertex);
}

size_t TestScene::GetSecondBoeAttributeOffset() {
  const TestScene::Vertex v;
  return reinterpret_cast<const char*>(&v.fv2) -
      reinterpret_cast<const char*>(&v);
}

const std::string TestScene::GetVertexShaderSource() const {
  return std::string(kVertexShader);
}

const std::string TestScene::GetGeometryShaderSource() const {
  return std::string(kGeometryShader);
}

const std::string TestScene::GetFragmentShaderSource() const {
  return std::string(kFragmentShader);
}


}  // namespace testing
}  // namespace gfx
}  // namespace ion
