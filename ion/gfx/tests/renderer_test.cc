/**
Copyright 2016 Google Inc. All Rights Reserved.

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

// These tests rely on trace streams, which are disabled in production builds.
#if !ION_PRODUCTION

#include "ion/gfx/renderer.h"

#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "ion/base/datacontainer.h"
#include "ion/base/enumhelper.h"
#include "ion/base/logchecker.h"
#include "ion/base/serialize.h"
#include "ion/base/tests/badwritecheckingallocator.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/threadspawner.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/resourcemanager.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/tests/mockgraphicsmanager.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/uniform.h"
#include "ion/math/matrix.h"
#include "ion/math/matrixutils.h"
#include "ion/math/range.h"
#include "ion/math/transformutils.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"
#include "ion/port/nullptr.h"  // For kNullFunction.
#include "ion/portgfx/visual.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

using testing::MockGraphicsManager;
using testing::MockGraphicsManagerPtr;
using testing::MockVisual;
using math::Point2i;
using math::Range1i;
using math::Range1ui;
using math::Range2i;
using math::Vector2i;

namespace {

struct SpecInfo {
  SpecInfo(size_t index_in, const std::string& type_in)
      : index(index_in),
        type(type_in) {}
  size_t index;
  std::string type;
};

struct Vertex {
  math::Vector3f point_coords;
  math::Vector2f tex_coords;
};

static const size_t kVboSize = 4U * sizeof(Vertex);

template <typename IndexType>
BufferObject::ComponentType GetComponentType();

template <>
BufferObject::ComponentType GetComponentType<uint16>() {
  return BufferObject::ComponentType::kUnsignedShort;
}

template <>
BufferObject::ComponentType GetComponentType<uint32>() {
  return BufferObject::ComponentType::kUnsignedInt;
}

template <typename T>
struct CallbackHelper {
  CallbackHelper() : was_called(false) {}
  ~CallbackHelper() {}
  void Callback(const std::vector<T>& infos_in) {
    was_called = true;
    infos = infos_in;
  }
  // Whether the callback has been called.
  bool was_called;
  // The saved vector of infos.
  std::vector<T> infos;
};

static const char* kPlaneVertexShaderString = (
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec2 aTexCoords;\n"
    "attribute vec4 aTestAttrib;\n"
    "varying vec2 vTexCoords;\n"
    "\n"
    "void main(void) {\n"
    "  vTexCoords = aTexCoords;\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
    "      vec4(aVertex, 1.);\n"
    "}\n");

static const char* kInstancedVertexShaderString = (
    "#extension GL_EXT_draw_instanced : enable\n"
    "uniform mat4 uProjectionMatrix;\n"
    "uniform mat4 uModelviewMatrix;\n"
    "attribute vec3 aVertex;\n"
    "attribute vec2 aTexCoords;\n"
    "varying vec2 vTexCoords;\n"
    "\n"
    "void main(void) {\n"
    "  vTexCoords = aTexCoords;\n"
    "  vec3 offset = vec3(15.0 * gl_InstanceID, 15.0 * gl_InstanceID, 0);\n"
    "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
    "      vec4(aVertex + offset, 1.);\n"
    "}\n");

static const char* kPlaneFragmentShaderString = (
    "uniform sampler2D uTexture;\n"
    "uniform sampler2D uTexture2;\n"
    "uniform samplerCube uCubeMapTexture;\n"
    "varying vec2 vTexCoords;\n"
    "\n"
    "void main(void) {\n"
    "  gl_FragColor = texture2D(uTexture, vTexCoords);\n"
    "}\n");

struct Options {
  Options() {
    vertex_buffer_usage = BufferObject::kStaticDraw;
    index_buffer_usage = BufferObject::kStaticDraw;
    primitive_type = Shape::kTriangles;
    image_format = Image::kRgba8888;
    compare_func = Sampler::kLess;
    compare_mode = Sampler::kNone;
    base_level = 0;
    max_level = 1000;
    max_anisotropy = 1.f;
    min_lod = -1000.f;
    max_lod = 1000.f;
    min_filter = Sampler::kNearest;
    mag_filter = Sampler::kNearest;
    swizzle_r = Texture::kRed;
    swizzle_g = Texture::kGreen;
    swizzle_b = Texture::kBlue;
    swizzle_a = Texture::kAlpha;
    wrap_r = Sampler::kClampToEdge;
    wrap_s = Sampler::kClampToEdge;
    wrap_t = Sampler::kClampToEdge;
  }
  BufferObject::UsageMode vertex_buffer_usage;
  BufferObject::UsageMode index_buffer_usage;
  Shape::PrimitiveType primitive_type;
  Image::Format image_format;
  Sampler::CompareFunction compare_func;
  Sampler::CompareMode compare_mode;
  float max_anisotropy;
  int base_level;
  int max_level;
  float min_lod;
  float max_lod;
  Sampler::FilterMode min_filter;
  Sampler::FilterMode mag_filter;
  Texture::Swizzle swizzle_r;
  Texture::Swizzle swizzle_g;
  Texture::Swizzle swizzle_b;
  Texture::Swizzle swizzle_a;
  Sampler::WrapMode wrap_r;
  Sampler::WrapMode wrap_s;
  Sampler::WrapMode wrap_t;
};

struct Data {
  base::DataContainerPtr index_container;
  base::DataContainerPtr image_container;
  base::DataContainerPtr vertex_container;
  AttributeArrayPtr attribute_array;
  BufferObjectPtr vertex_buffer;
  FramebufferObjectPtr fbo;
  IndexBufferPtr index_buffer;
  SamplerPtr sampler;
  ShaderProgramPtr shader;
  ShapePtr shape;
  TexturePtr texture;
  CubeMapTexturePtr cubemap;
  ImagePtr image;
  NodePtr rect;
};

static Data s_data;
static Options s_options;
static const int s_num_indices = 6;
static const int s_num_vertices = 4;
static const Image::Format kDepthBufferFormat = Image::kRenderbufferDepth32f;

static void BuildRectangleBufferObject() {
  if (!s_data.vertex_buffer.Get())
    s_data.vertex_buffer = new BufferObject;
  if (!s_data.vertex_container.Get()) {
    Vertex* vertices = new Vertex[s_num_vertices];
    static const float kHalfSize = 10.f;
    static const float kY = -0.1f;
    vertices[0].point_coords.Set(-kHalfSize, kY,  kHalfSize);
    vertices[0].tex_coords.Set(0.f, 1.f);
    vertices[1].point_coords.Set(kHalfSize, kY,  kHalfSize);
    vertices[1].tex_coords.Set(1.f, 1.f);
    vertices[2].point_coords.Set(kHalfSize, kY, -kHalfSize);
    vertices[2].tex_coords.Set(1.f, 0.f);
    vertices[3].point_coords.Set(-kHalfSize, kY, -kHalfSize);
    vertices[3].tex_coords.Set(0.f, 0.f);
    s_data.vertex_container =
        base::DataContainer::Create<Vertex>(
            vertices, base::DataContainer::ArrayDeleter<Vertex>, false,
            s_data.vertex_buffer->GetAllocator());
  }
  s_data.vertex_buffer->SetData(
      s_data.vertex_container, sizeof(Vertex), s_num_vertices,
      s_options.vertex_buffer_usage);
}

static void BuildNonIndexedRectangleBufferObject() {
  if (!s_data.vertex_buffer.Get()) s_data.vertex_buffer = new BufferObject;
  if (!s_data.vertex_container.Get()) {
    Vertex* vertices = new Vertex[s_num_indices];
    static const float kHalfSize = 10.f;
    static const float kY = -0.1f;
    vertices[0].point_coords.Set(-kHalfSize, kY, kHalfSize);
    vertices[0].tex_coords.Set(0.f, 1.f);
    vertices[1].point_coords.Set(kHalfSize, kY, kHalfSize);
    vertices[1].tex_coords.Set(1.f, 1.f);
    vertices[2].point_coords.Set(kHalfSize, kY, -kHalfSize);
    vertices[2].tex_coords.Set(1.f, 0.f);
    vertices[3] = vertices[0];
    vertices[4] = vertices[2];
    vertices[5].point_coords.Set(-kHalfSize, kY, -kHalfSize);
    vertices[5].tex_coords.Set(0.f, 0.f);

    s_data.vertex_container = base::DataContainer::Create<Vertex>(
        vertices, base::DataContainer::ArrayDeleter<Vertex>, false,
        s_data.vertex_buffer->GetAllocator());
  }
  s_data.vertex_buffer->SetData(s_data.vertex_container, sizeof(Vertex),
                                s_num_indices, s_options.vertex_buffer_usage);
}

static void BuildRectangleAttributeArray() {
  // The attributes have names that are defined in the global registry and so
  // must be set there.
  const ShaderInputRegistryPtr& global_reg =
      ShaderInputRegistry::GetGlobalRegistry();

  s_data.attribute_array = new AttributeArray;
  s_data.attribute_array->AddAttribute(global_reg->Create<Attribute>(
      "aVertex", BufferObjectElement(
          s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
              BufferObject::kFloat, 3, 0))));
  s_data.attribute_array->AddAttribute(global_reg->Create<Attribute>(
      "aTexCoords", BufferObjectElement(
          s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
              BufferObject::kFloat, 2, sizeof(float) * 3))));

  s_data.shape->SetAttributeArray(s_data.attribute_array);
}

static void BuildShape() {
  if (!s_data.shape.Get()) s_data.shape = new Shape;
  s_data.shape->SetPrimitiveType(s_options.primitive_type);
}

template <typename IndexType>
static void BuildRectangleShape() {
  BuildShape();

  if (!s_data.index_buffer.Get())
    s_data.index_buffer = new IndexBuffer;
  if (!s_data.index_container.Get()) {
    // Set up the triangle vertex indices.
    IndexType* indices = new IndexType[s_num_indices];
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;
    if (!s_data.index_container.Get())
      s_data.index_container = ion::base::DataContainer::Create<IndexType>(
          indices, ion::base::DataContainer::ArrayDeleter<IndexType>, false,
          s_data.index_buffer->GetAllocator());
  }

  s_data.index_buffer->SetData(
      s_data.index_container, sizeof(IndexType), s_num_indices,
      s_options.index_buffer_usage);
  s_data.index_buffer->AddSpec(GetComponentType<IndexType>(), 1, 0);

  s_data.shape->SetIndexBuffer(s_data.index_buffer);
}

static void AddDefaultUniformsToNode(const NodePtr& node) {
  node->AddUniform(ShaderInputRegistry::GetGlobalRegistry()->Create<Uniform>(
      "uModelviewMatrix", math::Matrix4f::Identity()));
  node->AddUniform(ShaderInputRegistry::GetGlobalRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));
  node->AddUniform(ShaderInputRegistry::GetGlobalRegistry()->Create<Uniform>(
      "uBaseColor", math::Vector4f::Zero()));
}

static void AddPlaneShaderUniformsToNode(const NodePtr& node) {
  const ShaderInputRegistryPtr& reg = node->GetShaderProgram()->GetRegistry();
  node->AddUniform(reg->Create<Uniform>("uTexture", s_data.texture));
  node->AddUniform(reg->Create<Uniform>("uTexture2", s_data.texture));
  node->AddUniform(reg->Create<Uniform>("uCubeMapTexture", s_data.cubemap));
  node->AddUniform(reg->Create<Uniform>("uModelviewMatrix",
      math::TranslationMatrix(math::Vector3f(-1.5f, 1.5f, 0.0f))));
  node->AddUniform(
      reg->Create<Uniform>("uProjectionMatrix", math::Matrix4f::Identity()));
}

static void SetImages() {
  s_data.texture->SetImage(0U, ImagePtr());
  s_data.texture->SetImage(0U, s_data.image);
  for (int i = 0; i < 6; ++i) {
    s_data.cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(i),
                             0U, ImagePtr());
    s_data.cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(i),
                             0U, s_data.image);
  }
}

static void Build1dArrayImage() {
  if (!s_data.image.Get())
    s_data.image = new Image;
  if (!s_data.image_container.Get()) {
    s_data.image_container = base::DataContainer::CreateOverAllocated<char>(
        65536, NULL, s_data.image->GetAllocator());
  }
  s_data.image->SetArray(s_options.image_format, 32, 32,
                         s_data.image_container);
  SetImages();
}

static void Build2dImage() {
  if (!s_data.image.Get())
    s_data.image = new Image;
  if (!s_data.image_container.Get()) {
    s_data.image_container = base::DataContainer::CreateOverAllocated<char>(
        65536, NULL, s_data.image->GetAllocator());
  }
  s_data.image->Set(s_options.image_format, 32, 32, s_data.image_container);
  SetImages();
}

static void Build2dArrayImage() {
  if (!s_data.image.Get())
    s_data.image = new Image;
  if (!s_data.image_container.Get()) {
    s_data.image_container = base::DataContainer::CreateOverAllocated<char>(
        65536, NULL, s_data.image->GetAllocator());
  }
  s_data.image->SetArray(s_options.image_format, 8, 8, 16,
                         s_data.image_container);
  SetImages();
}

static void Build3dImage() {
  if (!s_data.image.Get())
    s_data.image = new Image;
  if (!s_data.image_container.Get()) {
    s_data.image_container = base::DataContainer::CreateOverAllocated<char>(
        65536, NULL, s_data.image->GetAllocator());
  }
  s_data.image->Set(s_options.image_format, 8, 8, 16, s_data.image_container);
  SetImages();
}

static void BuildEglImage() {
  if (!s_data.image.Get())
    s_data.image = new Image;
  static uint8 kData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                            0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  s_data.image->SetEglImage(
      base::DataContainer::Create<void>(kData, kNullFunction, false,
                                        s_data.image->GetAllocator()));
  SetImages();
}

static void BuildExternalEglImage() {
  if (!s_data.image.Get())
    s_data.image = new Image;
  static uint8 kData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                            0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  s_data.image->SetExternalEglImage(
      base::DataContainer::Create<void>(kData, kNullFunction, false,
                                        s_data.image->GetAllocator()));
  SetImages();
}

static void BuildRectangle(bool is_indexed, bool use_32bit_indices,
                           const char* vertex_shader,
                           const char* fragment_shader) {
  if (!s_data.texture.Get()) {
    s_data.texture = new Texture();
    s_data.texture->SetLabel("Texture");
  }
  if (!s_data.cubemap.Get()) {
    s_data.cubemap = new CubeMapTexture();
    s_data.cubemap->SetLabel("Cubemap Texture");
  }
  if (!s_data.sampler.Get()) {
    s_data.sampler = new Sampler();
    s_data.sampler->SetLabel("Sampler");
  }
  Build2dImage();
  s_data.texture->SetBaseLevel(s_options.base_level);
  s_data.texture->SetMaxLevel(s_options.max_level);
  s_data.texture->SetSwizzleRed(s_options.swizzle_r);
  s_data.texture->SetSwizzleGreen(s_options.swizzle_g);
  s_data.texture->SetSwizzleBlue(s_options.swizzle_b);
  s_data.texture->SetSwizzleAlpha(s_options.swizzle_a);
  s_data.sampler->SetCompareFunction(s_options.compare_func);
  s_data.sampler->SetCompareMode(s_options.compare_mode);
  s_data.sampler->SetMaxAnisotropy(s_options.max_anisotropy);
  s_data.sampler->SetMinLod(s_options.min_lod);
  s_data.sampler->SetMaxLod(s_options.max_lod);
  s_data.sampler->SetMinFilter(s_options.min_filter);
  s_data.sampler->SetMagFilter(s_options.mag_filter);
  s_data.sampler->SetWrapR(s_options.wrap_r);
  s_data.sampler->SetWrapS(s_options.wrap_s);
  s_data.sampler->SetWrapT(s_options.wrap_t);
  s_data.cubemap->SetSampler(s_data.sampler);
  s_data.texture->SetSampler(s_data.sampler);
  if (!s_data.rect.Get()) {
    s_data.rect = new Node;

    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture2"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));

    if (is_indexed) {
      BuildRectangleBufferObject();
      if (use_32bit_indices) {
        BuildRectangleShape<uint32>();
      } else {
        BuildRectangleShape<uint16>();
      }
    } else {
      BuildNonIndexedRectangleBufferObject();
      BuildShape();
    }
    BuildRectangleAttributeArray();
    s_data.rect->AddShape(s_data.shape);
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, vertex_shader,
        fragment_shader, base::AllocatorPtr());
    s_data.rect->SetShaderProgram(s_data.shader);
    AddPlaneShaderUniformsToNode(s_data.rect);

    StateTablePtr state_table(new StateTable());
    state_table->Enable(StateTable::kCullFace, false);
    s_data.rect->SetStateTable(state_table);
  }
}

static void BuildRectangle(bool is_indexed, bool use_32bit_indices) {
  BuildRectangle(is_indexed, use_32bit_indices, kPlaneVertexShaderString,
                 kPlaneFragmentShaderString);
}

static void BuildRectangle() { BuildRectangle(true, false); }

// Returns an array Uniform of the passed name created using the passed registry
// and values to initialize it.
template <typename T>
static Uniform CreateArrayUniform(const ShaderInputRegistryPtr& reg,
                                  const std::string& name,
                                  const std::vector<T>& values) {
  return reg->CreateArrayUniform(name, &values[0], values.size(),
                                 base::AllocatorPtr());
}

static const NodePtr BuildGraph(int width, int height, bool is_indexed,
                                bool use_32bit_indices,
                                const char* vertex_shader,
                                const char* fragment_shader) {
  NodePtr root(new Node);
  // Set up global state.
  StateTablePtr state_table(new StateTable(width, height));
  state_table->SetViewport(
      math::Range2i(math::Point2i(0, 0), math::Point2i(width, height)));
  state_table->SetClearColor(math::Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(0.f);
  state_table->Enable(StateTable::kDepthTest, true);
  state_table->Enable(StateTable::kCullFace, true);
  root->SetStateTable(state_table);

  BuildRectangle(is_indexed, use_32bit_indices, vertex_shader, fragment_shader);
  root->AddChild(s_data.rect);
  root->SetShaderProgram(s_data.shader);
  root->AddUniform(ShaderInputRegistry::GetGlobalRegistry()->Create<Uniform>(
      "uModelviewMatrix", math::Matrix4f::Identity()));
  return root;
}

static const NodePtr BuildGraph(int width, int height, bool is_indexed,
                                bool use_32bit_indices) {
  return BuildGraph(width, height, is_indexed, use_32bit_indices,
                    kPlaneVertexShaderString,
                    kPlaneFragmentShaderString);
}

static const NodePtr BuildGraph(int width, int height, bool use_32bit_indices) {
  return BuildGraph(width, height, true, use_32bit_indices);
}

static const NodePtr BuildGraph(int width, int height) {
  return BuildGraph(width, height, false);
}

// Helper class that encapsulates a varying argument to a function.
template <typename Enum>
struct VaryingArg {
  VaryingArg() : index(0) {}
  VaryingArg(size_t index_in, Enum value_in, const std::string& string_value_in)
      : value(value_in),
        index(index_in),
        string_value(string_value_in) {}
  Enum value;
  size_t index;
  std::string string_value;
};

// Helper class that encapsulates a non-varying argument to a function.
struct StaticArg {
  StaticArg() : index(0) {}
  StaticArg(size_t index_in, const std::string& string_value_in)
      : index(index_in),
        string_value(string_value_in) {}
  size_t index;
  std::string string_value;
};

template <typename Enum>
struct VerifyRenderData {
  VerifyRenderData() : debug(false) {}
  // Function to call to update the scene when value of the Enum changes.
  void (*update_func)();
  // Name of the call in trace_verifier to examine.
  std::string call_name;
  // Arguments to the call that do not change.
  std::vector<StaticArg> static_args;
  std::vector<VaryingArg<Enum> > arg_tests;
  // Option value to change.
  Enum* option;
  // Index of the varying argument.
  size_t varying_arg_index;
  bool debug;
};

// Verifies that a particular function is in the trace stream. All of the static
// arguments in VerifyRenderData must be present, and so must the varying
// argument as the Enum value changes.
template <typename Enum>
static ::testing::AssertionResult VerifyRenderCalls(
    const VerifyRenderData<Enum>& data, testing::TraceVerifier* trace_verifier,
    const RendererPtr& renderer, const NodePtr& root) {
  TracingHelper helper;
  size_t num_static_args = data.static_args.size();
  // Stop one past the end of the enum for error-case handling.
  size_t num_tests = data.arg_tests.size();
  for (size_t i = 0; i < num_tests; ++i) {
    // Update the value.
    *data.option = data.arg_tests[i].value;
    // Update the scene graph with the value.
    data.update_func();
    // Reset call counts and the trace stream.
    MockGraphicsManager::ResetCallCount();
    trace_verifier->Reset();
    // Draw the scene.
    renderer->DrawScene(root);
    if (data.debug)
      std::cerr << trace_verifier->GetTraceString() << "\n";
    // Check the static args are correct.
    testing::TraceVerifier::Call call = trace_verifier->VerifyCallAt(
        trace_verifier->GetNthIndexOf(data.arg_tests[i].index, data.call_name));
    for (size_t j = 0; j < num_static_args; ++j) {
      ::testing::AssertionResult result = call.HasArg(
          data.static_args[j].index, data.static_args[j].string_value);
      if (result != ::testing::AssertionSuccess())
        return result << ". Failure in iteration " << i << ", on call #"
                      << data.arg_tests[i].index << " of " << data.call_name
                      << ", testing static arg " << data.static_args[j].index
                      << " (" << data.static_args[j].string_value << ")";
    }
    // Check that the varying arg is correct.
    ::testing::AssertionResult result =
        call.HasArg(data.varying_arg_index, data.arg_tests[i].string_value);
    if (result != ::testing::AssertionSuccess())
      return result << ". Failure in iteration " << i << ", on call #"
                    << data.arg_tests[i].index << " of " << data.call_name
                    << ", testing varying arg " << data.varying_arg_index
                    << " (" << data.arg_tests[i].string_value << ")";
  }
  // Restore the initial value.
  *data.option = data.arg_tests[0].value;
  data.update_func();
  return ::testing::AssertionSuccess();
}

// Checks that Renderer catches failed certain cases. We draw to a framebuffer
// to also catch framebuffer and renderbuffer errors.
static ::testing::AssertionResult VerifyFunctionFailure(
    const MockGraphicsManagerPtr& gm, const std::string& func_name,
    const std::string& error_msg) {
  base::LogChecker log_checker;
  gm->SetForceFunctionFailure(func_name, true);
  {
    FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
    fbo->SetColorAttachment(0U,
                            FramebufferObject::Attachment(Image::kRgba4Byte));

    NodePtr root = BuildGraph(800, 800);
    RendererPtr renderer(new Renderer(gm));
    renderer->BindFramebuffer(fbo);
    renderer->DrawScene(root);
    renderer->BindFramebuffer(FramebufferObjectPtr());
  }
  gm->SetForceFunctionFailure(func_name, false);
  // We purposefully induced a GL error above.
  gm->SetErrorCode(GL_NO_ERROR);
  if (log_checker.HasMessage("ERROR", error_msg))
    return ::testing::AssertionSuccess();
  else
    return ::testing::AssertionFailure() << "Expected that disabling "
                                         << func_name
                                         << " generates the message \""
                                         << error_msg << "\"";
}

template <typename TextureType>
::testing::AssertionResult VerifyImmutableTexture(
    const RendererPtr& renderer, testing::TraceVerifier* trace_verifier,
    size_t levels, const std::string& base_call) {
  typename base::ReferentPtr<TextureType>::Type texture(new TextureType);
  texture->SetImmutableImage(s_data.image, levels);
  texture->SetSampler(s_data.sampler);
  renderer->CreateOrUpdateResource(texture.Get());
  const std::string call = base_call + base::ValueToString(levels);
  if (trace_verifier->GetCountOf(call) != 1U)
    return ::testing::AssertionFailure() << "There should be one call to "
                                         << call;
  if (trace_verifier->GetCountOf("TexImage"))
    return ::testing::AssertionFailure()
           << "There should be no calls to TexImage*";
  return ::testing::AssertionSuccess();
}

template <typename TextureType>
::testing::AssertionResult VerifyImmutableMultisampledTexture(
    const RendererPtr& renderer, testing::TraceVerifier* trace_verifier,
    int samples, const std::string& base_call) {
  typename base::ReferentPtr<TextureType>::Type texture(new TextureType);
  texture->SetImmutableImage(s_data.image, 1);
  if (samples > 0) {
    texture->SetMultisampling(samples, true);
  }
  texture->SetSampler(s_data.sampler);
  renderer->CreateOrUpdateResource(texture.Get());
  const std::string call = base_call + base::ValueToString(samples);
  if (trace_verifier->GetCountOf(call) != 1U)
    return ::testing::AssertionFailure() << "There should be one call to "
                                         << call;
  if (trace_verifier->GetCountOf("TexImage"))
    return ::testing::AssertionFailure()
           << "There should be no calls to TexImage*";
  return ::testing::AssertionSuccess();
}

static ImagePtr CreateNullImage(uint32 width, uint32 height,
                                Image::Format format) {
  ImagePtr image(new Image);
  image->Set(format, width, height, base::DataContainerPtr(NULL));
  return image;
}

static void PopulateUniformValues(const NodePtr& node,
                                  const UniformBlockPtr& block1,
                                  const UniformBlockPtr& block2,
                                  const ShaderInputRegistryPtr& reg,
                                  int offset) {
  const float foffset = static_cast<float>(offset);
  node->ClearUniforms();
  block1->ClearUniforms();
  block2->ClearUniforms();
  block1->AddUniform(reg->Create<Uniform>("uInt", 13 + offset));
  block1->AddUniform(reg->Create<Uniform>("uFloat", 1.5f + foffset));
  block2->AddUniform(
      reg->Create<Uniform>("uFV2", math::Vector2f(2.f + foffset, 3.f)));
  block2->AddUniform(
      reg->Create<Uniform>("uFV3", math::Vector3f(4.f + foffset, 5.f, 6.f)));
  block2->AddUniform(reg->Create<Uniform>(
      "uFV4", math::Vector4f(7.f + foffset, 8.f, 9.f, 10.f)));
  node->AddUniform(reg->Create<Uniform>("uIV2",
                                        math::Vector2i(2 + offset, 3)));
  node->AddUniform(reg->Create<Uniform>("uIV3",
                                        math::Vector3i(4 + offset, 5, 6)));
  node->AddUniform(reg->Create<Uniform>("uIV4",
                                        math::Vector4i(7 + offset, 8, 9, 10)));
  node->AddUniform(reg->Create<Uniform>("uMat2",
                                        math::Matrix2f(1.f + foffset, 2.f,
                                                       3.f, 4.f)));
  node->AddUniform(reg->Create<Uniform>("uMat3",
                                        math::Matrix3f(1.f + foffset, 2.f, 3.f,
                                                       4.f, 5.f, 6.f,
                                                       7.f, 8.f, 9.f)));
  node->AddUniform(reg->Create<Uniform>("uMat4",
                                        math::Matrix4f(1.f + foffset, 2.f, 3.f,
                                                       4.f, 5.f, 6.f, 7.f, 8.f,
                                                       9.f, 1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f, 7.f)));
}

static ::testing::AssertionResult VerifyUniformCounts(
    size_t count,
    testing::TraceVerifier* trace_verifier) {
  if (count != trace_verifier->GetCountOf("Uniform1i"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to Uniform1i";
  if (count != trace_verifier->GetCountOf("Uniform1f"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to Uniform1f";
  if (count != trace_verifier->GetCountOf("Uniform2fv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to Uniform2fv";
  if (count != trace_verifier->GetCountOf("Uniform3fv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to Uniform3fv";
  if (count != trace_verifier->GetCountOf("Uniform4fv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to Uniform4fv";
  if (count != trace_verifier->GetCountOf("Uniform2iv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to Uniform2iv";
  if (count != trace_verifier->GetCountOf("Uniform3iv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to Uniform3iv";
  if (count != trace_verifier->GetCountOf("Uniform4iv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to Uniform4iv";
  if (count != trace_verifier->GetCountOf("UniformMatrix2fv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to UniformMatrix2fv";
  if (count != trace_verifier->GetCountOf("UniformMatrix3fv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to UniformMatrix3fv";
  if (count != trace_verifier->GetCountOf("UniformMatrix4fv"))
    return ::testing::AssertionFailure() << "Expected " << count
                                         << " calls to UniformMatrix4fv";
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifySaveAndRestoreFlag(
    const GraphicsManagerPtr& gm, const RendererPtr& renderer,
    testing::TraceVerifier* trace_verifier, Renderer::Flag save_flag,
    Renderer::Flag restore_flag, const std::string& save_call,
    const std::string& restore_call) {
  NodePtr root = BuildGraph(800, 800);

  // Framebuffer handling is special, we want to test without the requested fbo
  // first, and bind it again before restoring.
  FramebufferObjectPtr fbo = renderer->GetCurrentFramebuffer();
  if (fbo.Get() != NULL)
    renderer->BindFramebuffer(FramebufferObjectPtr());

  // Test that saving alone works.
  renderer->ClearFlag(restore_flag);
  renderer->SetFlag(save_flag);
  trace_verifier->Reset();
  renderer->DrawScene(NodePtr());
  if (trace_verifier->GetCountOf(save_call) != 1U)
    return ::testing::AssertionFailure(
        ::testing::Message() << "There should be one call to " << save_call
                             << " with the save flag set!");
  if (trace_verifier->GetCountOf(restore_call))
    return ::testing::AssertionFailure(
        ::testing::Message() << "There should be no calls to " << restore_call
                             << " with the save flag set!");
  renderer->ClearFlag(save_flag);

  // Without either flag nothing happens.
  trace_verifier->Reset();
  renderer->DrawScene(NodePtr());
  if (trace_verifier->GetCountOf(save_call))
    return ::testing::AssertionFailure(::testing::Message()
                                       << "There should be no calls to "
                                       << save_call << " with no flags set!");
  if (trace_verifier->GetCountOf(restore_call))
    return ::testing::AssertionFailure(
        ::testing::Message() << "There should be no calls to " << restore_call
                             << " with no flags set!");

  // Draw something that modifies the state internally so that a restore will
  // be needed.
  renderer->DrawScene(root);

  // Test that restoring works.
  if (fbo.Get() != NULL)
    renderer->BindFramebuffer(fbo);
  trace_verifier->Reset();
  renderer->SetFlag(restore_flag);
  renderer->DrawScene(NodePtr());
  if (trace_verifier->GetCountOf(save_call))
    return ::testing::AssertionFailure(
        ::testing::Message() << "There should be no calls to " << save_call
                             << " with the restore flag set!");
  if (trace_verifier->GetCountOf(restore_call) != 1U)
    return ::testing::AssertionFailure(
        ::testing::Message() << "There should be one call to " << restore_call
                             << " with the restore flag set! Trace: ");

  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyAllSaveAndRestoreFlags(
    const GraphicsManagerPtr& gm, const RendererPtr& renderer) {
  gm->Viewport(1, 2, 3, 4);
  gm->ClearColor(.1f, .2f, .3f, .4f);
  gm->Scissor(1, 2, 3, 4);

  GLuint texture;
  gm->GenTextures(1, &texture);
  EXPECT_NE(0U, texture);
  GLuint buffer[4];
  gm->GenBuffers(4, buffer);
  EXPECT_NE(0U, buffer[2]);
  EXPECT_NE(0U, buffer[3]);
  EXPECT_NE(buffer[2], buffer[3]);

  gm->ActiveTexture(GL_TEXTURE1);

  GLint length = static_cast<GLint>(strlen(kPlaneVertexShaderString));
  GLuint vertex_shader = gm->CreateShader(GL_VERTEX_SHADER);
  gm->ShaderSource(vertex_shader, 1, &kPlaneVertexShaderString, &length);

  gm->CompileShader(vertex_shader);
  GLint is_compiled = 0;
  gm->GetShaderiv(vertex_shader, GL_COMPILE_STATUS, &is_compiled);
  EXPECT_EQ(GL_TRUE, is_compiled);

  length = static_cast<GLint>(strlen(kPlaneFragmentShaderString));
  GLuint fragment_shader = gm->CreateShader(GL_FRAGMENT_SHADER);
  gm->ShaderSource(fragment_shader, 1, &kPlaneFragmentShaderString, &length);

  gm->CompileShader(fragment_shader);
  gm->GetShaderiv(fragment_shader, GL_COMPILE_STATUS, &is_compiled);

  EXPECT_EQ(GL_TRUE, is_compiled);

  GLuint program = gm->CreateProgram();

  gm->AttachShader(program, vertex_shader);
  gm->AttachShader(program, fragment_shader);
  gm->LinkProgram(program);
  GLint is_linked;
  gm->GetProgramiv(program, GL_LINK_STATUS, &is_linked);
  EXPECT_EQ(GL_TRUE, is_linked);

  GLint binding;
  gm->UseProgram(program);
  gm->GetIntegerv(GL_CURRENT_PROGRAM, &binding);
  EXPECT_EQ(program, static_cast<GLuint>(binding));

  gm->Enable(GL_CULL_FACE);
  gm->Enable(GL_SCISSOR_TEST);
  gm->Enable(GL_DEPTH_TEST);

  gm->BindBuffer(GL_ARRAY_BUFFER, buffer[2]);
  gm->BufferData(GL_ARRAY_BUFFER, sizeof(GLuint), &buffer[2], GL_STATIC_DRAW);
  gm->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer[3]);
  gm->BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint), &buffer[3],
                 GL_STATIC_DRAW);

  NodePtr root = BuildGraph(800, 800);
  renderer->ClearFlags(Renderer::AllFlags());
  renderer->SetFlags(Renderer::AllSaveFlags() | Renderer::AllRestoreFlags());
  renderer->DrawScene(root);

  gm->GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &binding);
  EXPECT_EQ(buffer[3], static_cast<GLuint>(binding));
  gm->GetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
  EXPECT_EQ(buffer[2], static_cast<GLuint>(binding));

  gm->GetIntegerv(GL_ACTIVE_TEXTURE, &binding);
  EXPECT_EQ(GL_TEXTURE1, binding);

  gm->GetIntegerv(GL_CURRENT_PROGRAM, &binding);
  EXPECT_EQ(program, static_cast<GLuint>(binding));

  EXPECT_TRUE(gm->IsEnabled(GL_DEPTH_TEST));
  EXPECT_TRUE(gm->IsEnabled(GL_SCISSOR_TEST));
  EXPECT_TRUE(gm->IsEnabled(GL_CULL_FACE));

  GLint scissor[4];
  gm->GetIntegerv(GL_SCISSOR_BOX, scissor);
  for (int i = 0; i < 4; i++)
    EXPECT_EQ(i + 1, scissor[i]);

  GLfloat color[4];
  gm->GetFloatv(GL_COLOR_CLEAR_VALUE, color);
  for (int i = 0; i < 4; i++)
    EXPECT_EQ(static_cast<float>(i + 1) * 0.1f, color[i]);

  GLint viewport[4];
  gm->GetIntegerv(GL_VIEWPORT, viewport);
  for (int i = 0; i < 4; i++)
    EXPECT_EQ(i + 1, viewport[i]);

  // Try again with values that can't be made unique (e.g. true/false values).

  gm->Disable(GL_CULL_FACE);
  gm->Disable(GL_SCISSOR_TEST);
  gm->Disable(GL_DEPTH_TEST);
  gm->ActiveTexture(GL_TEXTURE2);

  renderer->DrawScene(root);

  gm->GetIntegerv(GL_ACTIVE_TEXTURE, &binding);
  EXPECT_EQ(GL_TEXTURE2, binding);
  EXPECT_FALSE(gm->IsEnabled(GL_DEPTH_TEST));
  EXPECT_FALSE(gm->IsEnabled(GL_SCISSOR_TEST));
  EXPECT_FALSE(gm->IsEnabled(GL_CULL_FACE));

  return ::testing::AssertionSuccess();
}

// Helper functions for updating resources on a worker thread.
template <typename T>
static bool UploadThread(const RendererPtr& renderer,
                         MockVisual* visual,
                         T* holder) {
  // Set the Visual for this thread.
  portgfx::Visual::MakeCurrent(visual);
  renderer->CreateOrUpdateResource(holder);
  return true;
}
template <>
bool UploadThread<ShapePtr>(const RendererPtr& renderer, MockVisual* visual,
                            ShapePtr* shapeptr) {
  // Set the Visual for this thread.
  portgfx::Visual::MakeCurrent(visual);
  renderer->CreateOrUpdateShapeResources(*shapeptr);
  return true;
}

static bool RenderingThread(const RendererPtr& renderer, MockVisual* visual,
                            NodePtr* nodeptr) {
  portgfx::Visual::MakeCurrent(visual);
  renderer->DrawScene(*nodeptr);
  return true;
}

static bool UniformThread(const RendererPtr& renderer, MockVisual* visual,
                          const NodePtr& node, size_t uindex, float uvalue,
                          std::vector<ResourceManager::ProgramInfo>* infos) {
  portgfx::Visual::MakeCurrent(visual);
  ResourceManager* manager = renderer->GetResourceManager();
  CallbackHelper<ResourceManager::ProgramInfo> callback;
  manager->RequestAllResourceInfos<ShaderProgram, ResourceManager::ProgramInfo>(
      std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                &callback, std::placeholders::_1));
  node->SetUniformValue(uindex, uvalue);
  renderer->DrawScene(node);
  *infos = callback.infos;
  return true;
}

class RendererTest : public ::testing::Test {
 public:
  ::testing::AssertionResult VerifyReleases(int times) {
    // Check that resources are released properly.
    std::vector<std::string> call_strings;
    for (int i = 0; i < times; ++i) {
      std::vector<std::string> strings = GetReleaseStrings();
      call_strings.insert(call_strings.end(), strings.begin(), strings.end());
    }
    std::sort(call_strings.begin(), call_strings.end());
    ::testing::AssertionResult result =
        trace_verifier_->VerifySortedCalls(call_strings);
    Reset();
    return result;
  }

  const MockGraphicsManagerPtr& GetGraphicsManager() const { return gm_; }
  testing::TraceVerifier* GetTraceVerifier() const { return trace_verifier_; }

 protected:
  void SetUp() override {
    // Use a special allocator to track certain kinds of memory overwrites. This
    // is quite useful since the Renderer has many internal pointers to state
    // holders, and it is a good idea to ensure we don't try to read from or
    // write to one that has already been destroyed.
    static const size_t kMemorySize = 1024 * 1024;
    saved_[0] = base::AllocationManager::GetDefaultAllocatorForLifetime(
        base::kShortTerm);
    saved_[1] = base::AllocationManager::GetDefaultAllocatorForLifetime(
        base::kMediumTerm);
    saved_[2] = base::AllocationManager::GetDefaultAllocatorForLifetime(
        base::kLongTerm);

    base::AllocationManager::SetDefaultAllocatorForLifetime(
        base::kShortTerm,
        base::AllocatorPtr(new base::testing::BadWriteCheckingAllocator(
            kMemorySize, saved_[0])));
    base::AllocationManager::SetDefaultAllocatorForLifetime(
        base::kMediumTerm,
        base::AllocatorPtr(new base::testing::BadWriteCheckingAllocator(
            kMemorySize, saved_[1])));
    base::AllocationManager::SetDefaultAllocatorForLifetime(
        base::kLongTerm,
        base::AllocatorPtr(new base::testing::BadWriteCheckingAllocator(
            kMemorySize, saved_[2])));

    visual_.reset(new MockVisual(kWidth, kHeight));
    gm_.Reset(new MockGraphicsManager());
    trace_verifier_ = new testing::TraceVerifier(gm_.Get());
    Reset();
  }

  void TearDown() override {
    s_data.index_container = NULL;
    s_data.image_container = NULL;
    s_data.vertex_container = NULL;
    s_data.attribute_array = NULL;
    s_data.vertex_buffer = NULL;
    s_data.fbo = NULL;
    s_data.index_buffer = NULL;
    s_data.sampler = NULL;
    s_data.shader = NULL;
    s_data.shape = NULL;
    s_data.texture = NULL;
    s_data.cubemap = NULL;
    s_data.image = NULL;
    s_data.rect = NULL;
    delete trace_verifier_;
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gm_->GetError());
    gm_.Reset(NULL);
    visual_.reset();
    s_options = Options();
    // Clear singly logged messages.
    base::logging_internal::SingleLogger::ClearMessages();

    // The BadWriteCheckingAllocators will log messages if there are any
    // overwrites.
    base::LogChecker log_checker;
    base::AllocationManager::SetDefaultAllocatorForLifetime(base::kShortTerm,
                                                            saved_[0]);
    base::AllocationManager::SetDefaultAllocatorForLifetime(base::kMediumTerm,
                                                            saved_[1]);
    base::AllocationManager::SetDefaultAllocatorForLifetime(base::kLongTerm,
                                                            saved_[2]);
    // There should be no messages.
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  // Resets the call count and invokes the UpdateStateTable() call.
  void Reset() {
    MockGraphicsManager::ResetCallCount();
    trace_verifier_->Reset();
    s_msg_stream_.str("");
    s_msg_stream_.clear();
  }

  const std::vector<std::string> GetReleaseStrings() {
    std::vector<std::string> call_strings;
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteProgram");
    if (gm_->IsFunctionGroupAvailable(GraphicsManager::kSamplerObjects))
      call_strings.push_back("DeleteSampler");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteVertexArrays");
    return call_strings;
  }

  std::unique_ptr<MockVisual> visual_;
  MockGraphicsManagerPtr gm_;
  testing::TraceVerifier* trace_verifier_;
  base::AllocatorPtr saved_[3];

  static const int kWidth = 400;
  static const int kHeight = 300;
  static std::stringstream s_msg_stream_;
};

std::stringstream RendererTest::s_msg_stream_("");

template <typename T>
::testing::AssertionResult VerifySamplerAndTextureTypedCalls(
    RendererTest* test, VerifyRenderData<T>* data,
    const std::string& texture_func, const std::string& sampler_func) {
  data->update_func = BuildRectangle;
  data->varying_arg_index = 3U;

  const MockGraphicsManagerPtr& gm = test->GetGraphicsManager();
  testing::TraceVerifier* trace_verifier = test->GetTraceVerifier();

  gm->EnableFunctionGroup(GraphicsManager::kSamplerObjects, false);
  {
    NodePtr root = BuildGraph(800, 800);
    RendererPtr renderer(new Renderer(gm));
    data->call_name = texture_func;
    data->static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
    ::testing::AssertionResult result =
        VerifyRenderCalls(*data, trace_verifier, renderer, root);
    if (result != ::testing::AssertionSuccess())
      return result << ". Failed while testing \"" << texture_func << "\"";
    MockGraphicsManager::ResetCallCount();
    trace_verifier->Reset();
  }
  // Note that ::testing::AssertionResult has no assignment operator, so it must
  // be scoped.
  {
    ::testing::AssertionResult result = test->VerifyReleases(1);
    if (result != ::testing::AssertionSuccess())
      return result;
  }

  gm->EnableFunctionGroup(GraphicsManager::kSamplerObjects, true);
  {
    NodePtr root = BuildGraph(800, 800);
    RendererPtr renderer(new Renderer(gm));
    data->call_name = sampler_func;
    // Instead of GL_TEXTURE_2D use the ID of the sampler.
    data->static_args[1] = StaticArg(1, "0x1");
    ::testing::AssertionResult result =
        VerifyRenderCalls(*data, trace_verifier, renderer, root);
    if (result != ::testing::AssertionSuccess())
      return result << ". Failed while testing \"" << sampler_func << "\"";
    MockGraphicsManager::ResetCallCount();
    trace_verifier->Reset();
  }
  // Note that ::testing::AssertionResult has no assignment operator, so it must
  // be scoped.
  {
    ::testing::AssertionResult result = test->VerifyReleases(1);
    if (result != ::testing::AssertionSuccess())
      return result;
  }

  return ::testing::AssertionSuccess();
}

// Calls VerifySamplerAndTextureTypedCalls expecting integer-type arguments.
template <typename T>
::testing::AssertionResult VerifySamplerAndTextureCalls(
    RendererTest* test, VerifyRenderData<T>* data) {
  return VerifySamplerAndTextureTypedCalls(
      test, data, "TexParameteri", "SamplerParameteri");
}
// Specialize for floats.
template <>
::testing::AssertionResult VerifySamplerAndTextureCalls<float>(
    RendererTest* test, VerifyRenderData<float>* data) {
  return VerifySamplerAndTextureTypedCalls(
      test, data, "TexParameterf", "SamplerParameterf");
}

::testing::AssertionResult VerifyGpuMemoryUsage(
    const RendererPtr& renderer, size_t buffer_usage, size_t framebuffer_usage,
    size_t texture_usage) {
  if (renderer->GetGpuMemoryUsage(Renderer::kAttributeArray))
    return ::testing::AssertionFailure()
           << "AttributeArrays should not use memory!";
  if (renderer->GetGpuMemoryUsage(Renderer::kBufferObject) != buffer_usage)
    return ::testing::AssertionFailure()
           << "Buffer usage should be " << buffer_usage << " but it is "
           << renderer->GetGpuMemoryUsage(Renderer::kBufferObject);
  if (renderer->GetGpuMemoryUsage(Renderer::kFramebufferObject) !=
      framebuffer_usage)
    return ::testing::AssertionFailure()
           << "Framebuffer usage should be " << framebuffer_usage
           << " but it is "
           << renderer->GetGpuMemoryUsage(Renderer::kFramebufferObject);
  if (renderer->GetGpuMemoryUsage(Renderer::kSampler))
    return ::testing::AssertionFailure()
           << "Samplers should not use memory!";
  if (renderer->GetGpuMemoryUsage(Renderer::kShaderInputRegistry))
    return ::testing::AssertionFailure()
           << "ShaderInputRegistries should not use memory!";
  if (renderer->GetGpuMemoryUsage(Renderer::kShaderProgram))
    return ::testing::AssertionFailure()
           << "ShaderPrograms should not use memory!";
  if (renderer->GetGpuMemoryUsage(Renderer::kShader))
    return ::testing::AssertionFailure() << "Shaders should not use memory!";
  if (renderer->GetGpuMemoryUsage(Renderer::kTexture) != texture_usage)
    return ::testing::AssertionFailure()
           << "Texture usage should be " << texture_usage << " but it is "
           << renderer->GetGpuMemoryUsage(Renderer::kTexture);
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyClearFlag(const GraphicsManagerPtr& gm,
                                           Renderer::Flag flag,
                                           GLenum enum_name,
                                           GLint expected_value) {
  NodePtr root = BuildGraph(800, 800);
  RendererPtr renderer(new Renderer(gm));
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  renderer->BindFramebuffer(fbo);

  renderer->ClearFlag(flag);
  renderer->DrawScene(root);
  // Check that the initial value is not the expected value.
  GLint value;
  gm->GetIntegerv(enum_name, &value);
  if (expected_value == value)
    return ::testing::AssertionFailure()
           << "Post-render value was equal to cleared value";
  renderer->SetFlag(flag);
  renderer->DrawScene(root);
  // Check that the expected value is set.
  gm->GetIntegerv(enum_name, &value);
  if (expected_value != value)
    return ::testing::AssertionFailure()
           << "Post-clear value not equal to expected value";

  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyClearImageUnitFlag(
    const GraphicsManagerPtr& gm, Renderer::Flag flag, GLenum enum_name,
    GLint expected_value) {
  NodePtr root = BuildGraph(800, 800);
  // Get the number of image units.
  GLint count;
  gm->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &count);

  RendererPtr renderer(new Renderer(gm));
  renderer->ClearFlag(flag);
  renderer->DrawScene(root);
  // Check that at least one image unit has a non-expected value.
  GLint value;
  bool non_expected_was_set = false;
  for (GLint i = 0; i < count; ++i) {
    gm->ActiveTexture(GL_TEXTURE0 + i);
    gm->GetIntegerv(enum_name, &value);
    if (value != expected_value) {
      non_expected_was_set = true;
      break;
    }
  }
  if (!non_expected_was_set)
    return ::testing::AssertionFailure()
           << "Post-render value was equal to cleared value";

  renderer->SetFlag(flag);
  renderer->DrawScene(root);
  // Check that all image units has the expected value.
  bool expected_was_set = true;
  for (GLint i = 0; i < count; ++i) {
    gm->ActiveTexture(GL_TEXTURE0 + i);
    gm->GetIntegerv(enum_name, &value);
    if (value != expected_value) {
      expected_was_set = false;
      break;
    }
  }
  if (!expected_was_set)
    return ::testing::AssertionFailure()
           << "Post-clear value not equal to expected value";

  return ::testing::AssertionSuccess();
}

// Multiplies two matrices together.
static const Uniform CombineMatrices(const Uniform& old_value,
                                     const Uniform& new_value) {
  DCHECK_EQ(kMatrix4x4Uniform, old_value.GetType());
  DCHECK_EQ(kMatrix4x4Uniform, new_value.GetType());

  const math::Matrix4f& m0 = old_value.GetValue<math::Matrix4f>();
  const math::Matrix4f& m1 = new_value.GetValue<math::Matrix4f>();

  Uniform result = old_value;
  result.SetValue(m0 * m1);
  return result;
}

// Extracts three floats that are the translation of a 4x4 matrix matrix. Note
// that this is just an illustrative example of using a GenerateFunction.
static std::vector<Uniform> ExtractTranslation(const Uniform& current) {
  DCHECK_EQ(kMatrix4x4Uniform, current.GetType());

  const math::Matrix4f mat = current.GetValue<math::Matrix4f>();
  const math::Vector3f trans(mat[0][3], mat[1][3], mat[2][3]);
  const ShaderInputRegistry& reg = current.GetRegistry();
  std::vector<Uniform> uniforms;
  uniforms.push_back(reg.Create<Uniform>("uTranslationX", trans[0]));
  uniforms.push_back(reg.Create<Uniform>("uTranslationY", trans[1]));
  uniforms.push_back(reg.Create<Uniform>("uTranslationZ", trans[2]));
  return uniforms;
}

}  // anonymous namespace

TEST_F(RendererTest, GetGraphicsManager) {
  RendererPtr renderer(new Renderer(gm_));
  EXPECT_EQ(static_cast<GraphicsManagerPtr>(gm_),
            renderer->GetGraphicsManager());
}

TEST_F(RendererTest, GetDefaultShaderProgram) {
  RendererPtr renderer(new Renderer(gm_));
  EXPECT_TRUE(renderer->GetDefaultShaderProgram().Get() != NULL);
}

TEST_F(RendererTest, UpdateDefaultFramebufferFromOpenGL) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);

  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));

  GLuint system_fb, bound_fb;
  // Get the system framebuffer.
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING,
                   reinterpret_cast<GLint*>(&system_fb));
  renderer->BindFramebuffer(fbo);
  // Binding the framebuffer should make it active.
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_NE(system_fb, bound_fb);
  renderer->DrawScene(root);

  // Unbinding the framebuffer should go back to the system default.
  renderer->BindFramebuffer(FramebufferObjectPtr());
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_EQ(system_fb, bound_fb);

  // Create a framebuffer outside of Ion.
  GLuint fb;
  gm_->GenFramebuffers(1, &fb);
  EXPECT_GT(fb, 0U);
  gm_->BindFramebuffer(GL_FRAMEBUFFER, fb);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_EQ(fb, bound_fb);

  // Since we haven't updated the default binding it will be blown away.
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(root);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_NE(system_fb, bound_fb);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  // The original framebuffer should be restored.
  EXPECT_EQ(system_fb, bound_fb);

  // Bind the non-Ion fbo.
  gm_->BindFramebuffer(GL_FRAMEBUFFER, fb);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_EQ(fb, bound_fb);
  // Tell the renderer to update its binding.
  renderer->ClearCachedBindings();
  renderer->UpdateDefaultFramebufferFromOpenGL();
  // Binding the Ion fbo will change the binding, but it should be restored
  // later.
  renderer->BindFramebuffer(fbo);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  EXPECT_NE(fb, bound_fb);
  EXPECT_NE(system_fb, bound_fb);
  renderer->DrawScene(root);
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  // The Ion fbo should still be bound.
  EXPECT_NE(fb, bound_fb);
  EXPECT_NE(system_fb, bound_fb);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  gm_->GetIntegerv(GL_FRAMEBUFFER_BINDING, reinterpret_cast<GLint*>(&bound_fb));
  // The renderer should have restored the new framebuffer.
  EXPECT_EQ(fb, bound_fb);
}

TEST_F(RendererTest, UpdateStateFromOpenGL) {
  RendererPtr renderer(new Renderer(gm_));

  // Verify the default StateTable matches the default OpenGL state.
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(0U, st.GetSetCapabilityCount());
    EXPECT_EQ(0U, st.GetSetValueCount());
  }

  // Modify the mock OpenGL state and try again.
  gm_->Enable(GL_SCISSOR_TEST);
  gm_->Enable(GL_STENCIL_TEST);
  gm_->DepthFunc(GL_GREATER);
  gm_->Viewport(2, 10, 120, 432);
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(2U, st.GetSetCapabilityCount());
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kStencilTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kStencilTest));
    EXPECT_EQ(2U, st.GetSetValueCount());
    EXPECT_EQ(StateTable::kDepthGreater, st.GetDepthFunction());
    EXPECT_EQ(math::Range2i(math::Point2i(2, 10), math::Point2i(122, 442)),
              st.GetViewport());
  }

  // Modify some more OpenGL state and try again.
  gm_->Enable(GL_BLEND);
  gm_->FrontFace(GL_CW);
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(3U, st.GetSetCapabilityCount());
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kBlend));
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsCapabilitySet(StateTable::kStencilTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kStencilTest));
    EXPECT_EQ(3U, st.GetSetValueCount());
    EXPECT_EQ(StateTable::kDepthGreater, st.GetDepthFunction());
    EXPECT_EQ(StateTable::kClockwise, st.GetFrontFaceMode());
    EXPECT_EQ(math::Range2i(math::Point2i(2, 10), math::Point2i(122, 442)),
              st.GetViewport());
  }

  // Modify all of the state for a full test.
  gm_->Enable(GL_CULL_FACE);
  gm_->Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  gm_->Enable(GL_DEPTH_TEST);
  gm_->Disable(GL_DITHER);
  gm_->Enable(GL_POLYGON_OFFSET_FILL);
  gm_->Enable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  gm_->Enable(GL_SAMPLE_COVERAGE);
  gm_->Enable(GL_SCISSOR_TEST);
  gm_->Enable(GL_STENCIL_TEST);
  gm_->BlendColor(.2f, .3f, .4f, .5f);
  gm_->BlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT);
  gm_->BlendFuncSeparate(GL_ONE_MINUS_CONSTANT_COLOR, GL_DST_COLOR,
                        GL_ONE_MINUS_CONSTANT_ALPHA, GL_DST_ALPHA);
  gm_->ClearColor(.5f, .6f, .7f, .8f);
  gm_->ClearDepthf(0.5f);
  gm_->ColorMask(true, false, true, false);
  gm_->CullFace(GL_FRONT_AND_BACK);
  gm_->DepthFunc(GL_GEQUAL);
  gm_->DepthRangef(0.2f, 0.7f);
  gm_->DepthMask(false);
  gm_->FrontFace(GL_CW);
  gm_->Hint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);
  gm_->LineWidth(0.4f);
  gm_->PolygonOffset(0.4f, 0.2f);
  gm_->SampleCoverage(0.5f, true);
  gm_->Scissor(4, 10, 123, 234);
  gm_->StencilFuncSeparate(GL_FRONT, GL_LEQUAL, 100, 0xbeefbeefU);
  gm_->StencilFuncSeparate(GL_BACK, GL_GREATER, 200, 0xfacefaceU);
  gm_->StencilMaskSeparate(GL_FRONT, 0xdeadfaceU);
  gm_->StencilMaskSeparate(GL_BACK, 0xcacabeadU);
  gm_->StencilOpSeparate(GL_FRONT, GL_REPLACE, GL_INCR, GL_INVERT);
  gm_->StencilOpSeparate(GL_BACK, GL_INCR_WRAP, GL_DECR_WRAP, GL_ZERO);
  gm_->ClearStencil(123);
  gm_->Viewport(16, 49, 220, 317);
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    const StateTable& st = renderer->GetStateTable();
    EXPECT_EQ(10U, st.GetSetCapabilityCount());
    EXPECT_TRUE(st.IsEnabled(StateTable::kBlend));
    EXPECT_TRUE(st.IsEnabled(StateTable::kCullFace));
    EXPECT_TRUE(st.IsEnabled(StateTable::kDebugOutputSynchronous));
    EXPECT_TRUE(st.IsEnabled(StateTable::kDepthTest));
    EXPECT_FALSE(st.IsEnabled(StateTable::kDither));
    EXPECT_TRUE(st.IsEnabled(StateTable::kPolygonOffsetFill));
    EXPECT_TRUE(st.IsEnabled(StateTable::kSampleAlphaToCoverage));
    EXPECT_TRUE(st.IsEnabled(StateTable::kSampleCoverage));
    EXPECT_TRUE(st.IsEnabled(StateTable::kScissorTest));
    EXPECT_TRUE(st.IsEnabled(StateTable::kStencilTest));
    EXPECT_EQ(math::Vector4f(.2f, .3f, .4f, .5f), st.GetBlendColor());
    EXPECT_EQ(StateTable::kSubtract, st.GetRgbBlendEquation());
    EXPECT_EQ(StateTable::kReverseSubtract, st.GetAlphaBlendEquation());
    EXPECT_EQ(StateTable::kOneMinusConstantColor,
              st.GetRgbBlendFunctionSourceFactor());
    EXPECT_EQ(StateTable::kDstColor, st.GetRgbBlendFunctionDestinationFactor());
    EXPECT_EQ(StateTable::kOneMinusConstantAlpha,
              st.GetAlphaBlendFunctionSourceFactor());
    EXPECT_EQ(StateTable::kDstAlpha,
              st.GetAlphaBlendFunctionDestinationFactor());
    EXPECT_EQ(math::Vector4f(.5f, .6f, .7f, .8f), st.GetClearColor());
    EXPECT_EQ(0.5f, st.GetClearDepthValue());
    EXPECT_TRUE(st.GetRedColorWriteMask());
    EXPECT_FALSE(st.GetGreenColorWriteMask());
    EXPECT_TRUE(st.GetBlueColorWriteMask());
    EXPECT_FALSE(st.GetAlphaColorWriteMask());
    EXPECT_EQ(StateTable::kCullFrontAndBack, st.GetCullFaceMode());
    EXPECT_EQ(StateTable::kDepthGreaterOrEqual, st.GetDepthFunction());
    EXPECT_EQ(math::Range1f(0.2f, 0.7f), st.GetDepthRange());
    EXPECT_FALSE(st.GetDepthWriteMask());
    EXPECT_EQ(StateTable::kClockwise, st.GetFrontFaceMode());
    EXPECT_EQ(StateTable::kHintNicest,
              st.GetHint(StateTable::kGenerateMipmapHint));
    EXPECT_EQ(0.4f, st.GetLineWidth());
    EXPECT_EQ(0.4f, st.GetPolygonOffsetFactor());
    EXPECT_EQ(0.2f, st.GetPolygonOffsetUnits());
    EXPECT_EQ(0.5f, st.GetSampleCoverageValue());
    EXPECT_TRUE(st.IsSampleCoverageInverted());
    EXPECT_EQ(math::Range2i(math::Point2i(4, 10), math::Point2i(127, 244)),
              st.GetScissorBox());
    EXPECT_EQ(StateTable::kStencilLessOrEqual, st.GetFrontStencilFunction());
    EXPECT_EQ(100, st.GetFrontStencilReferenceValue());
    EXPECT_EQ(0xbeefbeefU, st.GetFrontStencilMask());
    EXPECT_EQ(StateTable::kStencilGreater, st.GetBackStencilFunction());
    EXPECT_EQ(200, st.GetBackStencilReferenceValue());
    EXPECT_EQ(0xfacefaceU, st.GetBackStencilMask());
    EXPECT_EQ(0xdeadfaceU, st.GetFrontStencilWriteMask());
    EXPECT_EQ(0xcacabeadU, st.GetBackStencilWriteMask());

    EXPECT_EQ(StateTable::kStencilReplace, st.GetFrontStencilFailOperation());
    EXPECT_EQ(StateTable::kStencilIncrement,
              st.GetFrontStencilDepthFailOperation());
    EXPECT_EQ(StateTable::kStencilInvert, st.GetFrontStencilPassOperation());
    EXPECT_EQ(StateTable::kStencilIncrementAndWrap,
              st.GetBackStencilFailOperation());
    EXPECT_EQ(StateTable::kStencilDecrementAndWrap,
              st.GetBackStencilDepthFailOperation());
    EXPECT_EQ(StateTable::kStencilZero, st.GetBackStencilPassOperation());
    EXPECT_EQ(123, st.GetClearStencilValue());
    EXPECT_EQ(math::Range2i(math::Point2i(16, 49), math::Point2i(236, 366)),
              st.GetViewport());
  }
}

TEST_F(RendererTest, UpdateFromStateTable) {
  RendererPtr renderer(new Renderer(gm_));

  // Verify the default StateTable matches the default OpenGL state.
  const StateTable& st = renderer->GetStateTable();
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);
  {
    EXPECT_EQ(0U, st.GetSetCapabilityCount());
    EXPECT_EQ(0U, st.GetSetValueCount());
  }
  NodePtr root = BuildGraph(kWidth, kHeight);
  renderer->DrawScene(root);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepth"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE"));

  // Create a StateTable with differing values from current state.
  StateTablePtr state_table(new StateTable(kWidth / 2, kHeight / 2));
  state_table->SetViewport(math::Range2i(
      math::Point2i(2, 2), math::Point2i(kWidth / 2, kHeight / 2)));
  state_table->SetClearColor(math::Vector4f(0.31f, 0.25f, 0.55f, 0.5f));
  state_table->SetClearDepthValue(0.5f);
  state_table->Enable(StateTable::kDepthTest, false);
  state_table->Enable(StateTable::kCullFace, true);
  // This is already set.
  state_table->Enable(StateTable::kScissorTest, false);

  renderer->UpdateStateFromStateTable(state_table);
  EXPECT_EQ(state_table->GetViewport(), st.GetViewport());
  EXPECT_EQ(state_table->GetClearColor(), st.GetClearColor());
  EXPECT_EQ(state_table->GetClearDepthValue(), st.GetClearDepthValue());
  EXPECT_EQ(state_table->IsEnabled(StateTable::kDepthTest),
            st.IsEnabled(StateTable::kDepthTest));
  EXPECT_EQ(state_table->IsEnabled(StateTable::kCullFace),
            st.IsEnabled(StateTable::kCullFace));

  // The next draw should trigger some additional state changes to invert the
  // changes.
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_CULL_FACE"));
  // Since the renderer thinks scissor was already disabled, nothing happens
  // here.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_SCISSOR"));
}

TEST_F(RendererTest, ProcessStateTable) {
  RendererPtr renderer(new Renderer(gm_));

  // Ensure the default StateTable is up to date.
  renderer->UpdateStateFromOpenGL(kWidth, kHeight);

  // Create a StateTable with a few values set.
  StateTablePtr state_table(new StateTable(kWidth / 2, kHeight / 2));
  state_table->SetViewport(math::Range2i(
      math::Point2i(2, 2), math::Point2i(kWidth / 2, kHeight / 2)));
  state_table->SetClearColor(math::Vector4f(0.31f, 0.25f, 0.55f, 0.5f));
  state_table->SetClearDepthValue(0.5f);
  state_table->Enable(StateTable::kBlend, true);
  state_table->Enable(StateTable::kStencilTest, true);
  // This is already set.
  state_table->Enable(StateTable::kScissorTest, false);

  Reset();
  renderer->ProcessStateTable(state_table);
  EXPECT_EQ(6U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST"));
  // Since the renderer thinks scissor was already disabled, nothing happens
  // here.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_SCISSOR"));

  NodePtr root = BuildGraph(kWidth, kHeight);
  renderer->DrawScene(root);
  Reset();
  // Check that the settings undone after the Node was processed are not made,
  // such as depth test.
  renderer->ProcessStateTable(state_table);
  EXPECT_EQ(4U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
  // These two were already set.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST"));
  // This is set in the client state table, but should not be processed.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST"));

  state_table->ResetValue(StateTable::kClearColorValue);
  state_table->ResetValue(StateTable::kClearDepthValue);
  // Change the state of a few things and verify that only they change.
  state_table->Enable(StateTable::kBlend, false);
  state_table->Enable(StateTable::kScissorTest, true);
  Reset();
  renderer->ProcessStateTable(state_table);
  EXPECT_TRUE(trace_verifier_->VerifyTwoCalls("Disable(GL_BLEND",
                                              "Enable(GL_SCISSOR"));

  state_table->SetBlendColor(math::Vector4f(1.f, 2.f, 3.f, 4.f));
  state_table->SetCullFaceMode(StateTable::kCullFront);
  Reset();
  renderer->ProcessStateTable(state_table);
  EXPECT_EQ(2U, trace_verifier_->GetCallCount());
  EXPECT_TRUE(trace_verifier_->VerifyTwoCalls("BlendColor(1, 2, 3, 4)",
                                              "CullFace(GL_FRONT"));

  // Test setting enforcement.
  Reset();
  state_table->SetEnforceSettings(true);
  renderer->ProcessStateTable(state_table);
  EXPECT_EQ(7U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Clear("));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepth"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_BLEND"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST"));
  // Since the renderer thinks scissor was already disabled, nothing happens
  // here.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_SCISSOR"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BlendColor(1, 2, 3, 4)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CullFace(GL_FRONT"));
}

TEST_F(RendererTest, DestroyStateCache) {
  // Doing something that requires internal resource access will trigger some
  // gets.
  {
    Renderer::DestroyCurrentStateCache();
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    // This time a binder will get created.
    // This triggers three calls, one to get the image unit count, and the
    // second to bind the framebuffer and the third to get the current
    // framebuffer.
    EXPECT_EQ(3U, trace_verifier_->GetCallCount());
    EXPECT_EQ(3U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // Doing the same thing again results in no calls since the calls are
  // associated with the current Visual.
  {
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    EXPECT_EQ(0U, MockGraphicsManager::GetCallCount());
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // Destroying the cached state will trigger recreation.
  {
    // Destroying twice has no ill effects.
    Renderer::DestroyStateCache(portgfx::Visual::GetCurrent());
    Renderer::DestroyStateCache(portgfx::Visual::GetCurrent());
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(2U, trace_verifier_->GetCallCount());
    EXPECT_EQ(2U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // We get the same effect if we clear the current state cache.
  {
    // Destroying twice has no ill effects.
    Renderer::DestroyCurrentStateCache();
    Renderer::DestroyStateCache(portgfx::Visual::GetCurrent());
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(2U, trace_verifier_->GetCallCount());
    EXPECT_EQ(2U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
}

TEST_F(RendererTest, NoScene) {
  // Nothing happens if there are no interactions with the renderer.
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    EXPECT_EQ(0U, MockGraphicsManager::GetCallCount());
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
  }
  // Destroying a renderer normally requires an internal bind cache,
  // unless one has never been created, as is the case here.
  // So none of the calls made when creating a binder should be seen here.
  EXPECT_EQ(0U, trace_verifier_->GetCallCount());
  EXPECT_EQ(0U, MockGraphicsManager::GetCallCount());
  EXPECT_EQ(0U, trace_verifier_->GetCountOf(
      "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf(
      "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
  Reset();

  // Doing something that requires internal resource access will trigger some
  // gets.
  {
    Renderer::DestroyCurrentStateCache();
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    // This time a binder will get created.
    // This triggers two calls, one to get the image unit count, and other to
    // get the current framebuffer.
    EXPECT_EQ(3U, trace_verifier_->GetCallCount());
    EXPECT_EQ(3U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // Doing the same thing again results in no calls since the calls are
  // associated with the current Visual.
  {
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    EXPECT_EQ(0U, MockGraphicsManager::GetCallCount());
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  // Destroying the cached state will trigger recreation.
  {
    Renderer::DestroyStateCache(portgfx::Visual::GetCurrent());
    RendererPtr renderer(new Renderer(gm_));
    renderer->BindFramebuffer(FramebufferObjectPtr());
    EXPECT_EQ(2U, trace_verifier_->GetCallCount());
    EXPECT_EQ(2U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }

  // There should be no calls when the renderer is destroyed.
  EXPECT_TRUE(trace_verifier_->VerifyNoCalls());
  EXPECT_EQ(0, MockGraphicsManager::GetCallCount());

  // Try to render using a NULL node.
  {
    // Also change to fake desktop OpenGL to test that path.
    gm_->SetVersionString("Ion fake OpenGL");
    Reset();
    Renderer::DestroyStateCache(portgfx::Visual::GetCurrent());
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(NodePtr(NULL));
    EXPECT_EQ(3U, trace_verifier_->GetCallCount());
    EXPECT_EQ(3U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_POINT_SPRITE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_PROGRAM_POINT_SIZE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
    renderer->DrawScene(NodePtr(NULL));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    Reset();
  }
  EXPECT_TRUE(trace_verifier_->VerifyNoCalls());
  EXPECT_EQ(0, MockGraphicsManager::GetCallCount());
}

TEST_F(RendererTest, BasicGraph) {
  {
    std::vector<std::string> call_strings;
    RendererPtr renderer(new Renderer(gm_));
    // Draw the simplest possible scene.
    NodePtr root(new Node);
    renderer->DrawScene(root);
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));

    EXPECT_EQ(2U, trace_verifier_->GetCallCount());
    EXPECT_EQ(2U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
        "GetIntegerv(GL_FRAMEBUFFER_BINDING"));
    Reset();
  }

  {
    base::LogChecker log_checker;
    RendererPtr renderer(new Renderer(gm_));
    // Have a state table.
    NodePtr root(new Node);
    StateTablePtr state_table(new StateTable(kWidth, kHeight));
    state_table->SetViewport(
        math::Range2i(math::Point2i(0, 0), math::Point2i(kWidth, kHeight)));
    state_table->SetClearColor(math::Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
    state_table->SetClearDepthValue(0.f);
    state_table->Enable(StateTable::kDepthTest, true);
    state_table->Enable(StateTable::kCullFace, true);
    root->SetStateTable(state_table);
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_EQ(3U, MockGraphicsManager::GetCallCount());
    // Only clearing should have occurred since no shapes are in the node.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable"));
    Reset();

    // Add a shape to get state changes and shader creation.
    BuildRectangleShape<uint16>();
    root->AddShape(s_data.shape);
    renderer->DrawScene(root);
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "no value set for uniform"));
    EXPECT_GT(MockGraphicsManager::GetCallCount(), 0);
    std::vector<std::string> call_strings;
    call_strings.push_back("Clear(");
    call_strings.push_back("CreateShader");
    call_strings.push_back("CompileShader");
    call_strings.push_back("ShaderSource");
    call_strings.push_back("GetShaderiv");
    call_strings.push_back("CreateProgram");
    call_strings.push_back("AttachShader");
    call_strings.push_back("LinkProgram");
    call_strings.push_back("GetProgramiv");
    call_strings.push_back("UseProgram");
    call_strings.push_back("Enable(GL_DEPTH_TEST)");
    call_strings.push_back("Enable(GL_CULL_FACE)");
    EXPECT_TRUE(trace_verifier_->VerifySomeCalls(call_strings));
    // The clear values have already been set.
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepth"));
    Reset();

    // Used as the base for the enforced settings.
    renderer->DrawScene(root);
    EXPECT_EQ(3U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepth"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Viewport"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable"));
    Reset();

    // Test setting enforcement.
    state_table->SetEnforceSettings(true);
    renderer->DrawScene(root);
    // 9 calls generated here. The 5 more calls are coming from 2 clear calls, 2
    // enable calls, and 1 viewport call.
    EXPECT_EQ(8U, MockGraphicsManager::GetCallCount());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepth"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Viewport"));
    // Settings are enforced. As a result, the two "Enable" calls will be passed
    // to OpenGL.
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("Enable"));

    EXPECT_TRUE(log_checker.HasMessage("WARNING", "no value set for uniform"));
  }
}

TEST_F(RendererTest, ZombieResourceBinderCache) {
  Renderer::DestroyCurrentStateCache();
  NodePtr root = BuildGraph(800, 800);
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);
  base::LogChecker log_checker;
  {
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(root);
    portgfx::Visual::MakeCurrent(nullptr);
  }
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "No Visual ID"));
  // All renderer resources are now destroyed.
  {
    // Reuse the same context, which will crash when drawing if we have any old
    // resource pointers.
    portgfx::Visual::MakeCurrent(visual_.get());
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(root);
  }
  Renderer::DestroyCurrentStateCache();
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
  {
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(root);
    portgfx::Visual::MakeCurrent(nullptr);
  }
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "No Visual ID"));
  // All renderer resources are now destroyed.
  {
    // Reuse the same context, which will crash when drawing if we have any old
    // resource pointers.
    portgfx::Visual::MakeCurrent(visual_.get());
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(root);
  }
}

TEST_F(RendererTest, VertexAttribDivisor) {
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute vec3 attribute1;\n"
      "attribute vec2 attribute2;\n";
  static const char* kFragmentShaderString = "void main() {}\n";

  BuildRectangleBufferObject();

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  Attribute attribute1 = reg->Create<Attribute>(
      "attribute1", BufferObjectElement(s_data.vertex_buffer,
                                        s_data.vertex_buffer->AddSpec(
                                            BufferObject::kFloat, 3, 0)));
  Attribute attribute2 = reg->Create<Attribute>(
      "attribute2",
      BufferObjectElement(s_data.vertex_buffer,
                          s_data.vertex_buffer->AddSpec(BufferObject::kFloat, 2,
                                                        sizeof(float) * 3)));
  {
    NodePtr root(new Node);
    AttributeArrayPtr aa(new AttributeArray);
    // Set Divisor for attribute2
    attribute2.SetDivisor(1);
    aa->AddAttribute(attribute1);
    aa->AddAttribute(attribute2);
    ShapePtr shape(new Shape);
    shape->SetAttributeArray(aa);
    root->SetShaderProgram(ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr()));
    root->GetShaderProgram()->SetLabel("root shader");
    root->AddShape(shape);
    renderer->DrawScene(root);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "VertexAttribDivisor"))
                    .HasArg(2, "0x0"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  1U, "VertexAttribDivisor"))
                    .HasArg(2, "0x1"));
    Reset();
  }

  gm_->EnableFunctionGroup(GraphicsManager::kInstancedDrawing, false);
  {
    NodePtr root(new Node);
    AttributeArrayPtr aa(new AttributeArray);
    attribute1.SetDivisor(5);
    attribute2.SetDivisor(3);
    aa->AddAttribute(attribute1);
    aa->AddAttribute(attribute2);
    ShapePtr shape(new Shape);
    shape->SetAttributeArray(aa);
    root->SetShaderProgram(ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr()));
    root->GetShaderProgram()->SetLabel("root shader");
    root->AddShape(shape);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
  }
}

TEST_F(RendererTest, DrawElementsInstanced) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(800, 800);

  {
    // DrawElements.
    renderer->DrawScene(root);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "VertexAttribPointer"))
                    .HasArg(1, "0"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // DrawElementsInstanced.
    root->GetChildren()[0]->GetShapes()[0]->SetInstanceCount(8);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElementsInstanced("));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "DrawElementsInstanced"))
                    .HasArg(5, "8"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // vertex range testing for instanced drawing.
    // DrawElements.
    root->GetChildren()[0]->GetShapes()[0]->AddVertexRange(Range1i(1, 3));
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // DrawElementsInstanced.
    root->GetChildren()[0]->GetShapes()[0]->SetVertexRangeInstanceCount(0, 5);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "DrawElementsInstanced"))
                    .HasArg(5, "5"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();
  }

  // Vertex range based drawing with kInstanceDrawing disabled.
  // This will result in 1 call for the DrawElements and a warning message
  // stating that instanced drawing functions are not available.
  gm_->EnableFunctionGroup(GraphicsManager::kInstancedDrawing, false);
  {
    base::LogChecker log_checker;
    // DrawElements.
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    EXPECT_TRUE(log_checker.HasMessage(
        "WARNING", "ION: Instanced drawing is not available."));
  }
}

TEST_F(RendererTest, DrawArraysInstanced) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(800, 800, false, false);

  {
    // DrawArrays
    renderer->DrawScene(root);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "VertexAttribPointer"))
                    .HasArg(1, "0"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // DrawArraysInstanced
    root->GetChildren()[0]->GetShapes()[0]->SetInstanceCount(8);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "DrawArraysInstanced"))
                    .HasArg(4, "8"));
    Reset();

    // vertex range testing for instanced drawing.
    root->GetChildren()[0]->GetShapes()[0]->AddVertexRange(Range1i(1, 3));
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    Reset();

    // DrawArraysInstanced.
    root->GetChildren()[0]->GetShapes()[0]->SetVertexRangeInstanceCount(0, 5);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                  0U, "DrawArraysInstanced"))
                    .HasArg(4, "5"));
    Reset();
  }

  // Vertex range based drawing with kInstanceDrawing disabled.
  // This will result in 1 call for the DrawArrays and a warning message stating
  // that instanced drawing functions are not available.
  gm_->EnableFunctionGroup(GraphicsManager::kInstancedDrawing, false);
  {
    base::LogChecker log_checker;
    // DrawElements.
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribDivisor"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements("));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays("));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElementsInstanced"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawArraysInstanced"));
    EXPECT_TRUE(log_checker.HasMessage(
        "WARNING", "ION: Instanced drawing is not available."));
  }
}

TEST_F(RendererTest, InstancedShaderDoesNotGenerateWarnings) {
  base::LogChecker log_checker;

  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(800, 800, true, false,
                            kInstancedVertexShaderString,
                            kPlaneFragmentShaderString);
  renderer->DrawScene(root);

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, GpuMemoryUsage) {
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(kWidth, kHeight);
    EXPECT_EQ(0U, s_data.index_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(0U, s_data.vertex_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(0U, s_data.texture->GetGpuMemoryUsed());
    EXPECT_EQ(0U, s_data.cubemap->GetGpuMemoryUsed());
    renderer->DrawScene(root);
    // There are 12 bytes in the index buffer, and 4 * sizeof(Vertex) in vertex
    // buffer. There are 7 32x32 RGBA texture images (one regular texture, one
    // cubemap).
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
    EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
    EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    s_data.attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>("aTestAttrib", 2.f);
    EXPECT_TRUE(a.IsValid());
    s_data.attribute_array->AddAttribute(a);
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(s_data.rect);
    Reset();
    renderer->DrawScene(root);
    // There are 12 bytes in the index buffer. Since there are no buffer
    // attributes, the vertex buffer never uploads its data. There are 7 32x32
    // RGBA texture images (one regular texture, one cubemap).
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U, 0U, 28672U));
    EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(0U, s_data.vertex_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
    EXPECT_EQ(4096U * 6U, s_data.cubemap->GetGpuMemoryUsed());
  }
}

TEST_F(RendererTest, BufferAttributeTypes) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;

  NodePtr root = BuildGraph(kWidth, kHeight);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uTexture", kTextureUniform, "Plane texture"));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uTexture2", kTextureUniform, "Plane texture"));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));

  // Create a spec for each type.
  std::vector<SpecInfo> spec_infos;
  spec_infos.push_back(SpecInfo(s_data.vertex_buffer->AddSpec(
      BufferObject::kByte, 1, 0), "GL_BYTE"));
  spec_infos.push_back(SpecInfo(s_data.vertex_buffer->AddSpec(
      BufferObject::kUnsignedByte, 1, 1), "GL_UNSIGNED_BYTE"));
  spec_infos.push_back(SpecInfo(s_data.vertex_buffer->AddSpec(
      BufferObject::kShort, 1, 2), "GL_SHORT"));
  spec_infos.push_back(SpecInfo(s_data.vertex_buffer->AddSpec(
      BufferObject::kUnsignedShort, 1, 4), "GL_UNSIGNED_SHORT"));
  spec_infos.push_back(SpecInfo(s_data.vertex_buffer->AddSpec(
      BufferObject::kInt, 1, 6), "GL_INT"));
  spec_infos.push_back(SpecInfo(s_data.vertex_buffer->AddSpec(
      BufferObject::kUnsignedInt, 1, 10), "GL_UNSIGNED_INT"));
  spec_infos.push_back(SpecInfo(s_data.vertex_buffer->AddSpec(
      BufferObject::kFloat, 1, 14), "GL_FLOAT"));
  spec_infos.push_back(SpecInfo(s_data.vertex_buffer->AddSpec(
      BufferObject::kInvalid, 1, 18), "GL_INVALID_ENUM"));

  s_data.attribute_array = new AttributeArray;
  s_data.shape->SetAttributeArray(s_data.attribute_array);
  s_data.shader = ShaderProgram::BuildFromStrings(
      "Plane shader", reg, kPlaneVertexShaderString,
      kPlaneFragmentShaderString, base::AllocatorPtr());
  s_data.rect->SetShaderProgram(s_data.shader);
  s_data.rect->ClearUniforms();
  AddPlaneShaderUniformsToNode(s_data.rect);

  const size_t count = spec_infos.size();
  for (size_t i = 0; i < count; ++i) {
    base::LogChecker log_checker;
    SCOPED_TRACE(::testing::Message() << "Iteration " << i);
    const Attribute a = s_data.shader->GetRegistry()->Create<Attribute>(
        "aTestAttrib", BufferObjectElement(s_data.vertex_buffer,
                                           spec_infos[i].index));
    EXPECT_TRUE(a.IsValid());
    if (s_data.attribute_array->GetAttributeCount())
      EXPECT_TRUE(s_data.attribute_array->ReplaceAttribute(0, a));
    else
      s_data.attribute_array->AddAttribute(a);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    const BufferObject::Spec& spec =
        s_data.vertex_buffer->GetSpec(spec_infos[i].index);
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "VertexAttribPointer"))
            .HasArg(3, spec_infos[i].type)
            .HasArg(5, helper.ToString(
                "GLint", static_cast<int>(
                    s_data.vertex_buffer->GetStructSize())))
            .HasArg(6, helper.ToString(
                "const void*", reinterpret_cast<const void*>(
                    spec.byte_offset))));
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();

  // Some of the specs are technically invalid, but are tested for coverage.
  gm_->SetErrorCode(GL_NO_ERROR);
}

TEST_F(RendererTest, PreventZombieUpdates) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  base::LogChecker log_checker;

  NodePtr root = BuildGraph(kWidth, kHeight);
  Reset();
  // Create resources.
  renderer->DrawScene(root);
  Reset();
  // Force resource destruction.
  s_data.vertex_buffer = NULL;
  s_data.rect = NULL;
  s_data.attribute_array = NULL;
  s_data.shape = NULL;
  root = NULL;
  // Clearing cached bindings causes the active buffer to be put on the update
  // list.
  renderer->ClearCachedBindings();
  // Drawing should just destroy resources, and should _not_ try to update the
  // buffer. If it does then this will crash when the Renderer processes the
  // update list.
  renderer->DrawScene(root);

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, EnableDisableBufferAttributes) {
  RendererPtr renderer(new Renderer(gm_));
  base::LogChecker log_checker;

  NodePtr root = BuildGraph(kWidth, kHeight);
  Reset();
  s_data.attribute_array->EnableAttribute(0, false);
  renderer->DrawScene(root);
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DisableVertexAttribArray"));

  Reset();
  s_data.attribute_array->EnableAttribute(0, true);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, VertexArraysPerShaderProgram) {
  // Check that a resource is created per ShaderProgram. We can test this by
  // checking that the proper VertexAttribPointer calls are sent. This requires
  // two shader programs where the second one uses more buffer Attributes than
  // the first one.
  base::LogChecker log_checker;

  static const char* kVertexShaderString = "attribute vec3 attribute;\n";
  static const char* kVertex2ShaderString =
      "attribute vec3 attribute;\n"
      "attribute vec3 attribute2;\n";
  static const char* kFragmentShaderString = "void main() {}\n";

  BuildRectangleBufferObject();

  NodePtr root(new Node);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  AttributeArrayPtr aa(new AttributeArray);
  aa->AddAttribute(reg->Create<Attribute>(
      "attribute", BufferObjectElement(
          s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
              BufferObject::kFloat, 3, 0))));
  aa->AddAttribute(reg->Create<Attribute>(
      "attribute2", BufferObjectElement(
          s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
              BufferObject::kFloat, 3, 12))));
  ShapePtr shape(new Shape);
  shape->SetAttributeArray(aa);
  root->SetShaderProgram(ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr()));
  root->GetShaderProgram()->SetLabel("root shader");
  root->AddShape(shape);

  // The child uses more attributes.
  NodePtr child(new Node);
  child->SetShaderProgram(ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertex2ShaderString, kFragmentShaderString,
      base::AllocatorPtr()));
  child->AddShape(shape);
  root->GetShaderProgram()->SetLabel("child shader");
  root->AddChild(child);

  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "contains buffer attribute"));
  }

  // If we disable the missing attribute there should be no warning.
  {
    aa->EnableBufferAttribute(1U, false);
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_FALSE(log_checker.HasAnyMessages());
    aa->EnableBufferAttribute(1U, true);
  }

  base::logging_internal::SingleLogger::ClearMessages();
  // Check without vertex arrays.
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "contains buffer attribute"));
  }
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
}

TEST_F(RendererTest, VertexArraysPerThread) {
  // Check that distinct vertex arrays are created for distinct threads.
  base::LogChecker log_checker;

  static const char* kVertexShaderString = "attribute vec3 attribute;\n";
  static const char* kFragmentShaderString = "void main() {}\n";

  BuildRectangleBufferObject();

  NodePtr root(new Node);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  AttributeArrayPtr aa(new AttributeArray);
  aa->AddAttribute(reg->Create<Attribute>(
      "attribute", BufferObjectElement(
          s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
              BufferObject::kFloat, 3, 0))));
  ShapePtr shape(new Shape);
  shape->SetAttributeArray(aa);
  root->SetShaderProgram(ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr()));
  root->GetShaderProgram()->SetLabel("root shader");
  root->AddShape(shape);

  MockVisual share_visual(*visual_);

  // The attribute location should be bound only once, since the program
  // object is shared between threads, while glVertexAttribPointer should be
  // called once per vertex array object.
  {
    Reset();
    RendererPtr renderer(new Renderer(gm_));
    std::function<bool()> thread_function = std::bind(
        RenderingThread, renderer, &share_visual, &root);
    ion::port::ThreadId thread_id = ion::port::SpawnThreadStd(&thread_function);
    // MockVisual is not thread-safe, so we don't try to render concurrently.
    ion::port::JoinThread(thread_id);
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  // Check without vertex arrays.
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);
  {
    Reset();
    RendererPtr renderer(new Renderer(gm_));
    std::function<bool()> thread_function = std::bind(
        RenderingThread, renderer, &share_visual, &root);
    ion::port::ThreadId thread_id = ion::port::SpawnThreadStd(&thread_function);
    ion::port::JoinThread(thread_id);
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
}

TEST_F(RendererTest, NonBufferAttributes) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;

  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture2"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kFloatAttribute, "Testing attribute"));

    s_data.attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>("aTestAttrib", 2.f);
    EXPECT_TRUE(a.IsValid());
    s_data.attribute_array->AddAttribute(a);
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(s_data.rect);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kFloatVector2Attribute, "Testing attribute"));

    s_data.attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>(
        "aTestAttrib", math::Vector2f(1.f, 2.f));
    EXPECT_TRUE(a.IsValid());
    s_data.attribute_array->AddAttribute(a);
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(s_data.rect);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kFloatVector3Attribute, "Testing attribute"));

    s_data.attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>(
        "aTestAttrib", math::Vector3f(1.f, 2.f, 3.f));
    EXPECT_TRUE(a.IsValid());
    s_data.attribute_array->AddAttribute(a);
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(s_data.rect);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kFloatVector4Attribute, "Testing attribute"));

    s_data.attribute_array = new AttributeArray;
    const Attribute a = reg->Create<Attribute>(
        "aTestAttrib", math::Vector4f(1.f, 2.f, 3.f, 4.f));
    EXPECT_TRUE(a.IsValid());
    s_data.attribute_array->AddAttribute(a);
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, kPlaneVertexShaderString,
        kPlaneFragmentShaderString, base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    AddPlaneShaderUniformsToNode(s_data.rect);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    Attribute* a = s_data.shape->GetAttributeArray()->GetMutableAttribute(0U);
    a->SetFixedPointNormalized(true);

    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "VertexAttribPointer"))
            .HasArg(4, "GL_TRUE"));
  }

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib1fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "VertexAttribPointer"))
            .HasArg(4, "GL_FALSE"));
  }

  Reset();
}

TEST_F(RendererTest, MissingInputFromRegistry) {
  // Test that if a shader defines an attribute or uniform but there is no
  // registry entry for it, a warning message is logged.
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute vec3 attribute1;\n"
      "attribute vec3 attribute2;\n";

  static const char* kFragmentShaderString =
      "uniform vec3 uniform1;\n"
      "uniform vec3 uniform2;\n";

  // Everything defined and added.
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform2", kFloatVector3Uniform, ""));

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  // Missing a uniform that is defined in the shader.
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform2", kFloatVector3Uniform, ""));

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                       "no value set for uniform 'uniform2'"));
  }

  // NULL texture value for a texture uniform.
  {
    static const char* kFragmentShaderString =
        "uniform vec3 uniform1;\n"
        "uniform sampler2D uniform2;\n";
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", TexturePtr());
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(log_checker.HasMessage(
        "WARNING", "no value set for uniform 'uniform2'"));
  }

  // Missing attribute.
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform1", kFloatVector3Uniform, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform2", kFloatVector3Uniform, ""));

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage(
            "WARNING",
            "Attribute 'attribute2' used in shader 'Shader' does not have a"));
  }

  // Missing attribute.
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute1", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform2", kFloatVector3Uniform, ""));

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage(
            "WARNING",
            "Uniform 'uniform1' used in shader 'Shader' does not have a"));
  }

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, ShaderRecompilationClearsUniforms) {
  base::LogChecker log_checker;

  static const char* kVertexShaderString =
      "attribute vec3 attribute1;\n"
      "attribute vec3 attribute2;\n";

  static const char* kFragmentShaderString =
      "uniform vec3 uniform1;\n"
      "uniform vec3 uniform2;\n";

  static const char* kFragmentShaderWithExtraUniformString =
      "uniform vec3 uniform1;\n"
      "uniform vec3 uniform2;\n"
      "uniform vec3 uniform3;\n";

  NodePtr root = BuildGraph(kWidth, kHeight);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "attribute1", kFloatVector3Attribute, ""));
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "attribute2", kFloatVector3Attribute, ""));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uniform1", kFloatVector3Uniform, ""));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uniform2", kFloatVector3Uniform, ""));
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uniform3", kFloatVector3Uniform, ""));

  s_data.attribute_array = new AttributeArray;
  s_data.attribute_array->AddAttribute(
      reg->Create<Attribute>("attribute1", math::Vector3f(1.f, 2.f, 3.f)));
  s_data.attribute_array->AddAttribute(
      reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
  reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
  reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
  s_data.shader = ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  s_data.shape->SetAttributeArray(s_data.attribute_array);
  s_data.rect->SetShaderProgram(s_data.shader);
  s_data.rect->ClearUniforms();
  s_data.rect->AddUniform(
      reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
  s_data.rect->AddUniform(
      reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
  Reset();
  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Now update the shader string to have another uniform, but without setting
  // a value for it.
  s_data.shader->GetFragmentShader()->SetSource(
      kFragmentShaderWithExtraUniformString);
  // The warning about not setting a uniform value will be triggered every draw.
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "There is no value set"));
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "There is no value set"));
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "There is no value set"));

  // Fixing the shader should remove the message.
  s_data.shader->GetFragmentShader()->SetSource(kFragmentShaderString);
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, RegistryHasWrongUniformType) {
  // Test that if a shader defines an attribute of different type than the
  // registry entry for it, a warning message is logged.
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString = "attribute vec3 attribute;\n";
  static const char* kFragmentShaderString = "uniform vec4 uniform;\n";

  // Everything defined.
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform", kFloatVector3Uniform, ""));

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute", math::Vector3f(1.f, 2.f, 3.f)));
    reg->Create<Uniform>("uniform", math::Vector3f(1.f, 2.f, 3.f));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage("WARNING",
                               "Uniform 'uniform' has a different type"));
  }

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, RegistryHasAliasedInputs) {
  // Test that if a registry has aliased inputs then a warning message is
  // logged.
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute vec3 attribute;\n"
      "attribute vec3 attribute2;\n";
  static const char* kFragmentShaderString = "uniform vec4 uniform;\n";

  // Everything defined.
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg1(new ShaderInputRegistry);
    reg1->Add(ShaderInputRegistry::AttributeSpec(
        "attribute", kFloatVector3Attribute, ""));
    reg1->Add(ShaderInputRegistry::UniformSpec(
        "uniform", kFloatVector3Uniform, ""));
    ShaderInputRegistryPtr reg2(new ShaderInputRegistry);
    reg2->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg1->Include(reg2);
    // Add an input to reg2 that already exists in reg1. This is only detected
    // when the shader resource is created.
    reg2->Add(ShaderInputRegistry::UniformSpec(
        "uniform", kFloatVector3Uniform, ""));

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg1->Create<Attribute>("attribute", math::Vector3f(1.f, 2.f, 3.f)));
    s_data.attribute_array->AddAttribute(
        reg1->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    reg1->Create<Uniform>("uniform", math::Vector3f(1.f, 2.f, 3.f));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg1, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage("WARNING",
                               "contains multiple definitions of some inputs"));
  }

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, AttributeArraysShareIndexBuffer) {
  // Test that if when multiple attribute arrays (VAOs) share an index buffer
  // that the index buffer is rebound for each.
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute vec3 attribute;\n"
      "attribute vec3 attribute2;\n";
  static const char* kFragmentShaderString = "void main() {}\n";

  NodePtr root = BuildGraph(kWidth, kHeight);
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  s_data.attribute_array = new AttributeArray;
  s_data.attribute_array->AddAttribute(
      reg->Create<Attribute>("attribute", math::Vector3f(1.f, 2.f, 3.f)));
  s_data.attribute_array->AddAttribute(
      reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
  s_data.shader = ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  s_data.shape->SetAttributeArray(s_data.attribute_array);
  s_data.rect->SetShaderProgram(s_data.shader);
  s_data.rect->ClearUniforms();
  Reset();
  renderer->DrawScene(root);
  // The element array buffer should have been bound once.
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));

  // Reset the renderer.
  renderer.Reset(new Renderer(gm_));
  AttributeArrayPtr array2(new AttributeArray);
  array2->AddAttribute(
      reg->Create<Attribute>("attribute", math::Vector3f(1.f, 2.f, 3.f)));
  array2->AddAttribute(
      reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
  ShapePtr shape(new Shape);
  shape->SetPrimitiveType(s_options.primitive_type);
  shape->SetIndexBuffer(s_data.index_buffer);
  shape->SetAttributeArray(array2);
  s_data.rect->AddShape(shape);

  Reset();
  renderer->DrawScene(root);
  // The element array buffer should have been bound twice.
  EXPECT_EQ(2U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, AttributeArrayHasAttributeShaderDoesnt) {
  // Test that if an attribute array contains an attribute that is not defined
  // in the shader then a warning message is logged.
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString = "attribute vec3 attribute;\n";
  static const char* kFragmentShaderString = "uniform vec3 uniform;\n";

  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute", kBufferObjectElementAttribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform", kFloatVector3Uniform, ""));

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(reg->Create<Attribute>(
        "attribute", BufferObjectElement(
            s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
                BufferObject::kFloat, 3, 0))));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute2", math::Vector3f(1.f, 2.f, 3.f)));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "AttributeArrayHasAttributeShaderDoesnt1", reg, kVertexShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform", math::Vector3f(1.f, 2.f, 3.f)));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage(
            "WARNING",
            "contains simple attribute 'attribute2' but the current shader"));
  }

  base::logging_internal::SingleLogger::ClearMessages();
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute2", kBufferObjectElementAttribute, ""));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "attribute", kFloatVector3Attribute, ""));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uniform", kFloatVector3Uniform, ""));

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(reg->Create<Attribute>(
        "attribute2", BufferObjectElement(
            s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
                BufferObject::kFloat, 3, 0))));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("attribute", math::Vector3f(1.f, 2.f, 3.f)));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "AttributeArrayHasAttributeShaderDoesnt2", reg, kVertexShaderString,
        kFragmentShaderString, base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform", math::Vector3f(1.f, 2.f, 3.f)));
    Reset();
    renderer->DrawScene(root);
    EXPECT_TRUE(
        log_checker.HasMessage(
            "WARNING",
            "contains buffer attribute 'attribute2' but the current shader"));
  }

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, ReuseSameBufferAndShader) {
  // Test that Renderer does not bind a shader or buffer when they are already
  // active.
  RendererPtr renderer(new Renderer(gm_));
  base::LogChecker log_checker;
  NodePtr root = BuildGraph(kWidth, kHeight);

  // Create a node with the same shader as the rect, and attach it as a child
  // of the rect.
  NodePtr node(new Node);
  node->AddShape(s_data.shape);
  node->SetShaderProgram(s_data.shader);
  s_data.rect->AddChild(node);
  Reset();
  s_data.attribute_array->EnableAttribute(0, false);
  renderer->DrawScene(root);
  // The shader and data for the shape should each only have been bound once.
  // Since the default shader is never bound, its ID should be 1.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UseProgram(0x1)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER, 0x1)"));

  // Reset.
  s_data.rect->ClearChildren();
}

TEST_F(RendererTest, ShaderHierarchies) {
  // Test that uniforms are sent to only if the right shader is bound (if they
  // aren't an error message is logged).
  // Use a node hierarchy as follows:
  //                             Root
  //              |                      |
  // LeftA ->ShaderA         RightA -> ShaderB
  //    |                                           |
  // nodes...                            RightB -> NULL shader
  //                                                 |
  //                                            RightC -> uniform for ShaderB
  // and ensure that the Uniform in rightC is sent to the proper shader.
  RendererPtr renderer(new Renderer(gm_));
  base::LogChecker log_checker;
  // Create data.
  BuildGraph(kWidth, kHeight);

  // Construct graph.
  NodePtr root(new Node);
  root->AddChild(s_data.rect);

  NodePtr right_a(new Node);
  root->AddChild(right_a);

  // Create a new shader.
  static const char* kVertexShaderString =
      "attribute float aFloat;\n"
      "uniform int uInt1;\n"
      "uniform int uInt2;\n";
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aFloat", kBufferObjectElementAttribute, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uInt1", kIntUniform, "."));
  reg->Add(ShaderInputRegistry::UniformSpec("uInt2", kIntUniform, "."));

  ShaderProgramPtr program(new ShaderProgram(reg));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));

  // Build the right side of the graph.
  right_a->SetShaderProgram(program);

  AttributeArrayPtr attribute_array(new AttributeArray);
  attribute_array->AddAttribute(reg->Create<Attribute>(
      "aFloat", BufferObjectElement(
          s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
              BufferObject::kFloat, 1, 0))));
  ShapePtr shape(new Shape);
  shape->SetAttributeArray(attribute_array);

  NodePtr right_b(new Node);
  right_a->AddChild(right_b);
  NodePtr right_c(new Node);
  right_b->AddChild(right_c);
  right_b->AddUniform(reg->Create<Uniform>("uInt2", 2));

  right_c->AddShape(shape);
  right_c->AddUniform(reg->Create<Uniform>("uInt1", 3));

  Reset();
  renderer->DrawScene(root);

  // There should be no log messages.
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, UniformPushAndPop) {
  // Repetitive uniforms should not cause unneeded uploads.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;
  root = BuildGraph(kWidth, kHeight);

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  // Dummy shader with the uniforms defined.
  static const char* kVertexShaderString = "uniform int uInt;\n";
  reg->Add(ShaderInputRegistry::UniformSpec("uInt", kIntUniform, "."));

  ShaderProgramPtr program(new ShaderProgram(reg));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  s_data.rect->SetShaderProgram(program);
  // Remove attribute array to prevent warnings; we are only testing uniforms
  // here.
  s_data.rect->ClearUniforms();
  s_data.rect->AddUniform(reg->Create<Uniform>("uInt", 1));
  s_data.shape->SetAttributeArray(AttributeArrayPtr(NULL));

  NodePtr node(new Node);
  node->AddShape(s_data.shape);
  s_data.rect->AddChild(node);
  node->AddUniform(reg->Create<Uniform>("uInt", 2));

  Reset();
  renderer->DrawScene(root);
  // The uniform should have been sent twice, once for each value.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("Uniform1i"));

  // Reset.
  s_data.rect = NULL;
  s_data.shape->SetAttributeArray(s_data.attribute_array);
  BuildRectangle();
}

TEST_F(RendererTest, UniformsShareTextureUnits) {
  // Test that all textures that share the same uniform are bound to the same
  // texture unit.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  s_data.rect->ClearChildren();
  s_data.rect->ClearUniforms();
  s_data.rect->ClearShapes();

  s_data.rect->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));
  s_data.rect->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uCubeMapTexture", s_data.cubemap));

  // Add many nodes with different textures bound to the same uniform; they
  // should all share the same image unit.
  static const int kNumNodes = 9;
  for (int i = 0; i < kNumNodes; ++i) {
    NodePtr node(new Node);

    TexturePtr texture(new Texture);
    texture->SetImage(0U, s_data.image);
    texture->SetSampler(s_data.sampler);
    node->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
        "uTexture", texture));

    texture = new Texture;
    texture->SetImage(0U, s_data.image);
    texture->SetSampler(s_data.sampler);
    node->AddUniform(
        s_data.shader->GetRegistry()->Create<Uniform>("uTexture2", texture));

    s_data.rect->AddChild(node);
  }
  Reset();
  renderer->DrawScene(root);
  // Nothing should have happened since there are no shapes.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));

  // Add shapes.
  for (int i = 0; i < kNumNodes; ++i)
    s_data.rect->GetChildren()[i]->AddShape(s_data.shape);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(24U, trace_verifier_->GetCountOf("TexImage2D"));
  // There should be 19 calls to ActiveTexture: the units will ping-pong; there
  // is also the cubemap which gets bound.
  EXPECT_EQ(19U, trace_verifier_->GetCountOf("ActiveTexture"));
  // The texture uniforms are only sent once.
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(9U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(9U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, UniformAreSentCorrectly) {
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  // Dummy shader with the uniforms defined.
  static const char* kVertexShaderString =
      "uniform int uInt;\n"
      "uniform float uFloat;\n"
      "uniform vec2 uFV2;\n"
      "uniform vec3 uFV3;\n"
      "uniform vec4 uFV4;\n"
      "uniform ivec2 uIV2;\n"
      "uniform ivec3 uIV3;\n"
      "uniform ivec4 uIV4;\n"
      "uniform mat2 uMat2;\n"
      "uniform mat3 uMat3;\n"
      "uniform mat4 uMat4;\n";

  // One of each uniform type.
  RendererPtr renderer(new Renderer(gm_));

  NodePtr root = BuildGraph(kWidth, kHeight);
  root->ClearUniforms();
  root->ClearUniformBlocks();
  UniformBlockPtr block1(new UniformBlock);
  UniformBlockPtr block2(new UniformBlock);

  PopulateUniformValues(s_data.rect, block1, block2, reg, 0);
  s_data.rect->AddUniformBlock(block1);
  s_data.rect->AddUniformBlock(block2);

  ShaderProgramPtr program(new ShaderProgram(reg));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  s_data.rect->SetShaderProgram(program);
  // Remove attribute array to prevent warnings; we are only testing uniforms
  // here.
  s_data.shape->SetAttributeArray(AttributeArrayPtr(NULL));

  {
    // Verify that the uniforms were sent only once, since there is only one
    // node.
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    VerifyUniformCounts(1U, trace_verifier_);
  }

  // Add another identical node with the same shape and uniforms. Since the
  // uniform values are the same no additional data should be sent to GL.
  NodePtr node(new Node);
  s_data.rect->AddChild(node);
  node->AddShape(s_data.shape);
  PopulateUniformValues(node, block1, block2, reg, 0);
  // Add the same uniform blocks.
  node->AddUniformBlock(block1);
  node->AddUniformBlock(block2);
  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    VerifyUniformCounts(0U, trace_verifier_);
  }

  // Use the same uniforms but with different values.
  PopulateUniformValues(node, block1, block2, reg, 1);
  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    // Verify that the uniforms were sent. Each should be sent once, when the
    // child node is processed, since the initial values were cached already.
    VerifyUniformCounts(1U, trace_verifier_);
  }

  // Set the same shader in the child node.
  node->SetShaderProgram(program);
  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    // The uniforms should have been sent twice since the child nodes blew away
    // the cached values in the last pass; both values will have to be sent this
    // time.
    VerifyUniformCounts(2U, trace_verifier_);
  }

  // Use a different shader for the child node.
  ShaderProgramPtr program2(new ShaderProgram(reg));
  program2->SetLabel("Dummy Shader");
  program2->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program2->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  node->SetShaderProgram(program2);
  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    // Uniforms will have to be sent twice, once for the first program using
    // the first set of values, and then again for the second shader.
    VerifyUniformCounts(2U, trace_verifier_);
  }

  {
    Reset();
    renderer->DrawScene(root);
    SCOPED_TRACE(__LINE__);
    // Now that both caches are populated no uniforms should be sent.
    VerifyUniformCounts(0U, trace_verifier_);
  }

  // Reset.
  s_data.rect = NULL;
  s_data.shape->SetAttributeArray(s_data.attribute_array);
  BuildRectangle();
}

TEST_F(RendererTest, SetTextureImageUnitRange) {
  // Test that all textures that share the same uniform are bound to the same
  // texture unit.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  s_data.rect->ClearChildren();
  s_data.rect->ClearUniforms();
  s_data.rect->ClearShapes();

  s_data.rect->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));
  s_data.rect->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uCubeMapTexture", s_data.cubemap));

  // Add many nodes with different textures bound to the same uniform; they
  // should all share the same image unit.
  static const int kNumNodes = 4;
  for (int i = 0; i < kNumNodes; ++i) {
    NodePtr node(new Node);

    TexturePtr texture(new Texture);
    texture->SetLabel("Texture_a " + base::ValueToString(i));
    texture->SetImage(0U, s_data.image);
    texture->SetSampler(s_data.sampler);
    node->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
        "uTexture", texture));

    texture = new Texture;
    texture->SetLabel("Texture_b " + base::ValueToString(i));
    texture->SetImage(0U, s_data.image);
    texture->SetSampler(s_data.sampler);
    node->AddUniform(
        s_data.shader->GetRegistry()->Create<Uniform>("uTexture2", texture));

    s_data.rect->AddChild(node);
  }
  Reset();

  // Add shapes to force GL calls.
  for (int i = 0; i < kNumNodes; ++i)
    s_data.rect->GetChildren()[i]->AddShape(s_data.shape);

  // Use two texture units.
  Reset();
  renderer->SetTextureImageUnitRange(Range1i(0, 1));
  renderer->DrawScene(root);
  EXPECT_EQ(14U, trace_verifier_->GetCountOf("TexImage2D"));
  // There should be 12 calls to ActiveTexture since we ping pong back and forth
  // between the two units for three textures. There are 12 binds, and we aren't
  // starting from 0.
  EXPECT_EQ(12U, trace_verifier_->GetCountOf("ActiveTexture"));
  // The texture uniforms are sent exactly once.
  EXPECT_EQ(12U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));

  // Use one texture unit.
  Reset();
  renderer->SetTextureImageUnitRange(Range1i(0, 0));
  renderer->DrawScene(root);
  // The textures are already updated.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  // There should be only one call to ActiveTexture since there is only one
  // unit.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture"));
  // The texture uniforms are only sent once, and the first already has the
  // right value.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));

  // Use three texture units.
  Reset();
  renderer->SetTextureImageUnitRange(Range1i(3, 5));
  renderer->DrawScene(root);
  // The textures are already updated.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  // There should be 9 calls to ActiveTexture since we ping pong back and forth
  // between the 3 units for 3 textures. The cubemap gets a single unit and
  // reuses it, while the other textures each requre rebinding.
  EXPECT_EQ(9U, trace_verifier_->GetCountOf("ActiveTexture"));
  // The texture uniforms are only sent once since we have exactly the right
  // number of units.
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE3)"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE4)"));
  // This is used for the cubemap.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE5)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE6)"));

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, ArrayUniforms) {
  // Add array uniform types to a node and make sure the right functions are
  // called in the renderer.
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();

  // Dummy shader with the uniforms defined.
  static const char* kVertexShaderString =
      "uniform int uInt;\n"
      "uniform float uFloat;\n"
      "uniform vec2 uFV2;\n"
      "uniform vec3 uFV3;\n"
      "uniform vec4 uFV4;\n"
      "uniform ivec2 uIV2;\n"
      "uniform ivec3 uIV3;\n"
      "uniform ivec4 uIV4;\n"
      "uniform mat2 uMat2;\n"
      "uniform mat3 uMat3;\n"
      "uniform mat4 uMat4;\n"
      "uniform sampler2D sampler;\n"
      "uniform samplerCube cubeSampler;\n"
      "uniform int uIntArray[2];\n"
      "uniform float uFloatArray[2];\n"
      "uniform vec2 uFV2Array[2];\n"
      "uniform vec3 uFV3Array[3];\n"
      "uniform vec4 uFV4Array[4];\n"
      "uniform ivec2 uIV2Array[2];\n"
      "uniform ivec3 uIV3Array[3];\n"
      "uniform ivec4 uIV4Array[4];\n"
      "uniform mat2 uMat2Array[2];\n"
      "uniform mat3 uMat3Array[3];\n"
      "uniform mat4 uMat4Array[4];\n"
      "uniform sampler2D samplerArray[2];\n"
      "uniform samplerCube cubeSamplerArray[2];\n";

  // One of each uniform type.
  RendererPtr renderer(new Renderer(gm_));

  NodePtr root = BuildGraph(kWidth, kHeight);
  root->ClearUniforms();
  root->ClearUniformBlocks();
  s_data.rect->ClearUniforms();
  s_data.rect->ClearUniformBlocks();

  // add all the uniforms here
  ShaderProgramPtr program(new ShaderProgram(reg));
  program->SetLabel("Dummy Shader");
  program->SetVertexShader(ShaderPtr(new Shader(kVertexShaderString)));
  program->SetFragmentShader(
      ShaderPtr(new Shader("Dummy Fragment Shader Source")));
  s_data.rect->SetShaderProgram(program);
  // Remove attribute array to prevent warnings; we are only testing uniforms
  // here.
  s_data.shape->SetAttributeArray(AttributeArrayPtr(NULL));

  root->AddUniform(reg->Create<Uniform>("uInt", 13));
  root->AddUniform(reg->Create<Uniform>("uFloat", 1.5f));
  root->AddUniform(
      reg->Create<Uniform>("uFV2", math::Vector2f(2.f, 3.f)));
  root->AddUniform(
      reg->Create<Uniform>("uFV3", math::Vector3f(4.f, 5.f, 6.f)));
  root->AddUniform(reg->Create<Uniform>(
      "uFV4", math::Vector4f(7.f, 8.f, 9.f, 10.f)));
  root->AddUniform(reg->Create<Uniform>("uIV2",
                                        math::Vector2i(2, 3)));
  root->AddUniform(reg->Create<Uniform>("uIV3",
                                        math::Vector3i(4, 5, 6)));
  root->AddUniform(reg->Create<Uniform>("uIV4",
                                        math::Vector4i(7, 8, 9, 10)));
  root->AddUniform(reg->Create<Uniform>("uMat2",
                                        math::Matrix2f(1.f, 2.f,
                                                       3.f, 4.f)));
  root->AddUniform(reg->Create<Uniform>("uMat3",
                                        math::Matrix3f(1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f,
                                                       7.f, 8.f, 9.f)));
  root->AddUniform(reg->Create<Uniform>("uMat4",
                                        math::Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                       5.f, 6.f, 7.f, 8.f,
                                                       9.f, 1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f, 7.f)));
  root->AddUniform(reg->Create<Uniform>("sampler", s_data.texture));
  root->AddUniform(reg->Create<Uniform>("cubeSampler", s_data.cubemap));

  TexturePtr texture1(new Texture);
  texture1->SetImage(0U, s_data.image);
  texture1->SetSampler(s_data.sampler);
  TexturePtr texture2(new Texture);
  texture2->SetImage(0U, s_data.image);
  texture2->SetSampler(s_data.sampler);
  CubeMapTexturePtr cubemap1(new CubeMapTexture);
  cubemap1->SetSampler(s_data.sampler);
  CubeMapTexturePtr cubemap2(new CubeMapTexture);
  cubemap2->SetSampler(s_data.sampler);  for (int i = 0; i < 6; ++i) {
    cubemap1->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                       s_data.image);
    cubemap2->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                       s_data.image);
  }

  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  std::vector<float> floats;
  floats.push_back(1.f);
  floats.push_back(2.f);
  std::vector<TexturePtr> textures;
  textures.push_back(texture1);
  textures.push_back(texture2);
  std::vector<CubeMapTexturePtr> cubemaps;
  cubemaps.push_back(cubemap1);
  cubemaps.push_back(cubemap2);
  std::vector<math::Vector2i> vector2is;
  vector2is.push_back(math::Vector2i(1, 2));
  vector2is.push_back(math::Vector2i(3, 4));
  std::vector<math::Vector3i> vector3is;
  vector3is.push_back(math::Vector3i(1, 2, 3));
  vector3is.push_back(math::Vector3i(4, 5, 6));
  std::vector<math::Vector4i> vector4is;
  vector4is.push_back(math::Vector4i(1, 2, 3, 4));
  vector4is.push_back(math::Vector4i(5, 6, 7, 8));
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
  matrix2fs.push_back(math::Matrix2f::Identity());
  std::vector<math::Matrix3f> matrix3fs;
  matrix3fs.push_back(math::Matrix3f::Identity());
  matrix3fs.push_back(math::Matrix3f::Identity());
  std::vector<math::Matrix4f> matrix4fs;
  matrix4fs.push_back(math::Matrix4f::Identity());
  matrix4fs.push_back(math::Matrix4f::Identity());

  root->AddUniform(CreateArrayUniform(reg, "uIntArray", ints));
  root->AddUniform(CreateArrayUniform(reg, "uFloatArray", floats));
  root->AddUniform(CreateArrayUniform(reg, "uIV2Array", vector2is));
  root->AddUniform(CreateArrayUniform(reg, "uIV3Array", vector3is));
  root->AddUniform(CreateArrayUniform(reg, "uIV4Array", vector4is));
  root->AddUniform(CreateArrayUniform(reg, "uFV2Array", vector2fs));
  root->AddUniform(CreateArrayUniform(reg, "uFV3Array", vector3fs));
  root->AddUniform(CreateArrayUniform(reg, "uFV4Array", vector4fs));
  root->AddUniform(CreateArrayUniform(reg, "uMat2Array", matrix2fs));
  root->AddUniform(CreateArrayUniform(reg, "uMat3Array", matrix3fs));
  root->AddUniform(CreateArrayUniform(reg, "uMat4Array", matrix4fs));
  root->AddUniform(CreateArrayUniform(reg, "samplerArray", textures));
  root->AddUniform(CreateArrayUniform(reg, "cubeSamplerArray", cubemaps));

  Reset();
  renderer->DrawScene(root);
  // Verify all the uniform types were sent.
  // 1i.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("Uniform1i("));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("Uniform1iv("));
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        0U, "Uniform1i(")).HasArg(2, "0"));
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        1U, "Uniform1i(")).HasArg(2, "1"));
  // The int uniform.
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        0U, "Uniform1iv(")).HasArg(2, "1"));
  // The int array uniform.
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        1U, "Uniform1iv(")).HasArg(2, "2"));
  // Textures need to be unique.

  // The texture array uniform.
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        2U, "Uniform1iv(")).HasArg(2, "2"));
  // The cubemap array uniform.
  EXPECT_TRUE(
      trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                        3U, "Uniform1iv(")).HasArg(2, "2"));

  for (int i = 2; i < 4; ++i) {
    const std::string f_name =
        std::string("Uniform") + base::ValueToString(i) + "f";
    const std::string i_name =
        std::string("Uniform") + base::ValueToString(i) + "i";
    const std::string mat_name =
        std::string("UniformMatrix") + base::ValueToString(i) + "fv";
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(f_name + "("));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf(f_name + "v("));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          0U, f_name + "v(")).HasArg(2, "1"));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          1U, f_name + "v(")).HasArg(2, "2"));

    EXPECT_EQ(0U, trace_verifier_->GetCountOf(i_name + "("));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf(i_name + "v("));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          0U, i_name + "v(")).HasArg(2, "1"));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          1U, i_name + "v(")).HasArg(2, "2"));

    EXPECT_EQ(3U, trace_verifier_->GetCountOf(mat_name));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          0U, mat_name)).HasArg(2, "1"));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          1U, mat_name)).HasArg(2, "1"));
    EXPECT_TRUE(
        trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                          2U, mat_name)).HasArg(2, "1"));
  }

  Reset();
  renderer->DrawScene(root);
  // Everything should be cached.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform"));

  // Ensure that the array textures are evicted.
  root->SetShaderProgram(program);
  // Remove attribute array to prevent warnings; we are only testing uniforms
  // here.
  root->AddShape(s_data.shape);

  // These uniforms are the same as those contained by root.
  s_data.rect->AddUniform(reg->Create<Uniform>("uInt", 13));
  s_data.rect->AddUniform(reg->Create<Uniform>("uFloat", 1.5f));
  s_data.rect->AddUniform(
      reg->Create<Uniform>("uFV2", math::Vector2f(2.f, 3.f)));
  s_data.rect->AddUniform(
      reg->Create<Uniform>("uFV3", math::Vector3f(4.f, 5.f, 6.f)));
  s_data.rect->AddUniform(reg->Create<Uniform>(
      "uFV4", math::Vector4f(7.f, 8.f, 9.f, 10.f)));
  s_data.rect->AddUniform(reg->Create<Uniform>("uIV2",
                                        math::Vector2i(2, 3)));
  s_data.rect->AddUniform(reg->Create<Uniform>("uIV3",
                                        math::Vector3i(4, 5, 6)));
  s_data.rect->AddUniform(reg->Create<Uniform>("uIV4",
                                        math::Vector4i(7, 8, 9, 10)));
  s_data.rect->AddUniform(reg->Create<Uniform>("uMat2",
                                        math::Matrix2f(1.f, 2.f,
                                                       3.f, 4.f)));
  s_data.rect->AddUniform(reg->Create<Uniform>("uMat3",
                                        math::Matrix3f(1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f,
                                                       7.f, 8.f, 9.f)));
  s_data.rect->AddUniform(reg->Create<Uniform>("uMat4",
                                        math::Matrix4f(1.f, 2.f, 3.f, 4.f,
                                                       5.f, 6.f, 7.f, 8.f,
                                                       9.f, 1.f, 2.f, 3.f,
                                                       4.f, 5.f, 6.f, 7.f)));
  s_data.rect->AddUniform(reg->Create<Uniform>("sampler", s_data.texture));
  s_data.rect->AddUniform(reg->Create<Uniform>("cubeSampler", s_data.cubemap));

  // Reverse following uniform arrays so they are different than those in root.
  std::reverse(ints.begin(), ints.end());
  std::reverse(floats.begin(), floats.end());
  std::reverse(textures.begin(), textures.end());
  std::reverse(cubemaps.begin(), cubemaps.end());
  std::reverse(vector2fs.begin(), vector2fs.end());
  std::reverse(vector3fs.begin(), vector3fs.end());
  std::reverse(vector4fs.begin(), vector4fs.end());
  std::reverse(vector2is.begin(), vector2is.end());
  std::reverse(vector3is.begin(), vector3is.end());
  std::reverse(vector4is.begin(), vector4is.end());
  std::reverse(matrix2fs.begin(), matrix2fs.end());
  std::reverse(matrix3fs.begin(), matrix3fs.end());
  std::reverse(matrix4fs.begin(), matrix4fs.end());
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uIntArray", ints));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uFloatArray", floats));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uIV2Array", vector2is));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uIV3Array", vector3is));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uIV4Array", vector4is));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uFV2Array", vector2fs));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uFV3Array", vector3fs));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uFV4Array", vector4fs));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uMat2Array", matrix2fs));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uMat3Array", matrix3fs));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "uMat4Array", matrix4fs));
  s_data.rect->AddUniform(CreateArrayUniform(reg, "samplerArray", textures));
  s_data.rect->AddUniform(
      CreateArrayUniform(reg, "cubeSamplerArray", cubemaps));

  Reset();
  renderer->DrawScene(root);
  // Expect all uniforms to be sent because they have different ids because
  // now s_data.rect uniforms replace those of root.
  EXPECT_EQ(25U, trace_verifier_->GetCountOf("Uniform"));
}

TEST_F(RendererTest, VertexArraysAndEmulator) {
  // Test that vertex arrays are enabled and used. Each test needs a fresh
  // renderer so that resources are initialized from scratch, otherwise
  // a VertexArrayEmulatorResource will not be created, since the resource
  // holder will already have a pointer to a VertexArrayResource.
  NodePtr root;

  root = BuildGraph(kWidth, kHeight);
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    // Vertex arrays should be bound. There is only one bind.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
  }

  // Use the emulator.
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);
  EXPECT_FALSE(gm_->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindVertexArray"));
    EXPECT_GT(trace_verifier_->GetCountOf("VertexAttribPointer"), 0U);
  }

  // Use vertex arrays.
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
    // We should not have to rebind the pointers.
    EXPECT_GT(trace_verifier_->GetCountOf("VertexAttribPointer"), 0U);
  }
}

TEST_F(RendererTest, VertexArrayEmulatorReuse) {
  // Test that when reusing the vertex array emulator, the bind calls are only
  // sent to OpenGL once.
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);

  NodePtr root = BuildGraph(kWidth, kHeight);
  RendererPtr renderer(new Renderer(gm_));
  Reset();
  renderer->DrawScene(root);
  // Vertex arrays are disabled.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindVertexArray"));
  // There are two buffer attributes bound, 1 index buffer, and 1 data buffer.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements"));

  // Drawing again should only draw the shape again, without rebinding or
  // enabling the pointers again.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements"));

  // If the same Shape is used in succession, we also shouldn't see rebinds
  // happen.
  NodePtr node(new Node);
  s_data.rect->AddChild(node);
  node->AddShape(s_data.shape);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DrawElements"));

  // If we modify an attribute then the entire state will be resent due to the
  // notification.
  Attribute* a = s_data.shape->GetAttributeArray()->GetMutableAttribute(0U);
  a->SetFixedPointNormalized(true);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DrawElements"));
  s_data.rect->RemoveChild(node);

  // If a different Shape is used, then the new one will be sent on the first
  // draw.
  ShaderInputRegistryPtr global_reg = ShaderInputRegistry::GetGlobalRegistry();
  AttributeArrayPtr aa(new AttributeArray);
  aa->AddAttribute(global_reg->Create<Attribute>(
      "aVertex", BufferObjectElement(s_data.vertex_buffer,
                                     s_data.vertex_buffer->AddSpec(
                                         BufferObject::kFloat, 3, 0))));
  aa->AddAttribute(global_reg->Create<Attribute>(
      "aTexCoords",
      BufferObjectElement(s_data.vertex_buffer,
                          s_data.vertex_buffer->AddSpec(BufferObject::kFloat, 2,
                                                        sizeof(float) * 3))));
  ShapePtr shape(new Shape);
  shape->SetAttributeArray(aa);
  s_data.rect->AddShape(shape);

  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays"));

  // Drawing again should rebind both Shapes.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawElements"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays"));

  s_data.rect->RemoveShape(shape);
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
}

TEST_F(RendererTest, VertexBufferUsage) {
  // Test vertex buffer usage.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;
  TracingHelper helper;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<BufferObject::UsageMode> verify_data;
  verify_data.update_func = BuildRectangleBufferObject;
  verify_data.call_name = "BufferData";
  verify_data.option = &s_options.vertex_buffer_usage;
  verify_data.static_args.push_back(StaticArg(1, "GL_ARRAY_BUFFER"));
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString(
          "GLsizei", static_cast<int>(sizeof(Vertex) * s_num_vertices))));
  verify_data.static_args.push_back(
      StaticArg(3, helper.ToString("void*",
                                   s_data.vertex_container->GetData())));
  verify_data.varying_arg_index = 4U;
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          0, BufferObject::kDynamicDraw, "GL_DYNAMIC_DRAW"));
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          0, BufferObject::kStaticDraw, "GL_STATIC_DRAW"));
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          0, BufferObject::kStreamDraw, "GL_STREAM_DRAW"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, VertexBufferNoData) {
  // Test handling of NULL, nonexistent, or empty buffer object data containers.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;

  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  s_data.vertex_buffer->SetData(base::DataContainerPtr(NULL), sizeof(Vertex),
                                s_num_vertices, s_options.vertex_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "DataContainer is NULL"));

  Vertex* vertices = new Vertex[2];
  vertices[0].point_coords.Set(-1, 0,  1);
  vertices[0].tex_coords.Set(0.f, 1.f);
  vertices[1].point_coords.Set(1, 0, 1);
  vertices[1].tex_coords.Set(1.f, 1.f);
  base::DataContainerPtr data =
      base::DataContainer::Create<Vertex>(
          vertices, base::DataContainer::ArrayDeleter<Vertex>, true,
          s_data.vertex_buffer->GetAllocator());
  s_data.vertex_buffer->SetData(data, 0U, s_num_vertices,
                                s_options.vertex_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "struct size is 0"));

  s_data.vertex_buffer->SetData(data, sizeof(vertices[0]), 0U,
                                s_options.vertex_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "struct count is 0"));

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, VertexBufferSubData) {
  // Test handling of BufferObject sub-data.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);

  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  Vertex* vertices = new Vertex[2];
  vertices[0].point_coords.Set(-1, 0,  1);
  vertices[0].tex_coords.Set(0.f, 1.f);
  vertices[1].point_coords.Set(1, 0, 1);
  vertices[1].tex_coords.Set(1.f, 1.f);
  base::DataContainerPtr sub_data =
      base::DataContainer::Create<Vertex>(
          vertices, base::DataContainer::ArrayDeleter<Vertex>, true,
          s_data.vertex_buffer->GetAllocator());

  math::Range1ui range(0, static_cast<unsigned int>(sizeof(vertices[0]) * 2));
  s_data.vertex_buffer->SetSubData(range, sub_data);
  Reset();
  renderer->DrawScene(root);
  // Buffer sub-data does not affect memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferSubData(GL_ARRAY_BUFFER"));

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, IndexBufferUsage) {
  // Test index buffer usage.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;
  TracingHelper helper;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<BufferObject::UsageMode> verify_data;
  verify_data.update_func = BuildRectangleShape<uint16>;
  verify_data.call_name = "BufferData";
  verify_data.option = &s_options.index_buffer_usage;
  verify_data.static_args.push_back(StaticArg(1, "GL_ELEMENT_ARRAY_BUFFER"));
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString(
          "GLsizei", static_cast<int>(sizeof(uint16) * s_num_indices))));
  verify_data.static_args.push_back(
      StaticArg(3, helper.ToString(
          "void*", s_data.index_container->GetData())));
  verify_data.varying_arg_index = 4U;
  // It's the second call in this case because the vertex buffer is bound first
  // since this is the initial draw.
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          1, BufferObject::kDynamicDraw, "GL_DYNAMIC_DRAW"));
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          0, BufferObject::kStaticDraw, "GL_STATIC_DRAW"));
  verify_data.arg_tests.push_back(
      VaryingArg<BufferObject::UsageMode>(
          0, BufferObject::kStreamDraw, "GL_STREAM_DRAW"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, ProgramAndShaderInfoLogs) {
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(kWidth, kHeight);;
    Reset();
    renderer->DrawScene(root);
    // Info logs are empty when there are no errors.
    EXPECT_EQ("", s_data.shader->GetInfoLog());
    EXPECT_EQ("", s_data.shader->GetFragmentShader()->GetInfoLog());
    EXPECT_EQ("", s_data.shader->GetVertexShader()->GetInfoLog());
  }

  VerifyFunctionFailure(gm_, "CompileShader", "Unable to compile");
  // Check that the info log was set.
  EXPECT_EQ("Shader compilation is set to always fail.",
            s_data.shader->GetVertexShader()->GetInfoLog());
  EXPECT_EQ("Shader compilation is set to always fail.",
            s_data.shader->GetFragmentShader()->GetInfoLog());
  EXPECT_EQ("", s_data.shader->GetInfoLog());
  // Reset data.
  s_data.rect = NULL;
  s_data.shader = NULL;
  BuildRectangle();

  Reset();
  VerifyFunctionFailure(gm_, "LinkProgram", "Unable to link");
  // Check that the info log was set.
  EXPECT_EQ("", s_data.shader->GetVertexShader()->GetInfoLog());
  EXPECT_EQ("", s_data.shader->GetFragmentShader()->GetInfoLog());
  EXPECT_EQ("Program linking is set to always fail.",
            s_data.shader->GetInfoLog());
}

TEST_F(RendererTest, FunctionFailures) {
  // Misc tests for error handling when some functions fail.
  base::LogChecker log_checker;
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(kWidth, kHeight);;
    Reset();
    renderer->DrawScene(root);
  }
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that Renderer catches failed compilation.
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);
  Reset();
  VerifyFunctionFailure(gm_, "CompileShader", "Unable to compile");
  // Check that Renderer catches failed program creation.
  Reset();
  VerifyFunctionFailure(gm_, "CreateProgram",
                        "Unable to create shader program object");
  // Check that Renderer catches failed shader creation.
  Reset();
  VerifyFunctionFailure(gm_, "CreateShader", "Unable to create shader object");
  // Check that Renderer catches failed linking.
  Reset();
  VerifyFunctionFailure(gm_, "LinkProgram", "Unable to link");

  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
  Reset();
  VerifyFunctionFailure(gm_, "CompileShader", "Unable to compile");
  // Check that Renderer catches failed program creation.

  Reset();
  VerifyFunctionFailure(gm_, "CreateProgram",
                        "Unable to create shader program object");
  // Check that Renderer catches failed shader creation.
  Reset();
  VerifyFunctionFailure(gm_, "CreateShader", "Unable to create shader object");
  // Check that Renderer catches failed linking.
  Reset();
  VerifyFunctionFailure(gm_, "LinkProgram", "Unable to link");

  // Check that Renderer catches failed buffer id generation.
  Reset();
  VerifyFunctionFailure(gm_, "GenBuffers", "Unable to create buffer");
  // Check that Renderer catches failed sampler id generation.
  Reset();
  VerifyFunctionFailure(gm_, "GenSamplers", "Unable to create sampler");
  // Check that Renderer catches failed framebuffer id generation.
  Reset();
  VerifyFunctionFailure(gm_, "GenFramebuffers", "Unable to create framebuffer");
  // Check that Renderer catches failed renderbuffer id generation.
  Reset();
  VerifyFunctionFailure(gm_, "GenRenderbuffers",
                       "Unable to create renderbuffer");
  // Check that Renderer catches failed texture id generation.
  Reset();
  VerifyFunctionFailure(gm_, "GenTextures", "Unable to create texture");
  // Check that Renderer catches failed vertex array id generation.
  Reset();
  VerifyFunctionFailure(gm_, "GenVertexArrays",
                        "Unable to create vertex array");
}

TEST_F(RendererTest, PrimitiveType) {
  // Test primitive type.
  NodePtr root;
  TracingHelper helper;
  base::LogChecker log_checker;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<Shape::PrimitiveType> verify_data;
  verify_data.update_func = BuildRectangleShape<uint16>;
  verify_data.call_name = "DrawElements";
  verify_data.option = &s_options.primitive_type;
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString("GLsizei", s_num_indices)));
  verify_data.static_args.push_back(StaticArg(3, "GL_UNSIGNED_SHORT"));
  verify_data.static_args.push_back(StaticArg(4, "NULL"));
  verify_data.varying_arg_index = 1U;
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(0, Shape::kLines, "GL_LINES"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(
          0, Shape::kLineLoop, "GL_LINE_LOOP"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(
          0, Shape::kLineStrip, "GL_LINE_STRIP"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(0, Shape::kPoints, "GL_POINTS"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(0, Shape::kTriangles, "GL_TRIANGLES"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(
          0, Shape::kTriangleFan, "GL_TRIANGLE_FAN"));
  verify_data.arg_tests.push_back(
      VaryingArg<Shape::PrimitiveType>(
          0, Shape::kTriangleStrip, "GL_TRIANGLE_STRIP"));
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(
        VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));
  }

  // Check some corner cases.
  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);
  {
    RendererPtr renderer(new Renderer(gm_));
    // Destroy the data in the datacontainer to get an error message.
    s_data.vertex_container = NULL;
    // The attribute_array must be destroyed as well to trigger a rebind.
    s_data.attribute_array = NULL;
    BuildRectangleAttributeArray();
    s_data.vertex_buffer->SetData(
        s_data.vertex_container, sizeof(Vertex), s_num_vertices,
        s_options.vertex_buffer_usage);
    Reset();
    renderer->DrawScene(root);
    // The buffer object should not be updated.
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Draw"));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "Unable to draw shape"));
    // Restore the data.
    BuildRectangleBufferObject();
    BuildRectangleAttributeArray();
  }

  gm_->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
  RendererPtr renderer(new Renderer(gm_));
  // Destroy the data in the datacontainer to get an error message.
  s_data.vertex_container = NULL;
  // The attribute_array must be destroyed as well to trigger a rebind.
  s_data.attribute_array = NULL;
  BuildRectangleAttributeArray();
  s_data.vertex_buffer->SetData(
      s_data.vertex_container, sizeof(Vertex), s_num_vertices,
      s_options.vertex_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  // The buffer object should not be updated.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Draw"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Unable to draw shape"));
  // Restore the data.
  BuildRectangleBufferObject();
  BuildRectangleAttributeArray();

  // Do the same with the index buffer.
  s_data.index_container = NULL;
  s_data.index_buffer->SetData(s_data.index_container, sizeof(uint16),
                               s_num_indices, s_options.index_buffer_usage);
  Reset();
  renderer->DrawScene(root);
  // The index buffer object should not be updated.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf(
      "BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf(
      "BufferData(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Draw"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Unable to draw shape"));
  // Restore the data.
  BuildRectangleShape<uint16>();

  // Check that the shape is not drawn if the IndexBuffer has no indices.
  s_data.shape->SetIndexBuffer(IndexBufferPtr(new IndexBuffer));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Draw"));
  s_data.shape->SetIndexBuffer(s_data.index_buffer);

  // Check that if there are no index buffers then DrawArrays is used. By
  // default, all vertices should be used.
  s_data.shape->SetPrimitiveType(Shape::kPoints);
  s_data.shape->SetIndexBuffer(IndexBufferPtr(NULL));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DrawElements"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 0, 4)"));
  // Try different vertex range settings.
  s_data.shape->AddVertexRange(Range1i(1, 3));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 1, 2)"));
  s_data.shape->AddVertexRange(Range1i(3, 4));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 1, 2)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 3, 1)"));
  s_data.shape->EnableVertexRange(0, false);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 3, 1)"));
  s_data.shape->EnableVertexRange(0, true);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 1, 2)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 3, 1)"));
  s_data.shape->ClearVertexRanges();
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DrawArrays(GL_POINTS, 0, 4)"));
  s_data.shape->SetIndexBuffer(s_data.index_buffer);

  // Check that if the shape has no attribute array that it is not drawn.
  s_data.shape->SetAttributeArray(AttributeArrayPtr(NULL));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Draw"));
  s_data.shape->SetAttributeArray(s_data.attribute_array);

  Reset();
  renderer = NULL;
}

TEST_F(RendererTest, TextureWithZeroDimensionsAreNotAllocated) {
  base::LogChecker log_checker;

  // A default scene should render fine.
  NodePtr root = BuildGraph(kWidth, kHeight);
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));
  }

  s_data.image->Set(s_options.image_format, 0, 0, base::DataContainerPtr());
  SetImages();

  Reset();
  {
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  }

  EXPECT_FALSE(log_checker.HasAnyMessages());
  Build2dImage();
}

TEST_F(RendererTest, Texture3dWarningsWhenDisabled) {
  RendererPtr renderer(new Renderer(gm_));
  base::LogChecker log_checker;

  // A default scene should render fine.
  NodePtr root = BuildGraph(kWidth, kHeight);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Using a 3D image should be no problem if the function group is available.
  Build3dImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_LT(0U, trace_verifier_->GetCountOf("TexImage3D"));

  // Without the function group we get an error.
  gm_->EnableFunctionGroup(GraphicsManager::kTexture3d, false);
  Build3dImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "3D texturing is not supported"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage3D"));

  s_options.image_format = Image::kDxt5;
  gm_->EnableFunctionGroup(GraphicsManager::kTexture3d, true);
  Build3dImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_LT(0U, trace_verifier_->GetCountOf("CompressedTexImage3D"));

  gm_->EnableFunctionGroup(GraphicsManager::kTexture3d, false);
  Build3dImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "3D texturing is not supported"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("CompressedTexImage3D"));

  gm_->EnableFunctionGroup(GraphicsManager::kTexture3d, true);
}

TEST_F(RendererTest, TextureTargets) {
  RendererPtr renderer(new Renderer(gm_));

  // Test usage of TexImage2D.
  NodePtr root = BuildGraph(kWidth, kHeight);
  Reset();
  renderer->DrawScene(root);

  Build1dArrayImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_1D_ARRAY"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_CUBE"));

  Build2dImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_2D, "));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_CUBE"));

  Build2dArrayImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage3D(GL_TEXTURE_2D_ARRAY"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage3D(GL_TEXTURE_CUBE"));

  Build3dImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage3D(GL_TEXTURE_3D, "));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage3D(GL_TEXTURE_CUBE"));

  {
    base::LogChecker log_checker;
    BuildExternalEglImage();
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(7U, trace_verifier_->GetCountOf(
                      "EGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, "));
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "number of components"));
  }

  BuildEglImage();
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(7U, trace_verifier_->GetCountOf(
                    "EGLImageTargetTexture2DOES(GL_TEXTURE_2D, "));
  gm_->EnableFunctionGroup(GraphicsManager::kTexture3d, true);
}

TEST_F(RendererTest, ImageFormat) {
  // Test image format usage.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  TracingHelper helper;
  base::LogChecker log_checker;

  // Save image dimensions.
  const uint32 width_2d = s_data.image->GetWidth();
  const uint32 height_2d = s_data.image->GetHeight();

  Build3dImage();
  const uint32 width_3d = s_data.image->GetWidth();
  const uint32 height_3d = s_data.image->GetHeight();
  const uint32 depth_3d = s_data.image->GetDepth();

  // Test usage of TexImage2D.
  VerifyRenderData<Image::Format> verify_2d_data;
  verify_2d_data.update_func = Build2dImage;
  verify_2d_data.option = &s_options.image_format;
  verify_2d_data.call_name = "TexImage2D";
  verify_2d_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
  verify_2d_data.static_args.push_back(StaticArg(2, "0"));
  verify_2d_data.static_args.push_back(
      StaticArg(4, helper.ToString("GLsizei", static_cast<GLsizei>(width_2d))));
  verify_2d_data.static_args.push_back(StaticArg(
      5, helper.ToString("GLsizei", static_cast<GLsizei>(height_2d))));
  verify_2d_data.static_args.push_back(StaticArg(7, "0"));  // Format.
  verify_2d_data.static_args.push_back(StaticArg(8, "0"));  // Type.
  verify_2d_data.static_args.push_back(
      StaticArg(9, helper.ToString(
          "void*", s_data.image_container->GetData())));
  verify_2d_data.varying_arg_index = 3U;

  // Test usage of TexImage3D.
  VerifyRenderData<Image::Format> verify_3d_data;
  verify_3d_data.update_func = Build3dImage;
  verify_3d_data.option = &s_options.image_format;
  verify_3d_data.call_name = "TexImage3D";
  verify_3d_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_3D"));
  verify_3d_data.static_args.push_back(StaticArg(2, "0"));
  verify_3d_data.static_args.push_back(
      StaticArg(4, helper.ToString("GLsizei", static_cast<GLsizei>(width_3d))));
  verify_3d_data.static_args.push_back(StaticArg(
      5, helper.ToString("GLsizei", static_cast<GLsizei>(height_3d))));
  verify_3d_data.static_args.push_back(
      StaticArg(6, helper.ToString("GLsizei", static_cast<GLsizei>(depth_3d))));
  verify_3d_data.static_args.push_back(StaticArg(8, "0"));  // Format.
  verify_3d_data.static_args.push_back(StaticArg(9, "0"));  // Type.
  verify_3d_data.static_args.push_back(
      StaticArg(10, helper.ToString(
          "void*", s_data.image_container->GetData())));
  verify_3d_data.varying_arg_index = 3U;

  int last_component_count = 0;
  for (uint32 i = 0; i < Image::kNumFormats - 1U; ++i) {
    const Image::Format format = static_cast<Image::Format>(i);
    if (!Image::IsCompressedFormat(format)) {
      const Image::PixelFormat& pf = Image::GetPixelFormat(format);
      if (pf.internal_format != GL_STENCIL_INDEX8) {
        verify_2d_data.static_args[4] =
            StaticArg(7, helper.ToString("GLenum", pf.format));
        verify_2d_data.static_args[5] =
            StaticArg(8, helper.ToString("GLenum", pf.type));
        verify_3d_data.static_args[5] =
            StaticArg(8, helper.ToString("GLenum", pf.format));
        verify_3d_data.static_args[6] =
            StaticArg(9, helper.ToString("GLenum", pf.type));
        verify_2d_data.arg_tests.clear();
        verify_3d_data.arg_tests.clear();
        VaryingArg<Image::Format> arg = VaryingArg<Image::Format>(
            0, format, helper.ToString("GLenum", pf.internal_format));
        verify_2d_data.arg_tests.push_back(arg);
        verify_3d_data.arg_tests.push_back(arg);
        EXPECT_TRUE(
            VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));
        const int component_count = Image::GetNumComponentsForFormat(format);
        if (last_component_count && component_count < last_component_count) {
          EXPECT_TRUE(log_checker.HasMessage(
              "WARNING", "the number of components for this upload is"));
        } else {
          EXPECT_FALSE(log_checker.HasAnyMessages());
        }
        EXPECT_TRUE(
            VerifyRenderCalls(verify_3d_data, trace_verifier_, renderer, root));
        EXPECT_FALSE(log_checker.HasAnyMessages());
        last_component_count = component_count;
      }
    }
  }

  // Test deprecation of luminance and luminance-alpha textures on newer desktop
  // GL. In the following paragraph, static_args[4] corresponds to the pixel
  // format (the 7th arg to glTexImage2D), and arg_tests[0] corresponds to the
  // internal format (the 3rd arg to glTexImage2D).
  verify_2d_data.static_args[5] = StaticArg(8, "GL_UNSIGNED_BYTE");

  // Luminance remains luminance in OpenGL 2.9.
  gm_->SetVersionString("2.9 Ion OpenGL");
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kLuminance, "GL_LUMINANCE");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_LUMINANCE");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // R8 becomes luminance in OpenGL 2.9.
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kR8, "GL_LUMINANCE");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_LUMINANCE");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // Luminance becomes R8 in OpenGL 3.0.
  gm_->SetVersionString("3.0 Ion OpenGL");
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kLuminance, "GL_R8");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_RED");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // R8 remains R8 in OpenGL 3.0.
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kR8, "GL_R8");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_RED");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // Luminance/alpha remains luminance/alpha in OpenGL 2.9.
  gm_->SetVersionString("2.9 Ion OpenGL");
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(
          0, Image::kLuminanceAlpha, "GL_LUMINANCE_ALPHA");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_LUMINANCE_ALPHA");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // RG8 becomes luminance/alpha in OpenGL 2.9.
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kRg8, "GL_LUMINANCE_ALPHA");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_LUMINANCE_ALPHA");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // Luminance/alpha becomes RG8 in OpenGL 3.0.
  gm_->SetVersionString("3.0 Ion OpenGL");
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kLuminanceAlpha, "GL_RG8");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_RG");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  // RG8 remains RG8 in OpenGL 3.0.
  verify_2d_data.arg_tests[0] =
      VaryingArg<Image::Format>(0, Image::kRg8, "GL_RG8");
  verify_2d_data.static_args[4] = StaticArg(7, "GL_RG");
  EXPECT_TRUE(
      VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));

  gm_->SetVersionString("3.3 Ion OpenGL / ES");

  // Test compressed formats.
  Image::Format compressed_formats[] = {
      Image::kDxt1, Image::kEtc1,        Image::kPvrtc1Rgb4,
      Image::kDxt5, Image::kPvrtc1Rgba2, Image::kPvrtc1Rgba4};
  const int num_compressed_formats =
      static_cast<int>(arraysize(compressed_formats));

  verify_3d_data.arg_tests.clear();
  verify_3d_data.static_args.clear();
  verify_3d_data.update_func = Build3dImage;
  verify_3d_data.call_name = "CompressedTexImage3D";
  verify_3d_data.option = &s_options.image_format;
  verify_3d_data.static_args.push_back(StaticArg(2, "0"));
  verify_3d_data.static_args.push_back(
      StaticArg(4, helper.ToString("GLsizei", static_cast<GLsizei>(width_3d))));
  verify_3d_data.static_args.push_back(StaticArg(
      5, helper.ToString("GLsizei", static_cast<GLsizei>(height_3d))));
  verify_3d_data.static_args.push_back(
      StaticArg(6, helper.ToString("GLsizei", static_cast<GLsizei>(depth_3d))));
  verify_3d_data.static_args.push_back(StaticArg(7, "0"));
  verify_3d_data.static_args.push_back(
      StaticArg(9, helper.ToString(
          "void*", s_data.image_container->GetData())));
  verify_3d_data.varying_arg_index = 3U;

  verify_2d_data.update_func = Build2dImage;
  verify_2d_data.arg_tests.clear();
  verify_2d_data.static_args.clear();
  verify_2d_data.call_name = "CompressedTexImage2D";
  verify_2d_data.option = &s_options.image_format;
  verify_2d_data.static_args.push_back(StaticArg(2, "0"));
  verify_2d_data.static_args.push_back(
      StaticArg(4, helper.ToString("GLsizei", static_cast<GLsizei>(width_2d))));
  verify_2d_data.static_args.push_back(StaticArg(
      5, helper.ToString("GLsizei", static_cast<GLsizei>(height_2d))));
  verify_2d_data.static_args.push_back(StaticArg(6, "0"));
  verify_2d_data.static_args.push_back(
      StaticArg(8, helper.ToString(
          "void*", s_data.image_container->GetData())));
  verify_2d_data.varying_arg_index = 3U;

  verify_2d_data.arg_tests.push_back(
      VaryingArg<Image::Format>(
          0, Image::kDxt1, "GL_COMPRESSED_RGB_S3TC_DXT1_EXT"));
  verify_2d_data.static_args.push_back(StaticArg(
      7, helper.ToString("GLsizei", static_cast<GLsizei>(Image::ComputeDataSize(
                                        Image::kDxt1, width_2d, height_2d)))));
  verify_3d_data.arg_tests.push_back(verify_2d_data.arg_tests[0]);
  verify_3d_data.static_args.push_back(StaticArg(
      8, helper.ToString("GLsizei",
                         static_cast<GLsizei>(Image::ComputeDataSize(
                             Image::kDxt1, width_3d, height_3d, depth_3d)))));

  for (int i = 0; i < num_compressed_formats; ++i) {
    const Image::Format format = compressed_formats[i];
    SCOPED_TRACE(Image::GetFormatString(format));
    verify_3d_data.arg_tests[0] = verify_2d_data.arg_tests[0] =
        VaryingArg<Image::Format>(
            0, format,
            helper.ToString("GLenum",
                            Image::GetPixelFormat(format).internal_format));
    verify_2d_data.static_args[5] = StaticArg(
        7,
        helper.ToString("GLsizei", static_cast<GLsizei>(Image::ComputeDataSize(
                                       format, width_2d, height_2d))));
    verify_3d_data.static_args[6] = StaticArg(
        8, helper.ToString("GLsizei",
                           static_cast<GLsizei>(Image::ComputeDataSize(
                               format, width_3d, height_3d, depth_3d))));
    EXPECT_TRUE(
        VerifyRenderCalls(verify_2d_data, trace_verifier_, renderer, root));
    // The first time through there is a warning about having fewer components
    // since the last image sent above was RGBA.
    if (!i)
      EXPECT_TRUE(log_checker.HasMessage(
          "WARNING", "the number of components for this upload is"));
    else
      EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(
        VerifyRenderCalls(verify_3d_data, trace_verifier_, renderer, root));
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, FramebufferObject) {
  s_options.image_format = Image::kRgba8888;
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgb888);

  uint32 texture_width = s_data.texture->GetImage(0U)->GetWidth();
  uint32 texture_height = s_data.texture->GetImage(0U)->GetHeight();
  s_data.fbo = new FramebufferObject(0, texture_height);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "zero width or height"));

  s_data.fbo = new FramebufferObject(texture_width, 0);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "zero width or height"));

  s_data.fbo = new FramebufferObject(texture_width, texture_height);

  {
    RendererPtr renderer(new Renderer(gm_));
    // Test an incomplete framebuffer.
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Framebuffer is not complete"));

    // Check no calls are made if there is no node.
    Reset();
    renderer->DrawScene(NodePtr());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
  }

  {
    // Check a texture color attachment.
    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.texture));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindFramebuffer"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D"));

    // Should not be multisampled.
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));

    // Check that the texture was generated and bound before being bound as
    // the framebuffer's attachment.
    EXPECT_LT(trace_verifier_->GetNthIndexOf(0U, "TexImage2D"),
              trace_verifier_->GetNthIndexOf(
                  0U, "FramebufferTexture2D(GL_RENDERBUFFER"));

    // Check args to FramebufferTexture2D.
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
          trace_verifier_->GetNthIndexOf(0U, "FramebufferTexture2D"))
              .HasArg(3, "GL_TEXTURE_2D"));

    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  {
    // Check that the current fbo follows the current visual.
    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.texture));
    FramebufferObjectPtr fbo2(
        new FramebufferObject(texture_width, texture_height));
    fbo2->SetColorAttachment(0U, FramebufferObject::Attachment(s_data.texture));

    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());

    MockVisual share_visual(*visual_);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    portgfx::Visual::MakeCurrent(&share_visual);
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(fbo2);
    EXPECT_EQ(fbo2.Get(), renderer->GetCurrentFramebuffer().Get());

    portgfx::Visual::MakeCurrent(visual_.get());
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());

    portgfx::Visual::MakeCurrent(&share_visual);
    EXPECT_EQ(fbo2.Get(), renderer->GetCurrentFramebuffer().Get());
    // Destroy the shared resource binder.
    Renderer::DestroyCurrentStateCache();
    portgfx::Visual::MakeCurrent(visual_.get());
  }

  {
    // Check a texture color attachment that uses mipmaps.
    // Set a full image pyramid.
    for (uint32 i = 0; i < kNumMipmaps; ++i)
      s_data.texture->SetImage(i, mipmaps[i]);

    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.texture));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    // Set up fbo to draw into first mip level.
    FramebufferObjectPtr mip_fbo(new FramebufferObject(
        texture_width >> 1, texture_height >> 1));
    mip_fbo->SetColorAttachment(0U,
        FramebufferObject::Attachment(s_data.texture, 1));
    Reset();
    renderer->BindFramebuffer(mip_fbo);
    EXPECT_EQ(mip_fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    // Expect mismatched dimensions error.
    mip_fbo->SetColorAttachment(0U,
        FramebufferObject::Attachment(s_data.texture, 0));
    Reset();
    renderer->BindFramebuffer(mip_fbo);
    EXPECT_EQ(mip_fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    s_msg_stream_ << "Mismatched Texture and FBO dimensions: 32 x 32 "
        "vs. 16 x 16";
    EXPECT_TRUE(log_checker.HasMessage("ERROR", s_msg_stream_.str()));
  }

  {
    // Check a cubemap color attachment.
    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.cubemap,
                                          CubeMapTexture::kPositiveX));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  {
    // Check a cubemap color attachment that uses mipmaps.
    // Set a full image pyramid for each face.
    for (int j = 0; j < 6; ++j) {
      for (uint32 i = 0; i < kNumMipmaps; ++i)
        s_data.cubemap->SetImage(
            static_cast<CubeMapTexture::CubeFace>(j), i, mipmaps[i]);
    }

    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.cubemap,
                                          CubeMapTexture::kPositiveZ));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    // Set up fbo to draw into first mip level.
    FramebufferObjectPtr mip_fbo(
        new FramebufferObject(texture_width >> 1, texture_height >> 1));
    mip_fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.cubemap,
                                          CubeMapTexture::kPositiveZ, 1));
    Reset();
    renderer->BindFramebuffer(mip_fbo);
    EXPECT_EQ(mip_fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());

    // Expect mismatched dimensions error.
    mip_fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.cubemap,
                                          CubeMapTexture::kPositiveZ, 0));
    Reset();
    renderer->BindFramebuffer(mip_fbo);
    EXPECT_EQ(mip_fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    s_msg_stream_ << "Mismatched CubeMapTexture and FBO dimensions: 32 x 32 "
        "vs. 16 x 16";
    EXPECT_TRUE(log_checker.HasMessage("ERROR", s_msg_stream_.str()));
  }

  {
    // Check renderbuffer types.
    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgba4Byte));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "FramebufferRenderbuffer(GL_FRAMEBUFFER"))
                .HasArg(4, "0x1"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_RGBA4))));

    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgb565Byte));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_RGB565))));

    static uint8 kData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                              0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
    ImagePtr egl_image(new Image);
    egl_image->SetEglImage(base::DataContainer::Create<void>(
        kData, kNullFunction, false, egl_image->GetAllocator()));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(egl_image));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "EGLImageTargetRenderbufferStorageOES"));
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "FramebufferRenderbuffer(GL_FRAMEBUFFER"))
                .HasArg(4, "0x1"));

    egl_image->SetEglImage(base::DataContainer::Create<void>(
        nullptr, kNullFunction, false, egl_image->GetAllocator()));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    // Since the buffer is nullptr nothing will be set.  This isn't an error
    // since the caller could just set it manually through OpenGL directly.
    EXPECT_EQ(0U, trace_verifier_->GetCountOf(
                      "EGLImageTargetRenderbufferStorageOES"));

    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgb5a1Byte));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_RGB5_A1))));

    s_data.fbo->SetDepthAttachment(
        FramebufferObject::Attachment(Image::kRenderbufferDepth16));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_DEPTH_COMPONENT16))));

    s_data.fbo->SetStencilAttachment(
        FramebufferObject::Attachment(Image::kStencil8));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorage(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLenum", static_cast<GLenum>(GL_STENCIL_INDEX8))));
  }

  {
    // Check color render buffer for multisamping.
    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgba8, 4));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorageMultisample(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLsizei", static_cast<GLsizei>(4))));

    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(Image::kRgba8, 2));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorageMultisample(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLsizei", static_cast<GLsizei>(2))));
  }

  {
    // Use a new fbo instead of the old one since we don't care about
    // color buffer.
    s_data.fbo = new FramebufferObject(texture_width, texture_height);
    // Check depth render buffer for multisamping.
    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetDepthAttachment(FramebufferObject::Attachment(
        kDepthBufferFormat, 4));
    Reset();
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    renderer->BindFramebuffer(s_data.fbo);
    EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorageMultisample(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLsizei", static_cast<GLsizei>(4))));

     s_data.fbo->SetDepthAttachment(
         FramebufferObject::Attachment(kDepthBufferFormat, 2));
    Reset();
    renderer->DrawScene(root);
    EXPECT_FALSE(log_checker.HasAnyMessages());
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(
            0U, "RenderbufferStorageMultisample(GL_RENDERBUFFER"))
                .HasArg(2, helper.ToString(
                    "GLsizei", static_cast<GLsizei>(2))));
  }
}

TEST_F(RendererTest, FramebufferObjectMultisampleTextureAttachment) {
  s_options.image_format = Image::kRgba8888;
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  uint32 texture_width = s_data.texture->GetImage(0U)->GetWidth();
  uint32 texture_height = s_data.texture->GetImage(0U)->GetHeight();
  s_data.fbo = new FramebufferObject(texture_width, texture_height);

  // Enable multisampling.
  s_data.texture->SetMultisampling(8, true);

  // Check a texture color attachment.
  RendererPtr renderer(new Renderer(gm_));
  s_data.fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment(s_data.texture));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
  renderer->BindFramebuffer(s_data.fbo);
  EXPECT_EQ(s_data.fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindFramebuffer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D"));

  // Check args to TexImage2DMultisample.
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "TexImage2DMultisample"))
            .HasArg(1, "GL_TEXTURE_2D_MULTISAMPLE")
            .HasArg(2, "8"));

  // Check texture target arg to FramebufferTexture2D.
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(0U, "FramebufferTexture2D"))
            .HasArg(3, "GL_TEXTURE_2D_MULTISAMPLE"));

  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, FramebufferObjectAttachmentsImplicitlyChangedByDraw) {
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  uint32 texture_width = s_data.texture->GetImage(0U)->GetWidth();
  uint32 texture_height = s_data.texture->GetImage(0U)->GetHeight();
  s_data.fbo = new FramebufferObject(texture_width, texture_height);

  {
    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.texture));
    s_data.sampler->SetAutogenerateMipmapsEnabled(true);
    renderer->BindFramebuffer(s_data.fbo);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
    Reset();
    // Since the contents have changed, we should regenerate mipmaps.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
    Reset();
    // Same thing again.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));

    // Nothing should happen if mipmaps are disabled.
    s_data.sampler->SetAutogenerateMipmapsEnabled(false);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));

    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  // Same thing with a cubemap face attachment.
  {
    RendererPtr renderer(new Renderer(gm_));
    s_data.fbo->SetColorAttachment(
        0U, FramebufferObject::Attachment(s_data.cubemap,
                                          CubeMapTexture::kPositiveX));
    s_data.sampler->SetAutogenerateMipmapsEnabled(true);
    renderer->BindFramebuffer(s_data.fbo);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
    Reset();
    // Since the contents have changed, we should regenerate mipmaps.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
    Reset();
    // Same thing again.
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));

    // Nothing should happen if mipmaps are disabled.
    s_data.sampler->SetAutogenerateMipmapsEnabled(false);
    Reset();
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));

    EXPECT_FALSE(log_checker.HasAnyMessages());
  }
}

TEST_F(RendererTest, CubeMapTextureMipmaps) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Check that each face of a CubeMapTexture with an image is sent as mipmap
  // level 0.
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));
  for (int i = 0; i < 6; ++i) {
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(i + 1U, "TexImage2D"))
            .HasArg(2, "0"));
  }

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid for each face.
  for (int j = 0; j < 6; ++j) {
    for (uint32 i = 0; i < kNumMipmaps; ++i)
      s_data.cubemap->SetImage(
          static_cast<CubeMapTexture::CubeFace>(j), i, mipmaps[i]);
  }

  // Check consistent dimensions.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNumMipmaps * 6U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // The cubemap now has mipmaps, so it's usage has increased by 4/3.
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());

  // Check the right calls were made. First check the level 0 mipmaps.
  for (uint32 j = 0; j < 6; ++j) {
    SCOPED_TRACE(::testing::Message() << "Testing face " << j);
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(j, "TexImage2D"))
            .HasArg(1, helper.ToString(
                "GLenum", base::EnumHelper::GetConstant(
                    static_cast<CubeMapTexture::CubeFace>(j))))
            .HasArg(2, "0")
            .HasArg(4, helper.ToString(
                "GLsizei", static_cast<GLint>(mipmaps[0]->GetWidth())))
            .HasArg(5, helper.ToString(
                "GLsizei", static_cast<GLint>(mipmaps[0]->GetHeight()))));
  }
  // Now check the level 1+ mipmaps.
  for (uint32 j = 0; j < 6; ++j) {
  SCOPED_TRACE(::testing::Message() << "Testing face " << j);
    for (uint32 i = 1; i < kNumMipmaps; ++i) {
      SCOPED_TRACE(::testing::Message() << "Testing mipmap level " << i);
      EXPECT_TRUE(trace_verifier_->VerifyCallAt(
          trace_verifier_->GetNthIndexOf(6U + j * (kNumMipmaps - 1U) + i - 1U,
                                         "TexImage2D"))
              .HasArg(1, helper.ToString(
                  "GLenum", base::EnumHelper::GetConstant(
                      static_cast<CubeMapTexture::CubeFace>(j))))
              .HasArg(2, helper.ToString("GLint", static_cast<GLint>(i)))
              .HasArg(4, helper.ToString(
                  "GLsizei", static_cast<GLint>(mipmaps[i]->GetWidth())))
              .HasArg(5, helper.ToString(
                  "GLsizei", static_cast<GLint>(mipmaps[i]->GetHeight()))));
    }
  }

  // Remove an image from a few faces and verify that GenerateMipmap is called
  // only once for the entire texture.
  s_data.cubemap->SetImage(CubeMapTexture::kNegativeZ, 1, ImagePtr(NULL));
  s_data.cubemap->SetImage(CubeMapTexture::kPositiveY, 3, ImagePtr(NULL));
  s_data.cubemap->SetImage(CubeMapTexture::kPositiveZ, 2, ImagePtr(NULL));
  Reset();
  renderer->DrawScene(root);
  // Overall memory usage should be unchanged.
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Since GenerateMipmap was called the override mipmaps were sent, but the
  // 0th level mipmap doesn't have to be (GenerateMipmap won't override it).
  // Only the mipmaps of the 3 modified faces are sent.
  EXPECT_EQ((kNumMipmaps - 1U) * 3U - 3U,
            trace_verifier_->GetCountOf("TexImage2D"));

  // Set an invalid image dimension.
  s_data.cubemap->SetImage(
      CubeMapTexture::kPositiveY, 1,
      CreateNullImage(mipmaps[1]->GetWidth() - 1, mipmaps[1]->GetHeight(),
                      Image::kRgba8888));
  Reset();
  renderer->DrawScene(root);
  // Generate mipmap will be called since a mipmap has changed, but no
  // overriding mipmaps will be set since one is invalid.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  s_msg_stream_ << "Mipmap width: " << (mipmaps[1]->GetWidth() - 1) <<
      " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", s_msg_stream_.str()));
  // Overall memory usage should be unchanged since there was an error
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());

  s_data.cubemap->SetImage(
      CubeMapTexture::kPositiveY, 1,
      CreateNullImage(mipmaps[1]->GetWidth(), mipmaps[1]->GetHeight() - 1,
                      Image::kRgba8888));
  Reset();
  renderer->DrawScene(root);
  // Generate mipmap will be called since a mipmap has changed, but no
  // overriding mipmaps will be set since one is invalid.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  s_msg_stream_ << "Mipmap height: " << (mipmaps[1]->GetWidth() - 1) <<
      " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", s_msg_stream_.str()));
  // Overall memory usage should be unchanged since there was an error
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());

  // Set an invalid image dimension.
  s_data.cubemap->SetImage(
      CubeMapTexture::kPositiveY, 1, CreateNullImage(
          mipmaps[1]->GetWidth(), mipmaps[1]->GetHeight(), Image::kRgb888));
  Reset();
  renderer->DrawScene(root);
  // Generate mipmap will be called since a mipmap has changed, but no
  // overriding mipmaps will be set since one is invalid.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(
      log_checker.HasMessage("ERROR", "level 1 has different format"));
  // Overall memory usage should be unchanged since there was an error
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
}

TEST_F(RendererTest, CubeMapTextureSubImages) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Check that a texture with an image is sent as mipmap level 0.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));

  // Now set a subimage on the CubeMapTexture.
  s_data.cubemap->SetSubImage(
      CubeMapTexture::kNegativeZ, 0U, math::Point2ui(12, 20), CreateNullImage(
          4, 8, Image::kRgba8888));
  // Check that the subimage is applied.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexSubImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexSubImage2D"))
          .HasArg(2, "0")
          .HasArg(3, "12")
          .HasArg(4, "20")
          .HasArg(5, "4")
          .HasArg(6, "8")
          .HasArg(7, "GL_RGB")
          .HasArg(8, "GL_UNSIGNED_BYTE"));

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid for each face.
  for (int j = 0; j < 6; ++j) {
    for (uint32 i = 0; i < kNumMipmaps; ++i)
      s_data.cubemap->SetImage(
          static_cast<CubeMapTexture::CubeFace>(j), i, mipmaps[i]);
  }

  // Check consistent dimensions.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNumMipmaps * 6U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());

  // Set a submipmap at level 3. Setting a compressed image requires non-NULL
  // image data.
  ImagePtr compressed_image(new Image);
  compressed_image->Set(
      Image::kDxt5, 4, 8,
      base::DataContainerPtr(base::DataContainer::Create(
          reinterpret_cast<void*>(1), kNullFunction, false,
          compressed_image->GetAllocator())));
  s_data.cubemap->SetSubImage(
      CubeMapTexture::kNegativeZ, 3U, math::Point2ui(12, 8), compressed_image);

  // Check the right call was made.
  Reset();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gm_->GetError());
  renderer->DrawScene(root);
  // Subimages do not resize textures.
  EXPECT_EQ(36864U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(32768U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  // Technically there is an errors since the cubemap is not compressed, but
  // this is just to test that the call is made.
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexSubImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CompressedTexSubImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "CompressedTexSubImage2D"))
          .HasArg(2, "3")
          .HasArg(3, "12")
          .HasArg(4, "8")
          .HasArg(5, "4")
          .HasArg(6, "8")
          .HasArg(7, "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT"));
}

TEST_F(RendererTest, CubeMapTextureMisc) {
  // Test various texture corner cases.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;

  // Check that a texture with no image does not get sent.
  for (int j = 0; j < 6; ++j)
    s_data.cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(j), 0U,
                             ImagePtr(NULL));
  Reset();
  renderer->DrawScene(root);
  // The regular texture is still sent the first time.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "texture face Negative X has no level"));
  Reset();
  s_data.cubemap->SetImage(CubeMapTexture::kNegativeX, 0U, s_data.image);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "texture face Negative Y has no level"));
  Reset();
  s_data.cubemap->SetImage(CubeMapTexture::kNegativeY, 0U, s_data.image);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "texture face Negative Z has no level"));

  // Make the texture valid.
  for (int j = 0; j < 6; ++j)
    s_data.cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(j), 0U,
                             s_data.image);

  // Check that a GenerateMipmap is called if requested.
  s_data.sampler->SetAutogenerateMipmapsEnabled(true);
  Reset();
  renderer->DrawScene(root);
  // Both the cubemap and texture will generate mipmaps.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  // Check that when disabled GenerateMipmap is not called.
  s_data.sampler->SetAutogenerateMipmapsEnabled(false);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check error cases for equal cubemap sizes.
  s_data.image->Set(s_options.image_format, 32, 33, s_data.image_container);
  s_data.sampler->SetMinFilter(Sampler::kLinearMipmapNearest);
  s_data.sampler->SetWrapS(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(
      log_checker.HasMessage("ERROR", "does not have square dimensions"));

  // Check error cases for non-power-of-2 textures.
  s_data.image->Set(s_options.image_format, 30, 30, s_data.image_container);
  s_data.sampler->SetMinFilter(Sampler::kLinearMipmapNearest);
  s_data.sampler->SetWrapS(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  s_data.image->Set(s_options.image_format, 30, 30, s_data.image_container);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  s_data.image->Set(s_options.image_format, 30, 30, s_data.image_container);
  s_data.sampler->SetWrapS(Sampler::kRepeat);
  s_data.sampler->SetWrapT(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  s_data.sampler->SetWrapT(Sampler::kRepeat);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Reset data.
  BuildRectangle();
}

TEST_F(RendererTest, SamplersFollowTextures) {
  // Test that a sampler follows a texture's binding when the texture's bind
  // point changes. This can happen when a set of textures share a sampler and
  // then the bind point changes for all of those textures (e.g., they are bound
  // to a uniform and that uniform's bind point changes).
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  s_data.rect->ClearChildren();
  s_data.rect->ClearUniforms();
  s_data.rect->ClearShapes();

  static const char* kFragmentShaderString = (
      "uniform sampler2D uTexture1;\n"
      "uniform sampler2D uTexture2;\n"
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTexture, vTexCoords);\n"
      "}\n");
  static const char* kBaseShaderString = (
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTexture, vTexCoords);\n"
      "}\n");

  // Create a shader that uses all of the image units.
  GLuint num_textures = -1;
  gm_->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                   reinterpret_cast<GLint*>(&num_textures));
  std::ostringstream shader_string;
  for (GLuint i = 0; i < num_textures; ++i) {
    shader_string << "uniform sampler2D uTexture" << i << ";\n";
  }
  shader_string << kBaseShaderString;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader = ShaderProgram::BuildFromStrings(
      "BigShader", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());

  // Add a node with the full compliment of textures.
  NodePtr node(new Node);
  node->SetShaderProgram(shader);
  node->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));
  node->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uModelviewMatrix", math::Matrix4f::Identity()));

  SamplerPtr sampler(new Sampler());
  s_data.sampler->SetLabel("Big Sampler");
  for (GLuint j = 0; j < num_textures; ++j) {
    // Make a unique name to create a new Uniform for each texture.
    std::ostringstream str;
    str << "uTexture" << j;

    TexturePtr texture(new Texture);
    texture->SetImage(0U, s_data.image);
    texture->SetSampler(sampler);
    node->AddUniform(reg->Create<Uniform>(str.str(), texture));
  }
  node->AddShape(s_data.shape);

  ShaderInputRegistryPtr reg1(new ShaderInputRegistry);
  reg1->IncludeGlobalRegistry();
  reg1->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderInputRegistryPtr reg2(new ShaderInputRegistry);
  reg2->IncludeGlobalRegistry();
  reg2->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader1 = ShaderProgram::BuildFromStrings(
      "Shader1", reg1, kPlaneVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  ShaderProgramPtr shader2 = ShaderProgram::BuildFromStrings(
      "Shader2", reg2, kPlaneVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  s_data.rect->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));

  // Add two nodes that have both textures.
  TexturePtr texture1(new Texture);
  texture1->SetImage(0U, s_data.image);
  texture1->SetSampler(s_data.sampler);
  TexturePtr texture2(new Texture);
  texture2->SetImage(0U, s_data.image);
  texture2->SetSampler(s_data.sampler);
  NodePtr node1(new Node);
  node1->SetShaderProgram(shader1);
  node1->AddUniform(reg1->Create<Uniform>("uTexture1", texture1));
  node1->AddUniform(reg1->Create<Uniform>("uTexture2", texture2));
  node1->AddShape(s_data.shape);
  s_data.rect->AddChild(node1);

  NodePtr node2(new Node);
  node2->SetShaderProgram(shader2);
  node2->AddUniform(reg2->Create<Uniform>("uTexture1", texture1));
  node2->AddUniform(reg2->Create<Uniform>("uTexture2", texture2));
  node2->AddShape(s_data.shape);
  s_data.rect->AddChild(node2);

  // Each texture should be bound once, and the sampler bound to each unit.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture"));
  // 4 image units will be used since there are 4 distinct uniforms.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE0)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE2)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE3)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE4)"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("BindSampler"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x0"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x2"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x3"));
  // The image units should be sent.
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("Uniform1i"));

  Reset();
  renderer->DrawScene(root);
  // Nothing new should be sent.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindSampler"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Force the uniform -> image unit associations to change by rendering a big
  // shader.
  Reset();
  renderer->DrawScene(node);

  // Check that the samplers were bound correctly.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("ActiveTexture"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("BindSampler"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x4"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x5"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x6"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindSampler(0x7"));
  // The new image units should be sent again.
  EXPECT_EQ(4U, trace_verifier_->GetCountOf("Uniform1i"));
}

TEST_F(RendererTest, MissingSamplerCausesWarning) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  s_data.texture->SetSampler(SamplerPtr());
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "has no Sampler!"));

  s_data.texture->SetSampler(s_data.sampler);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, ImmutableTextures) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);

  // Create immutable textures and test the proper TexStorage calls are made.
  Build1dArrayImage();
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<Texture>(
      renderer, trace_verifier_, 3, "TexStorage2D(GL_TEXTURE_1D_ARRAY, "));
  Reset();
  // 1D cubemaps are illegal.

  Build2dImage();
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<Texture>(renderer, trace_verifier_, 2,
                                              "TexStorage2D(GL_TEXTURE_2D, "));
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<CubeMapTexture>(
      renderer, trace_verifier_, 4, "TexStorage2D(GL_TEXTURE_CUBE_MAP, "));

  Build2dArrayImage();
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<Texture>(
      renderer, trace_verifier_, 4, "TexStorage3D(GL_TEXTURE_2D_ARRAY, "));
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<CubeMapTexture>(
      renderer, trace_verifier_, 3,
      "TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, "));

  Build3dImage();
  Reset();
  EXPECT_TRUE(VerifyImmutableTexture<Texture>(
      renderer, trace_verifier_, 3, "TexStorage3D(GL_TEXTURE_3D, "));
  // 3D cubemaps are illegal.

  s_data.image.Reset(NULL);
}

TEST_F(RendererTest, ImmutableMultisampleTextures) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);

  Build2dImage();
  Reset();
  EXPECT_TRUE(VerifyImmutableMultisampledTexture<Texture>(
      renderer, trace_verifier_, 8,
      "TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, "));

  Build2dArrayImage();
  Reset();
  EXPECT_TRUE(VerifyImmutableMultisampledTexture<Texture>(
      renderer, trace_verifier_, 8,
      "TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, "));

  s_data.image.Reset(NULL);
}

TEST_F(RendererTest, TextureEvictionCausesRebind) {
  // Test that when a texture is evicted from an image unit by a texture from a
  // different uniform that the original texture will be rebound when drawn
  // again.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  s_data.rect->ClearChildren();
  s_data.rect->ClearUniforms();
  s_data.rect->ClearShapes();

  // Add many nodes with different textures bound to different uniforms; they
  // will eventually wrap image units.
  GLuint num_textures = -1;
  gm_->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                   reinterpret_cast<GLint*>(&num_textures));

  // Construct a shader with many textures.
  static const char* kBaseShaderString = (
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTexture, vTexCoords);\n"
      "}\n");
  std::ostringstream shader_string;
  for (GLuint i = 0; i < num_textures; ++i) {
    shader_string << "uniform sampler2D uTexture" << i << ";\n";
  }
  shader_string << kBaseShaderString;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader1 = ShaderProgram::BuildFromStrings(
      "Shader1", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());
  ShaderProgramPtr shader2 = ShaderProgram::BuildFromStrings(
      "Shader2", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());

  s_data.rect->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));

  // Add a node with the full compliment of textures.
  NodePtr node(new Node);
  node->SetShaderProgram(shader1);
  for (GLuint j = 0; j < num_textures; ++j) {
    // Make a unique name to create a new Uniform for each texture.
    std::ostringstream str;
    str << "uTexture" << j;

    TexturePtr texture(new Texture);
    texture->SetImage(0U, s_data.image);
    texture->SetSampler(s_data.sampler);
    node->AddUniform(reg->Create<Uniform>(str.str(), texture));
  }
  node->AddShape(s_data.shape);
  s_data.rect->AddChild(node);

  // Each texture should be bound once.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 1; i < num_textures; ++i) {
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("BindSampler"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again should do nothing, since everything is already bound.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again with a framebuffer should rebind all textures, since they are
  // all evicted when cleared.
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
  renderer->BindFramebuffer(fbo);
  EXPECT_EQ(fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  renderer->DrawScene(root);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // Since all the sampler bindings were reset across the fbo bind, they will
  // be bound again.
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("BindSampler"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again should rebind, since they are evicted when the framebuffer
  // was changed.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("BindSampler"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Add another node with the full compliment of textures.
  node.Reset(new Node);
  node->SetShaderProgram(shader2);
  for (GLuint j = 0; j < num_textures; ++j) {
    // Make a unique name to create a new Uniform for each texture.
    std::ostringstream str;
    str << "uTexture" << j;

    TexturePtr texture(new Texture);
    texture->SetImage(0U, s_data.image);
    texture->SetSampler(s_data.sampler);
    node->AddUniform(reg->Create<Uniform>(str.str(), texture));
  }
  node->AddShape(s_data.shape);
  s_data.rect->AddChild(node);

  // Get the new resources created.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The new texture uniforms should share the same units, but they have to be
  // sent once for the new shader. The samplers, however, are already in the
  // right place.
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindSampler"));

  // Drawing again should rebind everything, since all textures were evicted by
  // the second node. The units should be consistent, however.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindSampler"));

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, ArrayTextureEvictionCausesRebind) {
  // Similar to the above test but using array textures.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  s_data.rect->ClearChildren();
  s_data.rect->ClearUniforms();
  s_data.rect->ClearShapes();

  // Add many nodes with different textures bound to different uniforms; they
  // will eventually wrap image units.
  GLuint num_textures = -1;
  gm_->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                   reinterpret_cast<GLint*>(&num_textures));

  // Construct a shader with many textures.
  static const char* kBaseShaderString = (
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTextures[0], vTexCoords);\n"
      "}\n");
  std::ostringstream shader_string;
  shader_string << "uniform sampler2D uTextures[" << num_textures << "];\n";
  shader_string << kBaseShaderString;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader1 = ShaderProgram::BuildFromStrings(
      "Shader1", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());
  ShaderProgramPtr shader2 = ShaderProgram::BuildFromStrings(
      "Shader2", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());

  s_data.rect->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));

  // Add a node with the full compliment of textures.
  NodePtr node(new Node);
  std::vector<TexturePtr> textures(num_textures);
  node->SetShaderProgram(shader1);
  for (GLuint i = 0; i < num_textures; ++i) {
    textures[i].Reset(new Texture);
    textures[i]->SetImage(0U, s_data.image);
    textures[i]->SetSampler(s_data.sampler);
  }
  node->AddUniform(CreateArrayUniform(reg, "uTextures", textures));
  node->AddShape(s_data.shape);
  s_data.rect->AddChild(node);

  // Each texture should be bound once.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    SCOPED_TRACE(i);
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should do nothing, since everything is already bound.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again with a framebuffer should rebind all textures, since they are
  // all evicted when cleared.
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
  renderer->BindFramebuffer(fbo);
  EXPECT_EQ(fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  renderer->DrawScene(root);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should rebind, since they are evicted when the framebuffer
  // was changed.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Add another node with the full compliment of textures.
  textures.clear();
  textures.resize(num_textures);
  node.Reset(new Node);
  node->SetShaderProgram(shader2);
  for (GLuint i = 0; i < num_textures; ++i) {
    textures[i].Reset(new Texture);
    textures[i]->SetImage(0U, s_data.image);
    textures[i]->SetSampler(s_data.sampler);
  }
  node->AddUniform(CreateArrayUniform(reg, "uTextures", textures));
  node->AddShape(s_data.shape);
  s_data.rect->AddChild(node);

  // Get the new resources created.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  // The texture uniforms for the new shader get their bindings.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should rebind everything, since all textures were evicted by
  // the second node.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, ArrayCubemapEvictionCausesRebind) {
  // The same as the above test but using array cubemaps.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  s_data.rect->ClearChildren();
  s_data.rect->ClearUniforms();
  s_data.rect->ClearShapes();

  // Add many nodes with different textures bound to different uniforms; they
  // will eventually wrap image units.
  GLuint num_textures = -1;
  gm_->GetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,
                   reinterpret_cast<GLint*>(&num_textures));

  // Construct a shader with many textures.
  static const char* kBaseShaderString = (
      "varying vec2 vTexCoords;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = texture2D(uTextures[0], vTexCoords);\n"
      "}\n");
  std::ostringstream shader_string;
  shader_string << "uniform samplerCube uTextures[" << num_textures << "];\n";
  shader_string << kBaseShaderString;
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::AttributeSpec(
      "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));
  ShaderProgramPtr shader1 = ShaderProgram::BuildFromStrings(
      "Shader1", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());
  ShaderProgramPtr shader2 = ShaderProgram::BuildFromStrings(
      "Shader2", reg, kPlaneVertexShaderString, shader_string.str(),
      base::AllocatorPtr());

  s_data.rect->AddUniform(s_data.shader->GetRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));

  // Add a node with the full compliment of textures.
  NodePtr node(new Node);
  std::vector<CubeMapTexturePtr> textures(num_textures);
  node->SetShaderProgram(shader1);
  for (GLuint i = 0; i < num_textures; ++i) {
    textures[i].Reset(new CubeMapTexture);
    for (int j = 0; j < 6; ++j)
      textures[i]->SetImage(static_cast<CubeMapTexture::CubeFace>(j), 0U,
                            s_data.image);
    textures[i]->SetSampler(s_data.sampler);
  }
  node->AddUniform(CreateArrayUniform(reg, "uTextures", textures));
  node->AddShape(s_data.shape);
  s_data.rect->AddChild(node);

  // Each texture should be bound once.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 6U, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    SCOPED_TRACE(i);
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should do nothing, since everything is already bound.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Drawing again with a framebuffer should rebind all textures, since they are
  // all evicted when cleared.
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  Reset();
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
  renderer->BindFramebuffer(fbo);
  EXPECT_EQ(fbo.Get(), renderer->GetCurrentFramebuffer().Get());
  renderer->DrawScene(root);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should rebind, since they are evicted when the framebuffer
  // was changed.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms are sent the first draw.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1i"));

  // Add another node with the full compliment of textures.
  textures.clear();
  textures.resize(num_textures);
  node.Reset(new Node);
  node->SetShaderProgram(shader2);
  for (GLuint i = 0; i < num_textures; ++i) {
    textures[i].Reset(new CubeMapTexture);
    for (int j = 0; j < 6; ++j)
      textures[i]->SetImage(static_cast<CubeMapTexture::CubeFace>(j), 0U,
                            s_data.image);
    textures[i]->SetSampler(s_data.sampler);
  }
  node->AddUniform(CreateArrayUniform(reg, "uTextures", textures));
  node->AddShape(s_data.shape);
  s_data.rect->AddChild(node);

  // Get the new resources created.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 6U, trace_verifier_->GetCountOf("TexImage2D"));
  for (GLuint i = 0; i < num_textures; ++i) {
    std::ostringstream str;
    str << "ActiveTexture(GL_TEXTURE" << i << ")";
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(str.str()));
  }
  EXPECT_EQ(num_textures,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  // The texture uniforms for the new shader get their bindings.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Drawing again should rebind everything, since all textures were evicted by
  // the second node.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("ActiveTexture(GL_TEXTURE"));
  EXPECT_EQ(num_textures * 2U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Uniform1iv"));

  // Reset data.
  s_data.rect = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, TextureMipmaps) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Check that a texture with an image is sent as mipmap level 0.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(28672U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage2D"))
          .HasArg(2, "0"));

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid.
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    s_data.texture->SetImage(i, mipmaps[i]);

  // Check consistent dimensions.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNumMipmaps, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Check that the texture memory increased properly.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());

  // Check the right calls were made.
  for (uint32 i = 0; i < kNumMipmaps; ++i) {
    SCOPED_TRACE(::testing::Message() << "Testing mipmap level " << i);
    EXPECT_TRUE(trace_verifier_->VerifyCallAt(
        trace_verifier_->GetNthIndexOf(i, "TexImage2D"))
            .HasArg(2, helper.ToString("GLint", static_cast<GLint>(i)))
            .HasArg(4, helper.ToString(
                "GLsizei", static_cast<GLint>(mipmaps[i]->GetWidth())))
            .HasArg(5, helper.ToString(
                "GLsizei", static_cast<GLint>(mipmaps[i]->GetHeight()))));
  }

  // Remove a texture and verify that GenerateMipmap is called.
  s_data.texture->SetImage(1U, ImagePtr(NULL));
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Since GenerateMipmap was called the override mipmaps were sent, but the
  // 0th level mipmap doesn't have to be (GenerateMipmap won't override it).
  EXPECT_EQ(kNumMipmaps - 2U, trace_verifier_->GetCountOf("TexImage2D"));
  // Memory usage should not change.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());

  // Set an invalid image dimension.
  s_data.texture->SetImage(
      1U, CreateNullImage(mipmaps[1]->GetWidth() - 1, mipmaps[1]->GetHeight(),
                          Image::kRgba8888));

  Reset();
  renderer->DrawScene(root);
  // Nothing will be called since the texture has the right number of levels,
  // just incorrect dimensions.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  s_msg_stream_ << "Mipmap width: " << (mipmaps[1]->GetWidth() - 1) <<
      " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", s_msg_stream_.str()));
  // Memory usage should not change.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());

  s_data.texture->SetImage(
      1U, CreateNullImage(mipmaps[1]->GetWidth(), mipmaps[1]->GetHeight() - 1,
                          Image::kRgba8888));

  Reset();
  renderer->DrawScene(root);
  // Nothing will be called since the texture has the right number of levels,
  // just incorrect dimensions.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  s_msg_stream_ << "Mipmap height: " << (mipmaps[1]->GetWidth() - 1) <<
      " is not a power of 2.";
  EXPECT_TRUE(log_checker.HasMessage("ERROR", s_msg_stream_.str()));
  // Memory usage should not change.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());

  // Set an invalid image dimension.
  s_data.texture
      ->SetImage(1U, CreateNullImage(mipmaps[1]->GetWidth(),
                                     mipmaps[1]->GetHeight(), Image::kRgb888));
  Reset();
  renderer->DrawScene(root);
  // Nothing will be called since the texture has the right number of levels,
  // just incorrect dimensions.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_TRUE(
      log_checker.HasMessage("ERROR", "level 1 has different format"));
  // Memory usage should not change.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());
}

TEST_F(RendererTest, TextureMultisamplingDisablesMipmapping) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  Reset();

  EXPECT_TRUE(gm_->IsFunctionGroupAvailable(
      GraphicsManager::kTextureMultisample));

  // Set multisampling.
  s_data.texture->SetMultisampling(4, true);
  // Clear cubemap as we don't need it and it affects the number of times
  // TexImage2D is invoked.
  for (int i = 0; i < 6; ++i) {
    s_data.cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                             ImagePtr());
  }

  renderer->DrawScene(root);
  EXPECT_EQ(4096U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(0U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());

  // Verify call to TexImage2DMultisample.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage2DMultisample"))
          .HasArg(1, "GL_TEXTURE_2D_MULTISAMPLE"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage2DMultisample"))
          .HasArg(2, "4"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexImage2DMultisample"))
          .HasArg(6, "GL_TRUE"));

  // Verify calls to TexImage2D and GenerateMipmap. "TexImage2D" is a prefix
  // of "TexImage2DMultisample" so it should appear once.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid.
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    s_data.texture->SetImage(i, mipmaps[i]);

  // Mipmaps will be counted against memory but should still not be used.
  // "TexImage2D" is a prefix of "TexImage2DMultisample" so it should appear
  // once.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Check that the texture memory is as expected.
  EXPECT_EQ(5461U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(0U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());

  // Unset multisampling.
  s_data.texture->SetMultisampling(0, false);

  // Mipmaps should now be used.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Check that the texture memory stayed the same.
  EXPECT_EQ(5461U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(0U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());

  // Clear warning from clearing the cubemap textures above.
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
      "***ION: Cubemap texture face Negative X has no level 0 mipmap."));
}

TEST_F(RendererTest, TextureMultisamplingDisablesSubImages) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  Reset();

  EXPECT_TRUE(gm_->IsFunctionGroupAvailable(
      GraphicsManager::kTextureMultisample));

  // Set multisampling.
  s_data.texture->SetMultisampling(4, true);
  // Clear cubemap as we don't need it and it affects the number of times
  // TexImage2D is invoked.
  for (int i = 0; i < 6; ++i) {
    s_data.cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                             ImagePtr());
  }

  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2DMultisample"));

  // Now set a subimage on the texture.
  s_data.texture->SetSubImage(0U, math::Point2ui(4, 8),
                              CreateNullImage(10, 12, Image::kRgba8888));

  // Check that the subimage is NOT applied.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexSubImage2D"));

  // Disable multisampling.
  s_data.texture->SetMultisampling(0, false);

  // Check that the subimage is applied.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2DMultisample"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexSubImage2D"));

  // Clear warning from clearing the cubemap textures above.
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
      "***ION: Cubemap texture face Negative X has no level 0 mipmap."));
}

TEST_F(RendererTest, TextureSubImages) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;
  TracingHelper helper;

  // Check that a texture with an image is sent as mipmap level 0.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("TexImage2D"));

  // Now set a subimage on the texture.
  s_data.texture->SetSubImage(0U, math::Point2ui(4, 8),
                              CreateNullImage(10, 12, Image::kRgba8888));
  // Check that the subimage is applied.
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexSubImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "TexSubImage2D"))
          .HasArg(2, "0")
          .HasArg(3, "4")
          .HasArg(4, "8")
          .HasArg(5, "10")
          .HasArg(6, "12")
          .HasArg(7, "GL_RGB")
          .HasArg(8, "GL_UNSIGNED_BYTE"));

  // Several images of different sizes.
  static const uint32 kMaxImageSize = 32;
  static const uint32 kNumMipmaps = 6U;  // log2(kMaxImageSize) + 1U;
  ImagePtr mipmaps[kNumMipmaps];
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    mipmaps[i] = CreateNullImage(kMaxImageSize >> i, kMaxImageSize >> i,
                                 Image::kRgba8888);

  // Set a full image pyramid.
  for (uint32 i = 0; i < kNumMipmaps; ++i)
    s_data.texture->SetImage(i, mipmaps[i]);

  // Check consistent dimensions.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_EQ(kNumMipmaps, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());

  // Set a submipmap at level 3. Setting a compressed image requires non-NULL
  // image data.
  ImagePtr compressed_image(new Image);
  compressed_image->Set(
      Image::kDxt5, 4, 8,
      base::DataContainerPtr(base::DataContainer::Create(
          reinterpret_cast<void*>(1), kNullFunction, false,
          compressed_image->GetAllocator())));
  s_data.texture->SetSubImage(3U, math::Point2ui(2, 6), compressed_image);

  // Check the right call was made.
  Reset();
  renderer->DrawScene(root);
  // Technically there is an errors since the cubemap is not compressed, but
  // this is just to test that the call is made.
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexSubImage2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CompressedTexSubImage2D"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "CompressedTexSubImage2D"))
          .HasArg(2, "3")
          .HasArg(3, "2")
          .HasArg(4, "6")
          .HasArg(5, "4")
          .HasArg(6, "8")
          .HasArg(7, "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT"));
  // Memory usage is not affected by sub images.
  EXPECT_EQ(30037U, renderer->GetGpuMemoryUsage(Renderer::kTexture));
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  EXPECT_EQ(5461U, s_data.texture->GetGpuMemoryUsed());
}

TEST_F(RendererTest, TextureMisc) {
  // Test various texture corner cases.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);
  base::LogChecker log_checker;

  // Check that a texture with no image does not get sent.
  s_data.texture->SetImage(0U, ImagePtr(NULL));
  s_data.texture->SetLabel("texture");
  Reset();
  renderer->DrawScene(root);
  // The cubemap is still sent.
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexImage2D"));
  s_data.texture->SetImage(0U, s_data.image);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "Texture \"texture\" has no level 0"));

  // Check that a GenerateMipmap is called if requested.
  s_data.sampler->SetAutogenerateMipmapsEnabled(true);
  Reset();
  renderer->DrawScene(root);
  // Both the cubemap and texture will generate mipmaps.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenerateMipmap"));
  // Check that when disabled GenerateMipmap is not called.
  s_data.sampler->SetAutogenerateMipmapsEnabled(false);
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenerateMipmap"));

  // Check error cases for non-power-of-2 textures.
  s_data.image->Set(s_options.image_format, 32, 33, s_data.image_container);
  s_data.sampler->SetMinFilter(Sampler::kLinearMipmapNearest);
  s_data.sampler->SetWrapS(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  s_data.image->Set(s_options.image_format, 33, 32, s_data.image_container);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  s_data.image->Set(s_options.image_format, 32, 33, s_data.image_container);
  s_data.sampler->SetWrapS(Sampler::kRepeat);
  s_data.sampler->SetWrapT(Sampler::kClampToEdge);
  Reset();
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Non-power-of-two textures"));

  s_data.sampler->SetWrapT(Sampler::kRepeat);
  Reset();
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Reset data.
  BuildRectangle();
}

TEST_F(RendererTest, TextureCompareFunction) {
  VerifyRenderData<Sampler::CompareFunction> verify_data;
  verify_data.option = &s_options.compare_func;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_COMPARE_FUNC"));
  verify_data.arg_tests.push_back(
        VaryingArg<Sampler::CompareFunction>(
            4, Sampler::kAlways, "GL_ALWAYS"));
  verify_data.arg_tests.push_back(
        VaryingArg<Sampler::CompareFunction>(
            0, Sampler::kEqual, "GL_EQUAL"));
  verify_data.arg_tests.push_back(
        VaryingArg<Sampler::CompareFunction>(
            0, Sampler::kGreater, "GL_GREATER"));
  verify_data.arg_tests.push_back(
        VaryingArg<Sampler::CompareFunction>(
            0, Sampler::kGreaterOrEqual, "GL_GEQUAL"));
  verify_data.arg_tests.push_back(
        VaryingArg<Sampler::CompareFunction>(
            0, Sampler::kLess, "GL_LESS"));
  verify_data.arg_tests.push_back(
        VaryingArg<Sampler::CompareFunction>(
            0, Sampler::kLessOrEqual, "GL_LEQUAL"));
  verify_data.arg_tests.push_back(
        VaryingArg<Sampler::CompareFunction>(
            0, Sampler::kNever, "GL_NEVER"));
  verify_data.arg_tests.push_back(
        VaryingArg<Sampler::CompareFunction>(
            0, Sampler::kNotEqual, "GL_NOTEQUAL"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureCompareMode) {
  VerifyRenderData<Sampler::CompareMode> verify_data;
  verify_data.option = &s_options.compare_mode;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_COMPARE_MODE"));
  verify_data.arg_tests.push_back(VaryingArg<Sampler::CompareMode>(
      5, Sampler::kCompareToTexture, "GL_COMPARE_REF_TO_TEXTURE"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::CompareMode>(0, Sampler::kNone, "GL_NONE"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureMaxAnisotropy) {
  VerifyRenderData<float> verify_data;
  verify_data.option = &s_options.max_anisotropy;
  verify_data.static_args.push_back(
      StaticArg(2, "GL_TEXTURE_MAX_ANISOTROPY_EXT"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, 10.f, "10"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, 4.f, "4"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, 1.f, "1"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));

  // Check that max anisotropy is bounded.
  Reset();
  s_options.max_anisotropy = 32.f;
  NodePtr root = BuildGraph(800, 800);
  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(root);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "SamplerParameterf"))
          .HasArg(2, "GL_TEXTURE_MAX_ANISOTROPY_EXT")
          .HasArg(3, "16"));

  // Disable anisotropy and make no anisotropy call is made.
  gm_->SetExtensionsString("");
  Reset();
  root = BuildGraph(800, 800);
  renderer->DrawScene(root);
  EXPECT_EQ(std::string::npos, trace_verifier_->GetTraceString().find(
                                   "GL_TEXTURE_MAX_ANISOTROPY_EXT"));
  s_options.max_anisotropy = 1.f;
}

TEST_F(RendererTest, TextureMagFilter) {
  VerifyRenderData<Sampler::FilterMode> verify_data;
  verify_data.option = &s_options.mag_filter;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_MAG_FILTER"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::FilterMode>(1, Sampler::kLinear, "GL_LINEAR"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::FilterMode>(0, Sampler::kNearest, "GL_NEAREST"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureMaxLod) {
  VerifyRenderData<float> verify_data;
  verify_data.option = &s_options.max_lod;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_MAX_LOD"));
  verify_data.arg_tests.push_back(VaryingArg<float>(1, 100.f, "100"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, -2.1f, "-2.1"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, 23.45f, "23.45"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureMinFilter) {
  VerifyRenderData<Sampler::FilterMode> verify_data;
  verify_data.option = &s_options.min_filter;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_MIN_FILTER"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::FilterMode>(
          0, Sampler::kLinear, "GL_LINEAR"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::FilterMode>(
          0, Sampler::kNearest, "GL_NEAREST"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::FilterMode>(
          0, Sampler::kNearestMipmapNearest, "GL_NEAREST_MIPMAP_NEAREST"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::FilterMode>(
          0, Sampler::kNearestMipmapLinear, "GL_NEAREST_MIPMAP_LINEAR"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::FilterMode>(
          0, Sampler::kLinearMipmapNearest, "GL_LINEAR_MIPMAP_NEAREST"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::FilterMode>(
          0, Sampler::kLinearMipmapLinear, "GL_LINEAR_MIPMAP_LINEAR"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureMinLod) {
  VerifyRenderData<float> verify_data;
  verify_data.option = &s_options.min_lod;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_MIN_LOD"));
  verify_data.arg_tests.push_back(VaryingArg<float>(2, 10.f, "10"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, -3.1f, "-3.1"));
  verify_data.arg_tests.push_back(VaryingArg<float>(0, 12.34f, "12.34"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureWrapR) {
  VerifyRenderData<Sampler::WrapMode> verify_data;
  verify_data.option = &s_options.wrap_r;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_WRAP_R"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          6, Sampler::kClampToEdge, "GL_CLAMP_TO_EDGE"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          0, Sampler::kRepeat, "GL_REPEAT"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          0, Sampler::kMirroredRepeat, "GL_MIRRORED_REPEAT"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureWrapS) {
  VerifyRenderData<Sampler::WrapMode> verify_data;
  verify_data.option = &s_options.wrap_s;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_WRAP_S"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          2, Sampler::kClampToEdge, "GL_CLAMP_TO_EDGE"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          0, Sampler::kRepeat, "GL_REPEAT"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          0, Sampler::kMirroredRepeat, "GL_MIRRORED_REPEAT"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureWrapT) {
  VerifyRenderData<Sampler::WrapMode> verify_data;
  verify_data.option = &s_options.wrap_t;
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_WRAP_T"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          3, Sampler::kClampToEdge, "GL_CLAMP_TO_EDGE"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          0, Sampler::kRepeat, "GL_REPEAT"));
  verify_data.arg_tests.push_back(
      VaryingArg<Sampler::WrapMode>(
          0, Sampler::kMirroredRepeat, "GL_MIRRORED_REPEAT"));
  EXPECT_TRUE(VerifySamplerAndTextureCalls(this, &verify_data));
}

TEST_F(RendererTest, TextureBaseLevel) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<int> verify_data;
  verify_data.update_func = BuildRectangle;
  verify_data.call_name = "TexParameteri";
  verify_data.option = &s_options.base_level;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_BASE_LEVEL"));
  verify_data.arg_tests.push_back(VaryingArg<int>(0, 10, "10"));
  verify_data.arg_tests.push_back(VaryingArg<int>(0, 3, "3"));
  verify_data.arg_tests.push_back(VaryingArg<int>(0, 123, "123"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureMaxLevel) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<int> verify_data;
  verify_data.update_func = BuildRectangle;
  verify_data.call_name = "TexParameteri";
  verify_data.option = &s_options.max_level;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
  verify_data.static_args.push_back(StaticArg(2, "GL_TEXTURE_MAX_LEVEL"));
  verify_data.arg_tests.push_back(VaryingArg<int>(1, 100, "100"));
  verify_data.arg_tests.push_back(VaryingArg<int>(0, 33, "33"));
  verify_data.arg_tests.push_back(VaryingArg<int>(0, 1234, "1234"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureSwizzleRed) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  NodePtr root;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<Texture::Swizzle> verify_data;
  verify_data.update_func = BuildRectangle;
  verify_data.call_name = "TexParameteri";
  verify_data.option = &s_options.swizzle_r;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString("GLtextureenum", GL_TEXTURE_SWIZZLE_R)));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(2, Texture::kGreen, "GL_GREEN"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kBlue, "GL_BLUE"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kAlpha, "GL_ALPHA"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kRed, "GL_RED"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureSwizzleGreen) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  NodePtr root;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<Texture::Swizzle> verify_data;
  verify_data.update_func = BuildRectangle;
  verify_data.call_name = "TexParameteri";
  verify_data.option = &s_options.swizzle_g;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString("GLtextureenum", GL_TEXTURE_SWIZZLE_G)));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(3, Texture::kBlue, "GL_BLUE"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kAlpha, "GL_ALPHA"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kRed, "GL_RED"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kGreen, "GL_GREEN"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureSwizzleBlue) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  NodePtr root;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<Texture::Swizzle> verify_data;
  verify_data.update_func = BuildRectangle;
  verify_data.call_name = "TexParameteri";
  verify_data.option = &s_options.swizzle_b;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString("GLtextureenum", GL_TEXTURE_SWIZZLE_B)));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(4, Texture::kAlpha, "GL_ALPHA"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kRed, "GL_RED"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kGreen, "GL_GREEN"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kBlue, "GL_BLUE"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, TextureSwizzleAlpha) {
  RendererPtr renderer(new Renderer(gm_));
  TracingHelper helper;
  NodePtr root;

  root = BuildGraph(kWidth, kHeight);
  VerifyRenderData<Texture::Swizzle> verify_data;
  verify_data.update_func = BuildRectangle;
  verify_data.call_name = "TexParameteri";
  verify_data.option = &s_options.swizzle_a;
  verify_data.varying_arg_index = 3U;
  verify_data.static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
  verify_data.static_args.push_back(
      StaticArg(2, helper.ToString("GLtextureenum", GL_TEXTURE_SWIZZLE_A)));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(5, Texture::kRed, "GL_RED"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kGreen, "GL_GREEN"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kBlue, "GL_BLUE"));
  verify_data.arg_tests.push_back(
      VaryingArg<Texture::Swizzle>(0, Texture::kAlpha, "GL_ALPHA"));
  EXPECT_TRUE(VerifyRenderCalls(verify_data, trace_verifier_, renderer, root));

  Reset();
  renderer = NULL;
  EXPECT_TRUE(VerifyReleases(1));
}

TEST_F(RendererTest, MultipleRenderers) {
  // Multiple Renderers create multiple instances of the same resources.
  // There isn't (yet) a way to get at the internal state of the
  // ResourceManager, but this at least will create ResourceGroups.
  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, false);
  {
    RendererPtr renderer1(new Renderer(gm_));
    RendererPtr renderer2(new Renderer(gm_));

    // Draw the simplest possible scene.
    NodePtr root = BuildGraph(800, 800);
    // Drawing will create resources.
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    // Improve coverage by changing a group bit.
    s_data.sampler->SetWrapS(Sampler::kMirroredRepeat);
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);

    // Each renderer has its own resources and memory counts.
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer1, 12U + kVboSize, 0U, 28672U));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer2, 12U + kVboSize, 0U, 28672U));
    // Memory usage per holder should be doubled; one resource per renderer.
    EXPECT_EQ(24U, s_data.index_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(8U * sizeof(Vertex), s_data.vertex_buffer->GetGpuMemoryUsed());
    EXPECT_EQ(8192U, s_data.texture->GetGpuMemoryUsed());
    EXPECT_EQ(49152U, s_data.cubemap->GetGpuMemoryUsed());

    Reset();
    // Force calls to OnDestroyed().
    s_data.attribute_array = NULL;
    s_data.vertex_buffer = NULL;
    s_data.index_buffer = NULL;
    s_data.shader = NULL;
    s_data.shape = NULL;
    s_data.texture = NULL;
    s_data.cubemap = NULL;
    s_data.rect = NULL;
    root->ClearChildren();
    root->ClearUniforms();
    root->SetShaderProgram(ShaderProgramPtr(NULL));
    // Force calls to ReleaseAll().
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    // It can take two calls to free up all resources because some may be added
    // to the release queue during traversal.
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer1, 0U, 0U, 0U));
    EXPECT_TRUE(VerifyGpuMemoryUsage(renderer2, 0U, 0U, 0U));
    // Everything will be destroyed since the resources go away.
    std::vector<std::string> call_strings;
    call_strings.push_back("Clear");
    call_strings.push_back("Clear");
    call_strings.push_back("Clear");
    call_strings.push_back("Clear");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteProgram");
    call_strings.push_back("DeleteProgram");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteVertexArrays");
    call_strings.push_back("DeleteVertexArrays");
    EXPECT_TRUE(trace_verifier_->VerifySortedCalls(call_strings));
    Reset();
    root = NULL;
    renderer1 = NULL;
    renderer2 = NULL;
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
  }
  // Reset data.
  BuildRectangle();

  {
    RendererPtr renderer1(new Renderer(gm_));
    RendererPtr renderer2(new Renderer(gm_));
    RendererPtr renderer3(new Renderer(gm_));

    // Draw the simplest possible scene.
    NodePtr root = BuildGraph(800, 800);
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    renderer3->DrawScene(root);
    Reset();
    // Force resource deletion from a renderer.
    renderer1 = NULL;
    // Force calls to OnDestroyed().
    root = NULL;
    s_data.shape = NULL;
    s_data.rect = NULL;
    renderer2 = NULL;
    renderer3 = NULL;
    EXPECT_TRUE(VerifyReleases(3));
  }

  {
    RendererPtr renderer1(new Renderer(gm_));
    RendererPtr renderer2(new Renderer(gm_));
    RendererPtr renderer3(new Renderer(gm_));

    // Draw the simplest possible scene.
    NodePtr root = BuildGraph(800, 800);
    renderer1->DrawScene(root);
    renderer2->DrawScene(root);
    renderer3->DrawScene(root);
    Reset();

    // Clear resources to improve coverage.
    renderer3->ClearAllResources();
    EXPECT_TRUE(VerifyReleases(1));
    Reset();
    renderer1->ClearResources(s_data.attribute_array.Get());
    renderer2->ClearTypedResources(Renderer::kTexture);
    renderer1 = NULL;
    // Force calls to OnDestroyed().
    root = NULL;
    s_data.shape = NULL;
    s_data.rect = NULL;
    renderer2 = NULL;
    EXPECT_TRUE(VerifyReleases(2));
  }

  // Reset data.
  BuildRectangle();

  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, true);
}

TEST_F(RendererTest, Clearing) {
  NodePtr node(new Node);
  StateTablePtr state_table;
  RendererPtr renderer(new Renderer(gm_));

  state_table = new StateTable();
  state_table->SetClearDepthValue(0.5f);
  node->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepthf(0.5)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear(GL_DEPTH_BUFFER_BIT)"));

  state_table = new StateTable();
  state_table->SetClearColor(math::Vector4f(0.3f, 0.3f, 0.5f, 1.0f));
  state_table->SetClearDepthValue(0.25f);
  node->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor(0.3, 0.3, 0.5, 1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepthf(0.25)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
      "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)"));

  state_table = new StateTable();
  state_table->SetClearStencilValue(27);
  node->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearStencil(27)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear(GL_STENCIL_BUFFER_BIT)"));

  state_table = new StateTable();
  state_table->SetClearDepthValue(0.15f);
  state_table->SetClearColor(math::Vector4f(0.2f, 0.1f, 0.5f, 0.3f));
  state_table->SetClearStencilValue(123);
  node->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor(0.2, 0.1, 0.5, 0.3)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearDepthf(0.15)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearStencil(123)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | "
                "GL_STENCIL_BUFFER_BIT)"));

  // In a simple hierarchy with the clears at the root, no other nodes should
  // trigger a clear.
  NodePtr child1(new Node);
  NodePtr child2(new Node);
  node->AddChild(child1);
  node->AddChild(child2);
  state_table = new StateTable();
  state_table->Enable(StateTable::kBlend, true);
  child1->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kCullFace, true);
  child2->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  // The particular values are already set.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepthf"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearStencil"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | "
                "GL_STENCIL_BUFFER_BIT)"));

  // If an internal node clears, only it should be cleared.
  NodePtr parent(new Node);
  parent->AddChild(node);
  state_table = new StateTable();
  state_table->Enable(StateTable::kStencilTest, true);
  parent->SetStateTable(state_table);

  Reset();
  renderer->DrawScene(node);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearDepthf"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("ClearStencil"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | "
                "GL_STENCIL_BUFFER_BIT)"));

  // Test that clear colors are propagated correctly.
  NodePtr clear_node_blue(new Node);
  state_table = new StateTable();
  state_table->SetClearColor(math::Vector4f(0.0f, 0.0f, 1.0f, 1.0f));
  clear_node_blue->SetStateTable(state_table);

  NodePtr clear_node_black(new Node);
  state_table = new StateTable();
  state_table->SetClearColor(math::Vector4f(0.0f, 0.0f, 0.0f, 0.0f));
  clear_node_black->SetStateTable(state_table);

  BuildGraph(kWidth, kHeight);
  NodePtr shape_node(new Node);
  shape_node->SetShaderProgram(s_data.shader);
  AddPlaneShaderUniformsToNode(shape_node);
  shape_node->AddShape(s_data.shape);
  shape_node->AddChild(clear_node_black);

  Reset();
  renderer->DrawScene(clear_node_blue);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor(0, 0, 1, 1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear(GL_COLOR_BUFFER_BIT)"));

  Reset();
  renderer->DrawScene(shape_node);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ClearColor(0, 0, 0, 0)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Clear(GL_COLOR_BUFFER_BIT)"));
}

TEST_F(RendererTest, ClearingResources) {
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(800, 800);
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  Reset();
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U, 28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());

  // Clear an entire scene at once.
  Reset();
  renderer->ClearAllResources();
  // Check that all memory was released.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 0U, 0U, 0U));
  EXPECT_EQ(0U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(0U, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(0U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(0U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(0U, s_data.cubemap->GetGpuMemoryUsed());
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(root);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DeleteTexture"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenVertexArrays(1"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenFramebuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenRenderbuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenTextures(1, "));
  // The texture is bound twice, once for the framebuffer, and again when it is
  // used.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
  EXPECT_EQ(
      7U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("SamplerParameteri"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("SamplerParameterf"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(6U,
            trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(6U,
            trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(
      1U,
      trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(
      1U,
      trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  // Everything should be recreated.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U, 28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());

  // AttributeArray.
  Reset();
  renderer->ClearResources(s_data.attribute_array.Get());
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenVertexArrays(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("EnableVertexAttribArray"));
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U, 28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());

  // BufferObject.
  Reset();
  renderer->ClearResources(s_data.vertex_buffer.Get());
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U, 32768U, 28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(0U, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  renderer->DrawScene(root);
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U, 28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  // Check calls.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // CubeMapTexture.
  Reset();
  renderer->ClearResources(s_data.cubemap.Get());
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U, 4096U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(0U, s_data.cubemap->GetGpuMemoryUsed());
  renderer->DrawScene(root);
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U, 28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  // Check calls.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(6U,
            trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(
      6U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(6U,
            trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_CUBE_MAP"));

  // Framebuffer.
  Reset();
  renderer->ClearResources(fbo.Get());
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 0U, 28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(0U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(root);
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U, 28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  // Check calls.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenSamplers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenFramebuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenRenderbuffers"));

  // Sampler.
  Reset();
  renderer->ClearResources(s_data.sampler.Get());
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenSamplers"));
  // The sampler should be bound for both the texture and cubemap. The texture
  // is bound twice, once when it is created, and again after it is bound to a
  // uniform.
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindSampler"));
  EXPECT_EQ(7U, trace_verifier_->GetCountOf("SamplerParameteri"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("SamplerParameterf"));

  // Shader.
  Reset();
  renderer->ClearResources(s_data.shader->GetFragmentShader().Get());
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("ShaderSource"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("AttachShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));

  // ShaderProgram.
  Reset();
  renderer->ClearResources(s_data.shader.Get());
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("AttachShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));

  // Texture.
  Reset();
  renderer->ClearResources(s_data.texture.Get());
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U, 24576U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(0U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  renderer->DrawScene(root);
  // Check memory usage.
  EXPECT_TRUE(VerifyGpuMemoryUsage(renderer, 12U + kVboSize, 32768U,
                                   28672U));
  EXPECT_EQ(12U, s_data.index_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(kVboSize, s_data.vertex_buffer->GetGpuMemoryUsed());
  EXPECT_EQ(32768U, fbo->GetGpuMemoryUsed());
  EXPECT_EQ(4096U, s_data.texture->GetGpuMemoryUsed());
  EXPECT_EQ(24576U, s_data.cubemap->GetGpuMemoryUsed());
  // Check calls.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(6U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
  EXPECT_EQ(
      1U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                "GL_UNSIGNED_BYTE"));

  // Clear all Shaders.
  Reset();
  renderer->ClearTypedResources(Renderer::kShader);
  renderer->DrawScene(root);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteFramebuffers"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteSamplers"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("DeleteShader"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteTextures"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("ShaderSource"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("AttachShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));
}

TEST_F(RendererTest, DisabledNodes) {
  // Build a graph with multiple nodes, each with a StateTable that enables
  // a different capability.
  // The graph looks like this:
  //        a
  //     b     c
  //          d e
  NodePtr a(new Node);
  NodePtr b(new Node);
  NodePtr c(new Node);
  NodePtr d(new Node);
  NodePtr e(new Node);

  StateTablePtr state_table;

  state_table = new StateTable();
  state_table->Enable(StateTable::kBlend, true);
  a->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kCullFace, true);
  b->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kDepthTest, true);
  c->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kScissorTest, true);
  d->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kStencilTest, true);
  e->SetStateTable(state_table);

  BuildRectangleShape<uint16>();
  a->AddShape(s_data.shape);
  b->AddShape(s_data.shape);
  c->AddShape(s_data.shape);
  d->AddShape(s_data.shape);
  e->AddShape(s_data.shape);

  a->AddChild(b);
  a->AddChild(c);
  c->AddChild(d);
  c->AddChild(e);

  AddDefaultUniformsToNode(a);

  // Draw the scene.
  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(a);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST)"));

  // Disable node b and render again.
  Reset();
  b->Enable(false);
  a->GetStateTable()->Enable(StateTable::kBlend, false);
  renderer->DrawScene(a);
  // The blend state won't be sent again because it is already enabled from the
  // first draw call.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST)"));

  // Disable node c and render again.
  Reset();
  c->Enable(false);
  renderer->DrawScene(a);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST)"));
}

TEST_F(RendererTest, StateCompression) {
  NodePtr a(new Node);
  NodePtr b(new Node);
  NodePtr c(new Node);
  NodePtr d(new Node);
  NodePtr e(new Node);

  StateTablePtr state_table;

  state_table = new StateTable();
  state_table->Enable(StateTable::kBlend, true);
  a->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kCullFace, true);
  b->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kBlend, true);
  state_table->Enable(StateTable::kDepthTest, true);
  c->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kScissorTest, true);
  d->SetStateTable(state_table);

  state_table = new StateTable();
  state_table->Enable(StateTable::kStencilTest, true);
  e->SetStateTable(state_table);

  BuildRectangleShape<uint16>();
  a->AddShape(s_data.shape);
  b->AddShape(s_data.shape);
  c->AddShape(s_data.shape);
  d->AddShape(s_data.shape);
  e->AddShape(s_data.shape);

  AddDefaultUniformsToNode(a);
  AddDefaultUniformsToNode(b);
  AddDefaultUniformsToNode(c);

  // Draw a, which should set blend and nothing else.
  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(a);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));

  Reset();
  renderer->DrawScene(c);
  // Drawing c should just enable depth test, since blending is already enabled.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));

  Reset();
  renderer->DrawScene(b);
  // Drawing b should disable blending and depth test but enable cull face.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_CULL_FACE)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));

  // Try hierarchies of nodes; the graphs look like this:
  //     a     c
  //     b    d e
  a->AddChild(b);
  Reset();
  renderer->DrawScene(a);
  // When a is drawn cull face is disabled but blending enabled, and then
  // the cull face re-enabled when b is drawn. Depth testing should not be
  // modified since it is currently disabled.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_CULL_FACE)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_CULL_FACE)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST)"));

  c->AddChild(d);
  c->AddChild(e);
  Reset();
  renderer->DrawScene(c);
  // First cull face is disabled since none of the nodes use it, then depth test
  // is enabled (blending is already enabled!), and will stay so through
  // inheritance.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_CULL_FACE)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_BLEND)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST)"));

  // Drawing d will enable scissor test, while drawing e will disable it and
  // enable stencil test.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_STENCIL_TEST)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_SCISSOR_TEST)"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_STENCIL_TEST)"));

  // Create a scene that is very deep (> 16) and ensure state changes happen.
  renderer.Reset(NULL);
  Renderer::DestroyCurrentStateCache();
  renderer.Reset(new Renderer(gm_));
  Reset();
  NodePtr root(new Node);
  AddDefaultUniformsToNode(root);
  NodePtr last_node = root;
  // Flip each cap in a new child node.
  for (int i = 0; i < StateTable::GetCapabilityCount(); ++i) {
    NodePtr node(new Node);
    StateTablePtr state_table(new StateTable);
    const StateTable::Capability cap = static_cast<StateTable::Capability>(i);
    if (cap == StateTable::kDither)
      state_table->Enable(cap, false);
    else
      state_table->Enable(cap, true);
    node->SetStateTable(state_table);
    node->AddShape(s_data.shape);
    last_node->AddChild(node);
    last_node = node;
  }
  // Flip them back...
  for (int i = 0; i < StateTable::GetCapabilityCount(); ++i) {
    NodePtr node(new Node);
    StateTablePtr state_table(new StateTable);
    const StateTable::Capability cap = static_cast<StateTable::Capability>(i);
    if (cap == StateTable::kDither)
      state_table->Enable(cap, true);
    else
      state_table->Enable(cap, false);
    node->SetStateTable(state_table);
    node->AddShape(s_data.shape);
    last_node->AddChild(node);
    last_node = node;
  }
  // ... and back again.
  for (int i = 0; i < StateTable::GetCapabilityCount(); ++i) {
    NodePtr node(new Node);
    StateTablePtr state_table(new StateTable);
    const StateTable::Capability cap = static_cast<StateTable::Capability>(i);
    if (cap == StateTable::kDither)
      state_table->Enable(cap, false);
    else
      state_table->Enable(cap, true);
    node->SetStateTable(state_table);
    node->AddShape(s_data.shape);
    last_node->AddChild(node);
    last_node = node;
  }
  renderer->DrawScene(root);
  EXPECT_EQ(StateTable::GetCapabilityCount() * 2 - 1,
            static_cast<int>(trace_verifier_->GetCountOf("Enable")));
  EXPECT_EQ(StateTable::GetCapabilityCount() + 1,
            static_cast<int>(trace_verifier_->GetCountOf("Disable")));
}

TEST_F(RendererTest, ReadImage) {
  ImagePtr image;
  RendererPtr renderer(new Renderer(gm_));
  base::AllocatorPtr al;

  image = renderer->ReadImage(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(50U, 80U)),
      Image::kRgb565, al);
  EXPECT_TRUE(image->GetData()->GetData() != NULL);
  EXPECT_EQ(Image::kRgb565, image->GetFormat());
  EXPECT_EQ(50U, image->GetWidth());
  EXPECT_EQ(80U, image->GetHeight());

  image = renderer->ReadImage(
      Range2i::BuildWithSize(Point2i(20, 10), Vector2i(50U, 80U)),
      Image::kRgba8888, al);
  EXPECT_TRUE(image->GetData()->GetData() != NULL);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(50U, image->GetWidth());
  EXPECT_EQ(80U, image->GetHeight());

  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  renderer->BindFramebuffer(fbo);
  image = renderer->ReadImage(
      Range2i::BuildWithSize(Point2i(0, 0), Vector2i(128U, 128U)),
      Image::kRgb888, al);
  EXPECT_TRUE(image->GetData()->GetData() != NULL);
  EXPECT_EQ(Image::kRgb888, image->GetFormat());
  EXPECT_EQ(128U, image->GetWidth());
  EXPECT_EQ(128U, image->GetHeight());
  renderer->BindFramebuffer(FramebufferObjectPtr());

  image = renderer->ReadImage(
      Range2i::BuildWithSize(Point2i(20, 10), Vector2i(50U, 80U)),
      Image::kRgba8888, al);
  EXPECT_TRUE(image->GetData()->GetData() != NULL);
  EXPECT_EQ(Image::kRgba8888, image->GetFormat());
  EXPECT_EQ(50U, image->GetWidth());
  EXPECT_EQ(80U, image->GetHeight());
}

TEST_F(RendererTest, MappedBuffer) {
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));
  // Ensure static data is available.
  NodePtr root = BuildGraph(kWidth, kHeight);
  Reset();

  const BufferObject::MappedBufferData& mbd =
      s_data.vertex_buffer->GetMappedData();
  const Range1ui full_range(
      0U, static_cast<uint32>(s_data.vertex_buffer->GetStructSize() *
                              s_data.vertex_buffer->GetCount()));

  // The buffer should not have any mapped data by default.
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == NULL);

  gm_->EnableFunctionGroup(GraphicsManager::kMapBuffer, false);
  gm_->EnableFunctionGroup(GraphicsManager::kMapBufferBase, false);
  gm_->EnableFunctionGroup(GraphicsManager::kMapBufferRange, false);
  renderer->MapBufferObjectData(s_data.vertex_buffer, Renderer::kWriteOnly);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // The data should have been mapped with a client-side pointer.
  EXPECT_EQ(full_range, mbd.range);
  EXPECT_FALSE(mbd.pointer == NULL);

  // Trying to map again should log a warning.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  renderer->MapBufferObjectData(s_data.vertex_buffer, Renderer::kWriteOnly);
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "buffer that is already mapped was passed"));

  // Unmapping the buffer should free the pointer.
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);

  // Unmapping again should log a warning.
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "unmapped BufferObject was passed"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Now use the GL functions.
  gm_->EnableFunctionGroup(GraphicsManager::kMapBuffer, true);
  gm_->EnableFunctionGroup(GraphicsManager::kMapBufferBase, true);

  // Simulate a failed GL call.
  gm_->SetForceFunctionFailure("MapBuffer", true);
  renderer->MapBufferObjectData(s_data.vertex_buffer, Renderer::kWriteOnly);
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Failed to allocate data for"));
  gm_->SetForceFunctionFailure("MapBuffer", false);

  Reset();
  renderer->MapBufferObjectData(s_data.vertex_buffer, Renderer::kWriteOnly);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(2, "GL_WRITE_ONLY"));

  EXPECT_FALSE(log_checker.HasAnyMessages());
  renderer->MapBufferObjectData(s_data.vertex_buffer, Renderer::kWriteOnly);
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "buffer that is already mapped was passed"));

  // Check that the mapped data changed.
  EXPECT_EQ(full_range, mbd.range);
  EXPECT_FALSE(mbd.pointer == NULL);
  EXPECT_TRUE(mbd.gpu_mapped);

  Reset();
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_TRUE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "unmapped BufferObject was passed"));
  // An additional call should not have been made.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Map using different access modes.
  Reset();
  renderer->MapBufferObjectData(s_data.vertex_buffer, Renderer::kReadOnly);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(2, "GL_READ_ONLY"));
  EXPECT_TRUE(mbd.gpu_mapped);
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_FALSE(mbd.gpu_mapped);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  Reset();
  renderer->MapBufferObjectData(s_data.vertex_buffer, Renderer::kReadWrite);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(mbd.gpu_mapped);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(2, "GL_READ_WRITE"));
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_FALSE(mbd.gpu_mapped);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Check that when the range is the entire buffer and MapBufferRange() is not
  // supported that we fall back to MapBuffer().
  Reset();
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, full_range);
  // Despite the call to MapBufferObjectDataRange(), MapBuffer() should have
  // been called.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(2, "GL_WRITE_ONLY"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_TRUE(mbd.gpu_mapped);

  // The entire buffer should be mapped.
  EXPECT_EQ(full_range, mbd.range);
  EXPECT_FALSE(mbd.pointer == NULL);
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_FALSE(mbd.gpu_mapped);

  // Check that platforms that do not support MapBufferRange() fall back to
  // a client-side pointer.
  Range1ui range(4U, 8U);
  Reset();
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, range);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBuffer"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  EXPECT_FALSE(mbd.gpu_mapped);

  // The range should be mapped.
  EXPECT_EQ(range, mbd.range);
  EXPECT_FALSE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_TRUE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);

  // Map a range of data using a client side pointer.
  Reset();
  gm_->EnableFunctionGroup(GraphicsManager::kMapBuffer, false);
  gm_->EnableFunctionGroup(GraphicsManager::kMapBufferBase, false);
  gm_->EnableFunctionGroup(GraphicsManager::kMapBufferRange, false);
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, Range1ui());
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Ignoring empty range"));
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, range);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  // Check that the mapped data changed.
  EXPECT_EQ(range, mbd.range);
  EXPECT_FALSE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);

  // Trying to map again should log a warning.
  EXPECT_FALSE(log_checker.HasAnyMessages());
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, Range1ui());
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "buffer that is already mapped was passed"));
  EXPECT_FALSE(mbd.gpu_mapped);

  // Unmapping the buffer should free the pointer.
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "unmapped BufferObject was passed"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("UnmapBuffer"));

  // Now use the GL function.
  gm_->EnableFunctionGroup(GraphicsManager::kMapBufferRange, true);
  gm_->EnableFunctionGroup(GraphicsManager::kMapBufferBase, true);

  // Simulate a failed GL call.
  gm_->SetForceFunctionFailure("MapBufferRange", true);
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, range);
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Failed to allocate data for"));
  gm_->SetForceFunctionFailure("MapBufferRange", false);

  Reset();
  // An empty range should only log a warning message.
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, Range1ui());
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "Ignoring empty range"));
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);

  // Try a range that is too large.
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, Range1ui(0, 16384U));
  gm_->SetErrorCode(GL_NO_ERROR);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Failed to allocate data for"));
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);

  Reset();
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, range);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(mbd.gpu_mapped);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(4, "GL_MAP_WRITE_BIT"));
  EXPECT_FALSE(log_checker.HasAnyMessages());

  // Check that the mapped data changed.
  Reset();
  EXPECT_EQ(range, mbd.range);
  EXPECT_FALSE(mbd.pointer == NULL);

  // Try again to get a warning.
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kWriteOnly, Range1ui());
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "buffer that is already mapped was passed"));
  EXPECT_TRUE(mbd.gpu_mapped);

  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_TRUE(mbd.range.IsEmpty());
  EXPECT_TRUE(mbd.pointer == NULL);
  EXPECT_FALSE(mbd.gpu_mapped);
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  // An additional call should not have been made.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "unmapped BufferObject was passed"));
  EXPECT_FALSE(mbd.gpu_mapped);

  // Map using different access modes.
  Reset();
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kReadOnly, range);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(mbd.gpu_mapped);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(4, "GL_MAP_READ_BIT"));
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_FALSE(mbd.gpu_mapped);
  Reset();
  renderer->MapBufferObjectDataRange(
      s_data.vertex_buffer, Renderer::kReadWrite, range);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("MapBufferRange"));
  EXPECT_TRUE(mbd.gpu_mapped);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "MapBuffer"))
          .HasArg(4, "GL_MAP_READ_BIT | GL_MAP_WRITE_BIT"));
  renderer->UnmapBufferObjectData(s_data.vertex_buffer);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UnmapBuffer"));
  EXPECT_FALSE(mbd.gpu_mapped);

  // Reset data.
  s_data.rect = NULL;
  s_data.vertex_container = NULL;
  BuildRectangle();
}

TEST_F(RendererTest, Flags) {
  // Test that flags can be set properly.
  RendererPtr renderer(new Renderer(gm_));
  // By default all process flags are set.
  Renderer::Flags flags = Renderer::AllProcessFlags();
  EXPECT_EQ(flags, renderer->GetFlags());

  flags.reset(Renderer::kProcessReleases);
  renderer->ClearFlag(Renderer::kProcessReleases);
  EXPECT_EQ(flags, renderer->GetFlags());

  flags.set(Renderer::kRestoreShaderProgram);
  renderer->SetFlag(Renderer::kRestoreShaderProgram);
  EXPECT_EQ(flags, renderer->GetFlags());

  flags.set(Renderer::kProcessInfoRequests);
  renderer->SetFlag(Renderer::kProcessInfoRequests);
  EXPECT_EQ(flags, renderer->GetFlags());

  flags.reset(Renderer::kProcessInfoRequests);
  renderer->ClearFlag(Renderer::kProcessInfoRequests);
  EXPECT_EQ(flags, renderer->GetFlags());

  // Setting no flags should do nothing.
  renderer->ClearFlags(Renderer::AllFlags());
  renderer->SetFlags(Renderer::Flags());
  EXPECT_EQ(0U, renderer->GetFlags().count());

  // Multiple flags.
  flags.reset();
  flags.set(Renderer::kProcessInfoRequests);
  flags.set(Renderer::kProcessReleases);
  flags.set(Renderer::kRestoreShaderProgram);
  flags.set(Renderer::kRestoreVertexArray);
  renderer->SetFlags(flags);
  EXPECT_EQ(4U, flags.count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Clearing no flags should do nothing.
  flags.reset();
  renderer->ClearFlags(flags);
  EXPECT_EQ(4U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Setting no flags should do nothing.
  flags.reset();
  renderer->SetFlags(flags);
  EXPECT_EQ(4U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Try to reset some unset flags.
  flags.reset();
  renderer->ClearFlags(flags);
  EXPECT_EQ(4U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Nothing should have changed.
  renderer->ClearFlags(flags);
  EXPECT_EQ(4U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessReleases));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreShaderProgram));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));

  // Reset some set flags.
  flags.reset();
  flags.set(Renderer::kProcessReleases);
  flags.set(Renderer::kRestoreShaderProgram);
  renderer->ClearFlags(flags);
  EXPECT_EQ(2U, renderer->GetFlags().count());
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kProcessInfoRequests));
  EXPECT_TRUE(renderer->GetFlags().test(Renderer::kRestoreVertexArray));
}

TEST_F(RendererTest, FlagsBehavior) {
  // Test the behavior of the Renderer when different flags are set.

  // Test kProcessInfoRequests.
  {
    RendererPtr renderer(new Renderer(gm_));
    ResourceManager* manager = renderer->GetResourceManager();
    CallbackHelper<ResourceManager::PlatformInfo> callback;

    manager->RequestPlatformInfo(
        std::bind(&CallbackHelper<ResourceManager::PlatformInfo>::Callback,
                  &callback, std::placeholders::_1));

    renderer->ClearFlag(Renderer::kProcessInfoRequests);
    renderer->DrawScene(NodePtr());
    EXPECT_FALSE(callback.was_called);

    renderer->SetFlag(Renderer::kProcessInfoRequests);
    renderer->DrawScene(NodePtr());
    EXPECT_TRUE(callback.was_called);

    // It is possible that in our test platform, we cannot grab some of the
    // capabilities and it will generate an error.
    gm_->SetErrorCode(GL_NO_ERROR);
  }

  // Test kProcessReleases.
  {
    RendererPtr renderer(new Renderer(gm_));
    NodePtr root = BuildGraph(800, 800);
    // Drawing will create resources.
    renderer->DrawScene(root);
    Reset();
    // These will trigger resources to be released.
    s_data.attribute_array = NULL;
    s_data.vertex_buffer = NULL;
    s_data.index_buffer = NULL;
    s_data.shader = NULL;
    s_data.shape = NULL;
    s_data.rect = NULL;
    root->ClearChildren();
    root->SetShaderProgram(ShaderProgramPtr(NULL));
    // Tell the renderer not to process releases.
    renderer->ClearFlag(Renderer::kProcessReleases);
    renderer->DrawScene(root);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteBuffers"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteProgram"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteShader"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("DeleteVertexArrays"));

    // Tell the renderer to process releases.
    renderer->SetFlag(Renderer::kProcessReleases);
    Reset();
    renderer->DrawScene(root);
    // Most objects will be destroyed since the resources go away.
    std::vector<std::string> call_strings;
    call_strings.push_back("Clear");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteProgram");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteVertexArrays");
    EXPECT_TRUE(trace_verifier_->VerifySortedCalls(call_strings));

    // Reset data.
    s_data.rect = NULL;
    BuildRectangle();
  }

  // Test k(Restore|Save)*.
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(
        gm_, renderer, trace_verifier_, Renderer::kSaveActiveTexture,
        Renderer::kRestoreActiveTexture, "GetIntegerv(GL_ACTIVE_TEXTURE",
        "ActiveTexture"));
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(
        gm_, renderer, trace_verifier_, Renderer::kSaveArrayBuffer,
        Renderer::kRestoreArrayBuffer, "GetIntegerv(GL_ARRAY_BUFFER_BINDING",
        "BindBuffer(GL_ARRAY_BUFFER"));
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(
        gm_, renderer, trace_verifier_, Renderer::kSaveElementArrayBuffer,
        Renderer::kRestoreElementArrayBuffer,
        "GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING",
        "BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    FramebufferObjectPtr fbo(new FramebufferObject(
        128, 128));
    fbo->SetColorAttachment(0U,
                            FramebufferObject::Attachment(Image::kRgba4Byte));
    renderer->BindFramebuffer(fbo);
    EXPECT_TRUE(VerifySaveAndRestoreFlag(
        gm_, renderer, trace_verifier_, Renderer::kSaveFramebuffer,
        Renderer::kRestoreFramebuffer, "GetIntegerv(GL_FRAMEBUFFER_BINDING",
        "BindFramebuffer"));
    EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(
        gm_, renderer, trace_verifier_, Renderer::kSaveShaderProgram,
        Renderer::kRestoreShaderProgram, "GetIntegerv(GL_CURRENT_PROGRAM",
        "UseProgram"));
  }
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifySaveAndRestoreFlag(
        gm_, renderer, trace_verifier_, Renderer::kSaveVertexArray,
        Renderer::kRestoreVertexArray, "GetIntegerv(GL_VERTEX_ARRAY_BINDING",
        "BindVertexArray"));
  }
  {
    // Saving and restoring StateTables is a little more complicated.
    RendererPtr renderer(new Renderer(gm_));

    renderer->ClearFlag(Renderer::kRestoreStateTable);
    renderer->SetFlag(Renderer::kSaveStateTable);
    Reset();
    renderer->DrawScene(NodePtr());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsEnabled(GL_DEPTH_TEST"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsEnabled(GL_BLEND"));
    renderer->ClearFlag(Renderer::kSaveStateTable);

    // Now change a bunch of state.
    Reset();
    NodePtr root = BuildGraph(800, 800);
    root->GetStateTable()->Enable(StateTable::kDepthTest, false);
    root->GetStateTable()->Enable(StateTable::kBlend, true);
    renderer->DrawScene(root);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_BLEND"));

    // Drawing again with no flags set should do nothing.
    Reset();
    renderer->DrawScene(NodePtr());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable"));

    Reset();
    renderer->SetFlag(Renderer::kRestoreStateTable);
    renderer->DrawScene(NodePtr());
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Disable(GL_DEPTH_TEST"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("Enable(GL_BLEND"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Enable(GL_DEPTH_TEST"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("Disable(GL_BLEND"));
  }

  // Test all save/restore flags simultaneously.
  {
    RendererPtr renderer(new Renderer(gm_));
    EXPECT_TRUE(VerifyAllSaveAndRestoreFlags(gm_, renderer));
  }

  // Test kClear*.
  {
    EXPECT_TRUE(VerifyClearFlag(gm_, Renderer::kClearActiveTexture,
                                GL_ACTIVE_TEXTURE, GL_TEXTURE0));
    EXPECT_TRUE(VerifyClearFlag(gm_, Renderer::kClearArrayBuffer,
                                GL_ARRAY_BUFFER_BINDING, 0));
    EXPECT_TRUE(VerifyClearFlag(gm_, Renderer::kClearElementArrayBuffer,
                                GL_ELEMENT_ARRAY_BUFFER_BINDING, 0));
    EXPECT_TRUE(VerifyClearFlag(gm_, Renderer::kClearFramebuffer,
                                GL_FRAMEBUFFER_BINDING, 0));
    EXPECT_TRUE(VerifyClearImageUnitFlag(gm_, Renderer::kClearSamplers,
                                         GL_SAMPLER_BINDING, 0));
    EXPECT_TRUE(VerifyClearFlag(gm_, Renderer::kClearShaderProgram,
                                GL_CURRENT_PROGRAM, 0));
    EXPECT_TRUE(VerifyClearImageUnitFlag(gm_, Renderer::kClearCubemaps,
                                         GL_TEXTURE_BINDING_CUBE_MAP, 0));
    EXPECT_TRUE(VerifyClearImageUnitFlag(gm_, Renderer::kClearTextures,
                                         GL_TEXTURE_BINDING_2D, 0));
    EXPECT_TRUE(VerifyClearFlag(gm_, Renderer::kClearVertexArray,
                                GL_VERTEX_ARRAY_BINDING, 0));

    // Check some corner cases. First, clearing the framebuffer should also
    // clear the cached FramebufferPtr.
    {
      NodePtr root = BuildGraph(800, 800);
      RendererPtr renderer(new Renderer(gm_));
      FramebufferObjectPtr fbo(new FramebufferObject(
          128, 128));
      fbo->SetColorAttachment(0U,
                              FramebufferObject::Attachment(Image::kRgba4Byte));
      renderer->BindFramebuffer(fbo);

      renderer->SetFlag(Renderer::kClearFramebuffer);
      renderer->DrawScene(root);
      // The framebuffer should have been cleared.
      EXPECT_TRUE(renderer->GetCurrentFramebuffer().Get() == NULL);
    }

    // Restoring a program binding should override clearing it.
    {
      NodePtr root = BuildGraph(800, 800);
      RendererPtr renderer(new Renderer(gm_));
      renderer->DrawScene(root);
      Reset();
      renderer->SetFlag(Renderer::kClearShaderProgram);
      renderer->SetFlag(Renderer::kRestoreShaderProgram);
      renderer->SetFlag(Renderer::kSaveShaderProgram);
      renderer->DrawScene(root);
      EXPECT_EQ(0U, trace_verifier_->GetCountOf("UseProgram(0x0)"));
    }

    // Restoring a VAO binding should override clearing it.
    {
      NodePtr root = BuildGraph(800, 800);
      RendererPtr renderer(new Renderer(gm_));
      renderer->DrawScene(root);
      Reset();
      renderer->SetFlag(Renderer::kClearVertexArray);
      renderer->SetFlag(Renderer::kRestoreVertexArray);
      renderer->SetFlag(Renderer::kSaveVertexArray);
      renderer->DrawScene(NodePtr());
      EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindVertexArray(0x0)"));
    }
  }
}

TEST_F(RendererTest, InitialUniformValue) {
  // Check that it is possible to set initial Uniform values.
  NodePtr node(new Node);
  BuildRectangleShape<uint16>();
  node->AddShape(s_data.shape);

  const math::Matrix4f mat1(1.f, 2.f, 3.f, 4.f,
                           5.f, 6.f, 7.f, 8.f,
                           9.f, 1.f, 2.f, 3.f,
                           4.f, 5.f, 6.f, 7.f);
  const math::Matrix4f mat2(1.f, 2.f, 3.f, 4.f,
                           9.f, 1.f, 2.f, 3.f,
                           5.f, 6.f, 7.f, 8.f,
                           4.f, 5.f, 6.f, 7.f);
  const math::Vector4f vec(1.f, 2.f, 3.f, 4.f);


  RendererPtr renderer(new Renderer(gm_));

  // Create some uniform values.
  const ShaderInputRegistryPtr& reg = ShaderInputRegistry::GetGlobalRegistry();
  const Uniform modelview_matrix =
      reg->Create<Uniform>("uModelviewMatrix", mat1);
  const Uniform projection_matrix =
      reg->Create<Uniform>("uProjectionMatrix", mat2);
  const Uniform color = reg->Create<Uniform>("uBaseColor", vec);

  renderer->SetInitialUniformValue(modelview_matrix);
  renderer->SetInitialUniformValue(projection_matrix);
  renderer->SetInitialUniformValue(color);

  // Check that the values were set correctly.
  ResourceManager* manager = renderer->GetResourceManager();
  CallbackHelper<ResourceManager::ProgramInfo> callback;
  manager->RequestAllResourceInfos<ShaderProgram, ResourceManager::ProgramInfo>(
      std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                &callback, std::placeholders::_1));
  renderer->DrawScene(node);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ(3U, callback.infos[0].uniforms.size());
  EXPECT_EQ("uProjectionMatrix", callback.infos[0].uniforms[0].name);
  EXPECT_EQ(static_cast<GLuint>(GL_FLOAT_MAT4),
            callback.infos[0].uniforms[0].type);
  EXPECT_EQ(mat2, callback.infos[0].uniforms[0].value.Get<math::Matrix4f>());


  EXPECT_EQ("uModelviewMatrix", callback.infos[0].uniforms[1].name);
  EXPECT_EQ(static_cast<GLuint>(GL_FLOAT_MAT4),
            callback.infos[0].uniforms[1].type);
  EXPECT_EQ(mat1, callback.infos[0].uniforms[1].value.Get<math::Matrix4f>());

  EXPECT_EQ("uBaseColor", callback.infos[0].uniforms[2].name);
  EXPECT_EQ(static_cast<GLuint>(GL_FLOAT_VEC4),
            callback.infos[0].uniforms[2].type);
  EXPECT_TRUE(math::VectorBase4f::AreValuesEqual(
      vec, callback.infos[0].uniforms[2].value.Get<math::VectorBase4f>()));
}

TEST_F(RendererTest, CombinedUniformsSent) {
  // Check that combined uniforms that change are sent.
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(800, 800);
  const ShaderInputRegistryPtr& global_reg =
        ShaderInputRegistry::GetGlobalRegistry();
  root->AddUniform(global_reg->Create<Uniform>("uModelviewMatrix",
      math::TranslationMatrix(math::Vector3f(0.5f, 0.5f, 0.5f))));
  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("UniformMatrix4fv"));
  Reset();
  renderer->DrawScene(root);
  // Combined uModelviewMatrix generates a new stamp so it is sent.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UniformMatrix4fv"));
  s_data.rect->SetUniformValue(
      s_data.rect->GetUniformIndex("uModelviewMatrix"),
      math::TranslationMatrix(math::Vector3f(-0.5f, 0.5f, 0.0f)));
  Reset();
  renderer->DrawScene(root);
  // The combined uniform is different, so it should have been sent.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("UniformMatrix4fv"));
}

TEST_F(RendererTest, GeneratedUniformsSent) {
  // Check that generated uniforms are properly created and sent.
  TracingHelper helper;

  ShaderInputRegistryPtr reg(new ShaderInputRegistry);
  reg->IncludeGlobalRegistry();
  reg->Add(ShaderInputRegistry::UniformSpec(
      "uTranslationMatrix", kMatrix4x4Uniform, "", CombineMatrices,
      ExtractTranslation));
  // Add the spec for the generated uniform.
  reg->Add(ShaderInputRegistry::UniformSpec("uTranslationX", kFloatUniform));
  reg->Add(ShaderInputRegistry::UniformSpec("uTranslationY", kFloatUniform));
  reg->Add(ShaderInputRegistry::UniformSpec("uTranslationZ", kFloatUniform));
  BuildGraph(800, 800);

  static const char* kVertexShaderString =
      "attribute vec3 aVertex;\n"
      "attribute vec2 aTexCoords;\n"
      "uniform mat4 uTranslationMatrix;\n";

  static const char* kFragmentShaderString =
      "uniform float uTranslationX;\n"
      "uniform float uTranslationY;\n"
      "uniform float uTranslationZ;\n";

  ShaderProgramPtr shader = ShaderProgram::BuildFromStrings(
      "Shader", reg, kVertexShaderString, kFragmentShaderString,
      base::AllocatorPtr());
  s_data.shape->SetAttributeArray(s_data.attribute_array);
  s_data.rect->SetShaderProgram(s_data.shader);
  s_data.rect->ClearUniforms();
  s_data.rect->AddUniform(
      reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
  s_data.rect->AddUniform(
          reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));

  RendererPtr renderer(new Renderer(gm_));
  NodePtr root(new Node);
  root->AddUniform(reg->Create<Uniform>("uTranslationMatrix",
      math::TranslationMatrix(math::Vector3f(0.5f, 0.5f, 0.5f))));
  root->SetShaderProgram(shader);

  NodePtr child1(new Node);
  child1->AddUniform(reg->Create<Uniform>("uTranslationMatrix",
      math::TranslationMatrix(math::Vector3f(2.0f, 4.0f, 6.0f))));

  NodePtr child2(new Node);
  child2->AddUniform(reg->Create<Uniform>("uTranslationMatrix",
      math::TranslationMatrix(math::Vector3f(10.0f, 8.0f, 6.0f))));

  root->AddChild(child1);
  child1->AddChild(child2);
  root->AddShape(s_data.shape);
  child1->AddShape(s_data.shape);
  child2->AddShape(s_data.shape);

  Reset();
  renderer->DrawScene(root);
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("UniformMatrix4fv"));
  EXPECT_EQ(9U, trace_verifier_->GetCountOf("Uniform1fv"));
  math::Vector3f vec;
  vec.Set(0.5f, 0.5f, 0.5f);
  math::Matrix4f mat = math::Transpose(
      math::TranslationMatrix(vec));
  const float* mat_floats = &mat[0][0];
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                0U, "UniformMatrix4fv"))
                  .HasArg(4, helper.ToString("GLmatrix4*", mat_floats)));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                0U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[0])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                1U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[1])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                2U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[2])));
  vec.Set(2.5f, 4.5f, 6.5f);
  mat = math::Transpose(math::TranslationMatrix(vec));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                1U, "UniformMatrix4fv"))
                  .HasArg(4, helper.ToString("GLmatrix4*", mat_floats)));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                3U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[0])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                4U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[1])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                5U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[2])));
  vec.Set(12.5f, 12.5f, 12.5f);
  mat = math::Transpose(math::TranslationMatrix(vec));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                2U, "UniformMatrix4fv"))
                  .HasArg(4, helper.ToString("GLmatrix4*", mat_floats)));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                6U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[0])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                7U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[1])));
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(trace_verifier_->GetNthIndexOf(
                                                8U, "Uniform1fv"))
                  .HasArg(3, base::ValueToString(vec[2])));
}

TEST_F(RendererTest, ConcurrentShader) {
  // Check that different threads can have different uniform values
  // set on the same shader when per-thread uniforms are enabled.
  static const char* kVertexShaderString =
      "uniform float uFloat;\n"
      "void main(){}\n";
  static const char* kFragmentShaderString =
      "void main(){}\n";

  RendererPtr renderer(new Renderer(gm_));
  ShaderInputRegistryPtr reg(new ShaderInputRegistry);

  BuildRectangleShape<uint16>();
  NodePtr root(new Node);
  root->AddShape(s_data.shape);
  size_t uindex = root->AddUniform(reg->Create<Uniform>("uFloat", 0.f));

  ResourceManager* manager = renderer->GetResourceManager();
  MockVisual share_visual(*visual_);
  std::vector<ResourceManager::ProgramInfo> other_infos;

  {
    // Default: shared uniforms
    Reset();
    ShaderProgramPtr shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    root->SetShaderProgram(shader);
    root->SetUniformValue(uindex, -7.f);
    CallbackHelper<ResourceManager::ProgramInfo> before, after;
    manager->RequestAllResourceInfos<ShaderProgram,
                                     ResourceManager::ProgramInfo>(
        std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                  &before, std::placeholders::_1));
    renderer->DrawScene(root);

    EXPECT_TRUE(before.was_called);
    EXPECT_EQ(1U, before.infos.size());
    EXPECT_EQ(1U, before.infos[0].uniforms.size());
    EXPECT_EQ(-7.f, before.infos[0].uniforms[0].value.Get<float>());

    std::function<bool()> thread_function = std::bind(UniformThread,
        renderer, &share_visual, root, uindex, 2.f, &other_infos);
    port::ThreadId tid = port::SpawnThreadStd(&thread_function);
    port::JoinThread(tid);

    EXPECT_EQ(1U, other_infos.size());
    EXPECT_EQ(1U, other_infos[0].uniforms.size());
    EXPECT_EQ(2.f, other_infos[0].uniforms[0].value.Get<float>());

    manager->RequestAllResourceInfos<ShaderProgram,
                                     ResourceManager::ProgramInfo>(
        std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                  &after, std::placeholders::_1));
    renderer->ProcessResourceInfoRequests();
    EXPECT_TRUE(after.was_called);
    EXPECT_EQ(1U, after.infos.size());
    EXPECT_EQ(1U, after.infos[0].uniforms.size());
    EXPECT_EQ(2.f, after.infos[0].uniforms[0].value.Get<float>());
  }

  {
    // Per-thread uniforms
    Reset();
    ShaderProgramPtr shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    shader->SetConcurrent(true);
    root->SetShaderProgram(shader);
    root->SetUniformValue(uindex, -7.f);
    CallbackHelper<ResourceManager::ProgramInfo> before, after;
    manager->RequestAllResourceInfos<ShaderProgram,
                                     ResourceManager::ProgramInfo>(
        std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                  &before, std::placeholders::_1));
    renderer->DrawScene(root);

    EXPECT_TRUE(before.was_called);
    EXPECT_EQ(1U, before.infos.size());
    EXPECT_EQ(1U, before.infos[0].uniforms.size());
    EXPECT_EQ(-7.f, before.infos[0].uniforms[0].value.Get<float>());

    std::function<bool()> thread_function = std::bind(UniformThread,
        renderer, &share_visual, root, uindex, 2.f, &other_infos);
    port::ThreadId tid = port::SpawnThreadStd(&thread_function);
    port::JoinThread(tid);

    EXPECT_EQ(1U, other_infos.size());
    EXPECT_EQ(1U, other_infos[0].uniforms.size());
    EXPECT_EQ(2.f, other_infos[0].uniforms[0].value.Get<float>());

    manager->RequestAllResourceInfos<ShaderProgram,
                                     ResourceManager::ProgramInfo>(
        std::bind(&CallbackHelper<ResourceManager::ProgramInfo>::Callback,
                  &after, std::placeholders::_1));
    renderer->ProcessResourceInfoRequests();
    EXPECT_TRUE(after.was_called);
    EXPECT_EQ(1U, after.infos.size());
    EXPECT_EQ(1U, after.infos[0].uniforms.size());
    EXPECT_EQ(-7.f, after.infos[0].uniforms[0].value.Get<float>());
  }
}

TEST_F(RendererTest, CreateResourceWithExternallyManagedId) {
  NodePtr root = BuildGraph(800, 800);

  // Test out the individual resource creation functions.
  RendererPtr renderer(new Renderer(gm_));
  // Ensure a resource binder exists.
  renderer->DrawScene(NodePtr());

  // BufferObject.
  GLuint id;
  gm_->GenBuffers(1, &id);
  Reset();
  // An invalid ID does nothing.
  renderer->CreateResourceWithExternallyManagedId(s_data.vertex_buffer.Get(),
                                                  2345U);
  EXPECT_EQ(1U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsBuffer"));

  renderer->CreateResourceWithExternallyManagedId(s_data.vertex_buffer.Get(),
                                                  id);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER, 0x" +
                                            base::ValueToString(id)));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // IndexBuffer.
  gm_->GenBuffers(1, &id);
  Reset();
  // An invalid ID does nothing.
  renderer->CreateResourceWithExternallyManagedId(
      s_data.shape->GetIndexBuffer().Get(), 2345U);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsBuffer"));

  renderer->CreateResourceWithExternallyManagedId(
      s_data.shape->GetIndexBuffer().Get(), id);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(
      1U, trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0x" +
                                      base::ValueToString(id)));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER"));

  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, false);
  // Texture.
  gm_->GenTextures(1, &id);
  Reset();
  // An invalid ID does nothing.
  renderer->CreateResourceWithExternallyManagedId(s_data.texture.Get(), 2345U);
  EXPECT_EQ(1U, trace_verifier_->GetCallCount());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("IsTexture"));

  renderer->CreateResourceWithExternallyManagedId(s_data.texture.Get(), id);
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D, 0x" +
                                            base::ValueToString(id)));
  EXPECT_EQ(13U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, true);

  Reset();
  // Destroy all resources.
  renderer.Reset(NULL);
  s_data.attribute_array = NULL;
  s_data.vertex_buffer = NULL;
  s_data.index_buffer = NULL;
  s_data.shader = NULL;
  s_data.shape = NULL;
  s_data.texture = NULL;
  s_data.rect = NULL;
  // Check that the managed resources were not deleted.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("Delete"));
}

TEST_F(RendererTest, CreateOrUpdateResources) {
  NodePtr root = BuildGraph(800, 800);

  // Test out the individual resource creation functions.
  {
    RendererPtr renderer(new Renderer(gm_));

    // AttributeArray. Only buffer data will be bound and sent.
    Reset();
    renderer->CreateOrUpdateResource(s_data.attribute_array.Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  }

  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, false);
  {
    RendererPtr renderer(new Renderer(gm_));

    // BufferObject.
    Reset();
    renderer->CreateOrUpdateResource(s_data.vertex_buffer.Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

    // ShaderProgram.
    Reset();
    renderer->CreateOrUpdateResource(s_data.shader.Get());
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf("CreateShader(GL_VERTEX_SHADER"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf("CreateShader(GL_FRAGMENT_SHADER"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("ShaderSource"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("AttachShader"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
    EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
    EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));

    // Texture.
    Reset();
    renderer->CreateOrUpdateResource(s_data.texture.Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(13U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
    EXPECT_EQ(
        1U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf(
                  "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                  "GL_UNSIGNED_BYTE"));
    // Cubemap.
    Reset();
    renderer->CreateOrUpdateResource(s_data.cubemap.Get());
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(13U,
              trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(3U,
              trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(
        6U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                      "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, "
                      "32, 32, 0, GL_RGBA, "
                      "GL_UNSIGNED_BYTE"));
  }
  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, true);

  {
    // Shape (the index buffer and the Shape's attribute array).
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->CreateOrUpdateShapeResources(s_data.shape);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  }

  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, false);
  {
    // Create an entire scene at once, which has all of the above except a
    // FramebufferObject.
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    renderer->CreateOrUpdateResources(root);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(
        1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(13U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
    EXPECT_EQ(
        7U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf(
                  "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                  "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  }
  {
    // Try the same thing, but this time with the Textures in a UniformBlock.
    RendererPtr renderer(new Renderer(gm_));
    s_data.rect->ClearUniforms();
    UniformBlockPtr block(new UniformBlock);
    s_data.rect->AddUniformBlock(block);
    const ShaderInputRegistryPtr& reg =
        s_data.rect->GetShaderProgram()->GetRegistry();
    block->AddUniform(reg->Create<Uniform>("uTexture", s_data.texture));
    block->AddUniform(reg->Create<Uniform>("uTexture2", s_data.texture));
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uCubeMapTexture", s_data.cubemap));
    s_data.rect->AddUniform(reg->Create<Uniform>(
        "uModelviewMatrix",
        math::TranslationMatrix(math::Vector3f(-1.5f, 1.5f, 0.0f))));
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uProjectionMatrix", math::Matrix4f::Identity()));

    Reset();
    renderer->CreateOrUpdateResources(root);

    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(
        1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
    EXPECT_EQ(13U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
    EXPECT_EQ(
        7U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(1U,
              trace_verifier_->GetCountOf(
                  "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                  "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  }
  {
    // One more time but with the UniformBlocks disabled; the textures shouldn't
    // be sent (though the cubemaps will be).
    RendererPtr renderer(new Renderer(gm_));
    s_data.rect->GetUniformBlocks()[0]->Enable(false);
    Reset();
    renderer->CreateOrUpdateResources(root);

    EXPECT_EQ(2U, trace_verifier_->GetCountOf("GenBuffers(1"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
    EXPECT_EQ(
        6U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
    EXPECT_EQ(0U,
              trace_verifier_->GetCountOf(
                  "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                  "GL_UNSIGNED_BYTE"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER, 12"));
  }
  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, true);

  {
    // Check that we never create resources for disabled Nodes.
    RendererPtr renderer(new Renderer(gm_));
    Reset();
    root->Enable(false);
    renderer->CreateOrUpdateResources(root);
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
  }
}

TEST_F(RendererTest, BindResource) {
  NodePtr root = BuildGraph(800, 800);

  // Test out the individual resource creation functions.
  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, false);
  RendererPtr renderer(new Renderer(gm_));

  // BufferObject.
  Reset();
  renderer->BindResource(s_data.vertex_buffer.Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenBuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // FramebufferObject.
  Reset();
  FramebufferObjectPtr fbo(new FramebufferObject(s_data.image->GetWidth(),
                                                 s_data.image->GetHeight()));
  TexturePtr texture(new Texture);
  texture->SetImage(0U, s_data.image);
  texture->SetSampler(s_data.sampler);
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(texture));
  fbo->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  renderer->BindResource(fbo.Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenFramebuffers(1"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindFramebuffer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenRenderbuffer"));
  // The unbound stencil attachment will be set to 0 explicitly.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("FramebufferRenderbuffer"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("FramebufferTexture2D"));
  // The texture has to be created to bind it as an attachment.
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures"));

  // ShaderProgram.
  Reset();
  renderer->BindResource(s_data.shader.Get());
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("CreateShader(GL_VERTEX_SHADER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("CreateShader(GL_FRAGMENT_SHADER"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("ShaderSource"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("AttachShader"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("BindAttribLocation"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("GetActiveAttrib"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetActiveUniform"));
  EXPECT_EQ(5U, trace_verifier_->GetCountOf("GetUniformLocation"));
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("LinkProgram"));

  // Texture.
  Reset();
  renderer->BindResource(s_data.texture.Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(13U, trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_2D"));
  EXPECT_EQ(3U, trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_2D"));
  EXPECT_EQ(
      1U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf(
                "TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, "
                "GL_UNSIGNED_BYTE"));
  // Cubemap.
  Reset();
  renderer->BindResource(s_data.cubemap.Get());
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("GenTextures(1, "));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(13U,
            trace_verifier_->GetCountOf("TexParameteri(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(3U,
            trace_verifier_->GetCountOf("TexParameterf(GL_TEXTURE_CUBE_MAP"));
  EXPECT_EQ(
      6U, trace_verifier_->GetCountOf("PixelStorei(GL_UNPACK_ALIGNMENT, 1)"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf(
                    "TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, "
                    "32, 32, 0, GL_RGBA, "
                    "GL_UNSIGNED_BYTE"));
}

TEST_F(RendererTest, GetResourceGlId) {
  BuildGraph(800, 800);
  RendererPtr renderer(new Renderer(gm_));
  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, false);
  EXPECT_EQ(1U, renderer->GetResourceGlId(s_data.vertex_buffer.Get()));
  EXPECT_EQ(1U, renderer->GetResourceGlId(s_data.shader.Get()));
  EXPECT_EQ(1U, renderer->GetResourceGlId(s_data.texture.Get()));
  EXPECT_EQ(2U, renderer->GetResourceGlId(s_data.cubemap.Get()));
  gm_->EnableFunctionGroup(GraphicsManager::kSamplerObjects, true);
  EXPECT_EQ(1U, renderer->GetResourceGlId(s_data.sampler.Get()));
}

TEST_F(RendererTest, ReleaseResources) {
  NodePtr root = BuildGraph(800, 800);

  RendererPtr renderer(new Renderer(gm_));

  const size_t initial_usage =
      renderer->GetGpuMemoryUsage(ion::gfx::Renderer::kTexture);

  // Verify at least Texture memory reduces after a ReleaseResources() call.
  Reset();
  renderer->DrawScene(s_data.rect);

  const size_t uploaded_usage =
      renderer->GetGpuMemoryUsage(ion::gfx::Renderer::kTexture);

  Reset();
  // Force calls to OnDestroyed().
  s_data.attribute_array = NULL;
  s_data.vertex_buffer = NULL;
  s_data.index_buffer = NULL;
  s_data.shader = NULL;
  s_data.shape = NULL;
  s_data.texture = NULL;
  s_data.cubemap = NULL;
  s_data.rect = NULL;
  root->ClearChildren();
  root->ClearUniforms();
  root->SetShaderProgram(ShaderProgramPtr(NULL));
  root.Reset(NULL);

  const size_t post_mark_usage =
      renderer->GetGpuMemoryUsage(ion::gfx::Renderer::kTexture);

  // In fact the texture's final ref doesn't go away until in the
  // ReleaseResources - the ShaderProgram has a final ref on it.
  renderer->ReleaseResources();

  const size_t post_release_usage =
      renderer->GetGpuMemoryUsage(ion::gfx::Renderer::kTexture);

  EXPECT_EQ(uploaded_usage, post_mark_usage);
  EXPECT_LT(post_release_usage, uploaded_usage);
  EXPECT_EQ(initial_usage, post_release_usage);
}

TEST_F(RendererTest, ClearCachedBindings) {
  NodePtr root = BuildGraph(800, 800);

  {
    // AttributeArray (just binds attribute buffers).
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    // Updating the array will trigger any buffers it references.
    renderer->RequestForcedUpdate(s_data.attribute_array.Get());
    Reset();
    renderer->DrawScene(s_data.rect);
    // The vertex array state will be refreshed, since CreateOrUpdateResources
    // sets the modified bit.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttribPointer"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(s_data.rect);
    // This time the VAO state will not be refreshed, since no resources
    // on which it depends were modified.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("VertexAttribPointer"));
  }

  {
    // BufferObject.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    renderer->RequestForcedUpdate(s_data.vertex_buffer.Get());
    Reset();
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  }

  {
    // ShaderProgram.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    renderer->RequestForcedUpdate(s_data.shader.Get());
    Reset();
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("UseProgram"));
  }

  {
    // Texture.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    renderer->RequestForcedUpdate(s_data.texture.Get());
    Reset();
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(s_data.rect);
    // The texture is bound twice, once when created, and again when bound to a
    // uniform.
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  }

  {
    // Shape (the index buffer and the Shape's attribute array's buffers).
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    renderer->RequestForcedShapeUpdates(s_data.shape);
    Reset();
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));

    renderer->ClearCachedBindings();
    Reset();
    renderer->DrawScene(s_data.rect);
    // Only the element buffer should be rebound, as part of the workaround
    // for broken drivers that don't save element buffer binding in the VAO.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindVertexArray"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  }
}

TEST_F(RendererTest, ForcedUpdateCausesCacheClear) {
  NodePtr root = BuildGraph(800, 800);

  {
    // AttributeArray (just binds attribute buffers).
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    Reset();
    renderer->RequestForcedUpdate(s_data.attribute_array.Get());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    Reset();
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  }

  {
    // BufferObject.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    Reset();
    renderer->RequestForcedUpdate(s_data.vertex_buffer.Get());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  }

  {
    // ShaderProgram.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    Reset();
    renderer->RequestForcedUpdate(s_data.shader.Get());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("UseProgram"));
  }

  {
    // Texture.
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    Reset();
    renderer->RequestForcedUpdate(s_data.texture.Get());
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  }

  {
    // Shape (the index buffer and the Shape's attribute array's buffers).
    RendererPtr renderer(new Renderer(gm_));
    // This will create all resources.
    renderer->DrawScene(s_data.rect);
    Reset();
    renderer->RequestForcedShapeUpdates(s_data.shape);
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    Reset();
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  }

  {
    // Entire scene.
    RendererPtr renderer(new Renderer(gm_));
    renderer->DrawScene(s_data.rect);
    Reset();
    renderer->RequestForcedUpdates(s_data.rect);
    EXPECT_EQ(0U, trace_verifier_->GetCallCount());
    renderer->DrawScene(s_data.rect);
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("UseProgram"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
    EXPECT_EQ(
        1U,
        trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  }
}

TEST_F(RendererTest, DebugLabels) {
  NodePtr root = BuildGraph(800, 800);

  RendererPtr renderer(new Renderer(gm_));
  renderer->DrawScene(s_data.rect);

  Reset();
  s_data.attribute_array->SetLabel("label");
  renderer->DrawScene(s_data.rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_VERTEX_ARRAY_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  s_data.vertex_buffer->SetLabel("label");
  renderer->DrawScene(s_data.rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_BUFFER_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  s_data.shader->SetLabel("label");
  renderer->DrawScene(s_data.rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_PROGRAM_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  s_data.shader->GetVertexShader()->SetLabel("label");
  renderer->DrawScene(s_data.rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_SHADER_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  s_data.shader->GetFragmentShader()->SetLabel("label");
  renderer->DrawScene(s_data.rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_SHADER_OBJECT")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  Reset();
  s_data.texture->SetLabel("label");
  renderer->DrawScene(s_data.rect);
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_TEXTURE")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));

  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  fbo->SetLabel("label");

  Reset();
  renderer->BindFramebuffer(fbo);
  renderer->DrawScene(s_data.rect);
  renderer->BindFramebuffer(FramebufferObjectPtr());
  EXPECT_TRUE(trace_verifier_->VerifyCallAt(
      trace_verifier_->GetNthIndexOf(0U, "LabelObject"))
          .HasArg(1, "GL_FRAMEBUFFER")
          .HasArg(3, "5")
          .HasArg(4, "\"label\""));
}

TEST_F(RendererTest, DebugMarkers) {
  NodePtr root = BuildGraph(800, 800);

  RendererPtr renderer(new Renderer(gm_));
  Reset();
  renderer->DrawScene(root);

  const std::vector<std::string> calls =
      base::SplitString(trace_verifier_->GetTraceString(), "\n");
  // Check that certain functions are grouped.
  const std::string plane_shader =
      "Plane shader [" + base::ValueToString(s_data.shader.Get()) + "]";
  const std::string plane_vertex_shader =
      "Plane shader vertex shader [" +
      base::ValueToString(s_data.shader->GetVertexShader().Get()) + "]";
  const std::string texture_address = base::ValueToString(s_data.texture.Get());
  const std::string cubemap_address = base::ValueToString(s_data.cubemap.Get());
  const std::string texture_length =
      base::ValueToString(texture_address.length() + 10U);
  const std::string cubemap_length =
      base::ValueToString(cubemap_address.length() + 18U);
  std::string texture_markers =
      "-->Texture [" + texture_address + "]:\n"
      "-->Texture [" + texture_address + "]:\n"
      "-->Cubemap Texture [" + cubemap_address + "]:\n";

  EXPECT_EQ(">" + plane_shader + ":", calls[7]);
  EXPECT_EQ("-->" + plane_vertex_shader + ":", calls[8]);
  EXPECT_EQ("    CreateShader(type = GL_VERTEX_SHADER)", calls[9]);

  std::string modelview_markers;
  {
    Reset();
    // There should be no ill effects from popping early.
    renderer->PopDebugMarker();
    renderer->PopDebugMarker();
    renderer->PopDebugMarker();
    renderer->DrawScene(root);
    // uModelviewMatrix uses a temporary Uniform when combining so we need to
    // extract the string from the trace to get proper addresses.
    const std::string actual = trace_verifier_->GetTraceString();
    size_t start =
        actual.find_first_of('\n', actual.find_first_of('\n') + 1) + 1;
    size_t end =
        actual.find_first_of('\n', actual.find_first_of('\n', start) + 1);
    modelview_markers = actual.substr(start, end - start + 1);
    // Check for a pop.
    const std::string expected(
        "Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        ">" + plane_shader + ":\n" +
        modelview_markers +
        texture_markers +
        "DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(expected, actual));
  }

  // Test a marker wrapping a draw.
  {
    Reset();
    renderer->PushDebugMarker("Marker");
    renderer->PopDebugMarker();
    renderer->DrawScene(root);
    const std::string expected(
        ">Marker:\n"
        "Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        ">" + plane_shader + ":\n" +
        modelview_markers +
        texture_markers +
        "DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected, trace_verifier_->GetTraceString()));
  }

  texture_markers = base::ReplaceString(texture_markers, "    ", "      ");
  texture_markers = base::ReplaceString(texture_markers, "-->", "---->");
  modelview_markers = base::ReplaceString(modelview_markers, "    ", "      ");
  modelview_markers = base::ReplaceString(modelview_markers, "-->", "---->");
  {
    Reset();
    renderer->PushDebugMarker("My scene");
    renderer->DrawScene(root);
    renderer->PopDebugMarker();
    // Extra pops should have no ill effects.
    renderer->PopDebugMarker();
    renderer->PopDebugMarker();
    const std::string expected(
        ">My scene:\n"
        "  Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        "-->" + plane_shader + ":\n" +
        modelview_markers +
        texture_markers +
        "  DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected, trace_verifier_->GetTraceString()));
  }

  {
    Reset();
    renderer->PushDebugMarker("My scene");
    renderer->DrawScene(root);
    const std::string expected(
        ">My scene:\n"
        "  Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        "-->" + plane_shader + ":\n" +
        modelview_markers +
        texture_markers +
        "  DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected, trace_verifier_->GetTraceString()));

    Reset();
    renderer->DrawScene(root);
    // There should still be indentation since we never popped the old marker.
    const std::string expected2(
        "  Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        "-->" + plane_shader + ":\n" +
        modelview_markers +
        texture_markers +
        "  DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected2, trace_verifier_->GetTraceString()));

    texture_markers = base::ReplaceString(texture_markers, "    ", "      ");
    texture_markers = base::ReplaceString(texture_markers, "-->", "---->");
    modelview_markers =
        base::ReplaceString(modelview_markers, "    ", "      ");
    modelview_markers =
        base::ReplaceString(modelview_markers, "-->", "---->");
    Reset();
    renderer->PushDebugMarker("Marker 2");
    renderer->DrawScene(root);
    renderer->PopDebugMarker();
    renderer->PopDebugMarker();
    const std::string expected3(
        "-->Marker 2:\n"
        "    Clear(mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)\n"
        "---->" + plane_shader + ":\n" +
        modelview_markers +
        texture_markers +
        "    DrawElements(mode = GL_TRIANGLES, count = 6, type = "
        "GL_UNSIGNED_SHORT, indices = NULL)\n");
    EXPECT_TRUE(base::testing::MultiLineStringsEqual(
        expected3, trace_verifier_->GetTraceString()));
  }
}

TEST_F(RendererTest, MatrixAttributes) {
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));

  static const char* kVertexShaderString =
      "attribute mat2 aMat2;\n"
      "attribute mat3 aMat3;\n"
      "attribute mat4 aMat4;\n";

  static const char* kFragmentShaderString =
      "uniform vec3 uniform1;\n"
      "uniform vec3 uniform2;\n";

  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("aMat2", math::Matrix2f(1.f, 2.f,
                                                       3.f, 4.f)));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("aMat3",
                               math::Matrix3f(1.f, 2.f, 3.f,
                                              4.f, 5.f, 6.f,
                                              7.f, 8.f, 9.f)));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>("aMat4",
                               math::Matrix4f(1.f, 2.f, 3.f, 4.f,
                                              5.f, 6.f, 7.f, 8.f,
                                              9.f, 10.f, 11.f, 12.f,
                                              13.f, 14.f, 15.f, 16.f)));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    s_data.rect->AddUniform(
            reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    // Check that the columns of matrix attributes are sent individually.
    EXPECT_EQ(2U, trace_verifier_->GetCountOf("VertexAttrib2fv"));
    EXPECT_EQ(3U, trace_verifier_->GetCountOf("VertexAttrib3fv"));
    EXPECT_EQ(4U, trace_verifier_->GetCountOf("VertexAttrib4fv"));
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }

  // Try the matrices as buffer objects.
  {
    NodePtr root = BuildGraph(kWidth, kHeight);
    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();

    s_data.attribute_array = new AttributeArray;
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>(
            "aMat2", BufferObjectElement(
                s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
                    BufferObject::kFloatMatrixColumn2, 2, 0))));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>(
            "aMat3", BufferObjectElement(
                s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
                    BufferObject::kFloatMatrixColumn3, 3, 16))));
    s_data.attribute_array->AddAttribute(
        reg->Create<Attribute>(
            "aMat4", BufferObjectElement(
                s_data.vertex_buffer, s_data.vertex_buffer->AddSpec(
                    BufferObject::kFloatMatrixColumn4, 4, 48))));
    reg->Create<Uniform>("uniform1", math::Vector3f(1.f, 2.f, 3.f));
    reg->Create<Uniform>("uniform2", math::Vector3f(1.f, 2.f, 3.f));
    s_data.shader = ShaderProgram::BuildFromStrings(
        "Shader", reg, kVertexShaderString, kFragmentShaderString,
        base::AllocatorPtr());
    s_data.shape->SetAttributeArray(s_data.attribute_array);
    s_data.rect->SetShaderProgram(s_data.shader);
    s_data.rect->ClearUniforms();
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform1", math::Vector3f::Zero()));
    s_data.rect->AddUniform(
        reg->Create<Uniform>("uniform2", math::Vector3f::Zero()));
    Reset();
    renderer->DrawScene(root);
    // Each column must be enabled separately.
    for (int i = 0; i < 9; ++i)
      EXPECT_EQ(1U,
                trace_verifier_->GetCountOf("EnableVertexAttribArray(0x" +
                                            base::ValueToString(i) + ")"));
    // Check that the each column of the matrix attributes were sent.
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x0, 2"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x1, 2"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x2, 3"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x3, 3"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x4, 3"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x5, 4"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x6, 4"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x7, 4"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("VertexAttribPointer(0x8, 4"));
    EXPECT_FALSE(log_checker.HasAnyMessages());
  }
}

TEST_F(RendererTest, BackgroundUpload) {
  MockVisual visual_background(*visual_);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  // Ideally, we could have a single texture unit, but the implementation
  // doesn't allow it, so we work around it below.
  gm->SetMaxTextureImageUnits(2);

  RendererPtr renderer(new Renderer(gm));

  // Create one Image to use for all Textures that we create.
  static const uint32 kImageWidth = 16;
  static const uint32 kImageHeight = 16;
  ImagePtr image(new Image);
  base::AllocatorPtr alloc =
      base::AllocationManager::GetDefaultAllocatorForLifetime(
          base::kShortTerm);
  base::DataContainerPtr data = base::DataContainer::CreateOverAllocated<uint8>(
      kImageWidth * kImageHeight, NULL, alloc);
  image->Set(Image::kLuminance, kImageWidth, kImageHeight, data);

  // Create textures.
  TexturePtr texture1(new Texture);
  TexturePtr texture2(new Texture);
  TexturePtr texture1_for_unit_1(new Texture);
  TexturePtr texture2_for_unit_1(new Texture);
  texture1->SetImage(0U, image);
  texture2->SetImage(0U, image);
  texture1_for_unit_1->SetImage(0U, image);
  texture2_for_unit_1->SetImage(0U, image);
  SamplerPtr sampler(new Sampler());
  texture1->SetSampler(sampler);
  texture2->SetSampler(sampler);
  texture1_for_unit_1->SetSampler(sampler);
  texture2_for_unit_1->SetSampler(sampler);

  // Create resources for the textures on the background Visual.
  portgfx::Visual::MakeCurrent(&visual_background);
  // Ping-pong the textures so that texture1 and texture2 both use image unit 0.
  renderer->CreateOrUpdateResource(texture1.Get());
  renderer->CreateOrUpdateResource(texture1_for_unit_1.Get());
  renderer->CreateOrUpdateResource(texture2.Get());
  renderer->CreateOrUpdateResource(texture2_for_unit_1.Get());

  // Rebind texture2 on the main thread, so that it is associated with the main
  // Visual's ResourceBinder. It should be unbound from the background Visual's
  // ResourceBinder.
  portgfx::Visual::MakeCurrent(visual_.get());
  renderer->CreateOrUpdateResource(texture2.Get());
  // Destroy texture2, calling OnDestroyed() in it's resource.
  texture2.Reset(NULL);
  // This will trigger the actual release.
  renderer->DrawScene(NodePtr());

  // Go back to the other visual and bind texture1 there, which will replace
  // the resource at image unit 0.
  portgfx::Visual::MakeCurrent(&visual_background);
  renderer->CreateOrUpdateResource(texture1.Get());

  // Set back the original Visual.
  portgfx::Visual::MakeCurrent(visual_.get());
}

// The following multithreaded tests cannot run on asmjs, where there are no
// threads.
#if !defined(ION_PLATFORM_ASMJS)
TEST_F(RendererTest, MultiThreadedDataLoading) {
  // Test that resources can be uploaded on a separate thread using a share
  // context via a MockVisual. Note that MockVisuals always set themselves
  // current, and using the copy constructor causes them to share mock GL state.
  MockVisual share_visual(*visual_);

  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight);

  // AttributeArray (just binds attribute buffers).
  {
    // Updating the array will trigger any buffers it references.
    port::ThreadStdFunc func =
        std::bind(UploadThread<AttributeArray>, renderer, &share_visual,
                  s_data.attribute_array.Get());
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  Reset();
  renderer->DrawScene(s_data.rect);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // BufferObject.
  renderer->ClearAllResources();
  Reset();
  {
    port::ThreadStdFunc func =
        std::bind(UploadThread<BufferObject>, renderer, &share_visual,
                  s_data.vertex_buffer.Get());
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  Reset();
  renderer->DrawScene(s_data.rect);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));

  // ShaderProgram.
  renderer->ClearAllResources();
  Reset();
  {
    port::ThreadStdFunc func =
        std::bind(UploadThread<ShaderProgram>, renderer, &share_visual,
                  s_data.shader.Get());
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("CreateProgram"));
  Reset();
  renderer->DrawScene(s_data.rect);
  // Since the program is not marked as concurrent, it should only be created
  // once and shared between threads.
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("CreateProgram"));

  // Texture.
  renderer->ClearAllResources();
  Reset();
  {
    port::ThreadStdFunc func =
        std::bind(UploadThread<Texture>, renderer, &share_visual,
                  s_data.texture.Get());
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_2D"));
  Reset();
  renderer->DrawScene(s_data.rect);
  // The texture gets bound twice, once for the resource change, and again for
  // the uniform binding.
  EXPECT_EQ(2U, trace_verifier_->GetCountOf("BindTexture(GL_TEXTURE_2D"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("TexImage2D(GL_TEXTURE_2D"));

  // Shape (the index buffer and the Shape's attribute array's buffers).
  renderer->ClearAllResources();
  Reset();
  {
    port::ThreadStdFunc func =
        std::bind(UploadThread<ShapePtr>, renderer, &share_visual,
                  &s_data.shape);
    base::ThreadSpawner spawner("worker", func);
  }
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BufferData(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BufferData(GL_ELEMENT_ARRAY_BUFFER"));
  Reset();
  renderer->DrawScene(s_data.rect);
  EXPECT_EQ(1U, trace_verifier_->GetCountOf("BindBuffer(GL_ARRAY_BUFFER"));
  EXPECT_EQ(1U,
            trace_verifier_->GetCountOf("BindBuffer(GL_ELEMENT_ARRAY_BUFFER"));
  EXPECT_EQ(0U, trace_verifier_->GetCountOf("BufferData"));
}
#endif

TEST_F(RendererTest, IndexBuffers32Bit) {
  base::LogChecker log_checker;
  RendererPtr renderer(new Renderer(gm_));
  NodePtr root = BuildGraph(kWidth, kHeight, true);
  Reset();

  // 32-bit vertex indices are not supported in OpenGL ES 2.0.
  gm_->SetVersionString("2.0 Ion OpenGL / ES");
  renderer->DrawScene(root);
  EXPECT_TRUE(log_checker.HasMessage(
      "ERROR", "The component type is not supported on this platform"));

  // 32-bit vertex indices are supported in OpenGL ES 3.0.
  gm_->SetVersionString("3.0 Ion OpenGL / ES");
  renderer->DrawScene(root);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(RendererTest, ResolveMultisampleFramebuffer) {
  RendererPtr renderer(new Renderer(gm_));

  int sample_size = 4;
  FramebufferObjectPtr ms_fbo(new FramebufferObject(128, 128));
  ms_fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment(Image::kRgba8888, sample_size));
  FramebufferObjectPtr dest_fbo(new FramebufferObject(128, 128));
  dest_fbo->SetColorAttachment(
      0U, FramebufferObject::Attachment(Image::kRgba8888));

  gm_->EnableFunctionGroup(GraphicsManager::kFramebufferBlit, true);
  {
    Reset();
    renderer->ResolveMultisampleFramebuffer(ms_fbo, dest_fbo);
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("BlitFramebuffer"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ResolveMultisampleFramebuffer"));
    // Verify the previous framebuffer (i.e., 0) is restored after the call.
    EXPECT_GE(1U, trace_verifier_->GetCountOf(
                      "BindFramebuffer(GL_FRAMEBUFFER, 0x0)"));
    EXPECT_EQ(
        0U, renderer->GetResourceGlId(renderer->GetCurrentFramebuffer().Get()));
  }

  gm_->EnableFunctionGroup(GraphicsManager::kFramebufferBlit, false);
  gm_->EnableFunctionGroup(GraphicsManager::kMultisampleFramebufferResolve,
                           true);
  {
    Reset();
    renderer->ResolveMultisampleFramebuffer(ms_fbo, dest_fbo);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BlitFramebuffer"));
    EXPECT_EQ(1U, trace_verifier_->GetCountOf("ResolveMultisampleFramebuffer"));
    // Verify the previous framebuffer (i.e., 0) is restored after the call.
    EXPECT_GE(1U, trace_verifier_->GetCountOf(
                      "BindFramebuffer(GL_FRAMEBUFFER, 0x0)"));
    EXPECT_EQ(
        0U, renderer->GetResourceGlId(renderer->GetCurrentFramebuffer().Get()));
  }

  gm_->EnableFunctionGroup(GraphicsManager::kFramebufferBlit, false);
  gm_->EnableFunctionGroup(GraphicsManager::kMultisampleFramebufferResolve,
                           false);
  {
    base::LogChecker log_checker;
    Reset();
    renderer->ResolveMultisampleFramebuffer(ms_fbo, dest_fbo);
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BindFramebuffer"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("BlitFramebuffer"));
    EXPECT_EQ(0U, trace_verifier_->GetCountOf("ResolveMultisampleFramebuffer"));
    EXPECT_TRUE(log_checker.HasMessage(
        "WARNING", "No multisampled frambuffer functions available."));
  }
}

}  // namespace gfx
}  // namespace ion

#endif  // ION_PRODUCTION
