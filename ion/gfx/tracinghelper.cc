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

#include "ion/gfx/tracinghelper.h"

#include <string.h>  // For strcmp().

#include <sstream>

#include "ion/base/stringutils.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace gfx {

#if ION_PRODUCTION

TracingHelper::TracingHelper() {}
template <typename T>
const std::string TracingHelper::ToString(const char*, T) {
  return std::string();
}

// Specialize for all types.
template ION_API const std::string TracingHelper::ToString(
    const char*, char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, char**);
template ION_API const std::string TracingHelper::ToString(
    const char*, const char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const char**);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned char);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned int);
template ION_API const std::string TracingHelper::ToString(
    const char*, const float*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const unsigned char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const unsigned int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const void*);
template ION_API const std::string TracingHelper::ToString(
    const char*, float);
template ION_API const std::string TracingHelper::ToString(
    const char*, float*);
template ION_API const std::string TracingHelper::ToString(
    const char*, int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, void*);
template ION_API const std::string TracingHelper::ToString(
    const char*, void**);
template ION_API const std::string TracingHelper::ToString(
    const char*, GLsync);
template ION_API const std::string TracingHelper::ToString(const char*,
                                                           GLDEBUGPROC);

#else

namespace {

// This helper function converts any type to a string.
template <typename T>
static const std::string AnyToString(const T& val) {
  std::ostringstream out;
  out << val;
  return out.str();
}

// Specialize for unsigned int to use hexadecimal.
template <> const std::string AnyToString(const unsigned int& val) {  // NOLINT
  std::ostringstream out;
  out << "0x" << std::hex << val;
  return out.str();
}

// This is used to convert a GLbitfield used for the glClear() call to a string
// indicating which buffers are being cleared. If anything is found to indicate
// it is a different type of GLbitfield, an empty string is returned.
static const std::string GetClearBitsString(GLbitfield mask) {
  std::string s;
  if (mask & GL_COLOR_BUFFER_BIT) {
    s += "GL_COLOR_BUFFER_BIT";
    mask &= ~GL_COLOR_BUFFER_BIT;
  }
  if (mask & GL_DEPTH_BUFFER_BIT) {
    if (!s.empty())
      s += " | ";
    s += "GL_DEPTH_BUFFER_BIT";
    mask &= ~GL_DEPTH_BUFFER_BIT;
  }
  if (mask & GL_STENCIL_BUFFER_BIT) {
    if (!s.empty())
      s += " | ";
    s += "GL_STENCIL_BUFFER_BIT";
    mask &= ~GL_STENCIL_BUFFER_BIT;
  }
  // If anything is left in the mask, assume it is something else.
  if (mask)
    s.clear();
  return s;
}

// This is used to convert a GLbitfield used for the glMapBufferRange() call to
// a string indicating the access mode for the buffer. If anything is found to
// indicate it is a different type of GLbitfield, an empty string is returned.
static const std::string GetMapBitsString(GLbitfield mode) {
  std::string s;
  if (mode & GL_MAP_READ_BIT) {
    s += "GL_MAP_READ_BIT";
    mode &= ~GL_MAP_READ_BIT;
  }
  if (mode & GL_MAP_WRITE_BIT) {
    if (!s.empty())
      s += " | ";
    s += "GL_MAP_WRITE_BIT";
    mode &= ~GL_MAP_WRITE_BIT;
  }
  // If anything is left in the mode, assume it is something else.
  if (mode)
    s.clear();
  return s;
}

// Helper function to print out values from an array-like type. By default this
// does nothing.
template <typename T>
static std::string ArrayToString(const std::string& type, T arg) {
  return std::string();
}

// A function that actually does the array printing.
template <typename T>
static std::string TypedArrayToString(const std::string& type, T arg) {
  std::ostringstream out;

  // Extract the number of elements from the end of the type.
  if (int count = base::StringToInt32(type.substr(type.length() - 2, 1))) {
    int rows = 1;
    // This assumes square matrices.
    if (type.find("matrix") != std::string::npos)
      rows = count;
    out << " -> [";
    for (int j = 0; j < rows; ++j) {
      for (int i = 0; i < count; ++i) {
        out << arg[j * count + i];
        if (i < count - 1)
          out << "; ";
      }
      if (j < rows - 1)
        out << " | ";
    }
    out << "]";
  }
  return out.str();
}

// Specializations for int and float pointers to allow array-like types to be
// printed.
template <>
std::string ArrayToString<const int*>(const std::string& type, const int* arg) {
  return TypedArrayToString(type, arg);
}
template <>
std::string ArrayToString<const float*>(const std::string& type,
                                        const float* arg) {
  return TypedArrayToString(type, arg);
}

// NOTE: Some constants are not available on some desktop platforms and are
// commented out in the following IonAddConstants* functions.

#define ION_ADD_CONSTANT(name) (*constants)[name] = #name
static void IonAddConstantsAToF(
    std::unordered_map<int, std::string>* constants) {
  ION_ADD_CONSTANT(GL_ACTIVE_ATTRIBUTES);
  ION_ADD_CONSTANT(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);
  ION_ADD_CONSTANT(GL_ACTIVE_TEXTURE);
  ION_ADD_CONSTANT(GL_ACTIVE_UNIFORMS);
  ION_ADD_CONSTANT(GL_ACTIVE_UNIFORM_MAX_LENGTH);
  ION_ADD_CONSTANT(GL_ALIASED_LINE_WIDTH_RANGE);
  ION_ADD_CONSTANT(GL_ALIASED_POINT_SIZE_RANGE);
  ION_ADD_CONSTANT(GL_ALPHA);
  ION_ADD_CONSTANT(GL_ALPHA_BITS);
  ION_ADD_CONSTANT(GL_ALREADY_SIGNALED);
  ION_ADD_CONSTANT(GL_ALWAYS);
  ION_ADD_CONSTANT(GL_ARRAY_BUFFER);
  ION_ADD_CONSTANT(GL_ARRAY_BUFFER_BINDING);
  ION_ADD_CONSTANT(GL_ATTACHED_SHADERS);
  ION_ADD_CONSTANT(GL_BACK);
  ION_ADD_CONSTANT(GL_BACK_LEFT);
  ION_ADD_CONSTANT(GL_BACK_RIGHT);
  ION_ADD_CONSTANT(GL_BLEND);
  ION_ADD_CONSTANT(GL_BLEND_COLOR);
  ION_ADD_CONSTANT(GL_BLEND_DST_ALPHA);
  ION_ADD_CONSTANT(GL_BLEND_DST_RGB);
  ION_ADD_CONSTANT(GL_BLEND_EQUATION);
  ION_ADD_CONSTANT(GL_BLEND_EQUATION_ALPHA);
  ION_ADD_CONSTANT(GL_BLEND_EQUATION_RGB);
  ION_ADD_CONSTANT(GL_BLEND_SRC_ALPHA);
  ION_ADD_CONSTANT(GL_BLEND_SRC_RGB);
  ION_ADD_CONSTANT(GL_BLUE);
  ION_ADD_CONSTANT(GL_BLUE_BITS);
  ION_ADD_CONSTANT(GL_BOOL);
  ION_ADD_CONSTANT(GL_BOOL_VEC2);
  ION_ADD_CONSTANT(GL_BOOL_VEC3);
  ION_ADD_CONSTANT(GL_BOOL_VEC4);
  ION_ADD_CONSTANT(GL_BUFFER_OBJECT);
  ION_ADD_CONSTANT(GL_BUFFER_SIZE);
  ION_ADD_CONSTANT(GL_BUFFER_USAGE);
  ION_ADD_CONSTANT(GL_BYTE);
  ION_ADD_CONSTANT(GL_CCW);
  ION_ADD_CONSTANT(GL_CLAMP_TO_EDGE);
  ION_ADD_CONSTANT(GL_COLOR_ATTACHMENT0);
  ION_ADD_CONSTANT(GL_COLOR_CLEAR_VALUE);
  ION_ADD_CONSTANT(GL_COLOR_WRITEMASK);
  ION_ADD_CONSTANT(GL_COMPARE_REF_TO_TEXTURE);
  ION_ADD_CONSTANT(GL_COMPILE_STATUS);
  ION_ADD_CONSTANT(GL_COMPRESSED_R11_EAC);
  ION_ADD_CONSTANT(GL_COMPRESSED_RG11_EAC);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGB8_ETC2);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
  ION_ADD_CONSTANT(GL_COMPRESSED_RGBA8_ETC2_EAC);
  ION_ADD_CONSTANT(GL_COMPRESSED_SIGNED_R11_EAC);
  ION_ADD_CONSTANT(GL_COMPRESSED_SIGNED_RG11_EAC);
  ION_ADD_CONSTANT(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
  ION_ADD_CONSTANT(GL_COMPRESSED_SRGB8_ETC2);
  ION_ADD_CONSTANT(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
  ION_ADD_CONSTANT(GL_COMPRESSED_TEXTURE_FORMATS);
  ION_ADD_CONSTANT(GL_CONDITION_SATISFIED);
  ION_ADD_CONSTANT(GL_CONSTANT_ALPHA);
  ION_ADD_CONSTANT(GL_CONSTANT_COLOR);
  ION_ADD_CONSTANT(GL_COPY_READ_BUFFER);
  ION_ADD_CONSTANT(GL_COPY_WRITE_BUFFER);
  ION_ADD_CONSTANT(GL_CULL_FACE);
  ION_ADD_CONSTANT(GL_CULL_FACE_MODE);
  ION_ADD_CONSTANT(GL_CURRENT_PROGRAM);
  ION_ADD_CONSTANT(GL_CURRENT_VERTEX_ATTRIB);
  ION_ADD_CONSTANT(GL_CW);
  ION_ADD_CONSTANT(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  ION_ADD_CONSTANT(GL_DECR);
  ION_ADD_CONSTANT(GL_DECR_WRAP);
  ION_ADD_CONSTANT(GL_DELETE_STATUS);
  ION_ADD_CONSTANT(GL_DEPTH_ATTACHMENT);
  ION_ADD_CONSTANT(GL_DEPTH_BITS);
  ION_ADD_CONSTANT(GL_DEPTH_CLEAR_VALUE);
  ION_ADD_CONSTANT(GL_DEPTH_COMPONENT);
  ION_ADD_CONSTANT(GL_DEPTH_COMPONENT16);
  ION_ADD_CONSTANT(GL_DEPTH_COMPONENT24);
  ION_ADD_CONSTANT(GL_DEPTH_COMPONENT32F);
  ION_ADD_CONSTANT(GL_DEPTH_FUNC);
  ION_ADD_CONSTANT(GL_DEPTH_RANGE);
  ION_ADD_CONSTANT(GL_DEPTH_STENCIL);
  ION_ADD_CONSTANT(GL_DEPTH_TEST);
  ION_ADD_CONSTANT(GL_DEPTH_WRITEMASK);
  ION_ADD_CONSTANT(GL_DEPTH24_STENCIL8);
  ION_ADD_CONSTANT(GL_DEPTH32F_STENCIL8);
  ION_ADD_CONSTANT(GL_DITHER);
  ION_ADD_CONSTANT(GL_DONT_CARE);
  ION_ADD_CONSTANT(GL_DRAW_BUFFER);
  ION_ADD_CONSTANT(GL_DRAW_BUFFER0);
  ION_ADD_CONSTANT(GL_DRAW_FRAMEBUFFER);
  // GL_DRAW_FRAMEBUFFER_BINDING is the same was GL_FRAMEBUFFER_BINDING
  // ION_ADD_CONSTANT(GL_DRAW_FRAMEBUFFER_BINDING);
  ION_ADD_CONSTANT(GL_DST_ALPHA);
  ION_ADD_CONSTANT(GL_DST_COLOR);
  ION_ADD_CONSTANT(GL_DYNAMIC_DRAW);
  ION_ADD_CONSTANT(GL_ELEMENT_ARRAY_BUFFER);
  ION_ADD_CONSTANT(GL_ELEMENT_ARRAY_BUFFER_BINDING);
  ION_ADD_CONSTANT(GL_EQUAL);
  ION_ADD_CONSTANT(GL_ETC1_RGB8_OES);
  ION_ADD_CONSTANT(GL_EXTENSIONS);
  ION_ADD_CONSTANT(GL_FASTEST);
  ION_ADD_CONSTANT(GL_FIXED);
  ION_ADD_CONSTANT(GL_FLOAT);
  ION_ADD_CONSTANT(GL_FLOAT_32_UNSIGNED_INT_24_8_REV);
  ION_ADD_CONSTANT(GL_FLOAT_MAT2);
  ION_ADD_CONSTANT(GL_FLOAT_MAT3);
  ION_ADD_CONSTANT(GL_FLOAT_MAT4);
  ION_ADD_CONSTANT(GL_FLOAT_VEC2);
  ION_ADD_CONSTANT(GL_FLOAT_VEC3);
  ION_ADD_CONSTANT(GL_FLOAT_VEC4);
  ION_ADD_CONSTANT(GL_FRAGMENT_SHADER);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_BINDING);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_COMPLETE);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT);
  ION_ADD_CONSTANT(GL_FRAMEBUFFER_UNSUPPORTED);
  ION_ADD_CONSTANT(GL_FRONT);
  ION_ADD_CONSTANT(GL_FRONT_AND_BACK);
  ION_ADD_CONSTANT(GL_FRONT_FACE);
  ION_ADD_CONSTANT(GL_FRONT_LEFT);
  ION_ADD_CONSTANT(GL_FRONT_RIGHT);
  ION_ADD_CONSTANT(GL_FUNC_ADD);
  ION_ADD_CONSTANT(GL_FUNC_REVERSE_SUBTRACT);
  ION_ADD_CONSTANT(GL_FUNC_SUBTRACT);
}

static void IonAddConstantsGToQ(
    std::unordered_map<int, std::string>* constants) {
  ION_ADD_CONSTANT(GL_GENERATE_MIPMAP_HINT);
  ION_ADD_CONSTANT(GL_GEQUAL);
  ION_ADD_CONSTANT(GL_GREATER);
  ION_ADD_CONSTANT(GL_GREEN);
  ION_ADD_CONSTANT(GL_GREEN_BITS);
  ION_ADD_CONSTANT(GL_HALF_FLOAT);
  ION_ADD_CONSTANT(GL_HIGH_FLOAT);
  ION_ADD_CONSTANT(GL_HIGH_INT);
  ION_ADD_CONSTANT(GL_IMPLEMENTATION_COLOR_READ_FORMAT);
  ION_ADD_CONSTANT(GL_IMPLEMENTATION_COLOR_READ_TYPE);
  ION_ADD_CONSTANT(GL_INCR);
  ION_ADD_CONSTANT(GL_INCR_WRAP);
  ION_ADD_CONSTANT(GL_INFO_LOG_LENGTH);
  ION_ADD_CONSTANT(GL_INT);
  ION_ADD_CONSTANT(GL_INTERLEAVED_ATTRIBS);
  ION_ADD_CONSTANT(GL_INT_SAMPLER_1D);
  ION_ADD_CONSTANT(GL_INT_SAMPLER_1D_ARRAY);
  ION_ADD_CONSTANT(GL_INT_SAMPLER_2D);
  ION_ADD_CONSTANT(GL_INT_SAMPLER_2D_ARRAY);
  ION_ADD_CONSTANT(GL_INT_SAMPLER_3D);
  ION_ADD_CONSTANT(GL_INT_SAMPLER_CUBE);
  ION_ADD_CONSTANT(GL_INT_SAMPLER_CUBE_MAP_ARRAY);
  ION_ADD_CONSTANT(GL_INT_VEC2);
  ION_ADD_CONSTANT(GL_INT_VEC3);
  ION_ADD_CONSTANT(GL_INT_VEC4);
  ION_ADD_CONSTANT(GL_INVALID_ENUM);
  ION_ADD_CONSTANT(GL_INVALID_FRAMEBUFFER_OPERATION);
  ION_ADD_CONSTANT(GL_INVALID_OPERATION);
  ION_ADD_CONSTANT(GL_INVALID_VALUE);
  ION_ADD_CONSTANT(GL_INVERT);
  ION_ADD_CONSTANT(GL_KEEP);
  ION_ADD_CONSTANT(GL_LEFT);
  ION_ADD_CONSTANT(GL_LEQUAL);
  ION_ADD_CONSTANT(GL_LESS);
  ION_ADD_CONSTANT(GL_LINEAR);
  ION_ADD_CONSTANT(GL_LINEAR_MIPMAP_LINEAR);
  ION_ADD_CONSTANT(GL_LINEAR_MIPMAP_NEAREST);
  ION_ADD_CONSTANT(GL_LINES);
  ION_ADD_CONSTANT(GL_LINE_LOOP);
  ION_ADD_CONSTANT(GL_LINE_STRIP);
  ION_ADD_CONSTANT(GL_LINE_WIDTH);
  ION_ADD_CONSTANT(GL_LINK_STATUS);
  ION_ADD_CONSTANT(GL_LOW_FLOAT);
  ION_ADD_CONSTANT(GL_LOW_INT);
  ION_ADD_CONSTANT(GL_LUMINANCE);
  ION_ADD_CONSTANT(GL_LUMINANCE_ALPHA);
  ION_ADD_CONSTANT(GL_MAX_COLOR_ATTACHMENTS);
  ION_ADD_CONSTANT(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
  ION_ADD_CONSTANT(GL_MAX_CUBE_MAP_TEXTURE_SIZE);
  ION_ADD_CONSTANT(GL_MAX_DRAW_BUFFERS);
  ION_ADD_CONSTANT(GL_MAX_FRAGMENT_UNIFORM_VECTORS);
  ION_ADD_CONSTANT(GL_MAX_RENDERBUFFER_SIZE);
  ION_ADD_CONSTANT(GL_MAX_SAMPLE_MASK_WORDS);
  ION_ADD_CONSTANT(GL_MAX_SERVER_WAIT_TIMEOUT);
  ION_ADD_CONSTANT(GL_MAX_TEXTURE_IMAGE_UNITS);
  ION_ADD_CONSTANT(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
  ION_ADD_CONSTANT(GL_MAX_TEXTURE_SIZE);
  ION_ADD_CONSTANT(GL_MAX_TRANSFORM_FEEDBACK_BUFFERS);
  ION_ADD_CONSTANT(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS);
  ION_ADD_CONSTANT(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
  ION_ADD_CONSTANT(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS);
  ION_ADD_CONSTANT(GL_MAX_VARYING_VECTORS);
  ION_ADD_CONSTANT(GL_MAX_VERTEX_ATTRIBS);
  ION_ADD_CONSTANT(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
  ION_ADD_CONSTANT(GL_MAX_VERTEX_UNIFORM_VECTORS);
  ION_ADD_CONSTANT(GL_MAX_VIEWPORT_DIMS);
  ION_ADD_CONSTANT(GL_MEDIUM_FLOAT);
  ION_ADD_CONSTANT(GL_MEDIUM_INT);
  ION_ADD_CONSTANT(GL_MIRRORED_REPEAT);
  ION_ADD_CONSTANT(GL_MULTISAMPLE);
  ION_ADD_CONSTANT(GL_NEAREST);
  ION_ADD_CONSTANT(GL_NEAREST_MIPMAP_LINEAR);
  ION_ADD_CONSTANT(GL_NEAREST_MIPMAP_NEAREST);
  ION_ADD_CONSTANT(GL_NEVER);
  ION_ADD_CONSTANT(GL_NICEST);
  // ION_ADD_CONSTANT(GL_NONE);
  ION_ADD_CONSTANT(GL_NOTEQUAL);
  ION_ADD_CONSTANT(GL_NO_ERROR);
  ION_ADD_CONSTANT(GL_NUM_COMPRESSED_TEXTURE_FORMATS);
  ION_ADD_CONSTANT(GL_NUM_EXTENSIONS);
  ION_ADD_CONSTANT(GL_NUM_SHADER_BINARY_FORMATS);
  ION_ADD_CONSTANT(GL_OBJECT_TYPE);
  // ION_ADD_CONSTANT(GL_ONE);
  ION_ADD_CONSTANT(GL_ONE_MINUS_CONSTANT_ALPHA);
  ION_ADD_CONSTANT(GL_ONE_MINUS_CONSTANT_COLOR);
  ION_ADD_CONSTANT(GL_ONE_MINUS_DST_ALPHA);
  ION_ADD_CONSTANT(GL_ONE_MINUS_DST_COLOR);
  ION_ADD_CONSTANT(GL_ONE_MINUS_SRC_ALPHA);
  ION_ADD_CONSTANT(GL_ONE_MINUS_SRC_COLOR);
  ION_ADD_CONSTANT(GL_OUT_OF_MEMORY);
  ION_ADD_CONSTANT(GL_PACK_ALIGNMENT);
  ION_ADD_CONSTANT(GL_PALETTE4_R5_G6_B5_OES);
  ION_ADD_CONSTANT(GL_PALETTE4_RGB5_A1_OES);
  ION_ADD_CONSTANT(GL_PALETTE4_RGB8_OES);
  ION_ADD_CONSTANT(GL_PALETTE4_RGBA4_OES);
  ION_ADD_CONSTANT(GL_PALETTE4_RGBA8_OES);
  ION_ADD_CONSTANT(GL_PALETTE8_R5_G6_B5_OES);
  ION_ADD_CONSTANT(GL_PALETTE8_RGB5_A1_OES);
  ION_ADD_CONSTANT(GL_PALETTE8_RGB8_OES);
  ION_ADD_CONSTANT(GL_PALETTE8_RGBA4_OES);
  ION_ADD_CONSTANT(GL_PALETTE8_RGBA8_OES);
  ION_ADD_CONSTANT(GL_POINTS);
  ION_ADD_CONSTANT(GL_POINT_SIZE_RANGE);
  ION_ADD_CONSTANT(GL_POINT_SPRITE);
  ION_ADD_CONSTANT(GL_POLYGON_OFFSET_FACTOR);
  ION_ADD_CONSTANT(GL_POLYGON_OFFSET_FILL);
  ION_ADD_CONSTANT(GL_POLYGON_OFFSET_UNITS);
  ION_ADD_CONSTANT(GL_PRIMITIVES_GENERATED);
  ION_ADD_CONSTANT(GL_PROGRAM_OBJECT);
  ION_ADD_CONSTANT(GL_PROGRAM_PIPELINE);
  ION_ADD_CONSTANT(GL_PROGRAM_PIPELINE_OBJECT);
  ION_ADD_CONSTANT(GL_PROGRAM_POINT_SIZE);
  ION_ADD_CONSTANT(GL_QUERY);
  ION_ADD_CONSTANT(GL_QUERY_OBJECT);
}

static void IonAddConstantsRToS(
    std::unordered_map<int, std::string>* constants) {
  ION_ADD_CONSTANT(GL_R11F_G11F_B10F);
  ION_ADD_CONSTANT(GL_R16F);
  ION_ADD_CONSTANT(GL_R16I);
  ION_ADD_CONSTANT(GL_R16UI);
  ION_ADD_CONSTANT(GL_R32F);
  ION_ADD_CONSTANT(GL_R32I);
  ION_ADD_CONSTANT(GL_R32UI);
  ION_ADD_CONSTANT(GL_R8);
  ION_ADD_CONSTANT(GL_R8I);
  ION_ADD_CONSTANT(GL_R8UI);
  ION_ADD_CONSTANT(GL_R8_SNORM);
  ION_ADD_CONSTANT(GL_RASTERIZER_DISCARD);
  ION_ADD_CONSTANT(GL_READ_BUFFER);
  ION_ADD_CONSTANT(GL_READ_FRAMEBUFFER);
  ION_ADD_CONSTANT(GL_READ_FRAMEBUFFER_BINDING);
  ION_ADD_CONSTANT(GL_READ_ONLY);
  ION_ADD_CONSTANT(GL_READ_WRITE);
  ION_ADD_CONSTANT(GL_RED);
  ION_ADD_CONSTANT(GL_RED_BITS);
  ION_ADD_CONSTANT(GL_RED_INTEGER);
  ION_ADD_CONSTANT(GL_RENDERBUFFER);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_ALPHA_SIZE);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_BINDING);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_BLUE_SIZE);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_DEPTH_SIZE);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_GREEN_SIZE);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_HEIGHT);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_INTERNAL_FORMAT);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_RED_SIZE);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_STENCIL_SIZE);
  ION_ADD_CONSTANT(GL_RENDERBUFFER_WIDTH);
  ION_ADD_CONSTANT(GL_RENDERER);
  ION_ADD_CONSTANT(GL_REPEAT);
  ION_ADD_CONSTANT(GL_REPLACE);
  ION_ADD_CONSTANT(GL_RG);
  ION_ADD_CONSTANT(GL_RG16F);
  ION_ADD_CONSTANT(GL_RG16I);
  ION_ADD_CONSTANT(GL_RG16UI);
  ION_ADD_CONSTANT(GL_RG32F);
  ION_ADD_CONSTANT(GL_RG32I);
  ION_ADD_CONSTANT(GL_RG32UI);
  ION_ADD_CONSTANT(GL_RG8);
  ION_ADD_CONSTANT(GL_RG8I);
  ION_ADD_CONSTANT(GL_RG8UI);
  ION_ADD_CONSTANT(GL_RG8_SNORM);
  ION_ADD_CONSTANT(GL_RGB);
  ION_ADD_CONSTANT(GL_RGB10_A2);
  ION_ADD_CONSTANT(GL_RGB10_A2UI);
  ION_ADD_CONSTANT(GL_RGB16F);
  ION_ADD_CONSTANT(GL_RGB16I);
  ION_ADD_CONSTANT(GL_RGB16UI);
  ION_ADD_CONSTANT(GL_RGB32F);
  ION_ADD_CONSTANT(GL_RGB32I);
  ION_ADD_CONSTANT(GL_RGB32UI);
  ION_ADD_CONSTANT(GL_RGB565);
  ION_ADD_CONSTANT(GL_RGB5_A1);
  ION_ADD_CONSTANT(GL_RGB8);
  ION_ADD_CONSTANT(GL_RGB8I);
  ION_ADD_CONSTANT(GL_RGB8UI);
  ION_ADD_CONSTANT(GL_RGB9_E5);
  ION_ADD_CONSTANT(GL_RGBA);
  ION_ADD_CONSTANT(GL_RGBA16F);
  ION_ADD_CONSTANT(GL_RGBA16I);
  ION_ADD_CONSTANT(GL_RGBA16UI);
  ION_ADD_CONSTANT(GL_RGBA32F);
  ION_ADD_CONSTANT(GL_RGBA32I);
  ION_ADD_CONSTANT(GL_RGBA32UI);
  ION_ADD_CONSTANT(GL_RGBA4);
  ION_ADD_CONSTANT(GL_RGBA8);
  ION_ADD_CONSTANT(GL_RGBA8I);
  ION_ADD_CONSTANT(GL_RGBA8UI);
  ION_ADD_CONSTANT(GL_RGBA8_SNORM);
  ION_ADD_CONSTANT(GL_RGBA_INTEGER);
  ION_ADD_CONSTANT(GL_RGB_INTEGER);
  ION_ADD_CONSTANT(GL_RG_INTEGER);
  ION_ADD_CONSTANT(GL_RIGHT);
  ION_ADD_CONSTANT(GL_SAMPLER);
  ION_ADD_CONSTANT(GL_SAMPLER_1D);
  ION_ADD_CONSTANT(GL_SAMPLER_1D_ARRAY);
  ION_ADD_CONSTANT(GL_SAMPLER_1D_ARRAY_SHADOW);
  ION_ADD_CONSTANT(GL_SAMPLER_1D_SHADOW);
  ION_ADD_CONSTANT(GL_SAMPLER_2D);
  ION_ADD_CONSTANT(GL_SAMPLER_2D);
  ION_ADD_CONSTANT(GL_SAMPLER_2D_ARRAY);
  ION_ADD_CONSTANT(GL_SAMPLER_2D_ARRAY_SHADOW);
  ION_ADD_CONSTANT(GL_SAMPLER_2D_MULTISAMPLE);
  ION_ADD_CONSTANT(GL_SAMPLER_2D_MULTISAMPLE_ARRAY);
  ION_ADD_CONSTANT(GL_SAMPLER_2D_SHADOW);
  ION_ADD_CONSTANT(GL_SAMPLER_3D);
  ION_ADD_CONSTANT(GL_SAMPLER_CUBE);
  ION_ADD_CONSTANT(GL_SAMPLER_CUBE);
  ION_ADD_CONSTANT(GL_SAMPLER_CUBE_MAP_ARRAY);
  ION_ADD_CONSTANT(GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW);
  ION_ADD_CONSTANT(GL_SAMPLER_CUBE_SHADOW);
  ION_ADD_CONSTANT(GL_SAMPLER_EXTERNAL_OES);
  ION_ADD_CONSTANT(GL_SAMPLES);
  ION_ADD_CONSTANT(GL_SAMPLE_ALPHA_TO_COVERAGE);
  ION_ADD_CONSTANT(GL_SAMPLE_BUFFERS);
  ION_ADD_CONSTANT(GL_SAMPLE_COVERAGE);
  ION_ADD_CONSTANT(GL_SAMPLE_COVERAGE_INVERT);
  ION_ADD_CONSTANT(GL_SAMPLE_COVERAGE_VALUE);
  ION_ADD_CONSTANT(GL_SAMPLE_POSITION);
  ION_ADD_CONSTANT(GL_SEPARATE_ATTRIBS);
  ION_ADD_CONSTANT(GL_SCISSOR_BOX);
  ION_ADD_CONSTANT(GL_SCISSOR_TEST);
  ION_ADD_CONSTANT(GL_SHADER_BINARY_FORMATS);
  ION_ADD_CONSTANT(GL_SHADER_COMPILER);
  ION_ADD_CONSTANT(GL_SHADER_OBJECT);
  ION_ADD_CONSTANT(GL_SHADER_SOURCE_LENGTH);
  ION_ADD_CONSTANT(GL_SHADER_TYPE);
  ION_ADD_CONSTANT(GL_SHADING_LANGUAGE_VERSION);
  ION_ADD_CONSTANT(GL_SHORT);
  ION_ADD_CONSTANT(GL_SIGNALED);
  ION_ADD_CONSTANT(GL_SRC_ALPHA);
  ION_ADD_CONSTANT(GL_SRC_ALPHA_SATURATE);
  ION_ADD_CONSTANT(GL_SRC_COLOR);
  ION_ADD_CONSTANT(GL_SRGB8);
  ION_ADD_CONSTANT(GL_SRGB8_ALPHA8);
  ION_ADD_CONSTANT(GL_STATIC_DRAW);
  ION_ADD_CONSTANT(GL_STENCIL);
  ION_ADD_CONSTANT(GL_STENCIL_ATTACHMENT);
  ION_ADD_CONSTANT(GL_STENCIL_BACK_FAIL);
  ION_ADD_CONSTANT(GL_STENCIL_BACK_FUNC);
  ION_ADD_CONSTANT(GL_STENCIL_BACK_PASS_DEPTH_FAIL);
  ION_ADD_CONSTANT(GL_STENCIL_BACK_PASS_DEPTH_PASS);
  ION_ADD_CONSTANT(GL_STENCIL_BACK_REF);
  ION_ADD_CONSTANT(GL_STENCIL_BACK_VALUE_MASK);
  ION_ADD_CONSTANT(GL_STENCIL_BACK_WRITEMASK);
  ION_ADD_CONSTANT(GL_STENCIL_BITS);
  ION_ADD_CONSTANT(GL_STENCIL_CLEAR_VALUE);
  ION_ADD_CONSTANT(GL_STENCIL_FAIL);
  ION_ADD_CONSTANT(GL_STENCIL_FUNC);
  ION_ADD_CONSTANT(GL_STENCIL_INDEX8);
  ION_ADD_CONSTANT(GL_STENCIL_PASS_DEPTH_FAIL);
  ION_ADD_CONSTANT(GL_STENCIL_PASS_DEPTH_PASS);
  ION_ADD_CONSTANT(GL_STENCIL_REF);
  ION_ADD_CONSTANT(GL_STENCIL_TEST);
  ION_ADD_CONSTANT(GL_STENCIL_VALUE_MASK);
  ION_ADD_CONSTANT(GL_STENCIL_WRITEMASK);
  ION_ADD_CONSTANT(GL_STREAM_DRAW);
  ION_ADD_CONSTANT(GL_SUBPIXEL_BITS);
  ION_ADD_CONSTANT(GL_SYNC_CONDITION);
  ION_ADD_CONSTANT(GL_SYNC_FENCE);
  ION_ADD_CONSTANT(GL_SYNC_FLAGS);
  ION_ADD_CONSTANT(GL_SYNC_GPU_COMMANDS_COMPLETE);
  ION_ADD_CONSTANT(GL_SYNC_STATUS);
}

static void IonAddConstantsTToZ(
    std::unordered_map<int, std::string>* constants) {
  ION_ADD_CONSTANT(GL_TEXTURE);
  ION_ADD_CONSTANT(GL_TEXTURE0);
  ION_ADD_CONSTANT(GL_TEXTURE1);
  ION_ADD_CONSTANT(GL_TEXTURE10);
  ION_ADD_CONSTANT(GL_TEXTURE11);
  ION_ADD_CONSTANT(GL_TEXTURE12);
  ION_ADD_CONSTANT(GL_TEXTURE13);
  ION_ADD_CONSTANT(GL_TEXTURE14);
  ION_ADD_CONSTANT(GL_TEXTURE15);
  ION_ADD_CONSTANT(GL_TEXTURE16);
  ION_ADD_CONSTANT(GL_TEXTURE17);
  ION_ADD_CONSTANT(GL_TEXTURE18);
  ION_ADD_CONSTANT(GL_TEXTURE19);
  ION_ADD_CONSTANT(GL_TEXTURE2);
  ION_ADD_CONSTANT(GL_TEXTURE20);
  ION_ADD_CONSTANT(GL_TEXTURE21);
  ION_ADD_CONSTANT(GL_TEXTURE22);
  ION_ADD_CONSTANT(GL_TEXTURE23);
  ION_ADD_CONSTANT(GL_TEXTURE24);
  ION_ADD_CONSTANT(GL_TEXTURE25);
  ION_ADD_CONSTANT(GL_TEXTURE26);
  ION_ADD_CONSTANT(GL_TEXTURE27);
  ION_ADD_CONSTANT(GL_TEXTURE28);
  ION_ADD_CONSTANT(GL_TEXTURE29);
  ION_ADD_CONSTANT(GL_TEXTURE3);
  ION_ADD_CONSTANT(GL_TEXTURE30);
  ION_ADD_CONSTANT(GL_TEXTURE31);
  ION_ADD_CONSTANT(GL_TEXTURE4);
  ION_ADD_CONSTANT(GL_TEXTURE5);
  ION_ADD_CONSTANT(GL_TEXTURE6);
  ION_ADD_CONSTANT(GL_TEXTURE7);
  ION_ADD_CONSTANT(GL_TEXTURE8);
  ION_ADD_CONSTANT(GL_TEXTURE9);
  ION_ADD_CONSTANT(GL_TEXTURE_1D_ARRAY);
  ION_ADD_CONSTANT(GL_TEXTURE_2D);
  ION_ADD_CONSTANT(GL_TEXTURE_2D_ARRAY);
  ION_ADD_CONSTANT(GL_TEXTURE_2D_MULTISAMPLE);
  ION_ADD_CONSTANT(GL_TEXTURE_2D_MULTISAMPLE_ARRAY);
  ION_ADD_CONSTANT(GL_TEXTURE_3D);
  ION_ADD_CONSTANT(GL_TEXTURE_BASE_LEVEL);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_1D_ARRAY);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_2D);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_2D_ARRAY);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_2D_MULTISAMPLE);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_3D);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_BUFFER);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_CUBE_MAP);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY);
  ION_ADD_CONSTANT(GL_TEXTURE_BINDING_EXTERNAL_OES);
  ION_ADD_CONSTANT(GL_TEXTURE_COMPARE_FUNC);
  ION_ADD_CONSTANT(GL_TEXTURE_COMPARE_MODE);
  ION_ADD_CONSTANT(GL_TEXTURE_CUBE_MAP);
  ION_ADD_CONSTANT(GL_TEXTURE_CUBE_MAP_ARRAY);
  ION_ADD_CONSTANT(GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
  ION_ADD_CONSTANT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
  ION_ADD_CONSTANT(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
  ION_ADD_CONSTANT(GL_TEXTURE_CUBE_MAP_POSITIVE_X);
  ION_ADD_CONSTANT(GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
  ION_ADD_CONSTANT(GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
  ION_ADD_CONSTANT(GL_TEXTURE_EXTERNAL_OES);
  ION_ADD_CONSTANT(GL_TEXTURE_IMMUTABLE_FORMAT);
  ION_ADD_CONSTANT(GL_TEXTURE_MAG_FILTER);
  ION_ADD_CONSTANT(GL_TEXTURE_MAX_ANISOTROPY_EXT);
  ION_ADD_CONSTANT(GL_TEXTURE_MAX_LEVEL);
  ION_ADD_CONSTANT(GL_TEXTURE_MAX_LOD);
  ION_ADD_CONSTANT(GL_TEXTURE_MIN_FILTER);
  ION_ADD_CONSTANT(GL_TEXTURE_MIN_LOD);
  ION_ADD_CONSTANT(GL_TEXTURE_SWIZZLE_A);
  ION_ADD_CONSTANT(GL_TEXTURE_SWIZZLE_B);
  ION_ADD_CONSTANT(GL_TEXTURE_SWIZZLE_G);
  ION_ADD_CONSTANT(GL_TEXTURE_SWIZZLE_R);
  ION_ADD_CONSTANT(GL_TEXTURE_WRAP_R);
  ION_ADD_CONSTANT(GL_TEXTURE_WRAP_S);
  ION_ADD_CONSTANT(GL_TEXTURE_WRAP_T);
  ION_ADD_CONSTANT(GL_TIMEOUT_EXPIRED);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_ACTIVE);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_BINDING);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_BUFFER);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_BUFFER_ACTIVE);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_BUFFER_MODE);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_BUFFER_PAUSED);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_BUFFER_START);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_PAUSED);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_VARYINGS);
  ION_ADD_CONSTANT(GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH);
  ION_ADD_CONSTANT(GL_TRIANGLES);
  ION_ADD_CONSTANT(GL_TRIANGLE_FAN);
  ION_ADD_CONSTANT(GL_TRIANGLE_STRIP);
  ION_ADD_CONSTANT(GL_UNPACK_ALIGNMENT);
  ION_ADD_CONSTANT(GL_UNSIGNALED);
  ION_ADD_CONSTANT(GL_UNSIGNED_BYTE);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_10F_11F_11F_REV);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_24_8);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_2_10_10_10_REV);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_5_9_9_9_REV);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_SAMPLER_1D);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_SAMPLER_1D_ARRAY);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_SAMPLER_2D);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_SAMPLER_2D_ARRAY);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_SAMPLER_3D);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_SAMPLER_CUBE);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_VEC2);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_VEC3);
  ION_ADD_CONSTANT(GL_UNSIGNED_INT_VEC4);
  ION_ADD_CONSTANT(GL_UNSIGNED_SHORT);
  ION_ADD_CONSTANT(GL_UNSIGNED_SHORT_4_4_4_4);
  ION_ADD_CONSTANT(GL_UNSIGNED_SHORT_5_5_5_1);
  ION_ADD_CONSTANT(GL_UNSIGNED_SHORT_5_6_5);
  ION_ADD_CONSTANT(GL_VALIDATE_STATUS);
  ION_ADD_CONSTANT(GL_VENDOR);
  ION_ADD_CONSTANT(GL_VERSION);
  ION_ADD_CONSTANT(GL_VERTEX_ARRAY_BINDING);
  ION_ADD_CONSTANT(GL_VERTEX_ARRAY_OBJECT);
  ION_ADD_CONSTANT(GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING);
  ION_ADD_CONSTANT(GL_VERTEX_ATTRIB_ARRAY_DIVISOR);
  ION_ADD_CONSTANT(GL_VERTEX_ATTRIB_ARRAY_ENABLED);
  ION_ADD_CONSTANT(GL_VERTEX_ATTRIB_ARRAY_NORMALIZED);
  ION_ADD_CONSTANT(GL_VERTEX_ATTRIB_ARRAY_POINTER);
  ION_ADD_CONSTANT(GL_VERTEX_ATTRIB_ARRAY_SIZE);
  ION_ADD_CONSTANT(GL_VERTEX_ATTRIB_ARRAY_STRIDE);
  ION_ADD_CONSTANT(GL_VERTEX_ATTRIB_ARRAY_TYPE);
  ION_ADD_CONSTANT(GL_VERTEX_SHADER);
  ION_ADD_CONSTANT(GL_VIEWPORT);
  ION_ADD_CONSTANT(GL_WAIT_FAILED);
  ION_ADD_CONSTANT(GL_WRITE_ONLY);
  // ION_ADD_CONSTANT(GL_ZERO);
}
#undef ION_ADD_CONSTANT

}  // anonymous namespace

TracingHelper::TracingHelper() {
  IonAddConstantsAToF(&constants_);
  IonAddConstantsGToQ(&constants_);
  IonAddConstantsRToS(&constants_);
  IonAddConstantsTToZ(&constants_);
}

// Unspecialized version.
template <typename T>
const std::string TracingHelper::ToString(const char* arg_type, T arg) {
  // Treat pointers specially.
  const std::string arg_type_str(arg_type);
  if (arg_type_str.find('*') != std::string::npos ||
      arg_type_str.find("PROC") != std::string::npos) {
    if (arg != static_cast<T>(0)) {
      std::ostringstream out;
      out << "0x" << std::hex << *reinterpret_cast<size_t*>(&arg);

      // If the pointer type is a known type then we can print more deeply.
      out << ArrayToString(arg_type, arg);
      return out.str();
    } else {
      return "NULL";
    }
  }

  return AnyToString(arg);
}

// Specialize to add quotes around strings.
template <> ION_API
const std::string TracingHelper::ToString(const char* arg_type,
                                          const char* arg) {
  return std::string("\"") + arg + '"';
}

// Specialize to add quotes around strings.
template <> ION_API
const std::string TracingHelper::ToString(const char* arg_type, char* arg) {
  return std::string("\"") + arg + '"';
}

// Specialize to print the first string in an array of strings.
template <> ION_API const std::string TracingHelper::ToString(
    const char* arg_type, const char** arg) {
  return arg ? "[" + ToString(arg_type, arg[0]) + ", ...]" : "NULL";
}

// Specialize to deal with GLboolean values.
template <> ION_API
const std::string TracingHelper::ToString(
    const char* arg_type, unsigned char arg) {
  // unsigned char is used only for GLboolean.
  switch (arg) {
    case 0:
      return "GL_FALSE";
    case 1:
      return "GL_TRUE";
    default: {
      return AnyToString(static_cast<int>(arg));
    }
  }
}

// Specialize to deal with GLenum values.
template <> ION_API
const std::string TracingHelper::ToString(const char* arg_type, int arg) {
  // int is sometimes used for parameter types and GLbitfield.
  if (!strcmp(arg_type, "GLtextureenum")) {
    // For texture parameters, only print certain valid values as enums, the
    // rest are just integers.
    if (arg >= 0 && !constants_[arg].empty()) {
      switch (arg) {
        case 0:
          return "GL_NONE";
        case GL_ALPHA:
        case GL_ALWAYS:
        case GL_BLUE:
        case GL_CLAMP_TO_EDGE:
        case GL_COMPARE_REF_TO_TEXTURE:
        case GL_EQUAL:
        case GL_GEQUAL:
        case GL_GREATER:
        case GL_GREEN:
        case GL_LESS:
        case GL_LINEAR:
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LEQUAL:
        case GL_MIRRORED_REPEAT:
        case GL_NEAREST:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_NEVER:
        case GL_NOTEQUAL:
        case GL_RED:
        case GL_REPEAT:
          return constants_[arg];
        default:
          break;
      }
      const std::string& name = constants_[arg];
      if (base::StartsWith(name, "GL_TEXTURE"))
        return constants_[arg];
    }
  } else if (!strcmp(arg_type, "GLintenum")) {
    if (arg >= 0 && !constants_[arg].empty()) {
      return constants_[arg];
    }
  }

  return AnyToString(arg);
}

// Specialize to deal with GLenum values.
template <> ION_API
const std::string TracingHelper::ToString(const char* arg_type,
                                          unsigned int arg) {
  // unsigned int is used for GLenum types, GLbitfield, GLmapaccess (a kind of
  // GLbitfield), and GLuint.
  if (!strcmp(arg_type, "GLblendenum")) {
    if (arg == GL_ZERO)
      return "GL_ZERO";
    else if (arg == GL_ONE)
      return "GL_ONE";
    else if (!constants_[arg].empty())
      return constants_[arg];
  } else if (!strcmp(arg_type, "GLstencilenum")) {
    if (arg == GL_ZERO)
      return "GL_ZERO";
    else if (!constants_[arg].empty())
      return constants_[arg];
  } else if (!strcmp(arg_type, "GLenum")) {
    if (!constants_[arg].empty())
      return constants_[arg];
  } else if (!strcmp(arg_type, "GLbitfield")) {
    // GLbitfield is used for glClear().
    const std::string s = GetClearBitsString(arg);
    if (!s.empty())
      return s;
  } else if (!strcmp(arg_type, "GLmapaccess")) {
    // GLmapaccess is used for glMapBufferRange().
    const std::string s = GetMapBitsString(arg);
    if (!s.empty())
      return s;
  } else if (!strcmp(arg_type, "GLtextureenum")) {
    if (arg == GL_NONE)
      return "GL_NONE";
    else if (!constants_[arg].empty())
      return constants_[arg];
  }
  return AnyToString(arg);
}

// Explicitly instantiate all the other unspecialized versions.
template ION_API const std::string TracingHelper::ToString(
    const char*, const float*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const unsigned char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const unsigned int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const void*);
template ION_API const std::string TracingHelper::ToString(
    const char*, float);
template ION_API const std::string TracingHelper::ToString(
    const char*, float*);
template ION_API const std::string TracingHelper::ToString(
    const char*, int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, void*);
template ION_API const std::string TracingHelper::ToString(
    const char*, void**);
template ION_API const std::string TracingHelper::ToString(
    const char*, GLsync);
template ION_API const std::string TracingHelper::ToString(const char*,
                                                           GLDEBUGPROC);

#endif  // ION_PRODUCTION

}  // namespace gfx
}  // namespace ion
