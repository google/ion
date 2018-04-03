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

#ifndef ION_GFX_OPENGLOBJECTS_H_
#define ION_GFX_OPENGLOBJECTS_H_

#include <vector>

#include "ion/base/variant.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace gfx {

// The below structs correspond to OpenGL "objects." For example, a
// BufferObject corresponds to an OpenGL buffer object, and a ProgramObject
// corresponds to an OpenGL program object. The structs are templated on their
// base class so that ancestors can implement specific functionalty, such as
// tracking memory or object id.
//
// The default values for constructors are taken from the manpages of OpenGL
// Get functions.
//
// An ArrayInfo corresponds an OpenGL Vertex Array Object.
template <typename T>
struct ArrayInfo : T {
  struct Attribute {
    Attribute()
        : buffer(0U),
          enabled(GL_FALSE),
          size(4U),
          stride(0U),
          type(GL_FLOAT),
          normalized(GL_FALSE),
          pointer(nullptr),
          value(0.f, 0.f, 0.f, 1.f),
          divisor(0U) {}
    // The OpenGL name of the array buffer when the attribute pointer was set.
    GLuint buffer;
    // Whether the attribute is enabled.
    GLboolean enabled;
    // The number of values in each component of each element of the data array,
    // e.g., a vec3 has size 3.
    GLuint size;
    // The number of bytes between successive elements in the data array.
    GLuint stride;
    // The type of the attribute values in the data array. Possible values are
    // GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_FIXED, and
    // GL_FLOAT.
    GLenum type;
    // Whether the attribute data will be normalized when sent to OpenGL.
    GLboolean normalized;
    // The value of the attribute pointer.
    GLvoid* pointer;
    // The value of float and vec[2-4] attributes.
    math::Vector4f value;
    // The rate at which new attributes are presented to the vertex shader.
    // A divisor value of 0 turns off the instancing for a specified attribute.
    // If divisor is two, the value of the attribute is updated every second
    // instance.
    GLuint divisor;
  };
  // The attribute index of an attribute is its index in the vector.
  std::vector<Attribute> attributes;
};

// A BufferInfo corresponds to an OpenGL Buffer Object.
template <typename T>
struct BufferInfo : T {
  BufferInfo()
      : size(-1),
        usage(0),
        mapped_data(nullptr) {}
  // The number of bytes of buffer data.
  GLsizeiptr size;
  // The usage pattern, one of GL_STREAM_DRAW, GL_STATIC_DRAW, or
  // GL_DYNAMIC_DRAW.
  GLenum usage;
  // The data pointer of the buffer if it is mapped, or nullptr.
  GLvoid* mapped_data;
};

// A BufferInfo corresponds to an OpenGL Framebuffer Object.
template <typename T>
struct FramebufferInfo : T {
  struct Attachment {
    Attachment()
        : type(GL_NONE),
          value(0),
          level(0),
          cube_face(0),
          layer(0),
          texture_samples(0),
          num_views(0) {}
    // The type of the attachment, one of GL_RENDERBUFFER, GL_TEXTURE, or if no
    // image is attached, GL_NONE.
    GLenum type;
    // The id of either the renderbuffer or texture bound to the attachment, or
    // 0 if neither is bound.
    GLuint value;
    // The mipmap level of the texture object if one is attached.
    GLuint level;
    // The cube map face of the texture if the attachment is a cube map texture
    // object.
    GLenum cube_face;
    // Target texture layer of a layer attachment. For multiview attachments,
    // this holds the base view index.
    GLint layer;
    // Number of samples for implicit texture multisampling (for the extension
    // EXT_multisampled_render_to_texture).
    GLsizei texture_samples;
    // Number of views (for multiview extension).
    GLsizei num_views;

    bool operator==(const Attachment& other) const {
      return type == other.type && value == other.value &&
          level == other.level && cube_face == other.cube_face &&
          layer == other.layer && texture_samples == other.texture_samples &&
          num_views == other.num_views;
    }
    bool operator!=(const Attachment& other) const {
      return !(*this == other);
    }
  };
  FramebufferInfo()
      : color(1),
        draw_buffers(1, GL_NONE),
        read_buffer(GL_NONE) {}
  // Attachments.
  std::vector<Attachment> color;
  Attachment depth;
  Attachment stencil;
  std::vector<GLenum> draw_buffers;
  GLenum read_buffer;
};

// A ProgramInfo corresponds to an OpenGL Program Object.
template <typename T>
struct ProgramInfo : T {
  // An attribute to a vertex shader.
  struct Attribute {
    Attribute() : index(0), type(GL_NONE), size(0U) {}
    // The attribute index.
    GLint index;
    // The attribute's array locations. Scalar attributes (e.g., vec2 attr;)
    // have size == 1, and no array indices, while array attributes (e.g., vec2
    // attr[2];) have indices for each element (including 0) of the array.
    std::vector<GLint> array_indices;
    // The type of the attribute, one of GL_FLOAT, GL_FLOAT_VEC2, GL_FLOAT_VEC3,
    // GL_FLOAT_VEC4, GL_FLOAT_MAT2, GL_FLOAT_MAT3, or GL_FLOAT_MAT4.
    GLenum type;
    // The number of components in the attribute.
    GLint size;
    // The name of the attribute in the program.
    std::string name;
  };
  // A uniform variable to a shader stage.
  struct Uniform {
    typedef base::Variant<
        int, uint32, float,
        math::VectorBase2f, math::VectorBase3f, math::VectorBase4f,
        math::VectorBase2i, math::VectorBase3i, math::VectorBase4i,
        math::VectorBase2ui, math::VectorBase3ui, math::VectorBase4ui,
        math::Matrix2f, math::Matrix3f, math::Matrix4f> ValueType;
    Uniform() : index(0U), type(GL_FLOAT), size(0U) {}
    // The uniform's location.
    GLint index;
    // The uniform array locations. Scalar uniforms (e.g., vec2 uni;) have size
    // == 1, and no array indices, while array uniforms (e.g., vec2 uni[2];)
    // have indices for each element (including 0) of the array.
    std::vector<GLint> array_indices;
    // The type of the uniform.
    GLenum type;
    // The number of elements in the uniform. A non-array uniform has size 1.
    GLint size;
    // The value of the uniform.
    ValueType value;
    // The name of the uniform in the program.
    std::string name;
  };
  // A varying to a fragment shader.
  struct Varying {
    Varying() : index(0), type(GL_NONE), size(0U) {}
    // The varying index.
    GLint index;
    // The varying's array locations. Scalar varyings (e.g., vec2 vary;)
    // have size == 1, and no array indices, while array varyings (e.g., vec2
    // vary[2];) have indices for each element (including 0) of the array.
    std::vector<GLint> array_indices;
    // The type of the varying, one of GL_FLOAT, GL_FLOAT_VEC2, GL_FLOAT_VEC3,
    // GL_FLOAT_VEC4, GL_FLOAT_MAT2, GL_FLOAT_MAT3, or GL_FLOAT_MAT4.
    GLenum type;
    // The number of components in the varying.
    GLint size;
    // The name of the varying in the program.
    std::string name;
  };

  // The OpenGL id of the vertex shader of the program.
  GLuint vertex_shader = 0U;
  // The OpenGL id of the tessellation control shader of the program.
  GLuint tess_ctrl_shader = 0U;
  // The OpenGL id of the tessellation evaluation shader of the program.
  GLuint tess_eval_shader = 0U;
  // The OpenGL id of the geometry shader of the program.
  GLuint geometry_shader = 0U;

  // The OpenGL id of the fragment shader of the program.
  GLuint fragment_shader = 0U;
  // The attributes, uniforms and varyings used in the program.
  std::vector<Attribute> attributes;
  std::vector<Uniform> uniforms;
  std::vector<Varying> varyings;
  // The state set by glTransformFeedbackVaryings.
  std::vector<std::string> requested_tf_varyings;
  GLenum transform_feedback_mode = GL_NONE;
  // The delete, link, and validate status.
  GLboolean delete_status = GL_FALSE;
  GLboolean link_status = GL_FALSE;
  GLboolean validate_status = GL_FALSE;
  // The latest info log of the program.
  std::string info_log;
};

// A RenderbufferInfo corresponds to an OpenGL Renderbuffer Object.
template <typename T>
struct RenderbufferInfo : T {
  RenderbufferInfo()
      : width(0),
        height(0),
        internal_format(GL_RGBA4),
        red_size(0),
        green_size(0),
        blue_size(0),
        alpha_size(0),
        depth_size(0),
        stencil_size(0),
        multisample_samples(0) {}
  // The dimensions of the renderbuffer.
  GLsizei width;
  GLsizei height;
  // The internal format of the renderbuffer.
  GLenum internal_format;
  // The size in bits of each channel in the renderbuffer's data stores.
  GLsizei red_size;
  GLsizei green_size;
  GLsizei blue_size;
  GLsizei alpha_size;
  GLsizei depth_size;
  GLsizei stencil_size;
  GLsizei multisample_samples;
};

// A SamplerInfo corresponds to an OpenGL Sampler Object.
template <typename T>
struct SamplerInfo : T {
  SamplerInfo()
      : compare_func(GL_LESS),
        compare_mode(GL_NONE),
        max_anisotropy(1.f),
        min_lod(-1000.f),
        max_lod(1000.f),
        min_filter(GL_NEAREST_MIPMAP_LINEAR),
        mag_filter(GL_LINEAR),
        wrap_r(GL_REPEAT),
        wrap_s(GL_REPEAT),
        wrap_t(GL_REPEAT) {}
  // The comparison function and mode of the sampler.
  GLenum compare_func;
  GLenum compare_mode;
  // The max anisotropy of the sampler.
  GLfloat max_anisotropy;
  // The min and max LOD of the sampler.
  GLfloat min_lod;
  GLfloat max_lod;
  // The filter modes of the sampler.
  GLenum min_filter;
  GLenum mag_filter;
  // The wrap modes of the sampler.
  GLenum wrap_r;
  GLenum wrap_s;
  GLenum wrap_t;
};

// A ShaderInfo corresponds to an OpenGL Shader Object.
template <typename T>
struct ShaderInfo : T {
  ShaderInfo()
      : type(static_cast<GLenum>(-1)),
        delete_status(GL_FALSE),
        compile_status(GL_FALSE) {}
  // The shader type, either GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
  GLenum type;
  // The status of the shader.
  GLboolean delete_status;
  GLboolean compile_status;
  // The source of the shader as a string.
  std::string source;
  // The latest info log of the shader.
  std::string info_log;
};

// A SyncInfo corresponds to an OpenGL Sync Object.
template <typename T>
struct SyncInfo : T {
  SyncInfo()
      : type(static_cast<GLenum>(-1)),
        status(static_cast<GLenum>(-1)),
        condition(static_cast<GLenum>(-1)),
        flags(0) {}
  // The sync object type.
  GLenum type;
  // The sync object status.
  GLenum status;
  // The sync object condition.
  GLenum condition;
  // The sync object flags.
  GLbitfield flags;
};

// A TransformFeedbackInfo corresponds to an OpenGL TransformFeedback Object.
template <typename T>
struct TransformFeedbackInfo : T {
  // Each attribute stream specifies where a varying gets recorded.
  // Multiple streams are useful only when SEPARATE_ATTRIBS mode is enabled.
  struct AttributeStream {
    GLuint buffer;
    GLintptr start;
    GLsizeiptr size;
  };
  std::vector<AttributeStream> streams;
  // The vertex buffer that records interleaved varyings while transform
  // feedback is active.
  GLuint buffer = 0;
  // This is true only when the user explicity pauses the transform feedback
  // object (Ion does not support this feature yet).
  GLboolean paused = GL_FALSE;
  // This is true only when a transform feedback object is bound and actively
  // recording varyings.
  GLboolean active = GL_FALSE;
};

// A TextureInfo corresponds to an OpenGL Texture Object.
template <typename T>
struct TextureInfo : T {
  TextureInfo()
      : base_level(0),
        max_level(1000),
        compare_func(GL_LESS),
        compare_mode(GL_NONE),
        max_anisotropy(1.f),
        min_lod(-1000.f),
        max_lod(1000.f),
        min_filter(GL_NEAREST_MIPMAP_LINEAR),
        mag_filter(GL_LINEAR),
        is_protected(GL_FALSE),
        samples(0),
        fixed_sample_locations(true),
        swizzle_r(GL_RED),
        swizzle_g(GL_GREEN),
        swizzle_b(GL_BLUE),
        swizzle_a(GL_ALPHA),
        wrap_r(GL_REPEAT),
        wrap_s(GL_REPEAT),
        wrap_t(GL_REPEAT),
        target(static_cast<GLenum>(-1)),
        foveated_bits(0),
        foveated_min_pixel_density(0.0f) {}
  GLint base_level;
  GLint max_level;
  // The comparison function and mode of the texture.
  GLenum compare_func;
  GLenum compare_mode;
  // The max anisotropy of the texture.
  GLfloat max_anisotropy;
  // The min and max LOD of the texture.
  GLfloat min_lod;
  GLfloat max_lod;
  // The filter modes of the texture.
  GLenum min_filter;
  GLenum mag_filter;
  // Whether the texture is protected.
  GLboolean is_protected;
  // Texture samples.
  GLuint samples;
  GLboolean fixed_sample_locations;
  // The swizzle modes of the texture.
  GLenum swizzle_r;
  GLenum swizzle_g;
  GLenum swizzle_b;
  GLenum swizzle_a;
  // The wrap modes of the texture.
  GLenum wrap_r;
  GLenum wrap_s;
  GLenum wrap_t;
  // The texture target.
  GLenum target;
  // The Qualcomm hardware foveation parameter.
  GLint foveated_bits;
  // Minimum ratio of computed pixels over displayed pixels (downsampling).
  GLfloat foveated_min_pixel_density;
};

// A TimerInfo corresponds to an OpenGL Timer Query Object.
template <typename T>
struct TimerInfo : T {
  TimerInfo()
      : mode(kNone),
        timestamp(0),
        duration(0),
        deleted(false),
        is_data_available(false) {}
  enum Mode {
    // Unused, so no known mode yet.
    kNone,
    // Is used as a query counter.
    kIsQueryCounter,
    // Is active, in use for begin/end query.
    kIsBeginEndQuery,
  };
  // The usage mode of this timer query.
  Mode mode;
  // Timestamp data, if used as a query counter or begin query.
  uint64 timestamp;
  // Duration data, if used as a begin/end query pair.
  uint64 duration;
  // Was deleted.
  bool deleted;
  // Is timestamp or duration available.
  bool is_data_available;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_OPENGLOBJECTS_H_
