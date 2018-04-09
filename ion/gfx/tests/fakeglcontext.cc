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

#include "ion/gfx/tests/fakeglcontext.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ion/base/allocator.h"
#include "ion/base/enumhelper.h"
#include "ion/base/logging.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/stringutils.h"
#include "ion/base/threadlocalobject.h"
#include "ion/base/weakreferent.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/glfunctiontypes.h"
#include "ion/gfx/image.h"
#include "ion/gfx/openglobjects.h"
#include "ion/math/range.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"
#include "ion/port/timer.h"
#include "absl/memory/memory.h"

// This file transitively includes windows.h, which contains the MemoryBarrier
// macro. We need to #undef it here, because it interferes with the definition
// of the dummy MemoryBarrier method in FakeGlContext::ShadowState.
#ifdef MemoryBarrier
#undef MemoryBarrier
#endif

namespace ion {
namespace gfx {
namespace testing {

// The set of supported GL extensions.
static const char kExtensionsString[] =
    "GL_OES_blend_func_separate "
    "GL_OES_blend_subtract "
    "GL_APPLE_clip_distance "
    "GL_OES_compressed_ETC1_RGB8_texture "
    "GL_ARB_compute_shader "
    "GL_EXT_debug_label "
    "GL_EXT_debug_marker "
    "GL_ARB_debug_output "
    "GL_OES_depth24 "
    "GL_OES_depth32 "
    "GL_OES_depth_texture "
    "GL_EXT_discard_framebuffer "
    "GL_EXT_disjoint_timer_query "
    "GL_EXT_draw_buffers "
    "GL_EXT_draw_instanced "
    "GL_OES_EGL_image "
    "GL_OES_EGL_image_external "
    "GL_OES_element_index_uint "
    "GL_OES_fbo_render_mipmap "
    "GL_EXT_frag_depth "
    "GL_OES_fragment_precision_high "
    "GL_EXT_framebuffer_blit "
    "GL_QCOM_framebuffer_foveated "
    "GL_APPLE_framebuffer_multisample "
    "GL_EXT_framebuffer_multisample "
    "GL_OES_framebuffer_object "
    "GL_ARB_geometry_shader4 "
    "GL_EXT_gpu_shader4 "
    "GL_EXT_instanced_arrays "
    "GL_OES_map_buffer_range "
    "GL_OES_mapbuffer "
    "GL_ARB_multisample "
    "GL_EXT_multisampled_render_to_texture "
    "GL_OVR_multiview "
    "GL_OVR_multiview2 "
    "GL_OVR_multiview_multisampled_render_to_texture "
    "GL_OES_packed_depth_stencil "
    "GL_EXT_protected_textures "
    "GL_OES_rgb8_rgba8 "
    "GL_OES_sample_shading "
    "GL_EXT_shader_texture_lod "
    "GL_NV_sRGB_formats "
    "GL_OES_standard_derivatives "
    "GL_OES_stencil8 "
    "GL_ARB_sync "
    "GL_OES_texture_3D "
    "GL_EXT_texture_array "
    "GL_NV_texture_barrier "
    "GL_EXT_texture_compression_dxt1 "
    "GL_ANGLE_texture_compression_dxt5 "
    "GL_IMG_texture_compression_pvrtc "
    "GL_EXT_texture_compression_s3tc "
    "GL_NV_texture_compression_s3tc "
    "GL_OES_texture_cube_map "
    "GL_ARB_texture_cube_map_array "
    "GL_EXT_texture_filter_anisotropic "
    "GL_OES_texture_float "
    "GL_QCOM_texture_foveated "
    "GL_OES_texture_half_float "
    "GL_EXT_texture_lod_bias "
    "GL_APPLE_texture_max_level "
    "GL_OES_texture_mirrored_repeat "
    "GL_ARB_texture_multisample "
    "GL_EXT_texture_rg "
    "GL_OES_texture_stencil8 "
    "GL_EXT_texture_storage "
    "GL_ARB_texture_storage_multisample "
    "GL_ARB_texture_swizzle "
    "GL_EXT_texture_type_2_10_10_10_REV "
    "GL_QCOM_tiled_rendering "
    "GL_ARB_transform_feedback2 "
    "GL_OES_vertex_array_object ";  // NOLINT
// Putting the semicolon on a separate line simplifies managing this list.

constexpr int kFoveationFocalPointCount = 2;

// Base struct for OpenGL object structs. See below comment.
struct OpenGlObject {
  OpenGlObject() : deleted(false) {}
  // A list of invocation numbers that hold the ShadowState::call_count_ from
  // when the object was bound. This is useful for checking that calls occur
  // in a certain order, and that a particular object has been bound. A new
  // number is appended to the vector every time the object is bound (e.g.,
  // BindBuffer, UseProgram).
  std::vector<int64> bindings;
  bool deleted;  // True if this object has been deleted.
  std::string label;
};

//-----------------------------------------------------------------------------
//
// Each struct holds the state of the object in the same manner as OpenGL; this
// state is set using GL calls such as BufferData or TexImage2D, and can be read
// using the Get functions.
//
// Objects are typically created from a Gen call (e.g., GenBuffers, GenTextures,
// GenVertexArrays), and become invalid when deleted (e.g., DeleteBuffers,
// DeleteTextures, DeleteVertexArrays), but are not destroyed. This allows
// tracking when the client tries to use an invalid id.
//
// Similar to OpenGL, there are default Array, Buffer, and TextureObjects with
// index 0. The rest must be created using the Gen functions.
//
//-----------------------------------------------------------------------------
struct ArrayObjectData : OpenGlObject {
  ArrayObjectData() : element_array(0U) {}
  GLuint element_array;
};
typedef ArrayInfo<ArrayObjectData> ArrayObject;
// Buffer data is only known when BindBuffer is called.
struct BufferObjectData : OpenGlObject {
  BufferObjectData() : data(nullptr), access(0) {}
  ~BufferObjectData() { ClearData(); }
  void ClearData() {
    if (data)
      delete[] reinterpret_cast<uint8*>(data);
  }
  // The data pointer.
  GLvoid* data;
  // The range of mapped data.
  math::Range1ui mapped_range;
  // The access mode used to map the data.
  GLbitfield access;
};
typedef BufferInfo<BufferObjectData> BufferObject;
struct FramebufferObject : FramebufferInfo<OpenGlObject> {
  FramebufferObject()
      : is_foveation_enabled(false),
        foveated_layer_count(0),
        foveated_focal_point_count(0) {}
  bool is_foveation_enabled;
  GLuint foveated_layer_count;
  GLuint foveated_focal_point_count;
};
struct ProgramObjectData : OpenGlObject {
  ProgramObjectData()
    : compute_shader(0U),
      has_compute_stage(false),
      max_uniform_location(0) {}
  // 
  // A program can have any number of shaders, though only one shader of a given
  // type can have a main() function.
  GLuint compute_shader;
  bool has_compute_stage;
  GLint max_uniform_location;
  // Resolved transform feedback varyings are generated at link time by
  // looking up the strings in requested_tf_varyings.
  struct ResolvedVarying {
    std::string name;
    GLint size;
    GLenum type;
  };
  std::vector<ResolvedVarying> resolved_tf_varyings;
};
typedef ProgramInfo<ProgramObjectData> ProgramObject;
struct RenderbufferObject : RenderbufferInfo<OpenGlObject> {
  RenderbufferObject() : implicit_multisampling(false) {}
  // Whether the renderbuffer storage was allocated with
  // RenderbufferStorageMultisampleEXT (from the multisampled_render_to_texture
  // extension).
  bool implicit_multisampling;
};
typedef SamplerInfo<OpenGlObject> SamplerObject;
struct ShaderObject : ShaderInfo<OpenGlObject> {
  // Programs to which the shader is attached.
  std::set<GLuint> programs;
};
typedef SyncInfo<OpenGlObject> SyncObject;
struct TransformFeedbackObjectData : OpenGlObject {
  TransformFeedbackObjectData()
      : id(0),
        primitive_mode(static_cast<GLenum>(-1)) {}
  // The name of the transform feedback object.
  GLuint id;
  // A vector that contains information of whether a binding point has a buffer
  // bound or not. -1 means that it is not bound. Other positive values are
  // the indices to the varyings in ProgramObject.
  std::vector<int> binding_point_status;
  // The output type of primitives that will be recorded into the buffer objects
  // that are bound for transform feedback.
  GLenum primitive_mode;
};
typedef TransformFeedbackInfo<TransformFeedbackObjectData>
    TransformFeedbackObject;
// Internal data known only when the texture is created.
struct TextureObjectData : OpenGlObject {
  TextureObjectData()
      : border(-1),
        format(static_cast<GLenum>(-1)),
        internal_format(static_cast<GLenum>(-1)),
        type(static_cast<GLenum>(-1)),
        compressed(false),
        immutable(false),
        egl_image(false) {}

  // A MipLevel of the texture. MipLevels should only be used here due to the
  // use of a unique_ptr for the data.
  struct MipLevel {
    MipLevel() : width(-1), height(-1), depth(-1) {}
    MipLevel(const MipLevel& other)
        : width(other.width), height(other.height), depth(other.depth) {
      data.swap(other.data);
    }
    // Note that this transfers ownership of data from other to this. This
    // allows construction of MipLevels on the stack, but their data is
    // transferred to the asignee contained within TextureObjectData.
    void operator=(const MipLevel& other) {
      width = other.width;
      height = other.height;
      depth = other.depth;
      data.swap(other.data);
    }
    // The dimensions of the mip level.
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    mutable std::unique_ptr<char[]> data;
  };

  GLint border;
  GLenum format;
  GLenum internal_format;
  GLenum type;
  // The texture data, with a pointer per mip-level.
  std::vector<MipLevel> levels;
  bool compressed;
  bool immutable;
  bool egl_image;
};
typedef TextureInfo<TextureObjectData> TextureObject;
typedef TimerInfo<OpenGlObject> TimerObject;

//-----------------------------------------------------------------------------
//
// DebugMessageState class functions.  This class tracks the current
// enabled/disabled state of OpenGL debugging messages, for the implementation
// of GL_ARB_debug_output.
//
//-----------------------------------------------------------------------------

class DebugMessageState {
 public:
  // Checks if a certain debug message is enabled.
  bool IsEnabled(GLenum source, GLenum type, GLuint id, GLenum severity) const {
    const DisableState& state =
        disabled_messages_[GetSourceIndex(source)][GetTypeIndex(type)];
    return !state.disabled_severities[GetSeverityIndex(severity)] &&
           state.disabled_ids.find(id) == state.disabled_ids.end();
  }

  // Enables or disables a set of debug messages.
  void SetEnabled(GLenum source, GLenum type, GLsizei count, const GLuint* ids,
                  GLenum severity, bool enabled) {
    for (size_t source_index = 0; source_index < kSourceCount; ++source_index) {
      if (source_index != GetSourceIndex(source) && source != GL_DONT_CARE) {
        continue;
      }
      for (size_t type_index = 0; type_index < kTypeCount; ++type_index) {
        if (type_index != GetTypeIndex(type) && type != GL_DONT_CARE) {
          continue;
        }
        DisableState& state = disabled_messages_[source_index][type_index];
        if (count <= 0) {
          // Enable/disable all messages of |source| and |type| at |severity|.
          for (size_t severity_index = 0; severity_index < kSeverityCount;
               ++severity_index) {
            if (severity_index != GetSeverityIndex(severity) &&
                severity != GL_DONT_CARE) {
              continue;
            }
            state.disabled_severities[GetSeverityIndex(severity)] = !enabled;
          }
        } else {
          // Enable/disable all messages with an id in |ids|, of |source| and
          // |type|.
          if (enabled) {
            for (GLsizei i = 0; i < count; ++i) {
              state.disabled_ids.erase(ids[i]);
            }
          } else {
            for (GLsizei i = 0; i < count; ++i) {
              state.disabled_ids.insert(ids[i]);
            }
          }
        }
      }
    }
  }

 private:
  static const size_t kSourceCount =
      GL_DEBUG_SOURCE_OTHER - GL_DEBUG_SOURCE_API + 1;
  static const size_t kTypeCount =
      GL_DEBUG_TYPE_OTHER - GL_DEBUG_TYPE_ERROR + 1;
  static const size_t kSeverityCount =
      GL_DEBUG_SEVERITY_LOW - GL_DEBUG_SEVERITY_HIGH + 1;

  // This struct represents the enable/disable debug message state of a given
  // set of messages.  The "set" usually comprises all messages of the same
  // source and type.
  struct DisableState {
    DisableState()
        : disabled_severities({{
              false,  // GL_DEBUG_SEVERITY_HIGH
              false,  // GL_DEBUG_SEVERITY_MEDIUM
              true,   // GL_DEBUG_SEVERITY_LOW
          }}) {}

    // Severities disabled in this set.
    std::array<bool, kSeverityCount> disabled_severities;
    // Message ids disabled in this set.
    std::set<GLuint> disabled_ids;
  };

  // Index from a GLenum to a source index.
  static size_t GetSourceIndex(GLenum source) {
    return source - GL_DEBUG_SOURCE_API;
  }
  // Index from a GLenum to a type index.
  static size_t GetTypeIndex(GLenum type) { return type - GL_DEBUG_TYPE_ERROR; }
  // Index from a GLenum to a severity index.
  static size_t GetSeverityIndex(GLenum severity) {
    return severity - GL_DEBUG_SEVERITY_HIGH;
  }

  std::array<std::array<DisableState, kTypeCount>, kSourceCount>
      disabled_messages_;
};

//-----------------------------------------------------------------------------
//
// Convenience functions.
//
//-----------------------------------------------------------------------------

// The ConvertValue() family of helper functions takes a single value of the
// type FromType and converts it to an array of values of type FromType.
// ToType can be: GLint, GLuint, GLfloat, GLboolean. FromType can be: bool, int,
// unsigned int, float, math::Range1f, math::Point2i, math::Vector3i,
// const std::vector<GLint>&. Note that these are overloads, not template
// specializations.
template <typename ToType,
          typename FromType,
          typename std::enable_if<
              !std::is_same<ToType, bool>::value &&
                  std::is_fundamental<ToType>::value &&
                  std::is_fundamental<FromType>::value,
              int>::type = 0>
static void ConvertValue(ToType* out, FromType v) {
  out[0] = static_cast<ToType>(v);
}

// From the glGet() documentation:
// If glGetBooleanv is called, a floating-point (or integer) value is converted
// to GL_FALSE if and only if it is 0.0 (or 0). Otherwise, it is converted to
// GL_TRUE
template <typename FromType,
          typename std::enable_if<
              std::is_fundamental<FromType>::value, int>::type = 0>
void ConvertValue(GLboolean* out, FromType v) {
  out[0] = v == FromType(0) ? GL_FALSE : GL_TRUE;
}

// Compound types.
template <typename ToType>
void ConvertValue(ToType* out, math::Range1f range) {
  ConvertValue<ToType, float>(out, range.GetMinPoint()[0]);
  ConvertValue<ToType, float>(out + 1, range.GetMaxPoint()[0]);
}
template <typename ToType>
void ConvertValue(ToType* out, math::Point2i point) {
  ConvertValue<ToType, int>(out, point[0]);
  ConvertValue<ToType, int>(out + 1, point[1]);
}
template <typename ToType>
void ConvertValue(ToType* out, math::Vector3i vec) {
  ConvertValue<ToType, int>(out, vec[0]);
  ConvertValue<ToType, int>(out + 1, vec[1]);
  ConvertValue<ToType, int>(out + 2, vec[2]);
}
template <typename ToType>
void ConvertValue(ToType* out, const std::vector<GLenum>& v) {
  for (std::size_t i = 0; i < v.size(); ++i) {
    ConvertValue(out + i, v[i]);
  }
}

// From the glGet() documentation:
// Floating-point colors and normals, however, are returned with a linear
// mapping that maps 1.0 to the most positive representable integer value and
// -1.0 to the most negative representable integer value. If glGetFloatv is
// called, boolean values are returned as GL_TRUE or GL_FALSE, and integer
// values are converted to floating-point values.
// 

GLfloat Clampf(GLfloat f) { return math::Clamp(f, 0.0f, 1.0f); }

// Returns the OpenGL type name of the named type.
static GLenum GetShaderInputTypeFromTypeName(const std::string& type) {
  if (type.compare("float") == 0)
    return GL_FLOAT;
  else if (type.compare("vec2") == 0)
    return GL_FLOAT_VEC2;
  else if (type.compare("vec3") == 0)
    return GL_FLOAT_VEC3;
  else if (type.compare("vec4") == 0)
    return GL_FLOAT_VEC4;
  else if (type.compare("int") == 0)
    return GL_INT;
  else if (type.compare("ivec2") == 0)
    return GL_INT_VEC2;
  else if (type.compare("ivec3") == 0)
    return GL_INT_VEC3;
  else if (type.compare("ivec4") == 0)
    return GL_INT_VEC4;
  else if (type.compare("isampler1D") == 0)
    return GL_INT_SAMPLER_1D;
  else if (type.compare("isampler1DArray") == 0)
    return GL_INT_SAMPLER_1D_ARRAY;
  else if (type.compare("isampler2D") == 0)
    return GL_INT_SAMPLER_2D;
  else if (type.compare("isampler2DArray") == 0)
    return GL_INT_SAMPLER_2D_ARRAY;
  else if (type.compare("isampler3D") == 0)
    return GL_INT_SAMPLER_3D;
  else if (type.compare("isamplerCube") == 0)
    return GL_INT_SAMPLER_CUBE;
  else if (type.compare("isamplerCubeArray") == 0)
    return GL_INT_SAMPLER_CUBE_MAP_ARRAY;
  else if (type.compare("sampler1D") == 0)
    return GL_SAMPLER_1D;
  else if (type.compare("sampler1DArray") == 0)
    return GL_SAMPLER_1D_ARRAY;
  else if (type.compare("sampler1DArrayShadow") == 0)
    return GL_SAMPLER_1D_ARRAY_SHADOW;
  else if (type.compare("sampler1DShadow") == 0)
    return GL_SAMPLER_1D_SHADOW;
  else if (type.compare("sampler2D") == 0)
    return GL_SAMPLER_2D;
  else if (type.compare("sampler2DArray") == 0)
    return GL_SAMPLER_2D_ARRAY;
  else if (type.compare("sampler2DArrayShadow") == 0)
    return GL_SAMPLER_2D_ARRAY_SHADOW;
  else if (type.compare("sampler2DMS") == 0)
    return GL_SAMPLER_2D_MULTISAMPLE;
  else if (type.compare("sampler2DMSArray") == 0)
    return GL_SAMPLER_2D_MULTISAMPLE_ARRAY;
  else if (type.compare("sampler2DShadow") == 0)
    return GL_SAMPLER_2D_SHADOW;
  else if (type.compare("sampler3D") == 0)
    return GL_SAMPLER_3D;
  else if (type.compare("samplerCube") == 0)
    return GL_SAMPLER_CUBE;
  else if (type.compare("samplerCubeArray") == 0)
    return GL_SAMPLER_CUBE_MAP_ARRAY;
  else if (type.compare("samplerCubeArrayShadow") == 0)
    return GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW;
  else if (type.compare("samplerCubeShadow") == 0)
    return GL_SAMPLER_CUBE_SHADOW;
  else if (type.compare("samplerExternalOES") == 0)
    return GL_SAMPLER_EXTERNAL_OES;
  else if (type.compare("uint") == 0)
    return GL_UNSIGNED_INT;
  else if (type.compare("usampler1D") == 0)
    return GL_UNSIGNED_INT_SAMPLER_1D;
  else if (type.compare("usampler1DArray") == 0)
    return GL_UNSIGNED_INT_SAMPLER_1D_ARRAY;
  else if (type.compare("usampler2D") == 0)
    return GL_UNSIGNED_INT_SAMPLER_2D;
  else if (type.compare("usampler2DArray") == 0)
    return GL_UNSIGNED_INT_SAMPLER_2D_ARRAY;
  else if (type.compare("usampler3D") == 0)
    return GL_UNSIGNED_INT_SAMPLER_3D;
  else if (type.compare("usamplerCube") == 0)
    return GL_UNSIGNED_INT_SAMPLER_CUBE;
  else if (type.compare("usamplerCubeArray") == 0)
    return GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY;
  else if (type.compare("uvec2") == 0)
    return GL_UNSIGNED_INT_VEC2;
  else if (type.compare("uvec3") == 0)
    return GL_UNSIGNED_INT_VEC3;
  else if (type.compare("uvec4") == 0)
    return GL_UNSIGNED_INT_VEC4;
  else if (type.compare("mat2") == 0)
    return GL_FLOAT_MAT2;
  else if (type.compare("mat3") == 0)
    return GL_FLOAT_MAT3;
  else if (type.compare("mat4") == 0)
    return GL_FLOAT_MAT4;
  else
    return GL_INVALID_ENUM;
}

// Returns the number of slots that an attribute type requires.
static GLuint GetAttributeSlotCount(GLenum type) {
  GLuint slots = 1U;
  switch (type) {
    case GL_FLOAT_MAT2:
      slots = 2;
      break;
    case GL_FLOAT_MAT3:
      slots = 3;
      break;
    case GL_FLOAT_MAT4:
      slots = 4;
      break;
    default:
      break;
  }
  return slots;
}

// Parses a shader input name and returns the name and array size of the input
// in the passed variables.
static void ParseShaderInputName(const std::string& input, std::string* name,
                                 GLint* size) {
  // Try to find an array specification.
  std::string count;
  *size = 0;
  if (input.find("[") != std::string::npos) {
    const std::vector<std::string> decls = base::SplitString(input, "[]");
    *name = decls[0];
    if (decls.size() > 1 && !decls[1].empty())
      *size = base::StringToInt32(decls[1]);
  } else {
    *name = input;
  }
}

// Very fragile way of detecting shader inputs.  This function is only for
// testing purposes, and is not intended to come close to approximating a full
// GLSL parser.  It does, however, provide a simple way to detect the most
// common types of shader input declarations.
// 
// parsing.
static void AddShaderInputs(ProgramObject* po, GLenum shader_type,
                            const std::string& shader_source) {
  const std::vector<std::string> statements =
      base::SplitString(shader_source, ";\n\r");

  // Remember #define (unordered) and #ifdef statements (in stack order).
  // The bool in the ifdefs vector indicates whether we are currently in a
  // negated block (either an #else or #ifndef).
  std::set<std::string> defines;
  std::vector<std::pair<std::string, bool>> ifdefs;

  // Split the source into statements separated by ;.
  for (size_t i = 0; i < statements.size(); ++i) {
    // Ignore tokens in single-line comments by stripping out the comment.
    const std::string stripped =
        statements[i].substr(0, statements[i].find("//"));
    const std::vector<std::string> words =
        base::SplitString(stripped, " \t");

    // Analyze preprocessor macros.
    // -------------------------------------------------------------------------
    // The following expressions are fully supported.
    if (words.size() >= 2 && words[0].compare("#define") == 0) {
      defines.insert(words[1]);
    }
    if (words.size() >= 2 && words[0].compare("#ifdef") == 0) {
      ifdefs.push_back(std::make_pair(words[1], false));
    }
    if (words.size() >= 2 && words[0].compare("#ifndef") == 0) {
      ifdefs.push_back(std::make_pair(words[1], true));
    }
    if (words.size() >= 1 && words[0].compare("#else") == 0) {
      ifdefs.back().second = !ifdefs.back().second;
    }
    if (words.size() >= 1 && words[0].compare("#endif") == 0) {
      ifdefs.pop_back();
    }

    // The more general expressions #if and #elif that would allow arbitrary
    // Boolean expressions are not supported. #undef is not supported either.
    if (words.size() >= 1 && words[0].compare("#if") == 0) {
      LOG_ONCE(WARNING)
          << "FakeGlContext shader preprocessor does not support #if. "
          << "The set of recognized shader inputs is most likely incorrect.";
      // We need to add something to the stack in order to not crash when
      // reading the next #endif statement. Since we know the result will be
      // incorrect, we just add an empty string.
      ifdefs.push_back(std::make_pair("", false));
    }
    if (words.size() >= 1 && words[0].compare("#elif") == 0) {
      LOG_ONCE(WARNING)
          << "FakeGlContext shader preprocessor does not support #elif. "
          << "The set of recognized shader inputs is most likely incorrect.";
      // The size of the ifdef stack stays the same with #elif, so we don't
      // need to push anything.
    }
    if (words.size() >= 1 && words[0].compare("#undef") == 0) {
      LOG_ONCE(WARNING)
          << "FakeGlContext shader preprocessor does not support #undef. "
          << "The set of recognized shader inputs is most likely incorrect.";
      // The size of the ifdef stack stays the same with #undef, so we don't
      // need to push anything.
    }


    // We need to skip this line if either of the following conditions is true:
    // (1) the define is not found and we are not in a negated block.
    // (2) the define is found and we are in a negated block.
    {
      bool skip = false;
      for (size_t j = 0; j < ifdefs.size(); ++j) {
        bool is_defined = defines.find(ifdefs[j].first) != defines.end();
        if (is_defined == ifdefs[j].second) {
          skip = true;
          break;
        }
      }
      if (skip)
        continue;
    }

    // Iterate through uniform and attribute declarations.
    // -------------------------------------------------------------------------

    // There must be at least 3 words to be a declaration (see below comment).
    if (words.size() >= 3 && words[0].compare("precision") != 0) {
      // Input declaration format:
      // <uniform | attribute | varying> [precision] <type> <name> [array size];
      size_t type_index = 1;
      GLint size = 0U;
      std::string type, precision, name;
      if (words[1].compare("lowp") == 0 || words[1].compare("mediump") == 0 ||
          words[1].compare("highp") == 0) {
        precision = words[1];
        type_index = 2;
      }
      type = words[type_index];
      name = words[type_index + 1];

      ParseShaderInputName(name, &name, &size);
      if (words[0].compare("attribute") == 0 ||
          (shader_type == GL_VERTEX_SHADER && words[0].compare("in") == 0)) {
        // Search for an existing attribute.
        bool exists = false;
        for (size_t i = 0; i < po->attributes.size(); ++i) {
          if (po->attributes[i].name.compare(name) == 0) {
            exists = true;
            break;
          }
        }
        if (!exists) {
          ProgramObject::Attribute a;
          a.name = name;
          a.size = std::max(1, size);
          a.type = GetShaderInputTypeFromTypeName(type);
          a.index = static_cast<GLint>(po->attributes.size());
          const GLuint slots = GetAttributeSlotCount(a.type);
          // If the attribute is of matrix type then it will take up multiple
          // slots.
          for (GLuint j = 0; j < slots; ++j)
            po->attributes.push_back(a);
        }
      } else if (words[0].compare("varying") == 0 ||
                 (shader_type == GL_FRAGMENT_SHADER &&
                  words[0].compare("in") == 0)) {
        // Search for an existing varying.
        bool exists = false;
        for (size_t i = 0; i < po->varyings.size(); ++i) {
          if (po->varyings[i].name.compare(name) == 0) {
            exists = true;
            break;
          }
        }
        if (!exists) {
          ProgramObject::Varying v;
          v.name = name;
          v.size = std::max(1, size);
          v.type = GetShaderInputTypeFromTypeName(type);
          v.index = static_cast<GLint>(po->varyings.size());
          const GLuint slots = GetAttributeSlotCount(v.type);
          // If the varying is of matrix type then it will take up multiple
          // slots.
          for (GLuint j = 0; j < slots; ++j) po->varyings.push_back(v);
        }
      } else if (words[0].compare("uniform") == 0) {
        // Search for an existing uniform.
        bool exists = false;
        for (size_t i = 0; i < po->uniforms.size(); ++i) {
          if (po->uniforms[i].name.compare(name) == 0) {
            exists = true;
            break;
          }
        }
        if (!exists) {
          ProgramObject::Uniform u;
          u.name = name;
          u.type = GetShaderInputTypeFromTypeName(type);
          u.size = std::max(1, size);
          if (u.type != GL_INVALID_ENUM) {
            switch (u.type) {
              case GL_FLOAT:
                if (size)
                  u.value.InitArray<float>(base::AllocatorPtr(), size);
                else
                  u.value.Set(0.f);
                break;
              case GL_FLOAT_VEC2:
                if (size)
                  u.value.InitArray<math::Vector2f>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector2f::Zero());
                break;
              case GL_FLOAT_VEC3:
                if (size)
                  u.value.InitArray<math::Vector3f>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector3f::Zero());
                break;
              case GL_FLOAT_VEC4:
                if (size)
                  u.value.InitArray<math::Vector4f>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector4f::Zero());
                break;

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
              case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
                if (size)
                  u.value.InitArray<int>(base::AllocatorPtr(), size);
                else
                  u.value.Set(0);
                break;
              case GL_INT_VEC2:
                if (size)
                  u.value.InitArray<math::Vector2i>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector2i::Zero());
                break;
              case GL_INT_VEC3:
                if (size)
                  u.value.InitArray<math::Vector3i>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector3i::Zero());
                break;
              case GL_INT_VEC4:
                if (size)
                  u.value.InitArray<math::Vector4i>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector4i::Zero());
                break;

              case GL_UNSIGNED_INT:
                if (size)
                  u.value.InitArray<uint32>(base::AllocatorPtr(), size);
                else
                  u.value.Set(0U);
                break;

              case GL_UNSIGNED_INT_VEC2:
                if (size)
                  u.value
                      .InitArray<math::Vector2ui>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector2ui::Zero());
                break;
              case GL_UNSIGNED_INT_VEC3:
                if (size)
                  u.value
                      .InitArray<math::Vector3ui>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector3ui::Zero());
                break;
              case GL_UNSIGNED_INT_VEC4:
                if (size)
                  u.value
                      .InitArray<math::Vector4ui>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Vector4ui::Zero());
                break;

              case GL_FLOAT_MAT2:
                if (size)
                  u.value.InitArray<math::Matrix2f>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Matrix2f::Zero());
                break;
              case GL_FLOAT_MAT3:
                if (size)
                  u.value.InitArray<math::Matrix3f>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Matrix3f::Zero());
                break;
              case GL_FLOAT_MAT4:
                if (size)
                  u.value.InitArray<math::Matrix4f>(base::AllocatorPtr(), size);
                else
                  u.value.Set(math::Matrix4f::Zero());
                break;
              default:
                break;
            }
            u.index = po->max_uniform_location;
            // Advance the location index by the number of elements.
            po->max_uniform_location += u.size;
            po->uniforms.push_back(u);
          }
        }
      }
    }
  }

  // The Nvidia driver reports gl_InstanceID as an attribute input
  // if it is used in the shader. Emulate this behavior.
  if (shader_source.find("gl_InstanceID") != std::string::npos) {
    ProgramObject::Attribute a;
    a.name = "gl_InstanceID";
    a.size = 1;
    a.type = GL_INT;
    a.index = static_cast<GLint>(po->attributes.size());
    po->attributes.push_back(a);
  }

  // TransformFeedbackVaryings allows capture of built-in variables that don't
  // have declarations, so manually add them here.
  ProgramObject::Varying v;
  v.name = "gl_Position";
  v.size = 4;
  v.type = GL_FLOAT;
  v.index = static_cast<GLint>(po->varyings.size());
  po->varyings.push_back(v);
}

// Returns a ProgramObject::Uniform for a given location.
static ProgramObject::Uniform* GetUniformFromLocation(ProgramObject* po,
                                                      GLint location) {
  ProgramObject::Uniform* u = nullptr;
  for (size_t i = 0; i < po->uniforms.size(); ++i) {
    if (location >= po->uniforms[i].index &&
        location < po->uniforms[i].index + po->uniforms[i].size) {
      u = &po->uniforms[i];
      break;
    }
  }
  DCHECK(u);
  return u;
}

// Returns the total size of a RenderbufferObject in bytes.
static GLsizeiptr ComputeRenderbufferObjectSize(const RenderbufferObject& rbo) {
  const GLsizeiptr bits_per_pixel = rbo.red_size + rbo.green_size +
                                    rbo.blue_size + rbo.alpha_size +
                                    rbo.depth_size + rbo.stencil_size;
  DCHECK_EQ(0, bits_per_pixel % 8);
  if (rbo.multisample_samples != 0 && !rbo.implicit_multisampling) {
    return rbo.multisample_samples *
        rbo.width * rbo.height * (bits_per_pixel / 8);
  } else {
    return rbo.width * rbo.height * (bits_per_pixel / 8);
  }
}

//-----------------------------------------------------------------------------
//
// FakeGlContext::ShadowState class functions.
//
//-----------------------------------------------------------------------------

class FakeGlContext::ShadowState {
 public:
  ShadowState(int window_width, int window_height);
  explicit ShadowState(ShadowState* parent_state);
  ~ShadowState();

  std::mutex* GetMutex() { return &mutex_; }

  // Sets/returns a maximum size allowed for allocating any OpenGL buffer.
  // This is used primarily for testing out-of-memory errors.
  void SetMaxBufferSize(GLsizeiptr size_in_bytes) {
    max_buffer_size_ = size_in_bytes;
  }
  GLsizeiptr GetMaxBufferSize() const { return max_buffer_size_; }

  // Gets/sets the current OpenGL error code for testing.
  GLenum GetErrorCode() const { return error_code_; }
  void SetErrorCode(GLenum error_code) { error_code_ = error_code; }

  // Sets the extensions string of the manager to the passed string for testing.
  void SetExtensionsString(const std::string& extensions) {
    extensions_string_ = extensions;
    extension_strings_ = base::SplitString(extensions_string_, " ");
  }

  // Sets the vendor string of the manager to the passed string for testing.
  void SetVendorString(const std::string& vendor) {
    vendor_string_ = vendor;
  }

  // Sets the renderer string of the manager to the passed string for testing.
  void SetRendererString(const std::string& renderer) {
    renderer_string_ = renderer;
  }

  // Sets the version string of the manager to the passed string for testing.
  void SetVersionString(const std::string& version) {
    version_string_ = version;
  }

  // Sets the context profile mask of the manager to the passed mask.
  void SetContextProfileMask(int mask) {
    context_profile_mask_ = mask;
  }

  void SetContextFlags(int flags) {
    context_flags_ = flags;
  }

  void SetForceFunctionFailure(const std::string& func_name,
                               bool always_fails) {
    if (always_fails)
      fail_functions_.insert(func_name);
    else
      fail_functions_.erase(func_name);
  }

  void EnableInvalidGlEnumState(bool enable) { allow_invalid_enums_ = enable; }

// Since the macros don't let us override the generated code for
// a particular capability, update the number of capabilities that are part of
// the state in every Set method.
#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init)             \
  Type Get##name() const { return sname##_; }                           \
  void Set##name(Type value) {                                          \
    sname##_ = std::move(value);                                        \
    ResizeInternalState();                                              \
  }
#include "ion/gfx/glconstants.inc"

 public:
  // Information associated with one indexed buffer binding.
  struct IndexedBufferBinding {
    IndexedBufferBinding() : id(0U), offset(0), size(0) {}
    GLuint id;
    GLintptr offset;
    GLsizeiptr size;
  };
  // Container for all currently bound OpenGL objects.
  struct ActiveObjects {
    ActiveObjects()
        : image_unit(0U),
          vertex_array(0U),
          draw_framebuffer(0U),
          read_framebuffer(0U),
          renderbuffer(0U),
          program(0U),
          transform_feedback(0U),
          array_buffer(0U),
          copy_read_buffer(0U),
          copy_write_buffer(0U),
          dispatch_indirect_buffer(0U),
          element_array_buffer(0U),
          transform_feedback_buffer(0U),
          uniform_buffer(0U) {}
    GLuint image_unit;
    GLuint vertex_array;
    GLuint draw_framebuffer;
    GLuint read_framebuffer;
    GLuint renderbuffer;
    GLuint program;
    GLuint transform_feedback;
    // Regular buffer targets.
    GLuint array_buffer;
    GLuint copy_read_buffer;
    GLuint copy_write_buffer;
    GLuint dispatch_indirect_buffer;
    GLuint element_array_buffer;
    GLuint transform_feedback_buffer;
    GLuint uniform_buffer;
    // Indexed buffer targets.
    std::vector<IndexedBufferBinding> transform_feedback_buffers;
    std::vector<IndexedBufferBinding> uniform_buffers;
  };

  // An OpenGL image unit.
  struct ImageUnit {
    ImageUnit()
        : sampler(0U),
          texture_1d_array(0U),
          texture_2d(0U),
          texture_2d_array(0U),
          texture_2d_multisample(0U),
          texture_2d_multisample_array(0U),
          texture_3d(0U),
          texture_external(0U),
          cubemap(0U),
          cubemap_array(0U) {}
    GLuint sampler;
    GLuint texture_1d_array;
    GLuint texture_2d;
    GLuint texture_2d_array;
    GLuint texture_2d_multisample;
    GLuint texture_2d_multisample_array;
    GLuint texture_3d;
    GLuint texture_external;
    GLuint cubemap;
    GLuint cubemap_array;
  };

  // Checks whether the passed enum is one of the valid attachment points for
  // framebuffer objects.
  bool IsAttachmentEnum(GLenum attachment) const {
    return (attachment >= GL_COLOR_ATTACHMENT0 &&
            attachment < GL_COLOR_ATTACHMENT0 + max_color_attachments_) ||
           attachment == GL_DEPTH_ATTACHMENT ||
           attachment == GL_STENCIL_ATTACHMENT ||
           attachment == GL_DEPTH_STENCIL_ATTACHMENT;
  }

  // Checks whether the passed enum is one of the valid buffers for the
  // default framebuffer object.
  bool IsDefaultFramebufferBufferEnum(GLenum buffer) const {
    return buffer == GL_COLOR || buffer == GL_DEPTH || buffer == GL_STENCIL ||
           buffer == GL_FRONT_LEFT || buffer == GL_FRONT_RIGHT ||
           buffer == GL_BACK_LEFT || buffer == GL_BACK_RIGHT;
  }

  // Checks whether the passed enum is one of the valid framebuffer targets.
  static bool IsFramebufferTarget(GLenum target) {
    return target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER ||
        target == GL_READ_FRAMEBUFFER;
  }

  // Checks whether the default framebuffer is bound to the specified target.
  bool IsDefaultFramebuffer(GLenum target) {
    DCHECK(IsFramebufferTarget(target));
    return (target == GL_READ_FRAMEBUFFER)
        ? (active_objects_.read_framebuffer == 0U)
        : (active_objects_.draw_framebuffer == 0U);
  }
  bool AttachmentsAreIncompatible(const FramebufferObject::Attachment& a,
                                  const FramebufferObject::Attachment& b) {
    if ((IsTexture(a.value) && object_state_->textures[a.value].egl_image) ||
        (IsTexture(b.value) && object_state_->textures[b.value].egl_image)) {
      return false;
    }
    return a.type != GL_NONE && b.type != GL_NONE &&
           (GetAttachmentWidth(a) != GetAttachmentWidth(b) ||
            GetAttachmentHeight(a) != GetAttachmentHeight(b));
  }

  // Checks whether the attachment satisfies the OpenGL attachment completeness
  // constraints (has the right size and a renderable format).
  bool AttachmentIsIncomplete(const FramebufferObject& fbo,
                              const FramebufferObject::Attachment& a) {
    bool ret = false;
    if (a.type == GL_RENDERBUFFER &&
        (!IsRenderbuffer(a.value) ||
         object_state_->renderbuffers[a.value].width == 0 ||
         object_state_->renderbuffers[a.value].height == 0 ||
         ((&a >= &fbo.color.front() && &a <= &fbo.color.back()) &&
          !gfx::FramebufferObject::IsColorRenderable(
              object_state_->renderbuffers[a.value].internal_format)) ||
         (&a == &fbo.depth &&
          !gfx::FramebufferObject::IsDepthRenderable(
              object_state_->renderbuffers[a.value].internal_format)) ||
         (&a == &fbo.stencil &&
          !gfx::FramebufferObject::IsStencilRenderable(
              object_state_->renderbuffers[a.value].internal_format))))
      ret = true;
    if (a.type == GL_TEXTURE && !object_state_->textures[a.value].egl_image &&
        (!IsTexture(a.value) ||
         object_state_->textures[a.value].levels.empty() ||
         object_state_->textures[a.value].levels[0].width == 0 ||
         object_state_->textures[a.value].levels[0].height == 0 ||
         ((&a >= &fbo.color.front() && &a <= &fbo.color.back()) &&
          !gfx::FramebufferObject::IsColorRenderable(
              object_state_->textures[a.value].internal_format)) ||
         (&a == &fbo.depth && !gfx::FramebufferObject::IsDepthRenderable(
             object_state_->textures[a.value].internal_format)) ||
         (&a == &fbo.stencil && !gfx::FramebufferObject::IsStencilRenderable(
             object_state_->textures[a.value].internal_format))))
      ret = true;
    return ret;
  }

  // Gets the natural height of the attachment.
  int GetAttachmentHeight(const FramebufferObject::Attachment& a) {
    int height = -1;
    if (a.type == GL_RENDERBUFFER && IsRenderbuffer(a.value))
      height = object_state_->renderbuffers[a.value].height;
    if (a.type == GL_TEXTURE && IsTexture(a.value) &&
        a.level < static_cast<GLuint>(
            object_state_->textures[a.value].levels.size()))
      height = object_state_->textures[a.value].levels[a.level].height;
    return height;
  }

  // Gets the natural width of the attachment.
  int GetAttachmentWidth(const FramebufferObject::Attachment& a) {
    int width = -1;
    if (a.type == GL_RENDERBUFFER && IsRenderbuffer(a.value))
      width = object_state_->renderbuffers[a.value].width;
    if (a.type == GL_TEXTURE && IsTexture(a.value) &&
        a.level < static_cast<GLuint>(
            object_state_->textures[a.value].levels.size()))
      width = object_state_->textures[a.value].levels[a.level].width;
    return width;
  }

  int GetAttachmentSamples(const FramebufferObject::Attachment& a) {
    int samples = -1;
    if (a.type == GL_RENDERBUFFER && IsRenderbuffer(a.value))
      samples = object_state_->renderbuffers[a.value].multisample_samples;
    if (a.type == GL_TEXTURE && IsTexture(a.value)) {
      if (a.texture_samples != 0)
        samples = a.texture_samples;
      else
        samples = object_state_->textures[a.value].samples;
    }
    return samples;
  }

  int GetAttachedShaderCount(GLuint program) {
    const ProgramObject& po = object_state_->programs[program];
    const int count = (po.vertex_shader > 0 ? 1 : 0) +
                      (po.geometry_shader > 0 ? 1 : 0) +
                      (po.fragment_shader > 0 ? 1 : 0);
    return count;
  }

  bool IsAttachmentImplicitlyMultisampled(
      const FramebufferObject::Attachment& a) {
    bool result = false;
    if (a.type == GL_RENDERBUFFER && IsRenderbuffer(a.value))
      result = object_state_->renderbuffers[a.value].implicit_multisampling;
    if (a.type == GL_TEXTURE)
      result = a.texture_samples;
    return result;
  }

  // Gets an attachment given a framebuffer target and an attachment enum.
  FramebufferObject::Attachment*
  GetAttachment(GLenum target, GLenum attachment) {
    FramebufferObject& fbo = target == GL_READ_FRAMEBUFFER
        ? container_state_->framebuffers[active_objects_.read_framebuffer]
        : container_state_->framebuffers[active_objects_.draw_framebuffer];
    FramebufferObject::Attachment* a = nullptr;
    if (attachment >= GL_COLOR_ATTACHMENT0 &&
        attachment < GL_COLOR_ATTACHMENT0 + max_color_attachments_) {
      int index = attachment - GL_COLOR_ATTACHMENT0;
      a = &fbo.color[index];
    } else if (attachment == GL_DEPTH_ATTACHMENT) {
      a = &fbo.depth;
    } else if (attachment == GL_STENCIL_ATTACHMENT) {
      a = &fbo.stencil;
    }
    DCHECK(a);
    return a;
  }

  // Gets an attachment that has been cleared to default values.
  FramebufferObject::Attachment*
  GetClearedAttachment(GLenum target, GLenum attachment) {
    FramebufferObject::Attachment* a = GetAttachment(target, attachment);
    *a = FramebufferObject::Attachment();
    return a;
  }

  // Sets the parameters of a renderbuffer. Used to implement the various
  // RenderbufferStorage* functions.
  void SetRenderbufferStorage(GLenum target, GLsizei samples,
                              GLenum internalformat, GLsizei width,
                              GLsizei height, bool implicit_multisampling) {
    // GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.
    // GL_INVALID_ENUM is generated if internalformat is not an accepted format.
    // GL_INVALID_VALUE is generated if samples is greater than GL_MAX_SAMPLES.
    // GL_INVALID_VALUE is generated if width or height is less than zero or
    // greater than GL_MAX_RENDERBUFFER_SIZE.
    // GL_OUT_OF_MEMORY is generated if the implementation is unable to create
    // a data store with the requested width and height.
    // GL_INVALID_OPERATION is generated if the reserved renderbuffer object
    // name 0 is bound.
    if (CheckGlEnum(
            target == GL_RENDERBUFFER &&
            (gfx::FramebufferObject::IsColorRenderable(internalformat) ||
             gfx::FramebufferObject::IsDepthRenderable(internalformat) ||
             gfx::FramebufferObject::IsStencilRenderable(internalformat))) &&
        CheckGlValue(samples >= 0 && samples <= max_samples_ &&
                     width >= 0 && width < max_renderbuffer_size_ &&
                     height >= 0 && height < max_renderbuffer_size_) &&
        CheckGlOperation(IsRenderbuffer(active_objects_.renderbuffer))) {
      // The out of memory error is ignored here since no allocation is done.
      RenderbufferObject& r =
          object_state_->renderbuffers[active_objects_.renderbuffer];
      r.width = width;
      r.height = height;
      r.internal_format = internalformat;
      r.multisample_samples = samples;
      if (samples > 0 && implicit_multisampling)
        r.implicit_multisampling = true;
      SetColorsFromInternalFormat(internalformat, &r);
      CheckGlMemory(ComputeRenderbufferObjectSize(r));
    }
  }

  // Sets the parameters of a texture attachment. Use to implement the
  // FramebufferTexture2D* functions.
  void SetFramebufferTexture(GLenum target, GLenum attachment, GLenum textarget,
                             GLuint texture, GLint level, GLint layer,
                             GLsizei num_views, GLsizei samples) {
    // GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.
    // GL_INVALID_ENUM is generated if attachment is not an accepted attachment
    // point.
    // GL_INVALID_OPERATION is generated if the default framebuffer object name
    // 0 is bound.
    if (!CheckGlEnum(IsFramebufferTarget(target) &&
                     IsAttachmentEnum(attachment))) return;
    if (!CheckGlOperation(!IsDefaultFramebuffer(target))) return;
    if (texture == 0U) {
      // When texture is 0, ignore all arguments and unbind.
      if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        GetClearedAttachment(target, GL_DEPTH_ATTACHMENT);
        GetClearedAttachment(target, GL_STENCIL_ATTACHMENT);
      } else {
        GetClearedAttachment(target, attachment);
      }
      return;
    }
    // GL_INVALID_ENUM is generated if textarget is not an accepted texture
    // target and texture is not 0.
    if (layer < 0 && !CheckTexture2dTargetType(textarget)) return;
    // GL_INVALID_VALUE is generated if texture is not zero and layer is larger
    // than the value of GL_MAX_3D_TEXTURE_SIZE minus one (for three-dimensional
    // texture objects), or larger than the value of GL_MAX_ARRAY_TEXTURE_LAYERS
    // minus one (for array texture objects).
    if (layer >= 0 &&
        (!CheckGlOperation(IsLayeredTextureTarget(textarget)) ||
         !CheckTextureLayer(textarget, layer)))
      return;
    // INVALID_OPERATION is generated by FramebufferTextureMultiviewOVR if
    // target is GL_READ_FRAMEBUFFER.
    // INVALID_VALUE is generated by FramebufferTextureMultiviewOVR if numViews
    // is less than 1, numViews is more than MAX_VIEWS_OVR or if (baseViewIndex
    // + numViews) exceeds GL_MAX_ARRAY_TEXTURE_LAYERS.
    // Note that the num_views >= 1 test is done FramebufferTextureMultiviewOVR,
    // not here. Here num_views == 0 means that we were called by a
    // non-multiview attachment function.
    if (num_views > 0 &&
        !(CheckGlOperation(target != GL_READ_FRAMEBUFFER) &&
          CheckGlValue(num_views <= max_views_ &&
                       layer + num_views < max_array_texture_layers_)))
      return;
    // GL_INVALID_VALUE may be generated if level is greater than log_2(max),
    // where max is the returned value of GL_MAX_TEXTURE_SIZE when target is
    // GL_TEXTURE_2D or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not
    // GL_TEXTURE_2D.
    // If samples is greater than the  value of MAX_SAMPLES_EXT, then the error
    // INVALID_VALUE is generated.
    if (!CheckGlValue(CheckTextureLevel(textarget, level) &&
                      samples >= 0 && samples <= max_samples_)) return;
    // GL_INVALID_OPERATION is generated if texture is neither 0 nor the name of
    // an existing texture object.
    // GL_INVALID_OPERATION is generated if texture is the name of an existing
    // two-dimensional texture object but textarget is not GL_TEXTURE_2D, if
    // texture is the name of an existing 2D multisample texture object but
    // textarget is GL_TEXTURE_2D_MULTISAMPLE, or if texture is the name of an
    // existing cube map texture object but textarget is GL_TEXTURE_2D.
    if (!CheckGlOperation(
            IsTexture(texture) &&
            ((IsCubeFaceTarget(textarget) &&
              object_state_->textures[texture].target == GL_TEXTURE_CUBE_MAP) ||
             (textarget == object_state_->textures[texture].target))))
      return;

    auto DoSet = [=](GLenum slot) -> void {
      FramebufferObject::Attachment* a = GetClearedAttachment(target, slot);
      a->type = GL_TEXTURE;
      a->value = texture;
      a->level = level;
      a->texture_samples = samples;
      if (IsCubeFaceTarget(textarget)) {
        a->cube_face = textarget;
      } else if (textarget == GL_TEXTURE_CUBE_MAP ||
                 textarget == GL_TEXTURE_CUBE_MAP_ARRAY) {
        a->layer = std::max(0, layer) / 6;
        a->cube_face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer % 6;
      } else {
        a->layer = std::max(layer, 0);
      }
      a->num_views = num_views;
    };
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
      DoSet(GL_DEPTH_ATTACHMENT);
      DoSet(GL_STENCIL_ATTACHMENT);
    } else {
      DoSet(attachment);
    }
  }

  // Checks the parameters passed to glInvalidateFramebuffer and
  // glInvalidateSubFramebuffer.
  void CheckInvalidateFramebufferArgs(GLenum target, GLsizei numAttachments,
                                      const GLenum *attachments) {
    // GL_INVALID_ENUM is generated by glInvalidateFramebuffer if target is not
    // one of the accepted framebuffer targets.
    // GL_INVALID_VALUE is generated if numAttachments is negative.
    if (CheckGlEnum(IsFramebufferTarget(target)) &&
        CheckGlValue(numAttachments >= 0)) {
      // GL_INVALID_ENUM is generated if any element of attachments is not one
      // of the accepted framebuffer attachment points, as described above.
      for (GLsizei i = 0; i < numAttachments; ++i) {
        if (IsDefaultFramebuffer(target)) {
          if (!CheckGlEnum(IsDefaultFramebufferBufferEnum(attachments[i])))
            return;
        } else {
          // GL_INVALID_OPERATION is generated if element of attachments is
          // GL_COLOR_ATTACHMENTm where m is greater than or equal to the value
          // of GL_MAX_COLOR_ATTACHMENTS.
          if (attachments[i] >= GL_COLOR_ATTACHMENT0 &&
              attachments[i] <= GL_COLOR_ATTACHMENT15 &&
              !CheckGlOperation(attachments[i] <
                                GL_COLOR_ATTACHMENT0 + max_color_attachments_))
            return;
          if (!CheckGlEnum(attachments[i] != GL_DEPTH_STENCIL_ATTACHMENT))
            return;
          if (!CheckGlEnum(IsAttachmentEnum(attachments[i])))
            return;
        }
      }
    }
  }

  // Log a debugging message.  If GL_DEBUG_CALLBACK_FUNCTION is set, the
  // debugging message is sent to the callback function.  Otherwise, it is saved
  // into the debug log.
  void LogDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity,
                       const GLchar* buf) {
    if (!debug_message_state_->IsEnabled(source, type, id, severity)) {
      return;
    }
    GLsizei buf_size = static_cast<GLsizei>(std::strlen(buf));
    if (buf_size >= max_debug_message_length_) {
      buf_size = max_debug_message_length_ - 1;
    }
    if (debug_callback_function_ != nullptr) {
      (*debug_callback_function_)(source, type, id, severity, buf_size, buf,
                                  debug_callback_user_param_);
      return;
    }
    while (static_cast<GLint>(debug_message_log_.size()) >=
           max_debug_logged_messages_) {
      debug_message_log_.pop_front();
    }
    debug_message_log_.emplace_back(source, type, id, severity,
                                    std::string(buf, buf_size));
  }

  // Useful Checks for setting GL errors.
  bool CheckGl(bool expr, GLenum error) {
    if (expr) {
      return true;
    } else {
      // OpenGL records only the first error.
      if (error_code_ == GL_NO_ERROR)
          error_code_ = error;
      GLchar buffer[32];
      snprintf(buffer, sizeof(buffer), "GL error: error=0x%04x", error);
      buffer[sizeof(buffer) - 1] = '\0';
      LogDebugMessage(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, 0,
                      GL_DEBUG_SEVERITY_HIGH, buffer);
      return false;
    }
  }
  bool CheckGlEnum(bool expr) {
    return allow_invalid_enums_ || CheckGl(expr, GL_INVALID_ENUM);
  }
  bool CheckGlValue(bool expr) { return CheckGl(expr, GL_INVALID_VALUE); }
  bool CheckGlOperation(bool expr) {
    return CheckGl(expr, GL_INVALID_OPERATION);
  }
  bool CheckGlMemory(GLsizeiptr size) {
    // This does not keep a running total of memory used - it just checks each
    // allocation against the maximum memory size per buffer.
    const GLsizeiptr max_size = GetMaxBufferSize();
    return CheckGl(max_size == 0 || size <= max_size, GL_OUT_OF_MEMORY);
  }

  bool CheckFunction(const std::string& func_name) {
    if (!fail_functions_.size()) {
      return true;
    } else {
      const size_t count = fail_functions_.count(func_name);
      if (count)
          CheckGl(false, GL_INVALID_OPERATION);
      return count == 0U;
    }
  }
  bool CheckAllBindingPointsBound(
      const std::vector<int>& binding_point_status) {
    bool are_bound = true;
    for (int is_bound : binding_point_status) {
      if (is_bound == -1) {
        are_bound = false;
        break;
      }
    }
    return CheckGlOperation(are_bound);
  }
  bool CheckBlendEquation(GLenum mode) {
    return CheckGlEnum(mode == GL_FUNC_ADD || mode == GL_FUNC_SUBTRACT ||
                       mode == GL_FUNC_REVERSE_SUBTRACT);
  }
  bool CheckBlendFunc(GLenum factor) {
    return CheckGlEnum(
        factor == GL_ZERO || factor == GL_ONE || factor == GL_SRC_COLOR ||
        factor == GL_ONE_MINUS_SRC_COLOR || factor == GL_DST_COLOR ||
        factor == GL_ONE_MINUS_DST_COLOR || factor == GL_SRC_ALPHA ||
        factor == GL_ONE_MINUS_SRC_ALPHA || factor == GL_DST_ALPHA ||
        factor == GL_ONE_MINUS_DST_ALPHA || factor == GL_CONSTANT_COLOR ||
        factor == GL_ONE_MINUS_CONSTANT_COLOR || factor == GL_CONSTANT_ALPHA ||
        factor == GL_ONE_MINUS_CONSTANT_ALPHA ||
        factor == GL_SRC_ALPHA_SATURATE);
  }
  bool CheckBufferTarget(GLenum target) {
    return CheckGlEnum(target == GL_ARRAY_BUFFER ||
                       target == GL_COPY_READ_BUFFER ||
                       target == GL_COPY_WRITE_BUFFER ||
                       target == GL_DISPATCH_INDIRECT_BUFFER ||
                       target == GL_ELEMENT_ARRAY_BUFFER ||
                       target == GL_TRANSFORM_FEEDBACK_BUFFER ||
                       target == GL_UNIFORM_BUFFER);
  }
  bool CheckBufferUsage(GLenum usage) {
    return CheckGlEnum(usage == GL_DYNAMIC_COPY || usage == GL_DYNAMIC_DRAW ||
                       usage == GL_DYNAMIC_READ || usage == GL_STATIC_COPY ||
                       usage == GL_STATIC_DRAW || usage == GL_STATIC_READ ||
                       usage == GL_STREAM_COPY || usage == GL_STREAM_DRAW ||
                       usage == GL_STREAM_READ);
  }
  bool CheckBufferZeroNotBound(GLenum target) {
    return CheckGlOperation(GetBufferIndex(target) != 0U);
  }
  bool CheckColorChannelEnum(GLenum channel) {
    return CheckGlEnum(channel == GL_RED || channel == GL_GREEN ||
                       channel == GL_BLUE || channel == GL_ALPHA ||
                       channel == GL_ONE || channel == GL_ZERO);
  }
  bool CheckCompressedTextureFormat(GLenum format) {
    return CheckGlEnum(
        std::find(compressed_texture_formats_.begin(),
                  compressed_texture_formats_.end(), format) !=
            compressed_texture_formats_.end());
  }
  bool CheckDrawBuffer(GLenum target, GLenum buffer) {
    // When the implementation supports 4 color attachments, is
    // GL_COLOR_ATTACHMENT15 "not an accepted value" or "a color buffer that
    // does not exist in the current GL context"? We choose the former
    // interpretation, since the latter seems to refer to stereo buffers.
    GLuint framebuffer = active_objects_.draw_framebuffer;
    if (target == GL_READ_FRAMEBUFFER)
      framebuffer = active_objects_.read_framebuffer;
    if (framebuffer == 0U) {
      return CheckGlEnum(buffer == GL_NONE || buffer == GL_FRONT_LEFT ||
                         buffer == GL_FRONT_RIGHT || buffer == GL_BACK_LEFT ||
                         buffer == GL_BACK_RIGHT || buffer == GL_FRONT ||
                         buffer == GL_BACK || buffer == GL_LEFT ||
                         buffer == GL_RIGHT || buffer == GL_FRONT_AND_BACK);
    } else {
      return CheckGlEnum(buffer == GL_NONE || (buffer >= GL_COLOR_ATTACHMENT0 &&
          buffer < GL_COLOR_ATTACHMENT0 + max_color_attachments_));
    }
  }
  bool CheckDrawMode(GLenum mode) {
    return CheckGlEnum(mode == GL_POINTS || mode == GL_LINE_STRIP ||
                       mode == GL_LINE_LOOP || mode == GL_LINES ||
                       mode == GL_TRIANGLE_STRIP || mode == GL_TRIANGLE_FAN ||
                       mode == GL_TRIANGLES);
  }
  bool CheckDepthOrStencilFunc(GLenum func) {
    return CheckGlEnum(func == GL_NEVER || func == GL_LESS ||
                       func == GL_EQUAL || func == GL_LEQUAL ||
                       func == GL_GREATER || func == GL_NOTEQUAL ||
                       func == GL_GEQUAL || func == GL_ALWAYS);
  }
  bool CheckFace(GLenum face) {
    return CheckGlEnum(face == GL_FRONT || face == GL_BACK ||
                       face == GL_FRONT_AND_BACK);
  }
  bool CheckFramebuffer() {
    return CheckGl(
        CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
        GL_INVALID_FRAMEBUFFER_OPERATION);
  }
  bool CheckProgram(GLuint program) {
    // The specification of program functions typically has this snippet:
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // This seems to mean that we should return GL_INVALID_VALUE if the ID
    // was never allocated, but GL_INVALID_OPERATION if it was deleted.
    return CheckGlValue(object_state_->programs.count(program)) &&
        CheckGlOperation(IsProgram(program));
  }
  bool CheckShaderType(GLenum type) {
    return CheckGlEnum(type == GL_COMPUTE_SHADER ||
                       type == GL_FRAGMENT_SHADER ||
                       type == GL_GEOMETRY_SHADER ||
                       type == GL_TESS_CONTROL_SHADER ||
                       type == GL_TESS_EVALUATION_SHADER ||
                       type == GL_VERTEX_SHADER);
  }
  bool CheckShader(GLuint shader) {
    // See above - the same applies to shaders.
    return CheckGlValue(object_state_->shaders.count(shader)) &&
        CheckGlOperation(IsShader(shader));
  }
  bool CheckStencilOp(GLenum op) {
    return CheckGlEnum(op == GL_KEEP || op == GL_ZERO || op == GL_REPLACE ||
                       op == GL_INCR || op == GL_INCR_WRAP || op == GL_DECR ||
                       op == GL_DECR_WRAP || op == GL_INVERT);
  }
  bool CheckTextureDimensions(GLenum target, GLsizei width, GLsizei height,
                              GLsizei depth) {
    bool ok = width >= 0 && height >= 0 && depth >= 0;

    const bool is_cubemap =
        target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY ||
        IsCubeFaceTarget(target);

    // Width.
    ok = ok && ((target == GL_TEXTURE_1D_ARRAY && width <= max_texture_size_) ||
                (target == GL_TEXTURE_2D && width <= max_texture_size_) ||
                (target == GL_TEXTURE_2D_ARRAY && width <= max_texture_size_) ||
                (target == GL_TEXTURE_2D_MULTISAMPLE &&
                    width <= max_texture_size_) ||
                (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY &&
                    width <= max_3d_texture_size_) ||
                (target == GL_TEXTURE_3D && width <= max_3d_texture_size_) ||
                (is_cubemap && width <= max_cube_map_texture_size_));

    // Height.
    ok = ok &&
         ((target == GL_TEXTURE_1D_ARRAY &&
              height <= max_array_texture_layers_) ||
          (target == GL_TEXTURE_2D && height <= max_texture_size_) ||
          (target == GL_TEXTURE_2D_ARRAY && height <= max_texture_size_) ||
          (target == GL_TEXTURE_2D_MULTISAMPLE &&
              height <= max_texture_size_) ||
          (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY &&
              height <= max_3d_texture_size_) ||
          (target == GL_TEXTURE_3D && height <= max_3d_texture_size_) ||
          (is_cubemap && height <= max_cube_map_texture_size_));

    // Depth.
    ok = ok && (depth == 1 ||
                (target == GL_TEXTURE_2D_ARRAY &&
                    depth <= max_array_texture_layers_) ||
                (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY &&
                    depth <= max_3d_texture_size_) ||
                (target == GL_TEXTURE_3D && depth <= max_3d_texture_size_) ||
                (target == GL_TEXTURE_CUBE_MAP_ARRAY &&
                    depth <= max_array_texture_layers_));

    // Cubemaps must be square.
    ok = ok && (!is_cubemap || (width == height));
    return CheckGlValue(ok);
  }
  bool CheckTextureFormat(GLenum format) {
    return CheckGlEnum(
        format == GL_RED || format == GL_RED_INTEGER || format == GL_RG ||
        format == GL_RG_INTEGER || format == GL_RGB ||
        format == GL_RGB_INTEGER || format == GL_RGBA ||
        format == GL_RGBA_INTEGER || format == GL_DEPTH_COMPONENT ||
        format == GL_DEPTH_STENCIL || format == GL_LUMINANCE_ALPHA ||
        format == GL_LUMINANCE || format == GL_ALPHA);
  }
  bool CheckTextureInternalFormat(GLenum format) {
    return CheckGlEnum(
        format == GL_ALPHA || format == GL_DEPTH24_STENCIL8 ||
        format == GL_DEPTH32F_STENCIL8 || format == GL_DEPTH_COMPONENT16 ||
        format == GL_DEPTH_COMPONENT24 || format == GL_DEPTH_COMPONENT32F ||
        format == GL_LUMINANCE || format == GL_LUMINANCE_ALPHA ||
        format == GL_R11F_G11F_B10F || format == GL_R16F || format == GL_R16I ||
        format == GL_R16UI || format == GL_R32F || format == GL_R32I ||
        format == GL_R32UI || format == GL_R8 || format == GL_R8I ||
        format == GL_R8UI || format == GL_R8_SNORM || format == GL_RG16F ||
        format == GL_RG16I || format == GL_RG16UI || format == GL_RG32F ||
        format == GL_RG32I || format == GL_RG32UI || format == GL_RG8 ||
        format == GL_RG8I || format == GL_RG8UI || format == GL_RG8_SNORM ||
        format == GL_RGB || format == GL_RGB10_A2 || format == GL_RGB10_A2UI ||
        format == GL_RGB16F || format == GL_RGB16I || format == GL_RGB16UI ||
        format == GL_RGB32F || format == GL_RGB32I || format == GL_RGB32UI ||
        format == GL_RGB565 || format == GL_RGB5_A1 || format == GL_RGB8 ||
        format == GL_RGB8I || format == GL_RGB8UI || format == GL_RGB8_SNORM ||
        format == GL_RGB9_E5 || format == GL_RGBA || format == GL_RGBA16F ||
        format == GL_RGBA16I || format == GL_RGBA16UI || format == GL_RGBA32F ||
        format == GL_RGBA32I || format == GL_RGBA32UI || format == GL_RGBA4 ||
        format == GL_RGBA8 || format == GL_RGBA8I || format == GL_RGBA8UI ||
        format == GL_RGBA8_SNORM || format == GL_SRGB8 ||
        format == GL_SRGB8_ALPHA8);
  }
  bool CheckTextureLevel(GLenum target, GLint level) {
    return level >= 0 &&
           (((target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D_ARRAY) &&
             level <= math::Log2(max_array_texture_layers_)) ||
            ((target == GL_TEXTURE_2D || target == GL_TEXTURE_3D) &&
             level <= math::Log2(max_texture_size_)) ||
            ((target == GL_TEXTURE_2D_MULTISAMPLE) && level == 0) ||
            ((target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) && level == 0) ||
            ((IsCubeFaceTarget(target) || target == GL_TEXTURE_CUBE_MAP ||
              target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
             level <= math::Log2(max_cube_map_texture_size_)) ||
            ((target == GL_TEXTURE_3D) &&
             level <= math::Log2(max_3d_texture_size_)));
  }
  bool CheckTextureLayer(GLenum target, GLint layer) {
    // For cube map textures, the limit applies to the number of layer-faces,
    // so multiplying by 6 is not necessary.
    return CheckGlValue(layer >= 0 &&
                        (((target == GL_TEXTURE_1D_ARRAY ||
                           target == GL_TEXTURE_2D_ARRAY ||
                           target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
                           target == GL_TEXTURE_CUBE_MAP_ARRAY) &&
                          layer < max_array_texture_layers_) ||
                         ((target == GL_TEXTURE_3D) &&
                          layer < max_3d_texture_size_) ||
                         ((target == GL_TEXTURE_CUBE_MAP) &&
                          layer < 6)));
  }
  bool CheckTexture2dTarget(GLenum target) {
    return CheckGlEnum(target == GL_TEXTURE_1D_ARRAY ||
                       target == GL_TEXTURE_2D ||
                       target == GL_TEXTURE_CUBE_MAP);
  }
  bool CheckTexture2dTargetType(GLenum target) {
    return CheckGlEnum(target == GL_TEXTURE_1D_ARRAY ||
                       target == GL_TEXTURE_2D ||
                       target == GL_TEXTURE_2D_MULTISAMPLE ||
                       IsCubeFaceTarget(target));
  }
  bool CheckTexture2dMultisampleTargetType(GLenum target) {
    return CheckGlEnum(target == GL_TEXTURE_2D_MULTISAMPLE);
  }
  bool CheckTexture3dTarget(GLenum target) {
    return CheckGlEnum(target == GL_TEXTURE_2D_ARRAY ||
                       target == GL_TEXTURE_3D ||
                       target == GL_TEXTURE_CUBE_MAP_ARRAY);
  }
  bool CheckTexture3dMultisampleTargetType(GLenum target) {
    return CheckGlEnum(target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
  }
  bool IsLayeredTextureTarget(GLenum target) {
    return target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D_ARRAY ||
        target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY || target == GL_TEXTURE_3D ||
        target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY;
  }
  bool IsTextureTarget(GLenum target) {
    return target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D ||
        target == GL_TEXTURE_EXTERNAL_OES || target == GL_TEXTURE_2D_ARRAY ||
        target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_3D ||
        target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
        target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_CUBE_MAP_ARRAY;
  }
  bool CheckTextureType(GLenum type) {
    return CheckGlEnum(
        type == GL_UNSIGNED_BYTE || type == GL_BYTE ||
        type == GL_UNSIGNED_SHORT || type == GL_SHORT ||
        type == GL_UNSIGNED_INT || type == GL_INT || type == GL_HALF_FLOAT ||
        type == GL_FLOAT || type == GL_UNSIGNED_SHORT_5_6_5 ||
        type == GL_UNSIGNED_SHORT_4_4_4_4 ||
        type == GL_UNSIGNED_SHORT_5_5_5_1 ||
        type == GL_UNSIGNED_INT_2_10_10_10_REV ||
        type == GL_UNSIGNED_INT_10F_11F_11F_REV ||
        type == GL_UNSIGNED_INT_5_9_9_9_REV || type == GL_UNSIGNED_INT_24_8 ||
        type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV);
  }
  bool CheckTextureSamples(GLsizei samples) {
    return CheckGlOperation(samples <= max_samples_);
  }
  bool CheckUniformLocation(GLint location) {
    // GL_INVALID_OPERATION is generated if there is no current program object.
    return CheckGlOperation(active_objects_.program > 0U) &&
           // GL_INVALID_OPERATION is generated if location is an invalid
           // uniform location for the current program object and location is
           // not equal to -1.
           //
           // This ensures we do not try to set the value if location is -1, but
           // do not generate an error, either (we just ignore it).
           CheckGlOperation(
               location >= 0 &&
               (location < static_cast<GLint>(
                   object_state_->programs[active_objects_.program]
                       .max_uniform_location) ||
                location == -1)) &&
           location != -1;
  }
  bool CheckWrapMode(GLenum wrap) {
    return CheckGlEnum(wrap == GL_CLAMP_TO_EDGE || wrap == GL_REPEAT ||
                       wrap == GL_MIRRORED_REPEAT);
  }
  GLuint& GetBufferIndex(GLenum target) {
    switch (target) {
      case GL_ARRAY_BUFFER: return active_objects_.array_buffer;
      case GL_COPY_READ_BUFFER: return active_objects_.copy_read_buffer;
      case GL_COPY_WRITE_BUFFER: return active_objects_.copy_write_buffer;
      case GL_DISPATCH_INDIRECT_BUFFER:
        return active_objects_.dispatch_indirect_buffer;
      case GL_ELEMENT_ARRAY_BUFFER: return active_objects_.element_array_buffer;
      case GL_TRANSFORM_FEEDBACK_BUFFER:
        return active_objects_.transform_feedback_buffer;
      case GL_UNIFORM_BUFFER: return active_objects_.uniform_buffer;
      default:
        LOG(FATAL) << "Unknown target";
        return active_objects_.array_buffer;
    }
  }
  bool CheckTextureFormatTypeAndInternalTypeAreValid(GLenum format, GLenum type,
                                                     GLenum internal_format) {
    // For the table these combinations are taken from, see:
    // http://www.khronos.org/opengles/sdk/docs/man3/xhtml/glTexImage2D.xml
    bool valid = false;
    switch (internal_format) {
      case GL_ALPHA:
        valid = format == GL_ALPHA && type == GL_UNSIGNED_BYTE;
        break;
      case GL_DEPTH_STENCIL:
        valid = format == GL_DEPTH_STENCIL && type == GL_UNSIGNED_INT_24_8;
        break;
      case GL_DEPTH24_STENCIL8:
        valid = format == GL_DEPTH_STENCIL && type == GL_UNSIGNED_INT_24_8;
        break;
      case GL_DEPTH32F_STENCIL8:
        valid = format == GL_DEPTH_STENCIL &&
                type == GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        break;
      case GL_DEPTH_COMPONENT:
        valid = format == GL_DEPTH_COMPONENT &&
                (type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT);
        break;
      case GL_DEPTH_COMPONENT16:
        valid = format == GL_DEPTH_COMPONENT &&
                (type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_INT);
        break;
      case GL_DEPTH_COMPONENT24:
        valid = format == GL_DEPTH_COMPONENT && type == GL_UNSIGNED_INT;
        break;
      case GL_DEPTH_COMPONENT32F:
        valid = format == GL_DEPTH_COMPONENT && type == GL_FLOAT;
        break;
      case GL_LUMINANCE:
        valid = format == GL_LUMINANCE && type == GL_UNSIGNED_BYTE;
        break;
      case GL_LUMINANCE_ALPHA:
        valid = format == GL_LUMINANCE_ALPHA && type == GL_UNSIGNED_BYTE;
        break;
      case GL_R11F_G11F_B10F:
        valid = format == GL_RGB && (type == GL_UNSIGNED_INT_10F_11F_11F_REV ||
                                     type == GL_HALF_FLOAT || type == GL_FLOAT);
        break;
      case GL_R16F:
        valid = format == GL_RED && (type == GL_HALF_FLOAT || type == GL_FLOAT);
        break;
      case GL_R16I:
        valid = format == GL_RED_INTEGER && type == GL_SHORT;
        break;
      case GL_R16UI:
        valid = format == GL_RED_INTEGER && type == GL_UNSIGNED_SHORT;
        break;
      case GL_R32F:
        valid = format == GL_RED && type == GL_FLOAT;
        break;
      case GL_R32I:
        valid = format == GL_RED_INTEGER && type == GL_INT;
        break;
      case GL_R32UI:
        valid = format == GL_RED_INTEGER && type == GL_UNSIGNED_INT;
        break;
      case GL_R8:
        valid = format == GL_RED && type == GL_UNSIGNED_BYTE;
        break;
      case GL_R8I:
        valid = format == GL_RED_INTEGER && type == GL_BYTE;
        break;
      case GL_R8UI:
        valid = format == GL_RED_INTEGER && type == GL_UNSIGNED_BYTE;
        break;
      case GL_R8_SNORM:
        valid = format == GL_RED && type == GL_BYTE;
        break;
      case GL_RG16F:
        valid = format == GL_RG && (type == GL_HALF_FLOAT || type == GL_FLOAT);
        break;
      case GL_RG16I:
        valid = format == GL_RG_INTEGER && type == GL_SHORT;
        break;
      case GL_RG16UI:
        valid = format == GL_RG_INTEGER && type == GL_UNSIGNED_SHORT;
        break;
      case GL_RG32F:
        valid = format == GL_RG && (type == GL_HALF_FLOAT || type == GL_FLOAT);
        break;
      case GL_RG32I:
        valid = format == GL_RG_INTEGER && type == GL_INT;
        break;
      case GL_RG32UI:
        valid = format == GL_RG_INTEGER && type == GL_UNSIGNED_INT;
        break;
      case GL_RG8:
        valid = format == GL_RG && type == GL_UNSIGNED_BYTE;
        break;
      case GL_RG8I:
        valid = format == GL_RG_INTEGER && type == GL_BYTE;
        break;
      case GL_RG8UI:
        valid = format == GL_RG_INTEGER && type == GL_UNSIGNED_BYTE;
        break;
      case GL_RGB:
        valid = format == GL_RGB &&
                (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_6_5);
        break;
      case GL_RG8_SNORM:
        valid = format == GL_RG && type == GL_BYTE;
        break;
      case GL_RGB10_A2:
        valid = format == GL_RGBA && type == GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
      case GL_RGB10_A2UI:
        valid =
            format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
      case GL_RGB16F:
        valid = format == GL_RGB && (type == GL_HALF_FLOAT || type == GL_FLOAT);
        break;
      case GL_RGB16I:
        valid = format == GL_RGB_INTEGER && type == GL_SHORT;
        break;
      case GL_RGB16UI:
        valid = format == GL_RGB_INTEGER && type == GL_UNSIGNED_SHORT;
        break;
      case GL_RGB32F:
        valid = format == GL_RGB && type == GL_FLOAT;
        break;
      case GL_RGB32I:
        valid = format == GL_RGB_INTEGER && type == GL_INT;
        break;
      case GL_RGB32UI:
        valid = format == GL_RGB_INTEGER && type == GL_UNSIGNED_INT;
        break;
      case GL_RGB565:
        valid = format == GL_RGB &&
                (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_5_6_5);
        break;
      case GL_RGB5_A1:
        valid = format == GL_RGBA && (type == GL_UNSIGNED_BYTE ||
                                      type == GL_UNSIGNED_SHORT_5_5_5_1 ||
                                      type == GL_UNSIGNED_INT_2_10_10_10_REV);
        break;
      case GL_RGB8:
      case GL_SRGB8:
        valid = format == GL_RGB && type == GL_UNSIGNED_BYTE;
        break;
      case GL_RGB8I:
        valid = format == GL_RGB_INTEGER && type == GL_BYTE;
        break;
      case GL_RGB8UI:
        valid = format == GL_RGB_INTEGER && type == GL_UNSIGNED_BYTE;
        break;
      case GL_RGB8_SNORM:
        valid = format == GL_RGB && type == GL_BYTE;
        break;
      case GL_RGB9_E5:
        valid = format == GL_RGB && (type == GL_UNSIGNED_INT_5_9_9_9_REV ||
                                     type == GL_HALF_FLOAT || type == GL_FLOAT);
        break;
      case GL_RGBA:
        valid = format == GL_RGBA && (type == GL_UNSIGNED_BYTE ||
                                      type == GL_UNSIGNED_SHORT_4_4_4_4 ||
                                      type == GL_UNSIGNED_SHORT_5_5_5_1 ||
                                      type == GL_FLOAT);
        break;
      case GL_RGBA16F:
        valid =
            format == GL_RGBA && (type == GL_HALF_FLOAT || type == GL_FLOAT);
        break;
      case GL_RGBA16I:
        valid = format == GL_RGBA_INTEGER && type == GL_SHORT;
        break;
      case GL_RGBA16UI:
        valid = format == GL_RGBA_INTEGER && type == GL_UNSIGNED_SHORT;
        break;
      case GL_RGBA32F:
        valid = format == GL_RGBA && type == GL_FLOAT;
        break;
      case GL_RGBA32I:
        valid = format == GL_RGBA_INTEGER && type == GL_INT;
        break;
      case GL_RGBA32UI:
        valid = format == GL_RGBA_INTEGER && type == GL_UNSIGNED_INT;
        break;
      case GL_RGBA4:
        valid = format == GL_RGBA &&
                (type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_SHORT_4_4_4_4);
        break;
      case GL_RGBA8:
      case GL_SRGB8_ALPHA8:
        valid = format == GL_RGBA && type == GL_UNSIGNED_BYTE;
        break;
      case GL_RGBA8I:
        valid = format == GL_RGBA_INTEGER && type == GL_BYTE;
        break;
      case GL_RGBA8UI:
        valid = format == GL_RGBA_INTEGER && type == GL_UNSIGNED_BYTE;
        break;
      case GL_RGBA8_SNORM:
        valid = format == GL_RGBA && type == GL_BYTE;
        break;
    }
    return CheckGlOperation(valid);
  }
  // Returns the active texture for the passed target; assumes that target is a
  // valid texture enum.
  GLuint& GetActiveTexture(GLenum target) {
    GLuint* active = nullptr;
    if (target == GL_TEXTURE_1D_ARRAY)
      active = &image_units_[active_objects_.image_unit].texture_1d_array;
    else if (target == GL_TEXTURE_2D)
      active = &image_units_[active_objects_.image_unit].texture_2d;
    else if (target == GL_TEXTURE_2D_ARRAY)
      active = &image_units_[active_objects_.image_unit].texture_2d_array;
    else if (target == GL_TEXTURE_2D_MULTISAMPLE)
      active = &image_units_[active_objects_.image_unit].texture_2d_multisample;
    else if (target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY)
      active = &image_units_[active_objects_.image_unit]
                             .texture_2d_multisample_array;
    else if (target == GL_TEXTURE_3D)
      active = &image_units_[active_objects_.image_unit].texture_3d;
    else if (target == GL_TEXTURE_CUBE_MAP || IsCubeFaceTarget(target))
      active =
          &image_units_[active_objects_.image_unit].cubemap;
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
      active =
          &image_units_[active_objects_.image_unit].cubemap_array;
    else if (target == GL_TEXTURE_EXTERNAL_OES)
      active = &image_units_[active_objects_.image_unit].texture_external;

    DCHECK(active);
    return *active;
  }
  // Returns the Image::PixelFormat corresponding to the passed enum.
  Image::PixelFormat GetImageTypeAndFormatFromInternalFormat(
      GLenum internalformat) {
    Image::PixelFormat pf;
    for (uint32 i = 0; i < Image::kNumFormats; ++i) {
      const Image::Format format = static_cast<Image::Format>(i);
      pf = Image::GetPixelFormat(format);
      if (pf.internal_format == internalformat)
        break;
    }
    return pf;
  }
  // Returns the number of mip levels for a given texture target for the texture
  // with the passed dimensions.
  GLsizei GetTextureMipMapLevelCount(GLenum target, GLsizei width,
                                    GLsizei height, GLsizei depth) {
    GLsizei levels = 0U;
    if (target == GL_TEXTURE_1D_ARRAY)
      levels = math::Log2(width);
    else if (target == GL_TEXTURE_3D)
      levels = math::Log2(std::max(width, std::max(height, depth)));
    else
      levels = math::Log2(std::max(width, height));
    return levels + 1;
  }
  // Returns whether target is a cubemap texture type.
  bool IsCubeFaceTarget(GLenum target) {
    return target == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
           target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X ||
           target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
           target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y ||
           target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z ||
           target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
  }
  // Returns whether type is a sampler uniform type.
  bool IsSamplerUniform(GLenum type) {
    return type == GL_INT_SAMPLER_1D || type == GL_INT_SAMPLER_1D_ARRAY ||
           type == GL_INT_SAMPLER_2D || type == GL_INT_SAMPLER_2D_ARRAY ||
           type == GL_INT_SAMPLER_3D || type == GL_INT_SAMPLER_CUBE ||
           type == GL_INT_SAMPLER_CUBE_MAP_ARRAY || type == GL_SAMPLER_1D ||
           type == GL_SAMPLER_1D_ARRAY || type == GL_SAMPLER_1D_ARRAY_SHADOW ||
           type == GL_SAMPLER_1D_SHADOW || type == GL_SAMPLER_2D ||
           type == GL_SAMPLER_2D_ARRAY || type == GL_SAMPLER_2D_ARRAY_SHADOW ||
           type == GL_SAMPLER_2D_SHADOW || type == GL_SAMPLER_3D ||
           type == GL_SAMPLER_CUBE || type == GL_SAMPLER_CUBE_MAP_ARRAY ||
           type == GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW ||
           type == GL_SAMPLER_EXTERNAL_OES || type == GL_SAMPLER_CUBE_SHADOW ||
           type == GL_UNSIGNED_INT_SAMPLER_1D ||
           type == GL_UNSIGNED_INT_SAMPLER_1D_ARRAY ||
           type == GL_UNSIGNED_INT_SAMPLER_2D ||
           type == GL_UNSIGNED_INT_SAMPLER_2D_ARRAY ||
           type == GL_UNSIGNED_INT_SAMPLER_3D ||
           type == GL_UNSIGNED_INT_SAMPLER_CUBE ||
           type == GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY;
  }
  // Returns whether target is a 2D texture type.
  bool IsTexture2dTarget(GLenum target) {
    return target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D;
  }
  // Returns whether target is a 2D multisample texture type.
  bool IsTexture2dMultisampleTarget(GLenum target) {
    return target == GL_TEXTURE_2D_MULTISAMPLE;
  }
  // Returns whether target is a 3D texture type.
  bool IsTexture3dTarget(GLenum target) {
    return target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_3D;
  }
  // Returns whether target is a 3D multisample texture type.
  bool IsTexture3dMultisampleTarget(GLenum target) {
    return target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
  }

  // These functions check whether an integer is a valid GL object name
  // generated by the relevant glGen* function. Note that is different from the
  // relevant glIs* function returning GL_TRUE, which only starts happening
  // after the objects are first bound. The exceptions are glIsProgram() and
  // glIsShader(), since shaders and programs do not conform to the OpenGL
  // object model.
  bool IsBufferName(GLuint buffer) {
    return object_state_->buffers.count(buffer) &&
        !object_state_->buffers[buffer].deleted;
  }
  bool IsFramebufferName(GLuint framebuffer) {
    return container_state_->framebuffers.count(framebuffer) &&
        !container_state_->framebuffers[framebuffer].deleted;
  }
  bool IsRenderbufferName(GLuint renderbuffer) {
    return object_state_->renderbuffers.count(renderbuffer) &&
        !object_state_->renderbuffers[renderbuffer].deleted;
  }
  bool IsTextureName(GLuint texture) {
    return object_state_->textures.count(texture) &&
        !object_state_->textures[texture].deleted;
  }
  bool IsTransformFeedbackName(GLuint id) {
    return container_state_->transform_feedbacks.count(id) &&
        !container_state_->transform_feedbacks[id].deleted;
  }
  bool IsVertexArrayName(GLuint array) {
    return container_state_->arrays.count(array) &&
        !container_state_->arrays[array].deleted;
  }

  //---------------------------------------------------------------------------
  // Each of these functions implements the corresponding OpenGL function using
  // local shadowed state instead of the real thing.

  // Core group.
  void ActiveTexture(GLenum texture) {
    // GL_INVALID_ENUM is generated if texture is not one of GL_TEXTUREi, where
    // i ranges from 0 to (GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS - 1).
    if (CheckGlEnum(texture >= GL_TEXTURE0 &&
                    (texture < GL_TEXTURE0 + max_texture_image_units_)) &&
        CheckFunction("ActiveTexture"))
      active_objects_.image_unit = texture - GL_TEXTURE0;
  }
  void AttachShader(GLuint program, GLuint shader) {
    // GL_INVALID_VALUE is generated if either program or shader is not a value
    // generated by OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // GL_INVALID_OPERATION is generated if shader is not a shader object.
    if (CheckProgram(program) && CheckShader(shader) &&
        CheckFunction("AttachShader")) {
      ShaderObject& so = object_state_->shaders[shader];
      ProgramObject& po = object_state_->programs[program];
      // GL_INVALID_OPERATION is generated if shader is already attached to
      // program.
      if (CheckGlOperation(po.vertex_shader != shader &&
                           po.geometry_shader != shader &&
                           po.fragment_shader != shader &&
                           po.tess_ctrl_shader != shader &&
                           po.tess_eval_shader != shader)) {
        so.programs.insert(program);
        if (so.type == GL_COMPUTE_SHADER) {
          po.compute_shader = shader;
        } else if (so.type == GL_VERTEX_SHADER) {
          po.vertex_shader = shader;
        } else if (so.type == GL_GEOMETRY_SHADER) {
          po.geometry_shader = shader;
        } else if (so.type == GL_TESS_CONTROL_SHADER) {
          po.tess_ctrl_shader = shader;
        } else if (so.type == GL_TESS_EVALUATION_SHADER) {
          po.tess_eval_shader = shader;
        } else {
          po.fragment_shader = shader;
        }
      }
    }
  }
  void BindAttribLocation(GLuint program, GLuint index, const GLchar* name) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    if (CheckProgram(program) && CheckGlValue(index < max_vertex_attribs_) &&
        CheckFunction("BindAttribLocation")) {
      ProgramObject& po = object_state_->programs[program];
      std::string sname(name);
      // GL_INVALID_OPERATION is generated if program is not a program object.
      // GL_INVALID_OPERATION is generated if name starts with the reserved
      // prefix "gl_".
      if (CheckGlOperation(!po.deleted && sname.find("gl_") != 0U)) {
        // Find the attribute and set its index.
        for (size_t i = 0; i < po.attributes.size(); ++i) {
          if (po.attributes[i].name.compare(name) == 0) {
            // Matrix attributes have their columns bound in successive slots.
            const GLuint slots = GetAttributeSlotCount(po.attributes[i].type);
            for (GLuint j = 0; j < slots; ++j)
              if (CheckGlValue(i + j < max_vertex_attribs_))
                po.attributes[i + j].index = index + j;
            break;
          }
        }
      }
    }
  }
  void BindBuffer(GLenum target, GLuint buffer) {
    // GL_INVALID_ENUM is generated if target is not one of the allowable
    // values.
    // GL_INVALID_VALUE is generated if buffer is not a name previously
    // returned from a call to glGenBuffers.
    if (CheckBufferTarget(target) &&
        CheckGlValue(object_state_->buffers.count(buffer)) &&
        CheckFunction("BindBuffer")) {
      GetBufferIndex(target) = buffer;
      if (target == GL_ELEMENT_ARRAY_BUFFER) {
        container_state_->arrays[active_objects_.vertex_array].element_array =
            buffer;
      }
      object_state_->buffers[buffer].bindings.push_back(GetCallCount());
    }
  }

  void BindBufferIndexed(GLenum target, GLuint index, GLuint buffer,
                         GLintptr offset, GLsizeiptr size) {
    // In this function a value of size == -1 means to bind an entire buffer.
    // This is to support the spec behavior of extending the binding if the
    // buffer size changes as a result of a call to glBufferData.
    // GL_INVALID_ENUM is generated if target is not one of the allowable
    // values.
    // GL_INVALID_VALUE is generated if buffer is not a name previously
    // returned from a call to glGenBuffers.
    if (!CheckGlValue(object_state_->buffers.count(buffer)) ||
        !CheckFunction("BindBufferRange")) return;
    const BufferObject& bo = object_state_->buffers[buffer];
    std::vector<IndexedBufferBinding>* bindings = nullptr;
    switch (target) {
      // We do not yet support ATOMIC_COUNTER_BUFFER or SHADER_STORAGE_BUFFER.
      case GL_TRANSFORM_FEEDBACK_BUFFER:
        bindings = &active_objects_.transform_feedback_buffers;
        break;
      case GL_UNIFORM_BUFFER:
        bindings = &active_objects_.uniform_buffers;
        break;
      default:
        CheckGlEnum(false);
        return;
    }
    DCHECK(bindings);
    // GL_INVALID_VALUE is generated if index is greater than or equal to the
    // number of target-specific indexed binding points.
    // GL_INVALID_VALUE is generated if ... offset + size is greater than the
    // value of GL_BUFFER_SIZE.
    const GLsizeiptr check_size = std::max(static_cast<GLsizeiptr>(1), size);
    if (!CheckGlValue(index < bindings->size() &&
                      offset + check_size <= bo.size)) return;
    IndexedBufferBinding* binding = &bindings->at(index);
    binding->id = buffer;
    binding->offset = offset;
    binding->size = size;
    object_state_->buffers[buffer].bindings.push_back(GetCallCount());
  }
  void BindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    BindBufferIndexed(target, index, buffer, 0, -1);
  }
  void BindBufferRange(GLenum target, GLuint index, GLuint buffer,
                       GLintptr offset, GLsizeiptr size) {
    // GL_INVALID_VALUE is generated if size is less than or equal to zero.
    if (!CheckGlValue(size > 0) || !CheckFunction("BindBufferRange")) return;
    BindBufferIndexed(target, index, buffer, offset, size);
  }

  void BindFramebuffer(GLenum target, GLuint framebuffer) {
    // GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER,
    // GL_READ_FRAMEBUFFER or GL_DRAW_FRAMEBUFFER.
    // GL_INVALID_OPERATION is generated if framebuffer is not zero or the name
    // of a framebuffer previously returned from a call to glGenFramebuffers.
    if (CheckGlEnum(IsFramebufferTarget(target)) &&
        CheckGlOperation(IsFramebufferName(framebuffer)) &&
        CheckFunction("BindFramebuffer")) {
      if (target == GL_FRAMEBUFFER) {
        // Calling glBindFramebuffer with target set to GL_FRAMEBUFFER binds
        // framebuffer to both the read and draw framebuffer targets.
        active_objects_.draw_framebuffer = framebuffer;
        active_objects_.read_framebuffer = framebuffer;
      } else if (target == GL_READ_FRAMEBUFFER) {
        active_objects_.read_framebuffer = framebuffer;
      } else if (target == GL_DRAW_FRAMEBUFFER) {
        active_objects_.draw_framebuffer = framebuffer;
      }
      container_state_->framebuffers[framebuffer].bindings.push_back(
          GetCallCount());
    }
  }

  void BindRenderbuffer(GLenum target, GLuint renderbuffer) {
    // GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.
    // GL_INVALID_OPERATION is generated if renderbuffer is not zero or the name
    // of a renderbuffer previously returned from a call to glGenRenderbuffers.
    if (!CheckGlEnum(target == GL_RENDERBUFFER)) return;
    if (!CheckGlOperation(IsRenderbufferName(renderbuffer))) return;
    if (!CheckFunction("BindRenderbuffer")) return;
    active_objects_.renderbuffer = renderbuffer;
    if (renderbuffer != 0U)
      object_state_->renderbuffers[renderbuffer].bindings.push_back(
          GetCallCount());
  }

  void BindTexture(GLenum target, GLuint texture) {
    // GL_INVALID_ENUM is generated if target is not one of the allowable
    // values.
    if (!CheckGlEnum(IsTextureTarget(target))) return;
    // GL_INVALID_VALUE is generated if texture is not a name returned from a
    // previous call to glGenTextures.
    if (texture != 0 && !CheckGlValue(IsTextureName(texture))) return;
    // GL_INVALID_OPERATION is generated if texture was previously created
    // with a target that doesn't match that of target.
    GLenum creation_target = object_state_->textures[texture].target;
    if (texture != 0 &&
        !CheckGlOperation(creation_target == target ||
                          creation_target == static_cast<GLenum>(-1)))
      return;
    if (CheckFunction("BindTexture")) {
      GLuint& active = GetActiveTexture(target);
      active = texture;
      object_state_->textures[texture].bindings.push_back(GetCallCount());
    }
  }
  void BlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    blend_color_[0] = Clampf(red);
    blend_color_[1] = Clampf(green);
    blend_color_[2] = Clampf(blue);
    blend_color_[3] = Clampf(alpha);
  }
  void BlendEquation(GLenum mode) {
    // GL_INVALID_ENUM is generated if mode is not one of GL_FUNC_ADD,
    // GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT (Desktop: GL_MAX, or GL_MIN)
    if (CheckBlendEquation(mode) && CheckFunction("BlendEquation"))
      rgb_blend_equation_ = alpha_blend_equation_ = mode;
  }
  void BlendEquationSeparate(GLenum mode_rgb, GLenum mode_alpha) {
    // GL_INVALID_ENUM is generated if either modeRGB or modeAlpha is not one of
    // GL_FUNC_ADD, GL_FUNC_SUBTRACT, or GL_FUNC_REVERSE_SUBTRACT.
    if (CheckBlendEquation(mode_rgb) && CheckBlendEquation(mode_alpha) &&
        CheckFunction("BlendEquationSeparate")) {
      rgb_blend_equation_ = mode_rgb;
      alpha_blend_equation_ = mode_alpha;
    }
  }
  void BlendFunc(GLenum sfactor, GLenum dfactor) {
    // GL_INVALID_ENUM is generated if either sfactor or dfactor is not an
    // accepted value.
    if (CheckBlendFunc(sfactor) && CheckBlendFunc(dfactor) &&
        CheckFunction("BlendFunc")) {
      rgb_blend_source_factor_ = alpha_blend_source_factor_ = sfactor;
      rgb_blend_destination_factor_ = alpha_blend_destination_factor_ = dfactor;
    }
  }
  void BlendFuncSeparate(GLenum sfactor_rgb, GLenum dfactor_rgb,
                         GLenum sfactor_alpha, GLenum dfactor_alpha) {
    // GL_INVALID_ENUM is generated if srcRGB, dstRGB, srcAlpha, or dstAlpha is
    // not an accepted value.
    if (CheckBlendFunc(sfactor_rgb) && CheckBlendFunc(dfactor_rgb) &&
        CheckBlendFunc(sfactor_alpha) && CheckBlendFunc(dfactor_alpha) &&
        CheckFunction("BlendFuncSeparate")) {
      rgb_blend_source_factor_ = sfactor_rgb;
      alpha_blend_source_factor_ = sfactor_alpha;
      rgb_blend_destination_factor_ = dfactor_rgb;
      alpha_blend_destination_factor_ = dfactor_alpha;
    }
  }
  void BufferData(GLenum target, GLsizeiptr size, const GLvoid* data,
                  GLenum usage) {
    // GL_INVALID_ENUM is generated if target is not one of the allowable
    // values.
    // GL_INVALID_ENUM is generated if usage is not GL_STREAM_DRAW,
    // GL_STATIC_DRAW, or GL_DYNAMIC_DRAW.
    // GL_INVALID_VALUE is generated if size is negative.
    // GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is
    // bound to target.
    // GL_OUT_OF_MEMORY is generated if the GL is unable to create a data store
    // with the specified size.
    if (CheckBufferTarget(target) && CheckBufferUsage(usage) &&
        CheckGlValue(size >= 0) && CheckBufferZeroNotBound(target) &&
        CheckGlMemory(size) && CheckFunction("BufferData")) {
      const GLuint index = GetBufferIndex(target);
      object_state_->buffers[index].size = size;
      object_state_->buffers[index].usage = usage;
      if (object_state_->buffers[index].data)
          object_state_->buffers[index].ClearData();
      object_state_->buffers[index].data =
          reinterpret_cast<void*>(new uint8[size]);
      // Copy the data if it is non-null.
      if (data) {
        std::memcpy(object_state_->buffers[index].data, data, size);
      }
    }
  }
  void BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                     const GLvoid* data) {
    // GL_INVALID_ENUM is generated if target is not GL_ARRAY_BUFFER or
    // GL_ELEMENT_ARRAY_BUFFER.
    // GL_INVALID_VALUE is generated if offset or size is negative, or if
    // together they define a region of memory that extends beyond the buffer
    // object's allocated data store.
    // GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is
    // bound to target.
    if (CheckBufferTarget(target) && CheckGlValue(offset >= 0 && size >= 0) &&
        CheckBufferZeroNotBound(target)) {
      const GLuint index = GetBufferIndex(target);
      if (CheckGlValue(object_state_->buffers[index].size > offset + size) &&
          CheckFunction("BufferSubData")) {
        // Copy the data.
        if (data) {
          uint8* int_data =
              reinterpret_cast<uint8*>(object_state_->buffers[index].data);
          std::memcpy(&int_data[offset], data, size);
        }
      }
    }
  }
  void CopyBufferSubData(GLenum read_target, GLenum write_target,
                         GLintptr read_offset, GLintptr write_offset,
                         GLsizeiptr size) {
    // GL_INVALID_ENUM is generated if target is not GL_ARRAY_BUFFER or
    // GL_ELEMENT_ARRAY_BUFFER.
    // GL_INVALID_VALUE is generated if offsets or size is negative, or if
    // together they define a region of memory that extends beyond the buffer
    // object's allocated data store or if the read/write ranges overlap.
    // GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is
    // bound to either target or if either read/write bufferobjects are mapped.
    if (CheckBufferTarget(read_target) &&
        CheckBufferTarget(write_target) &&
        CheckGlValue(write_offset >= 0) &&
        CheckGlValue(size >= 0) &&
        CheckGlValue(read_offset >= 0) &&
        CheckBufferZeroNotBound(read_target) &&
        CheckBufferZeroNotBound(write_target)) {
      const GLuint read_index = GetBufferIndex(read_target);
      const GLuint write_index = GetBufferIndex(write_target);
      BufferObject& src = object_state_->buffers[read_index];
      BufferObject& dst = object_state_->buffers[write_index];
      if (CheckGlOperation(src.mapped_data == nullptr) &&
          CheckGlOperation(dst.mapped_data == nullptr) &&
          CheckGlValue(dst.size >= write_offset + size) &&
          CheckGlValue(src.size >= read_offset + size) &&
          (read_index != write_index ||
           CheckGlValue(read_offset + size <= write_offset ||
                        write_offset + size <= read_offset)) &&
          CheckFunction("CopyBufferSubData")) {
        // Copy the data.
        uint8* int_data = reinterpret_cast<uint8*>(dst.data);
        std::memcpy(int_data + write_offset,
                    reinterpret_cast<uint8*>(src.data) + read_offset, size);
      }
    }
  }
  GLenum CheckFramebufferStatus(GLenum target, GLuint framebuffer_name) {
    // GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.
    // Possible return values:
    // GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
    // Not all framebuffer attachment points are framebuffer attachment
    // complete. This means that at least one attachment point with a
    // renderbuffer or texture attached has its attached object no longer in
    // existence or has an attached image with a width or height of zero, or the
    // color attachment point has a non-color-renderable image attached, or the
    // depth attachment point has a non-depth-renderable image attached, or the
    // stencil attachment point has a non-stencil-renderable image attached.
    // Color-renderable formats include GL_RGBA4, GL_RGB5_A1, and GL_RGB565.
    // Depth-renderable formats include GL_DEPTH_COMPONENT16, and
    // stencil-renderable formats include GL_STENCIL_INDEX8.
    //
    // GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
    // Not all attached images have the same width and height.
    //
    // GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
    // No images are attached to the framebuffer.
    //
    // GL_FRAMEBUFFER_UNSUPPORTED
    // The combination of internal formats of the attached images violates an
    // implementation-dependent set of restrictions.
    if (CheckGlEnum(IsFramebufferTarget(target)) &&
        CheckGlOperation(IsFramebuffer(framebuffer_name)) &&
        CheckFunction("CheckFramebufferStatus")) {
      // Update the status of the framebuffer.
      // The base framebuffer is always complete.
      if (framebuffer_name == 0)
        return GL_FRAMEBUFFER_COMPLETE;

      // Prepare helper vector that simplifies code.
      const FramebufferObject& fbo =
          container_state_->framebuffers[framebuffer_name];
      std::vector<const FramebufferObject::Attachment*> attachments;
      attachments.reserve(max_color_attachments_ + 2);
      for (const auto& attachment : fbo.color)
        attachments.push_back(&attachment);
      attachments.push_back(&fbo.depth);
      attachments.push_back(&fbo.stencil);

      // Check whether we have any attachments.
      bool has_attachment = false;
      for (auto attachment_ptr : attachments) {
        if (attachment_ptr->type != GL_NONE) {
          has_attachment = true;
          break;
        }
      }
      if (!has_attachment) {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
      }
      // Check attachment compatibility. We check every combination, since
      // an empty attachment will be compatible with everything.
      for (size_t i = 0; i < attachments.size(); ++i) {
        for (size_t j = i + 1; j < attachments.size(); ++j) {
          if (AttachmentsAreIncompatible(*attachments[i], *attachments[j])) {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
          }
        }
      }
      // Check attachment completeness.
      for (auto attachment_ptr : attachments) {
        if (AttachmentIsIncomplete(fbo, *attachment_ptr)) {
          return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
      }
      // Check multisampling and views.
      int prev_samples = -1;
      GLsizei prev_views = -1;
      bool prev_implicit = false;
      for (auto attachment_ptr : attachments) {
        int samples = GetAttachmentSamples(*attachment_ptr);
        GLsizei views = attachment_ptr->num_views;
        bool implicit = IsAttachmentImplicitlyMultisampled(*attachment_ptr);
        if (samples > -1 && prev_samples > -1 &&
            (prev_samples != samples || prev_implicit != implicit)) {
          return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
        }
        if (views > 0 && prev_views > 0 && prev_views != views)
          return GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR;
        if (samples > -1) {
          prev_samples = samples;
          prev_implicit = implicit;
        }
        if (views > 0)
          prev_views = views;
      }
      // Check whether draw buffers are valid.
      for (GLenum draw_buffer : fbo.draw_buffers) {
        if (draw_buffer != GL_NONE) {
          FramebufferObject::Attachment* a =
              GetAttachment(target, draw_buffer);
          if (a->type == GL_NONE)
            return GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER;
        }
      }
      // Check whether the read buffer is valid.
      if (fbo.read_buffer != GL_NONE) {
        FramebufferObject::Attachment* a =
            GetAttachment(target, fbo.read_buffer);
        if (a->type == GL_NONE)
          return GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER;
      }

      return GL_FRAMEBUFFER_COMPLETE;
    } else {
      return 0U;
    }
  }
  GLenum CheckFramebufferStatus(GLenum target) {
    GLuint framebuffer = target == GL_READ_FRAMEBUFFER
        ? active_objects_.read_framebuffer : active_objects_.draw_framebuffer;
    return CheckFramebufferStatus(target, framebuffer);
  }
  void Clear(GLbitfield mask) {
    // GL_INVALID_VALUE is generated if any bit other than the three defined
    // bits is set in mask.
    static const GLbitfield kAllBits =
        GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    CheckGlValue((mask & ~kAllBits) == 0);
    CheckFunction("Clear");
    // There is nothing to do since we do not implement draw functions.
  }
  void ClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    clear_color_[0] = Clampf(red);
    clear_color_[1] = Clampf(green);
    clear_color_[2] = Clampf(blue);
    clear_color_[3] = Clampf(alpha);
  }
  void ClearDepthf(GLfloat depth) { clear_depth_value_ = Clampf(depth); }
  void ClearStencil(GLint s) { clear_stencil_value_ = s; }
  void ColorMask(GLboolean red, GLboolean green, GLboolean blue,
                 GLboolean alpha) {
    color_write_masks_[0] = red;
    color_write_masks_[1] = green;
    color_write_masks_[2] = blue;
    color_write_masks_[3] = alpha;
  }
  void CompileShader(GLuint shader) {
    // GL_INVALID_VALUE is generated if shader is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if shader is not a shader object.
    if (CheckShader(shader)) {
      ShaderObject& so = object_state_->shaders[shader];
      if (CheckGlOperation(!so.deleted)) {
        if (CheckFunction("CompileShader")) {
          so.compile_status = GL_TRUE;
          so.info_log.clear();
        } else {
          // Set the info log.
          so.compile_status = GL_FALSE;
          so.info_log = "Shader compilation is set to always fail.";
        }
      }
    }
  }
  void CompressedTexImage2D(GLenum target, GLint level, GLenum internal_format,
                            GLsizei width, GLsizei height, GLint border,
                            GLsizei image_size, const GLvoid* data) {
    if (CheckGlEnum(
            // GL_INVALID_ENUM is generated if target is not
            // GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_Z, or
            // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z.
            CheckTexture2dTargetType(target) &&
            // GL_INVALID_ENUM is generated if internal_format is not a
            // supported format returned in GL_COMPRESSED_TEXTURE_FORMATS.
            CheckCompressedTextureFormat(internal_format)) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_ARRAY_TEXTURE_LAYERS when target is GL_TEXTURE_1D_ARRAY,
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if width or height is less than 0
            // or greater than GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D
            // or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureDimensions(target, width, height, 1) &&
            // GL_INVALID_VALUE is generated if border is not 0.
            border == 0 &&
            // GL_INVALID_VALUE is generated if image_size is not consistent
            // with the format, dimensions, and contents of the specified
            // compressed image data.
            //
            // GL_INVALID_OPERATION is generated if parameter combinations are
            // not supported by the specific compressed internal format as
            // specified in the specific texture compression extension.
            //
            // 
            // above errors.
            image_size > 0)) {
      const GLenum tex_target =
          target == GL_TEXTURE_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP;
      const GLuint texture = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[texture];
      // Once a texture is marked immutable it cannot be modified.
      if (CheckGlOperation(!to.immutable) &&
          CheckFunction("CompressedTexImage2D")) {
        to.target = tex_target;
        // Type and format are not used for compressed textures.
        to.internal_format = internal_format;
        to.border = border;
        TextureObject::MipLevel miplevel;
        miplevel.width = width;
        miplevel.height = height;
        miplevel.depth = 1;
        miplevel.data.reset(new char[image_size]);
        if (data) {
          std::memcpy(miplevel.data.get(), data, image_size);
        }
        to.levels
            .resize(std::max(level + 1, static_cast<GLint>(to.levels.size())));
        to.levels[level] = miplevel;
        to.compressed = true;
      }
    }
  }
  void CompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                               GLint yoffset, GLsizei width, GLsizei height,
                               GLenum format, GLsizei imageSize,
                               const GLvoid* data) {
    if (CheckGlEnum(
            // GL_INVALID_ENUM is generated if target is not
            // GL_TEXTURE_1D_ARRAY, GL_TEXTURE_2D,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_Z, or
            // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z.
            CheckTexture2dTargetType(target) &&
            // GL_INVALID_ENUM is generated if internal_format is not a
            // supported format returned in GL_COMPRESSED_TEXTURE_FORMATS.
            CheckCompressedTextureFormat(format)) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if width or height is less than 0.
            width >= 0 && height >= 0 &&
            // GL_INVALID_VALUE is generated if imageSize is not consistent
            // with the format, dimensions, and contents of the specified
            // compressed image data.
            //
            // GL_INVALID_OPERATION is generated if parameter combinations are
            // not supported by the specific compressed internal format as
            // specified in the specific texture compression extension.
            //
            // 
            // above errors.
            imageSize > 0)) {
      const GLuint tex_index = GetActiveTexture(target);
      TextureObject& texture = object_state_->textures[tex_index];
      // GL_INVALID_VALUE is generated if xoffset < 0, xoffset + width > w,
      // yoffset < 0, or yoffset + height > h, where w is the width and h is the
      // height of the texture image being modified.
      // GL_INVALID_OPERATION is generated if the texture array has not
      // been defined by a previous glCompressedTexImage2D operation whose
      // internalformat matches the format of glCompressedTexSubImage2D.
      if (CheckGlOperation(texture.compressed) &&
          CheckGlOperation(level < static_cast<GLint>(texture.levels.size())) &&
          CheckGlValue(xoffset >= 0 &&
                       xoffset + width <= texture.levels[level].width &&
                       yoffset >= 0 &&
                       yoffset + height <= texture.levels[level].height) &&
          CheckFunction("CompressedTexSubImage2D")) {
        // Do nothing since we do not implement mock compression.
      }
    }
  }
  void CopyTexImage2D(GLenum target, GLint level, GLenum internalformat,
                      GLint x, GLint y, GLsizei width, GLsizei height,
                      GLint border) {
    if (CheckGlEnum(
            // GL_INVALID_ENUM is generated if target is not GL_TEXTURE_2D,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
            // GL_TEXTURE_CUBE_MAP_POSITIVE_Z, or
            // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z.
            CheckTexture2dTargetType(target) &&
            // GL_INVALID_ENUM is generated if internalformat is not an accepted
            // format.
            (internalformat == GL_ALPHA || internalformat == GL_RGB ||
             internalformat == GL_RGBA || internalformat == GL_LUMINANCE ||
             internalformat == GL_LUMINANCE_ALPHA)) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if target is one of the six cube
            // map 2D image targets and the width and height parameters are not
            // equal.
            ((IsCubeFaceTarget(target) && width == height) ||
             IsTexture2dTarget(target)) &&
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if width or height is less than 0.
            CheckTextureDimensions(target, width, height, 1) &&
            // GL_INVALID_VALUE is generated if width or height is less than 0
            // or greater than GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D
            // or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            // GL_INVALID_VALUE is generated if border is not 0.
            border == 0) &&
        CheckFunction("CopyTexImage2D")) {
      // GL_INVALID_OPERATION is generated if the currently bound framebuffer's
      // format does not contain a superset of the components required by the
      // base format of internalformat.
      // GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound
      // framebuffer is not framebuffer complete (i.e. the return value from
      // glCheckFramebufferStatus is not GL_FRAMEBUFFER_COMPLETE).
      // 
      if (CheckFramebuffer()) {
        // We don't copy mock texture data.
      }
    }
  }
  void CopyTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                         GLint yoffset, GLint x, GLint y, GLsizei width,
                         GLsizei height) {
    // GL_INVALID_ENUM is generated if target is not GL_TEXTURE_2D,
    // GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
    // GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
    // GL_TEXTURE_CUBE_MAP_POSITIVE_Z, or
    // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z.
    if (CheckGlEnum(CheckTexture2dTargetType(target)) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if width or height is less than 0.
            width >= 0 && height >= 0)) {
      const GLuint tex_id = GetActiveTexture(target);
      TextureObject& texture = object_state_->textures[tex_id];
      // GL_INVALID_VALUE is generated if xoffset < 0, xoffset + width > w,
      // yoffset < 0, or yoffset + height > h, where w is the width and h is the
      // height of the texture image being modified.
      // GL_INVALID_OPERATION is generated if the texture array has not been
      // defined by a previous glTexImage2D or glCopyTexImage2D operation.
      // GL_INVALID_OPERATION is generated if the currently bound framebuffer's
      // format does not contain a superset of the components required by the
      // base format of internalformat.
      // 
      // GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound
      // framebuffer is not framebuffer complete (i.e. the return value from
      // glCheckFramebufferStatus is not GL_FRAMEBUFFER_COMPLETE).
      if (CheckGlOperation(level < static_cast<GLint>(texture.levels.size())) &&
          CheckGlValue(xoffset >= 0 &&
                       xoffset + width <= texture.levels[level].width &&
                       yoffset >= 0 &&
                       yoffset + height <= texture.levels[level].height) &&
          CheckFramebuffer() && CheckFunction("CopyTexSubImage2D")) {
        // We don't copy mock texture data.
      }
    }
  }
  GLuint CreateProgram() {
    if (CheckFunction("CreateProgram")) {
      ProgramObject po;
      // OpenGL ids are 1-based.
      const GLuint id =
          static_cast<GLuint>(object_state_->programs.size() + 1);
      object_state_->programs[id] = po;
      return id;
    } else {
      return 0U;
    }
  }
  GLuint CreateShader(GLenum type) {
    GLuint id = 0U;
    // GL_INVALID_ENUM is generated if shaderType is not an accepted value.
    if (CheckShaderType(type) && CheckFunction("CreateShader")) {
      ShaderObject so;
      so.type = type;
      // OpenGL ids are 1-based.
      id = static_cast<GLuint>(object_state_->shaders.size() + 1);
      object_state_->shaders[id] = so;
    }
    return id;
  }
  void CullFace(GLenum mode) {
    // GL_INVALID_ENUM is generated if mode is not an accepted value.
    if (CheckFace(mode) && CheckFunction("CullFace"))
        cull_face_mode_ = mode;
  }
  void DeleteBuffers(GLsizei n, const GLuint* buffers) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("DeleteBuffers")) {
      for (GLsizei i = 0; i < n; ++i) {
        // glDeleteBuffers silently ignores 0's and names that do not correspond
        // to existing buffer objects.
        if (buffers[i] != 0U && IsBufferName(buffers[i])) {
          // Free any data storage.
          object_state_->buffers[buffers[i]].ClearData();
          // Reset the buffer object.
          object_state_->buffers[buffers[i]] = BufferObject();
          // Mark the buffer as deleted, so that it cannot be reused.
          object_state_->buffers[buffers[i]].deleted = true;

          // Reset the binding if the index is the currently bound object.
          if (buffers[i] == active_objects_.array_buffer)
              active_objects_.array_buffer = 0U;
          if (buffers[i] == active_objects_.element_array_buffer)
            active_objects_.element_array_buffer = 0U;
          if (buffers[i] == active_objects_.copy_read_buffer)
            active_objects_.copy_read_buffer = 0U;
          if (buffers[i] == active_objects_.copy_write_buffer)
            active_objects_.copy_write_buffer = 0U;
          if (buffers[i] == active_objects_.transform_feedback_buffer)
            active_objects_.transform_feedback_buffer = 0U;
          if (buffers[i] == active_objects_.dispatch_indirect_buffer)
            active_objects_.dispatch_indirect_buffer = 0U;
        }
      }
    }
  }
  void DeleteFramebuffers(GLsizei n, const GLuint* framebuffers) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("DeleteFramebuffers")) {
      for (GLsizei i = 0; i < n; ++i) {
        // glDeleteFramebuffers silently ignores 0's and names that do not
        // correspond to existing framebuffer objects.
        if (framebuffers[i] != 0U && IsFramebufferName(framebuffers[i])) {
          // Reset the framebuffer object.
          container_state_->framebuffers[framebuffers[i]] = FramebufferObject();
          // Mark the framebuffer as deleted, so that it cannot be reused.
          container_state_->framebuffers[framebuffers[i]].deleted = true;

          // Reset the binding if the index is the currently bound object.
          if (framebuffers[i] == active_objects_.draw_framebuffer)
            active_objects_.draw_framebuffer = 0U;
        }
      }
    }
  }
  void DeleteProgram(GLuint program) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    if (CheckProgram(program) &&
        CheckFunction("DeleteProgram")) {
      ProgramObject& po = object_state_->programs[program];
      // Mark the program for deletion. Note that setting these does not
      // make IsProgram return false; for that, the program must also not be
      // set as the active program.
      po.delete_status = GL_TRUE;
      po.deleted = true;
      // Detach all shaders.
      if (IsShader(po.vertex_shader))
        object_state_->shaders[po.vertex_shader].programs.erase(program);
      if (IsShader(po.geometry_shader))
        object_state_->shaders[po.geometry_shader].programs.erase(program);
      if (IsShader(po.fragment_shader))
        object_state_->shaders[po.fragment_shader].programs.erase(program);
      if (IsShader(po.compute_shader))
        object_state_->shaders[po.compute_shader].programs.erase(program);
    }
  }
  void DeleteRenderbuffers(GLsizei n, const GLuint* renderbuffers) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("DeleteRenderbuffers")) {
      for (GLsizei i = 0; i < n; ++i) {
        // glDeleteRenderbuffers silently ignores 0's and names that do not
        // correspond to existing renderbuffer objects.
        if (renderbuffers[i] != 0U && IsRenderbufferName(renderbuffers[i])) {
          // Reset the renderbuffer object.
          object_state_->renderbuffers[renderbuffers[i]] = RenderbufferObject();
          // Mark the renderbuffer as deleted, so that it cannot be reused.
          object_state_->renderbuffers[renderbuffers[i]].deleted = true;

          // Reset the binding if the index is the currently bound object.
          if (renderbuffers[i] == active_objects_.renderbuffer)
            active_objects_.renderbuffer = 0U;
        }
      }
    }
  }
  void DeleteShader(GLuint shader) {
    // GL_INVALID_VALUE is generated if shader is not a value generated by
    // OpenGL.
    if (CheckGlValue(IsShader(shader)) &&
        CheckFunction("DeleteShader")) {
      // Mark the shader for deletion. Note that setting these does not make
      // IsShader return false; for that, the shader must also not be attached
      // to any program.
      object_state_->shaders[shader].delete_status = GL_TRUE;
      object_state_->shaders[shader].deleted = true;
    }
  }
  void DeleteTextures(GLsizei n, const GLuint* textures) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("DeleteTextures")) {
      const size_t unit_count = image_units_.size();
      for (GLsizei i = 0; i < n; ++i) {
        // glDeleteTextures silently ignores 0's and names that do not
        // correspond to existing textures.
        if (textures[i] != 0U && IsTexture(textures[i])) {
          // Reset the texture object.
          object_state_->textures[textures[i]] = TextureObject();
          // Mark the texture as deleted, so that it cannot be reused.
          object_state_->textures[textures[i]].deleted = true;

          // Reset the binding if the index is the currently bound object.
          for (size_t j = 0; j < unit_count; ++j) {
            if (image_units_[j].texture_1d_array == textures[i])
              image_units_[j].texture_1d_array = 0;
            if (image_units_[j].texture_2d == textures[i])
              image_units_[j].texture_2d = 0;
            if (image_units_[j].texture_2d_array == textures[i])
              image_units_[j].texture_2d_array = 0;
            if (image_units_[j].texture_2d_multisample == textures[i])
              image_units_[j].texture_2d_multisample = 0;
            if (image_units_[j].texture_2d_multisample_array == textures[i])
              image_units_[j].texture_2d_multisample_array = 0;
            if (image_units_[j].texture_3d == textures[i])
              image_units_[j].texture_3d = 0;
            if (image_units_[j].cubemap == textures[i])
              image_units_[j].cubemap = 0;
            if (image_units_[j].cubemap_array == textures[i])
              image_units_[j].cubemap_array = 0;
          }
        }
      }
    }
  }
  void DepthFunc(GLenum func) {
    // GL_INVALID_ENUM is generated if func is not an accepted value.
    if (CheckDepthOrStencilFunc(func) && CheckFunction("DepthFunc"))
      depth_function_ = func;
  }
  void DepthMask(GLboolean flag) { depth_write_mask_ = flag; }
  void DepthRangef(GLfloat near_val, GLfloat far_val) {
    depth_range_ = math::Range1f(Clampf(near_val), Clampf(far_val));
  }
  void DetachShader(GLuint program, GLuint shader) {
    // GL_INVALID_VALUE is generated if either program or shader is not a value
    // generated by OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // GL_INVALID_OPERATION is generated if shader is not a shader object.
    if (CheckProgram(program) && CheckShader(shader) &&
        CheckFunction("DetachShader")) {
      ShaderObject& so = object_state_->shaders[shader];
      ProgramObject& po = object_state_->programs[program];
      // GL_INVALID_OPERATION is generated if shader is not attached to program.
      if (CheckGlOperation(po.vertex_shader == shader ||
                           po.geometry_shader == shader ||
                           po.fragment_shader == shader ||
                           po.compute_shader == shader)) {
        so.programs.erase(program);
        if (po.vertex_shader == shader)
          po.vertex_shader = 0;
        else if (po.geometry_shader == shader)
          po.geometry_shader = 0;
        else if (po.fragment_shader == shader)
          po.fragment_shader = 0;
        else
          po.compute_shader = 0;
      }
    }
  }
  void Disable(GLenum cap) {
    // GL_INVALID_ENUM is generated if cap is not a valid value.
    GLint index = GetAndVerifyCapabilityIndex(cap);
    if (CheckGlEnum(index >= 0 &&
                    index < static_cast<GLint>(enabled_state_.size())) &&
        CheckFunction("Disable"))
      enabled_state_[index] = false;
  }
  void DisableVertexAttribArray(GLuint index) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    if (CheckGlValue(index < max_vertex_attribs_) &&
        CheckFunction("DisableVertexAttribArray")) {
      ArrayObject& ao = container_state_->arrays[active_objects_.vertex_array];
      ao.attributes[index].enabled = GL_FALSE;
    }
  }
  void DrawArrays(GLenum mode, GLint first, GLsizei count) {
    GLuint tfo_id = active_objects_.transform_feedback;
    TransformFeedbackObject& tfo =
        container_state_->transform_feedbacks[tfo_id];
    // GL_INVALID_ENUM is generated if mode is not an accepted value.
    // GL_INVALID_VALUE is generated if count is negative.
    // GL_INVALID_OPERATION is generated if a non-zero buffer object name is
    // bound to an enabled array and the buffer object's data store is currently
    // mapped.
    // GL_INVALID_OPERATION is generated if transform feedback is active and
    // mode does not exactly match primitive_mode.
    if (CheckDrawMode(mode) && CheckGlValue(count >= 0) &&
        (active_objects_.array_buffer == 0 ||
         CheckGlOperation(
             object_state_->buffers[active_objects_.array_buffer].data !=
                 nullptr)) &&
        CheckGlOperation(!tfo.active || tfo.primitive_mode == mode) &&
        CheckFunction("DrawArrays")) {
      // There is nothing to do since we do not implement draw functions.
    }
  }
  void DrawElements(GLenum mode, GLsizei count, GLenum type,
                    const GLvoid* indices) {
    // GL_INVALID_ENUM is generated if mode is not an accepted value.
    // GL_INVALID_ENUM is generated if type is not GL_UNSIGNED_BYTE,
    // GL_UNSIGNED_INT or GL_UNSIGNED_SHORT.
    // GL_INVALID_VALUE is generated if count is negative.
    // GL_INVALID_OPERATION is generated if a non-zero buffer object name is
    // bound to an enabled array or the element array and the buffer object's
    // data store is currently mapped.
    // GL_INVALID_OPERATION is generated if transform feedback is active and not
    // paused.
    if (CheckDrawMode(mode) && CheckGlValue(count >= 0) &&
        CheckGlEnum(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_INT ||
                    type == GL_UNSIGNED_SHORT) &&
        (active_objects_.array_buffer == 0 ||
         CheckGlOperation(
             object_state_->buffers[active_objects_.array_buffer].data
                 != nullptr)) &&
        (active_objects_.element_array_buffer == 0 ||
         CheckGlOperation(
             object_state_->buffers[active_objects_.element_array_buffer]
                 .data != nullptr)) &&
        CheckGlOperation(
            !container_state_
                 ->transform_feedbacks[active_objects_.transform_feedback]
                 .active) &&
        CheckFunction("DrawElements")) {
      // There is nothing to do since we do not implement draw functions.
    }
  }
  void Enable(GLenum cap) {
    // GL_INVALID_ENUM is generated if cap is not a valid value.
    GLint index = GetAndVerifyCapabilityIndex(cap);
    if (CheckGlEnum(index >= 0 &&
                    index < static_cast<GLint>(enabled_state_.size())) &&
        CheckFunction("Enable"))
      enabled_state_[index] = true;
  }
  void EnableVertexAttribArray(GLuint index) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    if (CheckGlValue(index < max_vertex_attribs_) &&
        CheckFunction("EnableVertexAttribArray")) {
      ArrayObject& ao = container_state_->arrays[active_objects_.vertex_array];
      ao.attributes[index].enabled = GL_TRUE;
    }
  }
  void Finish() {
    // Nothing to do.
  }
  void Flush() {
    // Nothing to do.
  }
  void FramebufferRenderbuffer(GLenum target, GLenum attachment,
                               GLenum renderbuffertarget, GLuint renderbuffer) {
    // GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.
    // GL_INVALID_ENUM is generated if renderbuffertarget is not GL_RENDERBUFFER
    // and renderbuffer is not 0.
    // GL_INVALID_ENUM is generated if attachment is not an accepted attachment
    // point.
    // GL_INVALID_OPERATION is generated if the default framebuffer object name
    // 0 is bound.
    // GL_INVALID_OPERATION is generated if renderbuffer is neither 0 nor the
    // name of an existing renderbuffer object.
    if (CheckGlEnum(
            IsFramebufferTarget(target) &&
            renderbuffertarget == GL_RENDERBUFFER &&
            IsAttachmentEnum(attachment)) &&
        CheckGlOperation(active_objects_.draw_framebuffer != 0U &&
                         (renderbuffer == 0U ||
                          IsRenderbuffer(renderbuffer))) &&
        CheckFunction("FramebufferRenderbuffer")) {
      auto DoSet = [=](GLenum slot) -> void {
        FramebufferObject::Attachment* a =
            GetClearedAttachment(target, slot);
        if (renderbuffer == 0U)
          a->type = GL_NONE;
        else
          a->type = GL_RENDERBUFFER;
        a->value = renderbuffer;
      };
      if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        DoSet(GL_DEPTH_ATTACHMENT);
        DoSet(GL_STENCIL_ATTACHMENT);
      } else {
        DoSet(attachment);
      }
    }
  }
  void FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                            GLuint texture, GLint level) {
    if (!CheckFunction("FramebufferTexture2D")) return;
    SetFramebufferTexture(target, attachment, textarget, texture, level, -1, 0,
                          0);
  }
  void FrontFace(GLenum mode) {
    // GL_INVALID_ENUM is generated if mode is not an accepted value.
    if (CheckGlEnum(mode == GL_CW || mode == GL_CCW) &&
        CheckFunction("FrontFace"))
      front_face_mode_ = mode;
  }
  void GenBuffers(GLsizei n, GLuint* buffers) {
    // We generate a synthetic GL_INVALID_OPERATION if
    // gen_buffers_always_fails_ is set
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckFunction("GenBuffers") && CheckGlValue(n >= 0) &&
        CheckFunction("GenBuffers")) {
      for (GLsizei i = 0; i < n; ++i) {
        BufferObject bo;
        // OpenGL ids are 1-based, but there is a default buffer at index 0.
        const GLuint id = static_cast<GLuint>(object_state_->buffers.size());
        object_state_->buffers[id] = bo;
        buffers[i] = id;
      }
    }
  }
  void GenerateMipmap(GLenum target) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted
    // targets.
    if (CheckGlEnum(IsTextureTarget(target))) {
      // GL_INVALID_OPERATION is generated if the texture bound to target is a
      // cube map, but its six faces do not share indentical widths, heights,
      // formats, and types.
      //
      // 
      // maps.
      //
      // GL_INVALID_OPERATION is generated if either the width or height of
      // the zero level array is not a power of two.
      // GL_INVALID_OPERATION is generated if the zero level array is stored
      // in a compressed internal format.
      const GLuint texture = GetActiveTexture(target);
      const TextureObject& to = object_state_->textures[texture];
      if (CheckGlOperation(
              to.levels.size() && math::IsPowerOfTwo(to.levels[0].width) &&
              math::IsPowerOfTwo(to.levels[0].height) && !to.compressed) &&
          CheckFunction("GenerateMipmap")) {
        // There is nothing to do since we do not implement data manipulation.
      }
    }
  }
  void GenFramebuffers(GLsizei n, GLuint * framebuffers) {
    // We generate a synthetic GL_INVALID_OPERATION if
    // gen_framebuffers_always_fails_ is set
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("GenFramebuffers")) {
      for (GLsizei i = 0; i < n; ++i) {
        FramebufferObject fbo;
        fbo.color.resize(max_color_attachments_);
        fbo.draw_buffers.resize(max_draw_buffers_, GL_NONE);
        fbo.draw_buffers[0] = GL_COLOR_ATTACHMENT0;
        fbo.read_buffer = GL_COLOR_ATTACHMENT0;
        // OpenGL ids are 1-based, but there is a default framebuffer at index
        // 0.
        const GLuint id = static_cast<GLuint>(
            container_state_->framebuffers.size());
        container_state_->framebuffers[id] = fbo;
        framebuffers[i] = id;
      }
    }
  }
  void GenRenderbuffers(GLsizei n, GLuint* renderbuffers) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("GenRenderbuffers")) {
      for (GLsizei i = 0; i < n; ++i) {
        RenderbufferObject rbo;
        // OpenGL ids are 1-based, but there is a default renderbuffer at index
        // 0.
        const GLuint id = static_cast<GLuint>(
            object_state_->renderbuffers.size());
        object_state_->renderbuffers[id] = rbo;
        renderbuffers[i] = id;
      }
    }
  }
  void GenTextures(GLsizei n, GLuint* textures) {
    // We generate a synthetic GL_INVALID_OPERATION if
    // gen_textures_always_fails_ is set
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("GenTextures")) {
      for (GLsizei i = 0; i < n; ++i) {
        TextureObject to;
        // OpenGL ids are 1-based, but there is a default texture at index 0.
        const GLuint id = static_cast<GLuint>(object_state_->textures.size());
        object_state_->textures[id] = to;
        textures[i] = id;
      }
    }
  }
  void GetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize,
                       GLsizei* length, GLint* size, GLenum* type,
                       GLchar* name) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // GL_INVALID_VALUE is generated if index is greater than or equal to the
    // number of active attribute variables in program.
    // GL_INVALID_VALUE is generated if bufSize is less than 0.
    if (CheckGlValue(object_state_->programs.count(program) && bufSize >= 0 &&
            index < object_state_->programs[program].attributes.size()) &&
        CheckGlOperation(!object_state_->programs[program].deleted) &&
        CheckFunction("GetActiveAttrib")) {
      const ProgramObject& po = object_state_->programs[program];
      size_t a_index = 0;
      size_t i = 0;
      for (; i < index && a_index < po.attributes.size(); ++i) {
        // GetAttributeSlotCount() returns at least 1.
        a_index += GetAttributeSlotCount(po.attributes[a_index].type);
      }
      if (CheckGlValue(i == index && a_index < po.attributes.size())) {
        const ProgramObject::Attribute& a = po.attributes[a_index];
        const size_t to_copy =
            std::min(bufSize - 1, static_cast<GLsizei>(a.name.length()) + 1);
        if (length)
            *length = static_cast<GLsizei>(to_copy);
        if (name) {
          std::memcpy(name, a.name.data(), to_copy);
          name[to_copy] = '\0';
        }
        if (size)
            *size = a.size;
        if (type)
            *type = a.type;
      }
    }
  }
  void GetActiveUniform(GLuint program, GLuint index, GLsizei bufSize,
                        GLsizei* length, GLint* size, GLenum* type,
                        GLchar* name) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // GL_INVALID_VALUE is generated if index is greater than or equal to the
    // number of active uniform variables in program.
    // GL_INVALID_VALUE is generated if bufSize is less than 0.
    if (CheckGlValue(object_state_->programs.count(program) && bufSize >= 0 &&
            index < object_state_->programs[program].uniforms.size()) &&
        CheckGlOperation(!object_state_->programs[program].deleted) &&
        CheckFunction("GetActiveUniform")) {
      ProgramObject::Uniform& u =
          object_state_->programs[program].uniforms[index];
      const size_t to_copy =
          std::min(bufSize - 1, static_cast<GLsizei>(u.name.length() + 1));
      if (length)
          *length = static_cast<GLsizei>(to_copy);
      if (name) {
        std::memcpy(name, u.name.data(), to_copy);
        name[to_copy] = '\0';
      }
      if (size)
          *size = u.size;
      if (type)
          *type = u.type;
    }
  }
  void GetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei* count,
                          GLuint* shaders) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // GL_INVALID_VALUE is generated if maxCount is less than 0.
    if (CheckProgram(program) && CheckGlValue(maxCount >= 0) &&
        CheckFunction("GetAttachedShaders")) {
      const ProgramObject& po = object_state_->programs[program];
      GLuint* shader_out = shaders;
      if (po.vertex_shader > 0 && maxCount-- > 0)
        *shader_out++ = po.vertex_shader;
      if (po.geometry_shader > 0 && maxCount-- > 0)
        *shader_out++ = po.geometry_shader;
      if (po.fragment_shader > 0 && maxCount-- > 0)
        *shader_out++ = po.fragment_shader;
      if (count)
        *count = static_cast<GLsizei>(shader_out - shaders);
    }
  }
  GLint GetAttribLocation(GLuint program, const GLchar* name) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // If the name stars with "gl_" -1 is returned.
    if (CheckProgram(program) && !base::StartsWith(name, "gl_")) {
      const ProgramObject& po = object_state_->programs[program];
      // GL_INVALID_OPERATION is generated if program has not been successfully
      // linked.
      if (CheckGlOperation(po.link_status == GL_TRUE) &&
          CheckFunction("GetAttribLocation")) {
        // Find the attribute with a matching name, if any, and return its
        // index.
        for (size_t i = 0; i < po.attributes.size(); ++i) {
          if (po.attributes[i].name.compare(name) == 0)
            return static_cast<GLint>(i);
        }
      }
    }
    return -1;
  }
  void GetBooleanv(GLenum pname, GLboolean* params) {
    if (CheckFunction("GetBooleanv"))
        Getv<GLboolean>(pname, params);
  }
  void GetBufferParameteriv(GLenum target, GLenum value, GLint* data) {
    // GL_INVALID_ENUM is generated if target or value is not an accepted value.
    // GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is
    // bound to target.
    if (CheckBufferTarget(target) &&
        CheckGlEnum(value == GL_BUFFER_SIZE || value == GL_BUFFER_USAGE) &&
        CheckBufferZeroNotBound(target) &&
        CheckFunction("GetBufferParameteriv")) {
      const GLuint index = GetBufferIndex(target);
      if (value == GL_BUFFER_SIZE) {
        *data = static_cast<GLint>(object_state_->buffers[index].size);
      } else {
        *data = object_state_->buffers[index].usage;
      }
    }
  }
  GLenum GetError() {
    // GetError() resets the error code to no error.
    GLenum error_code = error_code_;
    error_code_ = GL_NO_ERROR;
    return error_code;
  }
  void GetFloatv(GLenum pname, GLfloat* params) {
    if (CheckFunction("GetFloatv"))
        Getv<GLfloat>(pname, params);
  }
  void GetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment,
                                           GLenum pname, GLint* params) {
    // GL_INVALID_ENUM is generated if target is not GL_FRAMEBUFFER.
    // GL_INVALID_ENUM is generated if attachment is not GL_COLOR_ATTACHMENTi,
    // GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT, or
    // GL_DEPTH_STENCIL_ATTACHMENT.
    if (!CheckGlEnum(IsFramebufferTarget(target) &&
                     IsAttachmentEnum(attachment))) return;
    // GL_INVALID_OPERATION is generated if the default framebuffer object
    // name 0 is bound.
    if (!CheckGlOperation(!IsDefaultFramebuffer(target))) return;
    // GL_INVALID_OPERATION is generated if attachment is
    // GL_DEPTH_STENCIL_ATTACHMENT and different objects are bound to the depth
    // and stencil attachment points of target.
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
      if (!CheckGlOperation(
            *GetAttachment(target, GL_DEPTH_ATTACHMENT) ==
            *GetAttachment(target, GL_STENCIL_ATTACHMENT))) return;
      attachment = GL_DEPTH_ATTACHMENT;
    }
    if (!CheckFunction("GetFramebufferAttachmentParameteriv")) return;

    FramebufferObject::Attachment* a = GetAttachment(target, attachment);
    // GL_INVALID_ENUM is generated if there is no attached object at the
    // named attachment point and pname is not
    // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE.
    // GL_INVALID_ENUM is generated if the attached object at the named
    // attachment point is GL_RENDERBUFFER and pname is not
    // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE or
    // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME.
    // GL_INVALID_ENUM is generated if the attached object at the named
    // attachment point is GL_TEXTURE and pname is not
    // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
    // GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
    // GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, or
    // GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE.
    GLint dummy = -1;
    if (!params) params = &dummy;
    switch (pname) {
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        *params = a->type;

        // Nexus 6 returns GL_RENDERBUFFER instead of GL_NONE. Fake that here
        // so we can test the fix for it.
        if (vendor_string_ == "Qualcomm" &&
            renderer_string_ == "Adreno (TM) 420" &&
            *params == GL_NONE) {
          *params = GL_RENDERBUFFER;
        }
        break;
      case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        if (CheckGlEnum(a->type == GL_RENDERBUFFER || a->type == GL_TEXTURE))
          *params = a->value;
        break;
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        if (CheckGlEnum(a->type == GL_TEXTURE))
          *params = a->level;
        break;
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        if (CheckGlEnum(a->type == GL_TEXTURE))
          *params = a->cube_face;
        break;
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
        if (CheckGlEnum(a->type == GL_TEXTURE))
          *params = a->num_views ? 0 : a->layer;
        break;
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT:
        if (CheckGlEnum(a->type == GL_TEXTURE))
          *params = a->texture_samples;
        break;
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR:
        if (CheckGlEnum(a->type == GL_TEXTURE))
          *params = a->num_views ? a->layer : 0;
        break;
      case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR:
        if (CheckGlEnum(a->type == GL_TEXTURE))
          *params = a->num_views;
        break;
      default:
        CheckGlEnum(false);
        break;
    }
  }
  void GetIntegerv(GLenum pname, GLint* params) {
    if (CheckFunction("GetIntegerv"))
      Getv<GLint>(pname, params);
  }
  void GetInteger64v(GLenum pname, GLint64* params) {
    if (CheckFunction("GetInteger64v"))
      Getv<GLint64>(pname, params);
  }
  void GetIntegeri_v(GLenum pname, GLuint index, GLint* params) {
    if (!CheckFunction("GetIntegeri_v")) return;
    const IndexedBufferBinding* binding = nullptr;
    // In this switch, process compute parameters and only pick the indexed
    // binding point for the query.
    switch (pname) {
      case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
        if (CheckGlValue(index < 3))
          *params = max_compute_work_group_count_[index];
        return;
      case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
        if (CheckGlValue(index < 3))
          *params = max_compute_work_group_size_[index];
        return;
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      case GL_TRANSFORM_FEEDBACK_BUFFER_START:
      case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        if (!CheckGlValue(index < max_transform_feedback_separate_attribs_))
          return;
        binding = &active_objects_.transform_feedback_buffers[index];
        break;
      case GL_UNIFORM_BUFFER_BINDING:
      case GL_UNIFORM_BUFFER_START:
      case GL_UNIFORM_BUFFER_SIZE:
        if (!CheckGlValue(index < max_uniform_buffer_bindings_))
          return;
        binding = &active_objects_.uniform_buffers[index];
        break;
      default:
        CheckGlEnum(false);
        return;
    }
    // If we reach here, this is an indexed binding query. Process according to
    // the type of the query.
    DCHECK(binding);
    switch (pname) {
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      case GL_UNIFORM_BUFFER_BINDING:
        *params = static_cast<GLint>(binding->id);
        break;
      case GL_TRANSFORM_FEEDBACK_BUFFER_START:
      case GL_UNIFORM_BUFFER_START:
        *params = static_cast<GLint>(binding->offset);
        break;
      case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
      case GL_UNIFORM_BUFFER_SIZE:
        if (binding->size == -1 && binding->id != 0 &&
            object_state_->buffers.count(binding->id)) {
          *params = static_cast<GLint>(
              object_state_->buffers[binding->id].size);
          break;
        }
        *params = static_cast<GLint>(binding->size);
        break;
      default:
        DCHECK(false);
    }
  }
  void GetInteger64i_v(GLenum pname, GLuint index, GLint64* params) {
    // For now, this is a raw entry point that we only support nominally.
  }
  void GetProgramInfoLog(GLuint program, GLsizei buf_size, GLsizei* length,
                         GLchar* info_log) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // GL_INVALID_VALUE is generated if bug_sze is less than 0.
    if (CheckGlValue(object_state_->programs.count(program) &&
                     buf_size >= 0) &&
        CheckGlOperation(!object_state_->programs[program].deleted) &&
        CheckFunction("GetProgramInfoLog")) {
      ProgramObject& po = object_state_->programs[program];
      // There is nothing to do since we do not compile programs.
      const size_t to_copy =
          std::min(buf_size - 1,
                   static_cast<GLsizei>(
                       po.info_log.length() ? po.info_log.length() + 1 : 0));
      if (length)
          *length = static_cast<GLsizei>(to_copy);
      if (info_log) {
        std::memcpy(info_log, po.info_log.data(), to_copy);
        info_log[to_copy] = '\0';
      }
    }
  }
  void GetProgramiv(GLuint program, GLenum pname, GLint* params) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    if (CheckProgram(program) && CheckFunction("GetProgramiv")) {
      const ProgramObject& po = object_state_->programs[program];
      switch (pname) {
        case GL_DELETE_STATUS:
          *params = po.delete_status;
          break;
        case GL_LINK_STATUS:
          *params = po.link_status;
          break;
        case GL_VALIDATE_STATUS:
          *params = po.validate_status;
          break;
        case GL_INFO_LOG_LENGTH:
          *params = static_cast<GLint>(
              po.info_log.length() ? po.info_log.length() + 1 : 0);
          break;
        case GL_ATTACHED_SHADERS: {
          *params = GetAttachedShaderCount(program);
          break;
        }
        case GL_ACTIVE_ATTRIBUTES: {
          GLint count = 0;
          for (size_t i = 0; i < po.attributes.size();) {
            // GetAttributeSlotCount() returns at least 1.
            i += GetAttributeSlotCount(po.attributes[i].type);
            ++count;
          }
          *params = count;
          break;
        }
        case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: {
          GLint length = 0;
          for (size_t i = 0; i < po.attributes.size(); ++i) {
            length = std::max(length, static_cast<GLint>(
                                          po.attributes[i].name.length() + 1U));
          }
          *params = length;
          break;
        }
        case GL_ACTIVE_UNIFORMS:
          *params = static_cast<GLint>(po.uniforms.size());
          break;
        case GL_ACTIVE_UNIFORM_MAX_LENGTH: {
          GLint length = 0;
          for (size_t i = 0; i < po.uniforms.size(); ++i) {
            length = std::max(
                length, static_cast<GLint>(po.uniforms[i].name.length() + 1U));
          }
          *params = length;
          break;
        }
        default:
          // GL_INVALID_ENUM is generated if pname is not an accepted value.
          CheckGlEnum(false);
          break;
      }
    }
  }
  void GetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params) {
    // GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.
    // GL_INVALID_ENUM is generated if pname is not GL_RENDERBUFFER_WIDTH,
    // GL_RENDERBUFFER_HEIGHT, GL_RENDERBUFFER_INTERNAL_FORMAT,
    // GL_RENDERBUFFER_RED_SIZE, GL_RENDERBUFFER_GREEN_SIZE,
    // GL_RENDERBUFFER_BLUE_SIZE, GL_RENDERBUFFER_ALPHA_SIZE,
    // GL_RENDERBUFFER_DEPTH_SIZE, or GL_RENDERBUFFER_STENCIL_SIZE.
    // GL_INVALID_OPERATION is generated if the reserved renderbuffer object
    // name 0 is bound.
    if (CheckGlEnum(target == GL_RENDERBUFFER) &&
        CheckGlOperation(active_objects_.renderbuffer != 0) &&
        CheckFunction("GetRenderbufferParameteriv")) {
      RenderbufferObject& r =
          object_state_->renderbuffers[active_objects_.renderbuffer];
      switch (pname) {
        case GL_RENDERBUFFER_WIDTH:
          *params = r.width;
          break;
        case GL_RENDERBUFFER_HEIGHT:
          *params = r.height;
          break;
        case GL_RENDERBUFFER_INTERNAL_FORMAT:
          *params = r.internal_format;
          break;
        case GL_RENDERBUFFER_RED_SIZE:
          *params = r.red_size;
          break;
        case GL_RENDERBUFFER_GREEN_SIZE:
          *params = r.green_size;
          break;
        case GL_RENDERBUFFER_BLUE_SIZE:
          *params = r.blue_size;
          break;
        case GL_RENDERBUFFER_ALPHA_SIZE:
          *params = r.alpha_size;
          break;
        case GL_RENDERBUFFER_DEPTH_SIZE:
          *params = r.depth_size;
          break;
        case GL_RENDERBUFFER_SAMPLES:
          *params = r.multisample_samples;
          break;
        case GL_RENDERBUFFER_STENCIL_SIZE:
          *params = r.stencil_size;
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }
  void GetShaderInfoLog(GLuint shader, GLsizei buf_size, GLsizei* length,
                        GLchar* info_log) {
    // GL_INVALID_VALUE is generated if shader is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if shader is not a shader object.
    // GL_INVALID_VALUE is generated if buf_size is less than 0.
    if (CheckShader(shader) &&
        CheckFunction("GetShaderInfoLog")) {
      // There is nothing to do since we do not compile shaders.
      ShaderObject& so = object_state_->shaders[shader];
      // There is nothing to do since we do not compile programs.
      const size_t to_copy =
          std::min(buf_size - 1,
                   static_cast<GLsizei>(
                       so.info_log.length() ? so.info_log.length() + 1 : 0));
      if (length)
          *length = static_cast<GLsizei>(to_copy);
      if (info_log) {
        std::memcpy(info_log, so.info_log.data(), to_copy);
        info_log[to_copy] = '\0';
      }
    }
  }
  void GetShaderPrecisionFormat(GLenum shaderType, GLenum precisionType,
                                GLint* range, GLint* precision) {
    // GL_INVALID_OPERATION is generated if a shader compiler is not supported.
    // GL_INVALID_ENUM is generated if shaderType or precisionType is not an
    // accepted value.
    if (CheckShaderType(shaderType) &&
        CheckFunction("GetShaderPrecisionFormat")) {
      switch (precisionType) {
        case GL_LOW_FLOAT:
        case GL_LOW_INT:
          if (range) {
            range[0] = 7;
            range[1] = 7;
          }
          if (precision)
              *precision = 8;
          break;

        case GL_MEDIUM_FLOAT:
        case GL_MEDIUM_INT:
          if (range) {
            range[0] = 15;
            range[1] = 15;
          }
          if (precision)
              *precision = 10;
          break;

        case GL_HIGH_FLOAT:
        case GL_HIGH_INT:
          if (range) {
            range[0] = 127;
            range[1] = 127;
          }
          if (precision)
              *precision = 23;
          break;

        default:
          CheckGlEnum(false);
          break;
      }
    }
  }

  void GetShaderSource(GLuint shader, GLsizei buf_size, GLsizei* length,
                       GLchar* source) {
    // GL_INVALID_VALUE is generated if shader is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if shader is not a shader object.
    // GL_INVALID_VALUE is generated if buf_size is less than 0.
    if (CheckShader(shader) && CheckGlValue(buf_size >= 0) &&
        CheckFunction("GetShaderSource")) {
      const ShaderObject& so = object_state_->shaders[shader];
      const size_t to_copy = std::min(
          buf_size - 1, static_cast<GLsizei>(
                            so.source.length() ? so.source.length() + 1 : 0));
      if (length)
          *length = static_cast<GLsizei>(to_copy);
      // Use memcpy to get around security warnings in Windows.
      if (source) {
        std::memcpy(source, &so.source[0], to_copy);
        // Terminate the string.
        source[*length] = '\0';
      }
    }
  }
  void GetShaderiv(GLuint shader, GLenum pname, GLint* params) {
    // GL_INVALID_VALUE is generated if shader is not a value generated by
    // OpenGL.
    // We want to check for deleted shaders here as well, to support querying
    // GL_DELETE_STATUS.
    if (CheckShader(shader) && CheckFunction("GetShaderiv")) {
      const ShaderObject& so = object_state_->shaders[shader];
      switch (pname) {
        case GL_SHADER_TYPE:
          *params = so.type;
          break;
        case GL_DELETE_STATUS:
          *params = so.delete_status;
          break;
        case GL_COMPILE_STATUS:
          *params = so.compile_status;
          break;
        case GL_INFO_LOG_LENGTH:
          *params = static_cast<GLint>(
              so.info_log.length() ? so.info_log.length() + 1 : 0);
          break;
        case GL_SHADER_SOURCE_LENGTH: {
          *params = static_cast<GLint>(
              so.source.length() ? so.source.length() + 1 : 0);
          break;
        }
        default:
          // GL_INVALID_ENUM is generated if pname is not an accepted value.
          CheckGlEnum(false);
          break;
      }
    }
  }
  const GLubyte* GetString(GLenum name) {
    if (CheckFunction("GetString")) {
      switch (name) {
        case GL_EXTENSIONS:
          return reinterpret_cast<const GLubyte*>(extensions_string_.c_str());
          break;
        case GL_VENDOR:
          return reinterpret_cast<const GLubyte*>(vendor_string_.c_str());
          break;
        case GL_RENDERER:
          return reinterpret_cast<const GLubyte*>(renderer_string_.c_str());
          break;
        case GL_VERSION:
          return reinterpret_cast<const GLubyte*>(version_string_.c_str());
          break;
        case GL_SHADING_LANGUAGE_VERSION:
          return reinterpret_cast<const GLubyte*>("1.10 Ion");
          break;
        default:
          // GL_INVALID_ENUM is generated if name is not an accepted value.
          CheckGlEnum(false);
          return nullptr;
      }
    }
    return nullptr;
  }
  template <typename T>
  void GetTexParameterv(GLenum target, GLenum pname, T* params) {
    // GL_INVALID_ENUM is generated if target or pname is not one of the
    // accepted defined values.
    if (CheckGlEnum(IsTextureTarget(target))) {
      const GLuint texture = GetActiveTexture(target);
      const TextureObject& to = object_state_->textures[texture];
      switch (pname) {
        case GL_TEXTURE_BASE_LEVEL:
          *params = static_cast<T>(to.base_level);
          break;
        case GL_TEXTURE_COMPARE_FUNC:
          *params = static_cast<T>(to.compare_func);
          break;
        case GL_TEXTURE_COMPARE_MODE:
          *params = static_cast<T>(to.compare_mode);
          break;
        case GL_TEXTURE_IMMUTABLE_FORMAT:
          *params = static_cast<T>(to.immutable);
          break;
        case GL_TEXTURE_FIXED_SAMPLE_LOCATIONS:
          *params = static_cast<T>(to.fixed_sample_locations);
          break;
        case GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM:
          *params = static_cast<T>(to.foveated_bits);
          break;
        case GL_TEXTURE_FOVEATED_FEATURE_QUERY_QCOM:
          // The texture foveation capability is constant, it is unclear if a
          // valid texture must be bound. This mock assumes that foveation is
          // supported.
          *params = static_cast<T>(GL_FOVEATION_ENABLE_BIT_QCOM |
                                   GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM);
          break;
        case GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM:
          *params = static_cast<T>(to.foveated_min_pixel_density);
          break;
        case GL_TEXTURE_FOVEATED_NUM_FOCAL_POINTS_QUERY_QCOM:
          // The texture foveation capability is constant, it is unclear if a
          // valid texture must be bound. This mock assumes two focal points.
          *params = static_cast<T>(kFoveationFocalPointCount);
          break;
        case GL_TEXTURE_MAG_FILTER:
          *params = static_cast<T>(to.mag_filter);
          break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
          *params = static_cast<T>(to.max_anisotropy);
          break;
        case GL_TEXTURE_MAX_LEVEL:
          *params = static_cast<T>(to.max_level);
          break;
        case GL_TEXTURE_MAX_LOD:
          *params = static_cast<T>(to.max_lod);
          break;
        case GL_TEXTURE_MIN_FILTER:
          *params = static_cast<T>(to.min_filter);
          break;
        case GL_TEXTURE_MIN_LOD:
          *params = static_cast<T>(to.min_lod);
          break;
        case GL_TEXTURE_PROTECTED_EXT:
          *params = static_cast<T>(to.is_protected);
          break;
        case GL_TEXTURE_SWIZZLE_R:
          *params = static_cast<T>(to.swizzle_r);
          break;
        case GL_TEXTURE_SWIZZLE_G:
          *params = static_cast<T>(to.swizzle_g);
          break;
        case GL_TEXTURE_SWIZZLE_B:
          *params = static_cast<T>(to.swizzle_b);
          break;
        case GL_TEXTURE_SWIZZLE_A:
          *params = static_cast<T>(to.swizzle_a);
          break;
        case GL_TEXTURE_SAMPLES:
          *params = static_cast<T>(to.samples);
          break;
        case GL_TEXTURE_WRAP_R:
          *params = static_cast<T>(to.wrap_r);
          break;
        case GL_TEXTURE_WRAP_S:
          *params = static_cast<T>(to.wrap_s);
          break;
        case GL_TEXTURE_WRAP_T:
          *params = static_cast<T>(to.wrap_t);
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }
  void GetTexParameterfv(GLenum target, GLenum pname, GLfloat* params) {
    if (CheckFunction("GetTexParameterfv"))
      GetTexParameterv(target, pname, params);
  }
  void GetTexParameteriv(GLenum target, GLenum pname, GLint* params) {
    if (CheckFunction("GetTexParameteriv"))
      GetTexParameterv(target, pname, params);
  }
  template <typename ValueType, typename T>
  void GetUniformValue(const ProgramObject::Uniform& u, GLint size, GLint index,
                       T* params) {
    if (u.value.GetCount()) {
      T* ptr = params;
      const GLint count = static_cast<GLint>(u.value.GetCount());
      if (index < count) {
        const ValueType& value = u.value.GetValueAt<ValueType>(index);
        const T* value_ptr = reinterpret_cast<const T*>(&value);
        for (GLint i = 0; i < size; ++i)
          ConvertValue(ptr + i, value_ptr[i]);
      }
    } else {
      const ValueType& value = u.value.Get<ValueType>();
      const T* value_ptr = reinterpret_cast<const T*>(&value);
      for (GLint i = 0; i < size; ++i)
        ConvertValue(params + i, value_ptr[i]);
    }
  }
  template <typename T>
  void GetUniformv(GLuint program, GLint location, T* params) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    if (CheckGlValue(object_state_->programs.count(program))) {
      ProgramObject& po = object_state_->programs[program];
      // GL_INVALID_OPERATION is generated if program is not a program object.
      // GL_INVALID_OPERATION is generated if program has not been successfully
      // linked.
      // GL_INVALID_OPERATION is generated if location does not correspond to a
      // valid uniform variable location for the specified program object.
      if (CheckGlOperation(!po.deleted && po.link_status == GL_TRUE &&
                           location >= 0 &&
                           location < po.max_uniform_location)) {
        const ProgramObject::Uniform& u =
            *GetUniformFromLocation(&po, location);
        const GLint index = location - u.index;
        switch (u.type) {
          case GL_FLOAT:
            GetUniformValue<float, T>(u, 1, index, params);
            break;
          case GL_FLOAT_VEC2:
            GetUniformValue<math::VectorBase2f, T>(u, 2, index, params);
            break;
          case GL_FLOAT_VEC3:
            GetUniformValue<math::VectorBase3f, T>(u, 3, index, params);
            break;
          case GL_FLOAT_VEC4:
            GetUniformValue<math::VectorBase4f, T>(u, 4, index, params);
            break;
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
          case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
            GetUniformValue<int, T>(u, 1, index, params);
            break;
          case GL_INT_VEC2:
            GetUniformValue<math::VectorBase2i, T>(u, 2, index, params);
            break;
          case GL_INT_VEC3:
            GetUniformValue<math::VectorBase3i, T>(u, 3, index, params);
            break;
          case GL_INT_VEC4:
            GetUniformValue<math::VectorBase4i, T>(u, 4, index, params);
            break;
          case GL_UNSIGNED_INT:
            GetUniformValue<uint32, T>(u, 1, index, params);
            break;
          case GL_UNSIGNED_INT_VEC2:
            GetUniformValue<math::VectorBase2ui, T>(u, 2, index, params);
            break;
          case GL_UNSIGNED_INT_VEC3:
            GetUniformValue<math::VectorBase3ui, T>(u, 3, index, params);
            break;
          case GL_UNSIGNED_INT_VEC4:
            GetUniformValue<math::VectorBase4ui, T>(u, 4, index, params);
            break;
          case GL_FLOAT_MAT2:
            GetUniformValue<math::Matrix2f, T>(u, 4, index, params);
            break;
          case GL_FLOAT_MAT3:
            GetUniformValue<math::Matrix3f, T>(u, 9, index, params);
            break;
          case GL_FLOAT_MAT4:
            GetUniformValue<math::Matrix4f, T>(u, 16, index, params);
            break;
        }
      }
    }
  }
  void GetUniformfv(GLuint program, GLint location, GLfloat* params) {
    if (CheckFunction("GetUniformfv"))
      GetUniformv<GLfloat>(program, location, params);
  }
  void GetUniformiv(GLuint program, GLint location, GLint* params) {
    if (CheckFunction("GetUniformiv"))
      GetUniformv<GLint>(program, location, params);
  }
  GLint GetUniformLocation(GLuint program, const GLchar* name) {
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    if (CheckGlValue(object_state_->programs.count(program)) &&
        CheckFunction("GetUniformLocation")) {
      const ProgramObject& po = object_state_->programs[program];
      // GL_INVALID_OPERATION is generated if program is not a program object.
      // GL_INVALID_OPERATION is generated if program has not been successfully
      // linked.
      if (CheckGlOperation(!po.deleted && po.link_status == GL_TRUE)) {
        // Find the uniform with a matching name, if any, and return its index.
        std::string uniform_name;
        GLint index = 0U;
        // First get the name and array index, if any. Uniform names have the
        // form "name[index]" where index is the offset into the array.
        // Technically, every element of an array uniform can have its own
        // location.
        ParseShaderInputName(name, &uniform_name, &index);
        for (size_t i = 0; i < po.uniforms.size(); ++i) {
          if (po.uniforms[i].name == uniform_name) {
            // Uniforms in the MGM take up the same number of locations as their
            // length.
            return po.uniforms[i].index + index;
          }
        }
      }
    }
    return -1;
  }
  template <typename T> void Getv(GLenum pname, T* params);  // Forward decl.
  void GetVertexAttribfv(GLuint index, GLenum pname, GLfloat* params) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    // GL_INVALID_ENUM is generated if pname is not an accepted value.
    if (CheckGlValue(index < max_vertex_attribs_) &&
        CheckFunction("GetVertexAttribfv")) {
      switch (pname) {
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
          *params = static_cast<GLfloat>(
              container_state_->arrays[active_objects_.vertex_array].
                  attributes[index].buffer);
          break;
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
          *params = static_cast<GLfloat>(
              container_state_->arrays[active_objects_.vertex_array].
                  attributes[index].enabled);
          break;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
          *params = static_cast<GLfloat>(
              container_state_->arrays[active_objects_.vertex_array].
                  attributes[index].size);
          break;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
          *params = static_cast<GLfloat>(
              container_state_->arrays[active_objects_.vertex_array].
                  attributes[index].stride);
          break;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
          *params = static_cast<GLfloat>(
              container_state_->arrays[active_objects_.vertex_array].
                  attributes[index].type);
          break;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
          *params = static_cast<GLfloat>(
              container_state_->arrays[active_objects_.vertex_array].
                  attributes[index].normalized);
          break;
        case GL_CURRENT_VERTEX_ATTRIB:
          for (int i = 0; i < 4; ++i)
            params[i] = container_state_->arrays[active_objects_.vertex_array].
                attributes[index].value[i];
          break;
        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
          *params = static_cast<GLfloat>(
              container_state_->arrays[active_objects_.vertex_array].
                  attributes[index].divisor);
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }
  void GetVertexAttribiv(GLuint index, GLenum pname, GLint* params) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    // GL_INVALID_ENUM is generated if pname is not an accepted value.
    if (CheckGlValue(index < max_vertex_attribs_) &&
        CheckFunction("GetVertexAttribiv")) {
      switch (pname) {
        case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
          *params = container_state_->arrays[active_objects_.vertex_array].
              attributes[index].buffer;
          break;
        case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
          *params = container_state_->arrays[active_objects_.vertex_array].
              attributes[index].enabled;
          break;
        case GL_VERTEX_ATTRIB_ARRAY_SIZE:
          *params = container_state_->arrays[active_objects_.vertex_array].
              attributes[index].size;
          break;
        case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
          *params = container_state_->arrays[active_objects_.vertex_array].
              attributes[index].stride;
          break;
        case GL_VERTEX_ATTRIB_ARRAY_TYPE:
          *params = container_state_->arrays[active_objects_.vertex_array].
              attributes[index].type;
          break;
        case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
          *params = container_state_->arrays[active_objects_.vertex_array].
              attributes[index].normalized;
          break;
        case GL_CURRENT_VERTEX_ATTRIB:
          for (int i = 0; i < 4; ++i)
            params[i] = static_cast<GLint>(
                container_state_->arrays[active_objects_.vertex_array].
                    attributes[index].value[i]);
          break;
        case GL_VERTEX_ATTRIB_ARRAY_DIVISOR:
          *params = container_state_->arrays[active_objects_.vertex_array].
              attributes[index].divisor;
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }
  void GetVertexAttribPointerv(GLuint index, GLenum pname, GLvoid** pointer) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    // GL_INVALID_ENUM is generated if pname is not an accepted value.
    if (CheckGlEnum(pname == GL_VERTEX_ATTRIB_ARRAY_POINTER) &&
        CheckGlValue(index < max_vertex_attribs_) &&
        CheckFunction("GetVertexAttribPointerv"))
      *pointer = container_state_->arrays[active_objects_.vertex_array].
          attributes[index].pointer;
  }
  void Hint(GLenum target, GLenum mode) {
    // GL_INVALID_ENUM is generated if either target or mode is not an accepted
    // value.
    if (CheckGlEnum(target == GL_GENERATE_MIPMAP_HINT &&
                    (mode == GL_FASTEST || mode == GL_NICEST ||
                     mode == GL_DONT_CARE)) &&
        CheckFunction("Hint")) {
      generate_mipmap_hint_ = mode;
    } else {
      LOG(ERROR) << "*** Set unimplemented hint in FakeGraphicsManager";
    }
  }
  GLboolean IsBuffer(GLuint buffer) {
    bool result = IsBufferName(buffer) &&
        !object_state_->buffers[buffer].bindings.empty();
    return result ? GL_TRUE : GL_FALSE;
  }
  GLboolean IsEnabled(GLenum cap) {
    GLint index = GetAndVerifyCapabilityIndex(cap);
    // GL_INVALID_ENUM is generated if cap is not an accepted value.
    if (CheckGlEnum(index >= 0 &&
                    index < static_cast<GLint>(enabled_state_.size())))
      return enabled_state_[index];
    else
      return GL_FALSE;
  }
  GLboolean IsFramebuffer(GLuint framebuffer) {
    bool result = framebuffer == 0U || (IsFramebufferName(framebuffer) &&
        !container_state_->framebuffers[framebuffer].bindings.empty());
    return result ? GL_TRUE : GL_FALSE;
  }
  GLboolean IsProgram(GLuint program) {
    bool result = object_state_->programs.count(program) &&
        (!object_state_->programs[program].deleted ||
         program == active_objects_.program);
    return result ? GL_TRUE : GL_FALSE;
  }
  GLboolean IsRenderbuffer(GLuint renderbuffer) {
    if (renderbuffer == 0) return GL_FALSE;
    bool result = IsRenderbufferName(renderbuffer) &&
        !object_state_->renderbuffers[renderbuffer].bindings.empty();
    return result ? GL_TRUE : GL_FALSE;
  }
  GLboolean IsShader(GLuint shader) {
    bool result = object_state_->shaders.count(shader) &&
        (!object_state_->shaders[shader].deleted ||
         !object_state_->shaders[shader].programs.empty());
    return result ? GL_TRUE : GL_FALSE;
  }
  GLboolean IsTexture(GLuint texture) {
    bool result = IsTextureName(texture) &&
        !object_state_->textures[texture].bindings.empty();
    return result ? GL_TRUE : GL_FALSE;
  }
  GLboolean IsTransformFeedback(GLuint id) {
    bool result = IsTransformFeedbackName(id) &&
        !container_state_->transform_feedbacks[id].bindings.empty();
    return result ? GL_TRUE : GL_FALSE;
  }
  GLboolean IsVertexArray(GLuint array) {
    bool result = IsVertexArrayName(array) &&
        !container_state_->arrays[array].bindings.empty();
    return result ? GL_TRUE : GL_FALSE;
  }
  void LineWidth(GLfloat width) {
    // GL_INVALID_VALUE is generated if width is less than or equal to 0.
    if (CheckGlValue(width > 0.f) && CheckFunction("LineWidth"))
      line_width_ = width;
  }

  // Internal method called at link time to resolve the varyings that have
  // been requested for capture, translating them from strings to structs.
  bool ResolveTransformFeedbackVaryings(ProgramObject* po) {
    // The program will fail to link if the following conditions are met:
    // (1) The count specified by TransformFeedbackVaryings is non-zero, but the
    // program object has no vertex or geometry shader.
    // (2) Any variable name specified in the varyings array is not declared as
    // an output in the vertex shader (or the geometry shader, if active).
    // (3) Any two entries in the varyings array specify the same varying
    // variable.
    // (4) The total number of components to capture in any varying variable in
    // varyings is greater than the constant
    // GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS and the buffer mode is
    // GL_SEPARATE_ATTRIBS.
    // (5) The total number of components to capture is greater than the
    // constant GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS and the buffer
    // mode is GL_INTERLEAVED_ATTRIBS.
    ObjectState& os = *object_state_;
    // Enforce (1).
    if (po->requested_tf_varyings.empty())
      return true;
    if ((!po->vertex_shader && !po->geometry_shader) ||
        (po->vertex_shader && !os.shaders[po->vertex_shader].compile_status) ||
        (po->fragment_shader &&
         !os.shaders[po->fragment_shader].compile_status) ||
        (po->geometry_shader &&
         !os.shaders[po->geometry_shader].compile_status))
      return false;
    // Don't bother enforcing (4) and (5) since it would be tedious to figure
    // out the number of vector components in each requested varying.
    // Create a string-to-varying map to help with enforcing (2) and (3).
    std::map<const std::string, ProgramObject::Varying*>
        varyings_name_map;
    for (int i = 0; i < static_cast<int>(po->varyings.size()); ++i)
      varyings_name_map[po->varyings[i].name] = &po->varyings[i];
    for (const auto& varying_name : po->requested_tf_varyings) {
      // Enforce (2) by checking if the program object has this varying.
      if (varyings_name_map.find(varying_name) == varyings_name_map.end())
        return false;
      // Enforce (3) by marking encountered varyings with nullptr.
      if (!varyings_name_map[varying_name])
        return false;
      ProgramObject::Varying* varying = varyings_name_map[varying_name];
      varyings_name_map[varying_name] = nullptr;
      ProgramObjectData::ResolvedVarying resolved_varying = {
        varying->name,
        varying->size,
        varying->type,
      };
      po->resolved_tf_varyings.push_back(resolved_varying);
    }
    return true;
  }

  void LinkProgram(GLuint program) {
    // We generate a synthetic GL_INVALID_OPERATION if
    // link_program_always_fails_ is set
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    if (CheckProgram(program)) {
      ProgramObject& po = object_state_->programs[program];
      // The below tests do not handle all of the requirements for a
      // successful link but cover the most obvious cases.
      if ((po.vertex_shader && po.fragment_shader &&
           object_state_->shaders[po.vertex_shader].compile_status ==
               GL_TRUE &&
           object_state_->shaders[po.fragment_shader].compile_status ==
               GL_TRUE &&
           (po.geometry_shader == 0U ||
            object_state_->shaders[po.fragment_shader].compile_status ==
                GL_TRUE)) ||
          (po.compute_shader &&
           object_state_->shaders[po.compute_shader].compile_status ==
               GL_TRUE)) {
        if (CheckFunction("LinkProgram")) {
          // Add attributes and uniforms to the program.
          ProgramObject old_po(po);
          po.attributes.clear();
          po.uniforms.clear();
          po.varyings.clear();
          po.max_uniform_location = 0U;
          if (po.vertex_shader != 0U)
            AddShaderInputs(&po, GL_VERTEX_SHADER,
                object_state_->shaders[po.vertex_shader].source);
          if (po.geometry_shader != 0U)
            AddShaderInputs(&po, GL_GEOMETRY_SHADER,
                object_state_->shaders[po.geometry_shader].source);
          if (!ResolveTransformFeedbackVaryings(&po)) {
            po = old_po;
            po.link_status = GL_FALSE;
            po.info_log = "Cannot bind transform feedback varyings.";
          } else {
            AddShaderInputs(&po, GL_FRAGMENT_SHADER,
                object_state_->shaders[po.fragment_shader].source);
            po.link_status = GL_TRUE;
            po.info_log.clear();
          }
          if (po.compute_shader != 0U) {
            AddShaderInputs(&po, GL_COMPUTE_SHADER,
                object_state_->shaders[po.compute_shader].source);
            po.has_compute_stage = true;
          }
        } else {
          po.link_status = GL_FALSE;
          po.info_log = "Program linking is set to always fail.";
        }
      }
    }
  }
  void PixelStorei(GLenum pname, GLint param) {
    // GL_INVALID_ENUM is generated if pname is not an accepted value.
    if (CheckGlEnum(pname == GL_PACK_ALIGNMENT ||
                    pname == GL_UNPACK_ALIGNMENT) &&
        // GL_INVALID_VALUE is generated if alignment is specified as other than
        // 1, 2, 4, or 8.
        CheckGlValue(param == 1 || param == 2 || param == 4 || param == 8) &&
        CheckFunction("PixelStorei")) {
      if (pname == GL_PACK_ALIGNMENT)
        pack_alignment_ = param;
      else
        unpack_alignment_ = param;
    }
  }
  void PolygonOffset(GLfloat factor, GLfloat units) {
    polygon_offset_factor_ = factor;
    polygon_offset_units_ = units;
  }
  void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, GLvoid* data) {
    // GL_INVALID_ENUM is generated if format or type is not an accepted value.
    // GL_INVALID_VALUE is generated if either width or height is negative.
    // GL_INVALID_OPERATION is generated if the readbuffer of the currently
    // bound framebuffer is a fixed point normalized surface and format and type
    // are neither GL_RGBA and GL_UNSIGNED_BYTE, respectively, nor the
    // format/type pair returned by querying GL_IMPLEMENTATION_COLOR_READ_FORMAT
    // and GL_IMPLEMENTATION_COLOR_READ_TYPE.
    // GL_INVALID_OPERATION is generated if the readbuffer of the currently
    // bound framebuffer is a floating point surface and format and type are
    // neither GL_RGBA and GL_FLOAT, respectively, nor the format/type pair
    // returned by querying GL_IMPLEMENTATION_COLOR_READ_FORMAT and
    // GL_IMPLEMENTATION_COLOR_READ_TYPE.
    // GL_INVALID_OPERATION is generated if the readbuffer of the currently
    // bound framebuffer is a signed integer surface and format and type are
    // neither GL_RGBA_INTEGER and GL_INT, respectively, nor the format/type
    // pair returned by querying GL_IMPLEMENTATION_COLOR_READ_FORMAT and
    // GL_IMPLEMENTATION_COLOR_READ_TYPE.
    // GL_INVALID_OPERATION is generated if the readbuffer of the currently
    // bound framebuffer is an unsigned integer surface and format and type are
    // neither GL_RGBA_INTEGER and GL_UNSIGNED_INT, respectively, nor the
    // format/type pair returned by querying GL_IMPLEMENTATION_COLOR_READ_FORMAT
    // and GL_IMPLEMENTATION_COLOR_READ_TYPE.
    // GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound
    // framebuffer is not framebuffer complete (i.e. the return value from
    // glCheckFramebufferStatus is not GL_FRAMEBUFFER_COMPLETE).
    if (CheckGlEnum((format == GL_RED || format == GL_RED_INTEGER ||
                     format == GL_RG || format == GL_RG_INTEGER ||
                     format == GL_RGB || format == GL_RGB_INTEGER ||
                     format == GL_RGBA || format == GL_RGBA_INTEGER ||
                     format == GL_LUMINANCE_ALPHA || format == GL_LUMINANCE ||
                     format == GL_ALPHA) &&
                    (type == GL_UNSIGNED_BYTE || type == GL_BYTE ||
                     type == GL_HALF_FLOAT || type == GL_FLOAT ||
                     type == GL_UNSIGNED_SHORT_5_6_5 ||
                     type == GL_UNSIGNED_SHORT_4_4_4_4 ||
                     type == GL_UNSIGNED_SHORT_5_5_5_1 ||
                     type == GL_UNSIGNED_INT_2_10_10_10_REV ||
                     type == GL_UNSIGNED_INT_10F_11F_11F_REV ||
                     type == GL_UNSIGNED_INT_5_9_9_9_REV)) &&
        CheckGlValue(width >= 0 && height >= 0) &&
        CheckGlOperation(((type != GL_UNSIGNED_SHORT_5_6_5 &&
                           type != GL_UNSIGNED_INT_10F_11F_11F_REV) ||
                          format == GL_RGB) &&
                         ((type != GL_UNSIGNED_SHORT_4_4_4_4 &&
                           type != GL_UNSIGNED_SHORT_5_5_5_1 &&
                           type != GL_UNSIGNED_INT_10F_11F_11F_REV &&
                           type != GL_UNSIGNED_INT_5_9_9_9_REV) ||
                          format == GL_RGBA)) &&
        // 
        CheckFramebuffer() && CheckFunction("ReadPixels")) {
      // FakeGlContext neither reads nor writes pixels.
    }
  }
  void ReleaseShaderCompiler() {
    // GL_INVALID_OPERATION is generated if a shader compiler is not supported.
    CheckGlOperation(false);
  }
  void SetColorsFromInternalFormat(const GLenum internalformat,
                                   RenderbufferObject* r) {
    switch (internalformat) {
      case GL_R8:
      case GL_R8UI:
      case GL_R8I:
        r->red_size = 8;
        r->blue_size = r->green_size = r->alpha_size = r->depth_size =
            r->stencil_size = 0;
        break;
      case GL_R16UI:
      case GL_R16I:
        r->red_size = 16;
        r->blue_size = r->green_size = r->alpha_size = r->depth_size =
            r->stencil_size = 0;
        break;
      case GL_R32UI:
      case GL_R32I:
        r->red_size = 32;
        r->blue_size = r->green_size = r->alpha_size = r->depth_size =
            r->stencil_size = 0;
        break;
      case GL_RG8:
      case GL_RG8UI:
      case GL_RG8I:
        r->red_size = r->green_size = 8;
        r->blue_size = r->alpha_size = r->depth_size = r->stencil_size = 0;
        break;
      case GL_RG16UI:
      case GL_RG16I:
        r->red_size = r->green_size = 16;
        r->blue_size = r->alpha_size = r->depth_size = r->stencil_size = 0;
        break;
      case GL_RG32UI:
      case GL_RG32I:
        r->red_size = r->green_size = 32;
        r->blue_size = r->alpha_size = r->depth_size = r->stencil_size = 0;
        break;
      case GL_RGB:
      case GL_RGB8:
        r->red_size = r->green_size = r->blue_size = 8;
        r->alpha_size = r->depth_size = r->stencil_size = 0;
        break;
      case GL_RGB565:
        r->red_size = r->blue_size = 5;
        r->green_size = 6;
        r->alpha_size = r->depth_size = r->stencil_size = 0;
        break;
      case GL_RGB5_A1:
        r->red_size = r->green_size = r->blue_size = 5;
        r->alpha_size = 1;
        r->depth_size = r->stencil_size = 0;
        break;
      case GL_RGBA4:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 4;
        r->depth_size = r->stencil_size = 0;
        break;
      case GL_RGB10_A2:
      case GL_RGB10_A2UI:
        r->red_size = r->green_size = r->blue_size = 10;
        r->alpha_size = 2;
        r->depth_size = r->stencil_size = 0;
        break;
      case GL_RGBA:
      case GL_RGBA8:
      case GL_SRGB8_ALPHA8:
      case GL_RGBA8UI:
      case GL_RGBA8I:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 8;
        r->depth_size = r->stencil_size = 0;
        break;
      case GL_RGBA16UI:
      case GL_RGBA16I:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 16;
        r->depth_size = r->stencil_size = 0;
        break;
      case GL_RGBA32I:
      case GL_RGBA32UI:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 32;
        r->depth_size = r->stencil_size = 0;
        break;
      case GL_DEPTH_COMPONENT16:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 0;
        r->depth_size = 16;
        r->stencil_size = 0;
        break;
      case GL_DEPTH_COMPONENT24:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 0;
        r->depth_size = 24;
        r->stencil_size = 0;
        break;
      case GL_DEPTH_COMPONENT32F:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 0;
        r->depth_size = 32;
        r->stencil_size = 0;
        break;
      case GL_DEPTH24_STENCIL8:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 0;
        r->depth_size = 24;
        r->stencil_size = 8;
        break;
      case GL_DEPTH32F_STENCIL8:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 0;
        r->depth_size = 32;
        r->stencil_size = 8;
        break;
      case GL_STENCIL_INDEX8:
        r->red_size = r->green_size = r->blue_size = r->alpha_size = 0;
        r->depth_size = 0;
        r->stencil_size = 8;
        break;
    }
  }
  void RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width,
                           GLsizei height) {
    if (!CheckFunction("RenderbufferStorage")) return;
    SetRenderbufferStorage(target, 0, internalformat, width, height, false);
  }
  void SampleCoverage(GLfloat value, GLboolean invert) {
    sample_coverage_value_ = Clampf(value);
    sample_coverage_inverted_ = invert;
  }
  void Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    // GL_INVALID_VALUE is generated if either width or height is negative.
    if (CheckGlValue(width >= 0 && height >= 0) && CheckFunction("Scissor")) {
      scissor_x_ = x;
      scissor_y_ = y;
      scissor_width_ = width;
      scissor_height_ = height;
    }
  }
  void ShaderBinary(GLsizei n, const GLuint* shaders, GLenum binaryformat,
                    const void* binary, GLsizei length) {
    // GL_INVALID_ENUM is generated if binaryformat is not a supported format
    // returned in GL_SHADER_BINARY_FORMATS.
    // GL_INVALID_VALUE is generated if any value in shaders is not a value
    // generated by OpenGL.
    // GL_INVALID_VALUE is generated if the format of the data pointed to by
    // binary does not match binaryformat.
    // GL_INVALID_VALUE is generated if n or length is negative.
    // GL_INVALID_OPERATION is generated if any value in shaders is not a shader
    // object, or if there is more than one vertex shader object handle or more
    // than one fragment shader object handle in shaders.
    // GL_INVALID_OPERATION is generated on implementations that do not support
    // any shader binary formats.
    CheckGlOperation(false);
  }
  void ShaderSource(GLuint shader, GLsizei count, const GLchar** string,
                    const GLint* length) {
    // GL_INVALID_VALUE is generated if shader is not a value generated by
    // OpenGL.
    // GL_INVALID_OPERATION is generated if shader is not a shader object.
    // GL_INVALID_VALUE is generated if count is less than 0.
    if (CheckShader(shader) && CheckGlValue(count >= 0) &&
        CheckFunction("ShaderSource")) {
      for (GLsizei i = 0; i < count; ++i)
        object_state_->shaders[shader].source.append(string[i]);
    }
  }
  void StencilMask(GLuint mask) {
    front_stencil_write_mask_ = back_stencil_write_mask_ = mask;
  }
  void StencilMaskSeparate(GLenum face, GLuint mask) {
    // GL_INVALID_ENUM is generated if face is not one of the accepted tokens.
    if (CheckFace(face) && CheckFunction("StencilMaskSeparate")) {
      if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
        front_stencil_write_mask_ = mask;
      if (face == GL_BACK || face == GL_FRONT_AND_BACK)
        back_stencil_write_mask_ = mask;
    }
  }
  void StencilFunc(GLenum func, GLint ref, GLuint mask) {
    // GL_INVALID_ENUM is generated if func is not one of the eight accepted
    // values.
    if (CheckDepthOrStencilFunc(func) && CheckFunction("StencilFunc")) {
      front_stencil_function_ = back_stencil_function_ = func;
      front_stencil_reference_value_ = back_stencil_reference_value_ = ref;
      front_stencil_mask_ = back_stencil_mask_ = mask;
    }
  }
  void StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    // GL_INVALID_ENUM is generated if func is not one of the eight accepted v
    // alues.
    // GL_INVALID_ENUM is generated if face is not one of the accepted tokens.
    if (CheckFace(face) && CheckDepthOrStencilFunc(func) &&
        CheckFunction("StencilFuncSeparate")) {
      if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
        front_stencil_function_ = func;
        front_stencil_reference_value_ = ref;
        front_stencil_mask_ = mask;
      }
      if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
        back_stencil_function_ = func;
        back_stencil_reference_value_ = ref;
        back_stencil_mask_ = mask;
      }
    }
  }
  void StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) {
    // GL_INVALID_ENUM is generated if sfail, dpfail, or dppass is any value
    // other than the defined constant values.
    if (CheckStencilOp(sfail) && CheckStencilOp(dpfail) &&
        CheckStencilOp(dppass) && CheckFunction("StencilOp")) {
      front_stencil_fail_op_ = back_stencil_fail_op_ = sfail;
      front_stencil_depth_fail_op_ = back_stencil_depth_fail_op_ = dpfail;
      front_stencil_pass_op_ = back_stencil_pass_op_ = dppass;
    }
  }
  void StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail,
                         GLenum dppass) {
    // GL_INVALID_ENUM is generated if face is any value other than GL_FRONT,
    // GL_BACK, or GL_FRONT_AND_BACK.
    // GL_INVALID_ENUM is generated if sfail, dpfail, or dppass is any value
    // other than the eight defined constant values.
    if (CheckFace(face) && CheckStencilOp(sfail) && CheckStencilOp(dpfail) &&
        CheckStencilOp(dppass) && CheckFunction("StencilOpSeparate")) {
      if (face == GL_FRONT || face == GL_FRONT_AND_BACK) {
        front_stencil_fail_op_ = sfail;
        front_stencil_depth_fail_op_ = dpfail;
        front_stencil_pass_op_ = dppass;
      }
      if (face == GL_BACK || face == GL_FRONT_AND_BACK) {
        back_stencil_fail_op_ = sfail;
        back_stencil_depth_fail_op_ = dpfail;
        back_stencil_pass_op_ = dppass;
      }
    }
  }
  void TexImage2D(GLenum target, GLint level, GLint internal_format,
                  GLsizei width, GLsizei height, GLint border, GLenum format,
                  GLenum type, const GLvoid* pixels) {
    if (
        // GL_INVALID_ENUM is generated if target is not GL_TEXTURE_1D_ARRAY,
        // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        // GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, or
        // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z.
        CheckTexture2dTargetType(target) &&
        // GL_INVALID_ENUM is generated if format or type is not an accepted
        // value.
        CheckTextureFormat(format) && CheckTextureType(type) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if target is one of the six cube
            // map 2D image targets and the width and height parameters are not
            // equal.
            ((IsCubeFaceTarget(target) && width == height) ||
             IsTexture2dTarget(target)) &&
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if internal_format is not an
            // accepted format.
            // GL_INVALID_VALUE is generated if width or height is less than 0
            // or greater than GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D
            // or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureDimensions(target, width, height, 1) &&
            // GL_INVALID_VALUE is generated if border is not 0.
            border == 0) &&
        // GL_INVALID_OPERATION is generated if the combination of
        // internal_format, format and type is not valid.
        CheckTextureFormatTypeAndInternalTypeAreValid(format, type,
                                                      internal_format)) {
      const GLuint texture = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[texture];
      // Once a texture is marked immutable it cannot be modified.
      if (CheckGlOperation(!to.immutable) && CheckFunction("TexImage2D")) {
        to.target =
            IsTexture2dTarget(target) ? target : GL_TEXTURE_CUBE_MAP;
        to.format = format;
        to.type = type;
        to.internal_format = internal_format;
        to.border = border;
        TextureObject::MipLevel miplevel;
        miplevel.width = width;
        miplevel.height = height;
        miplevel.depth = 1;
        miplevel.data.reset(new char[1]);
        to.levels
            .resize(std::max(level + 1, static_cast<GLint>(to.levels.size())));
        to.levels[level] = miplevel;
        // We do not convert to internal_format for mock data, we just need a
        // pointer to exist.
        to.compressed = false;
      }
    }
  }
  void TexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLsizei width, GLsizei height, GLenum format, GLenum type,
                     const GLvoid* data) {
    if (
        // GL_INVALID_ENUM is generated if target is not GL_TEXTURE_1D_ARRAY,
        // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        // GL_TEXTURE_CUBE_MAP_NEGATIVE_X, GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, or
        // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z.
        CheckTexture2dTargetType(target) &&
        // GL_INVALID_ENUM is generated if format or type is not an accepted
        // value.
        CheckTextureFormat(format) && CheckTextureType(type) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if internal_format is not an
            // accepted format.
            // GL_INVALID_VALUE is generated if width or height is less than 0
            // or greater than GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D
            // or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            width >= 0 && height >= 0)) {
      const GLuint tex_id = GetActiveTexture(target);
      TextureObject& texture = object_state_->textures[tex_id];
      // GL_INVALID_VALUE is generated if xoffset < 0, xoffset + width > w,
      // yoffset < 0, or yoffset + height > h, where w is the width and h is the
      // height of the texture image being modified.
      // GL_INVALID_OPERATION is generated if the texture array has not been
      // defined by a previous glTexImage2D or glCopyTexImage2D.
      // GL_INVALID_OPERATION is generated if the combination of
      // internalFormat of the previously specified texture vertex_array, format
      // and type is not valid
      if (CheckGlOperation(level < static_cast<GLint>(texture.levels.size())) &&
          CheckGlValue(xoffset >= 0 &&
                       xoffset + width <= texture.levels[level].width &&
                       yoffset >= 0 &&
                       yoffset + height <= texture.levels[level].height) &&
          CheckTextureFormatTypeAndInternalTypeAreValid(
              format, type, texture.internal_format) &&
          CheckFunction("TexSubImage2D")) {
        // The Check functions will log errors as appropriate.
      }
    }
  }
  template <typename T>
  void TexParameter(GLenum target, GLenum pname, T param) {
    // GL_INVALID_ENUM is generated if target or pname is not one of the
    // accepted defined values.
    // GL_INVALID_ENUM is generated if params should have a defined symbolic
    // constant value (based on the value of pname) and does not.
    if (CheckGlEnum(IsTextureTarget(target))) {
      const GLuint texture = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[texture];
      switch (pname) {
        case GL_TEXTURE_BASE_LEVEL:
          to.base_level = static_cast<GLint>(param);
          break;
        case GL_TEXTURE_COMPARE_FUNC:
          if (CheckGlEnum(param == GL_LEQUAL || param == GL_GEQUAL ||
                          param == GL_LESS || param == GL_GREATER ||
                          param == GL_EQUAL || param == GL_NOTEQUAL ||
                          param == GL_ALWAYS || param == GL_NEVER))
            to.compare_func = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_COMPARE_MODE:
          if (CheckGlEnum(param == GL_COMPARE_REF_TO_TEXTURE ||
                          param == GL_NONE))
            to.compare_mode = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM:
          if ((static_cast<int>(param) & GL_FOVEATION_ENABLE_BIT_QCOM) == 0) {
            if (!CheckGlOperation(
                    (to.foveated_bits & GL_FOVEATION_ENABLE_BIT_QCOM) == 0))
              break;
          }
          if (CheckGlEnum(
                  (static_cast<int>(param) & ~(GL_FOVEATION_ENABLE_BIT_QCOM |
                             GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM)) == 0))
            to.foveated_bits = static_cast<GLint>(param);
          break;
        case GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM:
          if (CheckGlOperation(static_cast<GLfloat>(param) >= 0.0f &&
                               static_cast<GLfloat>(param) <= 1.0f))
            to.foveated_min_pixel_density = static_cast<GLfloat>(param);
          break;
        case GL_TEXTURE_MAG_FILTER:
          if (CheckGlEnum(param == GL_NEAREST || param == GL_LINEAR))
            to.mag_filter = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
          if (CheckGlValue(param >= 1.f &&
                           param <= max_texture_max_anisotropy_))
            to.max_anisotropy = static_cast<GLfloat>(param);
          break;
        case GL_TEXTURE_MAX_LEVEL:
          to.max_level = static_cast<GLint>(param);
          break;
        case GL_TEXTURE_MAX_LOD:
          to.max_lod = static_cast<GLfloat>(param);
          break;
        case GL_TEXTURE_MIN_FILTER:
          if (CheckGlEnum(param == GL_NEAREST || param == GL_LINEAR ||
                          param == GL_NEAREST_MIPMAP_NEAREST ||
                          param == GL_LINEAR_MIPMAP_NEAREST ||
                          param == GL_NEAREST_MIPMAP_LINEAR ||
                          param == GL_LINEAR_MIPMAP_LINEAR))
            to.min_filter = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_MIN_LOD:
          to.min_lod = static_cast<GLfloat>(param);
          break;
        case GL_TEXTURE_PROTECTED_EXT:
          if (CheckGlValue(static_cast<GLint>(param) == GL_TRUE ||
                           static_cast<GLint>(param) == GL_FALSE)) {
            to.is_protected = static_cast<GLint>(param) ? GL_TRUE : GL_FALSE;
          }
          break;
        case GL_TEXTURE_SWIZZLE_R:
          if (CheckColorChannelEnum(static_cast<GLenum>(param)))
            to.swizzle_r = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_SWIZZLE_G:
          if (CheckColorChannelEnum(static_cast<GLenum>(param)))
            to.swizzle_g = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_SWIZZLE_B:
          if (CheckColorChannelEnum(static_cast<GLenum>(param)))
            to.swizzle_b = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_SWIZZLE_A:
          if (CheckColorChannelEnum(static_cast<GLenum>(param)))
            to.swizzle_a = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_WRAP_R:
          if (CheckWrapMode(static_cast<GLenum>(param)))
            to.wrap_r = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_WRAP_S:
          if (CheckWrapMode(static_cast<GLenum>(param)))
            to.wrap_s = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_WRAP_T:
          if (CheckWrapMode(static_cast<GLenum>(param)))
            to.wrap_t = static_cast<GLenum>(param);
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }
  template <typename T>
  void TexParameterv(GLenum target, GLenum pname, const T* params) {
    TexParameter(target, pname, params[0]);
  }
  void TexParameterf(GLenum target, GLenum pname, GLfloat param) {
    if (CheckFunction("TexParameterf"))
      TexParameter(target, pname, param);
  }
  void TexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    if (CheckFunction("TexParameterfv"))
      TexParameterv(target, pname, params);
  }
  void TexParameteri(GLenum target, GLenum pname, GLint param) {
    if (CheckFunction("TexParameteri"))
      TexParameter(target, pname, param);
  }
  void TexParameteriv(GLenum target, GLenum pname, const GLint* params) {
    if (CheckFunction("TexParameteriv"))
      TexParameterv(target, pname, params);
  }
  void PatchParameteri(GLenum pname, GLint value) {
    CheckGlEnum(pname == GL_PATCH_VERTICES);
    patch_vertices_ = value;
  }
  void PatchParameterfv(GLenum pname, GLfloat *values) {
    CheckGlEnum(pname == GL_PATCH_DEFAULT_OUTER_LEVEL ||
                pname == GL_PATCH_DEFAULT_INNER_LEVEL);
    if (pname == GL_PATCH_DEFAULT_INNER_LEVEL) {
      std::memcpy(default_inner_tess_level_.Data(), values, sizeof(float) * 2);
    } else if (pname == GL_PATCH_DEFAULT_OUTER_LEVEL) {
      memcpy(default_outer_tess_level_.Data(), values, sizeof(float) * 4);
    }
  }

  // GL_INVALID_OPERATION is generated if the size of the uniform variable
  // declared in the shader does not match the size indicated by the
  // glUniform command.
  // GL_INVALID_OPERATION is generated if one of the integer variants of
  // this function is used to load a uniform variable of type float, vec2,
  // vec3, vec4, or an array of these, or if one of the floating-point
  // variants of this function is used to load a uniform variable of type
  // int, ivec2, ivec3, or ivec4, or an array of these.
  // GL_INVALID_OPERATION is generated if a sampler is loaded using a
  // command other than glUniform1i and glUniform1iv.
  // GL_INVALID_VALUE is generated if count is less than 0.
  // GL_INVALID_OPERATION is generated if count is greater than 1 and the
  // indicated uniform variable is not an array variable.
  //
  // Note that for array operations, if the caller tries to set more elements
  // than exist in an array, OpenGL silently ignores the extra values.
  //
  // The below template functions greatly ease writing and testing the
  // Uniform*() functions.
  template <typename T>
  void SetSingleUniform(const std::string& func_name, GLenum type,
                        GLint location, const T& value) {
    if (CheckUniformLocation(location) && CheckFunction(func_name)) {
      ProgramObject::Uniform& uniform = *GetUniformFromLocation(
          &object_state_->programs[active_objects_.program], location);
      if (CheckGlOperation(
              uniform.type == type ||
              // Allow sampler types to be set as ints.
              (type == GL_INT && IsSamplerUniform(uniform.type)))) {
        if (uniform.value.GetCount()) {
          const GLint index = location - uniform.index;
          DCHECK_GE(index, 0);
          uniform.value.SetValueAt(index, value);
        } else {
          uniform.value.Set(value);
        }
      }
    }
  }
  template <typename ValueType, typename T>
  void SetVectorArrayUniform(const std::string& func_name, GLint size,
                             GLenum type, GLint location, GLsizei count,
                             const T* value) {
    if (CheckUniformLocation(location) && CheckGlValue(count >= 0) &&
        CheckFunction(func_name)) {
      ProgramObject::Uniform& uniform = *GetUniformFromLocation(
          &object_state_->programs[active_objects_.program], location);
      if (CheckGlOperation(
              (count <= 1 || uniform.value.GetCount()) &&
              (uniform.type == type ||
               // Allow sampler types to be set as ints.
               (type == GL_INT && IsSamplerUniform(uniform.type))))) {
        if (uniform.value.GetCount()) {
          const GLint index = location - uniform.index;
          DCHECK_GE(index, 0);
          const GLint last = std::min(index + count, uniform.size);
          const T* ptr = value;
          for (GLint i = index; i < last; ++i, ptr += size) {
            ValueType v;
            T* value_ptr = reinterpret_cast<T*>(&v);
            for (GLint j = 0; j < size; ++j)
              value_ptr[j] = ptr[j];
            uniform.value.SetValueAt(i, v);
          }
        } else {
          ValueType v;
          T* value_ptr = reinterpret_cast<T*>(&v);
          for (GLint j = 0; j < size; ++j)
            value_ptr[j] = value[j];
          uniform.value.Set(v);
        }
      }
    }
  }
  // GL_INVALID_VALUE is generated if transpose is not GL_FALSE.
  template <typename ValueType, typename T>
  void SetMatrixArrayUniform(const std::string& func_name, GLint size,
                             GLenum type, GLint location, GLsizei count,
                             GLboolean transpose, const T* value) {
    if (CheckUniformLocation(location) &&
        CheckGlValue(count >= 0 && transpose == GL_FALSE) &&
        CheckFunction(func_name)) {
      ProgramObject::Uniform& uniform = *GetUniformFromLocation(
          &object_state_->programs[active_objects_.program], location);
      if (CheckGlOperation((count <= 1 || uniform.value.GetCount()) &&
                           uniform.type == type)) {
        if (uniform.value.GetCount()) {
          const GLint index = location - uniform.index;
          DCHECK_GE(index, 0);
          const GLint last = std::min(index + count, uniform.size);
          const T* ptr = value;
          for (GLint j = index; j < last; ++j, ptr += size) {
            ValueType mat;
            for (int i = 0; i < size; ++i)
                mat.Data()[i] = ptr[i];
            uniform.value.SetValueAt(j, mat);
          }
        } else {
          ValueType mat;
          for (int i = 0; i < size; ++i)
              mat.Data()[i] = value[i];
          uniform.value.Set(mat);
        }
      }
    }
  }
  void Uniform1f(GLint location, GLfloat value) {
    SetSingleUniform("Uniform1f", GL_FLOAT, location, value);
  }
  void Uniform1fv(GLint location, GLsizei count, const GLfloat* value) {
    SetVectorArrayUniform<float, GLfloat>("Uniform1fv", 1, GL_FLOAT, location,
                                          count, value);
  }
  void Uniform1i(GLint location, GLint value) {
    SetSingleUniform("Uniform1i", GL_INT, location, value);
  }
  void Uniform1iv(GLint location, GLsizei count, const GLint* value) {
    SetVectorArrayUniform<int, GLint>("Uniform1iv", 1, GL_INT, location, count,
                                      value);
  }
  void Uniform2f(GLint location, GLfloat v0, GLfloat v1) {
    SetSingleUniform("Uniform2f", GL_FLOAT_VEC2, location,
                     math::Vector2f(v0, v1));
  }
  void Uniform2fv(GLint location, GLsizei count, const GLfloat* value) {
    SetVectorArrayUniform<math::Vector2f, GLfloat>(
        "Uniform2fv", 2, GL_FLOAT_VEC2, location, count, value);
  }
  void Uniform2i(GLint location, GLint v0, GLint v1) {
    SetSingleUniform("Uniform2i", GL_INT_VEC2, location,
                     math::Vector2i(v0, v1));
  }
  void Uniform2iv(GLint location, GLsizei count, const GLint* value) {
    SetVectorArrayUniform<math::Vector2i, GLint>("Uniform2iv", 2, GL_INT_VEC2,
                                                 location, count, value);
  }
  void Uniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    SetSingleUniform("Uniform3f", GL_FLOAT_VEC3, location,
                     math::Vector3f(v0, v1, v2));
  }
  void Uniform3fv(GLint location, GLsizei count, const GLfloat* value) {
    SetVectorArrayUniform<math::Vector3f, GLfloat>(
        "Uniform3fv", 3, GL_FLOAT_VEC3, location, count, value);
  }
  void Uniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    SetSingleUniform("Uniform3i", GL_INT_VEC3, location,
                     math::Vector3i(v0, v1, v2));
  }
  void Uniform3iv(GLint location, GLsizei count, const GLint* value) {
    SetVectorArrayUniform<math::Vector3i, GLint>("Uniform3iv", 3, GL_INT_VEC3,
                                                 location, count, value);
  }
  void Uniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2,
                 GLfloat v3) {
    SetSingleUniform("Uniform4f", GL_FLOAT_VEC4, location,
                     math::Vector4f(v0, v1, v2, v3));
  }
  void Uniform4fv(GLint location, GLsizei count, const GLfloat* value) {
    SetVectorArrayUniform<math::Vector4f, GLfloat>(
        "Uniform4fv", 4, GL_FLOAT_VEC4, location, count, value);
  }
  void Uniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    SetSingleUniform("Uniform4i", GL_INT_VEC4, location,
                     math::Vector4i(v0, v1, v2, v3));
  }
  void Uniform4iv(GLint location, GLsizei count, const GLint* value) {
    SetVectorArrayUniform<math::Vector4i, GLint>("Uniform4iv", 4, GL_INT_VEC4,
                                                 location, count, value);
  }
  void UniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat* value) {
    SetMatrixArrayUniform<math::Matrix2f, GLfloat>("UniformMatrix2fv", 4,
                                                   GL_FLOAT_MAT2, location,
                                                   count, transpose, value);
  }
  void UniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat* value) {
    SetMatrixArrayUniform<math::Matrix3f, GLfloat>("UniformMatrix3fv", 9,
                                                   GL_FLOAT_MAT3, location,
                                                   count, transpose, value);
  }
  void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat* value) {
    SetMatrixArrayUniform<math::Matrix4f, GLfloat>("UniformMatrix4fv", 16,
                                                   GL_FLOAT_MAT4, location,
                                                   count, transpose, value);
  }
  void UseProgram(GLuint program) {
    if (!CheckFunction("UseProgram")) return;
    // GL_INVALID_VALUE is generated if program is neither 0 nor a value
    // generated by OpenGL.
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // GL_INVALID_OPERATION is generated if program could not be made part
    // of current state.
    if (program != 0U) {
      if (!CheckProgram(program)) return;
      if (!CheckGlOperation(object_state_->programs[program].link_status))
        return;
      object_state_->programs[program].bindings.push_back(GetCallCount());
    }
    active_objects_.program = program;
  }
  void ValidateProgram(GLuint program) {
    // GL_INVALID_OPERATION is generated if program is not a program object.
    // GL_INVALID_VALUE is generated if program is not a value generated by
    // OpenGL.
    if (CheckProgram(program) && CheckFunction("ValidateProgram")) {
      object_state_->programs[program].validate_status = GL_TRUE;
    }
  }
  void SetSimpleAttributeFields(ArrayObject::Attribute* attr) {
    attr->buffer = 0U;
    attr->stride = 0U;
    attr->type = GL_FLOAT;
    attr->enabled = GL_TRUE;
    attr->normalized = GL_FALSE;
    attr->pointer = nullptr;
  }

  void VertexAttrib1f(GLint index, GLfloat v0) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    if (CheckGlValue(index < static_cast<GLint>(max_vertex_attribs_)) &&
        CheckFunction("VertexAttrib1f")) {
      // Simple attributes affect global state.
      for (auto& entry : container_state_->arrays) {
        ArrayObject::Attribute& attr = entry.second.attributes[index];
        // Only update the attribute if it is not a buffer attribute.
        if (!attr.buffer) {
          attr.value.Set(v0, 0.f, 0.f, 1.f);
          attr.size = 1;
          SetSimpleAttributeFields(&attr);
        }
      }
    }
  }
  void VertexAttrib1fv(GLuint index, const GLfloat* value) {
    VertexAttrib1f(index, value[0]);
  }
  void VertexAttrib2f(GLint index, GLfloat v0, GLfloat v1) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    if (CheckGlValue(index < static_cast<GLint>(max_vertex_attribs_)) &&
        CheckFunction("VertexAttrib2f")) {
      // Simple attributes affect global state.
      for (auto& entry : container_state_->arrays) {
        ArrayObject::Attribute& attr = entry.second.attributes[index];
        // Only update the attribute if it is not a buffer attribute.
        if (!attr.buffer) {
          attr.value.Set(v0, v1, 0.f, 1.f);
          attr.size = 2;
          SetSimpleAttributeFields(&attr);
        }
      }
    }
  }
  void VertexAttrib2fv(GLuint index, const GLfloat* value) {
    VertexAttrib2f(index, value[0], value[1]);
  }
  void VertexAttrib3f(GLint index, GLfloat v0, GLfloat v1, GLfloat v2) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    if (CheckGlValue(index < static_cast<GLint>(max_vertex_attribs_)) &&
        CheckFunction("VertexAttrib3f")) {
      // Simple attributes affect global state.
      for (auto& entry : container_state_->arrays) {
        ArrayObject::Attribute& attr = entry.second.attributes[index];
        // Only update the attribute if it is not a buffer attribute.
        if (!attr.buffer) {
          attr.value.Set(v0, v1, v2, 1.f);
          attr.size = 3;
          SetSimpleAttributeFields(&attr);
        }
      }
    }
  }
  void VertexAttrib3fv(GLuint index, const GLfloat* value) {
    VertexAttrib3f(index, value[0], value[1], value[2]);
  }
  void VertexAttrib4f(GLint index, GLfloat v0, GLfloat v1, GLfloat v2,
                      GLfloat v3) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    if (CheckGlValue(index < static_cast<GLint>(max_vertex_attribs_)) &&
        CheckFunction("VertexAttrib4f")) {
      // Simple attributes affect global state.
      for (auto& entry : container_state_->arrays) {
        ArrayObject::Attribute& attr = entry.second.attributes[index];
        // Only update the attribute if it is not a buffer attribute.
        if (!attr.buffer) {
          attr.value.Set(v0, v1, v2, v3);
          attr.size = 4;
          SetSimpleAttributeFields(&attr);
        }
      }
    }
  }
  void VertexAttrib4fv(GLuint index, const GLfloat* value) {
    VertexAttrib4f(index, value[0], value[1], value[2], value[3]);
  }
  void VertexAttribPointer(GLuint index, GLint size, GLenum type,
                           GLboolean normalized, GLsizei stride,
                           const GLvoid* pointer) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    // GL_INVALID_VALUE is generated if size is not 1, 2, 3, 4.
    // GL_INVALID_VALUE is generated if stride is negative.
    if (CheckGlValue(index < max_vertex_attribs_ && size >= 1 && size <= 4 &&
                     stride >= 0) &&
        // GL_INVALID_ENUM is generated if type is not an accepted value.
        CheckGlEnum(type == GL_BYTE || type == GL_UNSIGNED_BYTE ||
                    type == GL_SHORT || type == GL_UNSIGNED_SHORT ||
                    type == GL_INT || type == GL_UNSIGNED_INT ||
                    type == GL_FIXED || type == GL_FLOAT) &&
        CheckFunction("VertexAttribPointer")) {
      ArrayObject::Attribute& attr =
          container_state_->arrays[active_objects_.vertex_array]
              .attributes[index];
      attr.buffer = active_objects_.array_buffer;
      attr.size = size;
      attr.type = type;
      attr.normalized = normalized;
      attr.stride = stride;
      attr.value = math::Vector4f(0.f, 0.f, 0.f, 1.f);
      attr.pointer = const_cast<GLvoid*>(pointer);
    }
  }
  void VertexAttribDivisor(GLuint index, GLuint divisor) {
    // GL_INVALID_VALUE is generated if index is greater than or equal to
    // GL_MAX_VERTEX_ATTRIBS.
    if (CheckGlValue(index < max_vertex_attribs_) &&
        CheckFunction("VertexAttribDivisor")) {
      ArrayObject::Attribute& attr =
          container_state_->arrays[active_objects_.vertex_array]
              .attributes[index];
      attr.divisor = divisor;
    }
  }
  void Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    // GL_INVALID_VALUE is generated if either width or height is negative.
    if (CheckGlValue(width >= 0 && height >= 0) && CheckFunction("Viewport")) {
      viewport_x_ = x;
      viewport_y_ = y;
      viewport_width_ = width;
      viewport_height_ = height;
    }
  }

  // ComputeShader group.
  void DispatchCompute(GLuint num_groups_x, GLuint num_groups_y,
                       GLuint num_groups_z) {
    // GL_INVALID_VALUE is generated if any of num_groups_x, num_groups_y, or
    // num_groups_z is greater than or equal to the maximum work-group count for
    // the corresponding dimension.
    const bool dispatch_within_limits =
        static_cast<int>(num_groups_x) <= max_compute_work_group_count_[0] &&
        static_cast<int>(num_groups_y) <= max_compute_work_group_count_[1] &&
        static_cast<int>(num_groups_z) <= max_compute_work_group_count_[2];
    if (!CheckGlValue(dispatch_within_limits)) return;
    // GL_INVALID_OPERATION is generated if there is no active program for the
    // compute shader stage.
    if (!CheckGlOperation(active_objects_.program > 0U &&
                          object_state_->programs[active_objects_.program]
                              .has_compute_stage)) return;
    CheckFunction("DispatchCompute");
  }
  void DispatchComputeIndirect(GLintptr indirect) {
    // GL_INVALID_VALUE is generated if indirect is less than zero or not a
    // multiple of four.
    if (!CheckGlValue(indirect >= 0 && indirect % 4 == 0)) return;
    // GL_INVALID_OPERATION is generated if there is no active program for the
    // compute shader stage.
    if (!CheckGlOperation(active_objects_.program > 0U &&
                              object_state_->programs[active_objects_.program]
                                  .has_compute_stage)) return;
    // GL_INVALID_OPERATION is generated if no buffer is bound to the
    // GL_DISPATCH_INDIRECT_BUFFER target or if the command would source data
    // beyond the end of the buffer object's data store.
    const GLuint id = active_objects_.dispatch_indirect_buffer;
    if (!CheckGlOperation(id != 0U)) return;
    // The indirect compute dispatch command is 3 x uint = 12 bytes.
    if (!CheckGlOperation(object_state_->buffers[id].size <= indirect + 12))
      return;
    CheckFunction("DispatchComputeIndirect");
  }

  template <typename T>
  void GetLabelFromObject(std::map<GLuint, T>* objects, GLuint id,
                          GLsizei bufSize, GLsizei* length, GLchar* label) {
    auto it = objects->find(id);
    if (CheckGlOperation(it != objects->end())) {
      if (label && bufSize) {
        const size_t to_copy = std::min(
            bufSize - 1, static_cast<GLsizei>(it->second.label.length()));
        std::memcpy(label, it->second.label.data(), to_copy);
        label[to_copy] = '\0';
        if (length)
          *length = static_cast<GLsizei>(to_copy);
      }
    }
  }

  // DebugLabel group.
  void GetObjectLabel(GLenum type, GLuint object, GLsizei bufSize,
                      GLsizei* length, GLchar* label) {
    // GL_INVALID_OPERATION is generated if the type of <object> does not match
    // <type>.
    // GL_INVALID_ENUM is generated if <type> is not one of the allowed object
    // types.
    // GL_INVALID_VALUE is generated if <bufSize> is less than zero.
    if (CheckGlValue(bufSize >= 0) && CheckFunction("GetObjectLabel")) {
      switch (type) {
        case GL_TEXTURE:
          GetLabelFromObject(&object_state_->textures, object, bufSize, length,
              label);
          break;
        case GL_FRAMEBUFFER:
          GetLabelFromObject(&container_state_->framebuffers, object, bufSize,
              length, label);
          break;
        case GL_RENDERBUFFER:
          GetLabelFromObject(&object_state_->renderbuffers, object, bufSize,
              length, label);
          break;
        case GL_BUFFER_OBJECT_EXT:
          GetLabelFromObject(&object_state_->buffers, object, bufSize, length,
              label);
          break;
        case GL_SHADER_OBJECT_EXT:
          GetLabelFromObject(&object_state_->shaders, object, bufSize, length,
              label);
          break;
        case GL_PROGRAM_OBJECT_EXT:
          GetLabelFromObject(&object_state_->programs, object, bufSize, length,
              label);
          break;
        case GL_VERTEX_ARRAY_OBJECT_EXT:
          GetLabelFromObject(&container_state_->arrays, object, bufSize, length,
              label);
          break;
        case GL_QUERY_OBJECT_EXT:
          // 
          break;
        case GL_SAMPLER:
          GetLabelFromObject(&object_state_->samplers, object, bufSize, length,
              label);
          break;
        case GL_TRANSFORM_FEEDBACK:
          GetLabelFromObject(&container_state_->transform_feedbacks, object,
                             bufSize, length, label);
          break;
        case GL_PROGRAM_PIPELINE_OBJECT_EXT:
          // 
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }
  template <typename T>
  void SetObjectLabel(std::map<GLuint, T>* objects, GLuint id, GLsizei length,
                      const GLchar* label) {
    typename std::map<GLuint, T>::iterator it = objects->find(id);
    if (CheckGlOperation(it != objects->end())) {
      if (label && length)
        it->second.label = std::string(label, length);
      else
        it->second.label.clear();
    }
  }
  void LabelObject(GLenum type, GLuint object, GLsizei length,
                   const GLchar* label) {
    // GL_INVALID_OPERATION is generated if the type of <object> does not match
    // <type>.
    // GL_INVALID_ENUM is generated if <type> is not one of the allowed object
    // types.
    // GL_INVALID_VALUE is generated by if <length> is less than zero.
    if (CheckGlValue(length >= 0) && CheckFunction("LabelObject")) {
      switch (type) {
        case GL_TEXTURE:
          SetObjectLabel(&object_state_->textures, object, length, label);
          break;
        case GL_FRAMEBUFFER:
          SetObjectLabel(&container_state_->framebuffers, object, length,
                         label);
          break;
        case GL_RENDERBUFFER:
          SetObjectLabel(&object_state_->renderbuffers, object, length, label);
          break;
        case GL_BUFFER_OBJECT_EXT:
          SetObjectLabel(&object_state_->buffers, object, length, label);
          break;
        case GL_SHADER_OBJECT_EXT:
          SetObjectLabel(&object_state_->shaders, object, length, label);
          break;
        case GL_PROGRAM_OBJECT_EXT:
          SetObjectLabel(&object_state_->programs, object, length, label);
          break;
        case GL_VERTEX_ARRAY_OBJECT_EXT:
          SetObjectLabel(&container_state_->arrays, object, length, label);
          break;
        case GL_QUERY_OBJECT_EXT:
          // 
          break;
        case GL_SAMPLER:
          SetObjectLabel(&object_state_->samplers, object, length, label);
          break;
        case GL_TRANSFORM_FEEDBACK:
          SetObjectLabel(&container_state_->transform_feedbacks, object, length,
                         label);
          break;
          break;
        case GL_PROGRAM_PIPELINE_OBJECT_EXT:
          // 
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }

  // DebugOutput group.
  void DebugMessageCallback(GLDEBUGPROC callback, const void* userParam) {
    if (!CheckFunction("DebugMessageCallback")) {
      return;
    }
    debug_callback_function_ = callback;
    debug_callback_user_param_ = userParam;
  }
  void DebugMessageControl(GLenum source, GLenum type, GLenum severity,
                           GLsizei count, const GLuint* ids,
                           GLboolean enabled) {
    if (!CheckFunction("DebugMessageControl")) {
      return;
    }
    if (!CheckGlEnum(source == GL_DEBUG_SOURCE_API ||
                     source == GL_DEBUG_SOURCE_SHADER_COMPILER ||
                     source == GL_DEBUG_SOURCE_WINDOW_SYSTEM ||
                     source == GL_DEBUG_SOURCE_THIRD_PARTY ||
                     source == GL_DEBUG_SOURCE_APPLICATION ||
                     source == GL_DEBUG_SOURCE_OTHER ||
                     source == GL_DONT_CARE)) {
      return;
    }
    if (!CheckGlEnum(type == GL_DEBUG_TYPE_ERROR ||
                     type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR ||
                     type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR ||
                     type == GL_DEBUG_TYPE_PERFORMANCE ||
                     type == GL_DEBUG_TYPE_PORTABILITY ||
                     type == GL_DEBUG_TYPE_OTHER || type == GL_DONT_CARE)) {
      return;
    }
    if (!CheckGlEnum(severity == GL_DEBUG_SEVERITY_HIGH ||
                     severity == GL_DEBUG_SEVERITY_MEDIUM ||
                     severity == GL_DEBUG_SEVERITY_LOW ||
                     severity == GL_DONT_CARE)) {
      return;
    }
    if (!CheckGlValue(count >= 0)) {
      return;
    }

    if (count > 0) {
      if (!CheckGlOperation(source != GL_DONT_CARE && type != GL_DONT_CARE &&
                            severity == GL_DONT_CARE)) {
        return;
      }
    }

    debug_message_state_->SetEnabled(source, type, count, ids, severity,
                                     enabled);
  }
  void DebugMessageInsert(GLenum source, GLenum type, GLuint id,
                          GLenum severity, GLsizei length, const GLchar* buf) {
    if (!CheckFunction("DebugMessageInsert")) {
      return;
    }
    if (!CheckGlEnum(source == GL_DEBUG_SOURCE_APPLICATION ||
                     source == GL_DEBUG_SOURCE_THIRD_PARTY)) {
      return;
    }
    if (!CheckGlEnum(type == GL_DEBUG_TYPE_ERROR ||
                     type == GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR ||
                     type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR ||
                     type == GL_DEBUG_TYPE_PERFORMANCE ||
                     type == GL_DEBUG_TYPE_PORTABILITY ||
                     type == GL_DEBUG_TYPE_OTHER)) {
      return;
    }
    if (!CheckGlEnum(severity == GL_DEBUG_SEVERITY_HIGH ||
                     severity == GL_DEBUG_SEVERITY_MEDIUM ||
                     severity == GL_DEBUG_SEVERITY_LOW)) {
      return;
    }
    if (length < 0) {
      length = static_cast<GLsizei>(std::strlen(buf));
    }
    if (!CheckGlValue(length < max_debug_message_length_)) return;
    LogDebugMessage(source, type, id, severity, buf);
  }
  GLuint GetDebugMessageLog(GLuint count, GLsizei bufSize, GLenum* sources,
                            GLenum* types, GLuint* ids, GLenum* severities,
                            GLsizei* lengths, GLchar* messageLog) {
    if (!CheckFunction("GetDebugMessageLog")) {
      return 0;
    }
    if (!CheckGlValue(bufSize >= 0)) {
      return 0;
    }
    GLuint retrieved_count = 0;
    size_t retrieved_offset = 0;
    while (retrieved_count < count) {
      if (debug_message_log_.empty()) {
        break;
      }
      const DebugMessage& message = debug_message_log_.front();
      if (retrieved_offset + message.message.size() + 1 >=
          static_cast<size_t>(bufSize)) {
        break;
      }
      if (sources != nullptr) {
        sources[retrieved_count] = message.source;
      }
      if (types != nullptr) {
        types[retrieved_count] = message.type;
      }
      if (ids != nullptr) {
        ids[retrieved_count] = message.id;
      }
      if (severities != nullptr) {
        severities[retrieved_count] = message.severity;
      }
      if (lengths != nullptr) {
        lengths[retrieved_count] =
            static_cast<GLsizei>(message.message.size()) + 1;
      }
      std::memcpy(messageLog + retrieved_offset, message.message.data(),
                  message.message.size());
      retrieved_offset += message.message.size();
      messageLog[retrieved_offset] = '\0';
      ++retrieved_offset;

      debug_message_log_.pop_front();
      ++retrieved_count;
    }
    return retrieved_count;
  }
  void GetPointerv(GLenum pname, void** params) {
    if (!CheckFunction("GetPointerv")) {
      return;
    }
    switch (pname) {
      case GL_DEBUG_CALLBACK_FUNCTION:
        *params = reinterpret_cast<void*>(debug_callback_function_);
        break;
      case GL_DEBUG_CALLBACK_USER_PARAM:
        *params = const_cast<void*>(debug_callback_user_param_);
        break;
      default:
        CheckGlEnum(false);
        break;
    }
  }

  // DrawBuffer, DrawBuffers and ReadBuffer groups.
  // Implemented together for clarity.
  void DrawBuffer(GLenum buffer) {
    // 
    // should actually translate into {GL_BACK_LEFT, GL_FRONT_LEFT,
    // GL_BACK_RIGHT, GL_FRONT_RIGHT} when accessed via GL_DRAW_BUFFERi.
    // Need to test on an actual GL implementation.
    if (CheckDrawBuffer(GL_DRAW_FRAMEBUFFER, buffer)) {
      FramebufferObject& draw_fbo =
          container_state_->framebuffers[active_objects_.draw_framebuffer];
      draw_fbo.draw_buffers[0] = buffer;
      for (uint32 i = 1; i < max_draw_buffers_; ++i) {
        draw_fbo.draw_buffers[i] = GL_NONE;
      }
    }
  }
  void ReadBuffer(GLenum buffer) {
    if (CheckDrawBuffer(GL_READ_FRAMEBUFFER, buffer)) {
      // GL_FRONT_AND_BACK is not a valid value for reading.
      if (!CheckGlEnum(buffer != GL_FRONT_AND_BACK)) return;
      FramebufferObject& read_fbo =
          container_state_->framebuffers[active_objects_.read_framebuffer];
      read_fbo.read_buffer = buffer;
    }
  }
  void DrawBuffers(GLsizei n, const GLenum* bufs) {
    // GL_INVALID_ENUM is generated if n is less than 0.
    // GL_INVALID_VALUE is generated if n is greater than GL_MAX_DRAW_BUFFERS.
    if (CheckGlEnum(n >= 0) &&
        CheckGlValue(n <= static_cast<GLsizei>(max_draw_buffers_))) {
      std::unordered_set<GLenum> values;
      for (GLsizei i = 0; i < n; ++i) {
        // GL_INVALID_ENUM is generated if one of the values in bufs is not an
        // accepted value.
        // GL_INVALID_ENUM is generated if the API call refers to the default
        // framebuffer and one or more of the values in bufs is one of the
        // GL_COLOR_ATTACHMENTn tokens.
        // GL_INVALID_ENUM is generated if the API call refers to a framebuffer
        // object and one or more of the values in bufs is anything other than
        // GL_NONE or one of the GL_COLOR_ATTACHMENTn tokens.
        if (!CheckDrawBuffer(GL_DRAW_FRAMEBUFFER, bufs[i])) return;
        // "The symbolic constants GL_FRONT, GL_BACK, GL_LEFT, GL_RIGHT, and
        // GL_FRONT_AND_BACK are not allowed in the bufs array since they may
        // refer to multiple buffers." (But GL_BACK is apparently allowed after
        // all, since this is implied by one of the mentioned error conditions.)
        if (!CheckGlEnum(bufs[i] != GL_FRONT && bufs[i] != GL_LEFT &&
                         bufs[i] != GL_RIGHT && bufs[i] != GL_FRONT_AND_BACK))
          return;
        // GL_INVALID_OPERATION is generated if any value in bufs is GL_BACK,
        // and n is not one.
        if (!CheckGlOperation(bufs[i] != GL_BACK || n == 1)) return;
        // GL_INVALID_OPERATION is generated if a symbolic constant other than
        // GL_NONE appears more than once in bufs.
        if (!CheckGlOperation(bufs[i] == GL_NONE ||
                              values.insert(bufs[i]).second)) return;
      }
      FramebufferObject& draw_fbo =
          container_state_->framebuffers[active_objects_.draw_framebuffer];
      for (uint32 i = 0; i < max_draw_buffers_; ++i) {
        draw_fbo.draw_buffers[i] =
            static_cast<GLsizei>(i) < n ? bufs[i] : GL_NONE;
      }
    }
  }

  // DebugMarker group.
  // These functions do nothing since the driver is supposed to expose stream
  // inspection. OpenGL does not provide any way of inspecting markers, stating
  // that "applications can implement their own marker stacks within their code
  // independent of OpenGL ES."  gfx::Renderer does precisely that, but since
  // this class does not have access to a Renderer there is no way to access
  // that functionality here for testing.  Even when using real OpenGL, however,
  // there is no way to verify that these calls actually do anything without
  // inspecting an OpenGL stream in a platform-specific trace analyzer.
  void InsertEventMarker(GLsizei length, const GLchar* marker) {}
  void PopGroupMarker() {}
  void PushGroupMarker(GLsizei length, const GLchar* marker) {}

  // GetString group.
  const GLubyte* GetStringi(GLenum name, GLuint index) {
    switch (name) {
      case GL_EXTENSIONS: {
        if (CheckGlValue(index < extension_strings_.size())) {
          return reinterpret_cast<const GLubyte*>(
              extension_strings_[index].c_str());
        }
        break;
      }
      default:
        // GL_INVALID_ENUM is generated if name is not an accepted value.
        CheckGlEnum(false);
        return nullptr;
    }
    return nullptr;
  }

  // EglImage group.
  void EGLImageTargetTexture2DOES(GLenum target, void* image) {
    // Minimal implementation since we do not implement EGL.
    const GLuint texture = GetActiveTexture(target);
    TextureObject& to = object_state_->textures[texture];
    if (CheckGlOperation(!to.immutable)) {
      to.target = target;
      to.egl_image = true;
    }
  }
  void EGLImageTargetRenderbufferStorageOES(GLenum target, void* image) {
    // Do nothing as we do not implement EGL.
  }

  // FramebufferBlit group.
  void BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                       GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                       GLbitfield mask, GLenum filter) {
    // Doesn't actually do anything as we do not render; we just check params.

    // Make sure mask is valid.
    GLbitfield removed_valid_bits = mask &
        ~(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    CheckGlOperation(removed_valid_bits == 0);

    // GL_INVALID_OPERATION is generated if mask contains any of the
    // GL_DEPTH_BUFFER_BIT or GL_STENCIL_BUFFER_BIT and filter is not
    // GL_NEAREST.
    if ((mask & GL_DEPTH_BUFFER_BIT)
        || (mask & GL_STENCIL_BUFFER_BIT)) {
      CheckGlOperation(filter == GL_NEAREST);
    }
  }

  // QCOM Framebuffer foveated group.
  void FramebufferFoveationConfigQCOM(GLuint framebuffer_name,
                                      GLuint layer_count,
                                      GLuint focal_point_count_per_layer,
                                      GLuint requested_features,
                                      GLuint* provided_features) {
    if (!CheckGlValue(IsFramebuffer(framebuffer_name))) return;

    // Update the status of the framebuffer.
    FramebufferObject& fbo = container_state_->framebuffers[framebuffer_name];

    if (!CheckGlOperation(!fbo.is_foveation_enabled)) return;

    *provided_features =
        requested_features & (GL_FOVEATION_ENABLE_BIT_QCOM |
                             GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM);

    fbo.is_foveation_enabled =
        *provided_features == (GL_FOVEATION_ENABLE_BIT_QCOM |
                              GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM);
    fbo.foveated_layer_count = layer_count;
    fbo.foveated_focal_point_count = focal_point_count_per_layer;
  }
  void FramebufferFoveationParametersQCOM(GLuint framebuffer_name, GLuint layer,
                                          GLuint focal_point, GLfloat focal_x,
                                          GLfloat focal_y, GLfloat gain_x,
                                          GLfloat gain_y, GLfloat fovea_area) {
    if (!CheckGlValue(IsFramebuffer(framebuffer_name))) return;

    // Check the status of the framebuffer.
    FramebufferObject& fbo = container_state_->framebuffers[framebuffer_name];

    if (!CheckGlOperation(fbo.is_foveation_enabled) ||
       !CheckGlValue(layer < fbo.foveated_layer_count) ||
       !CheckGlValue(focal_point < fbo.foveated_focal_point_count))
      return;
  }

  // QCOM Texture foveated group.
  void TextureFoveationParametersQCOM(GLuint texture, GLuint layer,
                                      GLuint focal_point, GLfloat focal_x,
                                      GLfloat focal_y, GLfloat gain_x,
                                      GLfloat gain_y, GLfloat fovea_area) {
    if (!CheckGlValue(IsTexture(texture))) return;

    // Check the status of the framebuffer.
    const TextureObject& to = object_state_->textures[texture];

    if (!CheckGlOperation((to.foveated_bits & GL_FOVEATION_ENABLE_BIT_QCOM) !=
                          0) ||
        !CheckGlValue(focal_point < kFoveationFocalPointCount))
      return;
  }

  // RenderbufferMultisample group.
  void RenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                      GLenum internalformat, GLsizei width,
                                      GLsizei height) {
    if (!CheckFunction("RenderbufferStorageMultisample")) return;
    SetRenderbufferStorage(target, samples, internalformat, width, height,
                           false);
  }

  // FramebufferTextureLayer group.
  void FramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture,
                               GLint level, GLint layer) {
    // GL_INVALID_VALUE is generated if texture is not zero and layer is
    // negative.
    if (texture != 0U && !CheckGlValue(layer >= 0)) return;
    if (!CheckFunction("FramebufferTextureLayer")) return;
    // There is an empty "default" texture at index 0.
    const GLenum textarget = object_state_->textures[texture].target;
    SetFramebufferTexture(target, attachment, textarget, texture, level, layer,
                          0, 0);
  }

  // ImplicitMultisample group.
  void FramebufferTexture2DMultisampleEXT(GLenum target, GLenum attachment,
                                          GLenum textarget, GLuint texture,
                                          GLint level, GLsizei samples) {
    if (!CheckGlEnum(textarget != GL_TEXTURE_2D_MULTISAMPLE)) return;
    if (!CheckFunction("FramebufferTexture2DMultisampleEXT")) return;
    SetFramebufferTexture(target, attachment, textarget, texture, level, -1, 0,
                          samples);
  }
  void RenderbufferStorageMultisampleEXT(GLenum target, GLsizei samples,
                                         GLenum internalformat, GLsizei width,
                                         GLsizei height) {
    if (!CheckFunction("RenderbufferStorageMultisampleEXT")) return;
    SetRenderbufferStorage(target, samples, internalformat, width, height,
                           true);
  }

  // MultisampleFramebufferResolve group.
  void ResolveMultisampleFramebuffer() {
    FramebufferObject& read_frameBuffer =
        container_state_->framebuffers[active_objects_.read_framebuffer];
    FramebufferObject& draw_frameBuffer =
        container_state_->framebuffers[active_objects_.draw_framebuffer];

    RenderbufferObject& colorBufferReadBuffer =
        object_state_->renderbuffers[read_frameBuffer.color[0].value];
    RenderbufferObject& colorBufferDrawbuffer =
        object_state_->renderbuffers[draw_frameBuffer.color[0].value];

    // From: https://www.khronos.org/registry/gles/extensions/APPLE/
    // APPLE_framebuffer_multisample.txt
    // The command
    // void ResolveMultisampleFramebufferAPPLE(void);
    //
    // INVALID_OPERATION is generated if SAMPLE_BUFFERS for the read framebuffer
    // is zero, or if SAMPLE_BUFFERS for the draw framebuffer is greater than
    // zero, or if the read framebuffer or draw framebuffer does not have a
    // color attachment, or if the dimensions of the read and draw framebuffers
    // are not identical, or if the components in the format of the draw
    // framebuffer's color attachment are not present in the format of the read
    // framebuffer's color attachment.
    // INVALID_FRAMEBUFFER_OPERATION is generated if the objects bound to
    // DRAW_FRAMEBUFFER_APPLE and READ_FRAMEBUFFER_APPLE are not framebuffer
    // complete (see section 4.4.5)."
    if (CheckFunction("ResolveMultisampleFramebuffer") &&
        CheckGlOperation(read_frameBuffer.color[0].value != 0) &&
        CheckGlOperation(draw_frameBuffer.color[0].value != 0) &&
        CheckGlOperation(colorBufferReadBuffer.multisample_samples > 0) &&
        CheckGlOperation(colorBufferDrawbuffer.multisample_samples == 0) &&
        CheckGlOperation(
            (colorBufferReadBuffer.width == colorBufferDrawbuffer.width) &&
            (colorBufferReadBuffer.height == colorBufferDrawbuffer.height)) &&
        CheckGlOperation(colorBufferReadBuffer.internal_format ==
            colorBufferDrawbuffer.internal_format) &&
        CheckGl(CheckFramebufferStatus(
            GL_READ_FRAMEBUFFER,  active_objects_.read_framebuffer) ==
                GL_FRAMEBUFFER_COMPLETE, GL_INVALID_FRAMEBUFFER_OPERATION) &&
        CheckGl(
            CheckFramebufferStatus(
            GL_DRAW_FRAMEBUFFER, active_objects_.draw_framebuffer) ==
                GL_FRAMEBUFFER_COMPLETE, GL_INVALID_FRAMEBUFFER_OPERATION)) {
      return;
    }
  }

  // MapBuffer group.
  void* MapBuffer(GLenum target, GLenum access) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted
    // targets.
    void* data = nullptr;
    if (CheckBufferTarget(target) &&
        // GL_INVALID_ENUM is generated if access is not GL_READ_ONLY,
        // GL_WRITE_ONLY, or GL_READ_WRITE.
        CheckGlEnum(access == GL_READ_ONLY || access == GL_WRITE_ONLY ||
                    access == GL_READ_WRITE) &&
        // GL_OUT_OF_MEMORY is generated when glMapBuffer is executed if the GL
        // is unable to map the buffer object's data store. This may occur for a
        // variety of system-specific reasons, such as the absence of sufficient
        // remaining virtual memory (ignored).
        // GL_INVALID_OPERATION is generated if the reserved buffer object name
        // 0 is bound to target.
        CheckBufferZeroNotBound(target) && CheckFunction("MapBuffer")) {
      const GLuint index = GetBufferIndex(target);
      BufferObject& bo = object_state_->buffers[index];
      // GL_INVALID_OPERATION is generated if glMapBuffer is executed for a
      // buffer object whose data store is already mapped.
      if (CheckGlOperation(bo.mapped_data == nullptr)) {
        data = bo.mapped_data = bo.data;
        bo.mapped_range.Set(0, static_cast<unsigned int>(bo.size));
        bo.access =
            (access == GL_READ_ONLY ? GL_MAP_READ_BIT : 0) |
            (access == GL_WRITE_ONLY ? GL_MAP_READ_BIT : 0) |
            (access == GL_READ_WRITE ? (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT)
                                     : 0);
      }
    }
    return data;
  }

  // Multiview group.
  void FramebufferTextureMultiviewOVR(GLenum target, GLenum attachment,
                                      GLuint texture, GLint level,
                                      GLint base_view_index,
                                      GLsizei num_views) {
    if (!CheckGlValue(texture == 0U || num_views >= 1)) return;
    const GLenum textarget = object_state_->textures[texture].target;
    SetFramebufferTexture(target, attachment, textarget, texture, level,
                          base_view_index, num_views, 0);
  }

  // MultiviewImplicitMultisample group.
  void FramebufferTextureMultisampleMultiviewOVR(GLenum target,
                                                 GLenum attachment,
                                                 GLuint texture, GLint level,
                                                 GLsizei samples,
                                                 GLint base_view_index,
                                                 GLsizei num_views) {
    if (!CheckGlValue(texture == 0U || num_views >= 1)) return;
    const GLenum textarget = object_state_->textures[texture].target;
    if (!CheckGlOperation(texture == 0U || textarget == GL_TEXTURE_2D_ARRAY))
      return;
    SetFramebufferTexture(target, attachment, textarget, texture, level,
                          base_view_index, num_views, samples);
  }

  // GpuShader4 group.
  void GetUniformuiv(GLuint program, GLint location, GLuint* params) {
    if (CheckFunction("GetUniformuiv"))
      GetUniformv<GLuint>(program, location, params);
  }
  void Uniform1ui(GLint location, GLuint value) {
    SetSingleUniform("Uniform1ui", GL_UNSIGNED_INT, location, value);
  }
  void Uniform1uiv(GLint location, GLsizei count, const GLuint* value) {
    SetVectorArrayUniform<uint32, GLuint>("Uniform1uiv", 1, GL_UNSIGNED_INT,
                                          location, count, value);
  }
  void Uniform2ui(GLint location, GLuint v0, GLuint v1) {
    SetSingleUniform("Uniform2ui", GL_UNSIGNED_INT_VEC2, location,
                     math::Vector2ui(v0, v1));
  }
  void Uniform2uiv(GLint location, GLsizei count, const GLuint* value) {
    SetVectorArrayUniform<math::Vector2ui, GLuint>(
        "Uniform2uiv", 2, GL_UNSIGNED_INT_VEC2, location, count, value);
  }
  void Uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    SetSingleUniform("Uniform3ui", GL_UNSIGNED_INT_VEC3, location,
                     math::Vector3ui(v0, v1, v2));
  }
  void Uniform3uiv(GLint location, GLsizei count, const GLuint* value) {
    SetVectorArrayUniform<math::Vector3ui, GLuint>(
        "Uniform3uiv", 3, GL_UNSIGNED_INT_VEC3, location, count, value);
  }
  void Uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    SetSingleUniform("Uniform4ui", GL_UNSIGNED_INT_VEC4, location,
                     math::Vector4ui(v0, v1, v2, v3));
  }
  void Uniform4uiv(GLint location, GLsizei count, const GLuint* value) {
    SetVectorArrayUniform<math::Vector4ui, GLuint>(
        "Uniform4uiv", 4, GL_UNSIGNED_INT_VEC4, location, count, value);
  }
  void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                           GLsizei primCount) {
    GLuint tfo_id = active_objects_.transform_feedback;
    TransformFeedbackObject& tfo =
        container_state_->transform_feedbacks[tfo_id];
    // GL_INVALID_ENUM is generated if mode is not an accepted value.
    // GL_INVALID_VALUE is generated if count or primCount is negative.
    // GL_INVALID_OPERATION is generated if a non-zero buffer object name is
    // bound to an enabled array and the buffer object's data store is currently
    // mapped.
    // GL_INVALID_OPERATION is generated if transform feedback is active and
    // mode does not exactly match primitive_mode.
    if (CheckDrawMode(mode) && CheckGlValue(count >= 0 && primCount >= 0) &&
        (active_objects_.array_buffer == 0 ||
         CheckGlOperation(object_state_->buffers[active_objects_.array_buffer]
                              .data != nullptr)) &&
        CheckGlOperation(!tfo.active || tfo.primitive_mode == mode) &&
        CheckFunction("DrawArraysInstanced")) {
      // There is nothing to do since we do not implement draw functions.
    }
  }
  void DrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
                             const GLvoid* indices, GLsizei primCount) {
    // GL_INVALID_ENUM is generated if mode is not an accepted value.
    // GL_INVALID_ENUM is generated if type is not GL_UNSIGNED_BYTE,
    // GL_UNSIGNED_INT or GL_UNSIGNED_SHORT.
    // GL_INVALID_VALUE is generated if count or primCount is negative.
    // GL_INVALID_OPERATION is generated if a non-zero buffer object name is
    // bound to an enabled array or the element array and the buffer object's
    // data store is currently mapped.
    // GL_INVALID_OPERATION is generated if transform feedback is active and not
    // paused.
    if (CheckDrawMode(mode) && CheckGlValue(count >= 0 && primCount >= 0) &&
        CheckGlEnum(type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_INT ||
                    type == GL_UNSIGNED_SHORT) &&
        (active_objects_.array_buffer == 0 ||
         CheckGlOperation(
             object_state_->buffers[active_objects_.array_buffer].data !=
                 nullptr)) &&
        (active_objects_.element_array_buffer == 0 ||
         CheckGlOperation(
             object_state_->buffers[active_objects_.element_array_buffer]
                 .data != nullptr)) &&
        CheckGlOperation(
            !container_state_
                 ->transform_feedbacks[active_objects_.transform_feedback]
                 .active) &&
        CheckFunction("DrawElementsInstanced")) {
      // There is nothing to do since we do not implement draw functions.
    }
  }

  // InvalidateFramebuffer group.
  void InvalidateFramebuffer(GLenum target, GLsizei numAttachments,
                             const GLenum *attachments) {
    // Only check arguments here, since this is just a performance hint.
    if (CheckFunction("InvalidateFramebuffer")) {
      CheckInvalidateFramebufferArgs(target, numAttachments, attachments);
    }
  }
  void InvalidateSubFramebuffer(GLenum target, GLsizei numAttachments,
                                const GLenum *attachments, GLint x, GLint y,
                                GLsizei width, GLsizei height) {
    // Only check arguments here, since this is just a performance hint.
    if (CheckFunction("InvalidateSubFramebuffer")) {
      CheckInvalidateFramebufferArgs(target, numAttachments, attachments);
    }
  }

  // DiscardFramebuffer group.
  void DiscardFramebufferEXT(GLenum target, GLsizei numAttachments,
                             const GLenum* attachments) {
    // Only check arguments here, since this is just a performance hint.
    if (CheckFunction("DiscardFramebufferEXT")) {
      CheckInvalidateFramebufferArgs(target, numAttachments, attachments);
    }
  }

  // MapBufferBase group.
  void GetBufferPointerv(GLenum target, GLenum pname, GLvoid** params) {
    // GL_INVALID_ENUM is generated if target or pname is not an accepted value.
    // GL_INVALID_OPERATION is generated if the reserved buffer object name 0 is
    // bound to target.
    if (CheckBufferTarget(target) &&
        CheckGlEnum(pname == GL_BUFFER_MAP_POINTER) &&
        CheckBufferZeroNotBound(target) && CheckFunction("GetBufferPointerv")) {
      GLuint index = GetBufferIndex(target);
      *params = object_state_->buffers[index].mapped_data;
    }
  }
  void UnmapBuffer(GLenum target) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted
    // targets.
    // GL_INVALID_OPERATION is generated if glUnmapBuffer is executed for a
    // buffer object whose data store is not currently mapped.
    // GL_INVALID_OPERATION is generated if the reserved buffer object name
    // 0 is bound to target.
    if (CheckBufferTarget(target) && CheckBufferZeroNotBound(target) &&
        CheckFunction("UnmapBuffer")) {
      GLuint index = GetBufferIndex(target);
      BufferObject& bo = object_state_->buffers[index];
      if (CheckGlOperation(bo.mapped_data != nullptr)) {
        bo.mapped_data = nullptr;
        bo.access = 0;
      }
    }
  }

  // MapBufferRange group.
  void FlushMappedBufferRange(GLenum target, GLintptr offset,
                                GLsizeiptr length) {
    // GL_INVALID_VALUE is generated if offset or length is negative, or if
    // offset + length exceeds the size of the mapping.
    // GL_INVALID_OPERATION is generated if zero is bound to target.
    // GL_INVALID_OPERATION is generated if the buffer bound to target is not
    // mapped, or is mapped without the GL_MAP_FLUSH_EXPLICIT_BIT flag.
    if (CheckBufferTarget(target) && CheckBufferZeroNotBound(target) &&
        CheckGlValue(offset >= 0 && length >= 0) &&
        CheckFunction("FlushMappedBufferRange")) {
      GLuint index = GetBufferIndex(target);
      BufferObject& bo = object_state_->buffers[index];
      if (CheckGlOperation(bo.mapped_data != nullptr &&
                           (bo.access & GL_MAP_FLUSH_EXPLICIT_BIT)) &&
          CheckGlValue(static_cast<uint32>(offset + length) <
                       bo.mapped_range.GetSize())) {
        // Nothing to do since we return explicit pointers into the data.
      }
    }
  }

  void* MapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                       GLbitfield access) {
    // GL_INVALID_VALUE is generated if either of offset or length is negative,
    // or if offset + length is greater than the value of GL_BUFFER_SIZE.
    //
    // GL_INVALID_VALUE is generated if access has any bits set other than those
    // defined.
    //
    // GL_INVALID_OPERATION is generated for any of the following conditions:
    //
    //   The buffer is already in a mapped state.
    //
    //   Neither GL_MAP_READ_BIT or GL_MAP_WRITE_BIT is set.
    //
    //   GL_MAP_READ_BIT is set and any of GL_MAP_INVALIDATE_RANGE_BIT,
    //     GL_MAP_INVALIDATE_BUFFER_BIT, or GL_MAP_UNSYNCHRONIZED_BIT is set.
    //
    //   GL_MAP_FLUSH_EXPLICIT_BIT is set and GL_MAP_WRITE_BIT is not set.
    //
    // GL_OUT_OF_MEMORY is generated if glMapBufferRange fails because memory
    // for the mapping could not be obtained.
    static const GLuint kRequiredMask = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
    static const GLuint kOptionalMask =
        GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
        GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
    static const GLuint kAllBadBits = ~(kRequiredMask | kOptionalMask);
    static const GLuint kBadReadBits = GL_MAP_INVALIDATE_RANGE_BIT |
                                       GL_MAP_INVALIDATE_BUFFER_BIT |
                                       GL_MAP_UNSYNCHRONIZED_BIT;
    static const GLuint kBadWriteBits = GL_MAP_FLUSH_EXPLICIT_BIT;
    void* data = nullptr;
    if (CheckBufferTarget(target) && CheckBufferZeroNotBound(target) &&
        CheckGlValue(offset >= 0 && length >= 0 &&
                     (access & kAllBadBits) == 0) &&
        CheckGlOperation(
            (access & kRequiredMask) &&
            !((access & GL_MAP_READ_BIT) && (access & kBadReadBits)) &&
            (!(access & kBadWriteBits) || (access & GL_MAP_WRITE_BIT))) &&
        CheckFunction("MapBufferRange")) {
      GLuint index = GetBufferIndex(target);
      BufferObject& bo = object_state_->buffers[index];
      if (CheckGlOperation(bo.mapped_data == nullptr) &&
          CheckGlValue(offset + length <= bo.size)) {
        uint8* int_data = reinterpret_cast<uint8*>(bo.data);
        data = bo.mapped_data = &int_data[offset];
        bo.access = access;
      }
    }
    return data;
  }

  // PointSize group.
  void PointSize(GLfloat size) {
    // GL_INVALID_VALUE is generated if size is less than or equal to 0.
    if (CheckGlValue(size > 0.f) && CheckFunction("PointSize"))
      point_size_ = size;
  }

  // SamplerObjects group.
  void BindSampler(GLuint unit, GLuint sampler) {
    // GL_INVALID_VALUE is generated if unit is greater than or equal to the
    // value of GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS.
    // GL_INVALID_OPERATION is generated if sampler is not zero or a name
    // previously returned from a call to glGenSamplers, or if such a name has
    // been deleted by a call to glDeleteSamplers.
    if (CheckGlValue(unit < max_texture_image_units_) &&
        CheckGlOperation(sampler == 0U || IsSampler(sampler)) &&
        CheckFunction("BindSampler"))
      image_units_[unit].sampler = sampler;
  }
  void DeleteSamplers(GLsizei n, const GLuint* samplers) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("DeleteSamplers")) {
      for (GLsizei i = 0; i < n; ++i) {
        // glDeleteSamplers silently ignores 0's and names that do not
        // correspond to existing sampler objects.
        if (samplers[i] != 0U && IsSampler(samplers[i])) {
          // Reset the sampler object.
          object_state_->samplers[samplers[i]] = SamplerObject();
          // Mark the sampler as deleted, so that it cannot be reused.
          object_state_->samplers[samplers[i]].deleted = true;

          // Reset any image units that use this sampler.
          for (GLuint j = 0; j < max_texture_image_units_; ++j)
            if (image_units_[j].sampler == samplers[i])
              image_units_[j].sampler = 0U;
        }
      }
    }
  }
  void GenSamplers(GLsizei n, GLuint* samplers) {
    // We generate a synthetic GL_INVALID_OPERATION if GenSamplers() is
    // disabled.
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("GenSamplers")) {
      for (GLsizei i = 0; i < n; ++i) {
        SamplerObject so;
        // OpenGL ids are 1-based.
        const GLuint id = static_cast<GLuint>(
            object_state_->samplers.size() + 1U);
        object_state_->samplers[id] = so;
        samplers[i] = id;
      }
    }
  }
  template <typename T>
  void GetSamplerParameterv(GLuint sampler, GLenum pname, T* params) {
    // GL_INVALID_VALUE is generated if sampler is not the name of a sampler
    // object returned from a previous call to glGenSamplers.
    // GL_INVALID_ENUM is generated if pname is not an accepted value.
    if (CheckGlValue(IsSampler(sampler))) {
      const SamplerObject& so = object_state_->samplers[sampler];
      switch (pname) {
        case GL_TEXTURE_COMPARE_FUNC:
          *params = static_cast<T>(so.compare_func);
          break;
        case GL_TEXTURE_COMPARE_MODE:
          *params = static_cast<T>(so.compare_mode);
          break;
        case GL_TEXTURE_MAG_FILTER:
          *params = static_cast<T>(so.mag_filter);
          break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
          *params = static_cast<T>(so.max_anisotropy);
          break;
        case GL_TEXTURE_MAX_LOD:
          *params = static_cast<T>(so.max_lod);
          break;
        case GL_TEXTURE_MIN_LOD:
          *params = static_cast<T>(so.min_lod);
          break;
        case GL_TEXTURE_MIN_FILTER:
          *params = static_cast<T>(so.min_filter);
          break;
        case GL_TEXTURE_WRAP_R:
          *params = static_cast<T>(so.wrap_r);
          break;
        case GL_TEXTURE_WRAP_S:
          *params = static_cast<T>(so.wrap_s);
          break;
        case GL_TEXTURE_WRAP_T:
          *params = static_cast<T>(so.wrap_t);
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }
  void GetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat* params) {
    GetSamplerParameterv(sampler, pname, params);
  }
  void GetSamplerParameteriv(GLuint sampler, GLenum pname, GLint* params) {
    GetSamplerParameterv(sampler, pname, params);
  }
  GLboolean IsSampler(GLuint id) {
    return (object_state_->samplers.count(id) &&
            !object_state_->samplers[id].deleted) ? GL_TRUE : GL_FALSE;
  }
  template <typename T>
  void SamplerParameter(GLuint sampler, GLenum pname, T param) {
    // GL_INVALID_VALUE is generated if sampler is not the name of a sampler
    // object previously returned from a call to glGenSamplers.
    // GL_INVALID_ENUM is generated if params should have a defined constant
    // value (based on the value of pname) and does not.
    if (CheckGlValue(IsSampler(sampler))) {
      SamplerObject& so = object_state_->samplers[sampler];
      switch (pname) {
        case GL_TEXTURE_COMPARE_FUNC:
          if (CheckGlEnum(param == GL_LEQUAL || param == GL_GEQUAL ||
                          param == GL_LESS || param == GL_GREATER ||
                          param == GL_EQUAL || param == GL_NOTEQUAL ||
                          param == GL_ALWAYS || param == GL_NEVER))
            so.compare_func = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_COMPARE_MODE:
          if (CheckGlEnum(param == GL_COMPARE_REF_TO_TEXTURE ||
                          param == GL_NONE))
            so.compare_mode = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_MAG_FILTER:
          if (CheckGlEnum(param == GL_NEAREST || param == GL_LINEAR))
            so.mag_filter = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_MAX_ANISOTROPY_EXT:
          if (CheckGlValue(param >= 1.f &&
                           param <= max_texture_max_anisotropy_))
            so.max_anisotropy = static_cast<GLfloat>(param);
          break;
        case GL_TEXTURE_MAX_LOD:
          so.max_lod = static_cast<GLfloat>(param);
          break;
        case GL_TEXTURE_MIN_FILTER:
          if (CheckGlEnum(param == GL_NEAREST || param == GL_LINEAR ||
                          param == GL_NEAREST_MIPMAP_NEAREST ||
                          param == GL_LINEAR_MIPMAP_NEAREST ||
                          param == GL_NEAREST_MIPMAP_LINEAR ||
                          param == GL_LINEAR_MIPMAP_LINEAR))
            so.min_filter = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_MIN_LOD:
          so.min_lod = static_cast<GLfloat>(param);
          break;
        case GL_TEXTURE_WRAP_R:
          if (CheckWrapMode(static_cast<GLenum>(param)))
            so.wrap_r = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_WRAP_S:
          if (CheckWrapMode(static_cast<GLenum>(param)))
            so.wrap_s = static_cast<GLenum>(param);
          break;
        case GL_TEXTURE_WRAP_T:
          if (CheckWrapMode(static_cast<GLenum>(param)))
            so.wrap_t = static_cast<GLenum>(param);
          break;
        default:
          CheckGlEnum(false);
          break;
      }
    }
  }
  template <typename T>
  void SamplerParameterv(GLuint sampler, GLenum pname, const T* params) {
    SamplerParameter(sampler, pname, params[0]);
  }
  void SamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    if (CheckFunction("SamplerParameterf"))
      SamplerParameter(sampler, pname, param);
  }
  void SamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat* params) {
    if (CheckFunction("SamplerParameterfv"))
      SamplerParameterv(sampler, pname, params);
  }
  void SamplerParameteri(GLuint sampler, GLenum pname, GLintenum param) {
    if (CheckFunction("SamplerParameteri"))
      SamplerParameter(sampler, pname, param);
  }
  void SamplerParameteriv(GLuint sampler, GLenum pname, const GLint* params) {
    if (CheckFunction("SamplerParameteriv"))
      SamplerParameterv(sampler, pname, params);
  }

  // SampleShading group.
  void MinSampleShading(float fraction) {
    if (CheckFunction("MinSampleShading"))
      min_sample_shading_ = Clampf(fraction);
  }

  // Sync objects group.
  GLenum ClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    const GLuint id = static_cast<GLuint>(reinterpret_cast<size_t>(sync));
    const GLbitfield allowed_flags = GL_SYNC_FLUSH_COMMANDS_BIT;
    // GL_INVALID_OPERATION is generated if sync is not a sync object.
    // GL_INVALID_VALUE is generated if flags has a bit set other than
    // GL_SYNC_FLUSH_COMMANDS_BIT.
    if (CheckGlValue(object_state_->syncs.count(id)) &&
        CheckGlValue(!object_state_->syncs[id].deleted) &&
        CheckGlValue((flags & ~allowed_flags) == 0) &&
        CheckFunction("ClientWaitSync")) {
      // We don't actually do any real waiting.
      object_state_->syncs[id].status = GL_SIGNALED;
    }
    return GL_CONDITION_SATISFIED;
  }
  void DeleteSync(GLsync sync) {
    // GL_INVALID_VALUE is generated if sync is not zero or the name of a sync
    // object.
    const GLuint id = static_cast<GLuint>(reinterpret_cast<size_t>(sync));
    if ((!sync || (CheckGlValue(object_state_->syncs.count(id)) &&
                          CheckGlValue(!object_state_->syncs[id].deleted))) &&
        CheckFunction("DeleteSync")) {
      // glDeleteSync silently ignores 0's.
      if (!sync)
        return;
      // Reset the sync object.
      object_state_->syncs[id] = SyncObject();
      object_state_->syncs[id].deleted = true;
    }
  }
  GLsync FenceSync(GLenum condition, GLbitfield flags) {
    // GL_INVALID_ENUM is generated if condition is not
    // GL_SYNC_GPU_COMMANDS_COMPLETE.
    // GL_INVALID_VALUE is generated if flags is not zero.
    if (!CheckGlEnum(condition == GL_SYNC_GPU_COMMANDS_COMPLETE) ||
        !CheckGlValue(flags == 0) || !CheckFunction("FenceSync")) {
      return nullptr;
    }
    // Create a SyncObject in signaled state.
    SyncObject sync = SyncObject();
    sync.type = GL_SYNC_FENCE;
    sync.status = GL_UNSIGNALED;
    sync.condition = GL_SYNC_GPU_COMMANDS_COMPLETE;
    const size_t id = static_cast<size_t>(object_state_->syncs.size() + 1U);
    object_state_->syncs[static_cast<GLuint>(id)] = sync;
    return reinterpret_cast<GLsync>(id);
  }
  void GetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei* length,
                 GLint* values) {
    const GLuint id = static_cast<GLuint>(reinterpret_cast<size_t>(sync));
    // GL_INVALID_VALUE is generated if sync is not a sync object.
    if (CheckGlValue(object_state_->syncs.count(id)) &&
        CheckGlValue(!object_state_->syncs[id].deleted) &&
        CheckFunction("GetSynciv")) {
        const SyncObject& so = object_state_->syncs[id];
        switch (pname) {
          case GL_OBJECT_TYPE:
            *values = so.type;
            break;
          case GL_SYNC_STATUS:
            *values = so.status;
            break;
          case GL_SYNC_CONDITION:
            *values = so.condition;
            break;
          case GL_SYNC_FLAGS:
            *values = so.flags;
            break;
          default:
            // GL_INVALID_ENUM is generated if pname is not an accepted value.
            CheckGlEnum(false);
            break;
        }
      }
  }
  void WaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    const GLuint id = static_cast<GLuint>(reinterpret_cast<size_t>(sync));
    // GL_INVALID_OPERATION is generated if sync is not a sync object.
    // GL_INVALID_VALUE is generated if flags is not zero or timeout is not
    // GL_TIMEOUT_IGNORED.
    if (CheckGlOperation(object_state_->syncs.count(id)) &&
        CheckGlOperation(!object_state_->syncs[id].deleted) &&
        CheckGlValue(flags == 0) &&
        CheckGlValue(timeout == GL_TIMEOUT_IGNORED) &&
        CheckFunction("WaitSync")) {
      // We don't actually do any real waiting.
      object_state_->syncs[id].status = GL_SIGNALED;
    }
  }

  // Texture3d group.
  void CompressedTexImage3D(GLenum target, GLint level, GLenum internal_format,
                            GLsizei width, GLsizei height, GLsizei depth,
                            GLint border, GLsizei image_size,
                            const GLvoid* data) {
    if (CheckGlEnum(
            // GL_INVALID_ENUM is generated if target is not
            // GL_TEXTURE_2D_ARRAY, GL_TEXTURE_3D, or GL_TEXTURE_CUBE_MAP_ARRAY.
            CheckTexture3dTarget(target) &&
            // GL_INVALID_ENUM is generated if internal_format is not a
            // supported format returned in GL_COMPRESSED_TEXTURE_FORMATS.
            CheckCompressedTextureFormat(internal_format)) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_ARRAY_TEXTURE_LAYERS when target is GL_TEXTURE_2D_ARRAY,
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_3D, or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is
            // GL_TEXTURE_CUBE_MAP_ARRAY.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if width or height or depth is less
            // than 0 or greater than GL_MAX_ARRAY_TEXTURE_LAYERS when target is
            // GL_TEXTURE_2D_ARRAY, GL_MAX_TEXTURE_SIZE when target is
            // GL_TEXTURE_3D, or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is
            // not.
            CheckTextureDimensions(target, width, height, depth) &&
            // GL_INVALID_VALUE is generated if border is not 0.
            border == 0 &&
            // GL_INVALID_VALUE is generated if image_size is not consistent
            // with the format, dimensions, and contents of the specified
            // compressed image data.
            //
            // GL_INVALID_OPERATION is generated if parameter combinations are
            // not supported by the specific compressed internal format as
            // specified in the specific texture compression extension.
            //
            // 
            // above errors.
            image_size > 0)) {
      const GLuint texture = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[texture];
      // Once a texture is marked immutable it cannot be modified.
      if (CheckGlOperation(!to.immutable) &&
          CheckFunction("CompressedTexImage3D")) {
        to.target = target;
        // Type and format are not used for compressed textures.
        to.internal_format = internal_format;
        to.border = border;
        TextureObject::MipLevel miplevel;
        miplevel.width = width;
        miplevel.height = height;
        miplevel.depth = depth;
        miplevel.data.reset(new char[image_size]);
        if (data) {
          std::memcpy(miplevel.data.get(), data, image_size);
        }
        to.levels
            .resize(std::max(level + 1, static_cast<GLint>(to.levels.size())));
        to.levels[level] = miplevel;
        to.compressed = true;
      }
    }
  }
  void CompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset,
                               GLint yoffset, GLint zoffset, GLsizei width,
                               GLsizei height, GLsizei depth, GLenum format,
                               GLsizei imageSize, const GLvoid* data) {
    if (CheckGlEnum(
            // GL_INVALID_ENUM is generated if target is not
            // GL_TEXTURE_2D_ARRAY, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARRAY.
            CheckTexture3dTarget(target) &&
            // GL_INVALID_ENUM is generated if internal_format is not a
            // supported format returned in GL_COMPRESSED_TEXTURE_FORMATS.
            CheckCompressedTextureFormat(format)) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D_ARRAY or
            // GL_TEXTURE_3D or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not
            // GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if width or height is less than 0.
            width >= 0 && height >= 0 &&
            // GL_INVALID_VALUE is generated if imageSize is not consistent
            // with the format, dimensions, and contents of the specified
            // compressed image data.
            //
            // GL_INVALID_OPERATION is generated if parameter combinations are
            // not supported by the specific compressed internal format as
            // specified in the specific texture compression extension.
            //
            // 
            // above errors.
            imageSize > 0)) {
      const GLuint tex_index = GetActiveTexture(target);
      TextureObject& texture = object_state_->textures[tex_index];
      // GL_INVALID_VALUE is generated if xoffset < 0, xoffset + width > w,
      // yoffset < 0, yoffset + height > h, zoffset < 0, or zoffset + depth > d,
      // where w is the width and h is the height and d is the depth of the
      // texture image being modified.
      // GL_INVALID_OPERATION is generated if the texture array has not been
      // defined by a previous glCompressedTexImage3D operation whose
      // internalformat matches the format of glCompressedTexSubImage3D.
      if (CheckGlOperation(texture.compressed) &&
          CheckGlOperation(level < static_cast<GLint>(texture.levels.size())) &&
          CheckGlValue(
              xoffset >= 0 && xoffset + width <= texture.levels[level].width &&
              yoffset >= 0 &&
              yoffset + height <= texture.levels[level].height &&
              zoffset >= 0 && zoffset + depth <= texture.levels[level].depth) &&
          CheckTextureDimensions(target, width, height, depth) &&
          CheckFunction("CompressedTexSubImage3D")) {
        // Do nothing since we do not implement mock compression.
      }
    }
  }
  void CopyTexSubImage3D(GLenum target, GLint level, GLint xoffset,
                         GLint yoffset, GLint zoffset, GLint x, GLint y,
                         GLsizei width, GLsizei height) {
    // GL_INVALID_ENUM is generated if target is not
    // GL_TEXTURE_2D_ARRAY, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARRAY.
    if (CheckGlEnum(CheckTexture3dTarget(target)) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if width or height is less than 0.
            width >= 0 && height >= 0)) {
      const GLuint tex_id = GetActiveTexture(target);
      TextureObject& texture = object_state_->textures[tex_id];
      // GL_INVALID_VALUE is generated if xoffset < 0, xoffset + width > w,
      // yoffset < 0, yoffset + height > h, zoffset < 0, or zoffset > d + 1,
      // where w is the width and h is the height and d is the depth of the
      // texture image being modified.
      // GL_INVALID_OPERATION is generated if the texture array has not been
      // defined by a previous glTexImage2D or glCopyTexImage2D operation.
      // GL_INVALID_OPERATION is generated if the currently bound framebuffer's
      // format does not contain a superset of the components required by the
      // base format of internalformat.
      // 
      // GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound
      // framebuffer is not framebuffer complete (i.e. the return value from
      // glCheckFramebufferStatus is not GL_FRAMEBUFFER_COMPLETE).
      if (CheckGlOperation(level < static_cast<GLint>(texture.levels.size())) &&
          CheckGlValue(
              xoffset >= 0 && xoffset + width <= texture.levels[level].width &&
              yoffset >= 0 &&
              yoffset + height <= texture.levels[level].height &&
              zoffset >= 0 && zoffset <= texture.levels[level].depth) &&
          CheckFramebuffer() && CheckFunction("CopyTexSubImage3D")) {
        // We don't copy mock texture data.
      }
    }
  }
  void TexImage3D(GLenum target, GLint level, GLint internal_format,
                  GLsizei width, GLsizei height, GLsizei depth, GLint border,
                  GLenum format, GLenum type, const GLvoid* pixels) {
    if (
        // GL_INVALID_ENUM is generated if target is not GL_TEXTURE_2D_ARRAY,
        // GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARRAY.
        CheckTexture3dTarget(target) &&
        // GL_INVALID_ENUM is generated if format or type is not an accepted
        // value.
        CheckTextureFormat(format) && CheckTextureType(type) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if internal_format is not an
            // accepted format.
            // GL_INVALID_VALUE is generated if width or height is less than 0
            // or greater than GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D
            // or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureDimensions(target, width, height, depth) &&
            // GL_INVALID_VALUE is generated if border is not 0.
            border == 0) &&
        // GL_INVALID_OPERATION is generated if the combination of
        // internal_format, format and type is not valid.
        CheckTextureFormatTypeAndInternalTypeAreValid(format, type,
                                                      internal_format)) {
      const GLuint texture = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[texture];
      // Once a texture is marked immutable it cannot be modified.
      if (CheckGlOperation(!to.immutable) && CheckFunction("TexImage3D")) {
        to.target = target;
        to.format = format;
        to.type = type;
        to.internal_format = internal_format;
        to.border = border;
        TextureObject::MipLevel miplevel;
        miplevel.width = width;
        miplevel.height = height;
        miplevel.depth = depth;
        miplevel.data.reset(new char[1]);
        to.levels
            .resize(std::max(level + 1, static_cast<GLint>(to.levels.size())));
        to.levels[level] = miplevel;
        // We do not convert to internal_format for mock data, we just need a
        // pointer to exist.
        to.compressed = false;
      }
    }
  }
  void TexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                     GLint zoffset, GLsizei width, GLsizei height,
                     GLsizei depth, GLenum format, GLenum type,
                     const GLvoid* data) {
    if (
        // GL_INVALID_ENUM is generated if target is not GL_TEXTURE_2D_ARRAY,
        // GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP_ARRAY.
        CheckTexture3dTarget(target) &&
        // GL_INVALID_ENUM is generated if format or type is not an accepted
        // value.
        CheckTextureFormat(format) && CheckTextureType(type) &&
        CheckGlValue(
            // GL_INVALID_VALUE is generated if level is less than 0.
            // GL_INVALID_VALUE may be generated if level is greater than
            // log_2(max), where max is the returned value of
            // GL_MAX_TEXTURE_SIZE when target is GL_TEXTURE_2D or
            // GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not GL_TEXTURE_2D.
            CheckTextureLevel(target, level) &&
            // GL_INVALID_VALUE is generated if internal_format is not an
            // accepted format.
            // GL_INVALID_VALUE is generated if width, height or depth is less
            // than 0 or greater than GL_MAX_TEXTURE_SIZE when target is
            // GL_TEXTURE_2D or GL_MAX_CUBE_MAP_TEXTURE_SIZE when target is not
            // GL_TEXTURE_2D.
            width >= 0 && height >= 0)) {
      const GLuint tex_id = GetActiveTexture(target);
      TextureObject& texture = object_state_->textures[tex_id];
      // GL_INVALID_VALUE is generated if xoffset < 0, xoffset + width > w,
      // yoffset < 0, yoffset + height > h, zoffset < 0, or zoffset + depth >
      // d,where w is the width, h is the height and d is the depth of the
      // texture image being modified.
      // GL_INVALID_OPERATION is generated if the texture array has not been
      // defined by a previous glTexImage2D or glCopyTexImage2D.
      // GL_INVALID_OPERATION is generated if the combination of
      // internalFormat of the previously specified texture array, format
      // and type is not valid
      if (CheckGlOperation(level < static_cast<GLint>(texture.levels.size())) &&
          CheckGlValue(
              xoffset >= 0 && xoffset + width <= texture.levels[level].width &&
              yoffset >= 0 &&
              yoffset + height <= texture.levels[level].height &&
              zoffset >= 0 && zoffset + depth <= texture.levels[level].depth) &&
          CheckTextureDimensions(target, width, height, depth) &&
          CheckTextureFormatTypeAndInternalTypeAreValid(
              format, type, texture.internal_format) &&
          CheckFunction("TexSubImage3D")) {
        // The Check functions will log errors as appropriate.
      }
    }
  }

  // TextureBarrier group.
  void TextureBarrier() {
    // TextureBarrier does not generate errors.
  }

  // TextureMultisample group.
  void TexImage2DMultisample(GLenum target, GLsizei samples,
                             GLenum internal_format, GLsizei width,
                             GLsizei height, GLboolean fixed_sample_locations) {
    if (
        // GL_INVALID_ENUM is generated if target is not TEXTURE_2D_MULTISAMPLE.
        CheckTexture2dMultisampleTargetType(target) &&
        // GL_INVALID_VALUE is generated if samples is invalid.
        CheckTextureSamples(samples) &&
        // GL_INVALID_OPERATION is generated if internal_format is invalid.
        CheckTextureInternalFormat(internal_format) &&
        // GL_INVALID_VALUE is generated if width or height is less than 0
        // or greater than GL_MAX_TEXTURE_SIZE.
        CheckTextureDimensions(target, width, height, 1)) {
      const GLuint texture = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[texture];
      // Once a texture is marked immutable it cannot be modified.
      if (CheckGlOperation(!to.immutable) &&
          CheckFunction("TexImage2DMultisample")) {
        to.target = target;
        to.samples = samples;
        to.fixed_sample_locations = fixed_sample_locations;
        to.internal_format = internal_format;
        TextureObject::MipLevel miplevel;
        miplevel.width = width;
        miplevel.height = height;
        miplevel.depth = 1;
        miplevel.data.reset(new char[1]);
        to.levels.resize(1);
        to.levels[0] = miplevel;
        // We do not convert to internal_format for mock data, we just need a
        // pointer to exist.
        to.compressed = false;
      }
    }
  }
  void TexImage3DMultisample(GLenum target, GLsizei samples,
                             GLenum internal_format, GLsizei width,
                             GLsizei height, GLsizei depth,
                             GLboolean fixed_sample_locations) {
    if (
        // GL_INVALID_ENUM is generated if target is not
        // GL_TEXTURE_2D_MULTISAMPLE_ARRAY.
        CheckTexture3dMultisampleTargetType(target) &&
        // GL_INVALID_VALUE is generated if samples is invalid.
        CheckTextureSamples(samples) &&
        // GL_INVALID_OPERATION is generated if internal_format is invalid.
        CheckTextureInternalFormat(internal_format) &&
        // GL_INVALID_VALUE is generated if width or height is less than 0
        // or greater than GL_MAX_TEXTURE_SIZE.
        CheckTextureDimensions(target, width, height, depth)) {
      const GLuint texture = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[texture];
      // Once a texture is marked immutable it cannot be modified.
      if (CheckGlOperation(!to.immutable) &&
          CheckFunction("TexImage3DMultisample")) {
        to.target = target;
        to.samples = samples;
        to.fixed_sample_locations = fixed_sample_locations;
        to.internal_format = internal_format;
        TextureObject::MipLevel miplevel;
        miplevel.width = width;
        miplevel.height = height;
        miplevel.depth = 1;
        miplevel.data.reset(new char[1]);
        to.levels.resize(1);
        to.levels[0] = miplevel;
        // We do not convert to internal_format for mock data, we just need a
        // pointer to exist.
        to.compressed = false;
      }
    }
  }
  void GetMultisamplefv(GLenum pname, GLuint index, GLfloat* val) {
    if (CheckGlEnum(pname == GL_SAMPLE_POSITION)) {
      GLuint texture = GetActiveTexture(GL_TEXTURE_2D_MULTISAMPLE);
      if (CheckGlOperation(texture)) {
        TextureObject& to = object_state_->textures[texture];
        if (CheckGlValue(index < to.samples)) {
          // Vary positions by sample index.
          GLfloat value =
              static_cast<GLfloat>(index) / static_cast<GLfloat>(to.samples);
          val[0] = value;
          val[1] = value;
        }
      }
    }
  }
  void SampleMaski(GLuint index, GLbitfield mask) {
    if (CheckGlValue(index <= max_sample_mask_words_)) {
      // We only store the mask as we don't do any actual rendering.
      sample_masks_[index] = mask;
    }
  }

  // TexStorage group.
  void TexStorage2D(GLenum target, GLsizei levels, GLenum internalformat,
                    GLsizei width, GLsizei height) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted target
    // enumerants.
    // GL_INVALID_OPERATION is generated if the default texture object is
    // curently bound to target.
    // GL_INVALID_OPERATION is generated if the texture object curently bound to
    // target already has GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.
    // GL_INVALID_ENUM is generated if internalformat is not a valid sized
    // internal format.
    // GL_INVALID_VALUE is generated if width, height or levels are less than 1.
    // GL_INVALID_OPERATION is generated if levels is greater than
    // log2(max(width, height)) + 1.
    if (CheckTexture2dTarget(target)) {
      const GLuint tex_id = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[tex_id];
      if (CheckGlOperation(
              tex_id && !to.immutable &&
              levels <= GetTextureMipMapLevelCount(target, width, height, 1)) &&
          CheckGlValue(width >= 1 && height >= 1 && levels >= 1) &&
          CheckTextureDimensions(target, width, height, 1) &&
          CheckTextureInternalFormat(internalformat) &&
          CheckFunction("TexStorage2D")) {
        // Find the proper format and type given an internal format.
        const Image::PixelFormat pf =
            GetImageTypeAndFormatFromInternalFormat(internalformat);
        if (target == GL_TEXTURE_1D_ARRAY) {
          for (GLsizei i = 0; i < levels; i++) {
            TexImage2D(target, i, internalformat, width, height, 0, pf.format,
                       pf.type, nullptr);
            width = std::max(1, (width / 2));
          }
        } else if (target == GL_TEXTURE_2D) {
          for (GLsizei i = 0; i < levels; i++) {
            TexImage2D(target, i, internalformat, width, height, 0, pf.format,
                       pf.type, nullptr);
            width = std::max(1, (width / 2));
            height = std::max(1, (height / 2));
          }
        } else if (target == GL_TEXTURE_CUBE_MAP) {
          for (GLsizei i = 0; i < levels; i++) {
            for (GLsizei j = 0; j < 6; ++j) {
              const GLenum face = base::EnumHelper::GetConstant(
                  static_cast<CubeMapTexture::CubeFace>(j));
              TexImage2D(face, i, internalformat, width, height, 0, pf.format,
                         pf.type, nullptr);
            }
            width = std::max(1, (width / 2));
            height = std::max(1, (height / 2));
          }
        }
        to.immutable = true;
      }
    }
  }
  void TexStorage3D(GLenum target, GLsizei levels, GLenum internalformat,
                    GLsizei width, GLsizei height, GLsizei depth) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted target
    // enumerants.
    // GL_INVALID_OPERATION is generated if the default texture object is
    // curently bound to target.
    // GL_INVALID_OPERATION is generated if the texture object curently bound to
    // target already has GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.
    // GL_INVALID_ENUM is generated if internalformat is not a valid sized
    // internal format.
    // GL_INVALID_VALUE is generated if width, height, depth or levels are less
    // than 1.
    // GL_INVALID_OPERATION is generated if levels is greater than
    // log2(max(width, height, depth)) + 1.
    if (CheckTexture3dTarget(target)) {
      const GLuint tex_id = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[tex_id];
      if (CheckGlOperation(tex_id && !to.immutable &&
                           levels <= GetTextureMipMapLevelCount(
                                         target, width, height, depth)) &&
          CheckGlValue(width >= 1 && height >= 1 && depth >= 1 &&
                       levels >= 1) &&
          CheckTextureDimensions(target, width, height, depth) &&
          CheckTextureInternalFormat(internalformat) &&
          CheckFunction("TexStorage3D")) {
        // Find the proper format and type given an internal format.
        const Image::PixelFormat pf =
            GetImageTypeAndFormatFromInternalFormat(internalformat);
        if (target == GL_TEXTURE_2D_ARRAY ||
            target == GL_TEXTURE_CUBE_MAP_ARRAY) {
          for (GLsizei i = 0; i < levels; i++) {
            TexImage3D(target, i, internalformat, width, height, depth, 0,
                       pf.format, pf.type, nullptr);
            width = std::max(1, (width / 2));
            height = std::max(1, (height / 2));
          }
        } else if (target == GL_TEXTURE_3D) {
          for (GLsizei i = 0; i < levels; i++) {
            TexImage3D(target, i, internalformat, width, height, depth, 0,
                       pf.format, pf.type, nullptr);
            width = std::max(1, (width / 2));
            height = std::max(1, (height / 2));
            depth = std::max(1, (depth / 2));
          }
        }
        to.immutable = true;
      }
    }
  }

  // TexStorageMultisample group.
  void TexStorage2DMultisample(GLenum target, GLsizei samples,
                               GLenum internal_format, GLsizei width,
                               GLsizei height,
                               GLboolean fixed_sample_locations) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted target
    // enumerants.
    // GL_INVALID_OPERATION is generated if the default texture object is
    // curently bound to target.
    // GL_INVALID_OPERATION is generated if the texture object curently bound to
    // target already has GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.
    // GL_INVALID_ENUM is generated if internalformat is not a valid sized
    // internal format.
    // GL_INVALID_VALUE is generated if width or height are less than 1.
    // GL_INVALID_VALUE if samples is more than max samples.
    if (CheckTexture2dMultisampleTargetType(target)) {
      const GLuint tex_id = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[tex_id];
      if (CheckGlOperation(
          tex_id && !to.immutable &&
          CheckGlValue(width >= 1 && height >= 1) &&
          CheckGlValue(samples <= max_samples_) &&
          CheckTextureDimensions(target, width, height, 1) &&
          CheckTextureInternalFormat(internal_format) &&
          CheckFunction("TexStorage2DMultisample"))) {
        TexImage2DMultisample(target, samples, internal_format, width,
                              height, fixed_sample_locations);
        width = std::max(1, (width / 2));
        to.immutable = true;
      }
    }
  }
  void TexStorage3DMultisample(GLenum target, GLsizei samples,
                               GLenum internal_format, GLsizei width,
                               GLsizei height, GLsizei depth,
                               GLboolean fixed_sample_locations) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted target
    // enumerants.
    // GL_INVALID_OPERATION is generated if the default texture object is
    // curently bound to target.
    // GL_INVALID_OPERATION is generated if the texture object curently bound to
    // target already has GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.
    // GL_INVALID_ENUM is generated if internalformat is not a valid sized
    // internal format.
    // GL_INVALID_VALUE is generated if width, height, or depth are less
    // than 1.
    // GL_INVALID_VALUE if samples is more than max samples.
    if (CheckTexture3dMultisampleTargetType(target)) {
      const GLuint tex_id = GetActiveTexture(target);
      TextureObject& to = object_state_->textures[tex_id];
      if (CheckGlOperation(tex_id && !to.immutable &&
          CheckGlValue(width >= 1 && height >= 1 && depth >= 1) &&
          CheckGlValue(samples <= max_samples_) &&
          CheckTextureDimensions(target, width, height, depth) &&
          CheckTextureInternalFormat(internal_format) &&
          CheckFunction("TexStorage3DMultisample"))) {
        TexImage3DMultisample(target, samples, internal_format, width, height,
                              depth, fixed_sample_locations);
        width = std::max(1, (width / 2));
        height = std::max(1, (height / 2));
        to.immutable = true;
      }
    }
  }

  // TiledRendering group.
  void StartTilingQCOM(GLuint x, GLuint y, GLuint width, GLuint height,
                       GLbitfield preserve_mask) {
    if (CheckGlOperation(!is_tiling_) &&
        CheckGlOperation(CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) ==
                         GL_FRAMEBUFFER_COMPLETE) &&
        CheckFunction("StartTilingQCOM")) {
      is_tiling_ = true;
    }
  }
  void EndTilingQCOM(GLbitfield preserve_mask) {
    if (CheckGlOperation(is_tiling_) && CheckFunction("StartTilingQCOM")) {
      is_tiling_ = false;
    }
  }

  // TransformFeedback group.
  void BeginTransformFeedback(GLenum primitive_mode) {
    GLuint tfo_id = active_objects_.transform_feedback;
    TransformFeedbackObject& tfo =
        container_state_->transform_feedbacks[tfo_id];
    GLuint po_id = active_objects_.program;
    const ProgramObject& po = object_state_->programs[po_id];
    // GL_INVALID_OPERATION is generated if BeginTransformFeedback is executed
    // while transform feedback is active.
    // GL_INVALID_ENUM is generated by BeginTransformFeedback if primitive_mode
    // is not one of GL_POINTS, GL_LINES, or GL_TRIANGLES.
    // GL_INVALID_OPERATION is generated by BeginTransformFeedback if there
    // is no active transform feedback object.
    if (CheckGlOperation(!tfo.active) &&
        CheckGlEnum(primitive_mode == GL_POINTS || primitive_mode == GL_LINES ||
                    primitive_mode == GL_TRIANGLES) &&
        CheckAllBindingPointsBound(tfo.binding_point_status) &&
        CheckGlOperation(tfo_id > 0U && po_id > 0U &&
                         !po.requested_tf_varyings.empty()) &&
        CheckFunction("BeginTransformFeedback")) {
      tfo.active = true;
      tfo.primitive_mode = primitive_mode;
    }
  }
  void EndTransformFeedback() {
    GLuint tfo_id = active_objects_.transform_feedback;
    TransformFeedbackObject& tfo =
        container_state_->transform_feedbacks[tfo_id];
    // According to OpenGl page:
    // https://www.khronos.org/opengles/sdk/docs/man31/, An implicit
    // ResumeTransformFeedback is performed by EndTransformFeedback if the
    // transform feedback is paused.
    if (tfo.paused) {
      tfo.paused = false;
    }
    // GL_INVALID_OPERATION is generated if EndTransformFeedback is executed
    // while transform feedback is not active.
    if (CheckGlOperation(tfo.active) && CheckFunction("EndTransformFeedback")) {
      tfo.active = false;
    }
  }
  void GetTransformFeedbackVarying(GLuint program, GLuint index,
                                   GLsizei buf_size, GLsizei* length,
                                   GLsizei* size, GLenum* type, GLchar* name) {
    auto it = object_state_->programs.find(program);
    // GL_INVALID_VALUE is generated if program is not the name of a program
    // object.
    // GL_INVALID_VALUE is generated if index is greater or equal to the value
    // of GL_TRANSFORM_FEEDBACK_VARYINGS.
    // GL_INVALID_OPERATION is generated program has not been linked.
    if (CheckGlValue(it != object_state_->programs.end()) &&
        CheckGlOperation(it->second.link_status) &&
        CheckGlValue(index < it->second.resolved_tf_varyings.size()) &&
        CheckFunction("GetTransformFeedbackVarying")) {
      const auto& v = it->second.resolved_tf_varyings[index];
      if (length) {
        *length = std::min(static_cast<GLsizei>(v.name.size()), buf_size);
      }
      *size = v.size;
      *type = v.type;
      std::strncpy(name, v.name.c_str(),
                   std::min(static_cast<GLsizei>(v.name.size() + 1), buf_size));
    }
  }
  void TransformFeedbackVaryings(GLuint program, GLsizei count,
                                 const GLchar** varyings, GLenum buffer_mode) {
    auto it = object_state_->programs.find(program);
    // GL_INVALID_VALUE is generated if program is not the name of a program
    // object.
    // An GL_INVALID_ENUM error is generated if bufferMode is not
    // GL_SEPARATE_ATTRIBS or GL_INTERLEAVED_ATTRIBS.
    // GL_INVALID_VALUE is generated if buffer_mode is GL_SEPARATE_ATTRIBS and
    // count is greater than the implementation-dependent limit
    // GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS.
    if (CheckGlValue(it != object_state_->programs.end()) &&
        CheckGlEnum(buffer_mode == GL_SEPARATE_ATTRIBS ||
                    buffer_mode == GL_INTERLEAVED_ATTRIBS) &&
        CheckGlValue(
            buffer_mode != GL_SEPARATE_ATTRIBS ||
            count <= GraphicsManager::kMaxTransformFeedbackSeparateAttribs) &&
        CheckFunction("TransformFeedbackVaryings")) {
      ProgramObject& po = it->second;
      po.requested_tf_varyings = std::vector<std::string>(
          varyings, varyings + count);
      po.transform_feedback_mode = buffer_mode;
    }
  }
  void BindTransformFeedback(GLenum target, GLuint id) {
    // GL_INVALID_ENUM is generated if target is not GL_TRANSFORM_FEEDBACK.
    if (!CheckGlEnum(target == GL_TRANSFORM_FEEDBACK)) return;
    // GL_INVALID_OPERATION is generated if the transform feedback operation is
    // active on the currently bound transform feedback object, and that
    // operation is not paused.
    // GL_INVALID_OPERATION is generated if id is not zero or the name of a
    // transform feedback object returned from a previous call to
    // GenTransformFeedbacks, or if such a name has been deleted by
    // DeleteTransformFeedbacks.
    GLuint tfid = active_objects_.transform_feedback;
    if (!CheckGlOperation(
            IsTransformFeedbackName(id) &&
            (tfid == 0U ||
             !container_state_->transform_feedbacks[tfid].active)))
      return;
    if (!CheckFunction("BindTransformFeedback")) return;

    active_objects_.transform_feedback = id;
    container_state_->transform_feedbacks[id].bindings.push_back(
        GetCallCount());
  }
  void DeleteTransformFeedbacks(GLsizei n, const GLuint* ids) {
    if (CheckGlValue(n >= 0) && CheckFunction("DeleteTransformFeedbacks")) {
      for (GLsizei i = 0; i < n; ++i) {
        if (IsTransformFeedbackName(ids[i])) {
          container_state_->transform_feedbacks[ids[i]] =
              TransformFeedbackObject();
          container_state_->transform_feedbacks[ids[i]].deleted = true;
          if (ids[i] == active_objects_.transform_feedback)
            active_objects_.transform_feedback = 0U;
        }
      }
    }
  }
  void GenTransformFeedbacks(GLsizei n, GLuint* ids) {
    if (CheckFunction("GenTransformFeedbacks")) {
      for (GLsizei i = 0; i < n; ++i) {
        // OpenGL ids are 1-based.
        const GLuint id = static_cast<GLuint>(
            container_state_->transform_feedbacks.size() + 1U);
        TransformFeedbackObject tfo;
        ids[i] = tfo.id = id;
        container_state_->transform_feedbacks[id] = tfo;
      }
    }
  }
  void PauseTransformFeedback() {
    GLuint tfo_id = active_objects_.transform_feedback;
    TransformFeedbackObject& tfo =
        container_state_->transform_feedbacks[tfo_id];
    // GL_INVALID_OPERATION is generated if the currently bound transform
    // feedback object is not active or is paused.
    if (CheckGlOperation(tfo.active && !tfo.paused) &&
        CheckFunction("PauseTransformFeedback")) {
      tfo.paused = true;
    }
  }
  void ResumeTransformFeedback() {
    GLuint tfo_id = active_objects_.transform_feedback;
    TransformFeedbackObject& tfo =
        container_state_->transform_feedbacks[tfo_id];
    // GL_INVALID_OPERATION is generated if the currently bound transform
    // feedback object is not active or is not paused.
    if (CheckGlOperation(tfo.active && tfo.paused) &&
        CheckFunction("ResumeTransformFeedback")) {
      tfo.paused = false;
    }
  }

  // VertexArray group.
  void BindVertexArray(GLuint array) {
    // GL_INVALID_OPERATION is generated if array is not zero or the name of a
    // vertex array object previously returned from a call to glGenVertexArrays.
    if (CheckGlOperation(IsVertexArrayName(array)) &&
        CheckFunction("BindVertexArray")) {
      active_objects_.vertex_array = array;
      container_state_->arrays[array].bindings.push_back(GetCallCount());
      active_objects_.element_array_buffer =
          container_state_->arrays[array].element_array;
    }
  }
  void DeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("DeleteVertexArrays")) {
      for (GLsizei i = 0; i < n; ++i) {
        // glDeleteArrays silently ignores 0's and names that do not correspond
        // to existing array objects.
        if (arrays[i] != 0U && IsVertexArray(arrays[i])) {
          // Reset the array object.
          container_state_->arrays[arrays[i]] = ArrayObject();
          // All attributes must be allocated, even for deleted vertex arrays.
          container_state_->arrays[arrays[i]].attributes.
              resize(max_vertex_attribs_);
          // Mark the array as deleted, so that it cannot be reused.
          container_state_->arrays[arrays[i]].deleted = true;

          // Reset the binding if the index is the currently bound object.
          if (arrays[i] == active_objects_.vertex_array)
              active_objects_.vertex_array = 0U;
        }
      }
    }
  }
  void GenVertexArrays(GLsizei n, GLuint* arrays) {
    // We generate a synthetic GL_INVALID_OPERATION if GenVertexArrays() is
    // disabled.
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("GenVertexArrays")) {
      for (GLsizei i = 0; i < n; ++i) {
        // A new array shares global state.
        ArrayObject ao = container_state_->arrays[0];
        ao.attributes.resize(max_vertex_attribs_);
        // OpenGL ids are 1-based, but there is a default array at index 0.
        const GLuint id = static_cast<GLuint>(container_state_->arrays.size());
        container_state_->arrays[id] = ao;
        arrays[i] = id;
      }
    }
  }

  // Raw group.
  // Misc.
  void BindImageTexture(GLuint unit, GLuint texture, GLint level,
                        GLboolean layered, GLint layer, GLenum access,
                        GLenum format) {}
  void MemoryBarrier(GLbitfield) {}
  void TexBuffer(GLintenum target, GLenum internal_format, GLint buffer) {}
  // Timer queries.
  void BeginQuery(GLenum target, GLuint id) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted
    // tokens.
    // GL_INVALID_OPERATION is generated if glBeginQuery is executed while a
    // query object of the same target is already active.
    // GL_INVALID_OPERATION is generated if id is 0.
    // GL_INVALID_OPERATION is generated if id is the name of an already active
    // query object.
    // GL_INVALID_OPERATION is generated if glBeginQuery is called
    // when a query of the given <target> is already active.
    if (!CheckFunction("BeginQuery") ||
        !CheckGlEnum(target == GL_TIME_ELAPSED_EXT) ||
        !CheckGlOperation(id != 0) ||
        !CheckGlOperation(object_state_->timers.count(id)) ||
        !CheckGlOperation(!object_state_->timers[id].deleted) ||
        !CheckGlOperation(active_begin_query_ == 0)) {
      return;
    }
    object_state_->timers[id].mode = TimerObject::kIsBeginEndQuery;
    // For testing we use fixed timestamps to avoid clock issues.
    object_state_->timers[id].timestamp = 1;
    active_begin_query_ = id;
  }
  void DeleteQueries(GLsizei n, const GLuint* ids) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("DeleteQueries")) {
      for (GLsizei i = 0; i < n; ++i) {
        // glDeleteQueries silently ignores 0's and names that do not correspond
        // to existing timer queries.
        if (ids[i] != 0U && object_state_->timers.count(ids[i]) &&
            !object_state_->timers[ids[i]].deleted) {
          // Reset the timer object.
          object_state_->timers[ids[i]] = TimerObject();
          // Mark the timer as deleted, so that it cannot be reused.
          object_state_->timers[ids[i]].deleted = true;
        }
      }
    }
  }
  void EndQuery(GLenum target) {
    GLuint id = active_begin_query_;
    // GL_INVALID_ENUM is generated if target is not one of the accepted
    // tokens.
    // GL_INVALID_OPERATION is generated if glEndQuery is executed when a query
    // object of the same target is not active.
    if (!CheckFunction("EndQuery") ||
        !CheckGlEnum(target == GL_TIME_ELAPSED_EXT) ||
        !CheckGlOperation(id != 0) ||
        !CheckGlOperation(object_state_->timers.count(id)) ||
        !CheckGlOperation(!object_state_->timers[id].deleted)) {
      return;
    }
    object_state_->timers[id].is_data_available = true;
    // For testing we use fixed duration to avoid clock issues.
    object_state_->timers[id].duration = 1;
    active_begin_query_ = 0;
  }
  void GenQueries(GLsizei n, GLuint *ids) {
    // GL_INVALID_VALUE is generated if n is negative.
    if (CheckGlValue(n >= 0) && CheckFunction("GenQueries")) {
      for (GLsizei i = 0; i < n; ++i) {
        TimerObject to;
        // OpenGL ids are 1-based, but there is a default timer at index 0.
        const GLuint id = static_cast<GLuint>(object_state_->timers.size());
        object_state_->timers[id] = to;
        ids[i] = id;
      }
    }
  }
  void GetQueryiv(GLenum target, GLenum pname, GLint *params) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted
    // tokens.
    // GL_INVALID_ENUM is generated if pname is not one of the accepted
    // tokens.
    if (!CheckFunction("GetQueryiv") ||
        !CheckGlEnum(target == GL_TIMESTAMP_EXT ||
                     target == GL_TIME_ELAPSED_EXT) ||
        !CheckGlEnum(pname == GL_CURRENT_QUERY_EXT ||
                     pname == GL_QUERY_COUNTER_BITS_EXT)) {
      return;
    }
    if (pname == GL_CURRENT_QUERY_EXT) {
      if (target == GL_TIME_ELAPSED_EXT) {
        *params = active_begin_query_;
      } else {
        *params = 0;
      }
    } else {
      // GL_QUERY_COUNTER_BITS_EXT
      *params = 64;
    }
  }
  void GetQueryObjecti64v(GLuint id, GLenum pname, GLint64* param) {
    if (CheckFunction("GetQueryObjecti64v"))
      GetQueryObjectv(id, pname, param);
  }
  void GetQueryObjectiv(GLuint id, GLenum pname, GLint* param) {
    if (CheckFunction("GetQueryObjectiv"))
      GetQueryObjectv(id, pname, param);
  }
  void GetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* param) {
    if (CheckFunction("GetQueryObjectui64v"))
      GetQueryObjectv(id, pname, param);
  }
  void GetQueryObjectuiv(GLuint id, GLenum pname, GLuint* param) {
    if (CheckFunction("GetQueryObjectuiv"))
      GetQueryObjectv(id, pname, param);
  }
  template<class T>
  void GetQueryObjectv(GLuint id, GLenum pname, T* param) {
    // GL_INVALID_ENUM is generated if pname is not one of the accepted
    // tokens.
    // GL_INVALID_OPERATION is generated if id is not the name of a query
    // object.
    if (!CheckGlEnum(pname == GL_QUERY_RESULT_EXT ||
                     pname == GL_QUERY_RESULT_AVAILABLE_EXT) ||
        !CheckGlOperation(id != 0) ||
        !CheckGlOperation(object_state_->timers.count(id)) ||
        !CheckGlOperation(!object_state_->timers[id].deleted) ||
        !CheckGlOperation(id != active_begin_query_)) {
      return;
    }
    if (pname == GL_QUERY_RESULT_EXT) {
      if (object_state_->timers[id].mode == TimerObject::kIsBeginEndQuery) {
        *param = static_cast<T>(object_state_->timers[id].duration);
      } else {
        // Assume GL_TIMESTAMP_EXT
        *param = static_cast<T>(object_state_->timers[id].timestamp);
      }
      object_state_->timers[id] = TimerObject();
    } else {
      // GL_QUERY_RESULT_AVAILABLE_EXT
      // Always return true because we don't simulate any async results.
      *param = static_cast<T>(GL_TRUE);
    }
  }
  GLboolean IsQuery(GLuint id) {
    return (id == 0 || !object_state_->timers.count(id) ||
            object_state_->timers[id].deleted) ? GL_FALSE : GL_TRUE;
  }
  void QueryCounter(GLuint id, GLenum target) {
    // GL_INVALID_ENUM is generated if target is not one of the accepted
    // tokens.
    // GL_INVALID_OPERATION is generated if glQueryCounter is called
    // on a query object that is already in use inside a
    // glBeginQuery/glEndQuery.
    if (!CheckFunction("QueryCounter") ||
        !CheckGlEnum(target == GL_TIMESTAMP_EXT) ||
        !CheckGlOperation(id != 0) ||
        !CheckGlOperation(object_state_->timers.count(id)) ||
        !CheckGlOperation(!object_state_->timers[id].deleted) ||
        !CheckGlOperation(id != active_begin_query_)) {
      return;
    }
    object_state_->timers[id].mode = TimerObject::kIsQueryCounter;
    object_state_->timers[id].is_data_available = true;
    // For testing we use fixed timestamps to avoid clock issues.
    object_state_->timers[id].timestamp = 1;
  }


  //---------------------------------------------------------------------------

  // Returns a bit index for a capability enum or -1 if there is none.
  int GetCapabilityIndex(GLenum cap);

  // Calls and verifies GetCapabilityIndex() for a known capability.
  int GetAndVerifyCapabilityIndex(GLenum cap) {
    return GetCapabilityIndex(cap);
  }

  void ResizeInternalState();

  // Storage for GL implementation limits.
#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init) Type sname##_;
#include "ion/gfx/glconstants.inc"

  // Window sizes passed to the constructor.
  int window_width_;
  int window_height_;

  ActiveObjects active_objects_;

  // Object state.
  struct ObjectState {
    std::map<GLuint, BufferObject> buffers;
    std::map<GLuint, ProgramObject> programs;
    std::map<GLuint, RenderbufferObject> renderbuffers;
    std::map<GLuint, SamplerObject> samplers;
    std::map<GLuint, ShaderObject> shaders;
    std::map<GLuint, SyncObject> syncs;
    std::map<GLuint, TextureObject> textures;
    std::map<GLuint, TimerObject> timers;
  };
  // State of container objects which are never shared between contexts, even
  // within the same share group.
  struct ContainerState {
    std::map<GLuint, ArrayObject> arrays;
    std::map<GLuint, FramebufferObject> framebuffers;
    std::map<GLuint, TransformFeedbackObject> transform_feedbacks;
  };

  std::shared_ptr<ObjectState> object_state_;
  std::unique_ptr<ContainerState> container_state_;

  // Image unit state.
  std::vector<ImageUnit> image_units_;

  // Set of calls that will always fail.
  std::set<std::string> fail_functions_;

  // Stack of debug tracing labels.
  std::vector<std::string> tracing_prefixes_;

  // Last error.
  GLenum error_code_;

  // Extensions strings. Stored as both a single space-separated string and a
  // vector of individual strings, so that glGetString can return a constant
  // value.
  std::string extensions_string_;
  std::vector<std::string> extension_strings_;

  std::string vendor_string_;
  std::string renderer_string_;
  std::string version_string_;

  GLint context_profile_mask_;
  GLint context_flags_;

  // Maximum buffer size for testing out-of-memory errors.
  GLsizeiptr max_buffer_size_;

  // Enabled capability state.
  static const size_t kNumStaticCapabilities = 16;
  std::vector<bool> enabled_state_;

  // Blending state.
  GLfloat blend_color_[4];
  GLenum rgb_blend_equation_;
  GLenum alpha_blend_equation_;
  GLenum rgb_blend_source_factor_;
  GLenum rgb_blend_destination_factor_;
  GLenum alpha_blend_source_factor_;
  GLenum alpha_blend_destination_factor_;

  // Color state.
  GLfloat clear_color_[4];
  GLboolean color_write_masks_[4];  // Red, green, blue, alpha.

  // Face culling state.
  GLenum cull_face_mode_;
  GLenum front_face_mode_;

  // Depth buffer state.
  float clear_depth_value_;
  GLenum depth_function_;
  math::Range1f depth_range_;
  GLboolean depth_write_mask_;

  // Hint state.
  GLenum generate_mipmap_hint_;

  // Pixel storage modes.
  GLint pack_alignment_;
  GLint unpack_alignment_;

  // Line width.
  GLfloat line_width_;

  // Point size.
  GLfloat point_size_;

  // Polygon offset state.
  GLfloat polygon_offset_factor_;
  GLfloat polygon_offset_units_;

  // Sample coverage state.
  GLfloat sample_coverage_value_;
  GLboolean sample_coverage_inverted_;

  // Sample masks.
  std::vector<GLbitfield> sample_masks_;

  // Sample shading state.
  GLfloat min_sample_shading_;

  // Scissoring state.
  GLint scissor_x_;
  GLint scissor_y_;
  GLsizei scissor_width_;
  GLsizei scissor_height_;

  // Stenciling state.
  GLenum front_stencil_function_;
  GLenum back_stencil_function_;
  GLint front_stencil_reference_value_;
  GLint back_stencil_reference_value_;
  GLuint front_stencil_mask_;
  GLuint back_stencil_mask_;
  GLenum front_stencil_fail_op_;
  GLenum front_stencil_depth_fail_op_;
  GLenum front_stencil_pass_op_;
  GLenum back_stencil_fail_op_;
  GLenum back_stencil_depth_fail_op_;
  GLenum back_stencil_pass_op_;
  GLint clear_stencil_value_;
  GLuint front_stencil_write_mask_;
  GLuint back_stencil_write_mask_;

  // Viewport state.
  GLint viewport_x_;
  GLint viewport_y_;
  GLsizei viewport_width_;
  GLsizei viewport_height_;

  // Patch vertices state.
  GLint patch_vertices_;

  // Default tess levels state.
  math::Vector2f default_inner_tess_level_;
  math::Vector4f default_outer_tess_level_;

  // Timer state
  GLuint active_begin_query_;

  // Tiled rendering state.
  bool is_tiling_;

  // Debug state
  std::unique_ptr<DebugMessageState> debug_message_state_;
  struct DebugMessage {
    DebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity,
                 std::string message)
        : source(source),
          type(type),
          id(id),
          severity(severity),
          message(std::move(message)) {}
    const GLenum source;
    const GLenum type;
    const GLuint id;
    const GLenum severity;
    const std::string message;
  };
  GLDEBUGPROC debug_callback_function_;
  const void* debug_callback_user_param_;
  std::list<DebugMessage> debug_message_log_;

  bool allow_invalid_enums_;

  // Mutex for concurrent access.
  std::mutex mutex_;
};

#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init) sname##_ init,

FakeGlContext::ShadowState::ShadowState(int window_width, int window_height) :
#include "ion/gfx/glconstants.inc"
      window_width_(window_width),
      window_height_(window_height),
      object_state_(std::make_shared<ObjectState>()),
      container_state_(new ContainerState),
      max_buffer_size_(0),
      allow_invalid_enums_(false) {
  // Set up default format lists.
  compressed_texture_formats_ = {
      GL_COMPRESSED_RGB_S3TC_DXT1_EXT, GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG,
      GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG,
      GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
      GL_ETC1_RGB8_OES, GL_COMPRESSED_RGB8_ETC2, GL_COMPRESSED_RGBA8_ETC2_EAC,
      GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2
  };
  shader_binary_formats_ = { 0xbadf00d };
  // Initialize default state.
  for (int i = 0; i < 4; ++i)
      blend_color_[i] = 0.0f;
  rgb_blend_equation_ = alpha_blend_equation_ = GL_FUNC_ADD;
  rgb_blend_source_factor_ = alpha_blend_source_factor_ = GL_ONE;
  rgb_blend_destination_factor_ = alpha_blend_destination_factor_ = GL_ZERO;
  for (int i = 0; i < 4; ++i) {
    clear_color_[i] = 0.0f;
    color_write_masks_[i] = true;
  }
  cull_face_mode_ = GL_BACK;
  front_face_mode_ = GL_CCW;
  clear_depth_value_ = 1.0f;
  depth_function_ = GL_LESS;
  depth_range_ = math::Range1f(0.f, 1.f);
  depth_write_mask_ = true;
  error_code_ = GL_NO_ERROR;
  extensions_string_ = kExtensionsString;
  extension_strings_ = base::SplitString(extensions_string_, " ");
  vendor_string_ = "Google";
  renderer_string_ = "Ion fake OpenGL / ES";
  version_string_ = "3.3 Ion OpenGL / ES";
  context_profile_mask_ = GL_CONTEXT_COMPATIBILITY_PROFILE_BIT;
  context_flags_ = 0;
  generate_mipmap_hint_ = GL_DONT_CARE;
  line_width_ = 1.f;
  pack_alignment_ = unpack_alignment_ = 4;
  point_size_ = 1.f;
  polygon_offset_factor_ = polygon_offset_units_ = 0.0f;
  sample_coverage_value_ = 1.0f;
  sample_coverage_inverted_ = false;
  scissor_x_ = scissor_y_ = 0;
  scissor_width_ = window_width_;
  scissor_height_ = window_height_;
  front_stencil_function_ = back_stencil_function_ = GL_ALWAYS;
  front_stencil_reference_value_ = back_stencil_reference_value_ = 0;
  front_stencil_mask_ = back_stencil_mask_ = static_cast<GLuint>(-1);
  front_stencil_fail_op_ = front_stencil_depth_fail_op_ =
      front_stencil_pass_op_ = back_stencil_fail_op_ =
          back_stencil_depth_fail_op_ = back_stencil_pass_op_ = GL_KEEP;
  clear_stencil_value_ = 0;
  front_stencil_write_mask_ = back_stencil_write_mask_ =
      static_cast<GLuint>(-1);
  viewport_x_ = viewport_y_ = 0;
  viewport_width_ = window_width_;
  viewport_height_ = window_height_;
  active_begin_query_ = 0;
  is_tiling_ = false;
  debug_message_state_.reset(new DebugMessageState());
  debug_callback_function_ = nullptr;
  debug_callback_user_param_ = nullptr;

  // Default global objects.
  object_state_->buffers[0] = BufferObject();
  object_state_->renderbuffers[0] = RenderbufferObject();
  object_state_->textures[0] = TextureObject();
  object_state_->timers[0] = TimerObject();
  container_state_->arrays[0] = ArrayObject();
  container_state_->framebuffers[0] = FramebufferObject();
  container_state_->transform_feedbacks[0] = TransformFeedbackObject();

  // All capabilities except GL_DITHER are disabled by default.
  enabled_state_.resize(kNumStaticCapabilities, false);
  enabled_state_[GetCapabilityIndex(GL_DITHER)] = true;
  enabled_state_[GetCapabilityIndex(GL_MULTISAMPLE)] = true;

  // Default is GL_FRONT for single-buffered contexts.
  container_state_->framebuffers[0].draw_buffers[0] = GL_BACK;
  container_state_->framebuffers[0].read_buffer = GL_BACK;

  ResizeInternalState();
}

// Adjust internal fields to match implementation limits.
void FakeGlContext::ShadowState::ResizeInternalState() {
  enabled_state_.resize(kNumStaticCapabilities + max_clip_distances_, false);
  container_state_->framebuffers[0].draw_buffers.resize(max_draw_buffers_,
                                                        GL_NONE);
  container_state_->arrays[0].attributes.resize(max_vertex_attribs_);
  image_units_.resize(max_texture_image_units_);
  sample_masks_.resize(max_sample_mask_words_);
  active_objects_.transform_feedback_buffers.resize(
      max_transform_feedback_separate_attribs_);
  active_objects_.uniform_buffers.resize(max_uniform_buffer_bindings_);
}

FakeGlContext::ShadowState::ShadowState(ShadowState* parent_state)
    : ShadowState(parent_state->window_width_, parent_state->window_height_) {
  object_state_ = parent_state->object_state_;
}

FakeGlContext::ShadowState::~ShadowState() {}

// Generic Getv function that works with any supported type where ConvertValue
// functions exist.
template <typename T>
void FakeGlContext::ShadowState::Getv(GLenum pname, T* params) {
  // Take care of capabilities first.
  const int index = GetCapabilityIndex(pname);
  if (index >= 0 && index < static_cast<int>(enabled_state_.size())) {
    *params = enabled_state_[index];
    return;
  }

#define ION_SET_INDEX(i, val) ConvertValue(params + i, val)
#define ION_SET(val)     \
  ION_SET_INDEX(0, val); \
  break

#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init)   \
  case gl_enum:                                               \
    ConvertValue(params, sname##_); break;
#define ION_WRAP_GL_LIST(name, sname, gl_enum, gl_count_enum) \
  case gl_count_enum:                                         \
    ConvertValue(params, sname##_.size()); break;             \
  case gl_enum:                                               \
    ConvertValue(params, sname##_); break;

  switch (pname) {
#include "ion/gfx/glconstants.inc"
    case GL_ACTIVE_TEXTURE:
      ION_SET(active_objects_.image_unit + GL_TEXTURE0);
    case GL_POINT_SIZE_RANGE:
      ION_SET_INDEX(0, aliased_point_size_range_.GetMinPoint()[0]);
      ION_SET_INDEX(1, aliased_point_size_range_.GetMaxPoint()[0]);
      break;
    case GL_ALPHA_BITS:
      if (active_objects_.draw_framebuffer == 0U) {
        ION_SET(8);
      } else {
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.draw_framebuffer];
        ION_SET(object_state_->renderbuffers[f.color[0].value].alpha_size);
      }
    case GL_ARRAY_BUFFER_BINDING:
      ION_SET(active_objects_.array_buffer);
    case GL_BLEND_COLOR:
      for (int i = 0; i < 4; ++i) {
        ION_SET_INDEX(i, blend_color_[i]);
      }
      break;
    case GL_BLEND_DST_ALPHA:
      ION_SET(alpha_blend_destination_factor_);
    case GL_BLEND_DST_RGB:
      ION_SET(rgb_blend_destination_factor_);
    case GL_BLEND_EQUATION_ALPHA:
      ION_SET(alpha_blend_equation_);
    case GL_BLEND_EQUATION_RGB:
      ION_SET(rgb_blend_equation_);
    case GL_BLEND_SRC_ALPHA:
      ION_SET(alpha_blend_source_factor_);
    case GL_BLEND_SRC_RGB:
      ION_SET(rgb_blend_source_factor_);
    case GL_BLUE_BITS:
      if (active_objects_.draw_framebuffer == 0U) {
        ION_SET(8);
      } else {
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.draw_framebuffer];
        ION_SET(object_state_->renderbuffers[f.color[0].value].blue_size);
      }
    case GL_COLOR_CLEAR_VALUE:
      for (int i = 0; i < 4; ++i) {
        ION_SET_INDEX(i, clear_color_[i]);
      }
      break;
    case GL_COLOR_WRITEMASK:
      for (int i = 0; i < 4; ++i) {
        ION_SET_INDEX(i, color_write_masks_[i]);
      }
      break;
    case GL_CONTEXT_FLAGS:
      ION_SET(context_flags_);
    case GL_CONTEXT_PROFILE_MASK:
      ION_SET(context_profile_mask_);
    case GL_CULL_FACE_MODE:
      ION_SET(cull_face_mode_);
    case GL_CURRENT_PROGRAM:
      ION_SET(active_objects_.program);
    case GL_DEBUG_LOGGED_MESSAGES:
      ION_SET(static_cast<GLint>(debug_message_log_.size()));
    case GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
      if (debug_message_log_.empty()) {
        ION_SET(0);
      } else {
        ION_SET(
            static_cast<GLint>(debug_message_log_.front().message.size() + 1));
      }
    case GL_DEPTH_CLEAR_VALUE:
      ION_SET(clear_depth_value_);
    case GL_DEPTH_FUNC:
      ION_SET(depth_function_);
    case GL_DEPTH_BITS:
      if (active_objects_.draw_framebuffer == 0U) {
        ION_SET(16);
      } else {
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.draw_framebuffer];
        ION_SET(object_state_->renderbuffers[f.depth.value].depth_size);
      }
    case GL_DEPTH_RANGE:
      ConvertValue(params, depth_range_);
      break;
    case GL_DEPTH_WRITEMASK:
      ION_SET(depth_write_mask_);
    case GL_GPU_DISJOINT_EXT:
      ION_SET(0);
    case GL_DRAW_BUFFER:
      {
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.draw_framebuffer];
        ION_SET(f.draw_buffers[0]);
      }
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      ION_SET(active_objects_.element_array_buffer);
    case GL_FRAMEBUFFER_BINDING:
    // case GL_DRAW_FRAMEBUFFER_BINDING same value as GL_FRAMEBUFFER_BINDING
      ION_SET(active_objects_.draw_framebuffer);
    case GL_READ_FRAMEBUFFER_BINDING:
      ION_SET(active_objects_.read_framebuffer);
    case GL_FRONT_FACE:
      ION_SET(front_face_mode_);
    case GL_GENERATE_MIPMAP_HINT:
      ION_SET(generate_mipmap_hint_);
    case GL_GREEN_BITS:
      if (active_objects_.draw_framebuffer == 0U) {
        ION_SET(8);
      } else {
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.draw_framebuffer];
        ION_SET(object_state_->renderbuffers[f.color[0].value].green_size);
      }
    // 
    // queries.
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
      ION_SET(GL_RGBA);
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      ION_SET(GL_UNSIGNED_BYTE);
    case GL_LINE_WIDTH:
      ION_SET(line_width_);
    case GL_MIN_SAMPLE_SHADING_VALUE:
      ION_SET(min_sample_shading_);
    case GL_MULTISAMPLE:
      ION_SET(IsEnabled(GL_MULTISAMPLE));
    case GL_NUM_EXTENSIONS:
      ION_SET(static_cast<GLint>(extension_strings_.size()));
    case GL_PACK_ALIGNMENT:
      ION_SET(pack_alignment_);
    case GL_POINT_SIZE:
      ION_SET(point_size_);
    case GL_POLYGON_OFFSET_FACTOR:
      ION_SET(polygon_offset_factor_);
    case GL_POLYGON_OFFSET_UNITS:
      ION_SET(polygon_offset_units_);
    case GL_READ_BUFFER:
      {
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.read_framebuffer];
        ION_SET(f.read_buffer);
      }
    case GL_RED_BITS:
      if (active_objects_.draw_framebuffer == 0U) {
        ION_SET(8);
      } else {
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.draw_framebuffer];
        ION_SET(object_state_->renderbuffers[f.color[0].value].red_size);
      }
    case GL_RENDERBUFFER_BINDING:
      ION_SET(active_objects_.renderbuffer);
    case GL_SAMPLE_BUFFERS:
    case GL_SAMPLES:
      ION_SET(1);
    case GL_SAMPLE_COVERAGE_INVERT:
      ION_SET(sample_coverage_inverted_);
    case GL_SAMPLE_COVERAGE_VALUE:
      ION_SET(sample_coverage_value_);
    case GL_SAMPLE_MASK_VALUE:
      for (GLuint i = 0; i < max_sample_mask_words_; ++i) {
        ION_SET_INDEX(i, sample_masks_[i]);
      }
      break;
    case GL_SAMPLE_SHADING:
      ION_SET(IsEnabled(GL_SAMPLE_SHADING));
    case GL_SAMPLER_BINDING:
      ION_SET(image_units_[active_objects_.image_unit].sampler);
    case GL_SCISSOR_BOX:
      ION_SET_INDEX(0, scissor_x_);
      ION_SET_INDEX(1, scissor_y_);
      ION_SET_INDEX(2, scissor_width_);
      ION_SET_INDEX(3, scissor_height_);
      break;
    case GL_STENCIL_BACK_FAIL:
      ION_SET(back_stencil_fail_op_);
    case GL_STENCIL_BACK_FUNC:
      ION_SET(back_stencil_function_);
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
      ION_SET(back_stencil_depth_fail_op_);
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
      ION_SET(back_stencil_pass_op_);
    case GL_STENCIL_BACK_REF:
      ION_SET(back_stencil_reference_value_);
    case GL_STENCIL_BACK_VALUE_MASK:
      ION_SET(back_stencil_mask_);
    case GL_STENCIL_BACK_WRITEMASK:
      ION_SET(back_stencil_write_mask_);
    case GL_STENCIL_BITS:
      if (active_objects_.draw_framebuffer == 0U) {
        ION_SET(8);
      } else {
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.draw_framebuffer];
        ION_SET(object_state_->renderbuffers[f.stencil.value].stencil_size);
      }
    case GL_STENCIL_CLEAR_VALUE:
      ION_SET(clear_stencil_value_);
    case GL_STENCIL_FAIL:
      ION_SET(front_stencil_fail_op_);
    case GL_STENCIL_FUNC:
      ION_SET(front_stencil_function_);
    case GL_STENCIL_PASS_DEPTH_FAIL:
      ION_SET(front_stencil_depth_fail_op_);
    case GL_STENCIL_PASS_DEPTH_PASS:
      ION_SET(front_stencil_pass_op_);
    case GL_STENCIL_REF:
      ION_SET(front_stencil_reference_value_);
    case GL_STENCIL_VALUE_MASK:
      ION_SET(front_stencil_mask_);
    case GL_STENCIL_WRITEMASK:
      ION_SET(front_stencil_write_mask_);
    case GL_SUBPIXEL_BITS:
      ION_SET(4);
    case GL_TEXTURE_BINDING_1D_ARRAY:
      ION_SET(image_units_[active_objects_.image_unit].texture_1d_array);
    case GL_TEXTURE_BINDING_2D:
      ION_SET(image_units_[active_objects_.image_unit].texture_2d);
    case GL_TEXTURE_BINDING_2D_ARRAY:
      ION_SET(image_units_[active_objects_.image_unit].texture_2d_array);
    case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
      ION_SET(image_units_[active_objects_.image_unit].texture_2d_multisample);
    case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY:
      ION_SET(image_units_[active_objects_.image_unit]
                           .texture_2d_multisample_array);
    case GL_TEXTURE_BINDING_3D:
      ION_SET(image_units_[active_objects_.image_unit].texture_3d);
    case GL_TEXTURE_BINDING_CUBE_MAP:
      ION_SET(image_units_[active_objects_.image_unit].cubemap);
    case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY:
      ION_SET(image_units_[active_objects_.image_unit].cubemap_array);
    case GL_TEXTURE_BINDING_EXTERNAL_OES:
      ION_SET(image_units_[active_objects_.image_unit].texture_external);
    case GL_TIMESTAMP_EXT:
      // For testing we use fixed timestamps to avoid clock issues.
      ION_SET(static_cast<T>(1));
    case GL_TRANSFORM_FEEDBACK_PAUSED: {
      GLuint tfid = active_objects_.transform_feedback;
      if (tfid == 0) {
        ION_SET(GL_FALSE);
        break;
      }
      TransformFeedbackObject& tf = container_state_->transform_feedbacks[tfid];
      ION_SET(tf.paused);
    }
    case GL_TRANSFORM_FEEDBACK_ACTIVE: {
      GLuint tfid = active_objects_.transform_feedback;
      if (tfid == 0) {
        ION_SET(GL_FALSE);
        break;
      }
      TransformFeedbackObject& tf = container_state_->transform_feedbacks[tfid];
      ION_SET(tf.active);
    }
    case GL_UNPACK_ALIGNMENT:
      ION_SET(unpack_alignment_);
    case GL_VERTEX_ARRAY_BINDING:
      ION_SET(active_objects_.vertex_array);
    case GL_VIEWPORT:
      ION_SET_INDEX(0, viewport_x_);
      ION_SET_INDEX(1, viewport_y_);
      ION_SET_INDEX(2, viewport_width_);
      ION_SET_INDEX(3, viewport_height_);
      break;

    default:
      // Handle GL_DRAW_BUFFERi.
      if (pname >= GL_DRAW_BUFFER0 &&
          pname < GL_DRAW_BUFFER0 + max_draw_buffers_) {
        int index = pname - GL_DRAW_BUFFER0;
        FramebufferObject& f =
            container_state_->framebuffers[active_objects_.draw_framebuffer];
        ION_SET(f.draw_buffers[index]);
      }
      // Handle GL_CLIP_DISTANCEi.
      if (pname >= GL_CLIP_DISTANCE0 &&
          pname < GL_CLIP_DISTANCE0 + max_clip_distances_) {
        ION_SET(IsEnabled(pname));
      }
      // The rest are unhandled for now.
      // GL_INVALID_ENUM is generated if pname is not an accepted value.
      CheckGlEnum(false);
      break;
  }

#undef ION_SET
#undef ION_SET_INDEX
}

int FakeGlContext::ShadowState::GetCapabilityIndex(GLenum cap) {
  switch (cap) {
    case GL_BLEND:
      return 0;
    case GL_CULL_FACE:
      return 1;
    case GL_DEPTH_TEST:
      return 2;
    case GL_DITHER:
      return 3;
    case GL_MULTISAMPLE:
      return 4;
    case GL_POLYGON_OFFSET_FILL:
      return 5;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      return 6;
    case GL_SAMPLE_COVERAGE:
      return 7;
    case GL_SCISSOR_TEST:
      return 8;
    case GL_SHADER_COMPILER:
      return 9;
    case GL_STENCIL_TEST:
      return 10;
    case GL_DEBUG_OUTPUT_SYNCHRONOUS:
      return 11;

    // Extensions.
    case GL_POINT_SPRITE:
      return 12;
    case GL_PROGRAM_POINT_SIZE:
      return 13;
    case GL_RASTERIZER_DISCARD:
      return 14;
    case GL_SAMPLE_SHADING:
      return 15;
    default:
      if (cap >= GL_CLIP_DISTANCE0 &&
          cap < GL_CLIP_DISTANCE0 + max_clip_distances_) {
        return static_cast<int>(
            kNumStaticCapabilities + cap - GL_CLIP_DISTANCE0);
      }
      return -1;
  }
}

namespace {

//---------------------------------------------------------------------------
// Each of these static functions is used to invoke the corresponding
// non-static member function on the thread local instance's shadow state.
// These are used as the entry points for the FakeGraphicsManager.
#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  return_type ION_APIENTRY Wrapped##name typed_args {                       \
    FakeGlContext::ShadowState* shadow_state =                              \
        FakeGlContext::IncrementAndCall(#name);                             \
    std::lock_guard<std::mutex> lock(*shadow_state->GetMutex());            \
    return shadow_state->name args;                                         \
  }

#include "ion/gfx/glfunctions.inc"

#undef ION_WRAP_GL_FUNC

}  // namespace

//-----------------------------------------------------------------------------
//
// FakeGlContext class functions.
//
//-----------------------------------------------------------------------------

FakeGlContext::FakeGlContext(std::unique_ptr<ShadowState> shadow_state,
                             bool is_valid)
    : shadow_state_(std::move(shadow_state)),
      call_count_(0),
      is_valid_(is_valid) {}

base::SharedPtr<FakeGlContext> FakeGlContext::CreateShared(
    const FakeGlContext& share_context) {
  base::SharedPtr<FakeGlContext> fake_context(new FakeGlContext(
      absl::make_unique<ShadowState>(share_context.shadow_state_.get()),
      share_context.is_valid_));
  fake_context->SetIds(CreateId(), share_context.GetShareGroupId(),
                       reinterpret_cast<uintptr_t>(fake_context.Get()));
  return fake_context;
}

// static
base::SharedPtr<FakeGlContext> FakeGlContext::Create(int window_width,
                                                     int window_height) {
  base::SharedPtr<FakeGlContext> fake_context(new FakeGlContext(
      absl::make_unique<ShadowState>(window_width, window_height),
      true));
  fake_context->SetIds(CreateId(), CreateShareGroupId(),
                       reinterpret_cast<uintptr_t>(fake_context.Get()));
  return fake_context;
}

FakeGlContext::~FakeGlContext() {
  const GLenum error_code = GetErrorCode();
  if (error_code != GL_NO_ERROR) {
    LOG(WARNING) << "FakeGlContext destroyed with uncaught OpenGL error: "
                 << GraphicsManager::ErrorString(error_code);
  }
}

void* FakeGlContext::GetProcAddress(const char* proc_name,
                                    uint32_t flags) const {
  using std::sort;
  using std::strcmp;

  // Create mapping of names to functions in a hash map.
  struct StringsEqual {
    bool operator()(const char* const& lhs, const char* const& rhs) const {
      return lhs == rhs || (lhs && rhs && !strcmp(lhs, rhs));
    }
  };
  struct StringHash {
    size_t operator()(const char* const& s) const {
      return std::hash<std::string>{}(std::string(s));
    }
  };

  using FunctionMap =
      std::unordered_map<const char*, void*, StringHash, StringsEqual>;

  static const FunctionMap kMockFunctions = []() {
#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  {"gl" #name, reinterpret_cast<void*>(&Wrapped##name)},
      FunctionMap mock_functions = {
#include "ion/gfx/glfunctions.inc"  // NOLINT
    };
#undef ION_WRAP_GL_FUNC
    return mock_functions;
  }();

  // Search for |proc_name| in the list of functions.
  auto iter = kMockFunctions.find(proc_name);
  return iter == kMockFunctions.end() ? nullptr : iter->second;
}

void FakeGlContext::SwapBuffers() {
  // Nothing to do here.
}

FakeGlContext::ShadowState* FakeGlContext::IncrementAndCall(const char* name) {
  base::SharedPtr<FakeGlContext> current = FakeGlContext::GetCurrent();
  DCHECK(current);
  ShadowState* instance = current->shadow_state_.get();
  DCHECK(instance);
  if (strcmp("GetError", name))
    current->call_count_++;
  return instance;
}

namespace {

// FakeGlContext emulates a platform GL implementation, so it has its own
// concept of a "current context" independent of the currently linked platform
// GL implementation.  This current context is held as a WeakReferentPtr<> as it
// should not hold a reference to the FakeGlContext, just as a platform GL
// implementation does not hold a reference to the GlContext which represents it
// to Ion.
base::WeakReferentPtr<FakeGlContext>* GetCurrentFakeGlContextPtr() {
  ION_DECLARE_SAFE_STATIC_POINTER(
      base::ThreadLocalObject<base::WeakReferentPtr<FakeGlContext>>, s_helper);
  return s_helper->Get();
}

}  // namespace

bool FakeGlContext::MakeContextCurrentImpl() {
  *GetCurrentFakeGlContextPtr() = base::WeakReferentPtr<FakeGlContext>(this);
  return true;
}

void FakeGlContext::ClearCurrentContextImpl() {
  GetCurrentFakeGlContextPtr()->Reset();
}

bool FakeGlContext::IsCurrentGlContext() const {
  return GetCurrentFakeGlContextPtr()->Acquire().Get() == this;
}

base::SharedPtr<FakeGlContext> FakeGlContext::GetCurrent() {
#if ION_NO_RTTI
  return base::SharedPtr<FakeGlContext>(
      static_cast<FakeGlContext*>(GlContext::GetCurrent().Get()));
#else
  return base::DynamicPtrCast<FakeGlContext, portgfx::GlContext>(
      portgfx::GlContext::GetCurrent());
#endif
}

void FakeGlContext::SetMaxBufferSize(GLsizeiptr size_in_bytes) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetMaxBufferSize(size_in_bytes);
}
GLsizeiptr FakeGlContext::GetMaxBufferSize() const {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  return shadow_state_->GetMaxBufferSize();
}

GLenum FakeGlContext::GetErrorCode() const {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  return shadow_state_->GetErrorCode();
}
void FakeGlContext::SetErrorCode(GLenum error_code) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetErrorCode(error_code);
}

void FakeGlContext::SetExtensionsString(const std::string& extensions) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetExtensionsString(extensions);
}

void FakeGlContext::SetVendorString(const std::string& vendor) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetVendorString(vendor);
}

void FakeGlContext::SetRendererString(const std::string& renderer) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetRendererString(renderer);
}

void FakeGlContext::SetVersionString(const std::string& version) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetVersionString(version);
}

void FakeGlContext::SetContextProfileMask(int mask) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetContextProfileMask(mask);
}

void FakeGlContext::SetContextFlags(int value) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetContextFlags(value);
}

void FakeGlContext::SetForceFunctionFailure(const std::string& func_name,
                                            bool always_fails) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->SetForceFunctionFailure(func_name, always_fails);
}

void FakeGlContext::EnableInvalidGlEnumState(bool enable) {
  std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex());
  shadow_state_->EnableInvalidGlEnumState(enable);
}

// Global platform capability values.
#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init)       \
  Type FakeGlContext::Get##name() const {                         \
    std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex()); \
    return shadow_state_->Get##name();                            \
  }                                                               \
  void FakeGlContext::Set##name(Type value) {                     \
    std::lock_guard<std::mutex> lock(*shadow_state_->GetMutex()); \
    shadow_state_->Set##name(std::move(value));                   \
  }
#include "ion/gfx/glconstants.inc"

}  // namespace testing
}  // namespace gfx
}  // namespace ion
