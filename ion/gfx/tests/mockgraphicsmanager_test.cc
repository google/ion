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

#include "ion/gfx/tests/mockgraphicsmanager.h"

#include <array>
#include <functional>
#include <memory>
#include <sstream>

#include "base/integral_types.h"
#include "ion/base/logchecker.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {
namespace testing {

namespace {

// Holds information about a uniform, such as its name, type, and location.
struct UniformInfo {
  enum Type {
    kInt,
    kUnsignedInt,
    kFloat,
    kMatrix
  };
  const char* name;
  GLenum gltype;
  // The vector length of the uniform (scalar, vec2, vec3, etc.).
  GLint length;
  Type type;
  GLint loc;
  // Array uniforms have 4 elements.
  GLint alocs[4];
};

// The following helpers are used by ExpandArgsAndCall.
//
// Sequence will work with std::get() to match a particular index and unpack
// a std::array.
template <size_t... Indicies> struct Sequence {};

// Inner case, instantiates an N-1 SequenceGenerator.
template <size_t N, size_t... Indices>
struct SequenceGenerator : SequenceGenerator<N - 1U, N - 1U, Indices...> {};

// Specializes for 0 base case, generating a Sequence with the other indices.
template <size_t... Indices>
struct SequenceGenerator<0U, Indices...> : Sequence<Indices...> {};

// Shader sources.
static const char kVertexSource[] =
    "// Vertex shader.\n"
    "attribute float attr_f;\n"
    // Technically the next line is an error, but it helps coverage.
    "attribute float attr_f;\n"
    "attribute vec2 attr_v2f;\n"
    "attribute vec3 attr_v3f;\n"
    "attribute vec4 attr_v4f;\n"
    "attribute mat2 attr_m2f;\n"
    "attribute mat3 attr_m3f;\n"
    "attribute mat4 attr_m4f;\n"
    "uniform highp float uni_f;\n"
    "uniform lowp vec2 uni_v2f;\n"
    "uniform vec3 uni_v3f;\n"
    "uniform vec4 uni_v4f;\n"
    "uniform int uni_i;\n"
    "uniform ivec2 uni_v2i;\n"
    "uniform ivec3 uni_v3i;\n"
    "uniform ivec4 uni_v4i;\n"
    "uniform uint uni_u;\n"
    "uniform uvec2 uni_v2u;\n"
    "uniform uvec3 uni_v3u;\n"
    "uniform uvec4 uni_v4u;\n"
    "uniform mat2 uni_m2;\n"
    "uniform mat3 uni_m3;\n"
    "uniform mat4 uni_m4;\n"
    "uniform isampler1D itex1d;\n"
    "uniform isampler1DArray itex1da;\n"
    "uniform isampler2D itex2d;\n"
    "uniform isampler2DArray itex2da;\n"
    "uniform isampler3D itex3d;\n"
    "uniform isamplerCube icm;\n"
    "uniform isamplerCubeArray icma;\n"
    "uniform sampler1D tex1d;\n"
    "uniform sampler1DArray tex1da;\n"
    "uniform sampler1DArrayShadow tex1das;\n"
    "uniform sampler1DShadow tex1ds;\n"
    "uniform sampler2D tex2d;\n"
    "uniform sampler2DArray tex2da;\n"
    "uniform sampler2DArrayShadow tex2das;\n"
    "uniform sampler2DShadow tex2ds;\n"
    "uniform sampler3D tex3d;\n"
    "uniform samplerCube cm;\n"
    "uniform samplerCubeArray cma;\n"
    "uniform samplerCubeArrayShadow cmas;\n"
    "uniform samplerCubeShadow cms;\n"
    "uniform samplerExternalOES seo;\n"
    "uniform usampler1D utex1d;\n"
    "uniform usampler1DArray utex1da;\n"
    "uniform usampler2D utex2d;\n"
    "uniform usampler2DArray utex2da;\n"
    "uniform usampler3D utex3d;\n"
    "uniform usamplerCube ucm;\n"
    "uniform usamplerCubeArray ucma;\n"
    // Will not generate a uniform.
    "uniform no_type bad_var;\n"
    "varying vec2 vary_v2f;\n"
    "varying mat4 vary_m4f;\n";

static const char kFragmentSource[] =
    "// Fragment shader.\n"
    "uniform highp float uni_f_array[4];\n"
    "uniform lowp vec2 uni_v2f_array[4];\n"
    "uniform vec3 uni_v3f_array[4];\n"
    "uniform vec4 uni_v4f_array[4];\n"
    "uniform int uni_i_array[4];\n"
    "uniform ivec2 uni_v2i_array[4];\n"
    "uniform ivec3 uni_v3i_array[4];\n"
    "uniform ivec4 uni_v4i_array[4];\n"
    "uniform uint uni_u_array[4];\n"
    "uniform uvec2 uni_v2u_array[4];\n"
    "uniform uvec3 uni_v3u_array[4];\n"
    "uniform uvec4 uni_v4u_array[4];\n"
    "uniform mat2 uni_m2_array[4];\n"
    "uniform mat3 uni_m3_array[4];\n"
    "uniform mat4 uni_m4_array[4];\n"
    "uniform isampler1D itex1d_array[4];\n"
    "uniform isampler1DArray itex1da_array[4];\n"
    "uniform isampler2D itex2d_array[4];\n"
    "uniform isampler2DArray itex2da_array[4];\n"
    "uniform isampler3D itex3d_array[4];\n"
    "uniform isamplerCube icm_array[4];\n"
    "uniform isamplerCubeArray icma_array[4];\n"
    "uniform sampler1D tex1d_array[4];\n"
    "uniform sampler1DArray tex1da_array[4];\n"
    "uniform sampler1DArrayShadow tex1das_array[4];\n"
    "uniform sampler1DShadow tex1ds_array[4];\n"
    "uniform sampler2D tex2d_array[4];\n"
    "uniform sampler2DArray tex2da_array[4];\n"
    "uniform sampler2DArrayShadow tex2das_array[4];\n"
    "uniform sampler2DShadow tex2ds_array[4];\n"
    "uniform sampler3D tex3d_array[4];\n"
    "uniform samplerCube cm_array[4];\n"
    "uniform samplerCubeArray cma_array[4];\n"
    "uniform samplerCubeArrayShadow cmas_array[4];\n"
    "uniform samplerCubeShadow cms_array[4];\n"
    "uniform samplerExternalOES seo_array[4];\n"
    "uniform usampler1D utex1d_array[4];\n"
    "uniform usampler1DArray utex1da_array[4];\n"
    "uniform usampler2D utex2d_array[4];\n"
    "uniform usampler2DArray utex2da_array[4];\n"
    "uniform usampler3D utex3d_array[4];\n"
    "uniform usamplerCube ucm_array[4];\n"
    "uniform usamplerCubeArray ucma_array[4];\n"
    "varying vec2 vary_v2f;\n";

// Convenience defines to call a GraphicsManager function and check a particular
// error or no error occurred.
#define GM_CHECK_ERROR(error) \
  EXPECT_EQ(static_cast<GLenum>(error), gm->GetError())
#define GM_CHECK_NO_ERROR \
  GM_CHECK_ERROR(GL_NO_ERROR)
#define GM_ERROR_CALL(call, error) gm->call; GM_CHECK_ERROR(error)
#define GM_CALL(call) GM_ERROR_CALL(call, GL_NO_ERROR)

// Convenience function to get a float value from an OpenGL vertex attribute.
static GLfloat GetAttribFloat(const MockGraphicsManagerPtr& gm, GLuint index,
                              GLenum what) {
  GLfloat f;
  GM_CALL(GetVertexAttribfv(index, what, &f));
  return f;
}

// Convenience function to get a vec4f value from an OpenGL vertex attribute.
static math::Vector4f GetAttribFloat4(const MockGraphicsManagerPtr& gm,
                                      GLuint index, GLenum what) {
  math::Vector4f f;
  GM_CALL(GetVertexAttribfv(index, what, &f[0]));
  return f;
}

// Convenience function to get an integer value from an OpenGL vertex attribute.
static GLint GetAttribInt(const MockGraphicsManagerPtr& gm, GLuint index,
                          GLenum what) {
  GLint i;
  GM_CALL(GetVertexAttribiv(index, what, &i));
  return i;
}

// Convenience function to get a vec4i value from an OpenGL vertex attribute.
static math::Vector4i GetAttribInt4(const MockGraphicsManagerPtr& gm,
                                    GLuint index, GLenum what) {
  math::Vector4i i;
  GM_CALL(GetVertexAttribiv(index, what, &i[0]));
  return i;
}

// Convenience function to get a vec4f value from an OpenGL vertex attribute.
static GLvoid* GetAttribPointer(const MockGraphicsManagerPtr& gm, GLuint index,
                                GLenum what) {
  GLvoid* p;
  GM_CALL(GetVertexAttribPointerv(index, what, &p));
  return p;
}

// Convenience function to get a single boolean value from OpenGL.
static GLboolean GetBoolean(const MockGraphicsManagerPtr& gm, GLenum what) {
  GLboolean b;
  GM_CALL(GetBooleanv(what, &b));
  return b;
}

// Convenience function to get a buffer parameter value from OpenGL.
static GLint GetBufferInt(const MockGraphicsManagerPtr& gm, GLenum target,
                          GLenum what) {
  GLint i;
  GM_CALL(GetBufferParameteriv(target, what, &i));
  return i;
}

static GLboolean GetEnabled(const MockGraphicsManagerPtr& gm, GLenum what) {
  GLboolean b = gm->IsEnabled(what);
  GM_CHECK_NO_ERROR;
  // Check that GetIntegerv also returns the same value for capabilities.
  GLint i;
  GM_CALL(GetIntegerv(what, &i));
  EXPECT_EQ(b, i);
  return b;
}

// Convenience function to get a framebuffer attachment value from OpenGL.
static GLint GetFramebufferAttachmentInt(
    const MockGraphicsManagerPtr& gm, GLenum target, GLenum attachment,
    GLenum pname) {
  GLint i;
  GM_CALL(GetFramebufferAttachmentParameteriv(target, attachment, pname, &i));
  return i;
}

// Convenience function to get a renderbuffer parameter value from OpenGL.
static GLint GetRenderbufferInt(
    const MockGraphicsManagerPtr& gm, GLenum target, GLenum pname) {
  GLint i;
  GM_CALL(GetRenderbufferParameteriv(target, pname, &i));
  return i;
}

// Convenience function to get a mask value from OpenGL.
static GLuint GetMask(const MockGraphicsManagerPtr& gm, GLenum what) {
  GLint i;
  GM_CALL(GetIntegerv(what, &i));
  return static_cast<GLuint>(i);
}

// Convenience function to get a single float value from OpenGL.
static GLfloat GetFloat(const MockGraphicsManagerPtr& gm, GLenum what) {
  GLfloat f;
  GM_CALL(GetFloatv(what, &f));
  return f;
}

// Convenience function to get a single integer value from OpenGL. Note that
// the #defined GL_ constants are actually integers, not GLenums.
static GLint GetInt(const MockGraphicsManagerPtr& gm, GLenum what) {
  GLint i;
  GM_CALL(GetIntegerv(what, &i));
  return i;
}

// Convenience function to get a single integer value from an OpenGL program.
static GLint GetProgramInt(const MockGraphicsManagerPtr& gm, GLuint program,
                           GLenum what) {
  GLint i;
  GM_CALL(GetProgramiv(program, what, &i));
  return i;
}

// Convenience function to get a single integer value from an OpenGL shader.
static GLint GetShaderInt(const MockGraphicsManagerPtr& gm, GLuint shader,
                           GLenum what) {
  GLint i;
  GM_CALL(GetShaderiv(shader, what, &i));
  return i;
}

// Convenience function to get a single string value from OpenGL.
static std::string GetString(const MockGraphicsManagerPtr& gm, GLenum what) {
  const GLubyte* string;
  string = GM_CALL(GetString(what));
  return std::string(reinterpret_cast<const char*>(string));
}

// Convenience function to get a single string value from OpenGL.
static std::string GetStringi(const MockGraphicsManagerPtr& gm, GLenum what,
                              GLuint index) {
  const GLubyte* string;
  string = GM_CALL(GetStringi(what, index));
  return std::string(reinterpret_cast<const char*>(string));
}

// Convenience functions to get a single float/integer value from an OpenGL
// sampler.
static GLfloat GetSamplerFloat(const MockGraphicsManagerPtr& gm, GLuint sampler,
                               GLenum what) {
  GLfloat f;
  GM_CALL(GetSamplerParameterfv(sampler, what, &f));
  return f;
}
static GLint GetSamplerInt(const MockGraphicsManagerPtr& gm, GLuint sampler,
                           GLenum what) {
  GLint i;
  GM_CALL(GetSamplerParameteriv(sampler, what, &i));
  return i;
}

// Convenience functions to get a single float/integer value from an OpenGL
// texture.
static GLfloat GetTextureFloat(const MockGraphicsManagerPtr& gm, GLuint texture,
                               GLenum what) {
  GLfloat f;
  GM_CALL(GetTexParameterfv(texture, what, &f));
  return f;
}
static GLint GetTextureInt(const MockGraphicsManagerPtr& gm, GLuint texture,
                           GLenum what) {
  GLint i;
  GM_CALL(GetTexParameteriv(texture, what, &i));
  return i;
}

static void VerifySetAndGetLabel(const MockGraphicsManagerPtr& gm,
                                 GLenum type, GLuint id) {
  static const int kBufLen = 64;
  char label[kBufLen];
  GLint length = 0;

  GM_ERROR_CALL(LabelObject(type, id + 1U, 0, ""), GL_INVALID_OPERATION);
  GM_ERROR_CALL(LabelObject(type, id, -1, ""), GL_INVALID_VALUE);

  // Set the label.
  const std::string test_label("texture_label");
  GM_CALL(LabelObject(
      type, id, static_cast<GLsizei>(test_label.length()), test_label.c_str()));

  GM_ERROR_CALL(GetObjectLabel(type, id, -1, &length, label),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(GetObjectLabel(type, id + 1U, kBufLen, &length, label),
                GL_INVALID_OPERATION);
  GM_CALL(GetObjectLabel(type, id, kBufLen, &length, label));
  EXPECT_EQ(static_cast<GLint>(test_label.length()), length);
  EXPECT_EQ(test_label, label);

  // Clear the label.
  GM_CALL(LabelObject(type, id, 0, ""));
  GM_CALL(GetObjectLabel(type, id, kBufLen, &length, label));
  EXPECT_EQ(0, length);
  EXPECT_EQ(std::string(), label);
}

// Convenience function to allocate and attach a multisample render buffer.
static void AllocateAndAttachMultisampleRenderBuffer(
    const MockGraphicsManagerPtr& gm, GLenum internal_format, GLenum attachment,
    int width, int height, int samples) {
  GLuint id;
  GM_CALL(GenRenderbuffers(1, &id));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, id));
  GM_CALL(RenderbufferStorageMultisample(
      GL_RENDERBUFFER, samples, internal_format, width, height));
  GM_CALL(FramebufferRenderbuffer(
      GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, id));
}

// Convenience function to allocate and attach a render buffer.
static void AllocateAndAttachRenderBuffer(
    const MockGraphicsManagerPtr& gm, GLenum internal_format, GLenum attachment,
    int width, int height) {
  GLuint id;
  GM_CALL(GenRenderbuffers(1, &id));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, id));
  GM_CALL(RenderbufferStorage(
      GL_RENDERBUFFER, internal_format, width, height));
  GM_CALL(FramebufferRenderbuffer(
      GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, id));
}

// Calls the passed Set function on the passed MockGraphicsManager. The passed
// array is expanded to fill the functions arguments.
template <typename T, typename... Args, size_t... Indices>
static void ExpandArgsAndCall(const MockGraphicsManagerPtr& gm, GLint loc,
                              void (GraphicsManager::*Set)(GLint, Args...),
                              Sequence<Indices...> seq,
                              const std::array<T, 4>& args) {
  ((*gm).*Set)(loc, std::get<Indices>(args)...);
}

template <typename T, typename... Args>
static void TestUniform(const UniformInfo& info,
                        const MockGraphicsManagerPtr& gm, GLuint pid,
                        GLint length, GLint array_len, UniformInfo::Type type,
                        void (GraphicsManager::*Getv)(GLuint, GLint, T*),
                        void (GraphicsManager::*Set)(GLint, Args...),
                        void (GraphicsManager::*Setv)(GLint, GLsizei,
                                                      const T*)) {
  const T v4[4][4] = {{static_cast<T>(1.1f), static_cast<T>(2.2f),
                       static_cast<T>(3.3f), static_cast<T>(4.4f)},
                      {static_cast<T>(11.11f), static_cast<T>(22.22f),
                       static_cast<T>(33.33f), static_cast<T>(44.44f)},
                      {static_cast<T>(111.111f), static_cast<T>(222.222f),
                       static_cast<T>(333.333f), static_cast<T>(444.444f)},
                      {static_cast<T>(1111.1111f), static_cast<T>(2222.2222f),
                       static_cast<T>(3333.3333f), static_cast<T>(4444.4444f)}};
  T test4[4][4];
  memset(test4, 0, sizeof(test4));
  std::array<T, 4> values[4];
  for (int i = 0; i < 4; ++i) {
    float base = 0.f;
    for (int j = 0; j <= i; ++j) {
      const float jf = static_cast<float>(j);
      base += powf(10.f, jf) + powf(10.f, -jf - 1.f);
    }
    for (int j = 0; j < 4; ++j) {
      values[i][j] = static_cast<T>(base * static_cast<float>(j + 1));
    }
  }
  if (info.length == length && info.type == type) {
    static const T kTolerance = static_cast<T>(1e-4f);

    // Set all 4 values, then 3, then 2, then 1, make sure overlaps work. If
    // there is only one value, then we don't need to check overlaps.
    ExpandArgsAndCall(gm, info.loc, Set, SequenceGenerator<sizeof...(Args)>{},
                      values[0]); GM_CHECK_NO_ERROR;
    ((*gm).*Getv)(pid, info.loc, &test4[0][0]); GM_CHECK_NO_ERROR;
    for (GLint j = 0; j < info.length; ++j)
      EXPECT_EQ(values[0][j], test4[0][j]);

    // Test array values if available.
    for (GLint i = 0; i < array_len; ++i) {
      if (info.alocs[i] != -1) {
        ExpandArgsAndCall(gm, info.alocs[i], Set,
                          SequenceGenerator<sizeof...(Args)>{},
                          values[i]); GM_CHECK_NO_ERROR;

        // Retrieve the array element.
        ((*gm).*Getv)(pid, info.alocs[i], &test4[i][0]); GM_CHECK_NO_ERROR;
        for (GLint j = 0; j < info.length; ++j) {
          EXPECT_EQ(values[i][j], test4[i][j]);
        }
      }
    }
    // Set / get the entire uniform.
    ((*gm).*Setv)(info.loc, array_len, &v4[0][0]); GM_CHECK_NO_ERROR;
    ((*gm).*Getv)(pid, info.loc, &test4[0][0]); GM_CHECK_NO_ERROR;
    for (GLint i = 0; i < array_len; ++i) {
      for (GLint j = 0; j < info.length; ++j) {
        EXPECT_NEAR(v4[i][j], test4[i][j], kTolerance);
      }
    }

    if (info.alocs[0] != -1) {
      // Since the values are set in memory order, we need to treat the values
      // sent to GL as a single array.
      const T* set_values = &v4[0][0];

      // Get each element.
      for (GLint i = 0; i < array_len; ++i) {
        ((*gm).*Getv)(pid, info.alocs[i], &test4[i][0]); GM_CHECK_NO_ERROR;
        for (GLint j = 0; j < info.length; ++j) {
          EXPECT_EQ(*set_values++, test4[i][j]);
        }
      }
    }
  } else {
    ExpandArgsAndCall(gm, info.loc, Set, SequenceGenerator<sizeof...(Args)>{},
                      values[0]); GM_CHECK_ERROR(GL_INVALID_OPERATION);
    ((*gm).*Setv)(info.loc, array_len,
                  &v4[0][0]); GM_CHECK_ERROR(GL_INVALID_OPERATION);
  }
}

}  // anonymous namespace

TEST(MockGraphicsManagerTest, Capabilities) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // By default, all capabilities are disabled except for GL_DITHER.
  EXPECT_FALSE(GetEnabled(gm, GL_BLEND));
  EXPECT_FALSE(GetEnabled(gm, GL_CULL_FACE));
  EXPECT_FALSE(GetEnabled(gm, GL_DEPTH_TEST));
  EXPECT_TRUE(GetEnabled(gm, GL_DITHER));
  EXPECT_FALSE(GetEnabled(gm, GL_MULTISAMPLE));
  EXPECT_FALSE(GetEnabled(gm, GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(gm, GL_SAMPLE_ALPHA_TO_COVERAGE));
  EXPECT_FALSE(GetEnabled(gm, GL_SAMPLE_COVERAGE));
  EXPECT_FALSE(GetEnabled(gm, GL_SCISSOR_TEST));
  EXPECT_FALSE(GetEnabled(gm, GL_STENCIL_TEST));
  EXPECT_FALSE(GetEnabled(gm, GL_POINT_SPRITE));
  EXPECT_FALSE(GetEnabled(gm, GL_PROGRAM_POINT_SIZE));

  GM_CALL(Enable(GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(gm, GL_BLEND));
  EXPECT_FALSE(GetEnabled(gm, GL_CULL_FACE));
  EXPECT_FALSE(GetEnabled(gm, GL_DEPTH_TEST));
  EXPECT_TRUE(GetEnabled(gm, GL_DITHER));
  EXPECT_FALSE(GetEnabled(gm, GL_MULTISAMPLE));
  EXPECT_TRUE(GetEnabled(gm, GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(gm, GL_SAMPLE_ALPHA_TO_COVERAGE));
  EXPECT_FALSE(GetEnabled(gm, GL_SAMPLE_COVERAGE));
  EXPECT_FALSE(GetEnabled(gm, GL_SCISSOR_TEST));
  EXPECT_FALSE(GetEnabled(gm, GL_STENCIL_TEST));
  EXPECT_FALSE(GetEnabled(gm, GL_POINT_SPRITE));
  EXPECT_FALSE(GetEnabled(gm, GL_PROGRAM_POINT_SIZE));

  GM_CALL(Disable(GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(gm, GL_BLEND));
  EXPECT_FALSE(GetEnabled(gm, GL_CULL_FACE));
  EXPECT_FALSE(GetEnabled(gm, GL_DEPTH_TEST));
  EXPECT_TRUE(GetEnabled(gm, GL_DITHER));
  EXPECT_FALSE(GetEnabled(gm, GL_MULTISAMPLE));
  EXPECT_FALSE(GetEnabled(gm, GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(gm, GL_SAMPLE_ALPHA_TO_COVERAGE));
  EXPECT_FALSE(GetEnabled(gm, GL_SAMPLE_COVERAGE));
  EXPECT_FALSE(GetEnabled(gm, GL_SCISSOR_TEST));
  EXPECT_FALSE(GetEnabled(gm, GL_STENCIL_TEST));
  EXPECT_FALSE(GetEnabled(gm, GL_POINT_SPRITE));
  EXPECT_FALSE(GetEnabled(gm, GL_PROGRAM_POINT_SIZE));

  GM_CALL(Enable(GL_BLEND));
  EXPECT_TRUE(GetEnabled(gm, GL_BLEND));
  GM_CALL(Disable(GL_BLEND));
  EXPECT_FALSE(GetEnabled(gm, GL_BLEND));

  GM_CALL(Enable(GL_STENCIL_TEST));
  EXPECT_TRUE(GetEnabled(gm, GL_STENCIL_TEST));
  GM_CALL(Disable(GL_STENCIL_TEST));
  EXPECT_FALSE(GetEnabled(gm, GL_STENCIL_TEST));

  GM_CALL(Enable(GL_POINT_SPRITE));
  EXPECT_TRUE(GetEnabled(gm, GL_POINT_SPRITE));
  GM_CALL(Disable(GL_POINT_SPRITE));
  EXPECT_FALSE(GetEnabled(gm, GL_POINT_SPRITE));

  GM_CALL(Enable(GL_PROGRAM_POINT_SIZE));
  EXPECT_TRUE(GetEnabled(gm, GL_PROGRAM_POINT_SIZE));
  GM_CALL(Disable(GL_PROGRAM_POINT_SIZE));
  EXPECT_FALSE(GetEnabled(gm, GL_PROGRAM_POINT_SIZE));
}

TEST(MockGraphicsManagerTest, VersionStandardRenderer) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // Check defaults.
  EXPECT_EQ("3.3 Ion OpenGL / ES", gm->GetGlVersionString());
  EXPECT_EQ(33U, gm->GetGlVersion());
  EXPECT_EQ(GraphicsManager::kEs, gm->GetGlApiStandard());

  gm->SetVersionString("3.0 Ion OpenGL");
  EXPECT_EQ("3.0 Ion OpenGL", gm->GetGlVersionString());
  EXPECT_EQ(30U, gm->GetGlVersion());
  EXPECT_EQ(GraphicsManager::kDesktop, gm->GetGlApiStandard());

  gm->SetVersionString("1.2 Ion WebGL");
  EXPECT_EQ("1.2 Ion WebGL", gm->GetGlVersionString());
  // WebGL is always 2.0 for compatibility with ES2.
  EXPECT_EQ(20U, gm->GetGlVersion());
  EXPECT_EQ(GraphicsManager::kWeb, gm->GetGlApiStandard());

  gm->SetVersionString("2.0 Ion OpenGL ES");
  EXPECT_EQ("2.0 Ion OpenGL ES", gm->GetGlVersionString());
  EXPECT_EQ(20U, gm->GetGlVersion());
  EXPECT_EQ(GraphicsManager::kEs, gm->GetGlApiStandard());

  EXPECT_EQ("Ion fake OpenGL / ES", gm->GetGlRenderer());
  gm->SetRendererString("Renderer");
  EXPECT_EQ("Renderer", gm->GetGlRenderer());
}

TEST(MockGraphicsManagerTest, ProfileType) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // Check default.
  EXPECT_EQ(GraphicsManager::kCompatibilityProfile, gm->GetGlProfileType());

  gm->SetContextProfileMask(GL_CONTEXT_CORE_PROFILE_BIT);
  EXPECT_EQ(GraphicsManager::kCoreProfile, gm->GetGlProfileType());

  gm->SetContextProfileMask(GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
  EXPECT_EQ(GraphicsManager::kCompatibilityProfile, gm->GetGlProfileType());
}

TEST(MockGraphicsManagerTest, CallCount) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  // This graphics manager relies upon the MockVisual set up by the first.
  MockGraphicsManagerPtr gm2(new MockGraphicsManager());

  // GetString is called thrice to get the GL version, renderer, and extensions
  // and then twice again by the second manager. GetIntegerv is called three
  // times, one at initial setup, again with the MGM inits, and twice this for
  // both managers. Vertex arrays are also checked via a call to GenVertexArrays
  // and a corresponding DeleteVertexArrays.
  EXPECT_EQ(12, MockGraphicsManager::GetCallCount());
  EXPECT_FALSE(GetEnabled(gm, GL_BLEND));
  // GetEnabled calls IsEnabled and GetIntegerv once, and GetError twice, plus
  // the above calls.
  EXPECT_EQ(14, MockGraphicsManager::GetCallCount());

  GLuint ids[2];
  GM_CALL(GenTextures(2, ids));
  EXPECT_EQ(15, MockGraphicsManager::GetCallCount());

  EXPECT_FALSE(GetEnabled(gm, GL_STENCIL_TEST));
  EXPECT_EQ(17, MockGraphicsManager::GetCallCount());

  MockGraphicsManager::ResetCallCount();
  EXPECT_EQ(0, MockGraphicsManager::GetCallCount());
}

TEST(MockGraphicsManagerTest, InitialState) {
  static const int kWidth = 400;
  static const int kHeight = 300;
  MockVisual visual(kWidth, kHeight);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  GLboolean b4[4];
  GLfloat f4[4];
  GLint i7[7];

  GM_CALL(GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, f4));
  EXPECT_EQ(1.f, f4[0]);
  EXPECT_EQ(256.f, f4[1]);
  GM_CALL(GetFloatv(GL_ALIASED_POINT_SIZE_RANGE, f4));
  EXPECT_EQ(1.f, f4[0]);
  EXPECT_EQ(8192.f, f4[1]);
  EXPECT_EQ(8, GetInt(gm, GL_ALPHA_BITS));
  GM_CALL(GetFloatv(GL_BLEND_COLOR, f4));
  EXPECT_EQ(0.0f, f4[0]);
  EXPECT_EQ(0.0f, f4[1]);
  EXPECT_EQ(0.0f, f4[2]);
  EXPECT_EQ(0.0f, f4[3]);
  EXPECT_EQ(GL_FUNC_ADD, GetInt(gm, GL_BLEND_EQUATION_ALPHA));
  EXPECT_EQ(GL_FUNC_ADD, GetInt(gm, GL_BLEND_EQUATION_RGB));
  EXPECT_EQ(GL_ONE, GetInt(gm, GL_BLEND_SRC_ALPHA));
  EXPECT_EQ(GL_ONE, GetInt(gm, GL_BLEND_SRC_RGB));
  EXPECT_EQ(GL_ZERO, GetInt(gm, GL_BLEND_DST_ALPHA));
  EXPECT_EQ(GL_ZERO, GetInt(gm, GL_BLEND_DST_RGB));
  EXPECT_EQ(8, GetInt(gm, GL_BLUE_BITS));
  GM_CALL(GetFloatv(GL_COLOR_CLEAR_VALUE, f4));
  EXPECT_EQ(0.0f, f4[0]);
  EXPECT_EQ(0.0f, f4[1]);
  EXPECT_EQ(0.0f, f4[2]);
  EXPECT_EQ(0.0f, f4[3]);
  // Type conversion check from float to boolean.
  GM_CALL(GetBooleanv(GL_COLOR_CLEAR_VALUE, b4));
  EXPECT_EQ(GL_FALSE, b4[0]);
  EXPECT_EQ(GL_FALSE, b4[1]);
  EXPECT_EQ(GL_FALSE, b4[2]);
  EXPECT_EQ(GL_FALSE, b4[3]);
  GM_CALL(GetBooleanv(GL_COLOR_WRITEMASK, b4));
  EXPECT_EQ(GL_TRUE, b4[0]);
  EXPECT_EQ(GL_TRUE, b4[1]);
  EXPECT_EQ(GL_TRUE, b4[2]);
  EXPECT_EQ(GL_TRUE, b4[3]);
  GM_CALL(GetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, i7));
  EXPECT_EQ(GL_COMPRESSED_RGB_S3TC_DXT1_EXT, i7[0]);
  EXPECT_EQ(GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG, i7[1]);
  EXPECT_EQ(GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, i7[2]);
  EXPECT_EQ(GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, i7[3]);
  EXPECT_EQ(GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, i7[4]);
  EXPECT_EQ(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, i7[5]);
  EXPECT_EQ(GL_ETC1_RGB8_OES, i7[6]);
  EXPECT_EQ(GL_BACK, GetInt(gm, GL_CULL_FACE_MODE));
  EXPECT_EQ(16, GetInt(gm, GL_DEPTH_BITS));
  EXPECT_EQ(1.0f, GetFloat(gm, GL_DEPTH_CLEAR_VALUE));
  EXPECT_EQ(GL_LESS, GetInt(gm, GL_DEPTH_FUNC));
  // Test type conversion with depth range.
  GM_CALL(GetBooleanv(GL_DEPTH_RANGE, b4));
  EXPECT_EQ(GL_FALSE, b4[0]);
  EXPECT_EQ(GL_TRUE, b4[1]);
  GM_CALL(GetFloatv(GL_DEPTH_RANGE, f4));
  EXPECT_EQ(0.0f, f4[0]);
  EXPECT_EQ(1.0f, f4[1]);
  GM_CALL(GetIntegerv(GL_DEPTH_RANGE, i7));
  EXPECT_EQ(0, i7[0]);
  EXPECT_EQ(1, i7[1]);
  // Conversions.
  EXPECT_EQ(GL_TRUE, GetBoolean(gm, GL_DEPTH_WRITEMASK));
  EXPECT_EQ(1.f, GetFloat(gm, GL_DEPTH_WRITEMASK));
  EXPECT_EQ(GL_CCW, GetInt(gm, GL_FRONT_FACE));
  // Boolean type conversion.
  EXPECT_EQ(GL_TRUE, GetBoolean(gm, GL_FRONT_FACE));
  EXPECT_EQ(GL_DONT_CARE, GetInt(gm, GL_GENERATE_MIPMAP_HINT));
  EXPECT_EQ(8, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(GL_UNSIGNED_BYTE, GetInt(gm, GL_IMPLEMENTATION_COLOR_READ_FORMAT));
  EXPECT_EQ(GL_RGB, GetInt(gm, GL_IMPLEMENTATION_COLOR_READ_TYPE));
  EXPECT_EQ(1.f, GetFloat(gm, GL_LINE_WIDTH));
  EXPECT_EQ(32, GetInt(gm, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(8192, GetInt(gm, GL_MAX_CUBE_MAP_TEXTURE_SIZE));
  EXPECT_EQ(8192, GetInt(gm, GL_MAX_TEXTURE_SIZE));
  EXPECT_EQ(4, GetInt(gm, GL_MAX_COLOR_ATTACHMENTS));
  EXPECT_EQ(4, GetInt(gm, GL_MAX_DRAW_BUFFERS));
  // Test type conversion from int to float.
  EXPECT_EQ(32.f, GetFloat(gm, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(16.f, GetFloat(gm, GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_EQ(8192.f, GetFloat(gm, GL_MAX_CUBE_MAP_TEXTURE_SIZE));
  EXPECT_EQ(512, GetInt(gm, GL_MAX_FRAGMENT_UNIFORM_VECTORS));
  EXPECT_EQ(4096, GetInt(gm, GL_MAX_RENDERBUFFER_SIZE));
  EXPECT_EQ(32, GetInt(gm, GL_MAX_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(8192.f, GetFloat(gm, GL_MAX_TEXTURE_SIZE));
  EXPECT_EQ(15, GetInt(gm, GL_MAX_VARYING_VECTORS));
  EXPECT_EQ(32, GetInt(gm, GL_MAX_VERTEX_ATTRIBS));
  EXPECT_EQ(32, GetInt(gm, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(1024, GetInt(gm, GL_MAX_VERTEX_UNIFORM_VECTORS));
  GM_CALL(GetIntegerv(GL_MAX_VIEWPORT_DIMS, i7));
  EXPECT_EQ(8192, i7[0]);
  EXPECT_EQ(8192, i7[1]);
  EXPECT_EQ(7, GetInt(gm, GL_NUM_COMPRESSED_TEXTURE_FORMATS));
  EXPECT_EQ(1, GetInt(gm, GL_NUM_SHADER_BINARY_FORMATS));
  EXPECT_EQ(4, GetInt(gm, GL_PACK_ALIGNMENT));
  EXPECT_EQ(1.f, GetFloat(gm, GL_POINT_SIZE));
  EXPECT_EQ(0.0f, GetFloat(gm, GL_POLYGON_OFFSET_FACTOR));
  EXPECT_EQ(0.0f, GetFloat(gm, GL_POLYGON_OFFSET_UNITS));
  EXPECT_EQ(8, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(1.0f, GetFloat(gm, GL_SAMPLE_COVERAGE_VALUE));
  EXPECT_EQ(GL_FALSE, GetBoolean(gm, GL_SAMPLE_COVERAGE_INVERT));
  EXPECT_EQ(1, GetInt(gm, GL_SAMPLES));
  GM_CALL(GetIntegerv(GL_SCISSOR_BOX, i7));
  EXPECT_EQ(0, i7[0]);
  EXPECT_EQ(0, i7[1]);
  EXPECT_EQ(kWidth, i7[2]);
  EXPECT_EQ(kHeight, i7[3]);
  EXPECT_EQ(0xbadf00d, GetInt(gm, GL_SHADER_BINARY_FORMATS));
  EXPECT_EQ(GL_FALSE, GetBoolean(gm, GL_SHADER_COMPILER));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_ALWAYS, GetInt(gm, GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_PASS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BACK_REF));
  // Boolean conversion.
  EXPECT_EQ(GL_FALSE, GetBoolean(gm, GL_STENCIL_BACK_REF));
  EXPECT_EQ(0xffffffffU, GetMask(gm, GL_STENCIL_BACK_VALUE_MASK));
  EXPECT_EQ(0xffffffffU, GetMask(gm, GL_STENCIL_BACK_WRITEMASK));
  EXPECT_EQ(8, GetInt(gm, GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_CLEAR_VALUE));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_FAIL));
  EXPECT_EQ(GL_ALWAYS, GetInt(gm, GL_STENCIL_FUNC));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_REF));
  EXPECT_EQ(0xffffffffU, GetMask(gm, GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(0xffffffffU, GetMask(gm, GL_STENCIL_WRITEMASK));
  EXPECT_EQ(4, GetInt(gm, GL_SUBPIXEL_BITS));
  EXPECT_EQ(4, GetInt(gm, GL_UNPACK_ALIGNMENT));
  GM_CALL(GetIntegerv(GL_VIEWPORT, i7));
  EXPECT_EQ(0, i7[0]);
  EXPECT_EQ(0, i7[1]);
  EXPECT_EQ(kWidth, i7[2]);
  EXPECT_EQ(kHeight, i7[3]);

  // Error conditions of GetFloat and GetInt.
  GM_ERROR_CALL(GetIntegerv(GL_ARRAY_BUFFER, i7), GL_INVALID_ENUM);
  GM_ERROR_CALL(GetFloatv(GL_ARRAY_BUFFER, f4), GL_INVALID_ENUM);
  // Check error case of IsEnabled.
  GM_ERROR_CALL(IsEnabled(GL_PACK_ALIGNMENT), GL_INVALID_ENUM);
}

TEST(MockGraphicsManagerTest, ChangeState) {
  // Check each supported call that modifies state.
  static const int kWidth = 400;
  static const int kHeight = 300;
  MockVisual visual(kWidth, kHeight);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  GLfloat f4[4];
  GLint i4[4];

  GM_CALL(BlendColor(.2f, .3f, -.4f, 1.5f));  // Should clamp.
  GM_CALL(GetFloatv(GL_BLEND_COLOR, f4));
  EXPECT_EQ(0.2f, f4[0]);
  EXPECT_EQ(0.3f, f4[1]);
  EXPECT_EQ(0.0f, f4[2]);
  EXPECT_EQ(1.0f, f4[3]);

  GM_CALL(BlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT));
  EXPECT_EQ(GL_FUNC_SUBTRACT, GetInt(gm, GL_BLEND_EQUATION_RGB));
  EXPECT_EQ(GL_FUNC_REVERSE_SUBTRACT, GetInt(gm, GL_BLEND_EQUATION_ALPHA));
  GM_CALL(BlendEquation(GL_FUNC_ADD));
  EXPECT_EQ(GL_FUNC_ADD, GetInt(gm, GL_BLEND_EQUATION_RGB));
  EXPECT_EQ(GL_FUNC_ADD, GetInt(gm, GL_BLEND_EQUATION_ALPHA));

  GM_CALL(BlendFuncSeparate(GL_ONE_MINUS_CONSTANT_COLOR, GL_DST_COLOR,
                            GL_ONE_MINUS_CONSTANT_ALPHA, GL_DST_ALPHA));
  EXPECT_EQ(GL_ONE_MINUS_CONSTANT_COLOR, GetInt(gm, GL_BLEND_SRC_RGB));
  EXPECT_EQ(GL_DST_COLOR, GetInt(gm, GL_BLEND_DST_RGB));
  EXPECT_EQ(GL_ONE_MINUS_CONSTANT_ALPHA, GetInt(gm, GL_BLEND_SRC_ALPHA));
  EXPECT_EQ(GL_DST_ALPHA, GetInt(gm, GL_BLEND_DST_ALPHA));
  GM_CALL(BlendFunc(GL_CONSTANT_COLOR, GL_SRC_ALPHA));
  EXPECT_EQ(GL_CONSTANT_COLOR, GetInt(gm, GL_BLEND_SRC_RGB));
  EXPECT_EQ(GL_SRC_ALPHA, GetInt(gm, GL_BLEND_DST_RGB));
  EXPECT_EQ(GL_CONSTANT_COLOR, GetInt(gm, GL_BLEND_SRC_ALPHA));
  EXPECT_EQ(GL_SRC_ALPHA, GetInt(gm, GL_BLEND_DST_ALPHA));

  GM_CALL(ClearColor(.2f, .3f, 1.4f, -.5f));  // Should clamp.
  GM_CALL(GetFloatv(GL_COLOR_CLEAR_VALUE, f4));
  EXPECT_EQ(0.2f, f4[0]);
  EXPECT_EQ(0.3f, f4[1]);
  EXPECT_EQ(1.0f, f4[2]);
  EXPECT_EQ(0.0f, f4[3]);

  GM_CALL(ClearDepthf(0.5f));
  EXPECT_EQ(0.5f, GetFloat(gm, GL_DEPTH_CLEAR_VALUE));
  GM_CALL(ClearDepthf(1.5f));  // Should clamp.
  EXPECT_EQ(1.0f, GetFloat(gm, GL_DEPTH_CLEAR_VALUE));

  GM_CALL(ColorMask(true, false, false, true));
  GM_CALL(GetIntegerv(GL_COLOR_WRITEMASK, i4));
  EXPECT_EQ(GL_TRUE, i4[0]);
  EXPECT_EQ(GL_FALSE, i4[1]);
  EXPECT_EQ(GL_FALSE, i4[2]);
  EXPECT_EQ(GL_TRUE, i4[3]);

  GM_CALL(CullFace(GL_FRONT_AND_BACK));
  EXPECT_EQ(GL_FRONT_AND_BACK, GetInt(gm, GL_CULL_FACE_MODE));

  GM_CALL(DepthFunc(GL_GEQUAL));
  EXPECT_EQ(GL_GEQUAL, GetInt(gm, GL_DEPTH_FUNC));

  GM_CALL(DepthRangef(0.2f, 0.7f));
  GM_CALL(GetFloatv(GL_DEPTH_RANGE, f4));
  EXPECT_EQ(0.2f, f4[0]);
  EXPECT_EQ(0.7f, f4[1]);
  GM_CALL(DepthRangef(-0.1f, 1.1f));  // Should clamp.
  GM_CALL(GetFloatv(GL_DEPTH_RANGE, f4));
  EXPECT_EQ(0.0f, f4[0]);
  EXPECT_EQ(1.0f, f4[1]);

  GM_CALL(DepthMask(false));
  EXPECT_EQ(GL_FALSE, GetInt(gm, GL_DEPTH_WRITEMASK));

  GM_CALL(FrontFace(GL_CW));
  EXPECT_EQ(GL_CW, GetInt(gm, GL_FRONT_FACE));

  {
    // Hints are not available on all platforms; ignore error messages.
    base::LogChecker log_checker;
    GM_ERROR_CALL(Hint(GL_ARRAY_BUFFER, GL_FASTEST), GL_INVALID_ENUM);
    GM_ERROR_CALL(Hint(GL_GENERATE_MIPMAP_HINT, GL_BLEND), GL_INVALID_ENUM);
    GM_CALL(Hint(GL_GENERATE_MIPMAP_HINT, GL_NICEST));
    EXPECT_EQ(GL_NICEST, GetInt(gm, GL_GENERATE_MIPMAP_HINT));
    log_checker.ClearLog();
  }

  GM_CALL(PixelStorei(GL_PACK_ALIGNMENT, 2));
  EXPECT_EQ(2, GetInt(gm, GL_PACK_ALIGNMENT));
  EXPECT_EQ(4, GetInt(gm, GL_UNPACK_ALIGNMENT));
  GM_CALL(PixelStorei(GL_UNPACK_ALIGNMENT, 8));
  EXPECT_EQ(2, GetInt(gm, GL_PACK_ALIGNMENT));
  EXPECT_EQ(8, GetInt(gm, GL_UNPACK_ALIGNMENT));

  GM_CALL(LineWidth(2.18f));
  EXPECT_EQ(2.18f, GetFloat(gm, GL_LINE_WIDTH));

  GM_CALL(PointSize(3.14f));
  EXPECT_EQ(3.14f, GetFloat(gm, GL_POINT_SIZE));

  GM_CALL(PolygonOffset(0.4f, 0.2f));
  EXPECT_EQ(0.4f, GetFloat(gm, GL_POLYGON_OFFSET_FACTOR));
  EXPECT_EQ(0.2f, GetFloat(gm, GL_POLYGON_OFFSET_UNITS));

  GM_CALL(SampleCoverage(0.5f, true));
  EXPECT_EQ(0.5f, GetFloat(gm, GL_SAMPLE_COVERAGE_VALUE));
  EXPECT_EQ(GL_TRUE, GetInt(gm, GL_SAMPLE_COVERAGE_INVERT));
  GM_CALL(SampleCoverage(1.2f, false));  // Should clamp.
  EXPECT_EQ(1.0f, GetFloat(gm, GL_SAMPLE_COVERAGE_VALUE));
  EXPECT_EQ(GL_FALSE, GetInt(gm, GL_SAMPLE_COVERAGE_INVERT));

  GM_CALL(Scissor(4, 10, 123, 234));
  GM_CALL(GetIntegerv(GL_SCISSOR_BOX, i4));
  EXPECT_EQ(4, i4[0]);
  EXPECT_EQ(10, i4[1]);
  EXPECT_EQ(123, i4[2]);
  EXPECT_EQ(234, i4[3]);

  GM_CALL(StencilFuncSeparate(GL_FRONT, GL_LEQUAL, 100, 0xbeefbeefU));
  EXPECT_EQ(GL_LEQUAL, GetInt(gm, GL_STENCIL_FUNC));
  EXPECT_EQ(100, GetInt(gm, GL_STENCIL_REF));
  EXPECT_EQ(0xbeefbeefU, GetMask(gm, GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(GL_ALWAYS, GetInt(gm, GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BACK_REF));
  EXPECT_EQ(0xffffffffU, GetMask(gm, GL_STENCIL_BACK_VALUE_MASK));
  GM_CALL(StencilFuncSeparate(GL_BACK, GL_GREATER, 200, 0xfacefaceU));
  EXPECT_EQ(GL_LEQUAL, GetInt(gm, GL_STENCIL_FUNC));
  EXPECT_EQ(100, GetInt(gm, GL_STENCIL_REF));
  EXPECT_EQ(0xbeefbeefU, GetMask(gm, GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(GL_GREATER, GetInt(gm, GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(200, GetInt(gm, GL_STENCIL_BACK_REF));
  EXPECT_EQ(0xfacefaceU, GetMask(gm, GL_STENCIL_BACK_VALUE_MASK));
  GM_CALL(
      StencilFuncSeparate(GL_FRONT_AND_BACK, GL_NOTEQUAL, 300, 0xbebebebeU));
  EXPECT_EQ(GL_NOTEQUAL, GetInt(gm, GL_STENCIL_FUNC));
  EXPECT_EQ(300, GetInt(gm, GL_STENCIL_REF));
  EXPECT_EQ(0xbebebebeU, GetMask(gm, GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(GL_NOTEQUAL, GetInt(gm, GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(300, GetInt(gm, GL_STENCIL_BACK_REF));
  EXPECT_EQ(0xbebebebeU, GetMask(gm, GL_STENCIL_BACK_VALUE_MASK));
  GM_CALL(StencilFunc(GL_LESS, 400, 0x20304050U));
  EXPECT_EQ(GL_LESS, GetInt(gm, GL_STENCIL_FUNC));
  EXPECT_EQ(400, GetInt(gm, GL_STENCIL_REF));
  EXPECT_EQ(0x20304050U, GetMask(gm, GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(GL_LESS, GetInt(gm, GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(400, GetInt(gm, GL_STENCIL_BACK_REF));
  EXPECT_EQ(0x20304050U, GetMask(gm, GL_STENCIL_BACK_VALUE_MASK));

  GM_CALL(StencilMaskSeparate(GL_FRONT, 0xdeadfaceU));
  EXPECT_EQ(0xdeadfaceU, GetMask(gm, GL_STENCIL_WRITEMASK));
  EXPECT_EQ(0xffffffffU, GetMask(gm, GL_STENCIL_BACK_WRITEMASK));
  GM_CALL(StencilMaskSeparate(GL_BACK, 0xcacabeadU));
  EXPECT_EQ(0xdeadfaceU, GetMask(gm, GL_STENCIL_WRITEMASK));
  EXPECT_EQ(0xcacabeadU, GetMask(gm, GL_STENCIL_BACK_WRITEMASK));
  GM_CALL(StencilMaskSeparate(GL_FRONT_AND_BACK, 0x87654321U));
  EXPECT_EQ(0x87654321U, GetMask(gm, GL_STENCIL_WRITEMASK));
  EXPECT_EQ(0x87654321U, GetMask(gm, GL_STENCIL_BACK_WRITEMASK));
  GM_CALL(StencilMask(0x24681359U));
  EXPECT_EQ(0x24681359U, GetMask(gm, GL_STENCIL_WRITEMASK));
  EXPECT_EQ(0x24681359U, GetMask(gm, GL_STENCIL_BACK_WRITEMASK));

  GM_CALL(StencilOpSeparate(GL_FRONT, GL_REPLACE, GL_INCR, GL_INVERT));
  EXPECT_EQ(GL_REPLACE, GetInt(gm, GL_STENCIL_FAIL));
  EXPECT_EQ(GL_INCR, GetInt(gm, GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_INVERT, GetInt(gm, GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_PASS));
  GM_CALL(StencilOpSeparate(GL_BACK, GL_INCR_WRAP, GL_DECR_WRAP, GL_ZERO));
  EXPECT_EQ(GL_REPLACE, GetInt(gm, GL_STENCIL_FAIL));
  EXPECT_EQ(GL_INCR, GetInt(gm, GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_INVERT, GetInt(gm, GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(GL_INCR_WRAP, GetInt(gm, GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_DECR_WRAP, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_ZERO, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_PASS));
  GM_CALL(StencilOpSeparate(GL_FRONT_AND_BACK, GL_ZERO, GL_KEEP, GL_DECR));
  EXPECT_EQ(GL_ZERO, GetInt(gm, GL_STENCIL_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_DECR, GetInt(gm, GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(GL_ZERO, GetInt(gm, GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_DECR, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_PASS));
  GM_CALL(StencilOp(GL_INCR, GL_DECR, GL_INVERT));
  EXPECT_EQ(GL_INCR, GetInt(gm, GL_STENCIL_FAIL));
  EXPECT_EQ(GL_DECR, GetInt(gm, GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_INVERT, GetInt(gm, GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(GL_INCR, GetInt(gm, GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_DECR, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_INVERT, GetInt(gm, GL_STENCIL_BACK_PASS_DEPTH_PASS));

  GM_CALL(ClearStencil(123));
  EXPECT_EQ(123, GetInt(gm, GL_STENCIL_CLEAR_VALUE));

  GM_CALL(Viewport(16, 49, 220, 317));
  GM_CALL(GetIntegerv(GL_VIEWPORT, i4));
  EXPECT_EQ(16, i4[0]);
  EXPECT_EQ(49, i4[1]);
  EXPECT_EQ(220, i4[2]);
  EXPECT_EQ(317, i4[3]);
}

TEST(MockGraphicsManagerTest, BindTexture_ActiveTexture) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[5];
  memset(ids, 0, sizeof(ids));
  GM_ERROR_CALL(GenTextures(-1, ids), GL_INVALID_VALUE);
  EXPECT_EQ(0U, ids[0]);
  EXPECT_EQ(0U, ids[1]);
  GM_CALL(GenTextures(5, ids));
  EXPECT_NE(0U, ids[0]);
  EXPECT_NE(0U, ids[1]);
  EXPECT_NE(0U, ids[2]);
  EXPECT_NE(0U, ids[3]);
  EXPECT_NE(0U, ids[4]);
  EXPECT_EQ(GL_TRUE, gm->IsTexture(0U));
  EXPECT_EQ(GL_TRUE, gm->IsTexture(ids[3]));
  EXPECT_EQ(GL_TRUE, gm->IsTexture(ids[4]));
  EXPECT_EQ(GL_FALSE, gm->IsTexture(ids[3] + ids[4]));

  GLuint max_units = static_cast<GLuint>(
      GetInt(gm, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
  EXPECT_GT(max_units, 0U);

  // Test bad texture unit ids.
  GM_ERROR_CALL(ActiveTexture(GL_TEXTURE0 + max_units), GL_INVALID_ENUM);
  GM_ERROR_CALL(ActiveTexture(GL_TEXTURE0 + max_units + 1U), GL_INVALID_ENUM);
  GM_ERROR_CALL(ActiveTexture(GL_TEXTURE0 + max_units + 10U), GL_INVALID_ENUM);
  GM_ERROR_CALL(ActiveTexture(GL_TEXTURE0 + max_units + 100U), GL_INVALID_ENUM);
  GM_ERROR_CALL(ActiveTexture(GL_TEXTURE0 - 1U), GL_INVALID_ENUM);
  GM_ERROR_CALL(ActiveTexture(GL_TEXTURE0 - 10U), GL_INVALID_ENUM);
  GM_ERROR_CALL(ActiveTexture(GL_TEXTURE0 - 10U), GL_INVALID_ENUM);

  // Default texture unit is 0.
  EXPECT_EQ(GL_TEXTURE0, GetInt(gm, GL_ACTIVE_TEXTURE));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  EXPECT_EQ(GL_TEXTURE4, GetInt(gm, GL_ACTIVE_TEXTURE));

  // Bad binds.
  GM_ERROR_CALL(BindTexture(GL_BACK, ids[0]), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindTexture(GL_TEXTURE_2D, 24U), GL_INVALID_VALUE);
  // Good binds.
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_2D));
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[0]));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(gm, GL_TEXTURE_BINDING_2D));

  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, ids[1]));
  EXPECT_EQ(static_cast<int>(ids[1]), GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));

  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_1D_ARRAY));
  GM_CALL(BindTexture(GL_TEXTURE_1D_ARRAY, ids[2]));
  EXPECT_EQ(static_cast<int>(ids[2]), GetInt(gm, GL_TEXTURE_BINDING_1D_ARRAY));

  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_2D_ARRAY));
  GM_CALL(BindTexture(GL_TEXTURE_2D_ARRAY, ids[3]));
  EXPECT_EQ(static_cast<int>(ids[3]), GetInt(gm, GL_TEXTURE_BINDING_2D_ARRAY));

  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP_ARRAY));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, ids[4]));
  EXPECT_EQ(static_cast<int>(ids[4]),
            GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP_ARRAY));

  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_EXTERNAL_OES));
  GM_CALL(BindTexture(GL_TEXTURE_EXTERNAL_OES, ids[3]));
  EXPECT_EQ(static_cast<int>(ids[3]),
            GetInt(gm, GL_TEXTURE_BINDING_EXTERNAL_OES));

  // Check that the texture binding is correct and follows the active image
  // unit.
  GM_CALL(ActiveTexture(GL_TEXTURE2));
  // Unit 2 is empty.
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  // Bind textures to unit 4.
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(ids[1]), GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  GLuint more_ids[2];
  more_ids[0] = more_ids[1] = 0U;
  GM_CALL(GenTextures(2, more_ids));
  // Bind textures to unit 5.
  GM_CALL(ActiveTexture(GL_TEXTURE5));
  GM_CALL(BindTexture(GL_TEXTURE_2D, more_ids[0]));
  EXPECT_EQ(static_cast<int>(more_ids[0]), GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, more_ids[1]));
  EXPECT_EQ(static_cast<int>(more_ids[0]), GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(more_ids[1]),
            GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  // Unit 2 should still be empty.
  GM_CALL(ActiveTexture(GL_TEXTURE2));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  // Unit 3 should be empty.
  GM_CALL(ActiveTexture(GL_TEXTURE3));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  // Units 4 and 5 should have the right bindings.
  GM_CALL(ActiveTexture(GL_TEXTURE5));
  EXPECT_EQ(static_cast<int>(more_ids[0]), GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(more_ids[1]),
            GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(ids[1]), GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  // Deleting the new textures should clear their binding.
  GM_CALL(DeleteTextures(2, more_ids));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(ids[1]), GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(ActiveTexture(GL_TEXTURE5));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(ActiveTexture(GL_TEXTURE4));

  // Delete textures.
  GLuint bad_id = 5U;
  // Error if n < 0.
  GM_ERROR_CALL(DeleteTextures(-1, ids), GL_INVALID_VALUE);
  // Bad ids are silently ignored.
  GM_CALL(DeleteTextures(1, &bad_id));
  // Actually delete the ids.
  GM_CALL(DeleteTextures(2, ids));
  EXPECT_FALSE(gm->IsTexture(ids[0]));
  EXPECT_FALSE(gm->IsTexture(ids[1]));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  // Bind should fail on a deleted texture.
  GM_ERROR_CALL(BindTexture(GL_TEXTURE_2D, ids[0]), GL_INVALID_VALUE);
}

TEST(MockGraphicsManagerTest, TexParameter) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, ids[1]));

  // Check errors.
  GM_ERROR_CALL(TexParameterf(GL_CULL_FACE, GL_TEXTURE_MIN_FILTER, GL_NEAREST),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameterf(GL_TEXTURE_2D, GL_NEAREST, GL_REPEAT),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_FRONT),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_LESS),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_SAMPLER),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RGBA),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_DITHER),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_BLEND),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_BACK),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_DEPTH_TEST),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_LINEAR),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_CUBE_MAP, GL_NEAREST, GL_REPEAT),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_FRONT),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(
      TexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_LINEAR),
      GL_INVALID_ENUM);
  // Anisotropic features.
  GM_ERROR_CALL(
      TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.9f),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 999.f),
      GL_INVALID_VALUE);

  // Mag filter cannot use mipmapping modes.
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                              GL_NEAREST_MIPMAP_LINEAR),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                              GL_LINEAR_MIPMAP_LINEAR),
                GL_INVALID_ENUM);

  // Check default texture modes.
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_R));
  EXPECT_EQ(GL_REPEAT, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_FALSE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT));
  // Error if an invalid enum is used.
  GM_ERROR_CALL(
      GetTexParameteriv(GL_TEXTURE_2D, GL_VERTEX_ATTRIB_ARRAY_SIZE, NULL),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetTexParameterfv(GL_TEXTURE_2D, GL_VERTEX_ATTRIB_ARRAY_SIZE, NULL),
      GL_INVALID_ENUM);

  // Check that changes happen.
  GLint mode = GL_CLAMP_TO_EDGE;
  GM_CALL(TexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &mode));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  GLfloat modef = GL_MIRRORED_REPEAT;
  GM_CALL(TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &modef));
  EXPECT_EQ(static_cast<GLfloat>(GL_MIRRORED_REPEAT),
            GetTextureFloat(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2));
  EXPECT_EQ(2, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 200));
  EXPECT_EQ(200, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL));
  GM_CALL(TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 3.14f));
  EXPECT_EQ(3.14f, GetTextureFloat(gm, GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD));
  GM_CALL(TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 2.18f));
  EXPECT_EQ(2.18f, GetTextureFloat(gm, GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS));
  EXPECT_EQ(GL_ALWAYS,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                        GL_COMPARE_REF_TO_TEXTURE));
  EXPECT_EQ(GL_COMPARE_REF_TO_TEXTURE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE));
  GM_CALL(TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 3.f));
  EXPECT_EQ(3.f,
            GetTextureFloat(gm, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT));

  // Check that changes affect only the proper parameter.
  EXPECT_EQ(GL_REPEAT, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  // Check that cube map settings have not changed.
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(gm, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR,
            GetTextureInt(gm, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT,
            GetTextureInt(gm, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT,
            GetTextureInt(gm, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T));

  // Check that texture state is saved over a bind.
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[1]));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(gm, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR,
            GetTextureInt(gm, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT,
            GetTextureInt(gm, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT,
            GetTextureInt(gm, GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T));

  // Check that original values are restored.
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[0]));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  // Delete textures.
  GLuint bad_id = 5U;
  // Error if n < 0.
  GM_ERROR_CALL(DeleteTextures(-1, ids), GL_INVALID_VALUE);
  // Bad ids are silently ignored.
  GM_CALL(DeleteTextures(1, &bad_id));
  // Actually delete the ids.
  GM_CALL(DeleteTextures(2, ids));
  EXPECT_FALSE(gm->IsTexture(ids[0]));
  EXPECT_FALSE(gm->IsTexture(ids[1]));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(gm, GL_TEXTURE_BINDING_CUBE_MAP));
  // Bind should fail on a deleted texture.
  GM_ERROR_CALL(BindTexture(GL_TEXTURE_2D, ids[0]), GL_INVALID_VALUE);
}

TEST(MockGraphicsManagerTest, TexImage2D_GenerateMipmap) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, ids[1]));

  // TexImage2D.
  GLint level = 0;
  GLint internal_format = GL_RGBA;
  GLsizei width = 128;
  GLsizei height = 128;
  GLint border = 0;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  // Error calls.
  GM_ERROR_CALL(TexImage2D(GL_REPEAT, level, internal_format, width, height,
                           border, format, type, NULL),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, -1, internal_format, width, height,
                           border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, 0, GL_TEXTURE_MIN_FILTER, width,
                           height, border, format, type, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, -1, height,
                           border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, -1,
                           border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, height,
                           2, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, border,
                           GL_RGBA, type, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, height,
                           border, format, GL_INCR, NULL),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, border,
                           GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, border,
                           GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, NULL),
                GL_INVALID_OPERATION);
  // Large textures should fail.
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, 65537, height,
                           border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, 65537,
                           border, format, type, NULL),
                GL_INVALID_VALUE);
  // Cube map requires an axis enum.
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_CUBE_MAP, level, internal_format, width,
                           128, border, format, type, NULL),
                GL_INVALID_ENUM);
  // Dimensions must be equal for cube maps.
  GM_ERROR_CALL(
      TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, level, internal_format, width,
                 256, border, format, type, NULL),
      GL_INVALID_VALUE);

  // Successful calls.
  GM_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, height,
                     border, format, type, NULL));
  GM_CALL(TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format,
                     width, height, border, format, type, NULL));

  // Mipmaps.
  // Bad target.
  GM_ERROR_CALL(GenerateMipmap(GL_VERTEX_SHADER), GL_INVALID_ENUM);
  GM_CALL(GenerateMipmap(GL_TEXTURE_2D));
  // Dimensions must be powers of two to generate mipmaps.
  GM_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, 100, 100, border,
                     format, type, NULL));
  GM_ERROR_CALL(GenerateMipmap(GL_TEXTURE_2D), GL_INVALID_OPERATION);
}

TEST(MockGraphicsManagerTest, TexImage3D) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  GM_CALL(BindTexture(GL_TEXTURE_3D, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, ids[1]));

  // TexImage2D.
  const GLint level = 0;
  const GLint internal_format = GL_RGBA;
  const GLsizei width = 128;
  const GLsizei height = 128;
  const GLsizei depth = 128;
  const GLint border = 0;
  const GLenum format = GL_RGBA;
  const GLenum type = GL_UNSIGNED_BYTE;
  // Error calls.
  GM_ERROR_CALL(TexImage3D(GL_REPEAT, level, internal_format, width, height,
                           depth, border, format, type, NULL),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, -1, internal_format, width, height,
                           depth, border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, 0, GL_TEXTURE_MIN_FILTER, width,
                           height, depth, border, format, type, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, -1, height,
                           depth, border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, -1,
                           depth, border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           -1, border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           depth, 2, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, GL_RGB, width, height, depth,
                           border, GL_RGBA, type, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           depth, border, format, GL_INCR, NULL),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, GL_RGBA, width, height, depth,
                           border, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, GL_RGB, width, height, depth,
                           border, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, NULL),
                GL_INVALID_OPERATION);
  // Large textures should fail.
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, 65537, height,
                           depth, border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, 65537,
                           depth, border, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           65537, border, format, type, NULL),
                GL_INVALID_VALUE);
  // Dimensions must be equal for cube map arrays.
  GM_ERROR_CALL(
      TexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format, width,
                 height / 2, depth, border, format, type, NULL),
      GL_INVALID_VALUE);

  // Successful calls.
  GM_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                     depth, border, format, type, NULL));
  // The number of cubemap layers doesn't have to be the same as the dimensions.
  GM_CALL(TexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                     width, height, width / 2, border, format, type, NULL));
}

TEST(MockGraphicsManagerTest, TexSubImage2D_CopyTexImage2D_CopyTexSubImage2D) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, ids[1]));

  // Setup.
  GLint level = 0;
  const GLint internal_format = GL_RGBA;
  GLsizei width = 128;
  GLsizei height = 128;
  const GLint border = 0;
  const GLenum format = GL_RGBA;
  const GLenum type = GL_UNSIGNED_BYTE;
  GM_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, height,
                     border, format, type, NULL));
  GM_CALL(TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format,
                     width, height, border, format, type, NULL));

  const GLint xoffset = 64;
  const GLint yoffset = 64;
  const GLint x = 64;
  const GLint y = 64;
  width = height = 63;
  // TexSubImage2D.
  GM_ERROR_CALL(TexSubImage2D(GL_DEPTH_TEST, level, xoffset, yoffset, width,
                              height, format, type, NULL),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, -1, xoffset, yoffset, width,
                              height, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, -1, yoffset, width, height,
                              format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, -1, width, height,
                              format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, -1,
                              height, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, -1,
                              format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                              height, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                              height, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, 1024, yoffset, width,
                              height, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, 1024, width,
                              height, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, 1024,
                              height, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                              1024, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                              height, GL_RGB, type, NULL),
                GL_INVALID_OPERATION);
  GM_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height,
                        format, type, NULL));

  // CopyTexImage2D.
  GM_ERROR_CALL(CopyTexImage2D(GL_BLEND_COLOR, level, internal_format, x, y,
                               width, height, border),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(CopyTexImage2D(GL_TEXTURE_2D, -1, internal_format, x, y, width,
                               height, border),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexImage2D(GL_TEXTURE_2D, level, GL_STENCIL_TEST, x, y,
                               width, height, border),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(CopyTexImage2D(GL_TEXTURE_2D, level, internal_format, x, y, -1,
                               height, border),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexImage2D(GL_TEXTURE_2D, level, internal_format, x, y,
                               width, -1, border),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, level,
                               internal_format, x, y, width, 32, border),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexImage2D(GL_TEXTURE_2D, level, internal_format, x, y,
                               width, height, -1),
                GL_INVALID_VALUE);
  level = 1;
  GM_ERROR_CALL(CopyTexImage2D(GL_TEXTURE_2D, level, internal_format, x, y,
                               width, height, 1),
                GL_INVALID_VALUE);
  GM_CALL(CopyTexImage2D(GL_TEXTURE_2D, level, internal_format, x, y, width,
                         height, border));

  // CopyTexSubImage2D.
  // Error calls.
  GM_ERROR_CALL(CopyTexSubImage2D(GL_REPEAT, level, xoffset, yoffset, x, y,
                                  width, height),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(CopyTexSubImage2D(GL_TEXTURE_2D, -1, xoffset, yoffset, x, y,
                                  width, height),
                GL_INVALID_VALUE);
  GM_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, 128, 128, border,
                     format, type, reinterpret_cast<const GLvoid*>(16)));
  GM_ERROR_CALL(
      CopyTexSubImage2D(GL_TEXTURE_2D, level, -1, yoffset, x, y, width, height),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, -1, x, y, width, height),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y,
                                  -1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y,
                                  width, -1),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y,
                                  1024, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y,
                                  width, 1024),
                GL_INVALID_VALUE);
  GM_CALL(ActiveTexture(GL_TEXTURE2));
  GM_ERROR_CALL(CopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y,
                                  width, height),
                GL_INVALID_OPERATION);
  GM_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, 128, 128, border,
                     format, type, reinterpret_cast<const GLvoid*>(16)));
  GM_CALL(CopyTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, x, y, width,
                            height));
}

TEST(MockGraphicsManagerTest, TexSubImage3D_CopyTexSubImage3D) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(BindTexture(GL_TEXTURE_3D, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, ids[1]));

  // Setup.
  GLint level = 0;
  const GLint internal_format = GL_RGBA;
  GLsizei width = 128;
  GLsizei height = 128;
  GLsizei depth = 128;
  const GLint border = 0;
  const GLenum format = GL_RGBA;
  const GLenum type = GL_UNSIGNED_BYTE;
  GM_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                     depth, border, format, type, NULL));
  GM_ERROR_CALL(
      TexImage3D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format, width,
                 height, depth, border, format, type, NULL),
      GL_INVALID_ENUM);
  GM_CALL(TexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                     width, height, depth, border, format, type, NULL));

  const GLint xoffset = 64;
  const GLint yoffset = 64;
  const GLint zoffset = 64;
  const GLint x = 64;
  const GLint y = 64;
  width = height = depth = 63;
  // TexSubImage3D.
  // Invalid target.
  GM_ERROR_CALL(TexSubImage3D(GL_DEPTH_TEST, level, xoffset, yoffset, zoffset,
                              width, height, depth, format, type, NULL),
                GL_INVALID_ENUM);
  // Invalid level.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, -1, xoffset, yoffset, zoffset,
                              width, height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  // Invalid offsets.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, -1, yoffset, zoffset, width,
                              height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, -1, zoffset, width,
                              height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, -1, width,
                              height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  // Invalid dimensions.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              -1, height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, -1, depth, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, -1, format, type, NULL),
                GL_INVALID_VALUE);
  // Invalid formats.
  GM_ERROR_CALL(
      TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width,
                    height, depth, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, NULL),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width,
                    height, depth, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, NULL),
      GL_INVALID_OPERATION);
  // Invalid offsets.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, 1024, yoffset, zoffset,
                              width, height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, 1024, zoffset,
                              width, height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, 1024,
                              width, height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  // Invalid dimensions.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              1024, height, depth, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, 1024, depth, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, 1024, format, type, NULL),
                GL_INVALID_VALUE);
  // Invalid format.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, depth, GL_RGB, type, NULL),
                GL_INVALID_OPERATION);
  GM_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width,
                        height, depth, format, type, NULL));

  // CopyTexSubImage3D.
  level = 1;
  // Invalid target.
  GM_ERROR_CALL(CopyTexSubImage3D(GL_REPEAT, level, xoffset, yoffset, zoffset,
                                  x, y, width, height),
                GL_INVALID_ENUM);
  // Invalid level.
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, -1, xoffset, yoffset, zoffset,
                                  x, y, width, height),
                GL_INVALID_VALUE);
  // Create a valid texture.
  GM_CALL(
      TexImage3D(GL_TEXTURE_3D, level, internal_format, 128, 128, 128, border,
                 format, type, reinterpret_cast<const GLvoid*>(16)));
  // Invalid offsets.
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, -1, yoffset, zoffset, x,
                                  y, width, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, -1, zoffset, x,
                                  y, width, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, -1, x,
                                  y, width, height),
                GL_INVALID_VALUE);
  // Invalid dimensions.
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset,
                                  zoffset, x, y, -1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset,
                                  zoffset, x, y, width, -1),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset,
                                  zoffset, x, y, 1024, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset,
                                  zoffset, x, y, width, 1024),
                GL_INVALID_VALUE);
  // Make a different unit active so that the target.
  GM_CALL(ActiveTexture(GL_TEXTURE2));
  GM_ERROR_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset,
                                  zoffset, x, y, width, height),
                GL_INVALID_OPERATION);
  // Create a valid texture so that the copy will succeed.
  GM_CALL(
      TexImage3D(GL_TEXTURE_3D, level, internal_format, 128, 128, 128, border,
                 format, type, reinterpret_cast<const GLvoid*>(16)));
  GM_CALL(CopyTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, x,
                            y, width, height));
}

TEST(MockGraphicsManagerTest, CompressedTexImage2D_CompressedTexSubImage2D) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, ids[1]));

  // CompressedTexImage2D
  const GLint level = 0;
  const GLint internal_format = GL_ETC1_RGB8_OES;
  GLsizei width = 64;
  GLsizei height = 64;
  const GLint border = 0;
  GLenum format = GL_RGBA;
  GLint xoffset = 64;
  GLint yoffset = 64;
  width = height = 63;

  // Invalid target.
  const GLsizei image_size = 1024;
  GM_ERROR_CALL(CompressedTexImage2D(GL_REPEAT, level, internal_format, width,
                                     height, border, image_size, NULL),
                GL_INVALID_ENUM);
  // Invalid level.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, -1, internal_format, width,
                                     height, border, image_size, NULL),
                GL_INVALID_VALUE);
  // Invalid format.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, 0, GL_TEXTURE_MIN_FILTER,
                                     width, height, border, image_size, NULL),
                GL_INVALID_ENUM);
  // Invalid dimensions.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format, -1,
                                     height, border, image_size, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format,
                                     width, -1, border, image_size, NULL),
                GL_INVALID_VALUE);
  // Invalid size.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format,
                                     width, height, border, -1, NULL),
                GL_INVALID_VALUE);
  // Large textures.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format,
                                     65537, height, border, image_size, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format,
                                     width, 65537, border, image_size, NULL),
                GL_INVALID_VALUE);
  // Cube map requires an axis enum.
  GM_ERROR_CALL(
      CompressedTexImage2D(GL_TEXTURE_CUBE_MAP, level, internal_format, width,
                           128, border, image_size, NULL),
      GL_INVALID_ENUM);
  // Successful calls.
  GM_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format, width,
                               height, border, image_size, NULL));
  GM_CALL(CompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level,
                               internal_format, width, height, border,
                               image_size, NULL));

  // CompressedTexSubImage2D.
  format = GL_ETC1_RGB8_OES;
  width = height = 16;
  xoffset = yoffset = 16;
  GM_ERROR_CALL(
      CompressedTexSubImage2D(GL_INVALID_ENUM, level, xoffset, yoffset, width,
                              height, format, image_size, NULL),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      CompressedTexSubImage2D(GL_TEXTURE_2D, -1, xoffset, yoffset, width,
                              height, format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage2D(GL_TEXTURE_2D, level, -1, yoffset, width, height,
                              format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, -1, width, height,
                              format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset,
                                        -1, height, format, image_size, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset,
                                        width, -1, format, image_size, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset,
                                        width, height, GL_RGBA, -1, NULL),
                GL_INVALID_ENUM);
  GM_CALL(CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                                  height, format, image_size, NULL));
}

TEST(MockGraphicsManagerTest, CompressedTexImage3D_CompressedTexSubImage3D) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  GM_CALL(BindTexture(GL_TEXTURE_3D, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, ids[1]));

  // CompressedTexImage3D
  const GLint level = 0;
  const GLint internal_format = GL_ETC1_RGB8_OES;
  GLsizei width = 64;
  GLsizei height = 64;
  GLsizei depth = 64;
  const GLint border = 0;
  GLenum format = GL_RGBA;
  GLint xoffset = 64;
  GLint yoffset = 64;
  GLint zoffset = 64;

  const GLsizei image_size = 1024;
  // Invalid target.
  GM_ERROR_CALL(CompressedTexImage3D(GL_REPEAT, level, internal_format, width,
                                     height, depth, border, image_size, NULL),
                GL_INVALID_ENUM);
  // Invalid level.
  GM_ERROR_CALL(CompressedTexImage3D(GL_TEXTURE_3D, -1, internal_format, width,
                                     height, depth, border, image_size, NULL),
                GL_INVALID_VALUE);
  // Invalid format.
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, 0, GL_TEXTURE_MIN_FILTER, width,
                           height, depth, border, image_size, NULL),
      GL_INVALID_ENUM);
  // Invalid dimensions.
  GM_ERROR_CALL(CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, -1,
                                     height, depth, border, image_size, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, -1,
                           depth, border, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           -1, border, image_size, NULL),
      GL_INVALID_VALUE);
  // Invalid border.
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           depth, 1, image_size, NULL),
      GL_INVALID_VALUE);
  // Invalid size.
  GM_ERROR_CALL(CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format,
                                     width, height, depth, border, -1, NULL),
                GL_INVALID_VALUE);
  // Large textures.
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, 65537, height,
                           depth, border, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, 65537,
                           depth, border, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           65537, border, image_size, NULL),
      GL_INVALID_VALUE);
  // Cube map dimensions must be equal.
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                           width, width / 2, depth, border, image_size, NULL),
      GL_INVALID_VALUE);
  // It's ok to have a different depth for an array, however.
  GM_CALL(
      CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                           width, height, width / 2, border, image_size, NULL));
  // Other calls.
  GM_CALL(CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width,
                               height, depth, border, image_size, NULL));
  GM_ERROR_CALL(CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level,
                                     internal_format, width, height, depth,
                                     border, image_size, NULL),
                GL_INVALID_ENUM);
  GM_CALL(
      CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                           width, height, depth, border, image_size, NULL));

  // CompressedTexSubImage3D.
  format = GL_ETC1_RGB8_OES;
  width = height = depth = 16;
  xoffset = yoffset = zoffset = 16;
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_INVALID_ENUM, level, xoffset, yoffset, zoffset,
                              width, height, depth, format, image_size, NULL),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, -1, xoffset, yoffset, zoffset,
                              width, height, depth, format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, -1, yoffset, zoffset, width,
                              height, depth, format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, -1, zoffset, width,
                              height, depth, format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, -1, width,
                              height, depth, format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              -1, height, depth, format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, -1, depth, format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, -1, format, image_size, NULL),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, depth, GL_RGBA, -1, NULL),
      GL_INVALID_ENUM);
  GM_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, depth, format, image_size, NULL));
}

TEST(MockGraphicsManagerTest, TexImage2DMultisample) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE, ids[1]));

  // Parameters.
  GLsizei samples = 4;
  GLint internal_format = GL_RGBA;
  GLsizei width = 128;
  GLsizei height = 128;
  GLboolean fixed_sample_locations = false;

  // Invalid target.
  GM_ERROR_CALL(
      TexImage2DMultisample(GL_REPEAT, samples, internal_format, width, height,
                            fixed_sample_locations),
      GL_INVALID_ENUM);
  // Invalid samples.
  GM_ERROR_CALL(
      TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 19, internal_format,
                            width, height, fixed_sample_locations),
      GL_INVALID_OPERATION);
  // Invalid internal format.
  GM_ERROR_CALL(
      TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, 0, width,
                            height, fixed_sample_locations),
      GL_INVALID_ENUM);

  // Large textures should fail.
  GM_ERROR_CALL(
      TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internal_format,
                            65537, height, fixed_sample_locations),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internal_format,
                            width, 65537, fixed_sample_locations),
      GL_INVALID_VALUE);

  // Successful call.
  GM_CALL(
      TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internal_format,
                            width, height, fixed_sample_locations));
}

TEST(MockGraphicsManagerTest, TexImage3DMultisample) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_CALL(GenTextures(2, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE, ids[1]));

  // Parameters.
  GLsizei samples = 4;
  GLint internal_format = GL_RGBA;
  GLsizei width = 128;
  GLsizei height = 128;
  GLsizei depth = 128;
  GLboolean fixed_sample_locations = false;

  // Invalid target.
  GM_ERROR_CALL(
      TexImage3DMultisample(GL_REPEAT, samples, internal_format, width, height,
                            depth, fixed_sample_locations),
      GL_INVALID_ENUM);
  // Invalid samples.
  GM_ERROR_CALL(
      TexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 19,
                            internal_format, width, height, depth,
                            fixed_sample_locations),
      GL_INVALID_OPERATION);
  // Invalid internal format.
  GM_ERROR_CALL(
      TexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples, 0, width,
                            height, depth, fixed_sample_locations),
      GL_INVALID_ENUM);

  // Large textures should fail.
  GM_ERROR_CALL(
      TexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples,
                            internal_format, 65537, height, depth,
                            fixed_sample_locations),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      TexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples,
                            internal_format, width, 65537, depth,
                            fixed_sample_locations),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      TexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples,
                            internal_format, width, height, 65537,
                            fixed_sample_locations),
      GL_INVALID_VALUE);

  // Successful call.
  GM_CALL(
      TexImage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples,
                            internal_format, width, height, depth,
                            fixed_sample_locations));
}

TEST(MockGraphicsManagerTest, GetMultisamplefv) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLenum pname = GL_SAMPLE_POSITION;
  GLuint index = 0;
  GLfloat val[2];

  // Invalid position name, assert doesn't change 'val'.
  val[0] = 19.0f;
  val[1] = 19.0f;
  GM_ERROR_CALL(GetMultisamplefv(GL_REPEAT, index, val), GL_INVALID_ENUM);
  EXPECT_EQ(19.0f, val[0]);
  EXPECT_EQ(19.0f, val[1]);

  // No active texture.
  GM_ERROR_CALL(GetMultisamplefv(pname, index, val), GL_INVALID_OPERATION);

  // Create texture.
  GLuint ids[1];
  GM_CALL(GenTextures(1, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE0));
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE, ids[0]));

  // Set up texture.
  GLsizei samples = 4;
  GLint internal_format = GL_RGBA;
  GLsizei width = 128;
  GLsizei height = 128;
  GLboolean fixed_sample_locations = false;
  GM_CALL(TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                internal_format, width, height,
                                fixed_sample_locations));

  // Invalid index.
  GM_ERROR_CALL(GetMultisamplefv(pname, 19, val), GL_INVALID_VALUE);

  // Successful calls.
  for (GLuint i = 0; i < 4; i++) {
    GM_CALL(GetMultisamplefv(pname, i, val));
    GLfloat value = static_cast<GLfloat>(i) / static_cast<GLfloat>(samples);
    EXPECT_EQ(value, val[0]);
    EXPECT_EQ(value, val[1]);
  }
}

TEST(MockGraphicsManagerTest, SampleMaski) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint index = 3;
  GLbitfield mask = 19;

  GLint maxSampleMaskWords = GetInt(gm, GL_MAX_SAMPLE_MASK_WORDS);
  std::unique_ptr<GLint[]> masks(new GLint[maxSampleMaskWords]);
  memset(masks.get(), 0, maxSampleMaskWords * sizeof(GLint));

  // Invalid index.
  GM_ERROR_CALL(SampleMaski(19, mask), GL_INVALID_VALUE);
  GM_CALL(GetIntegerv(GL_SAMPLE_MASK_VALUE, masks.get()));
  EXPECT_EQ(0, masks[index]);

  // Successful call.
  GM_CALL(SampleMaski(index, mask));
  GM_CALL(GetIntegerv(GL_SAMPLE_MASK_VALUE, masks.get()));
  EXPECT_EQ(19, masks[index]);
}

TEST(MockGraphicsManagerTest, TexStorage2D) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[3];
  memset(ids, 0, sizeof(ids));
  GM_CALL(GenTextures(3, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE2));
  GM_CALL(BindTexture(GL_TEXTURE_1D_ARRAY, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, ids[2]));

  const GLint levels = 5;
  const GLint internal_format = GL_RGBA;
  const GLsizei width = 16;
  const GLsizei height = 16;

  // Should fail for a non-2D texture target.
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_3D, levels, internal_format, width, height),
      GL_INVALID_ENUM);
  // A valid texture object must be bound.
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_2D, 0, internal_format, width, height),
      GL_INVALID_OPERATION);
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[1]));
  // Not enough levels.
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_2D, 0, internal_format, width, height),
      GL_INVALID_VALUE);
  // Too many levels, since the max is 5 given a size of 16.
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_2D, 6, internal_format, width, height),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_2D, levels, GL_LESS, width, height),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_2D, levels, internal_format, 0, height),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_2D, levels, internal_format, width, 0),
      GL_INVALID_VALUE);
  // Large values for 1D array.
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_1D_ARRAY, levels, internal_format,
                             gm->GetMaxTextureSize() + 1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_1D_ARRAY, levels, internal_format,
                             width, gm->GetMaxArrayTextureLayers() + 1),
                GL_INVALID_VALUE);
  // Large values for 2D.
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_2D, levels, internal_format,
                             gm->GetMaxTextureSize() + 1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_2D, levels, internal_format,
                             width, gm->GetMaxTextureSize() + 1),
                GL_INVALID_VALUE);
  // Large values for cubemap.
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format,
                             gm->GetMaxCubeMapTextureSize() + 1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format,
                             width, gm->GetMaxCubeMapTextureSize() + 1),
                GL_INVALID_VALUE);
  // Cubemap dims not equal.
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format,
                             width, height + 1),
                GL_INVALID_VALUE);

  // Verify textures are mutable.
  EXPECT_EQ(GL_FALSE, GetTextureInt(gm, GL_TEXTURE_1D_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE, GetTextureInt(gm, GL_TEXTURE_CUBE_MAP,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));

  // Valid calls for each 2D type.
  GM_CALL(TexStorage2D(GL_TEXTURE_1D_ARRAY, levels, internal_format, width,
                       height));
  GM_CALL(TexStorage2D(GL_TEXTURE_2D, levels, internal_format, width, height));
  GM_CALL(TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format, width,
                       height));

  // These textures should now be immutable.
  EXPECT_EQ(GL_TRUE, GetTextureInt(gm, GL_TEXTURE_1D_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_TRUE,
            GetTextureInt(gm, GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_TRUE, GetTextureInt(gm, GL_TEXTURE_CUBE_MAP,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));

  // Calling again on an already set texture is an invalid operation.
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_1D_ARRAY, levels, internal_format, width, height),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_2D, levels, internal_format, width, height),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format, width, height),
      GL_INVALID_OPERATION);

  // Calling a non sub-image texture function after TexStorage is also an
  // invalid operation.
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL), GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      CompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_ETC1_RGB8_OES,
                           width, height, 0, 1024, NULL), GL_INVALID_OPERATION);
}

TEST(MockGraphicsManagerTest, TexStorage3D) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[3];
  memset(ids, 0, sizeof(ids));
  GM_CALL(GenTextures(3, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE2));
  GM_CALL(BindTexture(GL_TEXTURE_2D_ARRAY, ids[0]));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, ids[2]));

  const GLint levels = 5;
  const GLint internal_format = GL_RGBA;
  const GLsizei width = 16;
  const GLsizei height = 16;
  const GLsizei depth = 16;

  // Should fail for a non-3D texture target.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_2D, levels, internal_format, width,
                             height, depth),
                GL_INVALID_ENUM);
  // A valid texture object must be bound.
  GM_ERROR_CALL(
      TexStorage3D(GL_TEXTURE_3D, 0, internal_format, width, height, depth),
      GL_INVALID_OPERATION);
  GM_CALL(BindTexture(GL_TEXTURE_3D, ids[1]));
  // Not enough levels.
  GM_ERROR_CALL(
      TexStorage3D(GL_TEXTURE_3D, 0, internal_format, width, height, depth),
      GL_INVALID_VALUE);
  // Too many levels, since the max is 5 given a size of 16.
  GM_ERROR_CALL(
      TexStorage3D(GL_TEXTURE_3D, 6, internal_format, width, height, depth),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      TexStorage3D(GL_TEXTURE_3D, levels, GL_LESS, width, height, depth),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      TexStorage3D(GL_TEXTURE_3D, levels, internal_format, 0, height, depth),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      TexStorage3D(GL_TEXTURE_3D, levels, internal_format, width, 0, depth),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      TexStorage3D(GL_TEXTURE_3D, levels, internal_format, width, height, 0),
      GL_INVALID_VALUE);
  // Large values for 2D array.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internal_format,
                             gm->GetMaxTextureSize() + 1, height, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internal_format,
                             width, gm->GetMaxTextureSize() + 1, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internal_format,
                             width, height, gm->GetMaxArrayTextureLayers() + 1),
                GL_INVALID_VALUE);
  // Large values for 3D.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_3D, levels, internal_format,
                             gm->GetMaxTextureSize() + 1, height, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_3D, levels, internal_format, width,
                             gm->GetMaxTextureSize() + 1, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_3D, levels, internal_format, width,
                             height, gm->GetMaxTextureSize() + 1),
                GL_INVALID_VALUE);
  // Large values for cubemap array.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             gm->GetMaxCubeMapTextureSize() + 1, height, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             width, gm->GetMaxCubeMapTextureSize() + 1, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             width, depth, gm->GetMaxArrayTextureLayers() + 1),
                GL_INVALID_VALUE);
  // Cubemap dims not equal.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             width, height + 1, depth),
                GL_INVALID_VALUE);

  // Verify textures are mutable.
  EXPECT_EQ(GL_FALSE, GetTextureInt(gm, GL_TEXTURE_2D_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE,
            GetTextureInt(gm, GL_TEXTURE_3D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE, GetTextureInt(gm, GL_TEXTURE_CUBE_MAP_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));

  // Valid calls for each 3D type.
  GM_CALL(TexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internal_format, width,
                       height, depth));
  GM_CALL(TexStorage3D(GL_TEXTURE_3D, levels, internal_format, width, height,
                       depth));
  // The depth for a cubemap array does not have to equal the dimensions.
  GM_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                       width, height, depth + 1));

  // These textures should now be immutable.
  EXPECT_EQ(GL_TRUE, GetTextureInt(gm, GL_TEXTURE_2D_ARRAY,
                                   GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_TRUE,
            GetTextureInt(gm, GL_TEXTURE_3D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_TRUE, GetTextureInt(gm, GL_TEXTURE_CUBE_MAP_ARRAY,
                                   GL_TEXTURE_IMMUTABLE_FORMAT));

  // Calling again on an already set texture is an invalid operation.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internal_format,
                             width, height, depth),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_3D, levels, internal_format, width,
                             height, depth),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             width, height, depth),
                GL_INVALID_OPERATION);
  // Calling a non sub-image texture function after TexStorage is also an
  // invalid operation.
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, width, height, depth,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL), GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_ETC1_RGB8_OES,
                           width, height, depth, 0, 1024, NULL),
      GL_INVALID_OPERATION);
}

TEST(MockGraphicsManagerTest, TexStorage2DMultisample) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[1];
  memset(ids, 0, sizeof(ids));
  GM_CALL(GenTextures(1, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE0));

  const GLint samples = 8;
  const GLint internal_format = GL_RGBA;
  const GLsizei width = 16;
  const GLsizei height = 16;

  // Should fail for a non-2D-MS texture target.
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_3D, samples, internal_format,
                                        width, height, false),
      GL_INVALID_ENUM);
  // A valid texture object must be bound.
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        internal_format, width, height, false),
      GL_INVALID_OPERATION);
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE, ids[0]));
  // Too many samples, since the max is 16.
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 19,
                                        internal_format, width, height, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        GL_LESS, width, height, false),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        internal_format, 0, height, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        internal_format, width, 0, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        internal_format,
                                        gm->GetMaxTextureSize() + 1, height,
                                        false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        internal_format, width,
                                        gm->GetMaxTextureSize() + 1, false),
      GL_INVALID_VALUE);

  // Verify texture is mutable.
  EXPECT_EQ(GL_FALSE, GetTextureInt(gm, GL_TEXTURE_2D_MULTISAMPLE,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));

  // Successful call.
  GM_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                  internal_format, width, height, false));

  // Texture should now be immutable.
  EXPECT_EQ(GL_TRUE, GetTextureInt(gm, GL_TEXTURE_2D_MULTISAMPLE,
                                   GL_TEXTURE_IMMUTABLE_FORMAT));

  // Calling again on an already set texture is an invalid operation.
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        internal_format, width, height, false),
      GL_INVALID_OPERATION);
}

TEST(MockGraphicsManagerTest, TexStorage3DMultisample) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[1];
  memset(ids, 0, sizeof(ids));
  GM_CALL(GenTextures(1, ids));
  GM_CALL(ActiveTexture(GL_TEXTURE0));

  const GLint samples = 8;
  const GLint internal_format = GL_RGBA;
  const GLsizei width = 16;
  const GLsizei height = 16;
  const GLsizei depth = 16;

  // Should fail for a non-3D-MS texture target.
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_3D, samples, internal_format,
                                        width, height, depth, false),
      GL_INVALID_ENUM);
  // A valid texture object must be bound.
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width, height,
                                        depth, false),
      GL_INVALID_OPERATION);
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, ids[0]));
  // Too many samples, since the max is 16.
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 19,
                                        internal_format, width, height,
                                        depth, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, GL_LESS, width, height,
                                        depth, false),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, 0, height,
                                        depth, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width, 0,
                                        depth, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width, height,
                                        0, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format,
                                        gm->GetMaxTextureSize() + 1, height,
                                        depth, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width,
                                        gm->GetMaxTextureSize() + 1, depth,
                                        false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width, height,
                                        gm->GetMaxTextureSize() + 1, false),
      GL_INVALID_VALUE);

  // Verify texture is mutable.
  EXPECT_EQ(GL_FALSE, GetTextureInt(gm, GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));

  // Successful call.
  GM_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples,
                                  internal_format, width, height, depth,
                                  false));

  // Texture should now be immutable.
  EXPECT_EQ(GL_TRUE, GetTextureInt(gm, GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                   GL_TEXTURE_IMMUTABLE_FORMAT));

  // Calling again on an already set texture is an invalid operation.
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width, height,
                                        depth, false),
      GL_INVALID_OPERATION);
}

TEST(MockGraphicsManagerTest, Samplers) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // The default sampler is 0.
  EXPECT_EQ(0, GetInt(gm, GL_SAMPLER_BINDING));

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_ERROR_CALL(GenSamplers(-1, ids), GL_INVALID_VALUE);
  EXPECT_EQ(0U, ids[0]);
  EXPECT_EQ(0U, ids[1]);
  GM_CALL(GenSamplers(2, ids));
  EXPECT_NE(0U, ids[0]);
  EXPECT_NE(0U, ids[1]);
  EXPECT_EQ(GL_FALSE, gm->IsSampler(0U));
  EXPECT_EQ(GL_TRUE, gm->IsSampler(ids[0]));
  EXPECT_EQ(GL_TRUE, gm->IsSampler(ids[1]));
  EXPECT_EQ(GL_FALSE, gm->IsSampler(ids[0] + ids[1]));

  GLuint max_units = static_cast<GLuint>(
      GetInt(gm, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
  EXPECT_GT(max_units, 0U);

  // Bad binds.
  GM_ERROR_CALL(BindSampler(max_units + 1, ids[0]), GL_INVALID_VALUE);
  GM_ERROR_CALL(BindSampler(0, ids[0] + ids[1]), GL_INVALID_OPERATION);
  // Good binds.
  EXPECT_EQ(0, GetInt(gm, GL_SAMPLER_BINDING));
  GM_CALL(BindSampler(0, 0));
  GM_CALL(BindSampler(0, ids[0]));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(gm, GL_SAMPLER_BINDING));

  // Check errors.
  GM_ERROR_CALL(
      SamplerParameterf(ids[0] + ids[1], GL_TEXTURE_MIN_FILTER, GL_NEAREST),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(SamplerParameterf(ids[0], GL_NEAREST, GL_REPEAT),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(
      SamplerParameteri(ids[0], GL_TEXTURE_COMPARE_FUNC, GL_FRONT),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      SamplerParameteri(ids[0], GL_TEXTURE_COMPARE_MODE, GL_LESS),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(SamplerParameteri(ids[0], GL_TEXTURE_WRAP_R, GL_BACK),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(
      SamplerParameteri(ids[0], GL_TEXTURE_WRAP_T, GL_DEPTH_TEST),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(SamplerParameteri(ids[0], GL_TEXTURE_WRAP_S, GL_LINEAR),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(SamplerParameteri(ids[1], GL_NEAREST, GL_REPEAT),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(
      SamplerParameteri(ids[1], GL_TEXTURE_WRAP_S, GL_FRONT),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      SamplerParameteri(ids[1], GL_TEXTURE_WRAP_S, GL_LINEAR),
      GL_INVALID_ENUM);
  // Mag filter cannot use mipmapping modes.
  GM_ERROR_CALL(SamplerParameteri(ids[0], GL_TEXTURE_MAG_FILTER,
                                  GL_NEAREST_MIPMAP_LINEAR), GL_INVALID_ENUM);
  GM_ERROR_CALL(SamplerParameteri(ids[0], GL_TEXTURE_MAG_FILTER,
                                  GL_LINEAR_MIPMAP_LINEAR), GL_INVALID_ENUM);

  // Check default texture modes.
  EXPECT_EQ(1.f, GetSamplerFloat(gm, ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetSamplerInt(gm, ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(gm, ids[0], GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_R));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_T));
  // Error if an invalid enum is used.
  GM_ERROR_CALL(
      GetSamplerParameteriv(ids[0], GL_VERTEX_ATTRIB_ARRAY_SIZE, NULL),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetSamplerParameterfv(ids[0], GL_VERTEX_ATTRIB_ARRAY_SIZE, NULL),
      GL_INVALID_ENUM);

  // Check that changes happen.
  GLint mode = GL_CLAMP_TO_EDGE;
  GM_CALL(SamplerParameteriv(ids[0], GL_TEXTURE_WRAP_S, &mode));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_S));
  GLfloat modef = GL_MIRRORED_REPEAT;
  GM_CALL(SamplerParameterfv(ids[0], GL_TEXTURE_WRAP_S, &modef));
  EXPECT_EQ(static_cast<GLfloat>(GL_MIRRORED_REPEAT),
            GetSamplerFloat(gm, ids[0], GL_TEXTURE_WRAP_S));
  GM_CALL(
      SamplerParameteri(ids[0], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_S));
  GM_ERROR_CALL(SamplerParameteri(ids[0], GL_TEXTURE_BASE_LEVEL, 2),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(SamplerParameteri(ids[0], GL_TEXTURE_MAX_LEVEL, 200),
                GL_INVALID_ENUM);
  GM_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MIN_LOD, 3.14f));
  EXPECT_EQ(3.14f, GetSamplerFloat(gm, ids[0], GL_TEXTURE_MIN_LOD));
  GM_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MAX_LOD, 2.18f));
  EXPECT_EQ(2.18f, GetSamplerFloat(gm, ids[0], GL_TEXTURE_MAX_LOD));
  GM_CALL(SamplerParameteri(ids[0], GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS));
  EXPECT_EQ(GL_ALWAYS, GetSamplerInt(gm, ids[0], GL_TEXTURE_COMPARE_FUNC));
  GM_CALL(SamplerParameteri(
      ids[0], GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE));
  EXPECT_EQ(GL_COMPARE_REF_TO_TEXTURE,
            GetSamplerInt(gm, ids[0], GL_TEXTURE_COMPARE_MODE));

  GM_ERROR_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.9f),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 999.f),
                GL_INVALID_VALUE);
  GM_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 3.f));
  EXPECT_EQ(3.f, GetSamplerFloat(gm, ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT));

  // Check that changes affect only the proper parameter.
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetSamplerInt(gm, ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(gm, ids[0], GL_TEXTURE_MAG_FILTER));

  GM_CALL(SamplerParameteri(ids[0], GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT));
  EXPECT_EQ(GL_CLAMP_TO_EDGE, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetSamplerInt(gm, ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(gm, ids[0], GL_TEXTURE_MAG_FILTER));

  GM_CALL(SamplerParameteri(ids[0], GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  EXPECT_EQ(GL_CLAMP_TO_EDGE, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST, GetSamplerInt(gm, ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(gm, ids[0], GL_TEXTURE_MAG_FILTER));

  GM_CALL(SamplerParameteri(ids[0], GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  EXPECT_EQ(GL_CLAMP_TO_EDGE, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT, GetSamplerInt(gm, ids[0], GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST, GetSamplerInt(gm, ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_NEAREST, GetSamplerInt(gm, ids[0], GL_TEXTURE_MAG_FILTER));

  // Check that the other sampler settings have not changed.
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetSamplerInt(gm, ids[1], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(gm, ids[1], GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(gm, ids[1], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(gm, ids[1], GL_TEXTURE_WRAP_T));

  // Delete samplers.
  GLuint bad_id = 5U;
  // Error if n < 0.
  GM_ERROR_CALL(DeleteSamplers(-1, ids), GL_INVALID_VALUE);
  // Bad ids are silently ignored.
  GM_CALL(DeleteSamplers(1, &bad_id));
  // Actually delete the ids.
  GM_CALL(DeleteSamplers(2, ids));
  EXPECT_FALSE(gm->IsSampler(ids[0]));
  EXPECT_FALSE(gm->IsSampler(ids[1]));
  EXPECT_EQ(0, GetInt(gm, GL_SAMPLER_BINDING));
  // Bind should fail on a deleted texture.
  GM_ERROR_CALL(BindSampler(0, ids[0]), GL_INVALID_OPERATION);
}

TEST(MockGraphicsManagerTest, ArraysBuffersDrawFunctions) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // The default vertex buffer is 0.
  EXPECT_EQ(0, GetInt(gm, GL_ARRAY_BUFFER_BINDING));

  // DrawArrays.
  // Draw mode error.
  GM_ERROR_CALL(DrawArrays(GL_NEVER, 0, 1), GL_INVALID_ENUM);
  // Negative count.
  GM_ERROR_CALL(DrawArrays(GL_TRIANGLES, 0, -2), GL_INVALID_VALUE);
  // Draw calls using default buffer 0 will succeed.
  GM_CALL(DrawArrays(GL_TRIANGLE_STRIP, 0, 100));

  // Call Clear to improve coverage.
  GM_CALL(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  // DrawElements
  // Draw mode error.
  GM_ERROR_CALL(DrawElements(GL_NEVER, 1, GL_UNSIGNED_BYTE, NULL),
                GL_INVALID_ENUM);
  // Negative count.
  GM_ERROR_CALL(DrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, NULL),
                GL_INVALID_VALUE);
  // Bad type.
  GM_ERROR_CALL(DrawElements(GL_POINTS, 10, GL_FLOAT, NULL),
                GL_INVALID_ENUM);
  // Successful call.
  GM_CALL(DrawElements(GL_POINTS, 2, GL_UNSIGNED_BYTE, NULL));
  // Increase code coverage using these draw related functions.
  GM_CALL(Flush());
  GM_CALL(Finish());

  // GenVertexArrays
  GLuint vao = 0, vao2 = 0;
  GM_ERROR_CALL(GenVertexArrays(-1, &vao), GL_INVALID_VALUE);
  // The value should not have been changed.
  EXPECT_EQ(0U, vao);
  // Create valid vertex arrays.
  GM_CALL(GenVertexArrays(1, &vao));
  GM_CALL(GenVertexArrays(1, &vao2));
  EXPECT_NE(0U, vao);
  EXPECT_NE(0U, vao2);
  EXPECT_TRUE(gm->IsVertexArray(0U));
  EXPECT_TRUE(gm->IsVertexArray(vao));
  EXPECT_TRUE(gm->IsVertexArray(vao2));
  EXPECT_FALSE(gm->IsVertexArray(vao + vao2));

  // BindVertexArray
  GM_ERROR_CALL(BindVertexArray(5U), GL_INVALID_OPERATION);
  GM_ERROR_CALL(BindVertexArray(4U), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(gm, GL_VERTEX_ARRAY_BINDING));
  // Bind valid array.
  GM_CALL(BindVertexArray(vao));

  // Check vertex attribute defaults.
  int attrib_count = GetInt(gm, GL_MAX_VERTEX_ATTRIBS);
  EXPECT_GT(attrib_count, 0);
  for (int i = 0; i < attrib_count; ++i) {
    EXPECT_EQ(0, GetAttribInt(gm, i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
    EXPECT_EQ(GL_FALSE, GetAttribInt(gm, i, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
    EXPECT_EQ(4, GetAttribInt(gm, i, GL_VERTEX_ATTRIB_ARRAY_SIZE));
    EXPECT_EQ(0, GetAttribInt(gm, i, GL_VERTEX_ATTRIB_ARRAY_STRIDE));
    EXPECT_EQ(GL_FLOAT, GetAttribInt(gm, i, GL_VERTEX_ATTRIB_ARRAY_TYPE));
    EXPECT_EQ(GL_FALSE, GetAttribInt(gm, i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED));
    EXPECT_EQ(0, GetAttribFloat(gm, i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
    EXPECT_EQ(GL_FALSE, GetAttribFloat(gm, i, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
    EXPECT_EQ(4, GetAttribFloat(gm, i, GL_VERTEX_ATTRIB_ARRAY_SIZE));
    EXPECT_EQ(0, GetAttribFloat(gm, i, GL_VERTEX_ATTRIB_ARRAY_STRIDE));
    EXPECT_EQ(GL_FLOAT, GetAttribFloat(gm, i, GL_VERTEX_ATTRIB_ARRAY_TYPE));
    EXPECT_EQ(GL_FALSE,
              GetAttribFloat(gm, i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED));
    EXPECT_EQ(math::Vector4f(0.f, 0.f, 0.f, 1.f),
              GetAttribFloat4(gm, i, GL_CURRENT_VERTEX_ATTRIB));
    EXPECT_EQ(math::Vector4i(0, 0, 0, 1),
              GetAttribInt4(gm, i, GL_CURRENT_VERTEX_ATTRIB));
    EXPECT_EQ(reinterpret_cast<GLvoid*>(0),
              GetAttribPointer(gm, 1, GL_VERTEX_ATTRIB_ARRAY_POINTER));
    EXPECT_EQ(0, GetAttribFloat(gm, i, GL_VERTEX_ATTRIB_ARRAY_DIVISOR));
  }
  // Check error conditions for GetVertexAttrib[if]v.
  GM_ERROR_CALL(GetVertexAttribiv(attrib_count,
                                  GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(GetVertexAttribfv(attrib_count,
                                  GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(GetVertexAttribiv(1, attrib_count, NULL), GL_INVALID_ENUM);
  GM_ERROR_CALL(GetVertexAttribfv(1, attrib_count, NULL), GL_INVALID_ENUM);

  // VertexAttributes
  // Enable attrib.
  GM_ERROR_CALL(EnableVertexAttribArray(attrib_count), GL_INVALID_VALUE);
  EXPECT_EQ(GL_FALSE, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
  GM_CALL(EnableVertexAttribArray(1));
  EXPECT_EQ(GL_TRUE, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));

  // Bad calls for setting the pointer.
  GM_ERROR_CALL(VertexAttribPointer(attrib_count, 2, GL_SHORT, GL_FALSE, 0,
                                    reinterpret_cast<GLvoid*>(4)),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttribPointer(1, 10, GL_SHORT, GL_FALSE, 0,
                                    reinterpret_cast<GLvoid*>(4)),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttribPointer(1, 2, GL_SHORT, GL_FALSE, -2,
                                    reinterpret_cast<GLvoid*>(4)),
                GL_INVALID_VALUE);
  // Successful call.
  GM_CALL(VertexAttribPointer(1, 2, GL_SHORT, GL_TRUE, 16,
                              reinterpret_cast<GLvoid*>(4)));
  // Check values.
  EXPECT_EQ(reinterpret_cast<GLvoid*>(4),
            GetAttribPointer(gm, 1, GL_VERTEX_ATTRIB_ARRAY_POINTER));

  // Check that state follows vertex array binding.
  GM_CALL(BindVertexArray(vao2));
  EXPECT_EQ(vao2, static_cast<GLuint>(GetInt(gm, GL_VERTEX_ARRAY_BINDING)));
  EXPECT_EQ(GL_FALSE, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
  EXPECT_EQ(0, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
  EXPECT_EQ(4, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_SIZE));
  EXPECT_EQ(0, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_STRIDE));
  EXPECT_EQ(GL_FLOAT, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_TYPE));
  EXPECT_EQ(GL_FALSE, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED));
  EXPECT_EQ(math::Vector4f(0.f, 0.f, 0.f, 1.f),
            GetAttribFloat4(gm, 1, GL_CURRENT_VERTEX_ATTRIB));
  EXPECT_EQ(reinterpret_cast<GLvoid*>(0),
            GetAttribPointer(gm, 1, GL_VERTEX_ATTRIB_ARRAY_POINTER));
  EXPECT_EQ(0, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_DIVISOR));

  GM_CALL(BindVertexArray(vao));
  EXPECT_EQ(vao, static_cast<GLuint>(GetInt(gm, GL_VERTEX_ARRAY_BINDING)));
  EXPECT_EQ(GL_TRUE, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
  EXPECT_EQ(0, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
  EXPECT_EQ(2, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_SIZE));
  EXPECT_EQ(16, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_STRIDE));
  EXPECT_EQ(GL_SHORT, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_TYPE));
  EXPECT_EQ(GL_TRUE, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED));
  EXPECT_EQ(math::Vector4f(0.f, 0.f, 0.f, 1.f),
            GetAttribFloat4(gm, 1, GL_CURRENT_VERTEX_ATTRIB));
  EXPECT_EQ(reinterpret_cast<GLvoid*>(4),
            GetAttribPointer(gm, 1, GL_VERTEX_ATTRIB_ARRAY_POINTER));

  // Disable attrib.
  GM_ERROR_CALL(DisableVertexAttribArray(attrib_count), GL_INVALID_VALUE);
  EXPECT_EQ(GL_TRUE, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
  GM_CALL(DisableVertexAttribArray(1));
  EXPECT_EQ(GL_FALSE, GetAttribInt(gm, 1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));

  // Set attribute float values.
  float f4[4] = { 1.1f, 2.2f, 3.3f, 4.4f };
  GM_ERROR_CALL(VertexAttrib1fv(attrib_count, f4), GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttrib2fv(attrib_count, f4), GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttrib3fv(attrib_count, f4), GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttrib4fv(attrib_count, f4), GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttrib1f(attrib_count, f4[0]), GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttrib2f(attrib_count, f4[0], f4[1]), GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttrib3f(attrib_count, f4[0], f4[1], f4[2]),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(VertexAttrib4f(attrib_count, f4[0], f4[1], f4[2], f4[3]),
                GL_INVALID_VALUE);
  // Successful calls.
  math::Vector4f vert(1.f, 2.f, 3.f, 4.f);
  GM_CALL(VertexAttrib1fv(3, &vert[0]));
  EXPECT_EQ(math::Vector4f(vert[0], 0.f, 0.f, 1.f),
            GetAttribFloat4(gm, 3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib2fv(3, &vert[0]));
  EXPECT_EQ(math::Vector4f(vert[0], vert[1], 0.f, 1.f),
            GetAttribFloat4(gm, 3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib3fv(3, &vert[0]));
  EXPECT_EQ(math::Vector4f(vert[0], vert[1], vert[2], 1.f),
            GetAttribFloat4(gm, 3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib4fv(3, &vert[0]));
  EXPECT_EQ(vert, GetAttribFloat4(gm, 3, GL_CURRENT_VERTEX_ATTRIB));
  vert.Set(4.f, 3.f, 2.f, 1.f);
  GM_CALL(VertexAttrib1f(3, vert[0]));
  EXPECT_EQ(math::Vector4f(vert[0], 0.f, 0.f, 1.f),
            GetAttribFloat4(gm, 3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib2f(3, vert[0], vert[1]));
  EXPECT_EQ(math::Vector4f(vert[0], vert[1], 0.f, 1.f),
            GetAttribFloat4(gm, 3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib3f(3, vert[0], vert[1], vert[2]));
  EXPECT_EQ(math::Vector4f(vert[0], vert[1], vert[2], 1.f),
            GetAttribFloat4(gm, 3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib4f(3, vert[0], vert[1], vert[2], vert[3]));
  EXPECT_EQ(vert, GetAttribFloat4(gm, 3, GL_CURRENT_VERTEX_ATTRIB));

  // Buffer objects.
  // GenBuffers
  GLuint vbo = 0, vbo2 = 0;
  GM_ERROR_CALL(GenBuffers(-1, &vbo), GL_INVALID_VALUE);
  // The value should not have been changed.
  EXPECT_EQ(0U, vbo);
  // Create valid vertex arrays.
  GM_CALL(GenBuffers(1, &vbo));
  GM_CALL(GenBuffers(1, &vbo2));
  EXPECT_NE(0U, vbo);
  EXPECT_NE(0U, vbo2);
  EXPECT_EQ(GL_TRUE, gm->IsBuffer(0));
  EXPECT_EQ(GL_TRUE, gm->IsBuffer(vbo));
  EXPECT_EQ(GL_TRUE, gm->IsBuffer(vbo2));
  EXPECT_EQ(GL_FALSE, gm->IsBuffer(vbo + vbo2));

  // GetBufferParameteriv.
  // No buffer is bound.
  GM_ERROR_CALL(GetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE,
                                     NULL), GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetBufferParameteriv(GL_TEXTURE_2D, GL_BUFFER_SIZE, NULL),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetBufferParameteriv(GL_ARRAY_BUFFER, GL_FLOAT, NULL),
                GL_INVALID_ENUM);

  // BindBuffer
  GM_ERROR_CALL(BindBuffer(GL_LINK_STATUS, 4U), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindBuffer(GL_ARRAY_BUFFER, 3U), GL_INVALID_VALUE);
  // Check that no buffer was bound.
  EXPECT_EQ(0, GetInt(gm, GL_ARRAY_BUFFER_BINDING));

  // Check that vertex element arrays are bound to the current VAO.
  GM_CALL(BindVertexArray(0));
  EXPECT_EQ(0U,
            static_cast<GLuint>(GetInt(gm, GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo));
  GM_CALL(BindVertexArray(vao));
  // The binding should be overwritten.
  EXPECT_EQ(0U,
            static_cast<GLuint>(GetInt(gm, GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo2));
  EXPECT_EQ(vbo2,
            static_cast<GLuint>(GetInt(gm, GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindVertexArray(0));
  EXPECT_EQ(vbo,
            static_cast<GLuint>(GetInt(gm, GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindVertexArray(vao));
  EXPECT_EQ(vbo2,
            static_cast<GLuint>(GetInt(gm, GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindVertexArray(0));

  // Bind valid buffers.
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, vbo));
  GM_CALL(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo2));
  // Check buffers are bound.
  EXPECT_EQ(vbo, static_cast<GLuint>(GetInt(gm, GL_ARRAY_BUFFER_BINDING)));
  EXPECT_EQ(vbo2,
            static_cast<GLuint>(GetInt(gm, GL_ELEMENT_ARRAY_BUFFER_BINDING)));

  // BufferData
  GM_ERROR_CALL(
      BufferData(GL_TEXTURE_2D, 1024, NULL, GL_STATIC_DRAW), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      BufferData(GL_ARRAY_BUFFER, -1, NULL, GL_STATIC_DRAW), GL_INVALID_VALUE);
  GM_ERROR_CALL(
      BufferData(GL_ARRAY_BUFFER, 1024, NULL, GL_FRONT), GL_INVALID_ENUM);
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, 0U));
  GM_ERROR_CALL(
      BufferData(GL_ARRAY_BUFFER, 1024, NULL, GL_STATIC_DRAW),
      GL_INVALID_OPERATION);
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, vbo));
  GM_CALL(
      BufferData(GL_ARRAY_BUFFER, 1024, NULL, GL_STATIC_DRAW));

  EXPECT_EQ(1024, GetBufferInt(gm, GL_ARRAY_BUFFER, GL_BUFFER_SIZE));
  EXPECT_EQ(GL_STATIC_DRAW, GetBufferInt(gm, GL_ARRAY_BUFFER, GL_BUFFER_USAGE));

  // BufferSubData
  GM_ERROR_CALL(BufferSubData(GL_TEXTURE_2D, 16, 10, NULL), GL_INVALID_ENUM);
  GM_ERROR_CALL(BufferSubData(GL_ARRAY_BUFFER, -1, 10, NULL), GL_INVALID_VALUE);
  GM_ERROR_CALL(BufferSubData(GL_ARRAY_BUFFER, 16, -1, NULL), GL_INVALID_VALUE);
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, 0U));
  GM_ERROR_CALL(BufferSubData(GL_ARRAY_BUFFER, 16, 10, NULL),
      GL_INVALID_OPERATION);
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, vbo));
  GM_ERROR_CALL(BufferSubData(GL_ARRAY_BUFFER, 1020, 10, NULL),
                GL_INVALID_VALUE);
  GM_CALL(BufferSubData(GL_ARRAY_BUFFER, 128, 10, NULL));

  // void CopyBufferSubData(enum readtarget, enum writetarget,
  //                        intptr readoffset, intptr writeoffset,
  //                        sizeiptr size);
  GM_ERROR_CALL(CopyBufferSubData(
      GL_TEXTURE_2D, GL_ARRAY_BUFFER, 16, 10, 4), GL_INVALID_ENUM);
  GM_ERROR_CALL(CopyBufferSubData(
      GL_ARRAY_BUFFER, GL_TEXTURE_2D, 16, 10, 4), GL_INVALID_ENUM);
  // "any of readoffset, writeoffset, or size are negative."
  GM_ERROR_CALL(CopyBufferSubData(
      GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, -16, 10, 4), GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyBufferSubData(
      GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 16, -10, 4), GL_INVALID_VALUE);
  GM_ERROR_CALL(CopyBufferSubData(
      GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 16, 10, -4), GL_INVALID_VALUE);
  // "readoffset + size exceeds the size of the buffer object"
  GM_ERROR_CALL(CopyBufferSubData(
      GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 1000, 10, 25), GL_INVALID_VALUE);
  // "writeoffset + size exceeds the size of the buffer object"
  GM_ERROR_CALL(CopyBufferSubData(
      GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 0, 1000, 25), GL_INVALID_VALUE);
  // "ranges [readoffset, readoffset +size) and
  // [writeoffset, writeoffset + size) overlap"
  GM_ERROR_CALL(CopyBufferSubData(
      GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 0, 10, 25), GL_INVALID_VALUE);
  // "the buffer objects bound to either readtarget or writetarget are mapped".
  GM_CALL(MapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE));
  GM_ERROR_CALL(CopyBufferSubData(
      GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 0, 10, 4), GL_INVALID_OPERATION);
  GM_CALL(UnmapBuffer(GL_ARRAY_BUFFER));
  GM_CALL(CopyBufferSubData(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER, 0, 25, 25));

  // Check that a vertex array tracks the buffer binding.
  EXPECT_EQ(0, GetAttribInt(gm, 5, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
  GM_CALL(VertexAttribPointer(5, 2, GL_SHORT, GL_FALSE, 0,
                              reinterpret_cast<GLvoid*>(8)));
  EXPECT_EQ(vbo2,
            static_cast<GLuint>(GetInt(gm, GL_ELEMENT_ARRAY_BUFFER_BINDING)));

  // DeleteVertexArrays
  GM_ERROR_CALL(DeleteVertexArrays(-1, &vao), GL_INVALID_VALUE);
  GM_CALL(DeleteVertexArrays(1, &vao));
  // Invalid deletes are silently ignored.
  vao = 12U;
  GM_CALL(DeleteVertexArrays(1, &vao));
  EXPECT_FALSE(gm->IsVertexArray(vao));

  // DeleteBuffers
  GM_ERROR_CALL(DeleteBuffers(-1, &vbo), GL_INVALID_VALUE);
  GM_CALL(DeleteBuffers(1, &vbo));
  GM_CALL(DeleteBuffers(1, &vbo2));
  EXPECT_FALSE(gm->IsBuffer(vbo));
  EXPECT_FALSE(gm->IsBuffer(vbo2));
  EXPECT_EQ(0U, static_cast<GLuint>(GetInt(gm, GL_ARRAY_BUFFER_BINDING)));
  EXPECT_EQ(0U,
            static_cast<GLuint>(GetInt(gm, GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  // Invalid deletes are silently ignored.
  vbo = 12U;
  GM_CALL(DeleteBuffers(1, &vbo));
}

TEST(MockGraphicsManagerTest, DrawInstancedFunctions) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // The default vertex buffer is 0.
  EXPECT_EQ(0, GetInt(gm, GL_ARRAY_BUFFER_BINDING));

  // VertexAttribDivisor.
  // Invalid index value.
  GM_ERROR_CALL(VertexAttribDivisor(GL_MAX_VERTEX_ATTRIBS, 1),
                GL_INVALID_VALUE);
  GM_CALL(VertexAttribDivisor(0, 1));

  // DrawArraysInstanced.
  // Draw mode error.
  GM_ERROR_CALL(DrawArraysInstanced(GL_NEVER, 0, 1, 10), GL_INVALID_ENUM);
  // Negative count.
  GM_ERROR_CALL(DrawArraysInstanced(GL_TRIANGLES, 0, -2, 10), GL_INVALID_VALUE);
  // Negative primCount.
  GM_ERROR_CALL(DrawArraysInstanced(GL_TRIANGLES, 0, 1, -10), GL_INVALID_VALUE);

  // Draw calls using default buffer 0 will succeed.
  GM_CALL(DrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 100, 10));

  // Call Clear to improve coverage.
  GM_CALL(Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

  // DrawElementsInstanced.
  // Draw mode error.
  GM_ERROR_CALL(DrawElementsInstanced(GL_NEVER, 1, GL_UNSIGNED_BYTE, NULL, 10),
                GL_INVALID_ENUM);
  // Negative count.
  GM_ERROR_CALL(
      DrawElementsInstanced(GL_POINTS, -1, GL_UNSIGNED_BYTE, NULL, 10),
      GL_INVALID_VALUE);
  // Negative primCount.
  GM_ERROR_CALL(DrawElementsInstanced(GL_POINTS, 1, GL_UNSIGNED_BYTE, NULL, -1),
                GL_INVALID_VALUE);
  // Bad type.
  GM_ERROR_CALL(DrawElementsInstanced(GL_POINTS, 10, GL_FLOAT, NULL, 10),
                GL_INVALID_ENUM);
  // Successful call.
  GM_CALL(DrawElementsInstanced(GL_POINTS, 2, GL_UNSIGNED_BYTE, NULL, 10));
}

TEST(MockGraphicsManagerTest, MappedBuffers) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint vbo = 0;
  GM_CALL(GenBuffers(1, &vbo));
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, vbo));

  // Try to map the buffer.
  uint8 data[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  GM_CALL(BufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW));

  // Check that data has been created.
  uint8* ptr = NULL;
  GM_ERROR_CALL(GetBufferPointerv(GL_STATIC_DRAW, GL_BUFFER_MAP_POINTER,
                                  reinterpret_cast<void**>(&ptr)),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetBufferPointerv(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER,
                                  reinterpret_cast<void**>(&ptr)),
                GL_INVALID_ENUM);
  EXPECT_TRUE(ptr == NULL);

  // Since we have yet to map the buffer, the mapped buffer pointer should be
  // NULL.
  GM_CALL(GetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER,
                            reinterpret_cast<void**>(&ptr)));
  EXPECT_TRUE(ptr == NULL);

  // Now map the buffer.
  GM_ERROR_CALL(MapBuffer(GL_INVALID_VALUE, GL_WRITE_ONLY), GL_INVALID_ENUM);
  GM_ERROR_CALL(MapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(MapBuffer(GL_ARRAY_BUFFER, GL_FRAMEBUFFER_COMPLETE),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(MapBuffer(GL_ARRAY_BUFFER, 0), GL_INVALID_ENUM);
  GM_CALL(MapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE));

  // Properly map the buffer.
  GM_CALL(GetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER,
                            reinterpret_cast<void**>(&ptr)));
  EXPECT_FALSE(ptr == NULL);
  // Check the contents of the buffer.
  for (int i = 0; i < 8; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(data[i], ptr[i]);
    ptr[i] = static_cast<uint8>(8 - i);
  }

  // Can't map a mapped buffer.
  GM_ERROR_CALL(MapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE),
                GL_INVALID_OPERATION);

  // Unmap the buffer.
  GM_ERROR_CALL(UnmapBuffer(GL_READ_WRITE), GL_INVALID_ENUM);
  GM_ERROR_CALL(UnmapBuffer(GL_ELEMENT_ARRAY_BUFFER), GL_INVALID_OPERATION);
  GM_CALL(UnmapBuffer(GL_ARRAY_BUFFER));
  // Can't unmap an unmapped buffer.
  GM_ERROR_CALL(UnmapBuffer(GL_ARRAY_BUFFER), GL_INVALID_OPERATION);

  // Remap the buffer and check the data was updated.
  GM_CALL(MapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE));
  GM_CALL(GetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER,
                            reinterpret_cast<void**>(&ptr)));
  EXPECT_FALSE(ptr == NULL);
  // Check the contents of the buffer.
  for (int i = 0; i < 8; ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(8 - i, ptr[i]);
  }
  GM_CALL(UnmapBuffer(GL_ARRAY_BUFFER));

  // Map a range of the buffer.
  GM_ERROR_CALL(MapBufferRange(GL_INVALID_OPERATION, 2, 4, GL_MAP_READ_BIT),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(MapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 2, 4, GL_MAP_READ_BIT),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(MapBufferRange(GL_ARRAY_BUFFER, -1, 4, GL_MAP_READ_BIT),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(MapBufferRange(GL_ARRAY_BUFFER, 2, -1, GL_MAP_READ_BIT),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(MapBufferRange(GL_ARRAY_BUFFER, 2, 10, GL_MAP_READ_BIT),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(MapBufferRange(GL_ARRAY_BUFFER, 2, 4, 0),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      MapBufferRange(GL_ARRAY_BUFFER, 2, 4, GL_MAP_INVALIDATE_BUFFER_BIT),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(MapBufferRange(GL_ARRAY_BUFFER, 2, 4,
                               GL_MAP_READ_BIT | GL_MAP_INVALIDATE_BUFFER_BIT),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(MapBufferRange(GL_ARRAY_BUFFER, 2, 4,
                               GL_MAP_READ_BIT | GL_MAP_FLUSH_EXPLICIT_BIT),
                GL_INVALID_OPERATION);

  GM_CALL(MapBufferRange(
      GL_ARRAY_BUFFER, 2, 4, GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
  // We can't flush because GL_MAP_FLUSH_EXPLICIT is not set.
  GM_ERROR_CALL(FlushMappedBufferRange(GL_ARRAY_BUFFER, 2, 4),
                GL_INVALID_OPERATION);
  // Error because the buffer is already mapped.
  GM_ERROR_CALL(MapBufferRange(
        GL_ARRAY_BUFFER, 2, 4, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT),
                GL_INVALID_OPERATION);
  GM_CALL(UnmapBuffer(GL_ARRAY_BUFFER));

  void* vptr = GM_CALL(MapBufferRange(
      GL_ARRAY_BUFFER, 2, 4, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT));
  uint8* ptr2 = reinterpret_cast<uint8*>(vptr);
  EXPECT_EQ(&ptr[2], ptr2);

  // Make some changes.
  ptr2[1] = 50;
  ptr2[2] = 100;
  ptr2[3] = 200;

  GM_ERROR_CALL(FlushMappedBufferRange(GL_TEXTURE_2D, 1, 1),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(FlushMappedBufferRange(GL_ELEMENT_ARRAY_BUFFER, 1, 2),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(FlushMappedBufferRange(GL_ARRAY_BUFFER, -1, 1),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(FlushMappedBufferRange(GL_ARRAY_BUFFER, 1, -1),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(FlushMappedBufferRange(GL_ARRAY_BUFFER, 1, 20),
                GL_INVALID_VALUE);
  GM_CALL(FlushMappedBufferRange(GL_ARRAY_BUFFER, 1, 2));
  GM_CALL(FlushMappedBufferRange(GL_ARRAY_BUFFER, 2, 2));
  GM_CALL(UnmapBuffer(GL_ARRAY_BUFFER));
}

TEST(MockGraphicsManagerTest, FrameAndRenderBuffers) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GM_ERROR_CALL(CheckFramebufferStatus(GL_BLEND), GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            gm->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CHECK_NO_ERROR;

  // GenFramebuffers.
  GLuint fb;
  GM_ERROR_CALL(GenFramebuffers(-1, &fb), GL_INVALID_VALUE);
  GM_CALL(GenFramebuffers(1, &fb));
  EXPECT_NE(0U, fb);

  // IsFramebuffer.
  EXPECT_TRUE(gm->IsFramebuffer(0U));
  EXPECT_TRUE(gm->IsFramebuffer(fb));
  EXPECT_FALSE(gm->IsFramebuffer(fb + 1U));

  // GenRenderbuffers.
  GLuint color0;
  GM_ERROR_CALL(GenRenderbuffers(-1, &color0), GL_INVALID_VALUE);
  GM_CALL(GenRenderbuffers(1, &color0));
  EXPECT_NE(0U, color0);
  GLuint depth;
  GM_ERROR_CALL(GenRenderbuffers(-1, &depth), GL_INVALID_VALUE);
  GM_CALL(GenRenderbuffers(1, &depth));
  EXPECT_NE(0U, depth);
  GLuint stencil;
  GM_ERROR_CALL(GenRenderbuffers(-1, &stencil), GL_INVALID_VALUE);
  GM_CALL(GenRenderbuffers(1, &stencil));
  EXPECT_NE(0U, stencil);
  EXPECT_NE(color0, depth);
  EXPECT_NE(depth, stencil);
  EXPECT_NE(color0, stencil);

  // IsRenderbuffer.
  EXPECT_TRUE(gm->IsRenderbuffer(0U));
  EXPECT_TRUE(gm->IsRenderbuffer(color0));
  EXPECT_TRUE(gm->IsRenderbuffer(depth));
  EXPECT_TRUE(gm->IsRenderbuffer(stencil));
  EXPECT_FALSE(gm->IsRenderbuffer(stencil + depth + color0));

  // Can't call on framebuffer 0.
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, color0), GL_INVALID_OPERATION);
  GLint value;
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value), GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, 0, 0),
                GL_INVALID_OPERATION);

  // Check values before binding a framebuffer.
  EXPECT_EQ(8, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(8, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(16, GetInt(gm, GL_DEPTH_BITS));
  EXPECT_EQ(8, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(8, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(8, GetInt(gm, GL_STENCIL_BITS));

  // BindFramebuffer.
  EXPECT_EQ(0, GetInt(gm, GL_FRAMEBUFFER_BINDING));
  GM_ERROR_CALL(BindFramebuffer(GL_TEXTURE_2D, fb), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindFramebuffer(GL_FRAMEBUFFER, 3U), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(gm, GL_FRAMEBUFFER_BINDING));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  EXPECT_EQ(static_cast<GLint>(fb), GetInt(gm, GL_FRAMEBUFFER_BINDING));

  // By default these are 0.
  EXPECT_EQ(0, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_DEPTH_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));

  // FramebufferRenderbuffer.
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_DEPTH_TEST, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, color0), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_BLEND_COLOR,
                              GL_RENDERBUFFER, color0), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_VERTEX_SHADER, color0), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, 5U), GL_INVALID_OPERATION);

  // Should be no attachments.
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  // Error to query name if there is no binding.
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, NULL), GL_INVALID_ENUM);

  // Status is incomplete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            gm->CheckFramebufferStatus(GL_FRAMEBUFFER));

  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, color0));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            gm->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depth));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            gm->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, stencil));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            gm->CheckFramebufferStatus(GL_FRAMEBUFFER));

  // GetFramebufferAttachmentParameteriv.
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAGMENT_SHADER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, NULL), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_SHADER_COMPILER,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, NULL), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_DEPTH_TEST, NULL), GL_INVALID_ENUM);

  // Check values.
  EXPECT_EQ(GL_RENDERBUFFER, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(static_cast<GLint>(color0), GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));
  EXPECT_EQ(GL_RENDERBUFFER, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(static_cast<GLint>(depth), GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));
  EXPECT_EQ(GL_RENDERBUFFER, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(static_cast<GLint>(stencil), GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));

  // Invalid calls since binding is not a texture.
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, NULL), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, NULL),
      GL_INVALID_ENUM);

  int width = 1024;
  int height = 1024;
  // Can't call if no renderbuffer is bound.
  GM_ERROR_CALL(
      GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, NULL),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, width, height),
                  GL_INVALID_OPERATION);

  // BindRenderbuffer.
  EXPECT_EQ(0, GetInt(gm, GL_RENDERBUFFER_BINDING));
  GM_ERROR_CALL(BindRenderbuffer(GL_TEXTURE_2D, color0), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindRenderbuffer(GL_RENDERBUFFER, 4U), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(gm, GL_RENDERBUFFER_BINDING));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color0));
  EXPECT_EQ(static_cast<GLint>(color0), GetInt(gm, GL_RENDERBUFFER_BINDING));

  // Check defaults using GetRenderbufferInt.
  GM_ERROR_CALL(
      GetRenderbufferParameteriv(GL_COMPILE_STATUS, GL_RENDERBUFFER_WIDTH,
                                 NULL), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_VERSION, NULL),
      GL_INVALID_ENUM);
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT));
  EXPECT_EQ(GL_RGBA4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                         GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(0, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_DEPTH_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));

  // RenderbufferStorage.
  int max_size = GetInt(gm, GL_MAX_RENDERBUFFER_SIZE);
  GM_ERROR_CALL(RenderbufferStorage(GL_DELETE_STATUS, GL_RGB565, width, height),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_ALPHA, width, height),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, -1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, width, -1),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, max_size,
                                    height), GL_INVALID_VALUE);
  GM_ERROR_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, width,
                                    max_size), GL_INVALID_VALUE);
  // RGB565
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, width, height));
  EXPECT_EQ(width,
            GetRenderbufferInt(gm, GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH));
  EXPECT_EQ(height,
            GetRenderbufferInt(gm, GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT));
  EXPECT_EQ(GL_RGB565, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                          GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(6, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(5, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(6, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_DEPTH_BITS));
  // RGBA4
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, width, height));
  EXPECT_EQ(GL_RGBA4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                         GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(4, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(4, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(4, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(4, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_DEPTH_BITS));
  // RGB5_A1
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB5_A1, width, height));
  EXPECT_EQ(GL_RGB5_A1, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                           GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(1, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(5, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(1, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_DEPTH_BITS));

  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, depth));
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                              width, height));
  EXPECT_EQ(GL_DEPTH_COMPONENT16,
            GetRenderbufferInt(gm, GL_RENDERBUFFER,
                               GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(16, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(5, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(1, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(16, GetInt(gm, GL_DEPTH_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, stencil));
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                              128, 128));

  // Status is incomplete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS),
            gm->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                              width, height));
  EXPECT_EQ(GL_STENCIL_INDEX8,
            GetRenderbufferInt(gm, GL_RENDERBUFFER,
                               GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(8, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(5, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(1, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(16, GetInt(gm, GL_DEPTH_BITS));
  EXPECT_EQ(8, GetInt(gm, GL_STENCIL_BITS));

  // Status is complete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            gm->CheckFramebufferStatus(GL_FRAMEBUFFER));

  // FramebufferTexture2D.
  // Create a texture.
  GLint level = 0;
  GLint internal_format = GL_RGBA;
  GLint border = 0;
  GLenum format = GL_RGBA;
  GLenum type = GL_UNSIGNED_BYTE;
  GLuint tex_id;
  GLuint cube_tex_id;
  GM_CALL(GenTextures(1, &tex_id));
  GM_CALL(GenTextures(1, &cube_tex_id));
  GM_CALL(BindTexture(GL_TEXTURE_2D, tex_id));
  GM_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width,
                     height, border, format, type, NULL));

  GM_ERROR_CALL(FramebufferTexture2D(GL_FRONT, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, tex_id, 0),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_BACK,
                                     GL_TEXTURE_2D, tex_id, 0),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_CCW, tex_id, 0), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, 3U, 0),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, tex_id, 1),
                GL_INVALID_OPERATION);

  // Bind the texture.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, tex_id, 1));
  // Now we have a texture bound.
  EXPECT_EQ(static_cast<GLint>(tex_id), GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));
  EXPECT_EQ(1, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL));
  // Not a cube map.
  EXPECT_EQ(0, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE));

  // Use a non-0 level.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, tex_id, 1));
  // Check that we have a texture bound.
  EXPECT_EQ(GL_TEXTURE, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  // Ok with texture 0, since that disables the attachment.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, 0, 0));
  // Check that we have no texture bound.
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));

  // Bind more for coverage.
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, cube_tex_id));
  GM_CALL(TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, level, internal_format,
                     width, height, border, format, type, NULL));
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, cube_tex_id, 1));
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, tex_id, 1));
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D, tex_id, 1));
  EXPECT_EQ(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GetFramebufferAttachmentInt(
          gm, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE));

  // ReadPixels.
  int x = 0;
  int y = 0;
  GM_ERROR_CALL(ReadPixels(x, y, -1, height, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(ReadPixels(x, y, width, -1, format, type, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(ReadPixels(x, y, width, height, GL_RED_BITS, type, NULL),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(ReadPixels(x, y, width, height, format, GL_VENDOR, NULL),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(ReadPixels(x, y, width, height, GL_RGB,
                           GL_UNSIGNED_SHORT_4_4_4_4, NULL),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(ReadPixels(x, y, width, height, GL_RGBA,
                           GL_UNSIGNED_SHORT_5_6_5, NULL),
                GL_INVALID_OPERATION);
  // Framebuffer is incomplete.
  GM_ERROR_CALL(ReadPixels(x, y, width, height, format, type, NULL),
                GL_INVALID_FRAMEBUFFER_OPERATION);

  GM_ERROR_CALL(DeleteFramebuffers(-1, &fb), GL_INVALID_VALUE);
  GM_CALL(DeleteFramebuffers(1, &fb));
  GM_CALL(DeleteFramebuffers(1, &fb));

  GM_ERROR_CALL(DeleteRenderbuffers(-1, &color0), GL_INVALID_VALUE);
  GM_CALL(DeleteRenderbuffers(1, &color0));
  GM_CALL(DeleteRenderbuffers(1, &color0));
  GM_CALL(DeleteRenderbuffers(1, &stencil));

  GM_ERROR_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb), GL_INVALID_OPERATION);
  GM_ERROR_CALL(BindRenderbuffer(GL_RENDERBUFFER, color0),
                GL_INVALID_OPERATION);

  // Works with framebuffer 0.
  GM_CALL(ReadPixels(x, y, width, height, format, type, NULL));
}

TEST(MockGraphicsManagerTest, MultisampleFramebuffers) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // GenFramebuffers.
  GLuint fb;
  GM_ERROR_CALL(GenFramebuffers(-1, &fb), GL_INVALID_VALUE);
  GM_CALL(GenFramebuffers(1, &fb));
  EXPECT_NE(0U, fb);

  // GenRenderbuffers.
  GLuint color0;
  GM_CALL(GenRenderbuffers(1, &color0));
  EXPECT_NE(0U, color0);
  GLuint depth;
  GM_CALL(GenRenderbuffers(1, &depth));
  EXPECT_NE(0U, depth);
  GLuint stencil;
  GM_CALL(GenRenderbuffers(1, &stencil));
  EXPECT_NE(0U, stencil);
  EXPECT_NE(color0, depth);
  EXPECT_NE(depth, stencil);
  EXPECT_NE(color0, stencil);

  // BindFramebuffer.
  EXPECT_EQ(0, GetInt(gm, GL_FRAMEBUFFER_BINDING));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  EXPECT_EQ(static_cast<GLint>(fb), GetInt(gm, GL_FRAMEBUFFER_BINDING));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, color0));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depth));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, stencil));

  // BindRenderbuffer.
  EXPECT_EQ(0, GetInt(gm, GL_RENDERBUFFER_BINDING));
  GM_ERROR_CALL(BindRenderbuffer(GL_TEXTURE_2D, color0), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindRenderbuffer(GL_RENDERBUFFER, 4U), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(gm, GL_RENDERBUFFER_BINDING));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color0));
  EXPECT_EQ(static_cast<GLint>(color0), GetInt(gm, GL_RENDERBUFFER_BINDING));

  // RenderbufferStorageMultisample.
  int width = 1024;
  int height = 1024;
  int samples = 8;
  int max_size = GetInt(gm, GL_MAX_RENDERBUFFER_SIZE);
  GM_ERROR_CALL(RenderbufferStorageMultisample(GL_DELETE_STATUS, samples,
                                               GL_RGB565, width, height),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                               GL_ALPHA, width, height),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                               GL_RGB565, -1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                               GL_RGB565, width, -1),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                               GL_RGB565, max_size, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                               GL_RGB565, width, max_size),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, 19,
                                               GL_RGB565, width, max_size),
                GL_INVALID_VALUE);
  // RGB565
  GM_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                         GL_RGB565, width, height));
  EXPECT_EQ(width,
            GetRenderbufferInt(gm, GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH));
  EXPECT_EQ(height,
            GetRenderbufferInt(gm, GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT));
  EXPECT_EQ(GL_RGB565, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                          GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(6, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(5, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(6, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_DEPTH_BITS));
  // RGBA4
  GM_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                         GL_RGBA4, width, height));
  EXPECT_EQ(GL_RGBA4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                         GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(4, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(4, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(4, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(4, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_DEPTH_BITS));
  // RGB5_A1
  GM_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                         GL_RGB5_A1, width, height));
  EXPECT_EQ(GL_RGB5_A1, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                           GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(1, GetRenderbufferInt(gm, GL_RENDERBUFFER,
                                  GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(5, GetInt(gm, GL_RED_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(gm, GL_BLUE_BITS));
  EXPECT_EQ(1, GetInt(gm, GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(gm, GL_DEPTH_BITS));

  // Create a multisample texture.
  GLint internal_format = GL_RGBA;
  GLuint tex_id;
  GLuint cube_tex_id;
  GM_CALL(GenTextures(1, &tex_id));
  GM_CALL(GenTextures(1, &cube_tex_id));
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE, tex_id));
  GM_CALL(TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                internal_format, width, height, true));

  GM_ERROR_CALL(FramebufferTexture2D(GL_FRONT, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D_MULTISAMPLE, tex_id, 0),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_BACK,
                                     GL_TEXTURE_2D_MULTISAMPLE, tex_id, 0),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_CCW, tex_id, 0), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D_MULTISAMPLE, 3U, 0),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, tex_id, 1),
                GL_INVALID_OPERATION);

  // Bind the texture.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D_MULTISAMPLE, tex_id, 0));
  // Now we have a texture bound.
  EXPECT_EQ(static_cast<GLint>(tex_id), GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));
  EXPECT_EQ(0, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL));

  // Check that we have a texture bound.
  EXPECT_EQ(GL_TEXTURE, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  // Ok with texture 0, since that disables the attachment.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D_MULTISAMPLE, 0, 0));
  // Check that we have no texture bound.
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(gm,
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
}

TEST(MockGraphicsManagerTest, ResolveMultisampleFramebuffer) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  int width = 1024;
  int height = 1024;
  int samples = 8;

  // 1. Test the valid case
  // Read buffer
  GLuint multisample_sample_read_buffer;
  GM_CALL(GenFramebuffers(1, &multisample_sample_read_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, multisample_sample_read_buffer));
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height, samples);
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_DEPTH_COMPONENT16, GL_DEPTH_ATTACHMENT, width, height, samples);
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_DEPTH24_STENCIL8, GL_STENCIL_ATTACHMENT, width, height, samples);

  // Draw buffer
  GLuint draw_frame_buffer;
  GM_CALL(GenFramebuffers(1, &draw_frame_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, draw_frame_buffer));
  AllocateAndAttachRenderBuffer(
      gm, GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, multisample_sample_read_buffer));
  EXPECT_EQ(static_cast<GLint>(draw_frame_buffer),
            GetInt(gm, GL_DRAW_FRAMEBUFFER_BINDING));
  EXPECT_EQ(static_cast<GLint>(multisample_sample_read_buffer),
            GetInt(gm, GL_READ_FRAMEBUFFER_BINDING));
  GM_CALL(ResolveMultisampleFramebuffer());

  // 2. GL_INVALID_OPERATION: SAMPLE_BUFFERS for the read framebuffer is zero.
  GLuint zero_sample_size_read_buffer;
  GM_CALL(GenFramebuffers(1, &zero_sample_size_read_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, zero_sample_size_read_buffer));
  // Set sample size to be zero.
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height, 0);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, zero_sample_size_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(), GL_INVALID_OPERATION);

  // 3. GL_INVALID_OPERATION: Sample size for the draw framebuffer is
  // greater than zero.
  GLuint multisample_draw_buffer;
  GM_CALL(GenFramebuffers(1, &multisample_draw_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, multisample_draw_buffer));
  // draw buffer is multisample frame buffer.
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height, 1);
  GM_CALL(BindFramebuffer(
      GL_DRAW_FRAMEBUFFER, multisample_draw_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, multisample_draw_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(), GL_INVALID_OPERATION);

  // 4 GL_INVALID_OPERATION: Read bufer doesn't have a color attachment.
  GLuint no_color_attachment_read_buffer;
  GM_CALL(GenFramebuffers(1, &no_color_attachment_read_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, no_color_attachment_read_buffer));
  // All other attachments are available except the color buffer attachment.
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_DEPTH_COMPONENT16, GL_DEPTH_ATTACHMENT, width, height, samples);
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_DEPTH24_STENCIL8, GL_STENCIL_ATTACHMENT, width, height, samples);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer));
  GM_CALL(BindFramebuffer(
      GL_READ_FRAMEBUFFER, no_color_attachment_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(), GL_INVALID_OPERATION);

  // 5 GL_INVALID_OPERATION: Draw buffer doesn't have a color attachment.
  GLuint no_color_attachment_draw_buffer;
  GM_CALL(GenFramebuffers(1, &no_color_attachment_draw_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, no_color_attachment_draw_buffer));
  // draw buffer doesn't have a color attachment.
  AllocateAndAttachRenderBuffer(
      gm, GL_DEPTH_COMPONENT16, GL_DEPTH_ATTACHMENT, width, height);
  GM_CALL(BindFramebuffer(
      GL_DRAW_FRAMEBUFFER, no_color_attachment_draw_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, multisample_sample_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(), GL_INVALID_OPERATION);

  // 6 GL_INVALID_OPERATION: The dimensions of the read and draw framebuffers
  // is not identical.
  GLuint small_dimension_draw_buffer;
  GM_CALL(GenFramebuffers(1, &small_dimension_draw_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, small_dimension_draw_buffer));
  // The dimension of draw buffer is half of the size of read buffer.
  AllocateAndAttachRenderBuffer(
      gm, GL_RGBA4, GL_COLOR_ATTACHMENT0, width / 2, height / 2);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, small_dimension_draw_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, multisample_sample_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(), GL_INVALID_OPERATION);

  // 7 GL_INVALID_OPERATION: The components in the format of the draw
  // framebuffer's color attachment are not present in the format of the read
  // framebuffer's color attachment.
  GLuint format_different_draw_buffer;
  GM_CALL(GenFramebuffers(1, &format_different_draw_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, format_different_draw_buffer));
  // The format of draw color buffer (GL_RGBA8) is different from the read
  // color buffer GL_RGBA4)
  AllocateAndAttachRenderBuffer(
      gm, GL_RGBA8, GL_COLOR_ATTACHMENT0, width, height);
  GM_CALL(BindFramebuffer(
      GL_DRAW_FRAMEBUFFER, format_different_draw_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, multisample_sample_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(), GL_INVALID_OPERATION);

  // 8 INVALID_FRAMEBUFFER_OPERATION: Draw buffer is not framebuffer complete.
  GLuint incomplete_draw_buffer;
  GM_CALL(GenFramebuffers(1, &incomplete_draw_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, incomplete_draw_buffer));
  AllocateAndAttachRenderBuffer(
      gm, GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height);
  // The format for the depth buffer is bad.
  AllocateAndAttachRenderBuffer(
      gm, GL_RGBA4, GL_DEPTH_ATTACHMENT, width, height);
  GM_CALL(BindFramebuffer(
      GL_DRAW_FRAMEBUFFER, incomplete_draw_buffer));
  GM_CALL(BindFramebuffer(
      GL_READ_FRAMEBUFFER, multisample_sample_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(),
                GL_INVALID_FRAMEBUFFER_OPERATION);

  // 9 INVALID_FRAMEBUFFER_OPERATION: read buffer is not framebuffer complete.
  GLuint incomplete_read_buffer;
  GM_CALL(GenFramebuffers(1, &incomplete_read_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, incomplete_read_buffer));
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height, samples);
  // The format for the depth buffer is bad.
  AllocateAndAttachMultisampleRenderBuffer(
      gm, GL_RGBA4, GL_DEPTH_ATTACHMENT, width, height, samples);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, incomplete_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(),
                GL_INVALID_FRAMEBUFFER_OPERATION);
}

TEST(MockGraphicsManagerTest, IsExtensionSupportedParsesUnprefixedExtension) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  gm->SetExtensionsString("GLX_SGI_swap_control");
  EXPECT_TRUE(gm->IsExtensionSupported("swap_control"));

  gm->SetExtensionsString("WGL_EXT_swap_control");
  EXPECT_TRUE(gm->IsExtensionSupported("swap_control"));

  gm->SetExtensionsString("FOO_bar_BAZ");
  EXPECT_FALSE(gm->IsExtensionSupported("FOO_bar_BAZ"));
  EXPECT_FALSE(gm->IsExtensionSupported("bar"));
  EXPECT_FALSE(gm->IsExtensionSupported("BAZ"));
  EXPECT_TRUE(gm->IsExtensionSupported("bar_BAZ"));
}

TEST(MockGraphicsManagerTest, FunctionGroupsAreDisabledByMissingExtensions) {
#if defined(ION_PLATFORM_ANDROID) || defined(ION_PLATFORM_GENERIC_ARM)
  static const bool kHasVertexArrays = false;
#else
  static const bool kHasVertexArrays = true;
#endif

  {
    MockVisual visual(600, 500);
    MockGraphicsManagerPtr gm(new MockGraphicsManager());

    // These tests are to increase coverage.
    EXPECT_TRUE(gm->IsExtensionSupported("debug_label"));
    EXPECT_EQ(kHasVertexArrays,
              gm->IsExtensionSupported("vertex_array_object"));
    EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kDebugLabel));
    EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
    gm->SetExtensionsString("GL_EXT_debug_label GL_OES_vertex_array_object");
    EXPECT_TRUE(gm->IsExtensionSupported("debug_label"));
    EXPECT_EQ(kHasVertexArrays,
              gm->IsExtensionSupported("vertex_array_object"));
    EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kDebugLabel));
    EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
    gm->SetExtensionsString("GL_OES_vertex_array_object");
    EXPECT_FALSE(gm->IsExtensionSupported("debug_label"));
    EXPECT_EQ(kHasVertexArrays,
              gm->IsExtensionSupported("vertex_array_object"));
    EXPECT_FALSE(gm->IsFunctionGroupAvailable(GraphicsManager::kDebugLabel));
    EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
    gm->SetExtensionsString("GL_EXT_debug_label");
    EXPECT_TRUE(gm->IsExtensionSupported("debug_label"));
    EXPECT_FALSE(gm->IsExtensionSupported("vertex_array_object"));
    EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kDebugLabel));
    EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));

    gm->SetVersionString("1.2 Ion OpenGL");
    EXPECT_FALSE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
  }

  // Check some special cases.
  //
  // Check that if GenVertexArrays fails that the extension is disabled.
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
  gm->SetForceFunctionFailure("GenVertexArrays", true);
  gm->InitGlInfo();
  EXPECT_FALSE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
  gm->SetForceFunctionFailure("GenVertexArrays", false);
}

TEST(MockGraphicsManagerTest, GetString) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  EXPECT_EQ("GL_OES_blend_func_separate", GetStringi(gm, GL_EXTENSIONS, 0));
  EXPECT_EQ("GL_OES_blend_subtract", GetStringi(gm, GL_EXTENSIONS, 1));
  GLint count = GetInt(gm, GL_NUM_EXTENSIONS);
  EXPECT_EQ(54, count);
  GM_ERROR_CALL(GetStringi(GL_EXTENSIONS, count), GL_INVALID_VALUE);

  // These tests are to increase coverage.
  EXPECT_TRUE(gm->IsExtensionSupported("mapbuffer"));
  EXPECT_TRUE(gm->IsExtensionSupported("texture_filter_anisotropic"));
  gm->SetExtensionsString("test extensions");
  EXPECT_FALSE(gm->IsExtensionSupported("mapbuffer"));
  EXPECT_FALSE(gm->IsExtensionSupported("texture_filter_anisotropic"));
  EXPECT_EQ("test extensions", GetString(gm, GL_EXTENSIONS));
  EXPECT_EQ("Google", GetString(gm, GL_VENDOR));
  gm->SetVendorString("I like turtles");
  EXPECT_EQ("I like turtles", GetString(gm, GL_VENDOR));
  EXPECT_EQ("Ion fake OpenGL / ES", GetString(gm, GL_RENDERER));
  EXPECT_EQ("3.3 Ion OpenGL / ES", GetString(gm, GL_VERSION));
  gm->SetVersionString("test version");
  EXPECT_EQ("test version", GetString(gm, GL_VERSION));
  EXPECT_EQ("1.10 Ion", GetString(gm, GL_SHADING_LANGUAGE_VERSION));
  GM_ERROR_CALL(GetString(GL_CULL_FACE_MODE), GL_INVALID_ENUM);
  GM_ERROR_CALL(GetString(GL_FRONT), GL_INVALID_ENUM);

  gm->SetForceFunctionFailure("GetString", true);
  gm->SetExtensionsString("GLX_SGI_swap_control GL_OES_blend_func_separate");
  count = GetInt(gm, GL_NUM_EXTENSIONS);
  EXPECT_EQ(2, count);

  EXPECT_EQ("GLX_SGI_swap_control", GetStringi(gm, GL_EXTENSIONS, 0));
  EXPECT_EQ("GL_OES_blend_func_separate", GetStringi(gm, GL_EXTENSIONS, 1));
  gm->SetForceFunctionFailure("GetString", false);
}

TEST(MockGraphicsManagerTest, ProgramAndShaderFunctions) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // There is no default program.
  GM_ERROR_CALL(AttachShader(0U, 0U), GL_INVALID_VALUE);

  // GetShaderPrecisionFormat.
  GM_ERROR_CALL(
      GetShaderPrecisionFormat(GL_DELETE_STATUS, GL_HIGH_FLOAT, NULL, NULL),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(GetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_RGB, NULL, NULL),
                GL_INVALID_ENUM);
  GLint range[2], precision = 0;
  range[0] = 0;
  range[1] = 0;
  GM_CALL(GetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_FLOAT, range,
                                   &precision));
  EXPECT_EQ(127, range[0]);
  EXPECT_EQ(127, range[1]);
  EXPECT_EQ(23, precision);
  GM_CALL(GetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_HIGH_INT, range,
                                   &precision));
  EXPECT_EQ(127, range[0]);
  EXPECT_EQ(127, range[1]);
  EXPECT_EQ(23, precision);
  GM_CALL(GetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_MEDIUM_FLOAT, range,
                                   &precision));
  EXPECT_EQ(15, range[0]);
  EXPECT_EQ(15, range[1]);
  EXPECT_EQ(10, precision);
  GM_CALL(GetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_MEDIUM_INT, range,
                                   &precision));
  EXPECT_EQ(15, range[0]);
  EXPECT_EQ(15, range[1]);
  EXPECT_EQ(10, precision);
  GM_CALL(GetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_FLOAT, range,
                                   &precision));
  EXPECT_EQ(7, range[0]);
  EXPECT_EQ(7, range[1]);
  EXPECT_EQ(8, precision);
  GM_CALL(GetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_LOW_INT, range,
                                   &precision));
  EXPECT_EQ(7, range[0]);
  EXPECT_EQ(7, range[1]);
  EXPECT_EQ(8, precision);

  GLuint pid = gm->CreateProgram();
  GLuint pid2 = gm->CreateProgram();
  EXPECT_NE(0U, pid);
  EXPECT_NE(0U, pid2);
  EXPECT_EQ(GL_FALSE, gm->IsProgram(0));
  EXPECT_EQ(GL_TRUE, gm->IsProgram(pid));
  EXPECT_EQ(GL_TRUE, gm->IsProgram(pid2));
  EXPECT_EQ(GL_FALSE, gm->IsProgram(pid + pid2));

  GM_CHECK_NO_ERROR;
  GLuint vid = gm->CreateShader(GL_VERTEX_SHADER);
  GLuint vid2 = gm->CreateShader(GL_VERTEX_SHADER);
  GM_CHECK_NO_ERROR;
  EXPECT_NE(0U, vid);
  EXPECT_NE(0U, vid2);
  GLuint fid = gm->CreateShader(GL_FRAGMENT_SHADER);
  GLuint fid2 = gm->CreateShader(GL_FRAGMENT_SHADER);
  GM_CHECK_NO_ERROR;
  EXPECT_NE(0U, fid);
  EXPECT_NE(0U, fid2);
  // Invalid enum returns 0 for the shader id.
  GLuint bad_id = gm->CreateShader(GL_FRONT);
  EXPECT_EQ(0U, bad_id);
  GM_CHECK_ERROR(GL_INVALID_ENUM);
  EXPECT_EQ(GL_FALSE, gm->IsShader(0));
  EXPECT_EQ(GL_TRUE, gm->IsShader(vid));
  EXPECT_EQ(GL_TRUE, gm->IsShader(vid2));
  EXPECT_EQ(GL_TRUE, gm->IsShader(fid));
  EXPECT_EQ(GL_TRUE, gm->IsShader(fid2));
  EXPECT_EQ(GL_FALSE, gm->IsShader(vid + vid2 + fid + fid2));

  // Invalid program ints.
  GM_ERROR_CALL(GetShaderiv(0U, 0U, NULL), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetShaderiv(8U, 0U, NULL), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetShaderiv(vid, GL_RENDERER, NULL), GL_INVALID_ENUM);

  // Check program and shader ints.
  EXPECT_EQ(GL_FALSE, GetProgramInt(gm, pid, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetProgramInt(gm, pid, GL_LINK_STATUS));
  EXPECT_EQ(GL_FALSE, GetProgramInt(gm, pid2, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetProgramInt(gm, pid2, GL_LINK_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gm, vid, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gm, vid, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_VERTEX_SHADER, GetShaderInt(gm, vid, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(gm, vid, GL_SHADER_SOURCE_LENGTH));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gm, vid2, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gm, vid2, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_VERTEX_SHADER, GetShaderInt(gm, vid2, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(gm, vid2, GL_SHADER_SOURCE_LENGTH));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gm, fid, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gm, fid, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_FRAGMENT_SHADER, GetShaderInt(gm, fid, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(gm, fid, GL_SHADER_SOURCE_LENGTH));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gm, fid2, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gm, fid2, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_FRAGMENT_SHADER, GetShaderInt(gm, fid2, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(gm, fid2, GL_SHADER_SOURCE_LENGTH));

  const std::string vertex_source(kVertexSource);
  const std::string fragment_source(kFragmentSource);

  // Cannot compile invalid shaders.
  GM_ERROR_CALL(CompileShader(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(CompileShader(11U), GL_INVALID_VALUE);
  GM_ERROR_CALL(ShaderSource(0U, 0, NULL, NULL), GL_INVALID_VALUE);
  GM_ERROR_CALL(ShaderSource(7U, 0, NULL, NULL), GL_INVALID_VALUE);
  GM_ERROR_CALL(ShaderSource(vid, -1, NULL, NULL), GL_INVALID_VALUE);
  // Valid source.
  {
    GLint length = static_cast<GLuint>(vertex_source.length());
    const char* ptr = vertex_source.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
  }
  {
    GLint length = static_cast<GLuint>(fragment_source.length());
    const char* ptr = fragment_source.c_str();
    GM_CALL(ShaderSource(fid, 1, &ptr, &length));
  }
  // Check that source was set.
  {
    const int kBufLen = 2048;
    char source[kBufLen];
    GLint length;
    GM_ERROR_CALL(GetShaderSource(0U, 0, NULL, NULL), GL_INVALID_VALUE);
    GM_ERROR_CALL(GetShaderSource(7U, 0, NULL, NULL), GL_INVALID_VALUE);
    GM_ERROR_CALL(GetShaderSource(vid, -1, NULL, NULL), GL_INVALID_VALUE);
    // Check vertex source.
    GM_CALL(GetShaderSource(vid, kBufLen, &length, source));
    EXPECT_EQ(static_cast<GLint>(vertex_source.length()) + 1, length);
    EXPECT_EQ(0, vertex_source.compare(source));
    // Check fragment source.
    GM_CALL(GetShaderSource(fid, kBufLen, &length, source));
    EXPECT_EQ(static_cast<GLint>(fragment_source.length()) + 1, length);
    EXPECT_EQ(0, fragment_source.compare(source));

    EXPECT_EQ(static_cast<GLint>(vertex_source.length()) + 1,
              GetShaderInt(gm, vid, GL_SHADER_SOURCE_LENGTH));
    EXPECT_EQ(static_cast<GLint>(fragment_source.length()) + 1,
              GetShaderInt(gm, fid, GL_SHADER_SOURCE_LENGTH));
  }

  // Try to compile shaders.
  GM_CALL(CompileShader(vid));
  GM_CALL(CompileShader(fid));
  EXPECT_EQ(GL_TRUE, GetShaderInt(gm, vid, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_TRUE, GetShaderInt(gm, fid, GL_COMPILE_STATUS));

  // Cannot link a program that does not have valid shaders.
  GM_ERROR_CALL(LinkProgram(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(LinkProgram(pid + pid2), GL_INVALID_VALUE);
  // Cannot validate an invalid program.
  GM_ERROR_CALL(ValidateProgram(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(ValidateProgram(fid + fid2 + vid + vid2), GL_INVALID_VALUE);

  // Check error case.
  GM_ERROR_CALL(GetProgramiv(pid, GL_TEXTURE_2D, NULL), GL_INVALID_ENUM);

  // There should be no shaders attached at first.
  EXPECT_EQ(0, GetProgramInt(gm, pid, GL_ATTACHED_SHADERS));

  // Invalid value is set if an invalid value is used.
  GM_ERROR_CALL(AttachShader(pid + pid2, vid), GL_INVALID_VALUE);
  GM_ERROR_CALL(AttachShader(pid, 0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(AttachShader(0U, vid), GL_INVALID_VALUE);
  EXPECT_EQ(0, GetProgramInt(gm, pid, GL_ATTACHED_SHADERS));

  {
    // GetAttachedShaders.
    GLsizei count;
    GLuint shaders[2];
    // Bad calls.
    GM_ERROR_CALL(GetAttachedShaders(0U, 2, &count, shaders), GL_INVALID_VALUE);
    GM_ERROR_CALL(
        GetAttachedShaders(pid, -1, &count, shaders), GL_INVALID_VALUE);

    GM_CALL(GetAttachedShaders(pid, 2, &count, shaders));
    EXPECT_EQ(0, count);

    // Actually attach the shader.
    GM_CALL(AttachShader(pid, vid));
    EXPECT_EQ(1, GetProgramInt(gm, pid, GL_ATTACHED_SHADERS));
    GM_CALL(GetAttachedShaders(pid, 2, &count, shaders));
    EXPECT_EQ(1, count);
    EXPECT_EQ(vid, shaders[0]);

    // Attaching a shader twice is an invalid operation.
    GM_ERROR_CALL(AttachShader(pid, vid), GL_INVALID_OPERATION);
    EXPECT_EQ(1, GetProgramInt(gm, pid, GL_ATTACHED_SHADERS));
    GM_CALL(GetAttachedShaders(pid, 2, &count, shaders));
    EXPECT_EQ(1, count);
    EXPECT_EQ(vid, shaders[0]);

    // Attach another shader.
    GM_CALL(AttachShader(pid, fid));
    EXPECT_EQ(2, GetProgramInt(gm, pid, GL_ATTACHED_SHADERS));
    GM_CALL(GetAttachedShaders(pid, 2, &count, shaders));
    EXPECT_EQ(2, count);
    EXPECT_EQ(vid, shaders[0]);
    EXPECT_EQ(fid, shaders[1]);
  }

  // Can't use an unlinked program.
  GM_ERROR_CALL(UseProgram(pid2), GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetUniformfv(pid2, 0, NULL), GL_INVALID_OPERATION);

  // Link the program.
  EXPECT_EQ(GL_FALSE, GetProgramInt(gm, pid, GL_LINK_STATUS));
  GM_CALL(LinkProgram(pid));
  EXPECT_EQ(GL_TRUE, GetProgramInt(gm, pid, GL_LINK_STATUS));
  EXPECT_EQ(GL_FALSE, GetProgramInt(gm, pid, GL_VALIDATE_STATUS));
  GM_CALL(ValidateProgram(pid));
  EXPECT_EQ(GL_TRUE, GetProgramInt(gm, pid, GL_VALIDATE_STATUS));

  // The default program is none.
  EXPECT_EQ(0, GetInt(gm, GL_CURRENT_PROGRAM));

  // Can't set an invalid program.
  GM_ERROR_CALL(UseProgram(5U), GL_INVALID_VALUE);

  // Set a valid program.
  GM_CALL(UseProgram(pid));
  EXPECT_EQ(pid, static_cast<GLuint>(GetInt(gm, GL_CURRENT_PROGRAM)));
  GM_CALL(UseProgram(0));
  EXPECT_EQ(0, GetInt(gm, GL_CURRENT_PROGRAM));
  GM_CALL(UseProgram(pid));
  EXPECT_EQ(pid, static_cast<GLuint>(GetInt(gm, GL_CURRENT_PROGRAM)));

  // Can't get log of invalids.
  GM_ERROR_CALL(GetShaderInfoLog(0U, 0, NULL, NULL), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetShaderInfoLog(vid + vid2 + fid + fid2, 0, NULL, NULL),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(GetProgramInfoLog(0U, 0, NULL, NULL), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetProgramInfoLog(pid + pid2, 0, NULL, NULL), GL_INVALID_VALUE);

  {
    // Validate calls, but we don't support compilation, so the logs are NULL
    // and length is 0.
    static const int kBufLen = 64;
    char log[kBufLen];
    GLint length;
    GM_CALL(GetShaderInfoLog(vid, kBufLen, &length, log));
    EXPECT_EQ(0, length);
    GM_CALL(GetShaderInfoLog(fid, kBufLen, &length, log));
    EXPECT_EQ(0, length);
    GM_CALL(GetProgramInfoLog(pid, kBufLen, &length, log));
    EXPECT_EQ(0, length);
  }

  // We don't support info logs, but there should be no errors.
  EXPECT_EQ(0, GetProgramInt(gm, pid, GL_INFO_LOG_LENGTH));
  EXPECT_EQ(0, GetShaderInt(gm, vid, GL_INFO_LOG_LENGTH));

  // Deleting invalid ids sets an invalid value error.
  GM_ERROR_CALL(DeleteShader(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(DeleteShader(vid + vid2 + fid + fid2), GL_INVALID_VALUE);
  GM_ERROR_CALL(DeleteProgram(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(DeleteProgram(pid + pid2), GL_INVALID_VALUE);

  // Delete a valid program and shader.
  GM_CALL(DeleteProgram(pid2));
  EXPECT_EQ(GL_TRUE, GetProgramInt(gm, pid2, GL_DELETE_STATUS));
  GM_CALL(DeleteShader(vid2));
  EXPECT_EQ(GL_TRUE, GetShaderInt(gm, vid2, GL_DELETE_STATUS));
  // Can't set the source of a deleted shader.
  GM_ERROR_CALL(ShaderSource(vid2, 0, NULL, NULL), GL_INVALID_OPERATION);
  // Can't compile a deleted shader.
  GM_ERROR_CALL(CompileShader(vid2), GL_INVALID_OPERATION);
  // Can't get a uniform location of an unlinked program.
  GM_ERROR_CALL(GetUniformLocation(pid2, "uni_v2f"), GL_INVALID_OPERATION);

  // Can't link a deleted program.
  GM_ERROR_CALL(LinkProgram(pid2), GL_INVALID_OPERATION);
  // Can't use a deleted program.
  GM_ERROR_CALL(UseProgram(pid2), GL_INVALID_OPERATION);
  // Can't validate a deleted program.
  GM_ERROR_CALL(ValidateProgram(pid2), GL_INVALID_OPERATION);

  // Check attribute and uniform counts.
  EXPECT_EQ(7, GetProgramInt(gm, pid, GL_ACTIVE_ATTRIBUTES));
  EXPECT_EQ(0, GetProgramInt(gm, pid2, GL_ACTIVE_ATTRIBUTES));
  EXPECT_EQ(86, GetProgramInt(gm, pid, GL_ACTIVE_UNIFORMS));
  EXPECT_EQ(0, GetProgramInt(gm, pid2, GL_ACTIVE_UNIFORMS));
  // Valid attribute max length.
  EXPECT_EQ(9, GetProgramInt(gm, pid, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH));
  // Valid uniform max length.
  EXPECT_EQ(14, GetProgramInt(gm, pid, GL_ACTIVE_UNIFORM_MAX_LENGTH));

  // BindAttribLocation
  // Check invalid values.
  GM_ERROR_CALL(BindAttribLocation(0U, 0U, "name"), GL_INVALID_VALUE);
  GM_ERROR_CALL(BindAttribLocation(4U, 0U, "name"), GL_INVALID_VALUE);
  // Out of range attribute index.
  GM_ERROR_CALL(BindAttribLocation(pid, 100U, "name"), GL_INVALID_VALUE);
  // Deleted program.
  GM_ERROR_CALL(BindAttribLocation(pid2, 0U, "name"), GL_INVALID_OPERATION);
  // Invalid name.
  GM_ERROR_CALL(BindAttribLocation(pid2, 0U, "gl_Normal"),
                GL_INVALID_OPERATION);
  GM_CALL(BindAttribLocation(pid, 0U, "attr_f"));
  GM_CALL(BindAttribLocation(pid, 1U, "attr_v2f"));
  // GetAttribLocation
  // Check invalid values.
  GM_ERROR_CALL(GetAttribLocation(0U, "name"), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetAttribLocation(4U, "name"), GL_INVALID_VALUE);
  // Deleted program.
  GM_ERROR_CALL(GetAttribLocation(pid2, "name"), GL_INVALID_OPERATION);
  // Unknown attribute.
  EXPECT_EQ(-1, gm->GetAttribLocation(pid, "name"));
  EXPECT_EQ(-1, gm->GetAttribLocation(pid, "gl_Position"));
  EXPECT_EQ(0, gm->GetAttribLocation(pid, "attr_f"));
  EXPECT_EQ(1, gm->GetAttribLocation(pid, "attr_v2f"));
  EXPECT_EQ(2, gm->GetAttribLocation(pid, "attr_v3f"));
  EXPECT_EQ(3, gm->GetAttribLocation(pid, "attr_v4f"));
  // For matrix attributes, the returned location is the index of the first
  // column of the matrix.
  EXPECT_EQ(4, gm->GetAttribLocation(pid, "attr_m2f"));
  EXPECT_EQ(6, gm->GetAttribLocation(pid, "attr_m3f"));
  EXPECT_EQ(9, gm->GetAttribLocation(pid, "attr_m4f"));
  GM_CHECK_NO_ERROR;

  // Check that no additional attributes were added.
  EXPECT_EQ(7, GetProgramInt(gm, pid, GL_ACTIVE_ATTRIBUTES));
  EXPECT_EQ(0, GetProgramInt(gm, pid2, GL_ACTIVE_ATTRIBUTES));

  {
    // GetActiveAttrib
    GLsizei length;
    GLint size;
    GLenum type;
    GLchar name[32];
    GM_ERROR_CALL(GetActiveAttrib(0, 0, 32, &length, &size, &type, name),
                  GL_INVALID_VALUE);
    GM_ERROR_CALL(GetActiveAttrib(pid, 13, 32, &length, &size, &type, name),
                  GL_INVALID_VALUE);
    GM_ERROR_CALL(GetActiveAttrib(pid, 0, -1, &length, &size, &type, name),
                  GL_INVALID_VALUE);

    // Successful calls.
    GM_CALL(GetActiveAttrib(pid, 0U, 32, &length, &size, &type, name));
    EXPECT_EQ(std::string("attr_f"), std::string(name));
    EXPECT_EQ(7, length);
    EXPECT_EQ(1, size);
    EXPECT_EQ(static_cast<GLenum>(GL_FLOAT), type);
    GM_CALL(GetActiveAttrib(pid, 1U, 32, &length, &size, &type, name));
    EXPECT_EQ(std::string("attr_v2f"), std::string(name));
    EXPECT_EQ(9, length);
    EXPECT_EQ(1, size);
    EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_VEC2), type);
    GM_CALL(GetActiveAttrib(pid, 2U, 32, &length, &size, &type, name));
    EXPECT_EQ(std::string("attr_v3f"), std::string(name));
    EXPECT_EQ(9, length);
    EXPECT_EQ(1, size);
    EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_VEC3), type);
    GM_CALL(GetActiveAttrib(pid, 3U, 32, &length, &size, &type, name));
    EXPECT_EQ(std::string("attr_v4f"), std::string(name));
    EXPECT_EQ(9, length);
    EXPECT_EQ(1, size);
    EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_VEC4), type);
    GM_CALL(GetActiveAttrib(pid, 4U, 32, &length, &size, &type, name));
    EXPECT_EQ(std::string("attr_m2f"), std::string(name));
    EXPECT_EQ(9, length);
    EXPECT_EQ(1, size);
    EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_MAT2), type);
    GM_CALL(GetActiveAttrib(pid, 5U, 32, &length, &size, &type, name));
    EXPECT_EQ(std::string("attr_m3f"), std::string(name));
    EXPECT_EQ(9, length);
    EXPECT_EQ(1, size);
    EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_MAT3), type);
    GM_CALL(GetActiveAttrib(pid, 6U, 32, &length, &size, &type, name));
    EXPECT_EQ(std::string("attr_m4f"), std::string(name));
    EXPECT_EQ(9, length);
    EXPECT_EQ(1, size);
    EXPECT_EQ(static_cast<GLenum>(GL_FLOAT_MAT4), type);
  }
}

TEST(MockGraphicsManagerTest, Uniforms) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  const std::string vertex_source(kVertexSource);
  const std::string fragment_source(kFragmentSource);
  GLuint vid = gm->CreateShader(GL_VERTEX_SHADER);
  GLuint fid = gm->CreateShader(GL_FRAGMENT_SHADER);
  GLuint vid2 = gm->CreateShader(GL_VERTEX_SHADER);
  GLuint fid2 = gm->CreateShader(GL_FRAGMENT_SHADER);
  GM_CHECK_NO_ERROR;
  {
    GLint length = static_cast<GLuint>(vertex_source.length());
    const char* ptr = vertex_source.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
  }
  {
    GLint length = static_cast<GLuint>(fragment_source.length());
    const char* ptr = fragment_source.c_str();
    GM_CALL(ShaderSource(fid, 1, &ptr, &length));
  }
  GLuint pid = gm->CreateProgram();
  GLuint pid2 = gm->CreateProgram();
  GM_CALL(CompileShader(vid));
  GM_CALL(CompileShader(fid));
  GM_CALL(AttachShader(pid, vid));
  GM_CALL(AttachShader(pid, fid));
  GM_CALL(LinkProgram(pid));
  GM_CALL(UseProgram(pid));

  // Uniform tests.
  GM_ERROR_CALL(GetUniformLocation(pid2, "uni_v2f"), GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetUniformLocation(0U, "uni_v2f"), GL_INVALID_VALUE);
  EXPECT_EQ(-1, gm->GetUniformLocation(0U, "attr_f"));
  gm->GetError();  // Clear the error.

  // Set all non-array uniform values.
  UniformInfo uniforms[] = {
    {"uni_f", GL_FLOAT, 1, UniformInfo::kFloat, -1, {-1, -1, -1, -1}},
    {"uni_v2f", GL_FLOAT_VEC2, 2, UniformInfo::kFloat, -1, {-1, -1, -1, -1}},
    {"uni_v3f", GL_FLOAT_VEC3, 3, UniformInfo::kFloat, -1, {-1, -1, -1, -1}},
    {"uni_v4f", GL_FLOAT_VEC4, 4, UniformInfo::kFloat, -1, {-1, -1, -1, -1}},
    {"uni_i", GL_INT, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"uni_v2i", GL_INT_VEC2, 2, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"uni_v3i", GL_INT_VEC3, 3, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"uni_v4i", GL_INT_VEC4, 4, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"uni_u", GL_UNSIGNED_INT, 1, UniformInfo::kUnsignedInt, -1,
     {-1, -1, -1, -1}},
    {"uni_v2u", GL_UNSIGNED_INT_VEC2, 2, UniformInfo::kUnsignedInt, -1,
     {-1, -1, -1, -1}},
    {"uni_v3u", GL_UNSIGNED_INT_VEC3, 3, UniformInfo::kUnsignedInt, -1,
     {-1, -1, -1, -1}},
    {"uni_v4u", GL_UNSIGNED_INT_VEC4, 4, UniformInfo::kUnsignedInt, -1,
     {-1, -1, -1, -1}},
    {"uni_m2", GL_FLOAT_MAT2, 2, UniformInfo::kMatrix, -1, {-1, -1, -1, -1}},
    {"uni_m3", GL_FLOAT_MAT3, 3, UniformInfo::kMatrix, -1, {-1, -1, -1, -1}},
    {"uni_m4", GL_FLOAT_MAT4, 4, UniformInfo::kMatrix, -1, {-1, -1, -1, -1}},
    {"itex1d", GL_INT_SAMPLER_1D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"itex1da", GL_INT_SAMPLER_1D_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"itex2d", GL_INT_SAMPLER_2D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"itex2da", GL_INT_SAMPLER_2D_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"itex3d", GL_INT_SAMPLER_3D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"icm", GL_INT_SAMPLER_CUBE, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"icma", GL_INT_SAMPLER_CUBE_MAP_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex1d", GL_SAMPLER_1D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"tex1da", GL_SAMPLER_1D_ARRAY, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"tex1das", GL_SAMPLER_1D_ARRAY_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex1ds", GL_SAMPLER_1D_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex2d", GL_SAMPLER_2D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"tex2da", GL_SAMPLER_2D_ARRAY, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"tex2das", GL_SAMPLER_2D_ARRAY_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex2ds", GL_SAMPLER_2D_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex3d", GL_SAMPLER_3D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"cm", GL_SAMPLER_CUBE, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"cma", GL_SAMPLER_CUBE_MAP_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"cmas", GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"cms", GL_SAMPLER_CUBE_SHADOW, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"seo", GL_SAMPLER_EXTERNAL_OES, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"utex1d", GL_UNSIGNED_INT_SAMPLER_1D, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"utex1da", GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, 1, UniformInfo::kInt, -1,
      {-1, -1, -1, -1}},
    {"utex2d", GL_UNSIGNED_INT_SAMPLER_2D, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"utex2da", GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, 1, UniformInfo::kInt, -1,
      {-1, -1, -1, -1}},
    {"utex3d", GL_UNSIGNED_INT_SAMPLER_3D, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"ucm", GL_UNSIGNED_INT_SAMPLER_CUBE, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"ucma", GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY, 1, UniformInfo::kInt, -1,
      {-1, -1, -1, -1}}
  };
  static const int kNumUniforms = static_cast<int>(arraysize(uniforms));

  // Get uniform locations.
  for (int i = 0; i < kNumUniforms; ++i) {
    SCOPED_TRACE(::testing::Message() << i << ": " << uniforms[i].name);
    uniforms[i].loc = gm->GetUniformLocation(pid, uniforms[i].name);
    GM_CHECK_NO_ERROR;
    EXPECT_EQ(i, uniforms[i].loc);
  }

  {
    // GetActiveUniform
    GLsizei length;
    GLint size;
    GLenum type;
    GLchar name[32];
    GM_ERROR_CALL(GetActiveUniform(0, 0, 32, &length, &size, &type, name),
                  GL_INVALID_VALUE);
    GM_ERROR_CALL(
        GetActiveUniform(pid, GetProgramInt(gm, pid, GL_ACTIVE_UNIFORMS),
                         32, &length, &size, &type, name), GL_INVALID_VALUE);
    GM_ERROR_CALL(GetActiveUniform(pid, 0, -1, &length, &size, &type, name),
                  GL_INVALID_VALUE);

    // Successful calls.
    for (int i = 0; i < kNumUniforms; ++i) {
      SCOPED_TRACE(::testing::Message() << i << ": " << uniforms[i].name);
      GM_CALL(GetActiveUniform(pid, uniforms[i].loc, 32, &length, &size,
                               &type, name));
      EXPECT_EQ(0, strcmp(uniforms[i].name, name));
      EXPECT_EQ(static_cast<GLsizei>(strlen(uniforms[i].name)) + 1, length);
      EXPECT_EQ(1, size);
      EXPECT_EQ(uniforms[i].gltype, type);
    }
  }

  // Some dummy values;
  math::Matrix2f mat2 = math::Matrix2f::Identity() * 2.f;
  math::Matrix3f mat3 = math::Matrix3f::Identity() * 3.f;
  math::Matrix4f mat4 = math::Matrix4f::Identity() * 4.f;
  for (int i = 0; i < kNumUniforms; ++i) {
    SCOPED_TRACE(::testing::Message() << i << ": " << uniforms[i].name);
    const UniformInfo& info = uniforms[i];
    // Check Uniform* calls.
    for (GLint length = 1; length <= 4; ++length) {
      SCOPED_TRACE(::testing::Message() << "length: " << length);
      if (length == 1) {
        TestUniform(info, gm, pid, length, 1, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform1f,
                    &GraphicsManager::Uniform1fv);
        TestUniform(info, gm, pid, length, 1, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform1i,
                    &GraphicsManager::Uniform1iv);
        TestUniform(info, gm, pid, length, 1, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform1ui,
                    &GraphicsManager::Uniform1uiv);
      } else if (length == 2) {
        TestUniform(info, gm, pid, length, 1, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform2f,
                    &GraphicsManager::Uniform2fv);
        TestUniform(info, gm, pid, length, 1, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform2i,
                    &GraphicsManager::Uniform2iv);
        TestUniform(info, gm, pid, length, 1, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform2ui,
                    &GraphicsManager::Uniform2uiv);
      } else if (length == 3) {
        TestUniform(info, gm, pid, length, 1, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform3f,
                    &GraphicsManager::Uniform3fv);
        TestUniform(info, gm, pid, length, 1, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform3i,
                    &GraphicsManager::Uniform3iv);
        TestUniform(info, gm, pid, length, 1, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform3ui,
                    &GraphicsManager::Uniform3uiv);
      } else {  // length == 4.
        TestUniform(info, gm, pid, length, 1, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform4f,
                    &GraphicsManager::Uniform4fv);
        TestUniform(info, gm, pid, length, 1, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform4i,
                    &GraphicsManager::Uniform4iv);
        TestUniform(info, gm, pid, length, 1, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform4ui,
                    &GraphicsManager::Uniform4uiv);
      }
    }
    // Manually check UniformMatrix* calls.
    if (info.type == UniformInfo::kMatrix) {
      GM_ERROR_CALL(UniformMatrix2fv(info.loc, -1, GL_FALSE, mat2.Data()),
                    GL_INVALID_VALUE);
      GM_ERROR_CALL(UniformMatrix3fv(info.loc, -1, GL_FALSE, mat3.Data()),
                    GL_INVALID_VALUE);
      GM_ERROR_CALL(UniformMatrix4fv(info.loc, -1, GL_FALSE, mat4.Data()),
                    GL_INVALID_VALUE);
      if (info.length == 2) {
        math::Matrix2f mattest2 = math::Matrix2f::Identity();
        GM_CALL(UniformMatrix2fv(info.loc, 1, GL_FALSE, mat2.Data()));
        GM_CALL(GetUniformfv(pid, info.loc, mattest2.Data()));
        EXPECT_EQ(mat2, mattest2);
      } else {
        GM_ERROR_CALL(UniformMatrix2fv(info.loc, 1, GL_FALSE, mat2.Data()),
                      GL_INVALID_OPERATION);
      }
      if (info.length == 3) {
        math::Matrix3f mattest3 = math::Matrix3f::Identity();
        GM_CALL(UniformMatrix3fv(info.loc, 1, GL_FALSE, mat3.Data()));
        GM_CALL(GetUniformfv(pid, info.loc, mattest3.Data()));
        EXPECT_EQ(mat3, mattest3);
      } else {
        GM_ERROR_CALL(UniformMatrix3fv(info.loc, 1, GL_FALSE, mat3.Data()),
                      GL_INVALID_OPERATION);
      }
      if (info.length == 4) {
        math::Matrix4f mattest4 = math::Matrix4f::Identity();
        GM_CALL(UniformMatrix4fv(info.loc, 1, GL_FALSE, mat4.Data()));
        GM_CALL(GetUniformfv(pid, info.loc, mattest4.Data()));
        EXPECT_EQ(mat4, mattest4);
      } else {
        GM_ERROR_CALL(UniformMatrix4fv(info.loc, 1, GL_FALSE, mat4.Data()),
                      GL_INVALID_OPERATION);
      }
    } else {
      GM_ERROR_CALL(UniformMatrix2fv(info.loc, 1, GL_FALSE, mat2.Data()),
                    GL_INVALID_OPERATION);
      GM_ERROR_CALL(UniformMatrix3fv(info.loc, 1, GL_FALSE, mat3.Data()),
                    GL_INVALID_OPERATION);
      GM_ERROR_CALL(UniformMatrix4fv(info.loc, 1, GL_FALSE, mat4.Data()),
                    GL_INVALID_OPERATION);
    }
  }

  // A negative count should give an invalid value, even for a valid location.
  static const float kF4[4] = {1.1f, 2.2f, 3.3f, 4.4f};
  static const int kI4[4] = {1, 2, 3, 4};
  static const uint32 kU4[4] = {1U, 2U, 3U, 4U};
  GM_ERROR_CALL(Uniform1fv(uniforms[0].loc, -1, kF4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform1iv(uniforms[0].loc, -1, kI4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform1uiv(uniforms[0].loc, -1, kU4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform2fv(uniforms[0].loc, -1, kF4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform2iv(uniforms[0].loc, -1, kI4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform2uiv(uniforms[0].loc, -1, kU4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform3fv(uniforms[0].loc, -1, kF4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform3iv(uniforms[0].loc, -1, kI4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform3uiv(uniforms[0].loc, -1, kU4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform4fv(uniforms[0].loc, -1, kF4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform4iv(uniforms[0].loc, -1, kI4), GL_INVALID_VALUE);
  GM_ERROR_CALL(Uniform4uiv(uniforms[0].loc, -1, kU4), GL_INVALID_VALUE);

  // Detach shaders from program.
  // Invalid value is set if an invalid value is used.
  GM_ERROR_CALL(DetachShader(12U, 5U), GL_INVALID_VALUE);
  GM_ERROR_CALL(DetachShader(pid, 0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(DetachShader(0U, vid), GL_INVALID_VALUE);
  // Invalid operation is a shader is not attached or if program is deleted.
  GM_ERROR_CALL(DetachShader(pid, vid2), GL_INVALID_OPERATION);
  GM_ERROR_CALL(DetachShader(pid, fid2), GL_INVALID_OPERATION);
  GM_ERROR_CALL(DetachShader(pid2, vid2), GL_INVALID_OPERATION);

  // Detach valid shaders.
  EXPECT_EQ(2, GetProgramInt(gm, pid, GL_ATTACHED_SHADERS));
  GM_CALL(DetachShader(pid, vid));
  EXPECT_EQ(1, GetProgramInt(gm, pid, GL_ATTACHED_SHADERS));
  GM_CALL(DetachShader(pid, fid));
  EXPECT_EQ(0, GetProgramInt(gm, pid, GL_ATTACHED_SHADERS));

  // The default program should get reset to none.
  GM_CALL(DeleteProgram(pid));
  EXPECT_EQ(0, GetInt(gm, GL_CURRENT_PROGRAM));

  // For coverage.
  GM_ERROR_CALL(ReleaseShaderCompiler(), GL_INVALID_OPERATION);
  GM_ERROR_CALL(ShaderBinary(0, NULL, 0U, NULL, 0), GL_INVALID_OPERATION);
}

TEST(MockGraphicsManagerTest, UniformArrays) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  const std::string vertex_source(kVertexSource);
  const std::string fragment_source(kFragmentSource);
  GLuint vid = gm->CreateShader(GL_VERTEX_SHADER);
  GLuint fid = gm->CreateShader(GL_FRAGMENT_SHADER);
  GM_CHECK_NO_ERROR;
  {
    GLint length = static_cast<GLuint>(vertex_source.length());
    const char* ptr = vertex_source.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
  }
  {
    GLint length = static_cast<GLuint>(fragment_source.length());
    const char* ptr = fragment_source.c_str();
    GM_CALL(ShaderSource(fid, 1, &ptr, &length));
  }
  GLuint pid = gm->CreateProgram();
  GM_CALL(CompileShader(vid));
  GM_CALL(CompileShader(fid));
  GM_CALL(AttachShader(pid, vid));
  GM_CALL(AttachShader(pid, fid));
  GM_CALL(LinkProgram(pid));
  GM_CALL(UseProgram(pid));

  UniformInfo uniforms[] = {
    {"uni_v2f_array", GL_FLOAT_VEC2, 2, UniformInfo::kFloat, -1,
     {-1, -1, -1, -1}},
    {"uni_v3f_array", GL_FLOAT_VEC3, 3, UniformInfo::kFloat, -1,
     {-1, -1, -1, -1}},
    {"uni_v4f_array", GL_FLOAT_VEC4, 4, UniformInfo::kFloat, -1,
     {-1, -1, -1, -1}},
    {"uni_i_array", GL_INT, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"uni_v2i_array", GL_INT_VEC2, 2, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"uni_v3i_array", GL_INT_VEC3, 3, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"uni_v4i_array", GL_INT_VEC4, 4, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"uni_u_array", GL_UNSIGNED_INT, 1, UniformInfo::kUnsignedInt, -1,
     {-1, -1, -1, -1}},
    {"uni_v2u_array", GL_UNSIGNED_INT_VEC2, 2, UniformInfo::kUnsignedInt, -1,
     {-1, -1, -1, -1}},
    {"uni_v3u_array", GL_UNSIGNED_INT_VEC3, 3, UniformInfo::kUnsignedInt, -1,
     {-1, -1, -1, -1}},
    {"uni_v4u_array", GL_UNSIGNED_INT_VEC4, 4, UniformInfo::kUnsignedInt, -1,
     {-1, -1, -1, -1}},
    {"uni_m2_array", GL_FLOAT_MAT2, 2, UniformInfo::kMatrix, -1,
     {-1, -1, -1, -1}},
    {"uni_m3_array", GL_FLOAT_MAT3, 3, UniformInfo::kMatrix, -1,
     {-1, -1, -1, -1}},
    {"uni_m4_array", GL_FLOAT_MAT4, 4, UniformInfo::kMatrix, -1,
     {-1, -1, -1, -1}},
    {"itex1d_array", GL_INT_SAMPLER_1D, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"itex1da_array", GL_INT_SAMPLER_1D_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"itex2d_array", GL_INT_SAMPLER_2D, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"itex2da_array", GL_INT_SAMPLER_2D_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"itex3d_array", GL_INT_SAMPLER_3D, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"icm_array", GL_INT_SAMPLER_CUBE, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"icma_array", GL_INT_SAMPLER_CUBE_MAP_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex1d_array", GL_SAMPLER_1D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"tex1da_array", GL_SAMPLER_1D_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex1das_array", GL_SAMPLER_1D_ARRAY_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex1ds_array", GL_SAMPLER_1D_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex2d_array", GL_SAMPLER_2D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"tex2da_array", GL_SAMPLER_2D_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex2das_array", GL_SAMPLER_2D_ARRAY_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex2ds_array", GL_SAMPLER_2D_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"tex3d_array", GL_SAMPLER_3D, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"cm_array", GL_SAMPLER_CUBE, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"cma_array", GL_SAMPLER_CUBE_MAP_ARRAY, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"cmas_array", GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"cms_array", GL_SAMPLER_CUBE_SHADOW, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"seo_array", GL_SAMPLER_EXTERNAL_OES, 1, UniformInfo::kInt, -1,
     {-1, -1, -1, -1}},
    {"utex1d_array", GL_UNSIGNED_INT_SAMPLER_1D, 1, UniformInfo::kInt, -1,
      {-1, -1, -1, -1}},
    {"utex1da_array", GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, 1, UniformInfo::kInt,
      -1, {-1, -1, -1, -1}},
    {"utex2d_array", GL_UNSIGNED_INT_SAMPLER_2D, 1, UniformInfo::kInt, -1,
      {-1, -1, -1, -1}},
    {"utex2da_array", GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, 1, UniformInfo::kInt,
      -1, {-1, -1, -1, -1}},
    {"utex3d_array", GL_UNSIGNED_INT_SAMPLER_3D, 1, UniformInfo::kInt, -1,
      {-1, -1, -1, -1}},
    {"ucm_array", GL_UNSIGNED_INT_SAMPLER_CUBE, 1, UniformInfo::kInt, -1,
      {-1, -1, -1, -1}},
    {"ucma_array", GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY, 1, UniformInfo::kInt,
      -1, {-1, -1, -1, -1}}
  };
  static const int kNumUniforms = static_cast<size_t>(arraysize(uniforms));

  // Get uniform array locations.
  for (int i = 0; i < kNumUniforms; ++i) {
    uniforms[i].loc = gm->GetUniformLocation(pid, uniforms[i].name);
    GM_CHECK_NO_ERROR;
    for (int j = 0; j < 4; ++j) {
      SCOPED_TRACE(::testing::Message() << i << ": " << uniforms[i].name << "["
                                        << j << "]");
      std::ostringstream str;
      str << uniforms[i].name << "[" << j << "]";
      uniforms[i].alocs[j] = gm->GetUniformLocation(pid, str.str().c_str());
      GM_CHECK_NO_ERROR;
      EXPECT_EQ(47 + i * 4 + j, uniforms[i].alocs[j]);
    }
    EXPECT_EQ(uniforms[i].loc, uniforms[i].alocs[0]);
  }

  math::Matrix2f mat2[4] = {
      math::Matrix2f::Identity(), math::Matrix2f::Identity() * 2.f,
      math::Matrix2f::Identity() * 3.f, math::Matrix2f::Identity() * 4.f};
  math::Matrix3f mat3[4] = {
      math::Matrix3f::Identity(), math::Matrix3f::Identity() * 2.f,
      math::Matrix3f::Identity() * 3.f, math::Matrix3f::Identity() * 4.f};
  math::Matrix4f mat4[4] = {
      math::Matrix4f::Identity(), math::Matrix4f::Identity() * 2.f,
      math::Matrix4f::Identity() * 3.f, math::Matrix4f::Identity() * 4.f};

  for (int i = 0; i < kNumUniforms; ++i) {
    SCOPED_TRACE(::testing::Message() << i << ": " << uniforms[i].name);
    const UniformInfo& info = uniforms[i];
    // Check Uniform* calls.
    for (GLint length = 1; length <= 4; ++length) {
      SCOPED_TRACE(::testing::Message() << "length: " << length);
      if (length == 1) {
        TestUniform(info, gm, pid, length, 4, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform1f,
                    &GraphicsManager::Uniform1fv);
        TestUniform(info, gm, pid, length, 4, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform1i,
                    &GraphicsManager::Uniform1iv);
        TestUniform(info, gm, pid, length, 4, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform1ui,
                    &GraphicsManager::Uniform1uiv);
      } else if (length == 2) {
        TestUniform(info, gm, pid, length, 4, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform2f,
                    &GraphicsManager::Uniform2fv);
        TestUniform(info, gm, pid, length, 4, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform2i,
                    &GraphicsManager::Uniform2iv);
        TestUniform(info, gm, pid, length, 4, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform2ui,
                    &GraphicsManager::Uniform2uiv);
      } else if (length == 3) {
        TestUniform(info, gm, pid, length, 4, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform3f,
                    &GraphicsManager::Uniform3fv);
        TestUniform(info, gm, pid, length, 4, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform3i,
                    &GraphicsManager::Uniform3iv);
        TestUniform(info, gm, pid, length, 4, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform3ui,
                    &GraphicsManager::Uniform3uiv);
      } else {  // length == 4.
        TestUniform(info, gm, pid, length, 4, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform4f,
                    &GraphicsManager::Uniform4fv);
        TestUniform(info, gm, pid, length, 4, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform4i,
                    &GraphicsManager::Uniform4iv);
        TestUniform(info, gm, pid, length, 4, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform4ui,
                    &GraphicsManager::Uniform4uiv);
      }
    }
    // Manually check UniformMatrix* calls.
    if (info.type == UniformInfo::kMatrix) {
      GM_ERROR_CALL(UniformMatrix2fv(info.loc, -1, GL_FALSE, mat2[0].Data()),
                    GL_INVALID_VALUE);
      GM_ERROR_CALL(UniformMatrix3fv(info.loc, -1, GL_FALSE, mat3[0].Data()),
                    GL_INVALID_VALUE);
      GM_ERROR_CALL(UniformMatrix4fv(info.loc, -1, GL_FALSE, mat4[0].Data()),
                    GL_INVALID_VALUE);
      GM_ERROR_CALL(UniformMatrix2fv(info.loc, 1, GL_TRUE, mat2[0].Data()),
                    GL_INVALID_VALUE);
      GM_ERROR_CALL(UniformMatrix3fv(info.loc, 1, GL_TRUE, mat3[0].Data()),
                    GL_INVALID_VALUE);
      GM_ERROR_CALL(UniformMatrix4fv(info.loc, 1, GL_TRUE, mat4[0].Data()),
                    GL_INVALID_VALUE);
      if (info.length == 2) {
        math::Matrix2f mattest2 = math::Matrix2f::Identity();
        for (int len = 1; len <= 4; ++len) {
          GM_CALL(UniformMatrix2fv(info.loc, len, GL_FALSE, mat2[0].Data()));
          for (int i = 0; i < len; ++i) {
            GM_CALL(GetUniformfv(pid, info.alocs[i], mattest2.Data()));
            EXPECT_EQ(mat2[i], mattest2);
          }
        }
      } else {
        GM_ERROR_CALL(UniformMatrix2fv(info.loc, 1, GL_FALSE, mat2[0].Data()),
                      GL_INVALID_OPERATION);
      }
      if (info.length == 3) {
        math::Matrix3f mattest3 = math::Matrix3f::Identity();
        for (int len = 1; len <= 4; ++len) {
          GM_CALL(UniformMatrix3fv(info.loc, len, GL_FALSE, mat3[0].Data()));
          for (int i = 0; i < len; ++i) {
            GM_CALL(GetUniformfv(pid, info.alocs[i], mattest3.Data()));
            EXPECT_EQ(mat3[i], mattest3);
          }
        }
      } else {
        GM_ERROR_CALL(UniformMatrix3fv(info.loc, 1, GL_FALSE, mat3[0].Data()),
                      GL_INVALID_OPERATION);
      }
      if (info.length == 4) {
        math::Matrix4f mattest4 = math::Matrix4f::Identity();
        for (int len = 1; len <= 4; ++len) {
          GM_CALL(UniformMatrix4fv(info.loc, len, GL_FALSE, mat4[0].Data()));
          for (int i = 0; i < len; ++i) {
            GM_CALL(GetUniformfv(pid, info.alocs[i], mattest4.Data()));
            EXPECT_EQ(mat4[i], mattest4);
          }
        }
      } else {
        GM_ERROR_CALL(UniformMatrix4fv(info.loc, 1, GL_FALSE, mat4[0].Data()),
                      GL_INVALID_OPERATION);
      }
    } else {
      GM_ERROR_CALL(UniformMatrix2fv(info.loc, 1, GL_FALSE, mat2[0].Data()),
                    GL_INVALID_OPERATION);
      GM_ERROR_CALL(UniformMatrix3fv(info.loc, 1, GL_FALSE, mat3[0].Data()),
                    GL_INVALID_OPERATION);
      GM_ERROR_CALL(UniformMatrix4fv(info.loc, 1, GL_FALSE, mat4[0].Data()),
                    GL_INVALID_OPERATION);
    }
  }  // Matrix calls.

  GM_CALL(DeleteProgram(pid));
}

TEST(MockGraphicsManagerTest, ImageExternal) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  // Just call the function.
  gm->EGLImageTargetTexture2DOES(0, NULL);
}

TEST(MockGraphicsManagerTest, ShaderPreprocessor) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  const std::string vertex_source(
      "#define FOO1\n"
      "uniform float uAvailableV1;\n"
      "\n"
      "#ifdef FOO1\n"
      "uniform float uAvailableV2;\n"
      "#else\n"
      "uniform float uNotAvailableV1;\n"
      "#endif\n"
      "\n"
      "#ifndef FOO2\n"
      "uniform float uAvailableV3;\n"
      "#ifdef FOO3\n"
      "uniform float uNotAvailableV2;\n"
      "#else\n"
      "uniform float uAvailableV4;\n"
      "#else\n"
      "uniform float uNotAvailableV3;\n"
      "#endif\n");
  const std::string fragment_source(
      "#ifndef BAR1\n"
      "#define BAR1\n"
      "#endif BAR1\n"
      "\n"
      "#ifdef BAR1\n"
      "#define BAR2\n"
      "uniform float uAvailableF1;\n"
      "#ifdef BAR2\n"
      "#define BAR2\n"
      "#ifdef BAR2\n"
      "uniform float uAvailableF2;\n"
      "#endif\n"
      "#else\n"
      "uniform float uNotAvailableF1;\n"
      "#endif\n"
      "uniform float uAvailableF2;\n"
      "#else\n"
      "uniform float uNotAvailableF2;\n"
      "#endif\n"
      "uniform float uAvailableF3;\n"
      "#ifdef BAR2\n"
      "uniform float uAvailableF4;\n"
      "#endif\n"
      "#ifdef BAR1\n"
      "#ifdef BAR2\n"
      "#define BAR3\n"
      "#endif\n"
      "#endif\n"
      "#ifdef BAR3\n"
      "uniform float uAvailableF5;\n"
      "#endif\n"
      "#ifdef BAR4\n"
      "uniform float uNotAvailableF3;\n"
      "#endif\n");

  GLuint vid = gm->CreateShader(GL_VERTEX_SHADER);
  GLuint fid = gm->CreateShader(GL_FRAGMENT_SHADER);
  {
    GLint length = static_cast<GLuint>(vertex_source.length());
    const char* ptr = vertex_source.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
  }
  {
    GLint length = static_cast<GLuint>(fragment_source.length());
    const char* ptr = fragment_source.c_str();
    GM_CALL(ShaderSource(fid, 1, &ptr, &length));
  }
  GLuint pid = gm->CreateProgram();
  GM_CALL(CompileShader(vid));
  GM_CALL(CompileShader(fid));
  GM_CALL(AttachShader(pid, vid));
  GM_CALL(AttachShader(pid, fid));
  GM_CALL(LinkProgram(pid));
  GM_CALL(UseProgram(pid));
  GM_CHECK_NO_ERROR;

  EXPECT_EQ(-1, gm->GetUniformLocation(pid, "uNotAvailableV1"));
  EXPECT_EQ(-1, gm->GetUniformLocation(pid, "uNotAvailableV2"));
  EXPECT_EQ(-1, gm->GetUniformLocation(pid, "uNotAvailableV3"));
  EXPECT_EQ(-1, gm->GetUniformLocation(pid, "uNotAvailableF1"));
  EXPECT_EQ(-1, gm->GetUniformLocation(pid, "uNotAvailableF2"));
  EXPECT_EQ(-1, gm->GetUniformLocation(pid, "uNotAvailableF3"));

  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableV1"));
  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableV2"));
  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableV3"));
  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableV4"));
  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableF1"));
  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableF2"));
  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableF3"));
  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableF4"));
  EXPECT_NE(-1, gm->GetUniformLocation(pid, "uAvailableF5"));
  GM_CHECK_NO_ERROR;
}

TEST(MockGraphicsManagerTest, ShaderPreprocessorUnsupportedFeatures) {
  // The shader preprocessor does not support all features. Upon reading an
  // unsupported clause, we should print a warning and not crash.
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // Boilerplate fragment shader. We just test using the vertex shader.
  const std::string fragment_source("\n");
  GLuint fid = gm->CreateShader(GL_FRAGMENT_SHADER);
  {
    GLint length = static_cast<GLuint>(fragment_source.length());
    const char* ptr = fragment_source.c_str();
    GM_CALL(ShaderSource(fid, 1, &ptr, &length));
  }
  GM_CALL(CompileShader(fid));

  // Make sure we print a warning and don't crash if we run into #if.
  {
    base::LogChecker log_checker;
    GLuint pid = gm->CreateProgram();
    GLuint vid = gm->CreateShader(GL_VERTEX_SHADER);
    const std::string vertex_source1(
        "#if defined (FOO1)\n"
        "#endif\n");
    GLint length = static_cast<GLuint>(vertex_source1.length());
    const char* ptr = vertex_source1.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
    GM_CALL(CompileShader(vid));
    GM_CALL(AttachShader(pid, vid));
    GM_CALL(AttachShader(pid, fid));
    GM_CALL(LinkProgram(pid));
    GM_CALL(UseProgram(pid));
    GM_CHECK_NO_ERROR;
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "does not support #if"));
  }

  // Make sure we print a warning and don't crash if we run into #elif.
  {
    base::LogChecker log_checker;
    GLuint pid = gm->CreateProgram();
    GLuint vid = gm->CreateShader(GL_VERTEX_SHADER);
    const std::string vertex_source1(
        "#ifdef FOO1\n"
        "#elif defined (FOO2)\n"
        "#endif\n");
    GLint length = static_cast<GLuint>(vertex_source1.length());
    const char* ptr = vertex_source1.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
    GM_CALL(CompileShader(vid));
    GM_CALL(AttachShader(pid, vid));
    GM_CALL(AttachShader(pid, fid));
    GM_CALL(LinkProgram(pid));
    GM_CALL(UseProgram(pid));
    GM_CHECK_NO_ERROR;
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "does not support #elif"));
  }

  // Make sure we print a warning and don't crash if we run into #undef.
  {
    base::LogChecker log_checker;
    GLuint pid = gm->CreateProgram();
    GLuint vid = gm->CreateShader(GL_VERTEX_SHADER);
    const std::string vertex_source1(
        "#ifdef FOO1\n"
        "#undef FOO1\n"
        "#endif\n");
    GLint length = static_cast<GLuint>(vertex_source1.length());
    const char* ptr = vertex_source1.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
    GM_CALL(CompileShader(vid));
    GM_CALL(AttachShader(pid, vid));
    GM_CALL(AttachShader(pid, fid));
    GM_CALL(LinkProgram(pid));
    GM_CALL(UseProgram(pid));
    GM_CHECK_NO_ERROR;
    EXPECT_TRUE(log_checker.HasMessage("WARNING", "does not support #undef"));
  }
}

TEST(MockGraphicsManagerTest, PlatformCapabilities) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  GLfloat f4[4];
  GLint i2[2];

  // Defaults.
  EXPECT_EQ(1.f, gm->GetMinAliasedLineWidth());
  EXPECT_EQ(256.f, gm->GetMaxAliasedLineWidth());
  EXPECT_EQ(1.f, gm->GetMinAliasedPointSize());
  EXPECT_EQ(8192.f, gm->GetMaxAliasedPointSize());
  EXPECT_EQ(static_cast<GLenum>(GL_UNSIGNED_BYTE),
            gm->GetImplementationColorReadFormat());
  EXPECT_EQ(static_cast<GLenum>(GL_RGB), gm->GetImplementationColorReadType());
  EXPECT_EQ(4096, gm->GetMax3dTextureSize());
  EXPECT_EQ(4096, gm->GetMaxArrayTextureLayers());
  EXPECT_EQ(32U, gm->GetMaxCombinedTextureImageUnits());
  EXPECT_EQ(8192, gm->GetMaxCubeMapTextureSize());
  EXPECT_EQ(256U, gm->GetMaxFragmentUniformComponents());
  EXPECT_EQ(512U, gm->GetMaxFragmentUniformVectors());
  EXPECT_EQ(4096, gm->GetMaxRenderbufferSize());
  EXPECT_EQ(16, gm->GetMaxSamples());
  EXPECT_EQ(32U, gm->GetMaxTextureImageUnits());
  EXPECT_EQ(8192, gm->GetMaxTextureSize());
  EXPECT_EQ(15U, gm->GetMaxVaryingVectors());
  EXPECT_EQ(32U, gm->GetMaxVertexAttribs());
  EXPECT_EQ(32U, gm->GetMaxVertexTextureImageUnits());
  EXPECT_EQ(512U, gm->GetMaxVertexUniformComponents());
  EXPECT_EQ(1024U, gm->GetMaxVertexUniformVectors());
  EXPECT_EQ(8192U, gm->GetMaxViewportDims());

  // Set values and check that GL returns them.
  gm->SetMinAliasedLineWidth(0.5f);
  EXPECT_EQ(0.5f, gm->GetMinAliasedLineWidth());
  GM_CALL(GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, f4));
  EXPECT_EQ(0.5f, f4[0]);
  gm->SetMaxAliasedLineWidth(12.f);
  GM_CALL(GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, f4));
  EXPECT_EQ(12.f, f4[1]);

  gm->SetMinAliasedPointSize(0.25f);
  EXPECT_EQ(0.25f, gm->GetMinAliasedPointSize());
  GM_CALL(GetFloatv(GL_ALIASED_POINT_SIZE_RANGE, f4));
  EXPECT_EQ(0.25f, f4[0]);
  gm->SetMaxAliasedPointSize(31.f);
  GM_CALL(GetFloatv(GL_ALIASED_POINT_SIZE_RANGE, f4));
  EXPECT_EQ(31.f, f4[1]);

  gm->SetImplementationColorReadFormat(GL_FLOAT);
  EXPECT_EQ(static_cast<GLenum>(GL_FLOAT),
            gm->GetImplementationColorReadFormat());
  EXPECT_EQ(GL_FLOAT, GetInt(gm, GL_IMPLEMENTATION_COLOR_READ_FORMAT));
  gm->SetImplementationColorReadType(GL_RGBA4);
  EXPECT_EQ(static_cast<GLenum>(GL_RGBA4),
            gm->GetImplementationColorReadType());
  EXPECT_EQ(GL_RGBA4, GetInt(gm, GL_IMPLEMENTATION_COLOR_READ_TYPE));

  gm->SetMax3dTextureSize(256);
  EXPECT_EQ(256, gm->GetMax3dTextureSize());
  EXPECT_EQ(256, GetInt(gm, GL_MAX_3D_TEXTURE_SIZE));

  gm->SetMaxArrayTextureLayers(320);
  EXPECT_EQ(320, gm->GetMaxArrayTextureLayers());
  EXPECT_EQ(320, GetInt(gm, GL_MAX_ARRAY_TEXTURE_LAYERS));

  gm->SetMaxCombinedTextureImageUnits(11U);
  EXPECT_EQ(11U, gm->GetMaxCombinedTextureImageUnits());
  EXPECT_EQ(11, GetInt(gm, GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));

  gm->SetMaxCubeMapTextureSize(2048);
  EXPECT_EQ(2048, gm->GetMaxCubeMapTextureSize());
  EXPECT_EQ(2048, GetInt(gm, GL_MAX_CUBE_MAP_TEXTURE_SIZE));

  gm->SetMaxFragmentUniformComponents(5896U);
  EXPECT_EQ(5896U, gm->GetMaxFragmentUniformComponents());
  EXPECT_EQ(5896, GetInt(gm, GL_MAX_FRAGMENT_UNIFORM_COMPONENTS));

  gm->SetMaxFragmentUniformVectors(8000U);
  EXPECT_EQ(8000U, gm->GetMaxFragmentUniformVectors());
  EXPECT_EQ(8000, GetInt(gm, GL_MAX_FRAGMENT_UNIFORM_VECTORS));

  gm->SetMaxSamples(534);
  EXPECT_EQ(534, gm->GetMaxSamples());
  EXPECT_EQ(534, GetInt(gm, GL_MAX_SAMPLES));

  gm->SetMaxRenderbufferSize(768);
  EXPECT_EQ(768, gm->GetMaxRenderbufferSize());
  EXPECT_EQ(768, GetInt(gm, GL_MAX_RENDERBUFFER_SIZE));

  gm->SetMaxTextureImageUnits(8U);
  EXPECT_EQ(8U, gm->GetMaxTextureImageUnits());
  EXPECT_EQ(8, GetInt(gm, GL_MAX_TEXTURE_IMAGE_UNITS));

  gm->SetMaxTextureMaxAnisotropy(4.f);
  EXPECT_EQ(4.f, gm->GetMaxTextureMaxAnisotropy());
  EXPECT_EQ(4, GetInt(gm, GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));

  gm->SetMaxTextureSize(64);
  EXPECT_EQ(64, gm->GetMaxTextureSize());
  EXPECT_EQ(64, GetInt(gm, GL_MAX_TEXTURE_SIZE));

  gm->SetMaxVaryingVectors(3U);
  EXPECT_EQ(3U, gm->GetMaxVaryingVectors());
  EXPECT_EQ(3, GetInt(gm, GL_MAX_VARYING_VECTORS));

  gm->SetMaxVertexAttribs(16U);
  EXPECT_EQ(16U, gm->GetMaxVertexAttribs());
  EXPECT_EQ(16, GetInt(gm, GL_MAX_VERTEX_ATTRIBS));

  gm->SetMaxVertexTextureImageUnits(50U);
  EXPECT_EQ(50U, gm->GetMaxVertexTextureImageUnits());
  EXPECT_EQ(50, GetInt(gm, GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS));

  gm->SetMaxVertexUniformVectors(356U);
  EXPECT_EQ(356U, gm->GetMaxVertexUniformVectors());
  EXPECT_EQ(356, GetInt(gm, GL_MAX_VERTEX_UNIFORM_VECTORS));

  gm->SetMaxVertexUniformComponents(73U);
  EXPECT_EQ(73U, gm->GetMaxVertexUniformComponents());
  EXPECT_EQ(73, GetInt(gm, GL_MAX_VERTEX_UNIFORM_COMPONENTS));

  gm->SetMaxViewportDims(2048U);
  EXPECT_EQ(2048U, gm->GetMaxViewportDims());
  GM_CALL(GetIntegerv(GL_MAX_VIEWPORT_DIMS, i2));
  EXPECT_EQ(2048, i2[0]);
  EXPECT_EQ(2048, i2[1]);
}

TEST(MockGraphicsManagerTest, ErrorChecking) {
  base::LogChecker log_checker;

  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  gm->EnableErrorChecking(true);

  // Should be ok.
  gm->CullFace(GL_BACK);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  //
  // Each of these should produce a single error of a different type.
  //

  gm->CullFace(GL_TRIANGLES);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid enumerant"));

  gm->Clear(static_cast<GLbitfield>(12345));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  gm->Uniform1f(300, 10.0f);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid operation"));

  {
    gm->SetMaxBufferSize(1024);
    EXPECT_EQ(1024, gm->GetMaxBufferSize());
    GLuint bo;
    gm->GenBuffers(1, &bo);
    gm->BindBuffer(GL_ARRAY_BUFFER, 1U);
    gm->BufferData(GL_ARRAY_BUFFER, 1026, NULL, GL_STATIC_DRAW);
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "out of memory"));
    gm->DeleteBuffers(1, &bo);
  }

  {
    GLuint fbo;
    gm->GenFramebuffers(1, &fbo);
    gm->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    uint8 data[10 * 10 * 4];
    gm->ReadPixels(0, 0, 10, 10, GL_RGBA, GL_UNSIGNED_BYTE, data);
    EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                       "invalid framebuffer operation"));
    gm->DeleteFramebuffers(1, &fbo);
  }

  gm->SetErrorCode(GL_TRIANGLES);
  gm->Clear(0);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "unknown error"));
}

TEST(MockGraphicsManagerTest, Tracing) {
  std::unique_ptr<MockVisual> visual(new MockVisual(600, 500));
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  base::LogChecker log_checker;

  {
    // The TraceVerifier has to have a shorter scope than the graphics manager.
    TraceVerifier trace_verifier(gm.Get());
    // Make function calls with different numbers and types of arguments.
    gm->Flush();
    gm->ClearDepthf(0.5f);
    gm->DepthMask(GL_TRUE);
    gm->CullFace(GL_FRONT);
    gm->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Make sure strings are quoted and NULL pointers are handled.
    GLchar source_string[] = "Source string";
    gm->GetShaderSource(1U, 128, NULL, source_string);
    gm->GetUniformLocation(2U, "SomeName");

    // Make sure bizarre values are handled reasonably.
    gm->DepthMask(13);
    gm->Clear(GL_DEPTH_BUFFER_BIT | 0x001);
    gm->MapBufferRange(GL_ARRAY_BUFFER, 2, 4, GL_MAP_READ_BIT | 0x100);
    math::Matrix3f mat(6.2f, 1.8f, 2.6f,
                       -7.4f, -9.2f, 1.3f,
                       -4.1f, 5.3f, -1.9f);
    gm->UniformMatrix3fv(1, 1, GL_FALSE, mat.Data());

    // Verify that each function is traced properly, in order.
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(0U, "Flush()"));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(1U, "ClearDepthf(0.5)"));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(2U, "DepthMask(GL_TRUE)"));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(3U, "CullFace(GL_FRONT)"));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(
        4U, "Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)"));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(
        5U, "GetShaderSource(0x1, 128, NULL, \"Source string\""));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(
        6U, "GetUniformLocation(0x2, \"SomeName\""));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(7U, "DepthMask(13)"));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(8U, "Clear(0x101)"));
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(
        9U, "MapBufferRange(GL_ARRAY_BUFFER, 2, 4, 0x101)"));
    std::ostringstream matrix_string;
    const float* data = mat.Data();
    matrix_string
        << "UniformMatrix3fv(1, 1, GL_FALSE, "
        << "0x" << std::hex << *reinterpret_cast<size_t*>(&data)
        << " -> [6.2; 1.8; 2.6 | -7.4; -9.2; 1.3 | -4.1; 5.3; -1.9])";
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(10U, matrix_string.str()));
  }
  // The UniformMatrix3fv is technically an error since there is no program
  // bound.
  gm.Reset(NULL);
  visual.reset();
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "destroyed with uncaught"));
}

TEST(MockGraphicsManagerTest, EnableAndDisableFunctionGroups) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kCore));
  EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
  gm->EnableFunctionGroup(GraphicsManager::kVertexArrays, false);
  EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kCore));
  EXPECT_FALSE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
  gm->EnableFunctionGroup(GraphicsManager::kCore, false);
  EXPECT_FALSE(gm->IsFunctionGroupAvailable(GraphicsManager::kCore));
  EXPECT_FALSE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
  gm->EnableFunctionGroup(GraphicsManager::kVertexArrays, true);
  EXPECT_FALSE(gm->IsFunctionGroupAvailable(GraphicsManager::kCore));
  EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
  gm->EnableFunctionGroup(GraphicsManager::kCore, true);
  EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kCore));
  EXPECT_TRUE(gm->IsFunctionGroupAvailable(GraphicsManager::kVertexArrays));
}

TEST(MockGraphicsManagerTest, ForceFailures) {
  // Test Gen* failure cases.
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  GLuint id = 0U;

  GM_CALL(GenBuffers(1, &id));
  EXPECT_GT(id, 0U);
  gm->SetForceFunctionFailure("GenBuffers", true);
  id = 0U;
  GM_ERROR_CALL(GenBuffers(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm->SetForceFunctionFailure("GenBuffers", false);
  GM_CALL(GenBuffers(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenFramebuffers(1, &id));
  EXPECT_GT(id, 0U);
  gm->SetForceFunctionFailure("GenFramebuffers", true);
  id = 0U;
  GM_ERROR_CALL(GenFramebuffers(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm->SetForceFunctionFailure("GenFramebuffers", false);
  GM_CALL(GenFramebuffers(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenRenderbuffers(1, &id));
  EXPECT_GT(id, 0U);
  gm->SetForceFunctionFailure("GenRenderbuffers", true);
  id = 0U;
  GM_ERROR_CALL(GenRenderbuffers(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm->SetForceFunctionFailure("GenRenderbuffers", false);
  GM_CALL(GenRenderbuffers(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenSamplers(1, &id));
  EXPECT_GT(id, 0U);
  gm->SetForceFunctionFailure("GenSamplers", true);
  id = 0U;
  GM_ERROR_CALL(GenSamplers(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm->SetForceFunctionFailure("GenSamplers", false);
  GM_CALL(GenSamplers(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenTextures(1, &id));
  EXPECT_GT(id, 0U);
  gm->SetForceFunctionFailure("GenTextures", true);
  id = 0U;
  GM_ERROR_CALL(GenTextures(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm->SetForceFunctionFailure("GenTextures", false);
  GM_CALL(GenTextures(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenVertexArrays(1, &id));
  EXPECT_GT(id, 0U);
  gm->SetForceFunctionFailure("GenVertexArrays", true);
  id = 0U;
  GM_ERROR_CALL(GenVertexArrays(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm->SetForceFunctionFailure("GenVertexArrays", false);
  GM_CALL(GenVertexArrays(1, &id));
  EXPECT_GT(id, 0U);
}

TEST(MockGraphicsManagerTest, DebugLabels) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  char label[64];
  GLint length = 0;
  // Try some invalid enums.
  GM_ERROR_CALL(LabelObject(GL_VERTEX_SHADER, 0U, 0, label), GL_INVALID_ENUM);
  GM_ERROR_CALL(LabelObject(GL_POINTS, 0U, 0, label), GL_INVALID_ENUM);
  GM_ERROR_CALL(LabelObject(GL_INVALID_ENUM, 0U, 0, label), GL_INVALID_ENUM);
  GM_ERROR_CALL(GetObjectLabel(GL_VERTEX_SHADER, 0U, 0, &length, label),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetObjectLabel(GL_POINTS, 0U, 0, &length, label),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetObjectLabel(GL_INVALID_ENUM, 0U, 0, &length, label),
                GL_INVALID_ENUM);

  // Create some objects, set and then get their labels.
  GLuint id;
  GM_CALL(GenTextures(1, &id));
  VerifySetAndGetLabel(gm, GL_TEXTURE, id);

  GM_CALL(GenFramebuffers(1, &id));
  VerifySetAndGetLabel(gm, GL_FRAMEBUFFER, id);

  GM_CALL(GenRenderbuffers(1, &id));
  VerifySetAndGetLabel(gm, GL_RENDERBUFFER, id);

  GM_CALL(GenBuffers(1, &id));
  VerifySetAndGetLabel(gm, GL_BUFFER_OBJECT, id);

  GM_CALL(GenSamplers(1, &id));
  VerifySetAndGetLabel(gm, GL_SAMPLER, id);

  GM_CALL(GenVertexArrays(1, &id));
  VerifySetAndGetLabel(gm, GL_VERTEX_ARRAY_OBJECT, id);

  id = gm->CreateProgram();
  VerifySetAndGetLabel(gm, GL_PROGRAM_OBJECT, id);

  id = gm->CreateShader(GL_VERTEX_SHADER);
  VerifySetAndGetLabel(gm, GL_SHADER_OBJECT, id);

  id = gm->CreateShader(GL_FRAGMENT_SHADER);
  VerifySetAndGetLabel(gm, GL_SHADER_OBJECT, id);
}

TEST(MockGraphicsManagerTest, DebugMarkers) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());
  base::LogChecker log_checker;
  std::string marker("marker");
  // These functions on their own do nothing visible.
  gm->InsertEventMarker(static_cast<GLsizei>(marker.length()), marker.c_str());
  gm->PushGroupMarker(static_cast<GLsizei>(marker.length()), marker.c_str());
  gm->PopGroupMarker();
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(MockGraphicsManagerTest, DebugOutput) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLenum callback_source = 0;
  GLenum callback_type = 0;
  GLuint callback_id = ~0;
  GLenum callback_severity = 0;
  std::string callback_message;

  // Create the debug callback lambda.
  std::function<void(GLenum, GLenum, GLuint, GLenum, const GLchar*)>
  debug_callback([&callback_source, &callback_type, &callback_id,
                  &callback_severity,
                  &callback_message](GLenum source, GLenum type, GLuint id,
                                     GLenum severity, const GLchar* message) {
    callback_source = source;
    callback_type = type;
    callback_id = id;
    callback_severity = severity;
    callback_message = std::string(message);
  });
  typedef decltype(debug_callback) DebugCallbackType;
  // Lambdas which perform capture like |debug_callback| cannot be cast directly
  // into a function pointer.  Instead, use the non-capturing lambda
  // |callback_lambda| below to forward function call arguments to the capturing
  // lambda.
  auto callback_lambda = [](GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar* message, const void* param) {
    const auto* cb = reinterpret_cast<const DebugCallbackType*>(param);
    (*cb)(source, type, id, severity, message);
  };

  // Verify that the debug callback is set correctly.
  void* ptr = nullptr;
  GM_CALL(GetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &ptr));
  EXPECT_EQ(nullptr, ptr);
  GM_CALL(GetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &ptr));
  EXPECT_EQ(nullptr, ptr);
  GM_CALL(DebugMessageCallback(callback_lambda, &debug_callback));
  GM_CALL(GetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &ptr));
  EXPECT_EQ(static_cast<GLDEBUGPROC>(callback_lambda), ptr);
  GM_CALL(GetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &ptr));
  EXPECT_EQ(&debug_callback, ptr);

  // Verify that a successful GL call does not report an error.
  callback_source = 0;
  callback_type = 0;
  callback_id = ~0;
  callback_severity = 0;
  callback_message.clear();
  GM_CALL(DepthFunc(GL_NEVER));
  EXPECT_EQ(0U, callback_source);
  EXPECT_EQ(0U, callback_type);
  EXPECT_EQ(~0U, callback_id);
  EXPECT_EQ(0U, callback_severity);
  EXPECT_TRUE(callback_message.empty());

  // Verify that an unsuccessful GL call reports an error.
  GM_ERROR_CALL(DepthFunc(GL_DITHER), GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_TYPE_ERROR), callback_type);
  EXPECT_EQ(0U, callback_id);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SEVERITY_HIGH), callback_severity);
  EXPECT_FALSE(callback_message.empty());

  // Verify that glDebugMessageInsert() rejects incorrect parameters.
  GLint max_debug_message_length = 0;
  GM_CALL(GetIntegerv(GL_MAX_DEBUG_MESSAGE_LENGTH, &max_debug_message_length));
  callback_source = 0;
  callback_type = 0;
  callback_id = ~0;
  callback_severity = 0;
  callback_message.clear();
  const std::string app_message("This is a test app message.");
  // Invalid source.
  GM_ERROR_CALL(
      DebugMessageInsert(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE, 4,
                         GL_DEBUG_SEVERITY_MEDIUM, -1, app_message.c_str()),
      GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  // Invalid type.
  GM_ERROR_CALL(
      DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DONT_CARE, 4,
                         GL_DEBUG_SEVERITY_MEDIUM, -1, app_message.c_str()),
      GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  // Invalid severity.
  GM_ERROR_CALL(
      DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE,
                         4, GL_DONT_CARE, -1, app_message.c_str()),
      GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  // Invalid length.
  GM_ERROR_CALL(
      DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE,
                         4, GL_DEBUG_SEVERITY_MEDIUM, max_debug_message_length,
                         app_message.c_str()),
      GL_INVALID_VALUE);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  const std::string too_long_message =
      app_message +
      std::string(max_debug_message_length - app_message.size(), ' ');
  GM_ERROR_CALL(DebugMessageInsert(
                    GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE, 4,
                    GL_DEBUG_SEVERITY_MEDIUM, -1, too_long_message.c_str()),
                GL_INVALID_VALUE);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);

  // Verify that GL_DEBUG_SEVERITY_LOW messages are suppressed by default.
  callback_source = 0;
  callback_type = 0;
  callback_id = ~0;
  callback_severity = 0;
  callback_message.clear();
  GM_CALL(DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                             GL_DEBUG_TYPE_PERFORMANCE, 4,
                             GL_DEBUG_SEVERITY_LOW, -1, app_message.c_str()));
  EXPECT_EQ(0U, callback_source);
  EXPECT_EQ(0U, callback_type);
  EXPECT_EQ(~0U, callback_id);
  EXPECT_EQ(0U, callback_severity);
  EXPECT_TRUE(callback_message.empty());

  // Verify success with a correct message and parameters.
  GM_CALL(
      DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE,
                         4, GL_DEBUG_SEVERITY_MEDIUM, -1, app_message.c_str()));
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_APPLICATION), callback_source);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_TYPE_PERFORMANCE), callback_type);
  EXPECT_EQ(4U, callback_id);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SEVERITY_MEDIUM), callback_severity);
  EXPECT_EQ(app_message, callback_message);

  // Verify that glDebugMessageControl() rejects incorrect parameters.
  callback_source = 0;
  callback_type = 0;
  callback_id = ~0;
  callback_severity = 0;
  callback_message.clear();
  // Invalid source.
  GM_ERROR_CALL(DebugMessageControl(GL_TRUE, GL_DEBUG_TYPE_PERFORMANCE,
                                    GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, true),
                GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  // Invalid type.
  GM_ERROR_CALL(DebugMessageControl(GL_DEBUG_SOURCE_APPLICATION, GL_TRUE,
                                    GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, true),
                GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  // Invalid severity.
  GM_ERROR_CALL(
      DebugMessageControl(GL_DEBUG_SOURCE_APPLICATION,
                          GL_DEBUG_TYPE_PERFORMANCE, GL_TRUE, 0, nullptr, true),
      GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  GLuint app_id = 4;
  // Must specify source with ids.
  GM_ERROR_CALL(DebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE,
                                    GL_DONT_CARE, 1, &app_id, true),
                GL_INVALID_OPERATION);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  // Must specify type with ids.
  GM_ERROR_CALL(DebugMessageControl(GL_DEBUG_SOURCE_APPLICATION, GL_DONT_CARE,
                                    GL_DONT_CARE, 1, &app_id, true),
                GL_INVALID_OPERATION);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;
  // Cannot specify ids and severity simultaneously.
  GM_ERROR_CALL(DebugMessageControl(GL_DEBUG_SOURCE_APPLICATION,
                                    GL_DEBUG_TYPE_PERFORMANCE,
                                    GL_DEBUG_SEVERITY_MEDIUM, 1, &app_id, true),
                GL_INVALID_OPERATION);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), callback_source);
  callback_source = 0;

  // Verify that GL_DEBUG_SEVERITY_LOW output can be turned on.
  GM_CALL(DebugMessageControl(GL_DEBUG_SOURCE_APPLICATION,
                              GL_DEBUG_TYPE_PERFORMANCE, GL_DEBUG_SEVERITY_LOW,
                              0, nullptr, true));
  callback_source = 0;
  callback_type = 0;
  callback_id = ~0;
  callback_severity = 0;
  callback_message.clear();
  GM_CALL(DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                             GL_DEBUG_TYPE_PERFORMANCE, 4,
                             GL_DEBUG_SEVERITY_LOW, -1, app_message.c_str()));
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_APPLICATION), callback_source);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_TYPE_PERFORMANCE), callback_type);
  EXPECT_EQ(4U, callback_id);
  EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SEVERITY_LOW), callback_severity);
  EXPECT_EQ(app_message, callback_message);

  // Verify that a particular message can be turned off.
  GM_CALL(DebugMessageControl(GL_DEBUG_SOURCE_APPLICATION,
                              GL_DEBUG_TYPE_PERFORMANCE, GL_DONT_CARE, 1,
                              &app_id, false));
  callback_source = 0;
  callback_type = 0;
  callback_id = ~0;
  callback_severity = 0;
  callback_message.clear();
  GM_CALL(DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION,
                             GL_DEBUG_TYPE_PERFORMANCE, 4,
                             GL_DEBUG_SEVERITY_HIGH, -1, app_message.c_str()));
  EXPECT_EQ(0U, callback_source);
  EXPECT_EQ(0U, callback_type);
  EXPECT_EQ(~0U, callback_id);
  EXPECT_EQ(0U, callback_severity);
  EXPECT_TRUE(callback_message.empty());

  // Verify that the callback is unset correctly.
  GM_CALL(GetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &ptr));
  EXPECT_EQ(static_cast<GLDEBUGPROC>(callback_lambda), ptr);
  GM_CALL(GetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &ptr));
  EXPECT_EQ(&debug_callback, ptr);
  GM_CALL(DebugMessageCallback(nullptr, nullptr));
  GM_CALL(GetPointerv(GL_DEBUG_CALLBACK_FUNCTION, &ptr));
  EXPECT_EQ(nullptr, ptr);
  GM_CALL(GetPointerv(GL_DEBUG_CALLBACK_USER_PARAM, &ptr));
  EXPECT_EQ(nullptr, ptr);

  // Verify that the debug message log logs the debug output if a callback is
  // unset, and that the log holds as many messages as it advertises through
  // GL_MAX_DEBUG_LOGGED_MESSAGES.
  // We fill up the message log with message ids counting from
  // (|max_debug_logged_messages| - 1) to 0, the the last one being an
  // API-generated error.
  GLint max_debug_logged_messages = 0;
  GLint debug_logged_messages = 0;
  GLint debug_next_logged_message_length = 0;
  GM_CALL(
      GetIntegerv(GL_MAX_DEBUG_LOGGED_MESSAGES, &max_debug_logged_messages));
  GM_CALL(GetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &debug_logged_messages));
  GM_CALL(GetIntegerv(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH,
                      &debug_next_logged_message_length));
  EXPECT_EQ(0, debug_logged_messages);
  EXPECT_EQ(0, debug_next_logged_message_length);
  for (GLint i = 0; i + 1 < max_debug_logged_messages; ++i) {
    GM_CALL(DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER,
                               max_debug_logged_messages - 1 - i,
                               GL_DEBUG_SEVERITY_MEDIUM, -1,
                               app_message.c_str()));
    GM_CALL(GetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &debug_logged_messages));
    EXPECT_EQ(i + 1, debug_logged_messages);
  }
  // Use an API-generated error to fill the last entry.  Note that we assume
  // below that this will generate a message with an id of 0.
  GM_ERROR_CALL(DepthFunc(GL_DITHER), GL_INVALID_ENUM);
  debug_logged_messages = 0;
  debug_next_logged_message_length = 0;
  GM_CALL(GetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &debug_logged_messages));
  GM_CALL(GetIntegerv(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH,
                      &debug_next_logged_message_length));
  EXPECT_EQ(max_debug_logged_messages, debug_logged_messages);
  EXPECT_EQ(app_message.size() + 1,
            static_cast<size_t>(debug_next_logged_message_length));

  // Verify that the entire debug message log can be downloaded.  We purposely
  // allocate enough download space for one message more than the mesage log's
  // advertised capacity, to catch off-by-ones.
  std::vector<GLenum> sources(max_debug_logged_messages + 1, 0);
  std::vector<GLenum> types(max_debug_logged_messages + 1, 0);
  std::vector<GLuint> ids(max_debug_logged_messages + 1, 0);
  std::vector<GLenum> severities(max_debug_logged_messages + 1, 0);
  std::vector<GLsizei> lengths(max_debug_logged_messages + 1, 0);
  std::vector<GLchar> messageLog(max_debug_logged_messages * 64, '\0');
  size_t message_offset = 0;
  GLuint message_count = gm->GetDebugMessageLog(
      max_debug_logged_messages + 1, static_cast<GLsizei>(messageLog.size()),
      sources.data(), types.data(), ids.data(), severities.data(),
      lengths.data(), messageLog.data());
  std::string message_string(messageLog.data(), messageLog.size());
  EXPECT_EQ(max_debug_logged_messages, static_cast<GLint>(message_count));
  for (GLuint i = 0; i < message_count; ++i) {
    if (i + 1 < message_count) {
      // Expect the first |max_debug_logged_messages| - 1 messages to be those
      // we inserted with glDebugMessageInsert().
      EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_APPLICATION), sources[i]);
      EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_TYPE_OTHER), types[i]);
      EXPECT_EQ(message_count - 1 - i, ids[i]);
      EXPECT_EQ(static_cast<GLsizei>(app_message.size()) + 1, lengths[i]);
      EXPECT_EQ(message_offset,
                message_string.find(app_message, message_offset));
      message_offset += lengths[i];
    } else {
      // Expect the last message to be one generated by an API call.
      EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_SOURCE_API), sources[i]);
      EXPECT_EQ(static_cast<GLenum>(GL_DEBUG_TYPE_ERROR), types[i]);
      EXPECT_EQ(0U, ids[i]);
    }
  }

  debug_logged_messages = 0;
  debug_next_logged_message_length = 0;
  GM_CALL(GetIntegerv(GL_DEBUG_LOGGED_MESSAGES, &debug_logged_messages));
  GM_CALL(GetIntegerv(GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH,
                      &debug_next_logged_message_length));
  EXPECT_EQ(0, debug_logged_messages);
  EXPECT_EQ(0, debug_next_logged_message_length);
}

TEST(MockGraphicsManagerTest, DrawBuffer) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // Invalid enum.
  GM_ERROR_CALL(DrawBuffer(GL_RED), GL_INVALID_ENUM);

  // Successful calls.
  GM_CALL(DrawBuffer(GL_NONE));
  EXPECT_EQ(GL_NONE, GetInt(gm, GL_DRAW_BUFFER));
  GM_CALL(DrawBuffer(GL_COLOR_ATTACHMENT0));
  EXPECT_EQ(GL_COLOR_ATTACHMENT0, GetInt(gm, GL_DRAW_BUFFER));
  GM_CALL(DrawBuffer(GL_NONE));
  EXPECT_EQ(GL_NONE, GetInt(gm, GL_DRAW_BUFFER));
}

TEST(MockGraphicsManagerTest, ReadBuffer) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // Invalid enum.
  GM_ERROR_CALL(ReadBuffer(GL_RED), GL_INVALID_ENUM);

  // Successful calls.
  GM_CALL(ReadBuffer(GL_NONE));
  EXPECT_EQ(GL_NONE, GetInt(gm, GL_READ_BUFFER));
  GM_CALL(ReadBuffer(GL_COLOR_ATTACHMENT0));
  EXPECT_EQ(GL_COLOR_ATTACHMENT0, GetInt(gm, GL_READ_BUFFER));
  GM_CALL(ReadBuffer(GL_NONE));
  EXPECT_EQ(GL_NONE, GetInt(gm, GL_READ_BUFFER));
}

TEST(MockGraphicsManagerTest, Sync) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  // Invalid parameters for fence creation.
  GM_ERROR_CALL(FenceSync(0, 0), GL_INVALID_ENUM);
  GM_ERROR_CALL(FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 1), GL_INVALID_VALUE);

  // Create a sync object properly.
  GLsync sync = gm->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  GM_CHECK_NO_ERROR;

  // Create a sync object to delete immediately.  This becomes an invalid sync
  // object.
  GLsync invalid_sync = gm->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  GM_CHECK_NO_ERROR;
  GM_CALL(DeleteSync(invalid_sync));

  // Invalid parameters for WaitSync.
  GM_ERROR_CALL(WaitSync(0, 0, GL_TIMEOUT_IGNORED), GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      WaitSync(invalid_sync, 0, GL_TIMEOUT_IGNORED), GL_INVALID_OPERATION);
  GM_ERROR_CALL(WaitSync(sync, 1, GL_TIMEOUT_IGNORED), GL_INVALID_VALUE);
  GM_ERROR_CALL(WaitSync(sync, 0, 1), GL_INVALID_VALUE);

  // Sync object is not signaled until we wait for it.
  GLint value = 0;
  GLsizei length = 0;
  GM_CALL(GetSynciv(sync, GL_SYNC_STATUS, sizeof(value), &length, &value));
  EXPECT_EQ(GL_UNSIGNALED, value);

  // Wait successfully.
  GM_CALL(WaitSync(sync, 0, GL_TIMEOUT_IGNORED));

  // Invalid parameters for ClientWaitSync.
  GM_ERROR_CALL(ClientWaitSync(0, 0, 0), GL_INVALID_VALUE);
  GM_ERROR_CALL(ClientWaitSync(invalid_sync, 0, 0), GL_INVALID_VALUE);
  GM_ERROR_CALL(ClientWaitSync(sync, ~0, 0), GL_INVALID_VALUE);

  // Client wait successfully.
  GM_CALL(ClientWaitSync(sync, 0, 0));
  GM_CALL(ClientWaitSync(sync, 0, 10));
  GM_CALL(ClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0));
  GM_CALL(ClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 10));

  // Invalid parameters to GetSynciv.
  GM_ERROR_CALL(GetSynciv(0, GL_OBJECT_TYPE, sizeof(value), &length, &value),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(
      GetSynciv(invalid_sync, GL_OBJECT_TYPE, sizeof(value), &length, &value),
      GL_INVALID_VALUE);
  GM_CALL(GetSynciv(sync, GL_OBJECT_TYPE, sizeof(value), &length, &value));
  EXPECT_EQ(GL_SYNC_FENCE, value);
  GM_CALL(GetSynciv(sync, GL_SYNC_STATUS, sizeof(value), &length, &value));
  EXPECT_EQ(GL_SIGNALED, value);
  GM_CALL(GetSynciv(sync, GL_SYNC_CONDITION, sizeof(value), &length, &value));
  EXPECT_EQ(GL_SYNC_GPU_COMMANDS_COMPLETE, value);
  GM_CALL(GetSynciv(sync, GL_SYNC_FLAGS, sizeof(value), &length, &value));
  EXPECT_EQ(0, value);

  // Delete sync objects.
  GM_CALL(DeleteSync(0)),
  GM_CALL(DeleteSync(sync));
  GM_ERROR_CALL(DeleteSync(invalid_sync), GL_INVALID_VALUE);
}

TEST(MockGraphicsManagerTest, DisjointTimerQuery) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[2] = {0, 0};
  GLint num = 0;
  GLuint unum = 0;
  GLint64 num64 = 0;
  GLuint64 unum64 = 0;

  // Error descriptions pasted from:
  // https://www.khronos.org/registry/gles/extensions/EXT/
  // EXT_disjoint_timer_query.txt

  // The error INVALID_VALUE is generated if GenQueriesEXT is called where
  // <n> is negative.
  GM_ERROR_CALL(GenQueries(-1, ids), GL_INVALID_VALUE);

  // The error INVALID_VALUE is generated if DeleteQueriesEXT is called
  // where <n> is negative.
  GM_ERROR_CALL(DeleteQueries(-1, ids), GL_INVALID_VALUE);

  // The error INVALID_OPERATION is generated if BeginQueryEXT is called
  // when a query of the given <target> is already active.
  GM_CALL(GenQueries(2, ids));
  GM_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, ids[0]));
  GM_ERROR_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, ids[1]), GL_INVALID_OPERATION);
  GM_CALL(EndQuery(GL_TIME_ELAPSED_EXT));

  // The error INVALID_OPERATION is generated if EndQueryEXT is called
  // when a query of the given <target> is not active.
  GM_ERROR_CALL(EndQuery(GL_TIME_ELAPSED_EXT), GL_INVALID_OPERATION);

  // The error INVALID_OPERATION is generated if BeginQueryEXT is called
  // where <id> is zero.
  GM_ERROR_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, 0), GL_INVALID_OPERATION);

  // The error INVALID_OPERATION is generated if BeginQueryEXT is called
  // where <id> is the name of a query currently in progress.
  GM_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, ids[0]));
  GM_ERROR_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, ids[0]), GL_INVALID_OPERATION);
  GM_CALL(EndQuery(GL_TIME_ELAPSED_EXT));

  // The error INVALID_ENUM is generated if BeginQueryEXT or EndQueryEXT
  // is called where <target> is not TIME_ELAPSED_EXT.
  GM_ERROR_CALL(BeginQuery(GL_TIMESTAMP_EXT, ids[0]), GL_INVALID_ENUM);
  GM_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, ids[0]));
  GM_ERROR_CALL(EndQuery(GL_TIMESTAMP_EXT), GL_INVALID_ENUM);
  GM_CALL(EndQuery(GL_TIME_ELAPSED_EXT));

  // The error INVALID_ENUM is generated if GetQueryivEXT is called where
  // <target> is not TIME_ELAPSED_EXT or TIMESTAMP_EXT.
  GM_CALL(GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, &num));
  GM_CALL(GetQueryiv(GL_TIME_ELAPSED_EXT, GL_QUERY_COUNTER_BITS_EXT, &num));
  GM_ERROR_CALL(GetQueryiv(GL_TIMEOUT_EXPIRED, GL_QUERY_COUNTER_BITS_EXT,
                           &num),
                GL_INVALID_ENUM);

  // The error INVALID_ENUM is generated if GetQueryivEXT is called where
  // <pname> is not QUERY_COUNTER_BITS_EXT or CURRENT_QUERY_EXT.
  GM_CALL(GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_COUNTER_BITS_EXT, &num));
  GM_CALL(GetQueryiv(GL_TIMESTAMP_EXT, GL_CURRENT_QUERY_EXT, &num));
  GM_CALL(GetQueryiv(GL_TIME_ELAPSED_EXT, GL_QUERY_COUNTER_BITS_EXT, &num));
  GM_CALL(GetQueryiv(GL_TIME_ELAPSED_EXT, GL_CURRENT_QUERY_EXT, &num));
  GM_ERROR_CALL(GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_OBJECT, &num),
                GL_INVALID_ENUM);

  // The error INVALID_ENUM is generated if QueryCounterEXT is called where
  // <target> is not TIMESTAMP_EXT.
  GM_CALL(QueryCounter(ids[0], GL_TIMESTAMP_EXT));
  GM_ERROR_CALL(QueryCounter(GL_TIMEOUT_EXPIRED, ids[0]), GL_INVALID_ENUM);

  // The error INVALID_OPERATION is generated if QueryCounterEXT is called
  // on a query object that is already in use inside a
  // BeginQueryEXT/EndQueryEXT.
  GM_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, ids[0]));
  GM_ERROR_CALL(QueryCounter(ids[0], GL_TIMESTAMP_EXT), GL_INVALID_OPERATION);
  GM_CALL(EndQuery(GL_TIME_ELAPSED_EXT));

  // The error INVALID_OPERATION is generated if GetQueryObjectivEXT,
  // GetQueryObjectuivEXT, GetQueryObjecti64vEXT, or
  // GetQueryObjectui64vEXT is called where <id> is not the name of a query
  // object.
  GM_ERROR_CALL(GetQueryObjectiv(123, GL_QUERY_RESULT_EXT, &num),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetQueryObjectuiv(123, GL_QUERY_RESULT_EXT, &unum),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetQueryObjecti64v(123, GL_QUERY_RESULT_EXT, &num64),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetQueryObjectui64v(123, GL_QUERY_RESULT_EXT, &unum64),
                GL_INVALID_OPERATION);

  // The error INVALID_OPERATION is generated if GetQueryObjectivEXT,
  // GetQueryObjectuivEXT, GetQueryObjecti64vEXT, or
  // GetQueryObjectui64vEXT is called where <id> is the name of a currently
  // active query object.
  GM_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, ids[0]));
  GM_ERROR_CALL(GetQueryObjectiv(ids[0], GL_QUERY_RESULT_EXT, &num),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetQueryObjectuiv(ids[0], GL_QUERY_RESULT_EXT, &unum),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetQueryObjecti64v(ids[0], GL_QUERY_RESULT_EXT, &num64),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetQueryObjectui64v(ids[0], GL_QUERY_RESULT_EXT, &unum64),
                GL_INVALID_OPERATION);
  GM_CALL(EndQuery(GL_TIME_ELAPSED_EXT));

  // The error INVALID_ENUM is generated if GetQueryObjectivEXT,
  // GetQueryObjectuivEXT, GetQueryObjecti64vEXT, or
  // GetQueryObjectui64vEXT is called where <pname> is not
  // QUERY_RESULT_EXT or QUERY_RESULT_AVAILABLE_EXT.
  GM_ERROR_CALL(GetQueryObjectiv(ids[0], GL_QUERY_OBJECT, &num),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetQueryObjectuiv(ids[0], GL_QUERY_OBJECT, &unum),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetQueryObjecti64v(ids[0], GL_QUERY_OBJECT, &num64),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetQueryObjectui64v(ids[0], GL_QUERY_OBJECT, &unum64),
                GL_INVALID_ENUM);
  EXPECT_EQ(0, num);
  EXPECT_EQ(0u, unum);
  EXPECT_EQ(0ll, num64);
  EXPECT_EQ(0ull, unum64);

  // Successful calls.

  // Begin/End Query
  GM_CALL(BeginQuery(GL_TIME_ELAPSED_EXT, ids[0]));
  GM_CALL(EndQuery(GL_TIME_ELAPSED_EXT));
  GM_CALL(GetQueryObjectiv(ids[0], GL_QUERY_RESULT_AVAILABLE_EXT, &num));
  EXPECT_NE(0, num);
  GM_CALL(GetQueryObjecti64v(ids[0], GL_QUERY_RESULT_EXT, &num64));
  EXPECT_NE(0ll, num64);
  num = 0;
  num64 = 0;

  // QueryCounter
  GM_CALL(QueryCounter(ids[0], GL_TIMESTAMP_EXT));
  GM_CALL(QueryCounter(ids[1], GL_TIMESTAMP_EXT));
  GM_CALL(GetQueryObjectiv(ids[0], GL_QUERY_RESULT_AVAILABLE_EXT, &num));
  EXPECT_NE(0, num);
  num = 0;
  GM_CALL(GetQueryObjectiv(ids[1], GL_QUERY_RESULT_AVAILABLE_EXT, &num));
  EXPECT_NE(0, num);
  GM_CALL(GetQueryObjecti64v(ids[0], GL_QUERY_RESULT_EXT, &num64));
  EXPECT_NE(0ll, num64);
  num64 = 0;
  GM_CALL(GetQueryObjecti64v(ids[1], GL_QUERY_RESULT_EXT, &num64));
  EXPECT_NE(0ll, num64);

  // Delete
  EXPECT_TRUE(gm->IsQuery(ids[0]));
  EXPECT_TRUE(gm->IsQuery(ids[1]));
  GM_CALL(DeleteQueries(2, ids));
  EXPECT_FALSE(gm->IsQuery(ids[0]));
  EXPECT_FALSE(gm->IsQuery(ids[1]));
}

TEST(MockGraphicsManagerTest, TransformFeedbackFunctions) {
  MockVisual visual(600, 500);
  MockGraphicsManagerPtr gm(new MockGraphicsManager());

  GLuint ids[] = {1, 2};
  GM_CALL(GenTransformFeedbacks(arraysize(ids), ids));
  EXPECT_TRUE(gm->IsTransformFeedback(ids[0]));
  EXPECT_TRUE(gm->IsTransformFeedback(ids[1]));
  GM_CALL(DeleteTransformFeedbacks(arraysize(ids), ids));
  // Deleted transform feedback objects.
  EXPECT_FALSE(gm->IsTransformFeedback(ids[0]));
  EXPECT_FALSE(gm->IsTransformFeedback(ids[1]));
  GM_ERROR_CALL(BindTransformFeedback(GL_TRANSFORM_FEEDBACK, ids[0]),
                GL_INVALID_OPERATION);
  GM_CALL(GenTransformFeedbacks(arraysize(ids), ids));
  EXPECT_TRUE(gm->IsTransformFeedback(ids[0]));
  EXPECT_TRUE(gm->IsTransformFeedback(ids[1]));

  // Error target and wrong id.
  GM_ERROR_CALL(BindTransformFeedback(GL_TRANSFORM_FEEDBACK - 1, ids[0]),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(BindTransformFeedback(GL_TRANSFORM_FEEDBACK, 23),
                GL_INVALID_OPERATION);
  GM_CALL(BindTransformFeedback(GL_TRANSFORM_FEEDBACK, ids[0]));

  // Wrong program name.
  GM_ERROR_CALL(
      TransformFeedbackVaryings(-1, 0, nullptr, GL_INTERLEAVED_ATTRIBS),
      GL_INVALID_VALUE);

  const std::string vertex_source(kVertexSource);
  const std::string fragment_source(kFragmentSource);
  GLuint vid = gm->CreateShader(GL_VERTEX_SHADER);
  GLuint fid = gm->CreateShader(GL_FRAGMENT_SHADER);
  GM_CHECK_NO_ERROR;
  {
    GLint length = static_cast<GLuint>(vertex_source.length());
    const char* ptr = vertex_source.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
  }
  {
    GLint length = static_cast<GLuint>(fragment_source.length());
    const char* ptr = fragment_source.c_str();
    GM_CALL(ShaderSource(fid, 1, &ptr, &length));
  }
  GLuint pid = gm->CreateProgram();
  GM_CALL(CompileShader(vid));
  GM_CALL(CompileShader(fid));
  GM_CALL(AttachShader(pid, vid));
  GM_CALL(AttachShader(pid, fid));

  const char* varyings[2];
  varyings[0] = "vary_v2f";
  varyings[1] = "vary_m4f";

  GM_CALL(TransformFeedbackVaryings(pid, 2, varyings, GL_INTERLEAVED_ATTRIBS));
  // Program not linked.
  GM_ERROR_CALL(GetTransformFeedbackVarying(pid, 0, 0, nullptr, nullptr,
                                            nullptr, nullptr),
                GL_INVALID_OPERATION);

  GM_CALL(LinkProgram(pid));
  GM_CALL(UseProgram(pid));

  // Resume when it is not active or paused.
  GM_ERROR_CALL(ResumeTransformFeedback(), GL_INVALID_OPERATION);
  // Pause when it is not active or paused
  GM_ERROR_CALL(PauseTransformFeedback(), GL_INVALID_OPERATION);
  // Wrong primitive_mode.
  GM_ERROR_CALL(BeginTransformFeedback(GL_POINTS - 1), GL_INVALID_ENUM);
  GM_CALL(BeginTransformFeedback(GL_POINTS));
  // Begin when it is already active.
  GM_ERROR_CALL(BeginTransformFeedback(GL_POINTS), GL_INVALID_OPERATION);
  // Resume when it is already active and not paused.
  GM_ERROR_CALL(ResumeTransformFeedback(), GL_INVALID_OPERATION);
  // Bind transform feedback when there is already one active.
  GM_ERROR_CALL(BindTransformFeedback(GL_TRANSFORM_FEEDBACK, ids[1]),
                GL_INVALID_OPERATION);
  GM_CALL(PauseTransformFeedback());
  // Pause when it is already paused.
  GM_ERROR_CALL(PauseTransformFeedback(), GL_INVALID_OPERATION);
  GM_CALL(ResumeTransformFeedback());
  // Draw arrays with the wrong primitive_mode.
  GM_ERROR_CALL(DrawArrays(GL_LINES, 0, 1), GL_INVALID_OPERATION);
  // Draw elements with the transform feedback active and not paused.
  GM_ERROR_CALL(DrawElements(GL_LINES, 0, GL_UNSIGNED_BYTE, nullptr),
                GL_INVALID_OPERATION);
  // Draw arrays instanced with the wrong primitive_mode.
  GM_ERROR_CALL(DrawArraysInstanced(GL_LINES, 0, 1, 1), GL_INVALID_OPERATION);
  // Draw elements instanced with the transform feedback active and not paused.
  GM_ERROR_CALL(
      DrawElementsInstanced(GL_LINES, 0, GL_UNSIGNED_BYTE, nullptr, 1),
      GL_INVALID_OPERATION);
  // Wrong pid for the program.
  GM_ERROR_CALL(
      GetTransformFeedbackVarying(-1, 0, 0, nullptr, nullptr, nullptr, nullptr),
      GL_INVALID_VALUE);
  // Large index.
  GM_ERROR_CALL(GetTransformFeedbackVarying(pid, 1000, 0, nullptr, nullptr,
                                            nullptr, nullptr),
                GL_INVALID_VALUE);
  const GLsizei buf_size = 20;
  char name[buf_size];
  GLsizei length = 0;
  GLsizei size = 0;
  GLenum type = static_cast<GLenum>(-1);
  GM_CALL(GetTransformFeedbackVarying(pid, 0, buf_size, &length, &size, &type,
                                      name));
  EXPECT_EQ(length, 8);
  EXPECT_EQ(size, 1);
  EXPECT_EQ(type, static_cast<GLenum>(GL_FLOAT_VEC2));
  EXPECT_EQ(0, strcmp(name, "vary_v2f"));
  GM_CALL(GetTransformFeedbackVarying(pid, 1, buf_size, &length, &size, &type,
                                      name));
  EXPECT_EQ(length, 8);
  EXPECT_EQ(size, 1);
  EXPECT_EQ(type, static_cast<GLenum>(GL_FLOAT_MAT4));
  EXPECT_EQ(0, strcmp(name, "vary_m4f"));
  GM_CALL(EndTransformFeedback());
  GM_CALL(DrawArrays(GL_LINES, 0, 1));
  GM_CALL(DrawElements(GL_LINES, 0, GL_UNSIGNED_BYTE, nullptr));
  GM_CALL(DrawArraysInstanced(GL_LINES, 0, 1, 1));
  GM_CALL(DrawElementsInstanced(GL_LINES, 0, GL_UNSIGNED_BYTE, nullptr, 1));
}

}  // namespace testing
}  // namespace gfx
}  // namespace ion

#endif  // ION_PRODUCTION
