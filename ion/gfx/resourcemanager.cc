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

#include <algorithm>
#include <memory>

#include "base/integral_types.h"
#include "ion/base/scopedallocation.h"
#include "ion/base/stringutils.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/sampler.h"
#include "ion/gfx/shader.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/texture.h"
#include "ion/math/matrixutils.h"

namespace ion {
namespace gfx {

namespace {

//---------------------------------------------------------------------------
//
// ArrayInfo helper function.
//
//---------------------------------------------------------------------------
static void FillArrayInfo(const GraphicsManagerPtr& gm,
                          ResourceManager::ArrayInfo* info) {
  // Get the number of possible attributes.
  GLint attrib_count;
  gm->GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attrib_count);
  // Create an entry for each attributes.
  info->attributes.resize(attrib_count);
  GLint boolean_value = GL_FALSE;
  for (GLint i = 0; i < attrib_count; ++i) {
    gm->GetVertexAttribiv(
        static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
        reinterpret_cast<GLint*>(&info->attributes[i].buffer));
    gm->GetVertexAttribiv(
        static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_ENABLED, &boolean_value);
    info->attributes[i].enabled = static_cast<GLboolean>(boolean_value);
    gm->GetVertexAttribiv(
        static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_SIZE,
        reinterpret_cast<GLint*>(&info->attributes[i].size));
    gm->GetVertexAttribiv(
        static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_STRIDE,
        reinterpret_cast<GLint*>(&info->attributes[i].stride));
    gm->GetVertexAttribiv(
        static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_TYPE,
        reinterpret_cast<GLint*>(&info->attributes[i].type));
    gm->GetVertexAttribiv(
        static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,
        &boolean_value);
    info->attributes[i].normalized = static_cast<GLboolean>(boolean_value);
    gm->GetVertexAttribfv(
        static_cast<GLuint>(i), GL_CURRENT_VERTEX_ATTRIB,
        &info->attributes[i].value[0]);
    gm->GetVertexAttribPointerv(
        static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_POINTER,
        &info->attributes[i].pointer);
    if (gm->IsFeatureAvailable(GraphicsManager::kInstancedArrays)) {
      gm->GetVertexAttribiv(
          static_cast<GLuint>(i), GL_VERTEX_ATTRIB_ARRAY_DIVISOR,
          reinterpret_cast<GLint*>(&info->attributes[i].divisor));
    }
  }
}

//---------------------------------------------------------------------------
//
// BufferInfo helper function.
//
//---------------------------------------------------------------------------
static void FillBufferInfo(const GraphicsManagerPtr& gm,
                           ResourceManager::BufferInfo* info) {
  // GLsizeiptr is the size of a pointer, which is not always the size of an
  // int.
  GLint size = 0;
  gm->GetBufferParameteriv(info->target, GL_BUFFER_SIZE, &size);
  info->size = size;
  gm->GetBufferParameteriv(
      info->target, GL_BUFFER_USAGE, reinterpret_cast<GLint*>(&info->usage));
  if (gm->IsFeatureAvailable(GraphicsManager::kMapBufferBase)) {
    void* data = nullptr;
    gm->GetBufferPointerv(info->target, GL_BUFFER_MAP_POINTER, &data);
    info->mapped_data = data;
  }
}

//---------------------------------------------------------------------------
//
// FramebufferInfo helper functions.
//
//---------------------------------------------------------------------------
static void FillFramebufferAttachmentInfo(
    const GraphicsManagerPtr& gm,
    ResourceManager::FramebufferInfo::Attachment* info,
    ResourceManager::RenderbufferInfo* rb_info,
    GLenum attachment) {
  gm->GetFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
      reinterpret_cast<GLint*>(&info->type));

  // On Nexus 6, the implementation returns GL_RENDERBUFFER when it should
  // return GL_NONE. This can be detected by the fact that ID is 0.
  // See http://b/21437493
  if ((info->type == GL_RENDERBUFFER) && (rb_info->id == 0U)) {
    info->type = GL_NONE;
  }

  if (info->type != GL_NONE) {
    gm->GetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
        reinterpret_cast<GLint*>(&info->value));
    DCHECK(info->type != GL_RENDERBUFFER || info->value == rb_info->id);
  }
  if (info->type == GL_TEXTURE) {
    gm->GetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
        reinterpret_cast<GLint*>(&info->level));
    gm->GetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, attachment,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
        reinterpret_cast<GLint*>(&info->cube_face));
    if (gm->IsFeatureAvailable(GraphicsManager::kMultiview)) {
      gm->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR, &info->layer);
      gm->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR,
          reinterpret_cast<GLint*>(&info->num_views));
    }
    // We test this after multiview, because a valid multiview attachment will
    // always have nonzero num_views. A layer attachment may legitimately have
    // the layer set to zero.
    if (gm->IsFeatureAvailable(GraphicsManager::kFramebufferTextureLayer) &&
        info->num_views <= 0) {
      gm->GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER,
          &info->layer);
    }
    if (gm->IsFeatureAvailable(GraphicsManager::kImplicitMultisample)) {
      gm->GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT,
          reinterpret_cast<GLint*>(&info->texture_samples));
    }
  }

  // If the attachment is a renderbuffer then fill its info.
  if (info->type == GL_RENDERBUFFER) {
    gm->BindRenderbuffer(GL_RENDERBUFFER, rb_info->id);
    gm->GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,
                                   reinterpret_cast<GLint*>(&rb_info->width));
    gm->GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT,
                                   reinterpret_cast<GLint*>(&rb_info->height));
    gm->GetRenderbufferParameteriv(
        GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT,
        reinterpret_cast<GLint*>(&rb_info->internal_format));
    gm->GetRenderbufferParameteriv(
        GL_RENDERBUFFER, GL_RENDERBUFFER_RED_SIZE,
        reinterpret_cast<GLint*>(&rb_info->red_size));
    gm->GetRenderbufferParameteriv(
        GL_RENDERBUFFER, GL_RENDERBUFFER_GREEN_SIZE,
        reinterpret_cast<GLint*>(&rb_info->green_size));
    gm->GetRenderbufferParameteriv(
        GL_RENDERBUFFER, GL_RENDERBUFFER_BLUE_SIZE,
        reinterpret_cast<GLint*>(&rb_info->blue_size));
    gm->GetRenderbufferParameteriv(
        GL_RENDERBUFFER, GL_RENDERBUFFER_ALPHA_SIZE,
        reinterpret_cast<GLint*>(&rb_info->alpha_size));
    gm->GetRenderbufferParameteriv(
        GL_RENDERBUFFER, GL_RENDERBUFFER_DEPTH_SIZE,
        reinterpret_cast<GLint*>(&rb_info->depth_size));
    gm->GetRenderbufferParameteriv(
        GL_RENDERBUFFER, GL_RENDERBUFFER_STENCIL_SIZE,
        reinterpret_cast<GLint*>(&rb_info->stencil_size));
  }
}

static void FillFramebufferInfo(const GraphicsManagerPtr& gm,
                                ResourceManager::FramebufferInfo* info) {
  // Fill each attachment's info.
  // The vectors storing color attachment information in FramebufferInfo must
  // be resized by the caller, since it also has to provide renderbuffer ID
  // information.
  for (uint32 i = 0; i < info->color.size(); ++i) {
    FillFramebufferAttachmentInfo(gm, &info->color[i],
        &info->color_renderbuffers[i], GL_COLOR_ATTACHMENT0 + i);
  }
  FillFramebufferAttachmentInfo(
      gm, &info->depth, &info->depth_renderbuffer, GL_DEPTH_ATTACHMENT);
  FillFramebufferAttachmentInfo(
      gm, &info->stencil, &info->stencil_renderbuffer, GL_STENCIL_ATTACHMENT);

  // Fill draw buffer information.
  if (gm->IsFeatureAvailable(GraphicsManager::kDrawBuffers)) {
    const int max_draw_buffers = gm->GetConstant<int>(
        GraphicsManager::kMaxDrawBuffers);
    info->draw_buffers.resize(max_draw_buffers);
    for (int i = 0; i < max_draw_buffers; ++i) {
      gm->GetIntegerv(GL_DRAW_BUFFER0 + i,
                      reinterpret_cast<GLint*>(&info->draw_buffers[i]));
    }
  } else if (gm->IsFeatureAvailable(GraphicsManager::kDrawBuffer)) {
    gm->GetIntegerv(GL_DRAW_BUFFER,
                    reinterpret_cast<GLint*>(&info->draw_buffers[0]));
  } else {
    info->draw_buffers[0] = info->id ? GL_COLOR_ATTACHMENT0 : GL_BACK;
  }

  // Fill read buffer information.
  if (gm->IsFeatureAvailable(GraphicsManager::kReadBuffer)) {
    gm->GetIntegerv(GL_READ_BUFFER,
                    reinterpret_cast<GLint*>(&info->read_buffer));
  } else {
    info->read_buffer = info->id ? GL_COLOR_ATTACHMENT0 : GL_BACK;
  }
}

//---------------------------------------------------------------------------
//
// SamplerInfo helper function.
//
//---------------------------------------------------------------------------
static void FillSamplerInfo(const GraphicsManagerPtr& gm,
                           ResourceManager::SamplerInfo* info) {
  if (gm->IsFeatureAvailable(GraphicsManager::kSamplerObjects)) {
    if (gm->IsFeatureAvailable(GraphicsManager::kShadowSamplers)) {
      gm->GetSamplerParameteriv(info->id, GL_TEXTURE_COMPARE_FUNC,
                                reinterpret_cast<GLint*>(&info->compare_func));
      gm->GetSamplerParameteriv(info->id, GL_TEXTURE_COMPARE_MODE,
                                reinterpret_cast<GLint*>(&info->compare_mode));
    }
    if (gm->IsFeatureAvailable(GraphicsManager::kTextureFilterAnisotropic)) {
      gm->GetSamplerParameterfv(
          info->id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
          reinterpret_cast<GLfloat*>(&info->max_anisotropy));
    }
    gm->GetSamplerParameteriv(info->id, GL_TEXTURE_MAG_FILTER,
                              reinterpret_cast<GLint*>(&info->mag_filter));
    gm->GetSamplerParameterfv(info->id, GL_TEXTURE_MAX_LOD,
                              reinterpret_cast<GLfloat*>(&info->max_lod));
    gm->GetSamplerParameteriv(info->id, GL_TEXTURE_MIN_FILTER,
                              reinterpret_cast<GLint*>(&info->min_filter));
    gm->GetSamplerParameterfv(info->id, GL_TEXTURE_MIN_LOD,
                              reinterpret_cast<GLfloat*>(&info->min_lod));
    gm->GetSamplerParameteriv(
        info->id, GL_TEXTURE_WRAP_R, reinterpret_cast<GLint*>(&info->wrap_r));
    gm->GetSamplerParameteriv(
        info->id, GL_TEXTURE_WRAP_S, reinterpret_cast<GLint*>(&info->wrap_s));
    gm->GetSamplerParameteriv(
        info->id, GL_TEXTURE_WRAP_T, reinterpret_cast<GLint*>(&info->wrap_t));
  }
}

//---------------------------------------------------------------------------
//
// ShaderInfo helper function.
//
//---------------------------------------------------------------------------
static void FillShaderInfo(const GraphicsManagerPtr& gm,
                           ResourceManager::ShaderInfo* info) {
  GLint boolean_value = GL_FALSE;
  gm->GetShaderiv(
      info->id, GL_SHADER_TYPE, reinterpret_cast<GLint*>(&info->type));
  gm->GetShaderiv(info->id, GL_DELETE_STATUS, &boolean_value);
  info->delete_status = static_cast<GLboolean>(boolean_value);
  gm->GetShaderiv(info->id, GL_COMPILE_STATUS, &boolean_value);
  info->compile_status = static_cast<GLboolean>(boolean_value);

  // Get the shader source.
  GLint length = 0;
  gm->GetShaderiv(info->id, GL_SHADER_SOURCE_LENGTH, &length);
  length = std::max(1, length);
  {
    base::ScopedAllocation<char> buffer(base::kShortTerm, length);
    buffer.Get()[0] = 0;
    gm->GetShaderSource(info->id, length, &length, buffer.Get());
    info->source = buffer.Get();
  }

  // Get the info log.
  length = 0;
  gm->GetShaderiv(info->id, GL_INFO_LOG_LENGTH, &length);
  length = std::max(1, length);
  {
    base::ScopedAllocation<char> buffer(base::kShortTerm, length);
    buffer.Get()[0] = 0;
    gm->GetShaderInfoLog(info->id, length, &length, buffer.Get());
    info->info_log = buffer.Get();
  }
}

//---------------------------------------------------------------------------
//
// ProgramInfo helper functions.
//
//---------------------------------------------------------------------------
template <typename T>
void FillShaderInputs(
    const GraphicsManagerPtr& gm, GLuint id, GLenum active_enum,
    GLenum length_enum,
    const std::function<void(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*,
                             GLchar*)>& GetInput,
    const std::function<GLint(GLuint, const GLchar*)>& GetLocation,
    std::vector<T>* infos) {
  GLint count = 0;
  gm->GetProgramiv(id, active_enum, &count);
  infos->resize(count);
  if (count) {
    char name[2048];
    // Get all attributes.
    for (GLint i = 0; i < count; ++i) {
      // Get the attribute information from OpenGL.
      GLsizei length = 0;
      name[0] = 0;
      GetInput(id, static_cast<GLuint>(i), 2047, &length, &(*infos)[i].size,
               &(*infos)[i].type, name);
      (*infos)[i].name = name;
      (*infos)[i].index = GetLocation(id, name);
      if ((*infos)[i].size > 1) {
        for (GLint j = 0; j < (*infos)[i].size; ++j) {
          std::ostringstream str;
          str << name << "[" << j << "]";
          const std::string array_name = str.str();
          (*infos)[i].array_indices.push_back(
              GetLocation(id, array_name.c_str()));
        }
      }
    }
  }
}

template <typename T>
static void FillUniformValue(T* values,
                             ResourceManager::ProgramInfo::Uniform* uniform,
                             const base::AllocatorPtr& allocator) {
  if (uniform->size > 1) {
    uniform->value.InitArray<T>(allocator, uniform->size);
    for (GLint i = 0; i < uniform->size; i++)
      uniform->value.SetValueAt(i, values[i]);
  } else {
    uniform->value.Set(values[0]);
  }
}

template <int Dimension, typename T>
static void FillUniformValue(math::Matrix<Dimension, T>* values,
                             ResourceManager::ProgramInfo::Uniform* uniform,
                             const base::AllocatorPtr& allocator) {
  if (uniform->size > 1) {
    uniform->value.InitArray<math::Matrix<Dimension, T>>(allocator,
                                                         uniform->size);
    for (GLint i = 0; i < uniform->size; i++)
      uniform->value.SetValueAt(i, math::Transpose(values[i]));
  } else {
    uniform->value.Set(math::Transpose(values[0]));
  }
}

// Retrieves the full set of values for a uniform. The stride is the size of one
// of the fully typed elements (e.g., a vector or scalar), that is, the size of
// the type T used in Fill*UniformValue().
template <typename T>
static void GetGlUniformValue(const GraphicsManagerPtr& gm, GLuint id,
                              GLuint stride,
                              ResourceManager::ProgramInfo::Uniform* uniform,
                              void (GraphicsManager::*Getv)(GLuint, GLint, T*),
                              T* gl_values) {
  // Retrieve each element of the uniform if it is an array.
  if (uniform->size == 1) {
    ((*gm).*Getv)(id, uniform->index, gl_values);
  } else {
    uint8* ptr = reinterpret_cast<uint8*>(gl_values);
    for (GLint i = 0; i < uniform->size; ++i) {
      T* values = reinterpret_cast<T*>(&ptr[i * stride]);
      ((*gm).*Getv)(id, uniform->array_indices[i], values);
    }
  }
}

template <typename T>
static void FillFloatUniformValue(
    const GraphicsManagerPtr& gm, GLuint id,
    ResourceManager::ProgramInfo::Uniform* uniform, T* values,
    GLfloat* gl_values, const base::AllocatorPtr& allocator) {
  // Retrieve each element of the uniform if it is an array.
  GetGlUniformValue(gm, id, sizeof(T), uniform, &GraphicsManager::GetUniformfv,
                    gl_values);
  FillUniformValue(values, uniform, allocator);
}

template <typename T>
static void FillIntUniformValue(const GraphicsManagerPtr& gm, GLuint id,
                                ResourceManager::ProgramInfo::Uniform* uniform,
                                T* values, GLint* gl_values,
                                const base::AllocatorPtr& allocator) {
  GetGlUniformValue(gm, id, sizeof(T), uniform, &GraphicsManager::GetUniformiv,
                    gl_values);
  FillUniformValue(values, uniform, allocator);
}

template <typename T>
static void FillUintUniformValue(const GraphicsManagerPtr& gm, GLuint id,
                                 ResourceManager::ProgramInfo::Uniform* uniform,
                                 T* values, GLuint* gl_values,
                                 const base::AllocatorPtr& allocator) {
  GetGlUniformValue(gm, id, sizeof(T), uniform, &GraphicsManager::GetUniformuiv,
                    gl_values);
  FillUniformValue(values, uniform, allocator);
}

static void FillUniformValues(
    const GraphicsManagerPtr& gm, GLuint id,
    std::vector<ResourceManager::ProgramInfo::Uniform>* uniforms) {
  base::AllocatorPtr allocator =
      base::AllocationManager::GetDefaultAllocatorForLifetime(
          base::kMediumTerm);
  const size_t count = uniforms->size();
  for (size_t i = 0; i < count; ++i) {
    ResourceManager::ProgramInfo::Uniform& u = (*uniforms)[i];
    switch (u.type) {
      case GL_FLOAT: {
        base::ScopedAllocation<float> value(allocator, u.size);
        FillFloatUniformValue<float>(gm, id, &u, value.Get(), value.Get(),
                                     allocator);
        break;
      }
      case GL_FLOAT_VEC2: {
        base::ScopedAllocation<math::Vector2f> value(allocator, u.size);
        FillFloatUniformValue<math::Vector2f>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0], allocator);
        break;
      }
      case GL_FLOAT_VEC3: {
        base::ScopedAllocation<math::Vector3f> value(allocator, u.size);
        FillFloatUniformValue<math::Vector3f>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0], allocator);
        break;
      }
      case GL_FLOAT_VEC4: {
        base::ScopedAllocation<math::Vector4f> value(allocator, u.size);
        FillFloatUniformValue<math::Vector4f>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0], allocator);
        break;
      }
      // A sampler is stored as in int in OpenGL.
      case GL_INT:
      case GL_INT_SAMPLER_1D:
      case GL_INT_SAMPLER_1D_ARRAY:
      case GL_INT_SAMPLER_2D:
      case GL_INT_SAMPLER_2D_ARRAY:
      case GL_INT_SAMPLER_3D:
      case GL_INT_SAMPLER_CUBE:
      case GL_INT_SAMPLER_CUBE_MAP_ARRAY:
      case GL_SAMPLER_1D:
      case GL_SAMPLER_1D_ARRAY:
      case GL_SAMPLER_1D_ARRAY_SHADOW:
      case GL_SAMPLER_1D_SHADOW:
      case GL_SAMPLER_2D:
      case GL_SAMPLER_2D_ARRAY:
      case GL_SAMPLER_2D_ARRAY_SHADOW:
      case GL_SAMPLER_2D_MULTISAMPLE:
      case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
      case GL_SAMPLER_2D_SHADOW:
      case GL_SAMPLER_3D:
      case GL_SAMPLER_CUBE:
      case GL_SAMPLER_CUBE_MAP_ARRAY:
      case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:
      case GL_SAMPLER_CUBE_SHADOW:
      case GL_SAMPLER_EXTERNAL_OES:
      case GL_UNSIGNED_INT_SAMPLER_1D:
      case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_2D:
      case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      case GL_UNSIGNED_INT_SAMPLER_3D:
      case GL_UNSIGNED_INT_SAMPLER_CUBE:
      case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY: {
        base::ScopedAllocation<int> value(allocator, u.size);
        FillIntUniformValue<int>(gm, id, &u, value.Get(), value.Get(),
                                 allocator);
        break;
      }
      case GL_INT_VEC2: {
        base::ScopedAllocation<math::Vector2i> value(allocator, u.size);
        FillIntUniformValue<math::Vector2i>(gm, id, &u, value.Get(),
                                            &(*value.Get())[0], allocator);
        break;
      }
      case GL_INT_VEC3: {
        base::ScopedAllocation<math::Vector3i> value(allocator, u.size);
        FillIntUniformValue<math::Vector3i>(gm, id, &u, value.Get(),
                                            &(*value.Get())[0], allocator);
        break;
      }
      case GL_INT_VEC4: {
        base::ScopedAllocation<math::Vector4i> value(allocator, u.size);
        FillIntUniformValue<math::Vector4i>(gm, id, &u, value.Get(),
                                            &(*value.Get())[0], allocator);
        break;
      }
      case GL_UNSIGNED_INT: {
        base::ScopedAllocation<uint32> value(allocator, u.size);
        FillUintUniformValue<uint32>(gm, id, &u, value.Get(), value.Get(),
                                     allocator);
        break;
      }
      case GL_UNSIGNED_INT_VEC2: {
        base::ScopedAllocation<math::Vector2ui> value(allocator, u.size);
        FillUintUniformValue<math::Vector2ui>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0], allocator);
        break;
      }
      case GL_UNSIGNED_INT_VEC3: {
        base::ScopedAllocation<math::Vector3ui> value(allocator, u.size);
        FillUintUniformValue<math::Vector3ui>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0], allocator);
        break;
      }
      case GL_UNSIGNED_INT_VEC4: {
        base::ScopedAllocation<math::Vector4ui> value(allocator, u.size);
        FillUintUniformValue<math::Vector4ui>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0], allocator);
        break;
      }
      case GL_FLOAT_MAT2: {
        base::ScopedAllocation<math::Matrix2f> value(allocator, u.size);
        FillFloatUniformValue<math::Matrix2f>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0][0], allocator);
        break;
      }
      case GL_FLOAT_MAT3: {
        base::ScopedAllocation<math::Matrix3f> value(allocator, u.size);
        FillFloatUniformValue<math::Matrix3f>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0][0], allocator);
        break;
      }
      case GL_FLOAT_MAT4: {
        base::ScopedAllocation<math::Matrix4f> value(allocator, u.size);
        FillFloatUniformValue<math::Matrix4f>(gm, id, &u, value.Get(),
                                              &(*value.Get())[0][0], allocator);
        break;
      }
#if !defined(ION_COVERAGE)  // COV_NF_START
      default:
        break;
#endif  // COV_NF_END
    }
  }
}

static void FillProgramInfo(const GraphicsManagerPtr& gm,
                            ResourceManager::ProgramInfo* info) {
  using std::bind;
  using std::placeholders::_1;
  using std::placeholders::_2;
  using std::placeholders::_3;
  using std::placeholders::_4;
  using std::placeholders::_5;
  using std::placeholders::_6;
  using std::placeholders::_7;

  // Get the status of the program.
  GLint boolean_value = GL_FALSE;
  gm->GetProgramiv(info->id, GL_DELETE_STATUS, &boolean_value);
  info->delete_status = static_cast<GLboolean>(boolean_value);
  gm->GetProgramiv(info->id, GL_LINK_STATUS, &boolean_value);
  info->link_status = static_cast<GLboolean>(boolean_value);
  gm->GetProgramiv(info->id, GL_VALIDATE_STATUS, &boolean_value);
  info->validate_status = static_cast<GLboolean>(boolean_value);

  // Get the info log.
  GLint info_log_length = 0;
  gm->GetProgramiv(info->id, GL_INFO_LOG_LENGTH, &info_log_length);
  info_log_length = std::max(1, info_log_length);
  {
    base::ScopedAllocation<char> buffer(base::kShortTerm, info_log_length);
    buffer.Get()[0] = 0;
    gm->GetProgramInfoLog(
        info->id, info_log_length, &info_log_length, buffer.Get());
    info->info_log = buffer.Get();
  }
  info->info_log.resize(info_log_length);

  // Get attribute information.
  FillShaderInputs(
      gm, info->id, GL_ACTIVE_ATTRIBUTES, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,
      bind(&GraphicsManager::GetActiveAttrib, gm.Get(),
           _1, _2, _3, _4, _5, _6, _7),
      bind(&GraphicsManager::GetAttribLocation, gm.Get(), _1, _2),
      &info->attributes);
  // Get uniform information.
  FillShaderInputs(
      gm, info->id, GL_ACTIVE_UNIFORMS, GL_ACTIVE_UNIFORM_MAX_LENGTH,
      bind(&GraphicsManager::GetActiveUniform, gm.Get(),
           _1, _2, _3, _4, _5, _6, _7),
      bind(&GraphicsManager::GetUniformLocation, gm.Get(), _1, _2),
      &info->uniforms);
  // Get uniform values.
  FillUniformValues(gm, info->id, &info->uniforms);
}

//---------------------------------------------------------------------------
//
// PlatformInfo helper functions.
//
//---------------------------------------------------------------------------
static void FillStringsAndVersions(const GraphicsManagerPtr& gm,
                                   ResourceManager::PlatformInfo* info) {
  info->renderer = reinterpret_cast<const char*>(gm->GetString(GL_RENDERER));
  info->vendor = reinterpret_cast<const char*>(gm->GetString(GL_VENDOR));
  info->version_string =
      reinterpret_cast<const char*>(gm->GetString(GL_VERSION));

  const size_t dot_pos = info->version_string.find('.');
  info->major_version = 0;
  info->minor_version = 0;
  info->glsl_version = 0;
  if (dot_pos != std::string::npos && dot_pos > 0 &&
      dot_pos < info->version_string.length() - 1) {
    info->major_version = info->version_string[dot_pos - 1] - '0';
    info->minor_version = info->version_string[dot_pos + 1] - '0';
  }
  const char* glsl_version_string =
      reinterpret_cast<const char*>(gm->GetString(GL_SHADING_LANGUAGE_VERSION));
  const std::vector<std::string> strings = base::SplitString(
      glsl_version_string, " ");
  const size_t count = strings.size();
  for (size_t i = 0; i < count; ++i) {
    size_t pos = strings[i].find(".");
    if (pos != std::string::npos) {
      const std::vector<std::string> numbers = base::SplitString(strings[i],
                                                                 ".");
      if (numbers.size() == 2)
        info->glsl_version = 100 * base::StringToInt32(numbers[0]) +
                             base::StringToInt32(numbers[1]);
    }
  }

  if (const char* extensions =
          reinterpret_cast<const char*>(gm->GetString(GL_EXTENSIONS))) {
    info->extensions = extensions;
  }
}

static void FillPlatformInfo(const GraphicsManagerPtr& gm,
                             ResourceManager::PlatformInfo* info) {
#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init) \
  info->sname = gm->GetConstant<Type>(GraphicsManager::k##name);
#include "ion/gfx/glconstants.inc"

  // If we are running on desktop GL, query a different value for point size.
  if (gm->GetGlFlavor() == GraphicsManager::kDesktop &&
      gm->GetGlVersion() >= 30) {
    GLfloat result[2] = { 0.f, 0.f };
    gm->GetFloatv(GL_POINT_SIZE_RANGE, result);
    info->aliased_point_size_range = math::Range1f(result[0], result[1]);
  }

  FillStringsAndVersions(gm, info);
}

//---------------------------------------------------------------------------
//
// TextureInfo helper function.
//
//---------------------------------------------------------------------------
static void FillTextureInfo(const GraphicsManagerPtr& gm,
                            ResourceManager::TextureInfo* info) {
  // If a sampler is bound then get the info from the sampler object.
  gm->ActiveTexture(info->unit);
  gm->GetTexParameterfv(info->target, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        reinterpret_cast<GLfloat*>(&info->max_anisotropy));
  gm->GetTexParameteriv(info->target, GL_TEXTURE_MAG_FILTER,
                        reinterpret_cast<GLint*>(&info->mag_filter));
  gm->GetTexParameteriv(info->target, GL_TEXTURE_MIN_FILTER,
                        reinterpret_cast<GLint*>(&info->min_filter));
  gm->GetTexParameteriv(info->target, GL_TEXTURE_WRAP_S,
                        reinterpret_cast<GLint*>(&info->wrap_s));
  gm->GetTexParameteriv(info->target, GL_TEXTURE_WRAP_T,
                        reinterpret_cast<GLint*>(&info->wrap_t));
  if (gm->GetGlVersion() > 20) {
    gm->GetIntegerv(GL_SAMPLER_BINDING,
                    reinterpret_cast<GLint*>(&info->sampler));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_BASE_LEVEL,
                          reinterpret_cast<GLint*>(&info->base_level));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_COMPARE_FUNC,
                          reinterpret_cast<GLint*>(&info->compare_func));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_COMPARE_MODE,
                          reinterpret_cast<GLint*>(&info->compare_mode));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_MAX_LEVEL,
                          reinterpret_cast<GLint*>(&info->max_level));
    gm->GetTexParameterfv(info->target, GL_TEXTURE_MAX_LOD,
                          reinterpret_cast<GLfloat*>(&info->max_lod));
    gm->GetTexParameterfv(info->target, GL_TEXTURE_MIN_LOD,
                          reinterpret_cast<GLfloat*>(&info->min_lod));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_SWIZZLE_R,
                          reinterpret_cast<GLint*>(&info->swizzle_r));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_SWIZZLE_G,
                          reinterpret_cast<GLint*>(&info->swizzle_g));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_SWIZZLE_B,
                          reinterpret_cast<GLint*>(&info->swizzle_b));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_SWIZZLE_A,
                          reinterpret_cast<GLint*>(&info->swizzle_a));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_WRAP_R,
                          reinterpret_cast<GLint*>(&info->wrap_r));
  }
  if (gm->IsFeatureAvailable(GraphicsManager::kProtectedTextures)) {
    gm->GetTexParameteriv(info->target, GL_TEXTURE_PROTECTED_EXT,
                          reinterpret_cast<GLint*>(&info->is_protected));
  }
  if (gm->IsFeatureAvailable(GraphicsManager::kTextureMultisample)) {
    gm->GetTexParameteriv(info->target, GL_TEXTURE_SAMPLES,
                          reinterpret_cast<GLint*>(&info->samples));
    gm->GetTexParameteriv(info->target, GL_TEXTURE_FIXED_SAMPLE_LOCATIONS,
                          reinterpret_cast<GLint*>(
                              &info->fixed_sample_locations));
  }
}

//---------------------------------------------------------------------------
//
// TransformFeedbackInfo helper function.
//
//---------------------------------------------------------------------------
static void FillTransformFeedbackInfo(
    const GraphicsManagerPtr& gm,
    ResourceManager::TransformFeedbackInfo* info) {
  if (!gm->IsFeatureAvailable(GraphicsManager::kTransformFeedback)) {
    return;
  }
  gm->GetIntegerv(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,
                  reinterpret_cast<GLint*>(&info->buffer));
  gm->GetBooleanv(GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE, &info->active);
  gm->GetBooleanv(GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED, &info->paused);
  // Memory sanitizers require this to be initialized to zero, because we
  // haven't bothered to implement SEPARATE_ATTRIBS in our mocks yet.
  int nbinding_points = gm->GetConstant<int>(
      GraphicsManager::kMaxTransformFeedbackSeparateAttribs);
  info->streams.resize(std::max(0, nbinding_points));
  for (GLint i = 0; i < nbinding_points; i++) {
    auto& attrib = info->streams[i];
    gm->GetIntegeri_v(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, i,
                      reinterpret_cast<GLint*>(&attrib.buffer));
    gm->GetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_START, i,
                        reinterpret_cast<GLint64*>(&attrib.start));
    gm->GetInteger64i_v(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, i,
                        reinterpret_cast<GLint64*>(&attrib.size));
  }
}

}  // anonymous namespace

ResourceManager::ResourceManager(const GraphicsManagerPtr& gm)
    : graphics_manager_(gm) {
}

ResourceManager::~ResourceManager() {
}

template <> ION_API
std::vector<ResourceManager::ResourceRequest<AttributeArray,
                                             ResourceManager::ArrayInfo> >*
ResourceManager::GetResourceRequestVector<AttributeArray,
                                          ResourceManager::ArrayInfo>() {
  return &array_requests_;
}

template <> ION_API
std::vector<ResourceManager::ResourceRequest<BufferObject,
                                             ResourceManager::BufferInfo> >*
ResourceManager::GetResourceRequestVector<BufferObject,
                                          ResourceManager::BufferInfo>() {
  return &buffer_requests_;
}

template <> ION_API std::vector<
    ResourceManager::ResourceRequest<FramebufferObject,
                                     ResourceManager::FramebufferInfo> >*
ResourceManager::GetResourceRequestVector<FramebufferObject,
                                          ResourceManager::FramebufferInfo>() {
  return &framebuffer_requests_;
}

template <> ION_API
std::vector<ResourceManager::DataRequest<ResourceManager::PlatformInfo> >*
ResourceManager::GetDataRequestVector<ResourceManager::PlatformInfo>() {
  return &platform_requests_;
}

template <> ION_API
std::vector<ResourceManager::ResourceRequest<ShaderProgram,
                                             ResourceManager::ProgramInfo> >*
ResourceManager::GetResourceRequestVector<ShaderProgram,
                                          ResourceManager::ProgramInfo>() {
  return &program_requests_;
}

template <> ION_API std::vector<
    ResourceManager::ResourceRequest<Sampler, ResourceManager::SamplerInfo> >*
ResourceManager::GetResourceRequestVector<Sampler,
                                          ResourceManager::SamplerInfo>() {
  return &sampler_requests_;
}

template <> ION_API std::vector<
    ResourceManager::ResourceRequest<Shader, ResourceManager::ShaderInfo> >*
ResourceManager::GetResourceRequestVector<Shader,
                                          ResourceManager::ShaderInfo>() {
  return &shader_requests_;
}

template <> ION_API
std::vector<ResourceManager::ResourceRequest<TextureBase,
                                             ResourceManager::TextureInfo> >*
ResourceManager::GetResourceRequestVector<TextureBase,
                                          ResourceManager::TextureInfo>() {
  return &texture_requests_;
}

template <> ION_API
std::vector<ResourceManager::DataRequest<ResourceManager::TextureImageInfo> >*
ResourceManager::GetDataRequestVector<ResourceManager::TextureImageInfo>() {
  return &texture_image_requests_;
}

template <>
ION_API std::vector<ResourceManager::ResourceRequest<
    TransformFeedback, ResourceManager::TransformFeedbackInfo>>*
ResourceManager::GetResourceRequestVector<
    TransformFeedback, ResourceManager::TransformFeedbackInfo>() {
  return &transform_feedback_requests_;
}

void ResourceManager::RequestPlatformInfo(
    const InfoCallback<PlatformInfo>::Type& callback) {
  std::lock_guard<std::mutex> lock_guard(this->request_mutex_);
  GetDataRequestVector<PlatformInfo>()->push_back(
      DataRequest<PlatformInfo>(0, callback));
}

void ResourceManager::RequestTextureImage(
    GLuint id, const InfoCallback<TextureImageInfo>::Type& callback) {
  std::lock_guard<std::mutex> lock_guard(this->request_mutex_);
  GetDataRequestVector<TextureImageInfo>()->push_back(
      DataRequest<TextureImageInfo>(id, callback));
}

template <>
void ResourceManager::FillInfoFromOpenGL(ResourceManager::ArrayInfo* info) {
  FillArrayInfo(graphics_manager_, info);
}

template <>
void ResourceManager::FillInfoFromOpenGL(ResourceManager::BufferInfo* info) {
  FillBufferInfo(graphics_manager_, info);
}

template <>
void ResourceManager::FillInfoFromOpenGL(
    ResourceManager::FramebufferInfo* info) {
  FillFramebufferInfo(graphics_manager_, info);
}

template <>
void ResourceManager::FillInfoFromOpenGL(ResourceManager::ProgramInfo* info) {
  FillProgramInfo(graphics_manager_, info);
}

template <>
void ResourceManager::FillInfoFromOpenGL(ResourceManager::SamplerInfo* info) {
  FillSamplerInfo(graphics_manager_, info);
}

template <>
void ResourceManager::FillInfoFromOpenGL(ResourceManager::ShaderInfo* info) {
  FillShaderInfo(graphics_manager_, info);
}

template <>
void ResourceManager::FillInfoFromOpenGL(ResourceManager::PlatformInfo* info) {
  FillPlatformInfo(graphics_manager_, info);
}

template <>
void ResourceManager::FillInfoFromOpenGL(ResourceManager::TextureInfo* info) {
  FillTextureInfo(graphics_manager_, info);
}

// Nothing to do as the renderer has already filled the info.
template <> void ResourceManager::FillInfoFromOpenGL(
    ResourceManager::TextureImageInfo* info) {}

template <>
void ResourceManager::FillInfoFromOpenGL(
    ResourceManager::TransformFeedbackInfo* info) {
  FillTransformFeedbackInfo(graphics_manager_, info);
}

}  // namespace gfx
}  // namespace ion
