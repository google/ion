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

#ifndef ION_GFX_TESTS_RENDERER_COMMON_H_
#define ION_GFX_TESTS_RENDERER_COMMON_H_

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
#include "ion/gfx/renderer.h"
#include "ion/gfx/resourcemanager.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
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
#include "ion/portgfx/glcontext.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

using math::Point2i;
using math::Range1i;
using math::Range1ui;
using math::Range2i;
using math::Vector2i;
using portgfx::GlContextPtr;
using testing::FakeGlContext;
using testing::FakeGraphicsManager;
using testing::FakeGraphicsManagerPtr;

struct SpecInfo {
  SpecInfo(size_t index_in, const std::string& type_in)
      : index(index_in), type(type_in) {}
  size_t index;
  std::string type;
};

struct Vertex {
  math::Vector3f point_coords;
  math::Vector2f tex_coords;
  bool operator==(const Vertex& other) const {
    return point_coords == other.point_coords && tex_coords == other.tex_coords;
  }
  bool operator!=(const Vertex& other) const { return !(*this == other); }
};

static const size_t kVboSize = 4U * sizeof(Vertex);

template <typename IndexType>
BufferObject::ComponentType GetComponentType();

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

static const char* kPlaneGeometryShaderString = R"glsl(#version 150 core
    layout(triangles) in;
    layout(triangle_strip, max_vertices=3) out;
    void main() {
      for(int i = 0; i < 3; i++) {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
      }
      EndPrimitive();
    })glsl";

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
    image_type = Image::kDense;
    image_dimensions = Image::k2d;
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

  void SetImageType(Image::Type type, Image::Dimensions dimensions) {
    image_type = type;
    image_dimensions = dimensions;
  }

  BufferObject::UsageMode vertex_buffer_usage;
  BufferObject::UsageMode index_buffer_usage;
  Shape::PrimitiveType primitive_type;
  Image::Format image_format;
  Image::Type image_type;
  Image::Dimensions image_dimensions;
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

static const int s_num_indices = 6;
static const int s_num_vertices = 4;
static const Image::Format kDepthFormat = Image::kRenderbufferDepth32f;

static void BuildRectangleBufferObject(Data* data, Options* options) {
  if (!data->vertex_buffer.Get()) data->vertex_buffer = new BufferObject;
  if (!data->vertex_container.Get()) {
    Vertex* vertices = new Vertex[s_num_vertices];
    static const float kHalfSize = 10.f;
    static const float kY = -0.1f;
    vertices[0].point_coords.Set(-kHalfSize, kY, kHalfSize);
    vertices[0].tex_coords.Set(0.f, 1.f);
    vertices[1].point_coords.Set(kHalfSize, kY, kHalfSize);
    vertices[1].tex_coords.Set(1.f, 1.f);
    vertices[2].point_coords.Set(kHalfSize, kY, -kHalfSize);
    vertices[2].tex_coords.Set(1.f, 0.f);
    vertices[3].point_coords.Set(-kHalfSize, kY, -kHalfSize);
    vertices[3].tex_coords.Set(0.f, 0.f);
    data->vertex_container =
        base::DataContainer::Create<Vertex>(
            vertices, base::DataContainer::ArrayDeleter<Vertex>, false,
            data->vertex_buffer->GetAllocator());
  }
  data->vertex_buffer->SetData(
      data->vertex_container, sizeof(Vertex), s_num_vertices,
      options->vertex_buffer_usage);
}

static void BuildNonIndexedRectangleBufferObject(Data* data, Options* options) {
  if (!data->vertex_buffer.Get()) data->vertex_buffer = new BufferObject;
  if (!data->vertex_container.Get()) {
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

    data->vertex_container = base::DataContainer::Create<Vertex>(
        vertices, base::DataContainer::ArrayDeleter<Vertex>, false,
        data->vertex_buffer->GetAllocator());
  }
  data->vertex_buffer->SetData(data->vertex_container, sizeof(Vertex),
                                s_num_indices, options->vertex_buffer_usage);
}

static void BuildRectangleAttributeArray(Data* data, Options* options) {
  // The attributes have names that are defined in the global registry and so
  // must be set there.
  const ShaderInputRegistryPtr& global_reg =
      ShaderInputRegistry::GetGlobalRegistry();

  data->attribute_array = new AttributeArray;
  data->attribute_array->AddAttribute(global_reg->Create<Attribute>(
      "aVertex", BufferObjectElement(
          data->vertex_buffer, data->vertex_buffer->AddSpec(
              BufferObject::kFloat, 3, 0))));
  data->attribute_array->AddAttribute(global_reg->Create<Attribute>(
      "aTexCoords", BufferObjectElement(
          data->vertex_buffer, data->vertex_buffer->AddSpec(
              BufferObject::kFloat, 2, sizeof(float) * 3))));

  data->shape->SetAttributeArray(data->attribute_array);
}

static void BuildShape(Data* data, Options* options) {
  if (!data->shape.Get()) data->shape = new Shape;
  data->shape->SetPrimitiveType(options->primitive_type);
}

template <typename IndexType>
static void BuildRectangleShape(Data* data, Options* options) {
  BuildShape(data, options);

  if (!data->index_buffer.Get())
    data->index_buffer = new IndexBuffer;
  if (!data->index_container.Get()) {
    // Set up the triangle vertex indices.
    IndexType* indices = new IndexType[s_num_indices];
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    indices[3] = 0;
    indices[4] = 2;
    indices[5] = 3;
    if (!data->index_container.Get())
      data->index_container = ion::base::DataContainer::Create<IndexType>(
          indices, ion::base::DataContainer::ArrayDeleter<IndexType>, false,
          data->index_buffer->GetAllocator());
  }

  data->index_buffer->SetData(
      data->index_container, sizeof(IndexType), s_num_indices,
      options->index_buffer_usage);
  data->index_buffer->AddSpec(GetComponentType<IndexType>(), 1, 0);

  data->shape->SetIndexBuffer(data->index_buffer);
}

static void AddDefaultUniformsToNode(const NodePtr& node) {
  node->AddUniform(ShaderInputRegistry::GetGlobalRegistry()->Create<Uniform>(
      "uModelviewMatrix", math::Matrix4f::Identity()));
  node->AddUniform(ShaderInputRegistry::GetGlobalRegistry()->Create<Uniform>(
      "uProjectionMatrix", math::Matrix4f::Identity()));
  node->AddUniform(ShaderInputRegistry::GetGlobalRegistry()->Create<Uniform>(
      "uBaseColor", math::Vector4f::Zero()));
}

static void AddPlaneShaderUniformsToNode(Data* data, const NodePtr& node) {
  const ShaderInputRegistryPtr& reg = node->GetShaderProgram()->GetRegistry();
  node->AddUniform(reg->Create<Uniform>("uTexture", data->texture));
  node->AddUniform(reg->Create<Uniform>("uTexture2", data->texture));
  node->AddUniform(reg->Create<Uniform>("uCubeMapTexture", data->cubemap));
  node->AddUniform(reg->Create<Uniform>("uModelviewMatrix",
      math::TranslationMatrix(math::Vector3f(-1.5f, 1.5f, 0.0f))));
  node->AddUniform(
      reg->Create<Uniform>("uProjectionMatrix", math::Matrix4f::Identity()));
}

static void SetImages(Data* data) {
  data->texture->SetImage(0U, ImagePtr());
  data->texture->SetImage(0U, data->image);
  for (int i = 0; i < 6; ++i) {
    data->cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                            ImagePtr());
    data->cubemap->SetImage(static_cast<CubeMapTexture::CubeFace>(i), 0U,
                            data->image);
  }
}

static void BuildImage(Data* data, Options* options) {
  // Data for EGL images.
  static uint8 kData[12] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6,
                            0x7, 0x8, 0x9, 0xa, 0xb, 0xc};
  if (!data->image.Get())
    data->image = new Image;
  if (!data->image_container.Get()) {
    data->image_container = base::DataContainer::CreateOverAllocated<char>(
        65536, nullptr, data->image->GetAllocator());
  }
  switch (options->image_type) {
    case Image::kDense:
      if (options->image_dimensions == Image::k2d) {
        data->image->Set(options->image_format, 32, 32, data->image_container);
      } else {
        data->image->Set(options->image_format, 8, 8, 16,
                         data->image_container);
      }
      break;
    case Image::kArray:
      if (options->image_dimensions == Image::k2d) {
        data->image->SetArray(options->image_format, 32, 32,
                              data->image_container);
      } else {
        data->image->SetArray(options->image_format, 8, 8, 16,
                              data->image_container);
      }
      break;
    case Image::kEgl:
      ASSERT_EQ(Image::k2d, options->image_dimensions);
      data->image->SetEglImage(
          base::DataContainer::Create<void>(kData, kNullFunction, false,
                                            data->image->GetAllocator()));
      break;
    case Image::kExternalEgl:
      ASSERT_EQ(Image::k2d, options->image_dimensions);
      data->image->SetExternalEglImage(
          base::DataContainer::Create<void>(kData, kNullFunction, false,
                                            data->image->GetAllocator()));
      break;
  }
  SetImages(data);
}

static void BuildRectangle(Data* data, Options* options, bool is_indexed,
                           bool use_32bit_indices, const char* vertex_shader,
                           const char* geometry_shader,
                           const char* fragment_shader) {
  if (!data->texture.Get()) {
    data->texture = new Texture();
    data->texture->SetLabel("Texture");
  }
  if (!data->cubemap.Get()) {
    data->cubemap = new CubeMapTexture();
    data->cubemap->SetLabel("Cubemap Texture");
  }
  if (!data->sampler.Get()) {
    data->sampler = new Sampler();
    data->sampler->SetLabel("Sampler");
  }
  BuildImage(data, options);
  data->texture->SetBaseLevel(options->base_level);
  data->texture->SetMaxLevel(options->max_level);
  data->texture->SetSwizzleRed(options->swizzle_r);
  data->texture->SetSwizzleGreen(options->swizzle_g);
  data->texture->SetSwizzleBlue(options->swizzle_b);
  data->texture->SetSwizzleAlpha(options->swizzle_a);
  data->sampler->SetCompareFunction(options->compare_func);
  data->sampler->SetCompareMode(options->compare_mode);
  data->sampler->SetMaxAnisotropy(options->max_anisotropy);
  data->sampler->SetMinLod(options->min_lod);
  data->sampler->SetMaxLod(options->max_lod);
  data->sampler->SetMinFilter(options->min_filter);
  data->sampler->SetMagFilter(options->mag_filter);
  data->sampler->SetWrapR(options->wrap_r);
  data->sampler->SetWrapS(options->wrap_s);
  data->sampler->SetWrapT(options->wrap_t);
  data->cubemap->SetSampler(data->sampler);
  data->texture->SetSampler(data->sampler);
  if (!data->rect.Get()) {
    data->rect = new Node;

    ShaderInputRegistryPtr reg(new ShaderInputRegistry);
    reg->IncludeGlobalRegistry();
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture", kTextureUniform, "Plane texture"));
    reg->Add(ShaderInputRegistry::UniformSpec(
        "uTexture2", kTextureUniform, "Plane texture2"));
    reg->Add(ShaderInputRegistry::AttributeSpec(
        "aTestAttrib", kBufferObjectElementAttribute, "Testing attribute"));

    if (is_indexed) {
      BuildRectangleBufferObject(data, options);
      if (use_32bit_indices) {
        BuildRectangleShape<uint32>(data, options);
      } else {
        BuildRectangleShape<uint16>(data, options);
      }
    } else {
      BuildNonIndexedRectangleBufferObject(data, options);
      BuildShape(data, options);
    }
    BuildRectangleAttributeArray(data, options);
    data->rect->AddShape(data->shape);
    data->shader = ShaderProgram::BuildFromStrings(
        "Plane shader", reg, vertex_shader, geometry_shader, fragment_shader,
        base::AllocatorPtr());
    data->rect->SetShaderProgram(data->shader);
    AddPlaneShaderUniformsToNode(data, data->rect);

    StateTablePtr state_table(new StateTable());
    state_table->Enable(StateTable::kCullFace, false);
    data->rect->SetStateTable(state_table);
  }
}

static void BuildRectangle(Data* data, Options* options, bool is_indexed,
                           bool use_32bit_indices) {
  BuildRectangle(data, options, is_indexed, use_32bit_indices,
                 kPlaneVertexShaderString, kPlaneGeometryShaderString,
                 kPlaneFragmentShaderString);
}

static void BuildRectangle(Data* data, Options* options) {
  BuildRectangle(data, options, true, false);
}

// Returns an array Uniform of the passed name created using the passed registry
// and values to initialize it.
template <typename T>
static Uniform CreateArrayUniform(const ShaderInputRegistryPtr& reg,
                                  const std::string& name,
                                  const std::vector<T>& values) {
  return reg->CreateArrayUniform(name, &values[0], values.size(),
                                 base::AllocatorPtr());
}

static const NodePtr BuildGraph(Data* data, Options* options, int width,
                                int height, bool is_indexed,
                                bool use_32bit_indices,
                                const char* vertex_shader,
                                const char* geometry_shader,
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

  BuildRectangle(data, options, is_indexed, use_32bit_indices,
                 vertex_shader, geometry_shader, fragment_shader);
  root->AddChild(data->rect);
  root->SetShaderProgram(data->shader);
  root->AddUniform(ShaderInputRegistry::GetGlobalRegistry()->Create<Uniform>(
      "uModelviewMatrix", math::Matrix4f::Identity()));
  return root;
}

static const NodePtr BuildGraph(Data* data, Options* options, int width,
                                int height, bool is_indexed,
                                bool use_32bit_indices) {
  return BuildGraph(data, options, width, height, is_indexed, use_32bit_indices,
                    kPlaneVertexShaderString, kPlaneGeometryShaderString,
                    kPlaneFragmentShaderString);
}

static const NodePtr BuildGraph(Data* data, Options* options, int width,
                                int height, bool use_32bit_indices) {
  return BuildGraph(data, options, width, height, true, use_32bit_indices);
}

static const NodePtr BuildGraph(Data* data, Options* options, int width,
                                int height) {
  return BuildGraph(data, options, width, height, false);
}

static void DestroyGraph(Data* data, NodePtr* root) {
  data->attribute_array = nullptr;
  data->vertex_buffer = nullptr;
  data->index_buffer = nullptr;
  data->shader = nullptr;
  data->shape = nullptr;
  data->texture = nullptr;
  data->cubemap = nullptr;
  data->rect = nullptr;
  (*root)->ClearChildren();
  (*root)->ClearUniforms();
  (*root)->SetShaderProgram(ShaderProgramPtr(nullptr));
  root->Reset(nullptr);
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
  std::function<void()> update_func;
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
    const RendererPtr& renderer, const NodePtr& root,
    GraphicsManager::FeatureId feature = GraphicsManager::kCore) {
  TracingHelper helper;
  // Stop one past the end of the enum for error-case handling.
  size_t num_tests = data.arg_tests.size();
  testing::TraceVerifier::ArgSpec arg_spec;
  arg_spec.push_back(std::make_pair(0, data.call_name));
  for (const auto& static_arg : data.static_args) {
    arg_spec.push_back(std::make_pair(static_cast<int>(static_arg.index),
                                      static_arg.string_value));
  }
  for (size_t i = 0; i < num_tests; ++i) {
    // Update the value.
    *data.option = data.arg_tests[i].value;
    // Update the scene graph with the value.
    data.update_func();
    // Reset call counts and the trace stream.
    FakeGraphicsManager::ResetCallCount();
    trace_verifier->Reset();
    // Draw the scene.
    renderer->DrawScene(root);
    if (data.debug)
      std::cerr << trace_verifier->GetTraceString() << "\n";
    // Check the static args are correct.
    testing::TraceVerifier::Call call = trace_verifier->VerifyCallAt(
        trace_verifier->GetNthIndexOf(data.arg_tests[i].index, arg_spec));
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
  if (feature == GraphicsManager::kCore) {
    data.update_func();
  } else {
    renderer->GetGraphicsManager()->EnableFeature(feature, false);
    data.update_func();
    FakeGraphicsManager::ResetCallCount();
    trace_verifier->Reset();
    renderer->DrawScene(root);
    if (trace_verifier->GetCountOf(arg_spec) != 0) {
      return ::testing::AssertionFailure()
             << "Unexpected call to " << data.call_name;
    }
    renderer->GetGraphicsManager()->EnableFeature(feature, true);
  }
  return ::testing::AssertionSuccess();
}

// Checks that Renderer catches failed certain cases. We draw to a framebuffer
// to also catch framebuffer and renderbuffer errors.
static ::testing::AssertionResult VerifyFunctionFailure(
    Data* data, Options* options, const FakeGraphicsManagerPtr& gm,
    const std::string& func_name, const std::string& error_msg) {
  base::LogChecker log_checker;
  gm->SetForceFunctionFailure(func_name, true);
  {
    FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
    fbo->SetColorAttachment(0U,
                            FramebufferObject::Attachment(Image::kRgba4Byte));
    fbo->SetColorAttachment(2U,
                            FramebufferObject::Attachment(Image::kRgba4Byte));
    fbo->SetDrawBuffer(0U, 0);
    fbo->SetDrawBuffer(1U, 2);

    NodePtr root = BuildGraph(data, options, 800, 800);
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

static ImagePtr CreateNullImage(uint32 width, uint32 height,
                                Image::Format format) {
  ImagePtr image(new Image);
  image->Set(format, width, height, base::DataContainerPtr(nullptr));
  return image;
}

template <typename TextureType>
::testing::AssertionResult VerifyImmutableTexture(Data* data,
    const RendererPtr& renderer, testing::TraceVerifier* trace_verifier,
    size_t levels, const std::string& base_call) {
  base::SharedPtr<TextureType> texture(new TextureType);
  texture->SetImmutableImage(data->image, levels);
  texture->SetSampler(data->sampler);
  texture->SetSubImage(0U, math::Point2ui(0, 0), data->image);
  renderer->CreateOrUpdateResource(texture.Get());
  const std::string call = base_call + base::ValueToString(levels);
  if (trace_verifier->GetCountOf(call) != 1U)
    return ::testing::AssertionFailure() << "There should be one call to "
                                         << call;

  if (trace_verifier->GetCountOf("TexSubImage") != 1U)
    return ::testing::AssertionFailure()
           << "There should be one call to TexSubImage";
  if (trace_verifier->GetCountOf("TexImage"))
    return ::testing::AssertionFailure()
           << "There should be no calls to TexImage*";
  return ::testing::AssertionSuccess();
}

template <typename TextureType>
::testing::AssertionResult VerifyImmutableCubemapTexture(Data* data,
    const RendererPtr& renderer, testing::TraceVerifier* trace_verifier,
    size_t levels, const std::string& base_call) {
  base::SharedPtr<TextureType> texture(new TextureType);
  texture->SetImmutableImage(data->image, levels);
  texture->SetSampler(data->sampler);
  texture->SetSubImage(CubeMapTexture::kPositiveZ, 0U, math::Point2ui(0, 0),
                       data->image);
  renderer->CreateOrUpdateResource(texture.Get());
  const std::string call = base_call + base::ValueToString(levels);
  if (trace_verifier->GetCountOf(call) != 1U)
    return ::testing::AssertionFailure() << "There should be one call to "
                                         << call;

  if (trace_verifier->GetCountOf("TexSubImage") != 1U)
    return ::testing::AssertionFailure()
           << "There should be one call to TexSubImage";
  if (trace_verifier->GetCountOf("TexImage"))
    return ::testing::AssertionFailure()
           << "There should be no calls to TexImage*";
  return ::testing::AssertionSuccess();
}

template <typename TextureType>
::testing::AssertionResult VerifyProtectedTexture(Data* data,
    const RendererPtr& renderer, testing::TraceVerifier* trace_verifier,
    size_t levels, const std::string& base_call) {
  base::SharedPtr<TextureType> texture(new TextureType);
  texture->SetProtectedImage(data->image, levels);
  texture->SetSampler(data->sampler);
  renderer->CreateOrUpdateResource(texture.Get());
  const std::string call = base_call + base::ValueToString(levels);
  if (trace_verifier->GetCountOf(call) != 1U)
    return ::testing::AssertionFailure() << "There should be one call to "
                                         << call;
  size_t index = trace_verifier->GetNthIndexOf(0, "TexParameteri");
  ::testing::AssertionResult result =
      trace_verifier->VerifyCallAt(index).HasArg(2,
                                                 "GL_TEXTURE_PROTECTED_EXT");
  if (result != ::testing::AssertionSuccess())
    return result;
  if (trace_verifier->GetCountOf("TexImage"))
    return ::testing::AssertionFailure()
           << "There should be no calls to TexImage*";
  return ::testing::AssertionSuccess();
}

template <typename TextureType>
::testing::AssertionResult VerifyImmutableMultisampledTexture(Data* data,
    const RendererPtr& renderer, testing::TraceVerifier* trace_verifier,
    int samples, const std::string& base_call) {
  base::SharedPtr<TextureType> texture(new TextureType);
  texture->SetImmutableImage(data->image, 1);
  if (samples > 0) {
    texture->SetMultisampling(samples, true);
  }
  texture->SetSampler(data->sampler);
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
    Data* data, Options* options, const GraphicsManagerPtr& gm,
    const RendererPtr& renderer, testing::TraceVerifier* trace_verifier,
    Renderer::Flag save_flag, Renderer::Flag restore_flag,
    const std::string& save_call, const std::string& restore_call) {
  NodePtr root = BuildGraph(data, options, 800, 800);

  // Framebuffer handling is special, we want to test without the requested fbo
  // first, and bind it again before restoring.
  FramebufferObjectPtr fbo = renderer->GetCurrentFramebuffer();
  if (fbo.Get() != nullptr)
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
  if (fbo.Get() != nullptr)
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
    Data* data, Options* options,
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

  NodePtr root = BuildGraph(data, options, 800, 800);
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
                         const GlContextPtr& gl_context, T* holder) {
  // Set the GL context for this thread.
  portgfx::GlContext::MakeCurrent(gl_context);
  renderer->CreateOrUpdateResource(holder);
  return true;
}
template <>
bool UploadThread<ShapePtr>(const RendererPtr& renderer,
                            const GlContextPtr& gl_context,
                            ShapePtr* shapeptr) {
  // Set the GL context for this thread.
  portgfx::GlContext::MakeCurrent(gl_context);
  renderer->CreateOrUpdateShapeResources(*shapeptr);
  return true;
}

static bool RenderingThread(const RendererPtr& renderer,
                            const GlContextPtr& gl_context, NodePtr* nodeptr) {
  portgfx::GlContext::MakeCurrent(gl_context);
  renderer->DrawScene(*nodeptr);
  return true;
}

static bool UniformThread(const RendererPtr& renderer,
                          const GlContextPtr& gl_context, const NodePtr& node,
                          size_t uindex, float uvalue,
                          std::vector<ResourceManager::ProgramInfo>* infos) {
  portgfx::GlContext::MakeCurrent(gl_context);
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

  const FakeGraphicsManagerPtr& GetGraphicsManager() const { return gm_; }
  testing::TraceVerifier* GetTraceVerifier() const { return trace_verifier_; }

  Data* GetData() { return data_; }
  Options* GetOptions() { return options_; }

 protected:
  void SetUp() override {
    data_ = new Data();
    options_ = new Options();
    // Use a special allocator to track certain kinds of memory overwrites. This
    // is quite useful since the Renderer has many internal pointers to state
    // holders, and it is a good idea to ensure we don't try to read from or
    // write to one that has already been destroyed.
    static const size_t kMemorySize = 8 * 1024 * 1024;
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

    gl_context_ = FakeGlContext::Create(kWidth, kHeight);
    portgfx::GlContext::MakeCurrent(gl_context_);
    gm_.Reset(new FakeGraphicsManager());
    gm_->EnableErrorChecking(false);
    trace_verifier_ = new testing::TraceVerifier(gm_.Get());
    Reset();
  }

  void TearDown() override {
    EXPECT_EQ(GLenum{GL_NO_ERROR}, gm_->GetError());
    delete trace_verifier_;
    delete data_;
    delete options_;
    EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), gm_->GetError());
    gm_.Reset(nullptr);
    gl_context_.Reset();
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
    FakeGraphicsManager::ResetCallCount();
    trace_verifier_->Reset();
    msg_stream_.str("");
    msg_stream_.clear();
  }

  const std::vector<std::string> GetReleaseStrings() {
    std::vector<std::string> call_strings;
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteBuffers");
    call_strings.push_back("DeleteProgram");
    if (gm_->IsFeatureAvailable(GraphicsManager::kSamplerObjects))
      call_strings.push_back("DeleteSampler");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteShader");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteTextures");
    call_strings.push_back("DeleteVertexArrays");
    return call_strings;
  }

  base::SharedPtr<FakeGlContext> gl_context_;
  FakeGraphicsManagerPtr gm_;
  testing::TraceVerifier* trace_verifier_;
  Data* data_;
  Options* options_;
  base::AllocatorPtr saved_[3];
  std::stringstream msg_stream_;

  static const int kWidth = 400;
  static const int kHeight = 300;
};

template <typename T>
static ::testing::AssertionResult VerifySamplerAndTextureTypedCalls(
    RendererTest* test, VerifyRenderData<T>* data,
    const std::string& texture_func, const std::string& sampler_func) {
  data->update_func =
      std::bind(static_cast<void (*)(Data*, Options*)>(BuildRectangle),
                test->GetData(), test->GetOptions());
  data->varying_arg_index = 3U;

  const FakeGraphicsManagerPtr& gm = test->GetGraphicsManager();
  testing::TraceVerifier* trace_verifier = test->GetTraceVerifier();

  gm->EnableFeature(GraphicsManager::kSamplerObjects, false);
  {
    NodePtr root = BuildGraph(test->GetData(), test->GetOptions(), 800, 800);
    RendererPtr renderer(new Renderer(gm));
    data->call_name = texture_func;
    // Assumption: we only get 2D or 3D dense textures here.
    if (test->GetOptions()->image_dimensions == Image::k3d) {
      data->static_args.push_back(StaticArg(1, "GL_TEXTURE_3D"));
    } else {
      data->static_args.push_back(StaticArg(1, "GL_TEXTURE_2D"));
    }
    ::testing::AssertionResult result =
        VerifyRenderCalls(*data, trace_verifier, renderer, root);
    if (result != ::testing::AssertionSuccess())
      return result << ". Failed while testing \"" << texture_func << "\"";
    FakeGraphicsManager::ResetCallCount();
    trace_verifier->Reset();
  }
  // Note that ::testing::AssertionResult has no assignment operator, so it must
  // be scoped.
  {
    ::testing::AssertionResult result = test->VerifyReleases(1);
    if (result != ::testing::AssertionSuccess())
      return result;
  }

  gm->EnableFeature(GraphicsManager::kSamplerObjects, true);
  {
    NodePtr root = BuildGraph(test->GetData(), test->GetOptions(), 800, 800);
    RendererPtr renderer(new Renderer(gm));
    data->call_name = sampler_func;
    // Instead of GL_TEXTURE_2D use the ID of the sampler.
    data->static_args[1] = StaticArg(1, "0x1");
    ::testing::AssertionResult result =
        VerifyRenderCalls(*data, trace_verifier, renderer, root);
    if (result != ::testing::AssertionSuccess())
      return result << ". Failed while testing \"" << sampler_func << "\"";
    FakeGraphicsManager::ResetCallCount();
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
static ::testing::AssertionResult VerifySamplerAndTextureCalls(
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

static ::testing::AssertionResult VerifyGpuMemoryUsage(
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

static ::testing::AssertionResult VerifyClearFlag(Data* data, Options* options,
                                                  const GraphicsManagerPtr& gm,
                                                  Renderer::Flag flag,
                                                  GLenum enum_name,
                                                  GLint expected_value) {
  NodePtr root = BuildGraph(data, options, 800, 800);
  RendererPtr renderer(new Renderer(gm));
  FramebufferObjectPtr fbo(new FramebufferObject(128, 128));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(Image::kRgba4Byte));
  renderer->BindFramebuffer(fbo);
  renderer->SetTextureImageUnitRange(Range1i(0, 31));  // Reset image units.

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
    Data* data, Options* options,
    const GraphicsManagerPtr& gm, Renderer::Flag flag, GLenum enum_name,
    GLint expected_value) {
  NodePtr root = BuildGraph(data, options, 800, 800);
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

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TESTS_RENDERER_COMMON_H_
