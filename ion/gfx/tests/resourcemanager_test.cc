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

#include "ion/gfx/resourcemanager.h"

#include "ion/base/datacontainer.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/shader.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfx/tests/testscene.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/transformfeedback.h"
#include "ion/gfx/uniform.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

namespace {

#define VERIFY_EQ(a, b)                                                     \
  if (a != b)                                                               \
    return ::testing::AssertionFailure() << "\n    Value of: " #b           \
                                            "\n      Actual: " << b << "\n" \
                                         << "    Expected: " #a             \
                                            "\n    Which is: " << a << "\n  ";
#define VERIFY_TRUE(a) VERIFY_EQ((a), true)

#define VERIFY(call)                                \
  {                                                 \
    ::testing::AssertionResult result = call;       \
    if (result != ::testing::AssertionSuccess())    \
      return result << "While testing " #call "\n"; \
  }

using math::Matrix2f;
using math::Matrix3f;
using math::Matrix4f;
using math::Vector2f;
using math::Vector2i;
using math::Vector2ui;
using math::Vector3f;
using math::Vector3i;
using math::Vector3ui;
using math::Vector4f;
using math::Vector4i;
using math::Vector4ui;
using testing::FakeGraphicsManager;
using testing::FakeGraphicsManagerPtr;
using testing::FakeGlContext;

//-----------------------------------------------------------------------------
//
// Callback helper.
//
//-----------------------------------------------------------------------------
template <typename T>
struct CallbackHelper {
  CallbackHelper() : was_called(false) {}
  ~CallbackHelper() {}
  // Just save the vector of resource infos.
  void Callback(const std::vector<T>& infos_in) {
    infos = infos_in;
    was_called = true;
  }
  void Reset() {
    was_called = false;
    infos.clear();
  }
    // The infos set in the callback.
  std::vector<T> infos;
  // Whether the callback has been called.
  bool was_called;
};

// Verifies that no infos are returned when querying for all available
// resources.
template <typename HolderType, typename InfoType>
static ::testing::AssertionResult VerifyNoInfos(const RendererPtr& renderer) {
  CallbackHelper<InfoType> callback;
  renderer->GetResourceManager()->RequestAllResourceInfos<HolderType, InfoType>(
      std::bind(&CallbackHelper<InfoType>::Callback, &callback,
                std::placeholders::_1));
  renderer->ProcessResourceInfoRequests();
  VERIFY_TRUE(callback.was_called);
  VERIFY_EQ(0U, callback.infos.size());
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// ArrayInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifyAttribute(
    const std::vector<ResourceManager::ArrayInfo::Attribute>& infos, int index,
    GLuint buffer, GLboolean enabled, GLuint size, GLuint stride, GLenum type,
    GLboolean normalized, GLvoid* pointer, const math::Vector4f& attr_value,
    GLuint divisor) {
  VERIFY_EQ(buffer, infos[index].buffer);
  VERIFY_EQ(enabled, infos[index].enabled);
  VERIFY_EQ(size, infos[index].size);
  VERIFY_EQ(stride, infos[index].stride);
  VERIFY_EQ(type, infos[index].type);
  VERIFY_EQ(normalized, infos[index].normalized);
  VERIFY_EQ(pointer, infos[index].pointer);
  VERIFY_EQ(attr_value, infos[index].value);
  VERIFY_EQ(divisor, infos[index].divisor);
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyArrayInfo(
    const ResourceManager::ArrayInfo& info, size_t vertex_count,
    size_t attrib_count) {
  VERIFY_EQ(attrib_count, info.attributes.size());
  VERIFY_EQ(vertex_count, info.vertex_count);
  VERIFY_EQ("Vertex array", info.label);
  // Buffer attributes.
  VERIFY(VerifyAttribute(info.attributes, 0, 2U, GL_TRUE, 1U,
                         sizeof(testing::TestScene::Vertex), GL_FLOAT, GL_FALSE,
                         nullptr, math::Vector4f(0.f, 0.f, 0.f, 1.f), 0));
  VERIFY(VerifyAttribute(info.attributes, 1, 2U, GL_TRUE, 2U,
                         sizeof(testing::TestScene::Vertex), GL_FLOAT, GL_TRUE,
                         reinterpret_cast<GLvoid*>(4),
                         math::Vector4f(0.f, 0.f, 0.f, 1.f), 0));
  // Non-buffer attributes.
  VERIFY(VerifyAttribute(info.attributes, 2, 0U, GL_TRUE, 1U, 0U, GL_FLOAT,
                         GL_FALSE, nullptr, math::Vector4f(1.f, 0.f, 0.f, 1.f),
                         0));
  VERIFY(VerifyAttribute(info.attributes, 3, 0U, GL_TRUE, 2U, 0U, GL_FLOAT,
                         GL_FALSE, nullptr, math::Vector4f(1.f, 2.f, 0.f, 1.f),
                         0));
  VERIFY(VerifyAttribute(info.attributes, 4, 0U, GL_TRUE, 3U, 0U, GL_FLOAT,
                         GL_FALSE, nullptr, math::Vector4f(1.f, 2.f, 3.f, 1.f),
                         0));
  VERIFY(VerifyAttribute(info.attributes, 5, 0U, GL_TRUE, 4U, 0U, GL_FLOAT,
                         GL_FALSE, nullptr, math::Vector4f(1.f, 2.f, 3.f, 4.f),
                         0));
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyDefaultArrayInfo(
    const ResourceManager::ArrayInfo& info, size_t attrib_count) {
  VERIFY_EQ(attrib_count, info.attributes.size());
  VERIFY_EQ(3U, info.vertex_count);
  VERIFY(VerifyAttribute(info.attributes, 0, 1U, GL_TRUE, 3U,
                         sizeof(testing::TestScene::Vertex), GL_FLOAT, GL_FALSE,
                         nullptr, math::Vector4f(0.f, 0.f, 0.f, 1.f), 0));
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// BufferInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifyBufferInfo(
    const ResourceManager::BufferInfo& info, GLenum target, GLsizeiptr size,
    GLenum usage, const std::string& label) {
  VERIFY_EQ(target, info.target);
  VERIFY_EQ(size, info.size);
  VERIFY_EQ(usage, info.usage);
  VERIFY_EQ(label, info.label);
  VERIFY_TRUE(info.mapped_data == nullptr);
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyIndexBufferInfo(
    const RendererPtr& renderer, const IndexBufferPtr& ibuffer, GLsizeiptr size,
    GLenum usage, const std::string& label) {
  typedef ResourceManager::BufferInfo BufferInfo;
  CallbackHelper<BufferInfo> callback;
  renderer->GetResourceManager()->RequestResourceInfo<BufferObject, BufferInfo>(
      ibuffer, std::bind(&CallbackHelper<BufferInfo>::Callback, &callback,
                         std::placeholders::_1));
  renderer->ProcessResourceInfoRequests();
  VERIFY_TRUE(callback.was_called);
  VERIFY_EQ(1U, callback.infos.size());
  VERIFY(VerifyBufferInfo(
      callback.infos[0], GL_ELEMENT_ARRAY_BUFFER, size, usage, label));
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// FramebufferInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifyAttachmentInfo(
    const ResourceManager::FramebufferInfo::Attachment& info, GLenum type,
    GLuint value, GLuint level, GLuint cube_face) {
  VERIFY_EQ(type, info.type);
  VERIFY_EQ(value, info.value);
  VERIFY_EQ(level, info.level);
  VERIFY_EQ(cube_face, info.cube_face);
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyRenderbufferInfo(
    const ResourceManager::RenderbufferInfo& info, GLsizei width,
    GLsizei height, GLenum internal_format, GLsizei red_size,
    GLsizei green_size, GLsizei blue_size, GLsizei alpha_size,
    GLsizei depth_size, GLsizei stencil_size, const std::string& name) {
  VERIFY_EQ(width, info.width);
  VERIFY_EQ(height, info.height);
  VERIFY_EQ(internal_format, info.internal_format);
  VERIFY_EQ(red_size, info.red_size);
  VERIFY_EQ(green_size, info.green_size);
  VERIFY_EQ(blue_size, info.blue_size);
  VERIFY_EQ(alpha_size, info.alpha_size);
  VERIFY_EQ(depth_size, info.depth_size);
  VERIFY_EQ(stencil_size, info.stencil_size);
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyFramebufferInfo(
    const ResourceManager::FramebufferInfo& info) {
  VERIFY(VerifyAttachmentInfo(info.color[0], GL_TEXTURE, 1U, 0U, 0U));
  VERIFY(VerifyAttachmentInfo(info.depth, GL_RENDERBUFFER, 1U, 0U, 0U));
  VERIFY(VerifyAttachmentInfo(info.stencil, GL_NONE, 0U, 0U, 0U));
  VERIFY(VerifyRenderbufferInfo(
      info.color_renderbuffers[0], 0, 0, GL_RGBA4, 0, 0, 0, 0, 0, 0, "color0"));
  VERIFY(VerifyRenderbufferInfo(
      info.depth_renderbuffer, 2, 2, GL_DEPTH_COMPONENT16,
      0, 0, 0, 0, 16, 0, "depth"));
  VERIFY(VerifyRenderbufferInfo(
      info.stencil_renderbuffer, 0, 0, GL_RGBA4, 0, 0, 0, 0, 0, 0, "stencil"));
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyFramebufferInfo2(
    const ResourceManager::FramebufferInfo& info) {
  VERIFY(VerifyAttachmentInfo(info.color[0], GL_RENDERBUFFER, 2U, 0U, 0U));
  VERIFY(VerifyAttachmentInfo(info.depth, GL_NONE, 0U, 0U, 0U));
  VERIFY(VerifyAttachmentInfo(info.stencil, GL_RENDERBUFFER, 3U, 0U, 0U));
  VERIFY(VerifyRenderbufferInfo(
      info.color_renderbuffers[0], 128, 1024, GL_RGB565, 5, 6, 5, 0, 0, 0,
      "color0"));
  VERIFY(VerifyRenderbufferInfo(
      info.depth_renderbuffer, 0, 0, GL_RGBA4, 0, 0, 0, 0, 0, 0, "depth"));
  VERIFY(VerifyRenderbufferInfo(
      info.stencil_renderbuffer, 128, 1024, GL_STENCIL_INDEX8,
      0, 0, 0, 0, 0, 8, "stencil"));
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// PlatformInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifyPlatformInfo(
    const ResourceManager::PlatformInfo& info, FakeGraphicsManager* gm) {
  VERIFY_EQ(3U, info.major_version);
  VERIFY_EQ(3U, info.minor_version);
  VERIFY_EQ(110U, info.glsl_version);
  VERIFY_EQ(gm->GetAliasedLineWidthRange(), info.aliased_line_width_range);
  VERIFY_EQ(gm->GetAliasedPointSizeRange(), info.aliased_point_size_range);
  VERIFY_EQ(gm->GetMaxCombinedTextureImageUnits(),
            info.max_combined_texture_image_units);
  VERIFY_EQ(gm->GetMaxCubeMapTextureSize(), info.max_cube_map_texture_size);
  VERIFY_EQ(gm->GetMaxFragmentUniformVectors(),
            info.max_fragment_uniform_vectors);
  VERIFY_EQ(gm->GetMaxRenderbufferSize(), info.max_renderbuffer_size);
  VERIFY_EQ(gm->GetMaxTextureImageUnits(), info.max_texture_image_units);
  VERIFY_EQ(gm->GetMaxTextureSize(), info.max_texture_size);
  VERIFY_EQ(gm->GetMaxTransformFeedbackInterleavedComponents(),
            info.max_transform_feedback_interleaved_components);
  VERIFY_EQ(gm->GetMaxTransformFeedbackSeparateAttribs(),
            info.max_transform_feedback_separate_attribs);
  VERIFY_EQ(gm->GetMaxTransformFeedbackSeparateComponents(),
            info.max_transform_feedback_separate_components);
  VERIFY_EQ(gm->GetMaxVaryingVectors(), info.max_varying_vectors);
  VERIFY_EQ(gm->GetMaxVertexAttribs(), info.max_vertex_attribs);
  VERIFY_EQ(gm->GetMaxVertexTextureImageUnits(),
            info.max_vertex_texture_image_units);
  VERIFY_EQ(gm->GetMaxVertexUniformVectors(),
            info.max_vertex_uniform_vectors);
  VERIFY_EQ(gm->GetMaxViewportDims(), info.max_viewport_dims);
  VERIFY_EQ(gm->GetTransformFeedbackVaryingMaxLength(),
            info.transform_feedback_varying_max_length);
  VERIFY_EQ(1U, info.shader_binary_formats.size());
  VERIFY_EQ(0xbadf00dU, info.shader_binary_formats[0]);
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// ShaderInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifyShaderInfo(
    const ResourceManager::ShaderInfo& info, int line, GLenum type,
    GLboolean delete_status, GLboolean compile_status,
    const std::string& source, const std::string& info_log,
    const std::string& label) {
  VERIFY_EQ(type, info.type);
  VERIFY_EQ(delete_status, info.delete_status);
  VERIFY_EQ(compile_status, info.compile_status);
  VERIFY_EQ(source, info.source);
  VERIFY_EQ(info_log, info.info_log);
  VERIFY_EQ(label, info.label);
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// TransformFeedbackInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifyTransformFeedbackInfo(
    const ResourceManager::TransformFeedbackInfo& info, GLuint buffer,
    GLboolean active, GLboolean paused) {
  VERIFY_EQ(buffer, info.buffer);
  VERIFY_EQ(active, info.active);
  VERIFY_EQ(paused, info.paused);
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// ProgramInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifyProgramAttribute(
    const ResourceManager::ProgramInfo::Attribute& attribute, GLint index,
    GLint type, GLint size, const std::string& name) {
  VERIFY_EQ(index, attribute.index);
  VERIFY_EQ(static_cast<GLenum>(type), attribute.type);
  VERIFY_EQ(size, attribute.size);
  VERIFY_EQ(name, attribute.name);
  return ::testing::AssertionSuccess();
}

template <typename T>
static ::testing::AssertionResult VerifyProgramUniform(
    const ResourceManager::ProgramInfo::Uniform& uniform, GLint index,
    GLint type, GLint size, const std::string& name, const T& value) {
  VERIFY_EQ(index, uniform.index);
  VERIFY_EQ(static_cast<GLenum>(type), uniform.type);
  VERIFY_EQ(size, uniform.size);
  VERIFY_EQ(name, uniform.name);
  VERIFY_TRUE(uniform.value.IsAssignableTo<T>());
  VERIFY_EQ(value, uniform.value.Get<T>());
  return ::testing::AssertionSuccess();
}

template <typename T>
static ::testing::AssertionResult VerifyProgramUniformArray(
    const ResourceManager::ProgramInfo::Uniform& uniform, GLint index,
    GLint type, GLint size, const std::string& name,
    const std::vector<T>& values) {
  VERIFY_EQ(index, uniform.index);
  VERIFY_EQ(static_cast<GLenum>(type), uniform.type);
  VERIFY_EQ(size, uniform.size);
  VERIFY_EQ(name, uniform.name);
  VERIFY_TRUE(uniform.value.ElementsAssignableTo<T>());
  VERIFY_EQ(size, static_cast<GLint>(uniform.value.GetCount()));
  for (GLint i = 0; i < size; ++i) {
    SCOPED_TRACE(::testing::Message() << "while testing element " << i);
    VERIFY_EQ(values[i], uniform.value.GetValueAt<T>(i));
  }
  return ::testing::AssertionSuccess();
}

template <int D, typename T>
static ::testing::AssertionResult VerifyProgramUniformVector(
    const ResourceManager::ProgramInfo::Uniform& uniform, GLint index,
    GLint type, GLint size, const std::string& name,
    const math::Vector<D, T>& value) {
  typedef math::VectorBase<D, T> Vector;
  VERIFY_EQ(index, uniform.index);
  VERIFY_EQ(static_cast<GLenum>(type), uniform.type);
  VERIFY_EQ(size, uniform.size);
  VERIFY_EQ(name, uniform.name);
  VERIFY_TRUE(uniform.value.IsAssignableTo<Vector>());
  VERIFY_TRUE(Vector::AreValuesEqual(value, uniform.value.Get<Vector>()));
  return ::testing::AssertionSuccess();
}

template <int D, typename T>
static ::testing::AssertionResult VerifyProgramUniformArrayVector(
    const ResourceManager::ProgramInfo::Uniform& uniform, GLint index,
    GLint type, GLint size, const std::string& name,
    const std::vector< math::Vector<D, T>>& values) {
  typedef math::VectorBase<D, T> Vector;
  VERIFY_EQ(index, uniform.index);
  VERIFY_EQ(static_cast<GLenum>(type), uniform.type);
  VERIFY_EQ(size, uniform.size);
  VERIFY_EQ(size, static_cast<GLint>(uniform.value.GetCount()));
  VERIFY_TRUE(uniform.value.ElementsAssignableTo<Vector>());
  for (GLint i = 0; i < size; ++i) {
    SCOPED_TRACE(::testing::Message() << "while testing element " << i);
    VERIFY_TRUE(
        Vector::AreValuesEqual(values[i], uniform.value.GetValueAt<Vector>(i)));
  }
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyDefaultProgramInfo(
    const ResourceManager::ProgramInfo& info, int line) {
  VERIFY_EQ(0U, info.geometry_shader);
  VERIFY_EQ(1U, info.vertex_shader);
  VERIFY_EQ(2U, info.fragment_shader);
  VERIFY_EQ("Default Renderer shader", info.label);
  VERIFY_EQ(1U, info.attributes.size());
  VERIFY(VerifyProgramAttribute(
      info.attributes[0], 0, GL_FLOAT_VEC3, 1, "aVertex"));

  VERIFY_EQ(3U, info.uniforms.size());
  VERIFY(VerifyProgramUniform(
      info.uniforms[0], 0, GL_FLOAT_MAT4, 1, "uProjectionMatrix",
      Matrix4f(1.f, 2.f, 3.f, 4.f,
               5.f, 1.f, 7.f, 8.f,
               9.f, 1.f, 1.f, 3.f,
               4.f, 5.f, 6.f, 1.f)));
  VERIFY(VerifyProgramUniform(
      info.uniforms[1], 1, GL_FLOAT_MAT4, 1, "uModelviewMatrix",
      Matrix4f(4.f, 2.f, 3.f, 4.f,
               5.f, 4.f, 7.f, 8.f,
               9.f, 1.f, 4.f, 3.f,
               4.f, 5.f, 6.f, 4.f)));
  VERIFY(VerifyProgramUniformVector(
      info.uniforms[2], 2, GL_FLOAT_VEC4, 1, "uBaseColor",
      math::Vector4f(4.f, 3.f, 2.f, 1.f)));
  return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult VerifyProgramInfo(
    const ResourceManager::ProgramInfo& info, int line) {
  VERIFY_EQ("Dummy Shader", info.label);
  VERIFY_EQ(GL_FALSE, info.delete_status);
  VERIFY_EQ(GL_TRUE, info.link_status);
  VERIFY_EQ(GL_FALSE, info.validate_status);
  VERIFY_EQ("", info.info_log);

  // Verify each attribute.
  VERIFY_EQ(9U, info.attributes.size());
  VERIFY(VerifyProgramAttribute(info.attributes[0], 0, GL_FLOAT, 1, "aFloat"));
  VERIFY(
      VerifyProgramAttribute(info.attributes[1], 1, GL_FLOAT_VEC2, 1, "aFV2"));
  VERIFY(
      VerifyProgramAttribute(info.attributes[2], 2, GL_FLOAT_VEC3, 1, "aFV3"));
  VERIFY(
      VerifyProgramAttribute(info.attributes[3], 3, GL_FLOAT_VEC4, 1, "aFV4"));
  VERIFY(
      VerifyProgramAttribute(info.attributes[4], 4, GL_FLOAT_MAT2, 1, "aMat2"));
  VERIFY(
      VerifyProgramAttribute(info.attributes[5], 6, GL_FLOAT_MAT3, 1, "aMat3"));
  VERIFY(
      VerifyProgramAttribute(info.attributes[6], 9, GL_FLOAT_MAT4, 1, "aMat4"));
  VERIFY(VerifyProgramAttribute(info.attributes[7], 13, GL_FLOAT_VEC2, 1,
                                "aBOE1"));
  VERIFY(VerifyProgramAttribute(info.attributes[8], 14, GL_FLOAT_VEC3, 1,
                                "aBOE2"));

  int i = 0;
  VERIFY_EQ(36U, info.uniforms.size());
  VERIFY(VerifyProgramUniform(info.uniforms[i], i, GL_INT, 1, "uInt", 13));
  ++i;
  VERIFY(
      VerifyProgramUniform(info.uniforms[i], i, GL_FLOAT, 1, "uFloat", 1.5f));
  ++i;
  VERIFY(VerifyProgramUniform(info.uniforms[i], i, GL_INT, 1, "uIntGS", 27));
  ++i;
  VERIFY(VerifyProgramUniform(info.uniforms[i], i, GL_UNSIGNED_INT, 1,
                              "uUintGS", 33U));
  ++i;
  VERIFY(VerifyProgramUniformVector(
      info.uniforms[i], i, GL_FLOAT_VEC2, 1, "uFV2", Vector2f(2.f, 3.f)));
  ++i;
  VERIFY(VerifyProgramUniformVector(
      info.uniforms[i], i, GL_FLOAT_VEC3, 1, "uFV3", Vector3f(4.f, 5.f, 6.f)));
  ++i;
  VERIFY(VerifyProgramUniformVector(
      info.uniforms[i], i, GL_FLOAT_VEC4, 1, "uFV4",
      Vector4f(7.f, 8.f, 9.f, 10.f)));
  ++i;
  VERIFY(VerifyProgramUniform(info.uniforms[i], i, GL_UNSIGNED_INT, 1, "uUint",
                              15U));
  ++i;
  VERIFY(VerifyProgramUniform(info.uniforms[i], i, GL_SAMPLER_CUBE, 1,
                              "uCubeMapTex", 0));
  ++i;
  VERIFY(
      VerifyProgramUniform(info.uniforms[i], i, GL_SAMPLER_2D, 1, "uTex", 1));
  ++i;
  VERIFY(VerifyProgramUniformVector(
      info.uniforms[i], i, GL_INT_VEC2, 1, "uIV2", Vector2i(2, 3)));
  ++i;
  VERIFY(VerifyProgramUniformVector(
      info.uniforms[i], i, GL_INT_VEC3, 1, "uIV3", Vector3i(4, 5, 6)));
  ++i;
  VERIFY(VerifyProgramUniformVector(
      info.uniforms[i], i, GL_INT_VEC4, 1, "uIV4", Vector4i(7, 8, 9, 10)));
  ++i;
  VERIFY(VerifyProgramUniformVector(info.uniforms[i], i, GL_UNSIGNED_INT_VEC2,
                                    1, "uUV2", Vector2ui(2U, 3U)));
  ++i;
  VERIFY(VerifyProgramUniformVector(info.uniforms[i], i, GL_UNSIGNED_INT_VEC3,
                                    1, "uUV3", Vector3ui(4U, 5U, 6U)));
  ++i;
  VERIFY(VerifyProgramUniformVector(info.uniforms[i], i, GL_UNSIGNED_INT_VEC4,
                                    1, "uUV4", Vector4ui(7U, 8U, 9U, 10U)));
  ++i;
  VERIFY(VerifyProgramUniform(info.uniforms[i], i, GL_FLOAT_MAT2, 1, "uMat2",
                              Matrix2f(1.f, 2.f,
                                       3.f, 4.f)));
  ++i;
  VERIFY(VerifyProgramUniform(info.uniforms[i], i, GL_FLOAT_MAT3, 1, "uMat3",
                              Matrix3f(1.f, 2.f, 3.f,
                                       4.f, 5.f, 6.f,
                                       7.f, 8.f, 9.f)));
  ++i;
  VERIFY(VerifyProgramUniform(info.uniforms[i], i, GL_FLOAT_MAT4, 1, "uMat4",
                              Matrix4f(1.f, 2.f, 3.f, 4.f,
                                       5.f, 6.f, 7.f, 8.f,
                                       9.f, 1.f, 2.f, 3.f,
                                       4.f, 5.f, 6.f, 7.f)));
  ++i;

  std::vector<int> ints;
  ints.push_back(1);
  ints.push_back(2);
  std::vector<uint32> uints;
  uints.push_back(3U);
  uints.push_back(4U);
  std::vector<float> floats;
  floats.push_back(1.f);
  floats.push_back(2.f);
  std::vector<int> cubemaps;
  cubemaps.push_back(2);
  cubemaps.push_back(3);
  std::vector<int> textures;
  textures.push_back(4);
  textures.push_back(5);
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

  int j = i;
  VERIFY(VerifyProgramUniformArray(info.uniforms[i], j, GL_INT, 2,
                                   "uIntArray", ints));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArray(info.uniforms[i], j, GL_UNSIGNED_INT, 2,
                                   "uUintArray", uints));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArray(info.uniforms[i], j, GL_FLOAT, 2,
                                   "uFloatArray", floats));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArray(info.uniforms[i], j, GL_SAMPLER_CUBE, 2,
                                   "uCubeMapTexArray", cubemaps));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArray(info.uniforms[i], j, GL_SAMPLER_2D, 2,
                                   "uTexArray", textures));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(info.uniforms[i], j, GL_FLOAT_VEC2,
                                         2, "uFV2Array", vector2fs));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(info.uniforms[i], j, GL_FLOAT_VEC3,
                                         2, "uFV3Array", vector3fs));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(info.uniforms[i], j, GL_FLOAT_VEC4,
                                         2, "uFV4Array", vector4fs));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(info.uniforms[i], j, GL_INT_VEC2, 2,
                                         "uIV2Array", vector2is));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(info.uniforms[i], j, GL_INT_VEC3, 2,
                                         "uIV3Array", vector3is));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(info.uniforms[i], j, GL_INT_VEC4, 2,
                                         "uIV4Array", vector4is));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(
      info.uniforms[i], j, GL_UNSIGNED_INT_VEC2, 2, "uUV2Array", vector2uis));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(
      info.uniforms[i], j, GL_UNSIGNED_INT_VEC3, 2, "uUV3Array", vector3uis));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArrayVector(
      info.uniforms[i], j, GL_UNSIGNED_INT_VEC4, 2, "uUV4Array", vector4uis));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArray(info.uniforms[i], j, GL_FLOAT_MAT2, 2,
                                   "uMat2Array", matrix2fs));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArray(info.uniforms[i], j, GL_FLOAT_MAT3, 2,
                                   "uMat3Array", matrix3fs));
  ++i; j += 2;
  VERIFY(VerifyProgramUniformArray(info.uniforms[i], j, GL_FLOAT_MAT4, 2,
                                   "uMat4Array", matrix4fs));
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// SamplerInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifySamplerInfo(
    const ResourceManager::SamplerInfo& expected,
    const ResourceManager::SamplerInfo& info) {
  VERIFY_EQ(expected.id, info.id);
  VERIFY_EQ(expected.label, info.label);
  VERIFY_EQ(expected.compare_mode, info.compare_mode);
  VERIFY_EQ(expected.compare_func, info.compare_func);
  VERIFY_EQ(expected.min_filter, info.min_filter);
  VERIFY_EQ(expected.mag_filter, info.mag_filter);
  VERIFY_EQ(expected.max_anisotropy, info.max_anisotropy);
  VERIFY_EQ(expected.min_lod, info.min_lod);
  VERIFY_EQ(expected.max_lod, info.max_lod);
  VERIFY_EQ(expected.wrap_r, info.wrap_r);
  VERIFY_EQ(expected.wrap_s, info.wrap_s);
  VERIFY_EQ(expected.wrap_t, info.wrap_t);
  return ::testing::AssertionSuccess();
}

//-----------------------------------------------------------------------------
//
// TextureInfo verification routines.
//
//-----------------------------------------------------------------------------
static ::testing::AssertionResult VerifyTextureInfo(
    const ResourceManager::TextureInfo& expected,
    const ResourceManager::TextureInfo& info) {
  VERIFY_EQ(expected.id, info.id);
  VERIFY_EQ(expected.label, info.label);
  VERIFY_EQ(expected.unit, info.unit);
  VERIFY_EQ(expected.compare_mode, info.compare_mode);
  VERIFY_EQ(expected.compare_func, info.compare_func);
  VERIFY_EQ(expected.fixed_sample_locations, info.fixed_sample_locations);
  VERIFY_EQ(expected.min_filter, info.min_filter);
  VERIFY_EQ(expected.mag_filter, info.mag_filter);
  VERIFY_EQ(expected.max_anisotropy, info.max_anisotropy);
  VERIFY_EQ(expected.min_lod, info.min_lod);
  VERIFY_EQ(expected.max_lod, info.max_lod);
  VERIFY_EQ(expected.samples, info.samples);
  VERIFY_EQ(expected.swizzle_r, info.swizzle_r);
  VERIFY_EQ(expected.swizzle_g, info.swizzle_g);
  VERIFY_EQ(expected.swizzle_b, info.swizzle_b);
  VERIFY_EQ(expected.swizzle_a, info.swizzle_a);
  if (expected.target == GL_TEXTURE_2D_ARRAY ||
      expected.target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
      expected.target == GL_TEXTURE_3D ||
      expected.target == GL_TEXTURE_CUBE_MAP_ARRAY) {
    VERIFY_EQ(expected.wrap_r, info.wrap_r);
  }
  VERIFY_EQ(expected.wrap_s, info.wrap_s);
  VERIFY_EQ(expected.wrap_t, info.wrap_t);
  VERIFY_EQ(expected.target, info.target);
  return ::testing::AssertionSuccess();
}

#undef VERIFY_EQ
#undef VERIFY_TRUE
#undef VERIFY

}  // anonymous namespace

class ResourceManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    fake_gl_context_ = FakeGlContext::Create(kWidth, kHeight);
    portgfx::GlContext::MakeCurrent(fake_gl_context_);
    gm_.Reset(new FakeGraphicsManager());
    renderer_.Reset(new Renderer(gm_));
  }

  void TearDown() override {
    renderer_.Reset(nullptr);
    gm_.Reset(nullptr);
    Renderer::DestroyStateCache(fake_gl_context_);
    fake_gl_context_.Reset(nullptr);
  }

  void DrawScene(const NodePtr& root) {
    renderer_->DrawScene(root);
    // TestScene includes some invalid index buffer types.
    gm_->SetErrorCode(GL_NO_ERROR);
  }

  base::SharedPtr<FakeGlContext> fake_gl_context_;
  FakeGraphicsManagerPtr gm_;
  RendererPtr renderer_;

  static const int kWidth = 400;
  static const int kHeight = 300;
};

TEST_F(ResourceManagerTest, GetGraphicsManager) {
  ResourceManager* manager = renderer_->GetResourceManager();
  EXPECT_EQ(gm_.Get(), manager->GetGraphicsManager().Get());
}

TEST_F(ResourceManagerTest, GetNoInfos) {
  EXPECT_TRUE(
      (VerifyNoInfos<AttributeArray, ResourceManager::ArrayInfo>(renderer_)));
  EXPECT_TRUE(
      (VerifyNoInfos<BufferObject, ResourceManager::BufferInfo>(renderer_)));
  EXPECT_TRUE(
      (VerifyNoInfos<FramebufferObject, ResourceManager::FramebufferInfo>(
          renderer_)));
  EXPECT_TRUE(
      (VerifyNoInfos<ShaderProgram, ResourceManager::ProgramInfo>(renderer_)));
  EXPECT_TRUE(
      (VerifyNoInfos<TextureBase, ResourceManager::TextureInfo>(renderer_)));
}

TEST_F(ResourceManagerTest, GetArrayInfo) {
  typedef ResourceManager::ArrayInfo ArrayInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<ArrayInfo> callback;

  // Get info on the attribute array.
  const ShapePtr shape =
      root->GetChildren()[0]->GetChildren()[0]->GetShapes()[0];
  manager->RequestResourceInfo<AttributeArray, ArrayInfo>(
      shape->GetAttributeArray(),
      std::bind(&CallbackHelper<ArrayInfo>::Callback, &callback,
                std::placeholders::_1));
  DrawScene(root);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_TRUE(VerifyArrayInfo(callback.infos[0], 3U,
                              static_cast<size_t>(gm_->GetMaxVertexAttribs())));

  callback.Reset();
  manager->RequestAllResourceInfos<AttributeArray, ArrayInfo>(std::bind(
      &CallbackHelper<ArrayInfo>::Callback, &callback, std::placeholders::_1));
  DrawScene(root);
  EXPECT_TRUE(callback.was_called);
  // There are two resources, one for each shader.
  EXPECT_EQ(2U, callback.infos.size());
  EXPECT_TRUE(VerifyDefaultArrayInfo(
      callback.infos[0], static_cast<size_t>(gm_->GetMaxVertexAttribs())));
  EXPECT_TRUE(VerifyArrayInfo(callback.infos[1], 3U,
                              static_cast<size_t>(gm_->GetMaxVertexAttribs())));
}

TEST_F(ResourceManagerTest, GetBufferInfo) {
  typedef ResourceManager::BufferInfo BufferInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();

  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<BufferInfo> callback;

  // Get the vertex buffer object info from the first buffer attribute.
  const base::AllocVector<ShapePtr>& shapes =
      root->GetChildren()[0]->GetChildren()[0]->GetShapes();
  ASSERT_LT(0U, shapes.size());
  ASSERT_TRUE(shapes[0]->GetAttributeArray().Get() != nullptr);
  ASSERT_LT(0U, shapes[0]->GetAttributeArray()->GetBufferAttributeCount());
  const BufferObjectPtr buffer =
      shapes[0]->GetAttributeArray()->GetBufferAttribute(0)
      .GetValue<BufferObjectElement>().buffer_object;
  manager->RequestResourceInfo<BufferObject, BufferInfo>(
      buffer, std::bind(&CallbackHelper<BufferInfo>::Callback, &callback,
                        std::placeholders::_1));
  DrawScene(root);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  // The vertex count here is 0 since the default shader is never executed on
  // a shape.
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[0], GL_ARRAY_BUFFER,
                               sizeof(testing::TestScene::Vertex) * 3,
                               GL_STATIC_DRAW, "Vertex buffer"));
  callback.Reset();

  // Get some index buffers infos.
  EXPECT_TRUE(VerifyIndexBufferInfo(
      renderer_, shapes[0]->GetIndexBuffer(),
      sizeof(int8) * scene.GetIndexCount(), GL_STATIC_DRAW, "Indices #0"));
  EXPECT_TRUE(VerifyIndexBufferInfo(
      renderer_, shapes[1]->GetIndexBuffer(),
      sizeof(uint8) * scene.GetIndexCount(), GL_STATIC_DRAW, "Indices #1"));
  EXPECT_TRUE(VerifyIndexBufferInfo(
      renderer_, shapes[2]->GetIndexBuffer(),
      sizeof(int16) * scene.GetIndexCount(), GL_DYNAMIC_DRAW, "Indices #2"));
  EXPECT_TRUE(VerifyIndexBufferInfo(
      renderer_, shapes[3]->GetIndexBuffer(),
      sizeof(uint16) * scene.GetIndexCount(), GL_STREAM_DRAW, "Indices #3"));
  EXPECT_TRUE(VerifyIndexBufferInfo(
      renderer_, shapes[4]->GetIndexBuffer(),
      sizeof(int32) * scene.GetIndexCount(), GL_STATIC_DRAW, "Indices #4"));
  EXPECT_TRUE(VerifyIndexBufferInfo(
      renderer_, shapes[5]->GetIndexBuffer(),
      sizeof(uint32) * scene.GetIndexCount(), GL_DYNAMIC_DRAW, "Indices #5"));
  EXPECT_TRUE(VerifyIndexBufferInfo(
      renderer_, shapes[6]->GetIndexBuffer(),
      sizeof(float) * scene.GetIndexCount(), GL_STREAM_DRAW, "Indices #6"));

  // Get all buffers.
  manager->RequestAllResourceInfos<BufferObject, BufferInfo>(std::bind(
      &CallbackHelper<BufferInfo>::Callback, &callback, std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(9U, callback.infos.size());
  // The vertex count here is 0 since the default shader is never executed on
  // a shape.
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[0], GL_ARRAY_BUFFER,
                   sizeof(testing::TestScene::Vertex) * 3, GL_STATIC_DRAW,
                   ""));
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[1], GL_ARRAY_BUFFER,
                   sizeof(testing::TestScene::Vertex) * 3, GL_STATIC_DRAW,
                   "Vertex buffer"));
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[2], GL_ELEMENT_ARRAY_BUFFER,
                   sizeof(int8) * scene.GetIndexCount(), GL_STATIC_DRAW,
                   "Indices #0"));
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[3], GL_ELEMENT_ARRAY_BUFFER,
                   sizeof(uint8) * scene.GetIndexCount(), GL_STATIC_DRAW,
                   "Indices #1"));
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[4], GL_ELEMENT_ARRAY_BUFFER,
                   sizeof(int16) * scene.GetIndexCount(), GL_DYNAMIC_DRAW,
                   "Indices #2"));
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[5], GL_ELEMENT_ARRAY_BUFFER,
                   sizeof(uint16) * scene.GetIndexCount(), GL_STREAM_DRAW,
                   "Indices #3"));
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[6], GL_ELEMENT_ARRAY_BUFFER,
                   sizeof(int32) * scene.GetIndexCount(), GL_STATIC_DRAW,
                   "Indices #4"));
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[7], GL_ELEMENT_ARRAY_BUFFER,
                   sizeof(uint32) * scene.GetIndexCount(), GL_DYNAMIC_DRAW,
                   "Indices #5"));
  EXPECT_TRUE(VerifyBufferInfo(callback.infos[8], GL_ELEMENT_ARRAY_BUFFER,
                   sizeof(float) * scene.GetIndexCount(), GL_STREAM_DRAW,
                   "Indices #6"));
}

TEST_F(ResourceManagerTest, GetFramebufferInfo) {
  typedef ResourceManager::FramebufferInfo FramebufferInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<FramebufferInfo> callback;

  TexturePtr tex = scene.CreateTexture();
  FramebufferObjectPtr fbo(new FramebufferObject(2, 2));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(tex));
  fbo->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  fbo->SetLabel("my fbo");

  // Get info on the fbo.
  manager->RequestResourceInfo<FramebufferObject, FramebufferInfo>(
      fbo, std::bind(&CallbackHelper<FramebufferInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->BindFramebuffer(fbo);
  DrawScene(root);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ("my fbo", callback.infos[0].label);
  EXPECT_TRUE(VerifyFramebufferInfo(callback.infos[0]));
  callback.Reset();

  manager->RequestAllResourceInfos<FramebufferObject, FramebufferInfo>(
      std::bind(&CallbackHelper<FramebufferInfo>::Callback, &callback,
                std::placeholders::_1));
  DrawScene(root);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_TRUE(VerifyFramebufferInfo(callback.infos[0]));
  callback.Reset();

  fbo.Reset(new FramebufferObject(128, 1024));
  fbo->SetColorAttachment(0U,
                          FramebufferObject::Attachment(Image::kRgb565Byte));
  fbo->SetStencilAttachment(FramebufferObject::Attachment(Image::kStencil8));
  fbo->SetLabel("my new fbo");

  // Get info on the fbo.
  manager->RequestResourceInfo<FramebufferObject, FramebufferInfo>(
      fbo, std::bind(&CallbackHelper<FramebufferInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ("my new fbo", callback.infos[0].label);
  EXPECT_TRUE(VerifyFramebufferInfo2(callback.infos[0]));

  // Now disable some function groups.
  gm_->EnableFeature(GraphicsManager::kDrawBuffers, false);
  manager->RequestResourceInfo<FramebufferObject, FramebufferInfo>(
      fbo, std::bind(&CallbackHelper<FramebufferInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ("my new fbo", callback.infos[0].label);
  EXPECT_TRUE(VerifyFramebufferInfo2(callback.infos[0]));

  gm_->EnableFeature(GraphicsManager::kDrawBuffer, false);
  manager->RequestResourceInfo<FramebufferObject, FramebufferInfo>(
      fbo, std::bind(&CallbackHelper<FramebufferInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ("my new fbo", callback.infos[0].label);
  EXPECT_TRUE(VerifyFramebufferInfo2(callback.infos[0]));

  gm_->EnableFeature(GraphicsManager::kReadBuffer, false);
  manager->RequestResourceInfo<FramebufferObject, FramebufferInfo>(
      fbo, std::bind(&CallbackHelper<FramebufferInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ("my new fbo", callback.infos[0].label);
  EXPECT_TRUE(VerifyFramebufferInfo2(callback.infos[0]));
}

TEST_F(ResourceManagerTest, GetFramebufferInfoNexus6) {
  typedef ResourceManager::FramebufferInfo FramebufferInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<FramebufferInfo> callback;

  TexturePtr tex = scene.CreateTexture();
  FramebufferObjectPtr fbo(new FramebufferObject(2, 2));
  fbo->SetColorAttachment(0U, FramebufferObject::Attachment(tex));
  fbo->SetDepthAttachment(
      FramebufferObject::Attachment(Image::kRenderbufferDepth16));
  fbo->SetLabel("my fbo");

  // Fake parameters of a Nexus 6. FakeGlContext will change its behavior.
  gm_->SetVendorString("Qualcomm");
  gm_->SetRendererString("Adreno (TM) 420");

  // Get info on the fbo.
  manager->RequestResourceInfo<FramebufferObject, FramebufferInfo>(
      fbo, std::bind(&CallbackHelper<FramebufferInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->BindFramebuffer(fbo);
  DrawScene(root);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ("my fbo", callback.infos[0].label);
  EXPECT_TRUE(VerifyFramebufferInfo(callback.infos[0]));
  callback.Reset();
}

TEST_F(ResourceManagerTest, GetPlatformInfo) {
  typedef ResourceManager::PlatformInfo PlatformInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  ResourceManager* manager = renderer_->GetResourceManager();
  testing::TraceVerifier verifier(gm_.Get());
  {
    CallbackHelper<PlatformInfo> callback;
    manager->RequestPlatformInfo(
        std::bind(&CallbackHelper<PlatformInfo>::Callback, &callback,
                  std::placeholders::_1));
    DrawScene(root);
    EXPECT_TRUE(callback.was_called);
    EXPECT_EQ(1U, callback.infos.size());
    EXPECT_TRUE(VerifyPlatformInfo(callback.infos[0], gm_.Get()));
    // Logging is stripped in production builds.
#if !ION_PRODUCTION
    EXPECT_EQ(1U, verifier.GetCountOf("GetFloatv(GL_ALIASED_POINT_SIZE_RANGE"));
    EXPECT_EQ(0U, verifier.GetCountOf("GetFloatv(GL_POINT_SIZE_RANGE"));
#endif
  }
  verifier.Reset();
  {
    gm_->SetVersionString("3.3 Ion OpenGL");
    CallbackHelper<PlatformInfo> callback;
    manager->RequestPlatformInfo(
        std::bind(&CallbackHelper<PlatformInfo>::Callback, &callback,
                  std::placeholders::_1));
    DrawScene(root);
    EXPECT_TRUE(callback.was_called);
    EXPECT_EQ(1U, callback.infos.size());
    EXPECT_TRUE(VerifyPlatformInfo(callback.infos[0], gm_.Get()));
    // Logging is stripped in production builds.
#if !ION_PRODUCTION
    EXPECT_EQ(1U, verifier.GetCountOf("GetFloatv(GL_ALIASED_POINT_SIZE_RANGE"));
    EXPECT_EQ(1U, verifier.GetCountOf("GetFloatv(GL_POINT_SIZE_RANGE"));
#endif
  }
}

TEST_F(ResourceManagerTest, GetProgramInfo) {
  typedef ResourceManager::ProgramInfo ProgramInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<ProgramInfo> callback;

  // Get the default shader from the root.
  manager->RequestResourceInfo<ShaderProgram, ProgramInfo>(
      renderer_->GetDefaultShaderProgram(),
      std::bind(&CallbackHelper<ProgramInfo>::Callback, &callback,
                std::placeholders::_1));
  DrawScene(root);

  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_TRUE(VerifyDefaultProgramInfo(callback.infos[0], __LINE__));
  callback.Reset();

  // Get all programs.
  manager->RequestAllResourceInfos<ShaderProgram, ProgramInfo>(
      std::bind(&CallbackHelper<ProgramInfo>::Callback, &callback,
                std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(2U, callback.infos.size());
  // The default program is in info[0];
  EXPECT_TRUE(VerifyDefaultProgramInfo(callback.infos[0], __LINE__));
  // The custom program is in info[1].
  EXPECT_TRUE(VerifyProgramInfo(callback.infos[1], __LINE__));
  callback.Reset();
}

TEST_F(ResourceManagerTest, GetSamplerInfo) {
  typedef ResourceManager::SamplerInfo SamplerInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  NodePtr child = root->GetChildren()[0];
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<SamplerInfo> callback;

  // Get info on the texture.
  const size_t tex_index = child->GetUniformIndex("uTex");
  TexturePtr texture = child->GetUniforms()[tex_index].GetValue<TexturePtr>();
  manager->RequestResourceInfo<Sampler, SamplerInfo>(
      texture->GetSampler(), std::bind(&CallbackHelper<SamplerInfo>::Callback,
                                       &callback, std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  SamplerInfo expected0;
  expected0.label = "Sampler";
  expected0.id = 1;
  expected0.compare_mode = GL_COMPARE_REF_TO_TEXTURE;
  expected0.compare_func = GL_NEVER;
  expected0.max_anisotropy = 1.f;
  expected0.min_lod = -0.5f;
  expected0.max_lod = 0.5f;
  expected0.min_filter = GL_LINEAR_MIPMAP_LINEAR;
  expected0.mag_filter = GL_NEAREST;
  expected0.wrap_r = GL_MIRRORED_REPEAT;
  expected0.wrap_s = GL_MIRRORED_REPEAT;
  expected0.wrap_t = GL_CLAMP_TO_EDGE;
  EXPECT_TRUE(VerifySamplerInfo(expected0, callback.infos[0]));
  callback.Reset();

  // Create a new texture and get its infos.
  TexturePtr tex = scene.CreateTexture();
  tex->GetSampler()->SetMinFilter(Sampler::kLinear);
  tex->GetSampler()->SetMagFilter(Sampler::kLinear);
  tex->GetSampler()->SetWrapS(Sampler::kClampToEdge);
  tex->GetSampler()->SetWrapT(Sampler::kRepeat);
  tex->GetSampler()->SetMaxAnisotropy(2.f);

  manager->RequestResourceInfo<Sampler, SamplerInfo>(
      tex->GetSampler(), std::bind(&CallbackHelper<SamplerInfo>::Callback,
                                   &callback, std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  SamplerInfo expected1 = expected0;
  expected1.id = 2;
  expected1.max_anisotropy = 2.f;
  expected1.min_filter = GL_LINEAR;
  expected1.mag_filter = GL_LINEAR;
  expected1.wrap_s = GL_CLAMP_TO_EDGE;
  expected1.wrap_t = GL_REPEAT;
  EXPECT_TRUE(VerifySamplerInfo(expected1, callback.infos[0]));
  callback.Reset();

  // Get all textures.
  manager->RequestAllResourceInfos<Sampler, SamplerInfo>(
      std::bind(&CallbackHelper<SamplerInfo>::Callback, &callback,
                std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(2U, callback.infos.size());
  EXPECT_TRUE(VerifySamplerInfo(expected0, callback.infos[0]));
  EXPECT_TRUE(VerifySamplerInfo(expected1, callback.infos[1]));
}

TEST_F(ResourceManagerTest, GetShaderInfo) {
  typedef ResourceManager::ShaderInfo ShaderInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<ShaderInfo> callback;

  // Get the default shader from the root.
  manager->RequestResourceInfo<Shader, ShaderInfo>(
      renderer_->GetDefaultShaderProgram()->GetVertexShader(),
      std::bind(&CallbackHelper<ShaderInfo>::Callback, &callback,
                std::placeholders::_1));
  DrawScene(root);

  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_TRUE(VerifyShaderInfo(
      callback.infos[0], __LINE__, GL_VERTEX_SHADER, GL_FALSE, GL_TRUE,
      renderer_->GetDefaultShaderProgram()->GetVertexShader()->GetSource(), "",
      "Default Renderer vertex shader"));
  callback.Reset();

  manager->RequestResourceInfo<Shader, ShaderInfo>(
      renderer_->GetDefaultShaderProgram()->GetFragmentShader(),
      std::bind(&CallbackHelper<ShaderInfo>::Callback, &callback,
                std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_TRUE(VerifyShaderInfo(
      callback.infos[0], __LINE__, GL_FRAGMENT_SHADER, GL_FALSE, GL_TRUE,
      renderer_->GetDefaultShaderProgram()->GetFragmentShader()->GetSource(),
      "",
      "Default Renderer fragment shader"));
  callback.Reset();

  // Get all shaders.
  manager->RequestAllResourceInfos<Shader, ShaderInfo>(std::bind(
      &CallbackHelper<ShaderInfo>::Callback, &callback, std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(5U, callback.infos.size());
  EXPECT_TRUE(VerifyShaderInfo(
      callback.infos[0], __LINE__, GL_VERTEX_SHADER, GL_FALSE, GL_TRUE,
      renderer_->GetDefaultShaderProgram()->GetVertexShader()->GetSource(), "",
      "Default Renderer vertex shader"));
  EXPECT_TRUE(VerifyShaderInfo(
      callback.infos[1], __LINE__, GL_FRAGMENT_SHADER, GL_FALSE, GL_TRUE,
      renderer_->GetDefaultShaderProgram()->GetFragmentShader()->GetSource(),
      "", "Default Renderer fragment shader"));
  EXPECT_TRUE(VerifyShaderInfo(
      callback.infos[2], __LINE__, GL_VERTEX_SHADER, GL_FALSE, GL_TRUE,
      scene.GetVertexShaderSource(), "", "Vertex shader"));
  EXPECT_TRUE(VerifyShaderInfo(
      callback.infos[3], __LINE__, GL_GEOMETRY_SHADER, GL_FALSE, GL_TRUE,
      scene.GetGeometryShaderSource(), "", "Geometry shader"));
  EXPECT_TRUE(VerifyShaderInfo(
      callback.infos[4], __LINE__, GL_FRAGMENT_SHADER, GL_FALSE, GL_TRUE,
      scene.GetFragmentShaderSource(), "", "Fragment shader"));
  callback.Reset();
}

TEST_F(ResourceManagerTest, GetTextureInfoNoSamplers) {
  gm_->EnableFeature(GraphicsManager::kSamplerObjects, false);
  typedef ResourceManager::TextureInfo TextureInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  NodePtr child = root->GetChildren()[0];
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<TextureInfo> callback;

  // Get info on the texture.
  const size_t tex_index = child->GetUniformIndex("uTex");
  TexturePtr texture = child->GetUniforms()[tex_index].GetValue<TexturePtr>();
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      texture, std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                         std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  TextureInfo expected0;
  expected0.label = "Texture";
  expected0.id = 1;
  expected0.sampler = 0;
  expected0.unit = GL_TEXTURE0;
  expected0.width = 2;
  expected0.height = 2;
  expected0.format = Image::kRgb888;
  expected0.base_level = 10;
  expected0.max_level = 100;
  expected0.compare_mode = GL_COMPARE_REF_TO_TEXTURE;
  expected0.compare_func = GL_NEVER;
  expected0.fixed_sample_locations = GL_TRUE;
  expected0.min_lod = -0.5f;
  expected0.max_lod = 0.5f;
  expected0.min_filter = GL_LINEAR_MIPMAP_LINEAR;
  expected0.mag_filter = GL_NEAREST;
  expected0.samples = 0;
  expected0.swizzle_r = GL_ALPHA;
  expected0.swizzle_g = GL_BLUE;
  expected0.swizzle_b = GL_GREEN;
  expected0.swizzle_a = GL_RED;
  expected0.wrap_r = GL_MIRRORED_REPEAT;
  expected0.wrap_s = GL_MIRRORED_REPEAT;
  expected0.wrap_t = GL_CLAMP_TO_EDGE;
  expected0.target = GL_TEXTURE_2D;
  EXPECT_TRUE(VerifyTextureInfo(expected0, callback.infos[0]));
  callback.Reset();

  // Get info on the cube_map texture.
  const size_t cube_map_index = child->GetUniformIndex("uCubeMapTex");
  CubeMapTexturePtr cube_map =
      child->GetUniforms()[cube_map_index].GetValue<CubeMapTexturePtr>();
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      cube_map, std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                         std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  TextureInfo expected_cube = expected0;
  expected_cube.label = "Cubemap";
  expected_cube.id = 2;
  expected_cube.unit = GL_TEXTURE1;
  expected_cube.min_lod = -1.5f;
  expected_cube.max_lod = 1.5f;
  expected_cube.wrap_r = GL_CLAMP_TO_EDGE;
  expected_cube.target = GL_TEXTURE_CUBE_MAP;
  EXPECT_TRUE(VerifyTextureInfo(expected_cube, callback.infos[0]));
  callback.Reset();

  // Create a new texture and get its infos.
  TexturePtr tex0 = scene.CreateTexture();
  tex0->GetSampler()->SetMinFilter(Sampler::kLinear);
  tex0->GetSampler()->SetMagFilter(Sampler::kLinear);
  tex0->GetSampler()->SetWrapS(Sampler::kClampToEdge);
  tex0->GetSampler()->SetWrapT(Sampler::kRepeat);
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      tex0, std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  TextureInfo expected1 = expected0;
  expected1.id = 3;
  expected1.unit = GL_TEXTURE2;
  expected1.min_filter = GL_LINEAR;
  expected1.mag_filter = GL_LINEAR;
  expected1.wrap_s = GL_CLAMP_TO_EDGE;
  expected1.wrap_t = GL_REPEAT;
  expected1.target = GL_TEXTURE_2D;
  EXPECT_TRUE(VerifyTextureInfo(expected1, callback.infos[0]));
  callback.Reset();

  // Create a new multisampled texture and get its infos.
  TexturePtr tex1 = scene.CreateTexture();
  tex1->SetMultisampling(4, false);
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      tex1, std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  TextureInfo expected2 = expected0;
  expected2.fixed_sample_locations = GL_FALSE;
  expected2.id = 4;
  expected2.unit = GL_TEXTURE3;
  expected2.samples = 4;
  expected2.target = GL_TEXTURE_2D_MULTISAMPLE;
  EXPECT_TRUE(VerifyTextureInfo(expected2, callback.infos[0]));
  callback.Reset();

  // Get all textures. This will bind the textures to new units to avoid
  // modifying uniform bindings.
  manager->RequestAllResourceInfos<TextureBase, TextureInfo>(
      std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(4U, callback.infos.size());
  EXPECT_TRUE(VerifyTextureInfo(expected0, callback.infos[0]));
  EXPECT_TRUE(VerifyTextureInfo(expected_cube, callback.infos[1]));
  EXPECT_TRUE(VerifyTextureInfo(expected1, callback.infos[2]));
  EXPECT_TRUE(VerifyTextureInfo(expected2, callback.infos[3]));
}

TEST_F(ResourceManagerTest, GetTextureInfoWithSamplers) {
  // With samplers, only some texture state will be modified.
  typedef ResourceManager::TextureInfo TextureInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();
  NodePtr child = root->GetChildren()[0];
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<TextureInfo> callback;

  // Get info on the texture.
  const size_t tex_index = child->GetUniformIndex("uTex");
  TexturePtr texture = child->GetUniforms()[tex_index].GetValue<TexturePtr>();
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      texture, std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                         std::placeholders::_1));
  DrawScene(root);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  TextureInfo expected0;
  expected0.label = "Texture";
  expected0.id = 2;
  expected0.sampler = 1;
  expected0.unit = GL_TEXTURE1;
  expected0.width = 2;
  expected0.height = 2;
  expected0.format = Image::kRgb888;
  expected0.base_level = 10;
  expected0.max_level = 100;
  expected0.swizzle_r = GL_ALPHA;
  expected0.swizzle_g = GL_BLUE;
  expected0.swizzle_b = GL_GREEN;
  expected0.swizzle_a = GL_RED;
  expected0.target = GL_TEXTURE_2D;
  EXPECT_TRUE(VerifyTextureInfo(expected0, callback.infos[0]));
  callback.Reset();

  // Get info on the cube_map texture.
  const size_t cube_map_index = child->GetUniformIndex("uCubeMapTex");
  CubeMapTexturePtr cube_map =
      child->GetUniforms()[cube_map_index].GetValue<CubeMapTexturePtr>();
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      cube_map, std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                         std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  TextureInfo expected_cube = expected0;
  expected_cube.label = "Cubemap";
  expected_cube.id = 1;
  expected_cube.sampler = 2;
  expected_cube.unit = GL_TEXTURE0;
  expected_cube.target = GL_TEXTURE_CUBE_MAP;
  EXPECT_TRUE(VerifyTextureInfo(expected_cube, callback.infos[0]));
  callback.Reset();

  // Create a new texture and get its infos.
  TexturePtr tex = scene.CreateTexture();
  tex->GetSampler()->SetMinFilter(Sampler::kLinear);
  tex->GetSampler()->SetMagFilter(Sampler::kLinear);
  tex->GetSampler()->SetWrapS(Sampler::kClampToEdge);
  tex->GetSampler()->SetWrapT(Sampler::kRepeat);
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      tex, std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                     std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  TextureInfo expected1 = expected0;
  expected1.id = 7;
  expected1.sampler = 7;
  expected1.unit = GL_TEXTURE6;
  expected1.target = GL_TEXTURE_2D;
  EXPECT_TRUE(VerifyTextureInfo(expected1, callback.infos[0]));
  callback.Reset();

  // Get all textures. This will bind the textures to new units to avoid
  // modifying uniform bindings.
  manager->RequestAllResourceInfos<TextureBase, TextureInfo>(
      std::bind(&CallbackHelper<TextureInfo>::Callback, &callback,
                std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(7U, callback.infos.size());
  EXPECT_TRUE(VerifyTextureInfo(expected_cube, callback.infos[0]));
  EXPECT_TRUE(VerifyTextureInfo(expected0, callback.infos[1]));
  EXPECT_TRUE(VerifyTextureInfo(expected1, callback.infos[6]));
  callback.Reset();
}

TEST_F(ResourceManagerTest, GetTextureData) {
  typedef ResourceManager::TextureImageInfo TextureImageInfo;
  typedef ResourceManager::TextureInfo TextureInfo;

  testing::TestScene scene;
  NodePtr root = scene.GetScene();

  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<TextureImageInfo> callback;

  // Get the texture's image.
  manager->RequestTextureImage(
      1U, std::bind(&CallbackHelper<TextureImageInfo>::Callback, &callback,
                    std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  // There should be no images since no resources have been created; the scene
  // has not been drawn yet.
  EXPECT_TRUE(callback.infos[0].images.empty());
  callback.Reset();

  // Make another request for an invalid texture.
  manager->RequestTextureImage(
      10U, std::bind(&CallbackHelper<TextureImageInfo>::Callback, &callback,
                     std::placeholders::_1));
  // Draw the scene to create resources.
  DrawScene(root);
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_TRUE(callback.infos[0].images.empty());

  // Make a request for a valid texture.
  manager->RequestTextureImage(
      2U, std::bind(&CallbackHelper<TextureImageInfo>::Callback, &callback,
                    std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ(1U, callback.infos[0].images.size());
  EXPECT_TRUE(callback.infos[0].images[0]);
  EXPECT_EQ(2U, callback.infos[0].images[0]->GetWidth());
  EXPECT_EQ(2U, callback.infos[0].images[0]->GetHeight());
  EXPECT_EQ(Image::kRgb888, callback.infos[0].images[0]->GetFormat());
  callback.Reset();

  // Create a mipmap texture.
  TexturePtr mipmap = scene.CreateTexture();
  ImagePtr image(new Image);
  static const uint8 kPixels[2 * 2 * 3] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b };
  image->Set(Image::kRgb888, 2, 2,
             base::DataContainer::CreateAndCopy<uint8>(kPixels, 12, false,
                                                       image->GetAllocator()));
  mipmap->SetImage(0U, image);

  CallbackHelper<TextureInfo> texture_callback;
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      mipmap, std::bind(&CallbackHelper<TextureInfo>::Callback,
                        &texture_callback, std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(texture_callback.was_called);
  EXPECT_FALSE(texture_callback.infos.empty());

  manager->RequestTextureImage(
      texture_callback.infos[0].id,
      std::bind(&CallbackHelper<TextureImageInfo>::Callback, &callback,
                std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ(1U, callback.infos[0].images.size());
  EXPECT_TRUE(callback.infos[0].images[0]);
  EXPECT_EQ(2U, callback.infos[0].images[0]->GetWidth());
  EXPECT_EQ(2U, callback.infos[0].images[0]->GetHeight());
  EXPECT_EQ(Image::kRgb888, callback.infos[0].images[0]->GetFormat());

  // Request a cube map image.
  manager->RequestTextureImage(
      1U, std::bind(&CallbackHelper<TextureImageInfo>::Callback, &callback,
                    std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ(6U, callback.infos[0].images.size());
  for (int i = 0; i < 6; ++i) {
    EXPECT_TRUE(callback.infos[0].images[i]);
    EXPECT_EQ(2U, callback.infos[0].images[i]->GetWidth());
    EXPECT_EQ(2U, callback.infos[0].images[i]->GetHeight());
    EXPECT_EQ(Image::kRgb888, callback.infos[0].images[i]->GetFormat());
  }
  callback.Reset();

  // Request a cube map mipmap image.
  CubeMapTexturePtr cube_mipmap = scene.CreateCubeMapTexture();
  for (int i = 0; i < 6; ++i)
    cube_mipmap->SetImage(
        static_cast<CubeMapTexture::CubeFace>(i), 0U, image);
  texture_callback.Reset();
  manager->RequestResourceInfo<TextureBase, TextureInfo>(
      cube_mipmap, std::bind(&CallbackHelper<TextureInfo>::Callback,
                             &texture_callback, std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(texture_callback.was_called);
  EXPECT_FALSE(texture_callback.infos.empty());

  manager->RequestTextureImage(
      texture_callback.infos[0].id,
      std::bind(&CallbackHelper<TextureImageInfo>::Callback, &callback,
                std::placeholders::_1));
  renderer_->ProcessResourceInfoRequests();
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  EXPECT_EQ(6U, callback.infos[0].images.size());
  for (int i = 0; i < 6; ++i) {
    EXPECT_TRUE(callback.infos[0].images[i]);
    EXPECT_EQ(2U, callback.infos[0].images[i]->GetWidth());
    EXPECT_EQ(2U, callback.infos[0].images[i]->GetHeight());
    EXPECT_EQ(Image::kRgb888, callback.infos[0].images[i]->GetFormat());
  }
  callback.Reset();
}

TEST_F(ResourceManagerTest, GetTransformFeedbackInfo) {
  if (!gm_->IsFeatureAvailable(GraphicsManager::kTransformFeedback)) {
    return;
  }
  typedef ResourceManager::TransformFeedbackInfo TransformFeedbackInfo;

  // Create a buffer object to capture vertex data.
  BufferObjectPtr buffer(new BufferObject);
  const size_t vert_count = 4;
  using ion::math::Vector4f;
  Vector4f* verts = new Vector4f[vert_count];
  ion::base::DataContainerPtr container =
      ion::base::DataContainer::Create<Vector4f>(
          verts, ion::base::DataContainer::ArrayDeleter<Vector4f>,
          true, buffer->GetAllocator());
  buffer->SetData(container, sizeof(verts[0]), vert_count,
                  ion::gfx::BufferObject::kStreamDraw);

  // Construct a simplified scene that includes captured varyings.
  testing::TestScene scene(true);
  ResourceManager* manager = renderer_->GetResourceManager();
  CallbackHelper<TransformFeedbackInfo> callback;
  ion::gfx::TransformFeedbackPtr tfo(new ion::gfx::TransformFeedback(buffer));
  manager->RequestResourceInfo<TransformFeedback, TransformFeedbackInfo>(
      tfo, std::bind(&CallbackHelper<TransformFeedbackInfo>::Callback,
                     &callback,
                     std::placeholders::_1));

  // Render the scene with transform feedback active.
  renderer_->BeginTransformFeedback(tfo);
  DrawScene(scene.GetScene());
  EXPECT_TRUE(callback.was_called);
  EXPECT_EQ(1U, callback.infos.size());
  GLuint bufid = renderer_->GetResourceGlId(buffer.Get());
  EXPECT_TRUE(VerifyTransformFeedbackInfo(
             callback.infos[0], bufid, true, false));
}

}  // namespace gfx
}  // namespace ion
