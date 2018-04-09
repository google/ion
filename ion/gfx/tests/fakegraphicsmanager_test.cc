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

// These tests rely on trace streams, which are disabled in production builds.
#if !ION_PRODUCTION

#include "ion/gfx/tests/fakegraphicsmanager.h"

#include <array>
#include <functional>
#include <memory>
#include <sstream>

#include "base/integral_types.h"
#include "ion/base/logchecker.h"
#include "ion/base/sharedptr.h"
#include "ion/gfx/tests/traceverifier.h"
#include "ion/gfx/tracinghelper.h"
#include "ion/math/matrix.h"
#include "ion/math/vector.h"
#include "ion/portgfx/glcontext.h"
#include "absl/base/macros.h"
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
template <size_t... Indices> struct Sequence {};

// Inner case, instantiates an N-1 SequenceGenerator.
template <size_t N, size_t... Indices>
struct SequenceGenerator : SequenceGenerator<N - 1U, N - 1U, Indices...> {};

// Specializes for 0 base case, generating a Sequence with the other indices.
template <size_t... Indices>
struct SequenceGenerator<0U, Indices...> : Sequence<Indices...> {};

// Shader sources.
static const char kVertexSource[] = R"glsl("
    // Vertex shader.
    attribute float attr_f;
    // Technically the next line is an error, but it helps coverage.
    attribute float attr_f;
    attribute vec2 attr_v2f;
    attribute vec3 attr_v3f;
    attribute vec4 attr_v4f;
    attribute mat2 attr_m2f;
    attribute mat3 attr_m3f;
    attribute mat4 attr_m4f;
    uniform highp float uni_f;
    uniform lowp vec2 uni_v2f;
    uniform vec3 uni_v3f;
    uniform vec4 uni_v4f;
    uniform int uni_i;
    uniform ivec2 uni_v2i;
    uniform ivec3 uni_v3i;
    uniform ivec4 uni_v4i;
    uniform uint uni_u;
    uniform uvec2 uni_v2u;
    uniform uvec3 uni_v3u;
    uniform uvec4 uni_v4u;
    uniform mat2 uni_m2;
    uniform mat3 uni_m3;
    uniform mat4 uni_m4;
    uniform isampler1D itex1d;
    uniform isampler1DArray itex1da;
    uniform isampler2D itex2d;
    uniform isampler2DArray itex2da;
    uniform isampler3D itex3d;
    uniform isamplerCube icm;
    uniform isamplerCubeArray icma;
    uniform sampler1D tex1d;
    uniform sampler1DArray tex1da;
    uniform sampler1DArrayShadow tex1das;
    uniform sampler1DShadow tex1ds;
    uniform sampler2D tex2d;
    uniform sampler2DArray tex2da;
    uniform sampler2DArrayShadow tex2das;
    uniform sampler2DShadow tex2ds;
    uniform sampler3D tex3d;
    uniform samplerCube cm;
    uniform samplerCubeArray cma;
    uniform samplerCubeArrayShadow cmas;
    uniform samplerCubeShadow cms;
    uniform samplerExternalOES seo;
    uniform usampler1D utex1d;
    uniform usampler1DArray utex1da;
    uniform usampler2D utex2d;
    uniform usampler2DArray utex2da;
    uniform usampler3D utex3d;
    uniform usamplerCube ucm;
    uniform usamplerCubeArray ucma;
    // Will not generate a uniform.
    uniform no_type bad_var;
    varying vec2 vary_v2f;
    varying mat4 vary_m4f;
    void main() {
      gl_Position = vec4(1.0);
    }
)glsl";

static const char kGeometrySource[] = R"glsl(#version 150 core
    layout(triangles) in;
    layout(triangle_strip, max_vertices=3) out;
    uniform int guni_i;
    uniform uint guni_u;
    void main() {
      for(int i = 0; i < 3; i++) {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
      }
    EndPrimitive();
    }
)glsl";

static const char kFragmentSource[] = R"glsl(
    // Fragment shader.
    uniform highp float uni_f_array[4];
    uniform lowp vec2 uni_v2f_array[4];
    uniform vec3 uni_v3f_array[4];
    uniform vec4 uni_v4f_array[4];
    uniform int uni_i_array[4];
    uniform ivec2 uni_v2i_array[4];
    uniform ivec3 uni_v3i_array[4];
    uniform ivec4 uni_v4i_array[4];
    uniform uint uni_u_array[4];
    uniform uvec2 uni_v2u_array[4];
    uniform uvec3 uni_v3u_array[4];
    uniform uvec4 uni_v4u_array[4];
    uniform mat2 uni_m2_array[4];
    uniform mat3 uni_m3_array[4];
    uniform mat4 uni_m4_array[4];
    uniform isampler1D itex1d_array[4];
    uniform isampler1DArray itex1da_array[4];
    uniform isampler2D itex2d_array[4];
    uniform isampler2DArray itex2da_array[4];
    uniform isampler3D itex3d_array[4];
    uniform isamplerCube icm_array[4];
    uniform isamplerCubeArray icma_array[4];
    uniform sampler1D tex1d_array[4];
    uniform sampler1DArray tex1da_array[4];
    uniform sampler1DArrayShadow tex1das_array[4];
    uniform sampler1DShadow tex1ds_array[4];
    uniform sampler2D tex2d_array[4];
    uniform sampler2DArray tex2da_array[4];
    uniform sampler2DArrayShadow tex2das_array[4];
    uniform sampler2DShadow tex2ds_array[4];
    uniform sampler3D tex3d_array[4];
    uniform samplerCube cm_array[4];
    uniform samplerCubeArray cma_array[4];
    uniform samplerCubeArrayShadow cmas_array[4];
    uniform samplerCubeShadow cms_array[4];
    uniform samplerExternalOES seo_array[4];
    uniform usampler1D utex1d_array[4];
    uniform usampler1DArray utex1da_array[4];
    uniform usampler2D utex2d_array[4];
    uniform usampler2DArray utex2da_array[4];
    uniform usampler3D utex3d_array[4];
    uniform usamplerCube ucm_array[4];
    uniform usamplerCubeArray ucma_array[4];
    varying vec2 vary_v2f;
)glsl";

const int kWidth = 500;
const int kHeight = 600;

}  // end anonymous namespace

// Convenience defines to call a GraphicsManager function and check a particular
// error or no error occurred.
#define GM_CHECK_ERROR(error) \
  EXPECT_EQ(static_cast<GLenum>(error), gm_->GetError())
#define GM_CHECK_NO_ERROR \
  GM_CHECK_ERROR(GL_NO_ERROR)
#define GM_ERROR_CALL(call, error) gm_->call; GM_CHECK_ERROR(error)
#define GM_CALL(call) GM_ERROR_CALL(call, GL_NO_ERROR)

class FakeGraphicsManagerTest : public ::testing::Test {
 public:
  // Convenience function to get a float value from an OpenGL vertex attribute.
  GLfloat GetAttribFloat(GLuint index, GLenum what) {
    GLfloat f;
    GM_CALL(GetVertexAttribfv(index, what, &f));
    return f;
  }
  // Convenience function to get a vec4f value from an OpenGL vertex attribute.
  math::Vector4f GetAttribFloat4(GLuint index, GLenum what) {
    math::Vector4f f;
    GM_CALL(GetVertexAttribfv(index, what, &f[0]));
    return f;
  }
  // Convenience function to get an integer value from an OpenGL vertex
  // attribute.
  GLint GetAttribInt(GLuint index, GLenum what) {
    GLint i;
    GM_CALL(GetVertexAttribiv(index, what, &i));
    return i;
  }
  // Convenience function to get a vec4i value from an OpenGL vertex attribute.
  math::Vector4i GetAttribInt4(GLuint index, GLenum what) {
    math::Vector4i i;
    GM_CALL(GetVertexAttribiv(index, what, &i[0]));
    return i;
  }

  // Convenience function to get a vec4f value from an OpenGL vertex attribute.
  GLvoid* GetAttribPointer(GLuint index, GLenum what) {
    GLvoid* p;
    GM_CALL(GetVertexAttribPointerv(index, what, &p));
    return p;
  }

  // Convenience function to get a single boolean value from OpenGL.
  GLboolean GetBoolean(GLenum what) {
    GLboolean b;
    GM_CALL(GetBooleanv(what, &b));
    return b;
  }

  // Convenience function to get a buffer parameter value from OpenGL.
  GLint GetBufferInt(GLenum target, GLenum what) {
    GLint i;
    GM_CALL(GetBufferParameteriv(target, what, &i));
    return i;
  }

  GLboolean GetEnabled(GLenum what) {
    GLboolean b = gm_->IsEnabled(what);
    GM_CHECK_NO_ERROR;
    // Check that GetIntegerv also returns the same value for capabilities.
    GLint i;
    GM_CALL(GetIntegerv(what, &i));
    EXPECT_EQ(b, i);
    return b;
  }

  // Convenience function to get a framebuffer attachment value from OpenGL.
  GLint GetFramebufferAttachmentInt(GLenum target, GLenum attachment,
                                    GLenum pname) {
    GLint i;
    GM_CALL(GetFramebufferAttachmentParameteriv(target, attachment, pname, &i));
    return i;
  }

  // Convenience function to get a renderbuffer parameter value from OpenGL.
  GLint GetRenderbufferInt(GLenum pname) {
    GLint i;
    GM_CALL(GetRenderbufferParameteriv(GL_RENDERBUFFER, pname, &i));
    return i;
  }

  // Convenience function to get a mask value from OpenGL.
  GLuint GetMask(GLenum what) {
    GLint i;
    GM_CALL(GetIntegerv(what, &i));
    return static_cast<GLuint>(i);
  }

  // Convenience function to get a single float value from OpenGL.
  GLfloat GetFloat(GLenum what) {
    GLfloat f;
    GM_CALL(GetFloatv(what, &f));
    return f;
  }

  // Convenience function to get a single integer value from OpenGL. Note that
  // the #defined GL_ constants are actually integers, not GLenums.
  GLint GetInt(GLenum what) {
    GLint i;
    GM_CALL(GetIntegerv(what, &i));
    return i;
  }

  // Convenience function to get a single integer value from an OpenGL program.
  GLint GetProgramInt(GLuint program, GLenum what) {
    GLint i;
    GM_CALL(GetProgramiv(program, what, &i));
    return i;
  }

  // Convenience function to get a single integer value from an OpenGL shader.
  GLint GetShaderInt(GLuint shader, GLenum what) {
    GLint i;
    GM_CALL(GetShaderiv(shader, what, &i));
    return i;
  }

  // Convenience function to get a single string value from OpenGL.
  std::string GetString(GLenum what) {
    const GLubyte* string;
    string = GM_CALL(GetString(what));
    return std::string(reinterpret_cast<const char*>(string));
  }

  // Convenience function to get a single string value from OpenGL.
  std::string GetStringi(GLenum what, GLuint index) {
    const GLubyte* string;
    string = GM_CALL(GetStringi(what, index));
    return std::string(reinterpret_cast<const char*>(string));
  }

  // Convenience functions to get a single float/integer value from an OpenGL
  // sampler.
  GLfloat GetSamplerFloat(GLuint sampler, GLenum what) {
    GLfloat f;
    GM_CALL(GetSamplerParameterfv(sampler, what, &f));
    return f;
  }
  GLint GetSamplerInt(GLuint sampler, GLenum what) {
    GLint i;
    GM_CALL(GetSamplerParameteriv(sampler, what, &i));
    return i;
  }

  // Convenience functions to get a single float/integer value from an OpenGL
  // texture.
  GLfloat GetTextureFloat(GLuint texture, GLenum what) {
    GLfloat f;
    GM_CALL(GetTexParameterfv(texture, what, &f));
    return f;
  }
  GLint GetTextureInt(GLuint texture, GLenum what) {
    GLint i;
    GM_CALL(GetTexParameteriv(texture, what, &i));
    return i;
  }

  void VerifySetAndGetLabel(GLenum type, GLuint id) {
    static const int kBufLen = 64;
    char label[kBufLen];
    GLint length = 0;

    GM_ERROR_CALL(LabelObject(type, id + 1U, 0, ""), GL_INVALID_OPERATION);
    GM_ERROR_CALL(LabelObject(type, id, -1, ""), GL_INVALID_VALUE);

    // Set the label.
    const std::string test_label("texture_label");
    GM_CALL(LabelObject(
        type, id, static_cast<GLsizei>(test_label.length()),
        test_label.c_str()));

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
  void AllocateAndAttachMultisampleRenderBuffer(GLenum internal_format,
      GLenum attachment, int width, int height, int samples) {
    GLuint id;
    GM_CALL(GenRenderbuffers(1, &id));
    GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, id));
    GM_CALL(RenderbufferStorageMultisample(
        GL_RENDERBUFFER, samples, internal_format, width, height));
    GM_CALL(FramebufferRenderbuffer(
        GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, id));
  }

  // Convenience function to allocate and attach a render buffer.
  void AllocateAndAttachRenderBuffer(const GLenum internal_format,
      GLenum attachment, int width, int height) {
    GLuint id;
    GM_CALL(GenRenderbuffers(1, &id));
    GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, id));
    GM_CALL(RenderbufferStorage(
        GL_RENDERBUFFER, internal_format, width, height));
    GM_CALL(FramebufferRenderbuffer(
        GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, id));
  }

 protected:
  void SetUp() override {
    gl_context_ = FakeGlContext::Create(kWidth, kHeight);
    portgfx::GlContext::MakeCurrent(gl_context_);
    gm_.Reset(new FakeGraphicsManager());
    gm_->EnableErrorChecking(false);
  }
  void TearDown() override {}

  base::SharedPtr<FakeGlContext> gl_context_;
  FakeGraphicsManagerPtr gm_;
};

// Calls the passed Set function on the passed FakeGraphicsManager. The passed
// array is expanded to fill the functions arguments.
template <typename T, typename... Args, size_t... Indices>
static void ExpandArgsAndCall(const FakeGraphicsManagerPtr& gm, GLint loc,
                              void (GraphicsManager::*Set)(GLint, Args...),
                              Sequence<Indices...> seq,
                              const std::array<T, 4>& args) {
  ((*gm).*Set)(loc, std::get<Indices>(args)...);
}

template <typename T, typename... Args>
static void TestUniform(const UniformInfo& info,
                        const FakeGraphicsManagerPtr& gm, GLuint pid,
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
                      values[0]);
    EXPECT_EQ(GLenum{GL_NO_ERROR}, gm->GetError());
    ((*gm).*Getv)(pid, info.loc, &test4[0][0]);
    EXPECT_EQ(GLenum{GL_NO_ERROR}, gm->GetError());
    for (GLint j = 0; j < info.length; ++j)
      EXPECT_EQ(values[0][j], test4[0][j]);

    // Test array values if available.
    for (GLint i = 0; i < array_len; ++i) {
      if (info.alocs[i] != -1) {
        ExpandArgsAndCall(gm, info.alocs[i], Set,
                          SequenceGenerator<sizeof...(Args)>{},
                          values[i]);
        EXPECT_EQ(GLenum{GL_NO_ERROR}, gm->GetError());

        // Retrieve the array element.
        ((*gm).*Getv)(pid, info.alocs[i], &test4[i][0]);
        EXPECT_EQ(GLenum{GL_NO_ERROR}, gm->GetError());
        for (GLint j = 0; j < info.length; ++j) {
          EXPECT_EQ(values[i][j], test4[i][j]);
        }
      }
    }
    // Set / get the entire uniform.
    ((*gm).*Setv)(info.loc, array_len, &v4[0][0]);
    EXPECT_EQ(GLenum{GL_NO_ERROR}, gm->GetError());
    ((*gm).*Getv)(pid, info.loc, &test4[0][0]);
    EXPECT_EQ(GLenum{GL_NO_ERROR}, gm->GetError());
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
        ((*gm).*Getv)(pid, info.alocs[i], &test4[i][0]);
        EXPECT_EQ(GLenum{GL_NO_ERROR}, gm->GetError());
        for (GLint j = 0; j < info.length; ++j) {
          EXPECT_EQ(*set_values++, test4[i][j]);
        }
      }
    }
  } else {
    ExpandArgsAndCall(gm, info.loc, Set, SequenceGenerator<sizeof...(Args)>{},
                      values[0]);
    EXPECT_EQ(GLenum{GL_INVALID_OPERATION}, gm->GetError());
    ((*gm).*Setv)(info.loc, array_len, &v4[0][0]);
    EXPECT_EQ(GLenum{GL_INVALID_OPERATION}, gm->GetError());
  }
}

TEST_F(FakeGraphicsManagerTest, GetProcAddress) {
  EXPECT_NE(nullptr, gl_context_->GetProcAddress("glGetError", 0));
  EXPECT_NE(nullptr, gl_context_->GetProcAddress("glDrawArrays", 0));
  EXPECT_EQ(nullptr, gl_context_->GetProcAddress("glNotAFunction", 0));
  EXPECT_EQ(nullptr, gl_context_->GetProcAddress("eglNotAFunction", 0));
}

TEST_F(FakeGraphicsManagerTest, Capabilities) {
  const GLenum nclips = static_cast<GLenum>(GetInt(GL_MAX_CLIP_DISTANCES));

  // By default, all capabilities are disabled except for GL_DITHER
  // and GL_MULTISAMPLE.
  EXPECT_FALSE(GetEnabled(GL_BLEND));
  for (GLenum i = GL_CLIP_DISTANCE0; i < GL_CLIP_DISTANCE0 + nclips; ++i)
    EXPECT_FALSE(GetEnabled(i));
  EXPECT_FALSE(GetEnabled(GL_CULL_FACE));
  EXPECT_FALSE(GetEnabled(GL_DEPTH_TEST));
  EXPECT_TRUE(GetEnabled(GL_DITHER));
  EXPECT_TRUE(GetEnabled(GL_MULTISAMPLE));
  EXPECT_FALSE(GetEnabled(GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_COVERAGE));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_SHADING));
  EXPECT_FALSE(GetEnabled(GL_SCISSOR_TEST));
  EXPECT_FALSE(GetEnabled(GL_STENCIL_TEST));
  EXPECT_FALSE(GetEnabled(GL_POINT_SPRITE));
  EXPECT_FALSE(GetEnabled(GL_PROGRAM_POINT_SIZE));

  GM_CALL(Enable(GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(GL_BLEND));
  for (GLenum i = GL_CLIP_DISTANCE0; i < GL_CLIP_DISTANCE0 + nclips; ++i)
    EXPECT_FALSE(GetEnabled(i));
  EXPECT_FALSE(GetEnabled(GL_CULL_FACE));
  EXPECT_FALSE(GetEnabled(GL_DEPTH_TEST));
  EXPECT_TRUE(GetEnabled(GL_DITHER));
  EXPECT_TRUE(GetEnabled(GL_MULTISAMPLE));
  EXPECT_TRUE(GetEnabled(GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_COVERAGE));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_SHADING));
  EXPECT_FALSE(GetEnabled(GL_SCISSOR_TEST));
  EXPECT_FALSE(GetEnabled(GL_STENCIL_TEST));
  EXPECT_FALSE(GetEnabled(GL_POINT_SPRITE));
  EXPECT_FALSE(GetEnabled(GL_PROGRAM_POINT_SIZE));

  GM_CALL(Disable(GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(GL_BLEND));
  for (GLenum i = GL_CLIP_DISTANCE0; i < GL_CLIP_DISTANCE0 + nclips; ++i)
    EXPECT_FALSE(GetEnabled(i));
  EXPECT_FALSE(GetEnabled(GL_CULL_FACE));
  EXPECT_FALSE(GetEnabled(GL_DEPTH_TEST));
  EXPECT_TRUE(GetEnabled(GL_DITHER));
  EXPECT_TRUE(GetEnabled(GL_MULTISAMPLE));
  EXPECT_FALSE(GetEnabled(GL_POLYGON_OFFSET_FILL));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_COVERAGE));
  EXPECT_FALSE(GetEnabled(GL_SAMPLE_SHADING));
  EXPECT_FALSE(GetEnabled(GL_SCISSOR_TEST));
  EXPECT_FALSE(GetEnabled(GL_STENCIL_TEST));
  EXPECT_FALSE(GetEnabled(GL_POINT_SPRITE));
  EXPECT_FALSE(GetEnabled(GL_PROGRAM_POINT_SIZE));

  GLenum caps[] = { GL_BLEND, GL_STENCIL_TEST, GL_POINT_SPRITE,
                    GL_PROGRAM_POINT_SIZE, GL_PROGRAM_POINT_SIZE };

  for (GLenum cap : caps) {
    GM_CALL(Enable(cap));
    EXPECT_TRUE(GetEnabled(cap));
    GM_CALL(Disable(cap));
    EXPECT_FALSE(GetEnabled(cap));
  }
}

TEST_F(FakeGraphicsManagerTest, VersionStandardRenderer) {
  // Check defaults.
  EXPECT_EQ("3.3 Ion OpenGL / ES", gm_->GetGlVersionString());
  EXPECT_EQ(33U, gm_->GetGlVersion());
  EXPECT_EQ(GraphicsManager::kEs, gm_->GetGlFlavor());

  gm_->SetVersionString("3.0 Ion OpenGL");
  EXPECT_EQ("3.0 Ion OpenGL", gm_->GetGlVersionString());
  EXPECT_EQ(30U, gm_->GetGlVersion());
  EXPECT_EQ(GraphicsManager::kDesktop, gm_->GetGlFlavor());

  gm_->SetVersionString("WebGL 1.2 Ion");
  EXPECT_EQ("WebGL 1.2 Ion", gm_->GetGlVersionString());
  EXPECT_EQ(12U, gm_->GetGlVersion());
  EXPECT_EQ(GraphicsManager::kWeb, gm_->GetGlFlavor());

  gm_->SetVersionString("2.0 Ion OpenGL ES");
  EXPECT_EQ("2.0 Ion OpenGL ES", gm_->GetGlVersionString());
  EXPECT_EQ(20U, gm_->GetGlVersion());
  EXPECT_EQ(GraphicsManager::kEs, gm_->GetGlFlavor());

  EXPECT_EQ("Ion fake OpenGL / ES", gm_->GetGlRenderer());
  gm_->SetRendererString("Renderer");
  EXPECT_EQ("Renderer", gm_->GetGlRenderer());
}

TEST_F(FakeGraphicsManagerTest, ProfileType) {
  // Non-desktop OpenGL platforms default to kCoreProfile.
  if (gm_->GetGlFlavor() != GraphicsManager::kDesktop) {
    EXPECT_EQ(GraphicsManager::kCoreProfile, gm_->GetGlProfileType());
  } else {
    // Desktop platforms default to kCompatibilityProfile, and use the value
    // of GL_CONTEXT_PROFILE_MASK to determine whether to use kCoreProfile.
    EXPECT_EQ(GraphicsManager::kCompatibilityProfile, gm_->GetGlProfileType());
    // Switch to kCoreProfile.
    gm_->SetContextProfileMask(GL_CONTEXT_CORE_PROFILE_BIT);
    EXPECT_EQ(GraphicsManager::kCoreProfile, gm_->GetGlProfileType());
    // Switch back to kCompatibilityProfile.
    gm_->SetContextProfileMask(GL_CONTEXT_COMPATIBILITY_PROFILE_BIT);
    EXPECT_EQ(GraphicsManager::kCompatibilityProfile, gm_->GetGlProfileType());
  }
}

TEST_F(FakeGraphicsManagerTest, ContextFlags) {
  GLint flags = 0;
  gm_->SetContextFlags(0x123);
  gm_->GetIntegerv(GL_CONTEXT_FLAGS, &flags);
  EXPECT_EQ(0x123, flags);
}

TEST_F(FakeGraphicsManagerTest, CallCount) {
  // This graphics manager relies upon the FakeGlContext set up by the first.
  FakeGraphicsManagerPtr gm2(new FakeGraphicsManager());

  // There is a non-zero number of calls at initialization time.
  const int64 init_calls = FakeGraphicsManager::GetCallCount();
  EXPECT_NE(0, init_calls);

  // GetEnabled calls IsEnabled and GetIntegerv once, and GetError twice, plus
  // the above calls.
  EXPECT_FALSE(GetEnabled(GL_BLEND));
  EXPECT_EQ(init_calls + 2, FakeGraphicsManager::GetCallCount());

  GLuint ids[2];
  GM_CALL(GenTextures(2, ids));
  EXPECT_EQ(init_calls + 3, FakeGraphicsManager::GetCallCount());

  EXPECT_FALSE(GetEnabled(GL_STENCIL_TEST));
  EXPECT_EQ(init_calls + 5, FakeGraphicsManager::GetCallCount());

  // Ensue that GetError calls are not counted.
  gm_->GetError();
  EXPECT_EQ(init_calls + 5, FakeGraphicsManager::GetCallCount());

  FakeGraphicsManager::ResetCallCount();
  EXPECT_EQ(0, FakeGraphicsManager::GetCallCount());
}

TEST_F(FakeGraphicsManagerTest, InitialState) {
  GLboolean b4[4];
  GLfloat f4[4];
  GLint i4[4];

  GM_CALL(GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, f4));
  EXPECT_EQ(1.f, f4[0]);
  EXPECT_EQ(256.f, f4[1]);
  GM_CALL(GetFloatv(GL_ALIASED_POINT_SIZE_RANGE, f4));
  EXPECT_EQ(1.f, f4[0]);
  EXPECT_EQ(8192.f, f4[1]);
  EXPECT_EQ(8, GetInt(GL_ALPHA_BITS));
  GM_CALL(GetFloatv(GL_BLEND_COLOR, f4));
  EXPECT_EQ(0.0f, f4[0]);
  EXPECT_EQ(0.0f, f4[1]);
  EXPECT_EQ(0.0f, f4[2]);
  EXPECT_EQ(0.0f, f4[3]);
  EXPECT_EQ(GL_FUNC_ADD, GetInt(GL_BLEND_EQUATION_ALPHA));
  EXPECT_EQ(GL_FUNC_ADD, GetInt(GL_BLEND_EQUATION_RGB));
  EXPECT_EQ(GL_ONE, GetInt(GL_BLEND_SRC_ALPHA));
  EXPECT_EQ(GL_ONE, GetInt(GL_BLEND_SRC_RGB));
  EXPECT_EQ(GL_ZERO, GetInt(GL_BLEND_DST_ALPHA));
  EXPECT_EQ(GL_ZERO, GetInt(GL_BLEND_DST_RGB));
  EXPECT_EQ(8, GetInt(GL_BLUE_BITS));
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

  // For querying compressed texture formats, verify that the expected number of
  // array items been populated, and that any remaining items are unchanged.
  GLint num_compressed = 0;
  GM_CALL(GetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &num_compressed));
  std::vector<GLint> formats(num_compressed + 1);
  GM_CALL(GetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, formats.data()));
  for (GLint s = 0; s < num_compressed; s++) {
    EXPECT_NE(0, formats[s]);
  }
  EXPECT_EQ(0, formats[num_compressed]);

  EXPECT_EQ(GL_BACK, GetInt(GL_CULL_FACE_MODE));
  EXPECT_EQ(16, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(1.0f, GetFloat(GL_DEPTH_CLEAR_VALUE));
  EXPECT_EQ(GL_LESS, GetInt(GL_DEPTH_FUNC));
  // Test type conversion with depth range.
  GM_CALL(GetBooleanv(GL_DEPTH_RANGE, b4));
  EXPECT_EQ(GL_FALSE, b4[0]);
  EXPECT_EQ(GL_TRUE, b4[1]);
  GM_CALL(GetFloatv(GL_DEPTH_RANGE, f4));
  EXPECT_EQ(0.0f, f4[0]);
  EXPECT_EQ(1.0f, f4[1]);
  GM_CALL(GetIntegerv(GL_DEPTH_RANGE, i4));
  EXPECT_EQ(0, i4[0]);
  EXPECT_EQ(1, i4[1]);
  // Conversions.
  EXPECT_EQ(GL_TRUE, GetBoolean(GL_DEPTH_WRITEMASK));
  EXPECT_EQ(1.f, GetFloat(GL_DEPTH_WRITEMASK));
  EXPECT_EQ(GL_BACK, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_BACK, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_CCW, GetInt(GL_FRONT_FACE));
  // Boolean type conversion.
  EXPECT_EQ(GL_TRUE, GetBoolean(GL_FRONT_FACE));
  EXPECT_EQ(GL_DONT_CARE, GetInt(GL_GENERATE_MIPMAP_HINT));
  EXPECT_EQ(8, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(GL_RGBA, GetInt(GL_IMPLEMENTATION_COLOR_READ_FORMAT));
  EXPECT_EQ(GL_UNSIGNED_BYTE, GetInt(GL_IMPLEMENTATION_COLOR_READ_TYPE));
  EXPECT_EQ(1.f, GetFloat(GL_LINE_WIDTH));
  EXPECT_EQ(96, GetInt(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(8192, GetInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE));
  EXPECT_EQ(8192, GetInt(GL_MAX_TEXTURE_SIZE));
  EXPECT_EQ(4, GetInt(GL_MAX_COLOR_ATTACHMENTS));
  EXPECT_EQ(4, GetInt(GL_MAX_DRAW_BUFFERS));
  // Test type conversion from int to float.
  EXPECT_EQ(4096.f, GetFloat(GL_MAX_3D_TEXTURE_SIZE));
  EXPECT_EQ(4096.f, GetFloat(GL_MAX_ARRAY_TEXTURE_LAYERS));
  EXPECT_EQ(96.f, GetFloat(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(16.f, GetFloat(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_EQ(8192.f, GetFloat(GL_MAX_CUBE_MAP_TEXTURE_SIZE));
  EXPECT_EQ(256, GetInt(GL_MAX_FRAGMENT_UNIFORM_VECTORS));
  EXPECT_EQ(4096, GetInt(GL_MAX_RENDERBUFFER_SIZE));
  EXPECT_EQ(16, GetInt(GL_MAX_SAMPLES));
  EXPECT_EQ(32, GetInt(GL_MAX_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(8192.f, GetFloat(GL_MAX_TEXTURE_SIZE));
  EXPECT_EQ(15, GetInt(GL_MAX_VARYING_VECTORS));
  EXPECT_EQ(32, GetInt(GL_MAX_VERTEX_ATTRIBS));
  EXPECT_EQ(32, GetInt(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(384, GetInt(GL_MAX_VERTEX_UNIFORM_VECTORS));
  GM_CALL(GetIntegerv(GL_MAX_VIEWPORT_DIMS, i4));
  EXPECT_EQ(8192, i4[0]);
  EXPECT_EQ(8192, i4[1]);
  EXPECT_EQ(4, GetInt(GL_MAX_VIEWS_OVR));
  EXPECT_EQ(10, GetInt(GL_NUM_COMPRESSED_TEXTURE_FORMATS));
  EXPECT_EQ(1, GetInt(GL_NUM_SHADER_BINARY_FORMATS));
  EXPECT_EQ(4, GetInt(GL_PACK_ALIGNMENT));
  EXPECT_EQ(1.f, GetFloat(GL_POINT_SIZE));
  EXPECT_EQ(0.0f, GetFloat(GL_POLYGON_OFFSET_FACTOR));
  EXPECT_EQ(0.0f, GetFloat(GL_POLYGON_OFFSET_UNITS));
  EXPECT_EQ(GL_BACK, GetInt(GL_READ_BUFFER));
  EXPECT_EQ(8, GetInt(GL_RED_BITS));
  EXPECT_EQ(1.0f, GetFloat(GL_SAMPLE_COVERAGE_VALUE));
  EXPECT_EQ(GL_FALSE, GetBoolean(GL_SAMPLE_COVERAGE_INVERT));
  EXPECT_EQ(1, GetInt(GL_SAMPLES));
  GM_CALL(GetIntegerv(GL_SCISSOR_BOX, i4));
  EXPECT_EQ(0, i4[0]);
  EXPECT_EQ(0, i4[1]);
  EXPECT_EQ(kWidth, i4[2]);
  EXPECT_EQ(kHeight, i4[3]);
  EXPECT_EQ(0xbadf00d, GetInt(GL_SHADER_BINARY_FORMATS));
  EXPECT_EQ(GL_FALSE, GetBoolean(GL_SHADER_COMPILER));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_ALWAYS, GetInt(GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_BACK_PASS_DEPTH_PASS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BACK_REF));
  // Boolean conversion.
  EXPECT_EQ(GL_FALSE, GetBoolean(GL_STENCIL_BACK_REF));
  EXPECT_EQ(0xffffffffU, GetMask(GL_STENCIL_BACK_VALUE_MASK));
  EXPECT_EQ(0xffffffffU, GetMask(GL_STENCIL_BACK_WRITEMASK));
  EXPECT_EQ(8, GetInt(GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_CLEAR_VALUE));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_FAIL));
  EXPECT_EQ(GL_ALWAYS, GetInt(GL_STENCIL_FUNC));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_REF));
  EXPECT_EQ(0xffffffffU, GetMask(GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(0xffffffffU, GetMask(GL_STENCIL_WRITEMASK));
  EXPECT_EQ(4, GetInt(GL_SUBPIXEL_BITS));
  EXPECT_EQ(4, GetInt(GL_UNPACK_ALIGNMENT));
  GM_CALL(GetIntegerv(GL_VIEWPORT, i4));
  EXPECT_EQ(0, i4[0]);
  EXPECT_EQ(0, i4[1]);
  EXPECT_EQ(kWidth, i4[2]);
  EXPECT_EQ(kHeight, i4[3]);

  // Error conditions of GetFloat and GetInt.
  GM_ERROR_CALL(GetIntegerv(GL_ARRAY_BUFFER, i4), GL_INVALID_ENUM);
  GM_ERROR_CALL(GetFloatv(GL_ARRAY_BUFFER, f4), GL_INVALID_ENUM);
  // Check error case of IsEnabled.
  GM_ERROR_CALL(IsEnabled(GL_PACK_ALIGNMENT), GL_INVALID_ENUM);
}

TEST_F(FakeGraphicsManagerTest, ChangeState) {
  // Check each supported call that modifies state.
  GLfloat f4[4];
  GLint i4[4];

  GM_CALL(BlendColor(.2f, .3f, -.4f, 1.5f));  // Should clamp.
  GM_CALL(GetFloatv(GL_BLEND_COLOR, f4));
  EXPECT_EQ(0.2f, f4[0]);
  EXPECT_EQ(0.3f, f4[1]);
  EXPECT_EQ(0.0f, f4[2]);
  EXPECT_EQ(1.0f, f4[3]);

  GM_CALL(BlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT));
  EXPECT_EQ(GL_FUNC_SUBTRACT, GetInt(GL_BLEND_EQUATION_RGB));
  EXPECT_EQ(GL_FUNC_REVERSE_SUBTRACT, GetInt(GL_BLEND_EQUATION_ALPHA));
  GM_CALL(BlendEquation(GL_FUNC_ADD));
  EXPECT_EQ(GL_FUNC_ADD, GetInt(GL_BLEND_EQUATION_RGB));
  EXPECT_EQ(GL_FUNC_ADD, GetInt(GL_BLEND_EQUATION_ALPHA));

  GM_CALL(BlendFuncSeparate(GL_ONE_MINUS_CONSTANT_COLOR, GL_DST_COLOR,
                            GL_ONE_MINUS_CONSTANT_ALPHA, GL_DST_ALPHA));
  EXPECT_EQ(GL_ONE_MINUS_CONSTANT_COLOR, GetInt(GL_BLEND_SRC_RGB));
  EXPECT_EQ(GL_DST_COLOR, GetInt(GL_BLEND_DST_RGB));
  EXPECT_EQ(GL_ONE_MINUS_CONSTANT_ALPHA, GetInt(GL_BLEND_SRC_ALPHA));
  EXPECT_EQ(GL_DST_ALPHA, GetInt(GL_BLEND_DST_ALPHA));
  GM_CALL(BlendFunc(GL_CONSTANT_COLOR, GL_SRC_ALPHA));
  EXPECT_EQ(GL_CONSTANT_COLOR, GetInt(GL_BLEND_SRC_RGB));
  EXPECT_EQ(GL_SRC_ALPHA, GetInt(GL_BLEND_DST_RGB));
  EXPECT_EQ(GL_CONSTANT_COLOR, GetInt(GL_BLEND_SRC_ALPHA));
  EXPECT_EQ(GL_SRC_ALPHA, GetInt(GL_BLEND_DST_ALPHA));

  GM_CALL(ClearColor(.2f, .3f, 1.4f, -.5f));  // Should clamp.
  GM_CALL(GetFloatv(GL_COLOR_CLEAR_VALUE, f4));
  EXPECT_EQ(0.2f, f4[0]);
  EXPECT_EQ(0.3f, f4[1]);
  EXPECT_EQ(1.0f, f4[2]);
  EXPECT_EQ(0.0f, f4[3]);

  GM_CALL(ClearDepthf(0.5f));
  EXPECT_EQ(0.5f, GetFloat(GL_DEPTH_CLEAR_VALUE));
  GM_CALL(ClearDepthf(1.5f));  // Should clamp.
  EXPECT_EQ(1.0f, GetFloat(GL_DEPTH_CLEAR_VALUE));

  GM_CALL(ColorMask(true, false, false, true));
  GM_CALL(GetIntegerv(GL_COLOR_WRITEMASK, i4));
  EXPECT_EQ(GL_TRUE, i4[0]);
  EXPECT_EQ(GL_FALSE, i4[1]);
  EXPECT_EQ(GL_FALSE, i4[2]);
  EXPECT_EQ(GL_TRUE, i4[3]);

  GM_CALL(CullFace(GL_FRONT_AND_BACK));
  EXPECT_EQ(GL_FRONT_AND_BACK, GetInt(GL_CULL_FACE_MODE));

  GM_CALL(DepthFunc(GL_GEQUAL));
  EXPECT_EQ(GL_GEQUAL, GetInt(GL_DEPTH_FUNC));

  GM_CALL(DepthRangef(0.2f, 0.7f));
  GM_CALL(GetFloatv(GL_DEPTH_RANGE, f4));
  EXPECT_EQ(0.2f, f4[0]);
  EXPECT_EQ(0.7f, f4[1]);
  GM_CALL(DepthRangef(-0.1f, 1.1f));  // Should clamp.
  GM_CALL(GetFloatv(GL_DEPTH_RANGE, f4));
  EXPECT_EQ(0.0f, f4[0]);
  EXPECT_EQ(1.0f, f4[1]);

  GM_CALL(DepthMask(false));
  EXPECT_EQ(GL_FALSE, GetInt(GL_DEPTH_WRITEMASK));

  GM_CALL(DrawBuffer(GL_FRONT));
  EXPECT_EQ(GL_FRONT, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_FRONT, GetInt(GL_DRAW_BUFFER0));

  GM_CALL(FrontFace(GL_CW));
  EXPECT_EQ(GL_CW, GetInt(GL_FRONT_FACE));

  {
    // Hints are not available on all platforms; ignore error messages.
    base::LogChecker log_checker;
    GM_ERROR_CALL(Hint(GL_ARRAY_BUFFER, GL_FASTEST), GL_INVALID_ENUM);
    GM_ERROR_CALL(Hint(GL_GENERATE_MIPMAP_HINT, GL_BLEND), GL_INVALID_ENUM);
    GM_CALL(Hint(GL_GENERATE_MIPMAP_HINT, GL_NICEST));
    EXPECT_EQ(GL_NICEST, GetInt(GL_GENERATE_MIPMAP_HINT));
    log_checker.ClearLog();
  }

  GM_CALL(PixelStorei(GL_PACK_ALIGNMENT, 2));
  EXPECT_EQ(2, GetInt(GL_PACK_ALIGNMENT));
  EXPECT_EQ(4, GetInt(GL_UNPACK_ALIGNMENT));
  GM_CALL(PixelStorei(GL_UNPACK_ALIGNMENT, 8));
  EXPECT_EQ(2, GetInt(GL_PACK_ALIGNMENT));
  EXPECT_EQ(8, GetInt(GL_UNPACK_ALIGNMENT));

  GM_CALL(LineWidth(2.18f));
  EXPECT_EQ(2.18f, GetFloat(GL_LINE_WIDTH));

  GM_CALL(MinSampleShading(0.7f));
  EXPECT_EQ(0.7f, GetFloat(GL_MIN_SAMPLE_SHADING_VALUE));
  GM_CALL(MinSampleShading(-2.5f));
  EXPECT_EQ(0.0f, GetFloat(GL_MIN_SAMPLE_SHADING_VALUE));
  GM_CALL(MinSampleShading(2.5f));
  EXPECT_EQ(1.0f, GetFloat(GL_MIN_SAMPLE_SHADING_VALUE));

  GM_CALL(PointSize(3.14f));
  EXPECT_EQ(3.14f, GetFloat(GL_POINT_SIZE));

  GM_CALL(PolygonOffset(0.4f, 0.2f));
  EXPECT_EQ(0.4f, GetFloat(GL_POLYGON_OFFSET_FACTOR));
  EXPECT_EQ(0.2f, GetFloat(GL_POLYGON_OFFSET_UNITS));

  GM_CALL(ReadBuffer(GL_FRONT));
  EXPECT_EQ(GL_FRONT, GetInt(GL_READ_BUFFER));

  GM_CALL(SampleCoverage(0.5f, true));
  EXPECT_EQ(0.5f, GetFloat(GL_SAMPLE_COVERAGE_VALUE));
  EXPECT_EQ(GL_TRUE, GetInt(GL_SAMPLE_COVERAGE_INVERT));
  GM_CALL(SampleCoverage(1.2f, false));  // Should clamp.
  EXPECT_EQ(1.0f, GetFloat(GL_SAMPLE_COVERAGE_VALUE));
  EXPECT_EQ(GL_FALSE, GetInt(GL_SAMPLE_COVERAGE_INVERT));

  GM_CALL(Scissor(4, 10, 123, 234));
  GM_CALL(GetIntegerv(GL_SCISSOR_BOX, i4));
  EXPECT_EQ(4, i4[0]);
  EXPECT_EQ(10, i4[1]);
  EXPECT_EQ(123, i4[2]);
  EXPECT_EQ(234, i4[3]);

  GM_CALL(StencilFuncSeparate(GL_FRONT, GL_LEQUAL, 100, 0xbeefbeefU));
  EXPECT_EQ(GL_LEQUAL, GetInt(GL_STENCIL_FUNC));
  EXPECT_EQ(100, GetInt(GL_STENCIL_REF));
  EXPECT_EQ(0xbeefbeefU, GetMask(GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(GL_ALWAYS, GetInt(GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BACK_REF));
  EXPECT_EQ(0xffffffffU, GetMask(GL_STENCIL_BACK_VALUE_MASK));
  GM_CALL(StencilFuncSeparate(GL_BACK, GL_GREATER, 200, 0xfacefaceU));
  EXPECT_EQ(GL_LEQUAL, GetInt(GL_STENCIL_FUNC));
  EXPECT_EQ(100, GetInt(GL_STENCIL_REF));
  EXPECT_EQ(0xbeefbeefU, GetMask(GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(GL_GREATER, GetInt(GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(200, GetInt(GL_STENCIL_BACK_REF));
  EXPECT_EQ(0xfacefaceU, GetMask(GL_STENCIL_BACK_VALUE_MASK));
  GM_CALL(
      StencilFuncSeparate(GL_FRONT_AND_BACK, GL_NOTEQUAL, 300, 0xbebebebeU));
  EXPECT_EQ(GL_NOTEQUAL, GetInt(GL_STENCIL_FUNC));
  EXPECT_EQ(300, GetInt(GL_STENCIL_REF));
  EXPECT_EQ(0xbebebebeU, GetMask(GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(GL_NOTEQUAL, GetInt(GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(300, GetInt(GL_STENCIL_BACK_REF));
  EXPECT_EQ(0xbebebebeU, GetMask(GL_STENCIL_BACK_VALUE_MASK));
  GM_CALL(StencilFunc(GL_LESS, 400, 0x20304050U));
  EXPECT_EQ(GL_LESS, GetInt(GL_STENCIL_FUNC));
  EXPECT_EQ(400, GetInt(GL_STENCIL_REF));
  EXPECT_EQ(0x20304050U, GetMask(GL_STENCIL_VALUE_MASK));
  EXPECT_EQ(GL_LESS, GetInt(GL_STENCIL_BACK_FUNC));
  EXPECT_EQ(400, GetInt(GL_STENCIL_BACK_REF));
  EXPECT_EQ(0x20304050U, GetMask(GL_STENCIL_BACK_VALUE_MASK));

  GM_CALL(StencilMaskSeparate(GL_FRONT, 0xdeadfaceU));
  EXPECT_EQ(0xdeadfaceU, GetMask(GL_STENCIL_WRITEMASK));
  EXPECT_EQ(0xffffffffU, GetMask(GL_STENCIL_BACK_WRITEMASK));
  GM_CALL(StencilMaskSeparate(GL_BACK, 0xcacabeadU));
  EXPECT_EQ(0xdeadfaceU, GetMask(GL_STENCIL_WRITEMASK));
  EXPECT_EQ(0xcacabeadU, GetMask(GL_STENCIL_BACK_WRITEMASK));
  GM_CALL(StencilMaskSeparate(GL_FRONT_AND_BACK, 0x87654321U));
  EXPECT_EQ(0x87654321U, GetMask(GL_STENCIL_WRITEMASK));
  EXPECT_EQ(0x87654321U, GetMask(GL_STENCIL_BACK_WRITEMASK));
  GM_CALL(StencilMask(0x24681359U));
  EXPECT_EQ(0x24681359U, GetMask(GL_STENCIL_WRITEMASK));
  EXPECT_EQ(0x24681359U, GetMask(GL_STENCIL_BACK_WRITEMASK));

  GM_CALL(StencilOpSeparate(GL_FRONT, GL_REPLACE, GL_INCR, GL_INVERT));
  EXPECT_EQ(GL_REPLACE, GetInt(GL_STENCIL_FAIL));
  EXPECT_EQ(GL_INCR, GetInt(GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_INVERT, GetInt(GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_BACK_PASS_DEPTH_PASS));
  GM_CALL(StencilOpSeparate(GL_BACK, GL_INCR_WRAP, GL_DECR_WRAP, GL_ZERO));
  EXPECT_EQ(GL_REPLACE, GetInt(GL_STENCIL_FAIL));
  EXPECT_EQ(GL_INCR, GetInt(GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_INVERT, GetInt(GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(GL_INCR_WRAP, GetInt(GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_DECR_WRAP, GetInt(GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_ZERO, GetInt(GL_STENCIL_BACK_PASS_DEPTH_PASS));
  GM_CALL(StencilOpSeparate(GL_FRONT_AND_BACK, GL_ZERO, GL_KEEP, GL_DECR));
  EXPECT_EQ(GL_ZERO, GetInt(GL_STENCIL_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_DECR, GetInt(GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(GL_ZERO, GetInt(GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_KEEP, GetInt(GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_DECR, GetInt(GL_STENCIL_BACK_PASS_DEPTH_PASS));
  GM_CALL(StencilOp(GL_INCR, GL_DECR, GL_INVERT));
  EXPECT_EQ(GL_INCR, GetInt(GL_STENCIL_FAIL));
  EXPECT_EQ(GL_DECR, GetInt(GL_STENCIL_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_INVERT, GetInt(GL_STENCIL_PASS_DEPTH_PASS));
  EXPECT_EQ(GL_INCR, GetInt(GL_STENCIL_BACK_FAIL));
  EXPECT_EQ(GL_DECR, GetInt(GL_STENCIL_BACK_PASS_DEPTH_FAIL));
  EXPECT_EQ(GL_INVERT, GetInt(GL_STENCIL_BACK_PASS_DEPTH_PASS));

  GM_CALL(ClearStencil(123));
  EXPECT_EQ(123, GetInt(GL_STENCIL_CLEAR_VALUE));

  GM_CALL(Viewport(16, 49, 220, 317));
  GM_CALL(GetIntegerv(GL_VIEWPORT, i4));
  EXPECT_EQ(16, i4[0]);
  EXPECT_EQ(49, i4[1]);
  EXPECT_EQ(220, i4[2]);
  EXPECT_EQ(317, i4[3]);
}

TEST_F(FakeGraphicsManagerTest, BindTexture_ActiveTexture) {
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

  EXPECT_EQ(GL_FALSE, gm_->IsTexture(0U));
  EXPECT_EQ(GL_FALSE, gm_->IsTexture(ids[3]));
  EXPECT_EQ(GL_FALSE, gm_->IsTexture(ids[4]));
  EXPECT_EQ(GL_FALSE, gm_->IsTexture(ids[3] + ids[4]));

  GLuint max_units = static_cast<GLuint>(
      GetInt(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
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
  EXPECT_EQ(GL_TEXTURE0, GetInt(GL_ACTIVE_TEXTURE));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  EXPECT_EQ(GL_TEXTURE4, GetInt(GL_ACTIVE_TEXTURE));

  // Bad binds.
  GM_ERROR_CALL(BindTexture(GL_BACK, ids[0]), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindTexture(GL_TEXTURE_2D, 24U), GL_INVALID_VALUE);
  // Good binds.
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_2D));
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[0]));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(GL_TEXTURE_BINDING_2D));

  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, ids[1]));
  EXPECT_EQ(static_cast<int>(ids[1]), GetInt(GL_TEXTURE_BINDING_CUBE_MAP));

  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_1D_ARRAY));
  GM_CALL(BindTexture(GL_TEXTURE_1D_ARRAY, ids[2]));
  EXPECT_EQ(static_cast<int>(ids[2]), GetInt(GL_TEXTURE_BINDING_1D_ARRAY));

  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_2D_ARRAY));
  GM_CALL(BindTexture(GL_TEXTURE_2D_ARRAY, ids[3]));
  EXPECT_EQ(static_cast<int>(ids[3]), GetInt(GL_TEXTURE_BINDING_2D_ARRAY));

  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, ids[4]));
  EXPECT_EQ(static_cast<int>(ids[4]),
            GetInt(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY));

  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_EXTERNAL_OES));
  GM_CALL(BindTexture(GL_TEXTURE_EXTERNAL_OES, ids[3]));
  EXPECT_EQ(static_cast<int>(ids[3]),
            GetInt(GL_TEXTURE_BINDING_EXTERNAL_OES));

  // Check that the texture binding is correct and follows the active image
  // unit.
  GM_CALL(ActiveTexture(GL_TEXTURE2));
  // Unit 2 is empty.
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  // Bind textures to unit 4.
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(ids[1]), GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  GLuint more_ids[2];
  more_ids[0] = more_ids[1] = 0U;
  GM_CALL(GenTextures(2, more_ids));
  // Bind textures to unit 5.
  GM_CALL(ActiveTexture(GL_TEXTURE5));
  GM_CALL(BindTexture(GL_TEXTURE_2D, more_ids[0]));
  EXPECT_EQ(static_cast<int>(more_ids[0]), GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, more_ids[1]));
  EXPECT_EQ(static_cast<int>(more_ids[0]), GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(more_ids[1]),
            GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  // Unit 2 should still be empty.
  GM_CALL(ActiveTexture(GL_TEXTURE2));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  // Unit 3 should be empty.
  GM_CALL(ActiveTexture(GL_TEXTURE3));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  // Units 4 and 5 should have the right bindings.
  GM_CALL(ActiveTexture(GL_TEXTURE5));
  EXPECT_EQ(static_cast<int>(more_ids[0]), GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(more_ids[1]),
            GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(ActiveTexture(GL_TEXTURE4));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(ids[1]), GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  // Deleting the new textures should clear their binding.
  GM_CALL(DeleteTextures(2, more_ids));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(static_cast<int>(ids[1]), GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(ActiveTexture(GL_TEXTURE5));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  GM_CALL(ActiveTexture(GL_TEXTURE4));

  // Delete textures.
  GLuint bad_id = 5U;
  // Error if n < 0.
  GM_ERROR_CALL(DeleteTextures(-1, ids), GL_INVALID_VALUE);
  // Bad ids are silently ignored.
  GM_CALL(DeleteTextures(1, &bad_id));
  // Actually delete the ids.
  GM_CALL(DeleteTextures(2, ids));
  EXPECT_FALSE(gm_->IsTexture(ids[0]));
  EXPECT_FALSE(gm_->IsTexture(ids[1]));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  // Bind should fail on a deleted texture.
  GM_ERROR_CALL(BindTexture(GL_TEXTURE_2D, ids[0]), GL_INVALID_VALUE);
}

TEST_F(FakeGraphicsManagerTest, TexParameter) {
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
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R));
  EXPECT_EQ(GL_REPEAT, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_FALSE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_PROTECTED_EXT));
  // Error if an invalid enum is used.
  GM_ERROR_CALL(
      GetTexParameteriv(GL_TEXTURE_2D, GL_VERTEX_ATTRIB_ARRAY_SIZE, nullptr),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetTexParameterfv(GL_TEXTURE_2D, GL_VERTEX_ATTRIB_ARRAY_SIZE, nullptr),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PROTECTED_EXT,
                              GL_LINEAR),
                GL_INVALID_VALUE);

  // Check that changes happen.
  GLint mode = GL_CLAMP_TO_EDGE;
  GM_CALL(TexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &mode));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  GLfloat modef = GL_MIRRORED_REPEAT;
  GM_CALL(TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &modef));
  EXPECT_EQ(static_cast<GLfloat>(GL_MIRRORED_REPEAT),
            GetTextureFloat(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 2));
  EXPECT_EQ(2, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 200));
  EXPECT_EQ(200, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL));
  GM_CALL(TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, 3.14f));
  EXPECT_EQ(3.14f, GetTextureFloat(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD));
  GM_CALL(TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 2.18f));
  EXPECT_EQ(2.18f, GetTextureFloat(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS));
  EXPECT_EQ(GL_ALWAYS,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                        GL_COMPARE_REF_TO_TEXTURE));
  EXPECT_EQ(GL_COMPARE_REF_TO_TEXTURE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE));
  GM_CALL(TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 3.f));
  EXPECT_EQ(3.f,
            GetTextureFloat(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT));
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PROTECTED_EXT, GL_TRUE));
  EXPECT_EQ(GL_TRUE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_PROTECTED_EXT));

  // Check that changes affect only the proper parameter.
  EXPECT_EQ(GL_REPEAT, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  // Check that cube map settings have not changed.
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR,
            GetTextureInt(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT,
            GetTextureInt(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT,
            GetTextureInt(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T));

  // Check that texture state is saved over a bind.
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[1]));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetTextureInt(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR,
            GetTextureInt(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT,
            GetTextureInt(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT,
            GetTextureInt(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T));

  // Check that original values are restored.
  GM_CALL(BindTexture(GL_TEXTURE_2D, ids[0]));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_NEAREST,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER));

  // Delete textures.
  GLuint bad_id = 5U;
  // Error if n < 0.
  GM_ERROR_CALL(DeleteTextures(-1, ids), GL_INVALID_VALUE);
  // Bad ids are silently ignored.
  GM_CALL(DeleteTextures(1, &bad_id));
  // Actually delete the ids.
  GM_CALL(DeleteTextures(2, ids));
  EXPECT_FALSE(gm_->IsTexture(ids[0]));
  EXPECT_FALSE(gm_->IsTexture(ids[1]));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_2D));
  EXPECT_EQ(0, GetInt(GL_TEXTURE_BINDING_CUBE_MAP));
  // Bind should fail on a deleted texture.
  GM_ERROR_CALL(BindTexture(GL_TEXTURE_2D, ids[0]), GL_INVALID_VALUE);
}

TEST_F(FakeGraphicsManagerTest, TexImage2D_GenerateMipmap) {
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
                           border, format, type, nullptr),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, -1, internal_format, width, height,
                           border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, 0, GL_TEXTURE_MIN_FILTER, width,
                           height, border, format, type, nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, -1, height,
                           border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, -1,
                           border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, height,
                           2, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, border,
                           GL_RGBA, type, nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, height,
                           border, format, GL_INCR, nullptr),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, GL_RGBA, width, height, border,
                           GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, border,
                           GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, nullptr),
                GL_INVALID_OPERATION);
  // Large textures should fail.
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, 65537, height,
                           border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, 65537,
                           border, format, type, nullptr),
                GL_INVALID_VALUE);
  // Cube map requires an axis enum.
  GM_ERROR_CALL(TexImage2D(GL_TEXTURE_CUBE_MAP, level, internal_format, width,
                           128, border, format, type, nullptr),
                GL_INVALID_ENUM);
  // Dimensions must be equal for cube maps.
  GM_ERROR_CALL(
      TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, level, internal_format, width,
                 256, border, format, type, nullptr),
      GL_INVALID_VALUE);

  // Successful calls.
  GM_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, width, height,
                     border, format, type, nullptr));
  GM_CALL(TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format,
                     width, height, border, format, type, nullptr));

  // Mipmaps.
  // Bad target.
  GM_ERROR_CALL(GenerateMipmap(GL_VERTEX_SHADER), GL_INVALID_ENUM);
  GM_CALL(GenerateMipmap(GL_TEXTURE_2D));
  // Dimensions must be powers of two to generate mipmaps.
  GM_CALL(TexImage2D(GL_TEXTURE_2D, level, internal_format, 100, 100, border,
                     format, type, nullptr));
  GM_ERROR_CALL(GenerateMipmap(GL_TEXTURE_2D), GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, TexImage3D) {
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
                           depth, border, format, type, nullptr),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, -1, internal_format, width, height,
                           depth, border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, 0, GL_TEXTURE_MIN_FILTER, width,
                           height, depth, border, format, type, nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, -1, height,
                           depth, border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, -1,
                           depth, border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           -1, border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           depth, 2, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, GL_RGB, width, height, depth,
                           border, GL_RGBA, type, nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           depth, border, format, GL_INCR, nullptr),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, GL_RGBA, width, height, depth,
                           border, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, GL_RGB, width, height, depth,
                           border, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, nullptr),
                GL_INVALID_OPERATION);
  // Large textures should fail.
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, 65537, height,
                           depth, border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, 65537,
                           depth, border, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           65537, border, format, type, nullptr),
                GL_INVALID_VALUE);
  // Dimensions must be equal for cube map arrays.
  GM_ERROR_CALL(
      TexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format, width,
                 height / 2, depth, border, format, type, nullptr),
      GL_INVALID_VALUE);

  // Successful calls.
  GM_CALL(TexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                     depth, border, format, type, nullptr));
  // The number of cubemap layers doesn't have to be the same as the dimensions.
  GM_CALL(TexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                     width, height, width / 2, border, format, type, nullptr));
}

TEST_F(FakeGraphicsManagerTest,
       TexSubImage2D_CopyTexImage2D_CopyTexSubImage2D) {
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
                     border, format, type, nullptr));
  GM_CALL(TexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format,
                     width, height, border, format, type, nullptr));

  const GLint xoffset = 64;
  const GLint yoffset = 64;
  const GLint x = 64;
  const GLint y = 64;
  width = height = 63;
  // TexSubImage2D.
  GM_ERROR_CALL(TexSubImage2D(GL_DEPTH_TEST, level, xoffset, yoffset, width,
                              height, format, type, nullptr),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, -1, xoffset, yoffset, width,
                              height, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, -1, yoffset, width, height,
                              format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, -1, width, height,
                              format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, -1,
                              height, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, -1,
                              format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                              height, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5,
                              nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                              height, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4,
                              nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, 1024, yoffset, width,
                              height, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, 1024, width,
                              height, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, 1024,
                              height, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                              1024, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                              height, GL_RGB, type, nullptr),
                GL_INVALID_OPERATION);
  GM_CALL(TexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width, height,
                        format, type, nullptr));

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

TEST_F(FakeGraphicsManagerTest, TexSubImage3D_CopyTexSubImage3D) {
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
                     depth, border, format, type, nullptr));
  GM_ERROR_CALL(
      TexImage3D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, level, internal_format, width,
                 height, depth, border, format, type, nullptr),
      GL_INVALID_ENUM);
  GM_CALL(TexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                     width, height, depth, border, format, type, nullptr));

  const GLint xoffset = 64;
  const GLint yoffset = 64;
  const GLint zoffset = 64;
  const GLint x = 64;
  const GLint y = 64;
  width = height = depth = 63;
  // TexSubImage3D.
  // Invalid target.
  GM_ERROR_CALL(TexSubImage3D(GL_DEPTH_TEST, level, xoffset, yoffset, zoffset,
                              width, height, depth, format, type, nullptr),
                GL_INVALID_ENUM);
  // Invalid level.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, -1, xoffset, yoffset, zoffset,
                              width, height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  // Invalid offsets.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, -1, yoffset, zoffset, width,
                              height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, -1, zoffset, width,
                              height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, -1, width,
                              height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  // Invalid dimensions.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              -1, height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, -1, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, -1, format, type, nullptr),
                GL_INVALID_VALUE);
  // Invalid formats.
  GM_ERROR_CALL(
      TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width,
                    height, depth, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, nullptr),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width,
                    height, depth, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, nullptr),
      GL_INVALID_OPERATION);
  // Invalid offsets.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, 1024, yoffset, zoffset,
                              width, height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, 1024, zoffset,
                              width, height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, 1024,
                              width, height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  // Invalid dimensions.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              1024, height, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, 1024, depth, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, 1024, format, type, nullptr),
                GL_INVALID_VALUE);
  // Invalid format.
  GM_ERROR_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, depth, GL_RGB, type, nullptr),
                GL_INVALID_OPERATION);
  GM_CALL(TexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset, width,
                        height, depth, format, type, nullptr));

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

TEST_F(FakeGraphicsManagerTest, CompressedTexImage2D_CompressedTexSubImage2D) {
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
                                     height, border, image_size, nullptr),
                GL_INVALID_ENUM);
  // Invalid level.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, -1, internal_format, width,
                                     height, border, image_size, nullptr),
                GL_INVALID_VALUE);
  // Invalid format.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, 0, GL_TEXTURE_MIN_FILTER,
                                     width, height, border, image_size,
                                     nullptr),
                GL_INVALID_ENUM);
  // Invalid dimensions.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format, -1,
                                     height, border, image_size, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format,
                                     width, -1, border, image_size, nullptr),
                GL_INVALID_VALUE);
  // Invalid size.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format,
                                     width, height, border, -1, nullptr),
                GL_INVALID_VALUE);
  // Large textures.
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format,
                                     65537, height, border, image_size,
                                     nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format,
                                     width, 65537, border, image_size, nullptr),
                GL_INVALID_VALUE);
  // Cube map requires an axis enum.
  GM_ERROR_CALL(
      CompressedTexImage2D(GL_TEXTURE_CUBE_MAP, level, internal_format, width,
                           128, border, image_size, nullptr),
      GL_INVALID_ENUM);
  // Successful calls.
  GM_CALL(CompressedTexImage2D(GL_TEXTURE_2D, level, internal_format, width,
                               height, border, image_size, nullptr));
  GM_CALL(CompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level,
                               internal_format, width, height, border,
                               image_size, nullptr));

  // CompressedTexSubImage2D.
  format = GL_ETC1_RGB8_OES;
  width = height = 16;
  xoffset = yoffset = 16;
  GM_ERROR_CALL(
      CompressedTexSubImage2D(GL_INVALID_ENUM, level, xoffset, yoffset, width,
                              height, format, image_size, nullptr),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      CompressedTexSubImage2D(GL_TEXTURE_2D, -1, xoffset, yoffset, width,
                              height, format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage2D(GL_TEXTURE_2D, level, -1, yoffset, width, height,
                              format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, -1, width, height,
                              format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset,
                                        -1, height, format, image_size,
                                        nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset,
                                        width, -1, format, image_size, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset,
                                        width, height, GL_RGBA, -1, nullptr),
                GL_INVALID_ENUM);
  GM_CALL(CompressedTexSubImage2D(GL_TEXTURE_2D, level, xoffset, yoffset, width,
                                  height, format, image_size, nullptr));
}

TEST_F(FakeGraphicsManagerTest, CompressedTexImage3D_CompressedTexSubImage3D) {
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
                                     height, depth, border, image_size,
                                     nullptr),
                GL_INVALID_ENUM);
  // Invalid level.
  GM_ERROR_CALL(CompressedTexImage3D(GL_TEXTURE_3D, -1, internal_format, width,
                                     height, depth, border, image_size,
                                     nullptr),
                GL_INVALID_VALUE);
  // Invalid format.
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, 0, GL_TEXTURE_MIN_FILTER, width,
                           height, depth, border, image_size, nullptr),
      GL_INVALID_ENUM);
  // Invalid dimensions.
  GM_ERROR_CALL(CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, -1,
                                     height, depth, border, image_size,
                                     nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, -1,
                           depth, border, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           -1, border, image_size, nullptr),
      GL_INVALID_VALUE);
  // Invalid border.
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           depth, 1, image_size, nullptr),
      GL_INVALID_VALUE);
  // Invalid size.
  GM_ERROR_CALL(CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format,
                                     width, height, depth, border, -1, nullptr),
                GL_INVALID_VALUE);
  // Large textures.
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, 65537, height,
                           depth, border, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, 65537,
                           depth, border, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width, height,
                           65537, border, image_size, nullptr),
      GL_INVALID_VALUE);
  // Cube map dimensions must be equal.
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                           width, width / 2, depth, border, image_size,
                           nullptr),
      GL_INVALID_VALUE);
  // It's ok to have a different depth for an array, however.
  GM_CALL(
      CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                           width, height, width / 2, border, image_size,
                           nullptr));
  // Other calls.
  GM_CALL(CompressedTexImage3D(GL_TEXTURE_3D, level, internal_format, width,
                               height, depth, border, image_size, nullptr));
  GM_ERROR_CALL(CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, level,
                                     internal_format, width, height, depth,
                                     border, image_size, nullptr),
                GL_INVALID_ENUM);
  GM_CALL(
      CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, level, internal_format,
                           width, height, depth, border, image_size, nullptr));

  // CompressedTexSubImage3D.
  format = GL_ETC1_RGB8_OES;
  width = height = depth = 16;
  xoffset = yoffset = zoffset = 16;
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_INVALID_ENUM, level, xoffset, yoffset, zoffset,
                              width, height, depth, format, image_size,
                              nullptr),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, -1, xoffset, yoffset, zoffset,
                              width, height, depth, format, image_size,
                              nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, -1, yoffset, zoffset, width,
                              height, depth, format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, -1, zoffset, width,
                              height, depth, format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, -1, width,
                              height, depth, format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              -1, height, depth, format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, -1, depth, format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, -1, format, image_size, nullptr),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, depth, GL_RGBA, -1, nullptr),
      GL_INVALID_ENUM);
  GM_CALL(
      CompressedTexSubImage3D(GL_TEXTURE_3D, level, xoffset, yoffset, zoffset,
                              width, height, depth, format, image_size,
                              nullptr));
}

TEST_F(FakeGraphicsManagerTest, TexImage2DMultisample) {
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

TEST_F(FakeGraphicsManagerTest, TexImage3DMultisample) {
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

TEST_F(FakeGraphicsManagerTest, GetMultisamplefv) {
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

TEST_F(FakeGraphicsManagerTest, SampleMaski) {
  GLuint index = 3;
  GLbitfield mask = 19;

  GLint maxSampleMaskWords = GetInt(GL_MAX_SAMPLE_MASK_WORDS);
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

TEST_F(FakeGraphicsManagerTest, TexStorage2D) {
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
                             gm_->GetMaxTextureSize() + 1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_1D_ARRAY, levels, internal_format,
                             width, gm_->GetMaxArrayTextureLayers() + 1),
                GL_INVALID_VALUE);
  // Large values for 2D.
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_2D, levels, internal_format,
                             gm_->GetMaxTextureSize() + 1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_2D, levels, internal_format,
                             width, gm_->GetMaxTextureSize() + 1),
                GL_INVALID_VALUE);
  // Large values for cubemap.
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format,
                             gm_->GetMaxCubeMapTextureSize() + 1, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format,
                             width, gm_->GetMaxCubeMapTextureSize() + 1),
                GL_INVALID_VALUE);
  // Cubemap dims not equal.
  GM_ERROR_CALL(TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format,
                             width, height + 1),
                GL_INVALID_VALUE);

  // Verify textures are mutable.
  EXPECT_EQ(GL_FALSE, GetTextureInt(GL_TEXTURE_1D_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE, GetTextureInt(GL_TEXTURE_CUBE_MAP,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));

  // Valid calls for each 2D type.
  GM_CALL(TexStorage2D(GL_TEXTURE_1D_ARRAY, levels, internal_format, width,
                       height));
  GM_CALL(TexStorage2D(GL_TEXTURE_2D, levels, internal_format, width, height));
  GM_CALL(TexStorage2D(GL_TEXTURE_CUBE_MAP, levels, internal_format, width,
                       height));

  // These textures should now be immutable.
  EXPECT_EQ(GL_TRUE, GetTextureInt(GL_TEXTURE_1D_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_TRUE,
            GetTextureInt(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_TRUE, GetTextureInt(GL_TEXTURE_CUBE_MAP,
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
  GM_ERROR_CALL(
      TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      CompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_ETC1_RGB8_OES,
                           width, height, 0, 1024, nullptr),
      GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, TexStorage3D) {
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
                             gm_->GetMaxTextureSize() + 1, height, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internal_format,
                             width, gm_->GetMaxTextureSize() + 1, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_2D_ARRAY, levels, internal_format,
                             width, height,
                             gm_->GetMaxArrayTextureLayers() + 1),
                GL_INVALID_VALUE);
  // Large values for 3D.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_3D, levels, internal_format,
                             gm_->GetMaxTextureSize() + 1, height, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_3D, levels, internal_format, width,
                             gm_->GetMaxTextureSize() + 1, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_3D, levels, internal_format, width,
                             height, gm_->GetMaxTextureSize() + 1),
                GL_INVALID_VALUE);
  // Large values for cubemap array.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             gm_->GetMaxCubeMapTextureSize() + 1,
                             height, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             width, gm_->GetMaxCubeMapTextureSize() + 1, depth),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             width, depth, gm_->GetMaxArrayTextureLayers() + 1),
                GL_INVALID_VALUE);
  // Cubemap dims not equal.
  GM_ERROR_CALL(TexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, levels, internal_format,
                             width, height + 1, depth),
                GL_INVALID_VALUE);

  // Verify textures are mutable.
  EXPECT_EQ(GL_FALSE, GetTextureInt(GL_TEXTURE_2D_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE,
            GetTextureInt(GL_TEXTURE_3D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_FALSE, GetTextureInt(GL_TEXTURE_CUBE_MAP_ARRAY,
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
  EXPECT_EQ(GL_TRUE, GetTextureInt(GL_TEXTURE_2D_ARRAY,
                                   GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_TRUE,
            GetTextureInt(GL_TEXTURE_3D, GL_TEXTURE_IMMUTABLE_FORMAT));
  EXPECT_EQ(GL_TRUE, GetTextureInt(GL_TEXTURE_CUBE_MAP_ARRAY,
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
  GM_ERROR_CALL(
      TexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, width, height, depth, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      CompressedTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_ETC1_RGB8_OES,
                           width, height, depth, 0, 1024, nullptr),
      GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, TexStorage2DMultisample) {
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
                                        gm_->GetMaxTextureSize() + 1, height,
                                        false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        internal_format, width,
                                        gm_->GetMaxTextureSize() + 1, false),
      GL_INVALID_VALUE);

  // Verify texture is mutable.
  EXPECT_EQ(GL_FALSE, GetTextureInt(GL_TEXTURE_2D_MULTISAMPLE,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));

  // Successful call.
  GM_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                  internal_format, width, height, false));

  // Texture should now be immutable.
  EXPECT_EQ(GL_TRUE, GetTextureInt(GL_TEXTURE_2D_MULTISAMPLE,
                                   GL_TEXTURE_IMMUTABLE_FORMAT));

  // Calling again on an already set texture is an invalid operation.
  GM_ERROR_CALL(TexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples,
                                        internal_format, width, height, false),
      GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, TexStorage3DMultisample) {
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
                                        gm_->GetMaxTextureSize() + 1, height,
                                        depth, false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width,
                                        gm_->GetMaxTextureSize() + 1, depth,
                                        false),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width, height,
                                        gm_->GetMaxTextureSize() + 1, false),
      GL_INVALID_VALUE);

  // Verify texture is mutable.
  EXPECT_EQ(GL_FALSE, GetTextureInt(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                    GL_TEXTURE_IMMUTABLE_FORMAT));

  // Successful call.
  GM_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples,
                                  internal_format, width, height, depth,
                                  false));

  // Texture should now be immutable.
  EXPECT_EQ(GL_TRUE, GetTextureInt(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                   GL_TEXTURE_IMMUTABLE_FORMAT));

  // Calling again on an already set texture is an invalid operation.
  GM_ERROR_CALL(TexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
                                        samples, internal_format, width, height,
                                        depth, false),
      GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, Samplers) {
  // The default sampler is 0.
  EXPECT_EQ(0, GetInt(GL_SAMPLER_BINDING));

  GLuint ids[2];
  ids[0] = ids[1] = 0U;
  GM_ERROR_CALL(GenSamplers(-1, ids), GL_INVALID_VALUE);
  EXPECT_EQ(0U, ids[0]);
  EXPECT_EQ(0U, ids[1]);
  GM_CALL(GenSamplers(2, ids));
  EXPECT_NE(0U, ids[0]);
  EXPECT_NE(0U, ids[1]);
  EXPECT_EQ(GL_FALSE, gm_->IsSampler(0U));
  EXPECT_EQ(GL_TRUE, gm_->IsSampler(ids[0]));
  EXPECT_EQ(GL_TRUE, gm_->IsSampler(ids[1]));
  EXPECT_EQ(GL_FALSE, gm_->IsSampler(ids[0] + ids[1]));

  GLuint max_units = static_cast<GLuint>(
      GetInt(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
  EXPECT_GT(max_units, 0U);

  // Bad binds.
  GM_ERROR_CALL(BindSampler(max_units + 1, ids[0]), GL_INVALID_VALUE);
  GM_ERROR_CALL(BindSampler(0, ids[0] + ids[1]), GL_INVALID_OPERATION);
  // Good binds.
  EXPECT_EQ(0, GetInt(GL_SAMPLER_BINDING));
  GM_CALL(BindSampler(0, 0));
  GM_CALL(BindSampler(0, ids[0]));
  EXPECT_EQ(static_cast<int>(ids[0]), GetInt(GL_SAMPLER_BINDING));

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
  EXPECT_EQ(1.f, GetSamplerFloat(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetSamplerInt(ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(ids[0], GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_R));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_T));
  // Error if an invalid enum is used.
  GM_ERROR_CALL(
      GetSamplerParameteriv(ids[0], GL_VERTEX_ATTRIB_ARRAY_SIZE, nullptr),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetSamplerParameterfv(ids[0], GL_VERTEX_ATTRIB_ARRAY_SIZE, nullptr),
      GL_INVALID_ENUM);

  // Check that changes happen.
  GLint mode = GL_CLAMP_TO_EDGE;
  GM_CALL(SamplerParameteriv(ids[0], GL_TEXTURE_WRAP_S, &mode));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetSamplerInt(ids[0], GL_TEXTURE_WRAP_S));
  GLfloat modef = GL_MIRRORED_REPEAT;
  GM_CALL(SamplerParameterfv(ids[0], GL_TEXTURE_WRAP_S, &modef));
  EXPECT_EQ(static_cast<GLfloat>(GL_MIRRORED_REPEAT),
            GetSamplerFloat(ids[0], GL_TEXTURE_WRAP_S));
  GM_CALL(
      SamplerParameteri(ids[0], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
  EXPECT_EQ(GL_CLAMP_TO_EDGE,
            GetSamplerInt(ids[0], GL_TEXTURE_WRAP_S));
  GM_ERROR_CALL(SamplerParameteri(ids[0], GL_TEXTURE_BASE_LEVEL, 2),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(SamplerParameteri(ids[0], GL_TEXTURE_MAX_LEVEL, 200),
                GL_INVALID_ENUM);
  GM_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MIN_LOD, 3.14f));
  EXPECT_EQ(3.14f, GetSamplerFloat(ids[0], GL_TEXTURE_MIN_LOD));
  GM_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MAX_LOD, 2.18f));
  EXPECT_EQ(2.18f, GetSamplerFloat(ids[0], GL_TEXTURE_MAX_LOD));
  GM_CALL(SamplerParameteri(ids[0], GL_TEXTURE_COMPARE_FUNC, GL_ALWAYS));
  EXPECT_EQ(GL_ALWAYS, GetSamplerInt(ids[0], GL_TEXTURE_COMPARE_FUNC));
  GM_CALL(SamplerParameteri(
      ids[0], GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE));
  EXPECT_EQ(GL_COMPARE_REF_TO_TEXTURE,
            GetSamplerInt(ids[0], GL_TEXTURE_COMPARE_MODE));

  GM_ERROR_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 0.9f),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 999.f),
                GL_INVALID_VALUE);
  GM_CALL(SamplerParameterf(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT, 3.f));
  EXPECT_EQ(3.f, GetSamplerFloat(ids[0], GL_TEXTURE_MAX_ANISOTROPY_EXT));

  // Check that changes affect only the proper parameter.
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetSamplerInt(ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(ids[0], GL_TEXTURE_MAG_FILTER));

  GM_CALL(SamplerParameteri(ids[0], GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT));
  EXPECT_EQ(GL_CLAMP_TO_EDGE, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetSamplerInt(ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(ids[0], GL_TEXTURE_MAG_FILTER));

  GM_CALL(SamplerParameteri(ids[0], GL_TEXTURE_MIN_FILTER, GL_NEAREST));
  EXPECT_EQ(GL_CLAMP_TO_EDGE, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST, GetSamplerInt(ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(ids[0], GL_TEXTURE_MAG_FILTER));

  GM_CALL(SamplerParameteri(ids[0], GL_TEXTURE_MAG_FILTER, GL_NEAREST));
  EXPECT_EQ(GL_CLAMP_TO_EDGE, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_MIRRORED_REPEAT, GetSamplerInt(ids[0], GL_TEXTURE_WRAP_T));
  EXPECT_EQ(GL_NEAREST, GetSamplerInt(ids[0], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_NEAREST, GetSamplerInt(ids[0], GL_TEXTURE_MAG_FILTER));

  // Check that the other sampler settings have not changed.
  EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR,
            GetSamplerInt(ids[1], GL_TEXTURE_MIN_FILTER));
  EXPECT_EQ(GL_LINEAR, GetSamplerInt(ids[1], GL_TEXTURE_MAG_FILTER));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(ids[1], GL_TEXTURE_WRAP_S));
  EXPECT_EQ(GL_REPEAT, GetSamplerInt(ids[1], GL_TEXTURE_WRAP_T));

  // Delete samplers.
  GLuint bad_id = 5U;
  // Error if n < 0.
  GM_ERROR_CALL(DeleteSamplers(-1, ids), GL_INVALID_VALUE);
  // Bad ids are silently ignored.
  GM_CALL(DeleteSamplers(1, &bad_id));
  // Actually delete the ids.
  GM_CALL(DeleteSamplers(2, ids));
  EXPECT_FALSE(gm_->IsSampler(ids[0]));
  EXPECT_FALSE(gm_->IsSampler(ids[1]));
  EXPECT_EQ(0, GetInt(GL_SAMPLER_BINDING));
  // Bind should fail on a deleted texture.
  GM_ERROR_CALL(BindSampler(0, ids[0]), GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, ArraysBuffersDrawFunctions) {
  // The default vertex buffer is 0.
  EXPECT_EQ(0, GetInt(GL_ARRAY_BUFFER_BINDING));

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
  GM_ERROR_CALL(DrawElements(GL_NEVER, 1, GL_UNSIGNED_BYTE, nullptr),
                GL_INVALID_ENUM);
  // Negative count.
  GM_ERROR_CALL(DrawElements(GL_POINTS, -1, GL_UNSIGNED_BYTE, nullptr),
                GL_INVALID_VALUE);
  // Bad type.
  GM_ERROR_CALL(DrawElements(GL_POINTS, 10, GL_FLOAT, nullptr),
                GL_INVALID_ENUM);
  // Successful call.
  GM_CALL(DrawElements(GL_POINTS, 2, GL_UNSIGNED_BYTE, nullptr));
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
  EXPECT_FALSE(gm_->IsVertexArray(0U));
  EXPECT_FALSE(gm_->IsVertexArray(vao));
  EXPECT_FALSE(gm_->IsVertexArray(vao2));
  EXPECT_FALSE(gm_->IsVertexArray(vao + vao2));

  // BindVertexArray
  GM_ERROR_CALL(BindVertexArray(5U), GL_INVALID_OPERATION);
  GM_ERROR_CALL(BindVertexArray(4U), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(GL_VERTEX_ARRAY_BINDING));
  // Bind valid array.
  GM_CALL(BindVertexArray(vao));
  EXPECT_TRUE(gm_->IsVertexArray(vao));

  // Check vertex attribute defaults.
  int attrib_count = GetInt(GL_MAX_VERTEX_ATTRIBS);
  EXPECT_GT(attrib_count, 0);
  for (int i = 0; i < attrib_count; ++i) {
    EXPECT_EQ(0, GetAttribInt(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
    EXPECT_EQ(GL_FALSE, GetAttribInt(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
    EXPECT_EQ(4, GetAttribInt(i, GL_VERTEX_ATTRIB_ARRAY_SIZE));
    EXPECT_EQ(0, GetAttribInt(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE));
    EXPECT_EQ(GL_FLOAT, GetAttribInt(i, GL_VERTEX_ATTRIB_ARRAY_TYPE));
    EXPECT_EQ(GL_FALSE, GetAttribInt(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED));
    EXPECT_EQ(0, GetAttribFloat(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
    EXPECT_EQ(GL_FALSE, GetAttribFloat(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
    EXPECT_EQ(4, GetAttribFloat(i, GL_VERTEX_ATTRIB_ARRAY_SIZE));
    EXPECT_EQ(0, GetAttribFloat(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE));
    EXPECT_EQ(GL_FLOAT, GetAttribFloat(i, GL_VERTEX_ATTRIB_ARRAY_TYPE));
    EXPECT_EQ(GL_FALSE,
              GetAttribFloat(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED));
    EXPECT_EQ(math::Vector4f(0.f, 0.f, 0.f, 1.f),
              GetAttribFloat4(i, GL_CURRENT_VERTEX_ATTRIB));
    EXPECT_EQ(math::Vector4i(0, 0, 0, 1),
              GetAttribInt4(i, GL_CURRENT_VERTEX_ATTRIB));
    EXPECT_EQ(reinterpret_cast<GLvoid*>(0),
              GetAttribPointer(1, GL_VERTEX_ATTRIB_ARRAY_POINTER));
    EXPECT_EQ(0, GetAttribFloat(i, GL_VERTEX_ATTRIB_ARRAY_DIVISOR));
  }
  // Check error conditions for GetVertexAttrib[if]v.
  GM_ERROR_CALL(GetVertexAttribiv(attrib_count,
                                  GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                                  nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(GetVertexAttribfv(attrib_count,
                                  GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,
                                  nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(GetVertexAttribiv(1, attrib_count, nullptr), GL_INVALID_ENUM);
  GM_ERROR_CALL(GetVertexAttribfv(1, attrib_count, nullptr), GL_INVALID_ENUM);

  // VertexAttributes
  // Enable attrib.
  GM_ERROR_CALL(EnableVertexAttribArray(attrib_count), GL_INVALID_VALUE);
  EXPECT_EQ(GL_FALSE, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
  GM_CALL(EnableVertexAttribArray(1));
  EXPECT_EQ(GL_TRUE, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));

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
            GetAttribPointer(1, GL_VERTEX_ATTRIB_ARRAY_POINTER));

  // Check that state follows vertex array binding.
  GM_CALL(BindVertexArray(vao2));
  EXPECT_EQ(vao2, static_cast<GLuint>(GetInt(GL_VERTEX_ARRAY_BINDING)));
  EXPECT_EQ(GL_FALSE, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
  EXPECT_EQ(0, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
  EXPECT_EQ(4, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_SIZE));
  EXPECT_EQ(0, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_STRIDE));
  EXPECT_EQ(GL_FLOAT, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_TYPE));
  EXPECT_EQ(GL_FALSE, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED));
  EXPECT_EQ(math::Vector4f(0.f, 0.f, 0.f, 1.f),
            GetAttribFloat4(1, GL_CURRENT_VERTEX_ATTRIB));
  EXPECT_EQ(reinterpret_cast<GLvoid*>(0),
            GetAttribPointer(1, GL_VERTEX_ATTRIB_ARRAY_POINTER));
  EXPECT_EQ(0, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_DIVISOR));

  GM_CALL(BindVertexArray(vao));
  EXPECT_EQ(vao, static_cast<GLuint>(GetInt(GL_VERTEX_ARRAY_BINDING)));
  EXPECT_EQ(GL_TRUE, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
  EXPECT_EQ(0, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
  EXPECT_EQ(2, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_SIZE));
  EXPECT_EQ(16, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_STRIDE));
  EXPECT_EQ(GL_SHORT, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_TYPE));
  EXPECT_EQ(GL_TRUE, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED));
  EXPECT_EQ(math::Vector4f(0.f, 0.f, 0.f, 1.f),
            GetAttribFloat4(1, GL_CURRENT_VERTEX_ATTRIB));
  EXPECT_EQ(reinterpret_cast<GLvoid*>(4),
            GetAttribPointer(1, GL_VERTEX_ATTRIB_ARRAY_POINTER));

  // Check that array IDs are not valid in other contexts.
  {
    portgfx::GlContextPtr share_context =
        FakeGlContext::CreateShared(*gl_context_);
    portgfx::GlContext::MakeCurrent(share_context);
    EXPECT_FALSE(gm_->IsVertexArray(vao));
    portgfx::GlContext::MakeCurrent(gl_context_);
  }

  // Disable attrib.
  GM_ERROR_CALL(DisableVertexAttribArray(attrib_count), GL_INVALID_VALUE);
  EXPECT_EQ(GL_TRUE, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));
  GM_CALL(DisableVertexAttribArray(1));
  EXPECT_EQ(GL_FALSE, GetAttribInt(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED));

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
            GetAttribFloat4(3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib2fv(3, &vert[0]));
  EXPECT_EQ(math::Vector4f(vert[0], vert[1], 0.f, 1.f),
            GetAttribFloat4(3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib3fv(3, &vert[0]));
  EXPECT_EQ(math::Vector4f(vert[0], vert[1], vert[2], 1.f),
            GetAttribFloat4(3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib4fv(3, &vert[0]));
  EXPECT_EQ(vert, GetAttribFloat4(3, GL_CURRENT_VERTEX_ATTRIB));
  vert.Set(4.f, 3.f, 2.f, 1.f);
  GM_CALL(VertexAttrib1f(3, vert[0]));
  EXPECT_EQ(math::Vector4f(vert[0], 0.f, 0.f, 1.f),
            GetAttribFloat4(3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib2f(3, vert[0], vert[1]));
  EXPECT_EQ(math::Vector4f(vert[0], vert[1], 0.f, 1.f),
            GetAttribFloat4(3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib3f(3, vert[0], vert[1], vert[2]));
  EXPECT_EQ(math::Vector4f(vert[0], vert[1], vert[2], 1.f),
            GetAttribFloat4(3, GL_CURRENT_VERTEX_ATTRIB));
  GM_CALL(VertexAttrib4f(3, vert[0], vert[1], vert[2], vert[3]));
  EXPECT_EQ(vert, GetAttribFloat4(3, GL_CURRENT_VERTEX_ATTRIB));

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
  EXPECT_FALSE(gm_->IsBuffer(0));
  EXPECT_FALSE(gm_->IsBuffer(vbo));
  EXPECT_FALSE(gm_->IsBuffer(vbo2));
  EXPECT_FALSE(gm_->IsBuffer(vbo + vbo2));

  // GetBufferParameteriv.
  // No buffer is bound.
  GM_ERROR_CALL(GetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE,
                                     nullptr), GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetBufferParameteriv(GL_TEXTURE_2D, GL_BUFFER_SIZE, nullptr),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetBufferParameteriv(GL_ARRAY_BUFFER, GL_FLOAT, nullptr),
                GL_INVALID_ENUM);

  // BindBuffer
  GM_ERROR_CALL(BindBuffer(GL_LINK_STATUS, 4U), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindBuffer(GL_ARRAY_BUFFER, 3U), GL_INVALID_VALUE);
  // Check that no buffer was bound.
  EXPECT_EQ(0, GetInt(GL_ARRAY_BUFFER_BINDING));

  // Check that vertex element arrays are bound to the current VAO.
  GM_CALL(BindVertexArray(0));
  EXPECT_EQ(0U,
            static_cast<GLuint>(GetInt(GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo));
  EXPECT_TRUE(gm_->IsBuffer(vbo));
  EXPECT_FALSE(gm_->IsBuffer(vbo2));
  GM_CALL(BindVertexArray(vao));
  // The binding should be overwritten.
  EXPECT_EQ(0U,
            static_cast<GLuint>(GetInt(GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo2));
  EXPECT_EQ(vbo2,
            static_cast<GLuint>(GetInt(GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindVertexArray(0));
  EXPECT_EQ(vbo,
            static_cast<GLuint>(GetInt(GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindVertexArray(vao));
  EXPECT_EQ(vbo2,
            static_cast<GLuint>(GetInt(GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  GM_CALL(BindVertexArray(0));

  // Bind valid buffers.
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, vbo));
  GM_CALL(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo2));
  // Check buffers are bound.
  EXPECT_EQ(vbo, static_cast<GLuint>(GetInt(GL_ARRAY_BUFFER_BINDING)));
  EXPECT_EQ(vbo2,
            static_cast<GLuint>(GetInt(GL_ELEMENT_ARRAY_BUFFER_BINDING)));

  // BufferData
  GM_ERROR_CALL(
      BufferData(GL_TEXTURE_2D, 1024, nullptr, GL_STATIC_DRAW),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      BufferData(GL_ARRAY_BUFFER, -1, nullptr, GL_STATIC_DRAW),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      BufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_FRONT), GL_INVALID_ENUM);
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, 0U));
  GM_ERROR_CALL(
      BufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW),
      GL_INVALID_OPERATION);
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, vbo));
  GM_CALL(
      BufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW));

  EXPECT_EQ(1024, GetBufferInt(GL_ARRAY_BUFFER, GL_BUFFER_SIZE));
  EXPECT_EQ(GL_STATIC_DRAW, GetBufferInt(GL_ARRAY_BUFFER, GL_BUFFER_USAGE));

  // BufferSubData
  GM_ERROR_CALL(BufferSubData(GL_TEXTURE_2D, 16, 10, nullptr), GL_INVALID_ENUM);
  GM_ERROR_CALL(BufferSubData(GL_ARRAY_BUFFER, -1, 10, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(BufferSubData(GL_ARRAY_BUFFER, 16, -1, nullptr),
                GL_INVALID_VALUE);
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, 0U));
  GM_ERROR_CALL(BufferSubData(GL_ARRAY_BUFFER, 16, 10, nullptr),
      GL_INVALID_OPERATION);
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, vbo));
  GM_ERROR_CALL(BufferSubData(GL_ARRAY_BUFFER, 1020, 10, nullptr),
                GL_INVALID_VALUE);
  GM_CALL(BufferSubData(GL_ARRAY_BUFFER, 128, 10, nullptr));

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
  EXPECT_EQ(0, GetAttribInt(5, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING));
  GM_CALL(VertexAttribPointer(5, 2, GL_SHORT, GL_FALSE, 0,
                              reinterpret_cast<GLvoid*>(8)));
  EXPECT_EQ(vbo2,
            static_cast<GLuint>(GetInt(GL_ELEMENT_ARRAY_BUFFER_BINDING)));

  // DeleteVertexArrays
  GM_ERROR_CALL(DeleteVertexArrays(-1, &vao), GL_INVALID_VALUE);
  GM_CALL(DeleteVertexArrays(1, &vao));
  // Invalid deletes are silently ignored.
  vao = 12U;
  GM_CALL(DeleteVertexArrays(1, &vao));
  EXPECT_FALSE(gm_->IsVertexArray(vao));

  // DeleteBuffers
  GM_ERROR_CALL(DeleteBuffers(-1, &vbo), GL_INVALID_VALUE);
  GM_CALL(DeleteBuffers(1, &vbo));
  GM_CALL(DeleteBuffers(1, &vbo2));
  EXPECT_FALSE(gm_->IsBuffer(vbo));
  EXPECT_FALSE(gm_->IsBuffer(vbo2));
  EXPECT_EQ(0U, static_cast<GLuint>(GetInt(GL_ARRAY_BUFFER_BINDING)));
  EXPECT_EQ(0U,
            static_cast<GLuint>(GetInt(GL_ELEMENT_ARRAY_BUFFER_BINDING)));
  // Invalid deletes are silently ignored.
  vbo = 12U;
  GM_CALL(DeleteBuffers(1, &vbo));
}

TEST_F(FakeGraphicsManagerTest, DrawInstancedFunctions) {
  // The default vertex buffer is 0.
  EXPECT_EQ(0, GetInt(GL_ARRAY_BUFFER_BINDING));

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
  GM_ERROR_CALL(
      DrawElementsInstanced(GL_NEVER, 1, GL_UNSIGNED_BYTE, nullptr, 10),
      GL_INVALID_ENUM);
  // Negative count.
  GM_ERROR_CALL(
      DrawElementsInstanced(GL_POINTS, -1, GL_UNSIGNED_BYTE, nullptr, 10),
      GL_INVALID_VALUE);
  // Negative primCount.
  GM_ERROR_CALL(
      DrawElementsInstanced(GL_POINTS, 1, GL_UNSIGNED_BYTE, nullptr, -1),
      GL_INVALID_VALUE);
  // Bad type.
  GM_ERROR_CALL(
      DrawElementsInstanced(GL_POINTS, 10, GL_FLOAT, nullptr, 10),
      GL_INVALID_ENUM);
  // Successful call.
  GM_CALL(DrawElementsInstanced(GL_POINTS, 2, GL_UNSIGNED_BYTE, nullptr, 10));
}

TEST_F(FakeGraphicsManagerTest, BindBufferIndexed) {
  struct {
    GLenum target, binding_query, start_query, size_query;
  } tests[] = {
    {GL_TRANSFORM_FEEDBACK_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER_BINDING,
     GL_TRANSFORM_FEEDBACK_BUFFER_START, GL_TRANSFORM_FEEDBACK_BUFFER_SIZE},
    {GL_UNIFORM_BUFFER, GL_UNIFORM_BUFFER_BINDING, GL_UNIFORM_BUFFER_START,
     GL_UNIFORM_BUFFER_SIZE}
  };
  TracingHelper helper;
  for (const auto& test : tests) {
    SCOPED_TRACE(helper.ToString("GLenum", test.target));
    // Prepare buffer for testing.
    GLuint id = 0;
    GM_CALL(GenBuffers(1, &id));
    GM_CALL(BindBuffer(test.target, id));
    std::vector<int> buffer_data(256);
    GM_CALL(BufferData(test.target, 256 * sizeof(int), buffer_data.data(),
                       GL_STATIC_READ));

    // Verify that the indexed target is distinct from the regular target and
    // that calls fail with invalid parameters.
    GLint bound_id = -1, offset = -1, size = -1;
    GM_CALL(GetIntegeri_v(test.binding_query, 0U, &bound_id));
    EXPECT_EQ(bound_id, 0);
    GM_ERROR_CALL(BindBufferBase(GL_INVALID_VALUE, 3U, id), GL_INVALID_ENUM);
    GM_ERROR_CALL(BindBufferBase(test.target, 3U, 0U), GL_INVALID_VALUE);
    GM_ERROR_CALL(BindBufferBase(test.target, 123456U, id), GL_INVALID_VALUE);
    GM_ERROR_CALL(BindBufferRange(GL_INVALID_VALUE, 2U, id, 256, 512),
                  GL_INVALID_ENUM);
    GM_ERROR_CALL(BindBufferRange(test.target, 123456U, id, 256, 512),
                  GL_INVALID_VALUE);
    GM_ERROR_CALL(BindBufferRange(test.target, 2U, 0U, 256, 512),
                  GL_INVALID_VALUE);
    GM_ERROR_CALL(BindBufferRange(test.target, 2U, id, 256, -12),
                  GL_INVALID_VALUE);
    GM_ERROR_CALL(BindBufferRange(test.target, 2U, id, 2048, 128),
                  GL_INVALID_VALUE);
    GM_CALL(BindBufferBase(test.target, 3U, id));
    GM_CALL(BindBufferRange(test.target, 2U, id, 256, 512));

    // Check that the bindings can be correctly queried.
    GM_CALL(GetIntegeri_v(test.binding_query, 3U, &bound_id));
    GM_CALL(GetIntegeri_v(test.start_query, 3U, &offset));
    GM_CALL(GetIntegeri_v(test.size_query, 3U, &size));
    EXPECT_EQ(static_cast<GLint>(id), bound_id);
    EXPECT_EQ(0, offset);
    EXPECT_EQ(static_cast<GLint>(256 * sizeof(int)), size);
    bound_id = 0U;
    GM_CALL(GetIntegeri_v(test.binding_query, 2U, &bound_id));
    GM_CALL(GetIntegeri_v(test.start_query, 2U, &offset));
    GM_CALL(GetIntegeri_v(test.size_query, 2U, &size));
    EXPECT_EQ(static_cast<GLint>(id), bound_id);
    EXPECT_EQ(256, offset);
    EXPECT_EQ(512, size);

    // Check that a different indexed target is still zero.
    offset = -1;
    GM_CALL(GetIntegeri_v(test.binding_query, 0U, &bound_id));
    GM_CALL(GetIntegeri_v(test.start_query, 0U, &offset));
    GM_CALL(GetIntegeri_v(test.size_query, 0U, &size));
    EXPECT_EQ(0, bound_id);
    EXPECT_EQ(0, offset);
    EXPECT_EQ(0, size);

    // Check behavior on buffer resize.
    GM_CALL(BufferData(test.target, 192 * sizeof(int),
                       buffer_data.data(), GL_STATIC_READ));
    GM_CALL(GetIntegeri_v(test.size_query, 3U, &size));
    EXPECT_EQ(static_cast<GLint>(192 * sizeof(int)), size);
  }
}

TEST_F(FakeGraphicsManagerTest, ComputeShaders) {
  GM_ERROR_CALL(DispatchCompute(1, 1, 1), GL_INVALID_OPERATION);
  GM_ERROR_CALL(DispatchComputeIndirect(0), GL_INVALID_OPERATION);

  GLuint id = gm_->CreateProgram();
  GLuint cid = gm_->CreateShader(GL_COMPUTE_SHADER);
  GM_CHECK_NO_ERROR;
  EXPECT_EQ(GL_TRUE, gm_->IsShader(cid));
  const char* compute_source = "void main() {}";
  GM_CALL(ShaderSource(cid, 1, &compute_source, nullptr));
  GM_CALL(AttachShader(id, cid));
  GM_CALL(CompileShader(cid));
  GM_CALL(LinkProgram(id));
  GM_CALL(DetachShader(id, cid));
  GM_CALL(DeleteShader(cid));
  GM_CALL(UseProgram(id));

  GM_ERROR_CALL(DispatchCompute(123456789, 123456789, 123456789),
                GL_INVALID_VALUE);
  GM_CALL(DispatchCompute(1, 2, 3));
  GM_ERROR_CALL(DispatchComputeIndirect(0), GL_INVALID_OPERATION);
  GLuint buffer = 0;
  GM_CALL(GenBuffers(1, &buffer));
  GM_CALL(BindBuffer(GL_DISPATCH_INDIRECT_BUFFER, buffer));
  uint32_t indirect_data[6] = { 1U, 2U, 3U, 4U, 5U, 6U };
  GM_CALL(BufferData(GL_DISPATCH_INDIRECT_BUFFER, 24, indirect_data,
                     GL_STATIC_DRAW));
  GM_ERROR_CALL(DispatchComputeIndirect(-1), GL_INVALID_VALUE);
  GM_ERROR_CALL(DispatchComputeIndirect(3), GL_INVALID_VALUE);
  GM_ERROR_CALL(DispatchComputeIndirect(123), GL_INVALID_VALUE);
  GM_CALL(DispatchComputeIndirect(12));
  GM_CALL(UseProgram(0U));
  GM_CALL(DeleteProgram(id));
}

TEST_F(FakeGraphicsManagerTest, MappedBuffers) {
  GLuint vbo = 0;
  GM_CALL(GenBuffers(1, &vbo));
  GM_CALL(BindBuffer(GL_ARRAY_BUFFER, vbo));

  // Try to map the buffer.
  uint8 data[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  GM_CALL(BufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW));

  // Check that data has been created.
  uint8* ptr = nullptr;
  GM_ERROR_CALL(GetBufferPointerv(GL_STATIC_DRAW, GL_BUFFER_MAP_POINTER,
                                  reinterpret_cast<void**>(&ptr)),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetBufferPointerv(GL_ARRAY_BUFFER, GL_ARRAY_BUFFER,
                                  reinterpret_cast<void**>(&ptr)),
                GL_INVALID_ENUM);
  EXPECT_EQ(nullptr, ptr);

  // Since we have yet to map the buffer, the mapped buffer pointer should be
  // nullptr.
  GM_CALL(GetBufferPointerv(GL_ARRAY_BUFFER, GL_BUFFER_MAP_POINTER,
                            reinterpret_cast<void**>(&ptr)));
  EXPECT_EQ(nullptr, ptr);

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
  EXPECT_NE(nullptr, ptr);
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
  EXPECT_NE(nullptr, ptr);
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

TEST_F(FakeGraphicsManagerTest, FrameAndRenderBuffers) {
  GM_ERROR_CALL(CheckFramebufferStatus(GL_BLEND), GL_INVALID_ENUM);
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CHECK_NO_ERROR;

  // GenFramebuffers.
  GLuint fb;
  GM_ERROR_CALL(GenFramebuffers(-1, &fb), GL_INVALID_VALUE);
  GM_CALL(GenFramebuffers(1, &fb));
  EXPECT_NE(0U, fb);

  // IsFramebuffer.
  EXPECT_TRUE(gm_->IsFramebuffer(0U));
  EXPECT_FALSE(gm_->IsFramebuffer(fb));
  EXPECT_FALSE(gm_->IsFramebuffer(fb + 1U));

  // GenRenderbuffers.
  GLuint color[2];
  GM_ERROR_CALL(GenRenderbuffers(-1, color), GL_INVALID_VALUE);
  GM_CALL(GenRenderbuffers(2, color));
  EXPECT_NE(0U, color[0]);
  EXPECT_NE(0U, color[1]);
  GLuint depth;
  GM_ERROR_CALL(GenRenderbuffers(-1, &depth), GL_INVALID_VALUE);
  GM_CALL(GenRenderbuffers(1, &depth));
  EXPECT_NE(0U, depth);
  GLuint stencil;
  GM_ERROR_CALL(GenRenderbuffers(-1, &stencil), GL_INVALID_VALUE);
  GM_CALL(GenRenderbuffers(1, &stencil));
  EXPECT_NE(0U, stencil);
  GLuint depth_stencil;
  GM_ERROR_CALL(GenRenderbuffers(-1, &depth_stencil), GL_INVALID_VALUE);
  GM_CALL(GenRenderbuffers(1, &depth_stencil));
  EXPECT_NE(0U, depth_stencil);
  // All allocated IDs should be unique.
  std::set<GLuint> id_set{color[0], color[1], depth, stencil, depth_stencil};
  EXPECT_EQ(5U, id_set.size());

  // IsRenderbuffer.
  EXPECT_FALSE(gm_->IsRenderbuffer(0U));
  EXPECT_FALSE(gm_->IsRenderbuffer(color[0]));
  EXPECT_FALSE(gm_->IsRenderbuffer(depth));
  EXPECT_FALSE(gm_->IsRenderbuffer(stencil));
  EXPECT_FALSE(gm_->IsRenderbuffer(stencil + depth + color[0] + color[1]));

  // Can't call on framebuffer 0.
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, color[0]), GL_INVALID_OPERATION);
  GLint value;
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &value), GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D, 0, 0),
                GL_INVALID_OPERATION);

  // Check values before binding a framebuffer.
  EXPECT_EQ(8, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(8, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(16, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(8, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(8, GetInt(GL_RED_BITS));
  EXPECT_EQ(8, GetInt(GL_STENCIL_BITS));

  // BindFramebuffer.
  EXPECT_EQ(0, GetInt(GL_FRAMEBUFFER_BINDING));
  GM_ERROR_CALL(BindFramebuffer(GL_TEXTURE_2D, fb), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindFramebuffer(GL_FRAMEBUFFER, 3U), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(GL_FRAMEBUFFER_BINDING));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  EXPECT_EQ(static_cast<GLint>(fb), GetInt(GL_FRAMEBUFFER_BINDING));

  // By default these are 0.
  EXPECT_EQ(0, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(0, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(0, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(0, GetInt(GL_RED_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));

  // BindRenderbuffer.
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, depth), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(GL_RENDERBUFFER_BINDING));
  GM_ERROR_CALL(BindRenderbuffer(GL_TEXTURE_2D, color[0]), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindRenderbuffer(GL_RENDERBUFFER, 217U), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(GL_RENDERBUFFER_BINDING));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color[0]));
  EXPECT_TRUE(gm_->IsRenderbuffer(color[0]));
  EXPECT_FALSE(gm_->IsRenderbuffer(color[1]));
  EXPECT_EQ(static_cast<GLint>(color[0]), GetInt(GL_RENDERBUFFER_BINDING));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color[1]));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, depth));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, stencil));
  EXPECT_TRUE(gm_->IsRenderbuffer(color[1]));
  EXPECT_TRUE(gm_->IsRenderbuffer(depth));
  EXPECT_TRUE(gm_->IsRenderbuffer(stencil));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, 0));

  // FramebufferRenderbuffer.
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_DEPTH_TEST, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, color[0]), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_BLEND_COLOR,
                              GL_RENDERBUFFER, color[0]), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_VERTEX_SHADER, color[0]), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, *(--id_set.end()) + 1),
      GL_INVALID_OPERATION);

  // Should be no attachments.
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  // Error to query name if there is no binding.
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, nullptr), GL_INVALID_ENUM);

  // Status is incomplete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, color[0]));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depth));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, stencil));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  // GetFramebufferAttachmentParameteriv.
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAGMENT_SHADER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, nullptr), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_SHADER_COMPILER,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, nullptr), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_DEPTH_TEST, nullptr), GL_INVALID_ENUM);

  // Check values.
  EXPECT_EQ(GL_RENDERBUFFER, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(static_cast<GLint>(color[0]), GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));
  EXPECT_EQ(GL_RENDERBUFFER, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(static_cast<GLint>(depth), GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));
  EXPECT_EQ(GL_RENDERBUFFER, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(static_cast<GLint>(stencil), GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));

  // Invalid calls since binding is not a texture.
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, nullptr), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetFramebufferAttachmentParameteriv(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, nullptr),
      GL_INVALID_ENUM);

  int width = 1024;
  int height = 1024;
  // Can't call if no renderbuffer is bound.
  GM_ERROR_CALL(
      GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,
                                 nullptr),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, width, height),
                GL_INVALID_OPERATION);

  // Check defaults using GetRenderbufferInt.
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color[0]));
  GM_ERROR_CALL(
      GetRenderbufferParameteriv(GL_COMPILE_STATUS, GL_RENDERBUFFER_WIDTH,
                                 nullptr),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_VERSION, nullptr),
      GL_INVALID_ENUM);
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_WIDTH));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_HEIGHT));
  EXPECT_EQ(GL_RGBA4, GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(0, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(0, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(0, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(0, GetInt(GL_RED_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));

  // RenderbufferStorage.
  int max_size = GetInt(GL_MAX_RENDERBUFFER_SIZE);
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
  EXPECT_EQ(width, GetRenderbufferInt(GL_RENDERBUFFER_WIDTH));
  EXPECT_EQ(height, GetRenderbufferInt(GL_RENDERBUFFER_HEIGHT));
  EXPECT_EQ(GL_RGB565, GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(6, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(5, GetInt(GL_RED_BITS));
  EXPECT_EQ(6, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(0, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(GL_DEPTH_BITS));
  // RGBA4
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, width, height));
  EXPECT_EQ(GL_RGBA4, GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(4, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(4, GetInt(GL_RED_BITS));
  EXPECT_EQ(4, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(4, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(4, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(GL_DEPTH_BITS));
  // RGB5_A1
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGB5_A1, width, height));
  EXPECT_EQ(GL_RGB5_A1,
            GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(1, GetRenderbufferInt(GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(5, GetInt(GL_RED_BITS));
  EXPECT_EQ(5, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(1, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(GL_DEPTH_BITS));

  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, depth));
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                              width, height));
  EXPECT_EQ(GL_DEPTH_COMPONENT16,
            GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(16, GetRenderbufferInt(GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(5, GetInt(GL_RED_BITS));
  EXPECT_EQ(5, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(1, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(16, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, stencil));
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                              128, 128));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(8, GetRenderbufferInt(GL_RENDERBUFFER_STENCIL_SIZE));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, depth_stencil));
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8,
                              width, height));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(32, GetRenderbufferInt(GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(8, GetRenderbufferInt(GL_RENDERBUFFER_STENCIL_SIZE));

  // Status is incomplete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, stencil));
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                              width, height));
  EXPECT_EQ(GL_STENCIL_INDEX8,
            GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(8, GetRenderbufferInt(GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(5, GetInt(GL_RED_BITS));
  EXPECT_EQ(5, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(1, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(16, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(8, GetInt(GL_STENCIL_BITS));

  // Status is complete.
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  // Should still be complete after binding a packed depth stencil attachment.
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, depth_stencil));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  EXPECT_EQ(32, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(8, GetInt(GL_STENCIL_BITS));

  // Unbinding only the stencil should keep the depth.
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, 0U));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  EXPECT_EQ(32, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));

  // After binding an incompatible attachment, the status should change.
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color[1]));
  GM_CALL(RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, width + 100,
                              height + 100));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                  GL_RENDERBUFFER, color[1]));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                  GL_RENDERBUFFER, 0U));
  EXPECT_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

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
                     height, border, format, type, nullptr));

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
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                               GL_TEXTURE_2D, tex_id, 1));
  // Now we have a texture bound.
  EXPECT_EQ(static_cast<GLint>(tex_id), GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));
  EXPECT_EQ(1, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL));
  // Not a cube map.
  EXPECT_EQ(0, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE));

  // Use a non-0 level.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                               GL_TEXTURE_2D, tex_id, 1));
  // Check that we have a texture bound.
  EXPECT_EQ(GL_TEXTURE, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  // Ok with texture 0, since that disables the attachment.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                               GL_TEXTURE_2D, 0, 0));
  // Check that we have no texture bound.
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));

  // Bind more for coverage.
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, cube_tex_id));
  GM_CALL(TexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, level, internal_format,
                     width, height, border, format, type, nullptr));
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3,
                               GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, cube_tex_id, 1));
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, tex_id, 1));
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D, tex_id, 1));
  EXPECT_EQ(
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE));

  // Check that framebuffer IDs are not valid in other contexts.
  {
    portgfx::GlContextPtr share_context =
        FakeGlContext::CreateShared(*gl_context_);
    portgfx::GlContext::MakeCurrent(share_context);
    EXPECT_FALSE(gm_->IsFramebuffer(fb));
    portgfx::GlContext::MakeCurrent(gl_context_);
  }

  // ReadPixels.
  int x = 0;
  int y = 0;
  GM_ERROR_CALL(ReadPixels(x, y, -1, height, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(ReadPixels(x, y, width, -1, format, type, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(ReadPixels(x, y, width, height, GL_RED_BITS, type, nullptr),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(ReadPixels(x, y, width, height, format, GL_VENDOR, nullptr),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(ReadPixels(x, y, width, height, GL_RGB,
                           GL_UNSIGNED_SHORT_4_4_4_4, nullptr),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(ReadPixels(x, y, width, height, GL_RGBA,
                           GL_UNSIGNED_SHORT_5_6_5, nullptr),
                GL_INVALID_OPERATION);
  // Framebuffer is incomplete.
  GM_ERROR_CALL(ReadPixels(x, y, width, height, format, type, nullptr),
                GL_INVALID_FRAMEBUFFER_OPERATION);

  GM_ERROR_CALL(DeleteFramebuffers(-1, &fb), GL_INVALID_VALUE);
  GM_CALL(DeleteFramebuffers(1, &fb));
  GM_CALL(DeleteFramebuffers(1, &fb));

  GM_ERROR_CALL(DeleteRenderbuffers(-1, &color[0]), GL_INVALID_VALUE);
  GM_CALL(DeleteRenderbuffers(2, color));
  GM_CALL(DeleteRenderbuffers(2, color));
  GM_CALL(DeleteRenderbuffers(1, &stencil));

  GM_ERROR_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb), GL_INVALID_OPERATION);
  GM_ERROR_CALL(BindRenderbuffer(GL_RENDERBUFFER, color[0]),
                GL_INVALID_OPERATION);

  // Works with framebuffer 0.
  GM_CALL(ReadPixels(x, y, width, height, format, type, nullptr));
}

TEST_F(FakeGraphicsManagerTest, FramebufferTextureLayerAttachments) {
  GLuint fb, tex2d, tex3d, tex3d_ds;
  GM_CALL(GenFramebuffers(1, &fb));
  GM_CALL(GenTextures(1, &tex2d));
  GM_CALL(GenTextures(1, &tex3d));
  GM_CALL(GenTextures(1, &tex3d_ds));
  GM_CALL(BindTexture(GL_TEXTURE_2D, tex2d));
  GM_CALL(TexImage2D(GL_TEXTURE_2D, 0, GL_R8, 64, 64, 0, GL_RED,
                     GL_UNSIGNED_BYTE, nullptr));
  GM_CALL(BindTexture(GL_TEXTURE_3D, tex3d));
  GM_CALL(TexImage3D(GL_TEXTURE_3D, 0, GL_R8, 64, 64, 16, 0, GL_RED,
                     GL_UNSIGNED_BYTE, nullptr));
  GM_CALL(BindTexture(GL_TEXTURE_2D_ARRAY, tex3d_ds));
  GM_CALL(TexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH24_STENCIL8, 64, 64, 16, 0,
                     GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr));
  GM_ERROR_CALL(FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        tex3d, 0, 4), GL_INVALID_OPERATION);
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  GM_ERROR_CALL(FramebufferTextureLayer(GL_FLOAT, GL_COLOR_ATTACHMENT0, tex3d,
                                        0, 7), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTextureLayer(GL_FRAMEBUFFER, GL_TEXTURE_3D, tex3d,
                                        0, 7), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        tex2d, 1, 0), GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        tex3d, 50, 0), GL_INVALID_VALUE);
  GM_ERROR_CALL(FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        tex3d, 0, 9000), GL_INVALID_VALUE);
  GM_CALL(FramebufferTextureLayer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, 0, -23, 570));
  GM_CALL(FramebufferTextureLayer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, tex3d, 2, 7));
  {
    GLint type = -1, name = -1, level = -1, layer = -1, face = -1;
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type));
    EXPECT_EQ(type, GL_TEXTURE);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name));
    EXPECT_EQ(name, static_cast<GLint>(tex3d));
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level));
    EXPECT_EQ(2, level);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &layer));
    EXPECT_EQ(7, layer);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
        &face));
    EXPECT_EQ(0, face);
  }

  GLuint cube_map;
  GM_CALL(GenTextures(1, &cube_map));
  GM_CALL(BindTexture(GL_TEXTURE_CUBE_MAP, cube_map));
  for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
       face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
       ++face) {
    GM_CALL(TexImage2D(face, 0, GL_R8, 64, 64, 0, GL_RED, GL_UNSIGNED_BYTE,
                       nullptr));
  }
  GM_ERROR_CALL(FramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                        cube_map, 0, 7), GL_INVALID_VALUE);
  GM_CALL(FramebufferTextureLayer(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, cube_map, 0, 3));
  {
    GLint type = -1, name = -1, level = -1, layer = -1, face = -1;
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type));
    EXPECT_EQ(type, GL_TEXTURE);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name));
    EXPECT_EQ(name, static_cast<GLint>(cube_map));
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level));
    EXPECT_EQ(0, level);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &layer));
    EXPECT_EQ(0, layer);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT1, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
        &face));
    EXPECT_EQ(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, face);
  }

  GM_CALL(FramebufferTextureLayer(
      GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, tex3d_ds, 0, 5));
  // Test queries for depth attachment.
  {
    GLint type = -1, name = -1, level = -1, layer = -1, face = -1;
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type));
    EXPECT_EQ(type, GL_TEXTURE);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name));
    EXPECT_EQ(name, static_cast<GLint>(tex3d_ds));
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level));
    EXPECT_EQ(0, level);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &layer));
    EXPECT_EQ(5, layer);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
        &face));
    EXPECT_EQ(0, face);
  }
  // Test queries for stencil attachment.
  {
    GLint type = -1, name = -1, level = -1, layer = -1, face = -1;
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type));
    EXPECT_EQ(type, GL_TEXTURE);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name));
    EXPECT_EQ(name, static_cast<GLint>(tex3d_ds));
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
        &level));
    EXPECT_EQ(0, level);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER,
        &layer));
    EXPECT_EQ(5, layer);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE,
        &face));
    EXPECT_EQ(0, face);
  }
  // Test queries for depth-stencil attachment.
  {
    GLint type = -1, name = -1, level = -1, layer = -1, face = -1;
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
        &type));
    EXPECT_EQ(type, GL_TEXTURE);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
        &name));
    EXPECT_EQ(name, static_cast<GLint>(tex3d_ds));
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL,
        &level));
    EXPECT_EQ(0, level);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER,
        &layer));
    EXPECT_EQ(5, layer);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE, &face));
    EXPECT_EQ(0, face);
  }

  // Unbind stencil attachment. Depth-stencil attachment queries should fail.
  GLint type;
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                  GL_RENDERBUFFER, 0));
  GM_ERROR_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
      GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
      &type), GL_INVALID_OPERATION);
  GM_CALL(FramebufferTextureLayer(
      GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, tex3d_ds, 0, 5));
  GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
      GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
      &type));
  EXPECT_EQ(type, GL_TEXTURE);
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, 0));
  GM_ERROR_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
      GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
      &type), GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, MultiviewAttachments) {
  GLuint fb, color_tex, depth_tex, tex3d;
  GM_CALL(GenFramebuffers(1, &fb));
  GM_CALL(GenTextures(1, &color_tex));
  GM_CALL(GenTextures(1, &depth_tex));
  GM_CALL(GenTextures(1, &tex3d));
  GM_CALL(BindTexture(GL_TEXTURE_2D_ARRAY, color_tex));
  GM_CALL(TexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R8, 64, 64, 8, 0, GL_RED,
                     GL_UNSIGNED_BYTE, nullptr));
  GM_CALL(BindTexture(GL_TEXTURE_2D_ARRAY, depth_tex));
  GM_CALL(TexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, 64, 64, 16, 0,
                     GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr));
  GM_CALL(BindTexture(GL_TEXTURE_3D, tex3d));
  GM_CALL(TexImage3D(GL_TEXTURE_3D, 0, GL_R8, 64, 64, 16, 0, GL_RED,
                     GL_UNSIGNED_BYTE, nullptr));

  GM_ERROR_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 3, 4), GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 4, 3, 4), GL_INVALID_OPERATION);
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  GM_ERROR_CALL(FramebufferTextureMultiviewOVR(GL_READ_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 3, 4), GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTextureMultiviewOVR(GL_TEXTURE_2D,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 3, 4), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, color_tex, 0, 3, 4), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 30, 3, 4), GL_INVALID_VALUE);
  GM_ERROR_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 9000, 4), GL_INVALID_VALUE);
  GM_ERROR_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 3, 0), GL_INVALID_VALUE);
  GM_ERROR_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 3, 16), GL_INVALID_VALUE);
  GM_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                         0, 0, 0, 0));
  GM_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                         color_tex, 0, 3, 4));
  {
    GLint type = -1, name = -1, level = -1, base_view_index = -1;
    GLint num_views = -1, samples = -1;
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type));
    EXPECT_EQ(type, GL_TEXTURE);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name));
    EXPECT_EQ(name, static_cast<GLint>(color_tex));
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level));
    EXPECT_EQ(0, level);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR,
        &base_view_index));
    EXPECT_EQ(3, base_view_index);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR,
        &num_views));
    EXPECT_EQ(4, num_views);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT,
        &samples));
    EXPECT_EQ(0, samples);
  }
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  GM_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                         depth_tex, 0, 7, 2));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                         depth_tex, 0, 9, 4));
  GM_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                                         tex3d, 0, 6, 4));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  // Test multisampled multiview attachments
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, 0, 0));
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D, 0, 0));
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_READ_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 8, 3, 4), GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_TEXTURE_2D,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 8, 3, 4), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, color_tex, 0, 8, 3, 4), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, tex3d, 0, 8, 3, 4), GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 30, 8, 3, 4), GL_INVALID_VALUE);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 100, 3, 4), GL_INVALID_VALUE);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 8, 9876, 4), GL_INVALID_VALUE);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 8, 3, 0), GL_INVALID_VALUE);
  GM_ERROR_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 8, 3, 16), GL_INVALID_VALUE);
  GM_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, 0, 0, 0, 0, 0));
  GM_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, color_tex, 0, 8, 3, 4));
  {
    GLint type = -1, name = -1, level = -1, base_view_index = -1;
    GLint num_views = -1, samples = -1;
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type));
    EXPECT_EQ(type, GL_TEXTURE);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name));
    EXPECT_EQ(name, static_cast<GLint>(color_tex));
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level));
    EXPECT_EQ(0, level);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT,
        &samples));
    EXPECT_EQ(8, samples);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR,
        &base_view_index));
    EXPECT_EQ(3, base_view_index);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR,
        &num_views));
    EXPECT_EQ(4, num_views);
  }
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  GM_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_DEPTH_ATTACHMENT, depth_tex, 0, 8, 7, 2));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_DEPTH_ATTACHMENT, depth_tex, 0, 4, 7, 4));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferTextureMultiviewOVR(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                         depth_tex, 0, 7, 4));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferTextureMultisampleMultiviewOVR(GL_FRAMEBUFFER,
      GL_DEPTH_ATTACHMENT, depth_tex, 0, 8, 9, 4));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
}

TEST_F(FakeGraphicsManagerTest, MultisampleFramebuffers) {
  // GenFramebuffers.
  GLuint fb;
  GM_ERROR_CALL(GenFramebuffers(-1, &fb), GL_INVALID_VALUE);
  GM_CALL(GenFramebuffers(1, &fb));
  EXPECT_NE(0U, fb);

  // GenRenderbuffers.
  GLuint color0;
  GM_CALL(GenRenderbuffers(1, &color0));
  EXPECT_NE(0U, color0);

  // BindRenderbuffer.
  EXPECT_EQ(0, GetInt(GL_RENDERBUFFER_BINDING));
  GM_ERROR_CALL(BindRenderbuffer(GL_TEXTURE_2D, color0), GL_INVALID_ENUM);
  GM_ERROR_CALL(BindRenderbuffer(GL_RENDERBUFFER, 4U), GL_INVALID_OPERATION);
  EXPECT_EQ(0, GetInt(GL_RENDERBUFFER_BINDING));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color0));
  EXPECT_EQ(static_cast<GLint>(color0), GetInt(GL_RENDERBUFFER_BINDING));

  // BindFramebuffer.
  EXPECT_EQ(0, GetInt(GL_FRAMEBUFFER_BINDING));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  EXPECT_EQ(static_cast<GLint>(fb), GetInt(GL_FRAMEBUFFER_BINDING));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, color0));

  // RenderbufferStorageMultisample.
  const int width = 1024;
  const int height = 1024;
  const int samples = 8;
  const int max_size = GetInt(GL_MAX_RENDERBUFFER_SIZE);
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
  EXPECT_EQ(width, GetRenderbufferInt(GL_RENDERBUFFER_WIDTH));
  EXPECT_EQ(height, GetRenderbufferInt(GL_RENDERBUFFER_HEIGHT));
  EXPECT_EQ(GL_RGB565, GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(6, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_DEPTH_SIZE));
  EXPECT_EQ(0, GetRenderbufferInt(GL_RENDERBUFFER_STENCIL_SIZE));
  EXPECT_EQ(samples, GetRenderbufferInt(GL_RENDERBUFFER_SAMPLES));
  EXPECT_EQ(5, GetInt(GL_RED_BITS));
  EXPECT_EQ(6, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(0, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(GL_DEPTH_BITS));
  // RGBA4
  GM_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                         GL_RGBA4, width, height));
  EXPECT_EQ(GL_RGBA4, GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(4, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(4, GetRenderbufferInt(GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(samples, GetRenderbufferInt(GL_RENDERBUFFER_SAMPLES));
  EXPECT_EQ(4, GetInt(GL_RED_BITS));
  EXPECT_EQ(4, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(4, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(4, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(GL_DEPTH_BITS));
  // RGB5_A1
  GM_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                         GL_RGB5_A1, width, height));
  EXPECT_EQ(GL_RGB5_A1,
            GetRenderbufferInt(GL_RENDERBUFFER_INTERNAL_FORMAT));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_RED_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_GREEN_SIZE));
  EXPECT_EQ(5, GetRenderbufferInt(GL_RENDERBUFFER_BLUE_SIZE));
  EXPECT_EQ(1, GetRenderbufferInt(GL_RENDERBUFFER_ALPHA_SIZE));
  EXPECT_EQ(samples, GetRenderbufferInt(GL_RENDERBUFFER_SAMPLES));
  EXPECT_EQ(5, GetInt(GL_RED_BITS));
  EXPECT_EQ(5, GetInt(GL_GREEN_BITS));
  EXPECT_EQ(5, GetInt(GL_BLUE_BITS));
  EXPECT_EQ(1, GetInt(GL_ALPHA_BITS));
  EXPECT_EQ(0, GetInt(GL_STENCIL_BITS));
  EXPECT_EQ(0, GetInt(GL_DEPTH_BITS));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  // Create a multisample texture.
  const GLint internal_format = GL_RGBA;
  GLuint ms_tex;
  GM_CALL(GenTextures(1, &ms_tex));
  GM_CALL(BindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms_tex));
  GM_CALL(TexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples/2,
                                internal_format, width, height, true));

  GM_ERROR_CALL(FramebufferTexture2D(GL_FRONT, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D_MULTISAMPLE, ms_tex, 0),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_BACK,
                                     GL_TEXTURE_2D_MULTISAMPLE, ms_tex, 0),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_CCW, ms_tex, 0), GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D_MULTISAMPLE, 3U, 0),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, ms_tex, 1),
                GL_INVALID_OPERATION);

  // Bind the texture.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D_MULTISAMPLE, ms_tex, 0));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(RenderbufferStorageMultisample(GL_RENDERBUFFER, samples/2,
                                         GL_RGBA4, width, height));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  // Now we have a texture bound.
  EXPECT_EQ(static_cast<GLint>(ms_tex), GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME));
  EXPECT_EQ(0, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
          GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL));

  // Check that we have a texture bound.
  EXPECT_EQ(GL_TEXTURE, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  // Ok with texture 0, since that disables the attachment.
  GM_CALL(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                               GL_TEXTURE_2D_MULTISAMPLE, 0, 0));
  // Check that we have no texture bound.
  EXPECT_EQ(GL_NONE, GetFramebufferAttachmentInt(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));

  // Implicit multisampling (EXT_multisampled_render_to_texture).
  GLuint color3;
  GM_CALL(GenRenderbuffers(1, &color3));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color3));
  GM_ERROR_CALL(RenderbufferStorageMultisampleEXT(GL_FRAMEBUFFER, samples,
                                                  GL_RGBA8, width, height),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, -1, GL_RGBA8,
                                                  width, height),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, samples,
                                                  GL_RED, width, height),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, samples,
                                                  GL_RGBA8, 23110481, height),
                GL_INVALID_VALUE);
  GM_CALL(RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, samples,
                                            GL_RGBA8, width, height));
  EXPECT_EQ(samples, GetRenderbufferInt(GL_RENDERBUFFER_SAMPLES));
  GM_CALL(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3,
                                  GL_RENDERBUFFER, color3));
  EXPECT_EQ(GL_RENDERBUFFER, GetFramebufferAttachmentInt(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, samples/2,
                                            GL_RGBA8, width, height));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(BindRenderbuffer(GL_RENDERBUFFER, color0));
  GM_CALL(RenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, samples/2,
                                            GL_RGBA8, width, height));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));

  // Implicit multisampling with textures.
  GM_ERROR_CALL(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
          GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, ms_tex, 0, samples),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
          GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ms_tex, 0, samples),
      GL_INVALID_OPERATION);

  GLuint regular_tex;
  GM_CALL(GenTextures(1, &regular_tex));
  GM_CALL(BindTexture(GL_TEXTURE_2D, regular_tex));
  GM_CALL(TexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0,
                     internal_format, GL_UNSIGNED_BYTE, nullptr));
  GM_ERROR_CALL(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
          GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, regular_tex, 0,
          samples),
      GL_INVALID_ENUM);
  GM_CALL(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, regular_tex, 0,
      samples));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  GM_CALL(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, regular_tex, 0,
      samples/2));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_FRAMEBUFFER));
  {
    GLint type = -1, name = -1, level = -1, layer = -1, samples = -1;
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type));
    EXPECT_EQ(type, GL_TEXTURE);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name));
    EXPECT_EQ(name, static_cast<GLint>(regular_tex));
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &level));
    EXPECT_EQ(0, level);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &layer));
    EXPECT_EQ(0, layer);
    GM_CALL(GetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT,
        &samples));
    EXPECT_EQ(4, samples);
  }
}

TEST_F(FakeGraphicsManagerTest, ResolveMultisampleFramebuffer) {
  const int width = 1024;
  const int height = 1024;
  const int samples = 8;

  // 1. Test the valid case
  // Read buffer
  GLuint multisample_sample_read_buffer;
  GM_CALL(GenFramebuffers(1, &multisample_sample_read_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, multisample_sample_read_buffer));
  AllocateAndAttachMultisampleRenderBuffer(
      GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height, samples);
  AllocateAndAttachMultisampleRenderBuffer(
      GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, width, height,
      samples);

  // Draw buffer
  GLuint draw_frame_buffer;
  GM_CALL(GenFramebuffers(1, &draw_frame_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, draw_frame_buffer));
  AllocateAndAttachRenderBuffer(
      GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, multisample_sample_read_buffer));
  EXPECT_EQ(static_cast<GLint>(draw_frame_buffer),
            GetInt(GL_DRAW_FRAMEBUFFER_BINDING));
  EXPECT_EQ(static_cast<GLint>(multisample_sample_read_buffer),
            GetInt(GL_READ_FRAMEBUFFER_BINDING));
  GM_CALL(ResolveMultisampleFramebuffer());

  // 2. GL_INVALID_OPERATION: SAMPLE_BUFFERS for the read framebuffer is zero.
  GLuint zero_sample_size_read_buffer;
  GM_CALL(GenFramebuffers(1, &zero_sample_size_read_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, zero_sample_size_read_buffer));
  // Set sample size to be zero.
  AllocateAndAttachMultisampleRenderBuffer(
      GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height, 0);
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
      GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height, 1);
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
      GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, width, height,
      samples);
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
      GL_DEPTH_COMPONENT16, GL_DEPTH_ATTACHMENT, width, height);
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
      GL_RGBA4, GL_COLOR_ATTACHMENT0, width / 2, height / 2);
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
      GL_RGBA8, GL_COLOR_ATTACHMENT0, width, height);
  GM_CALL(BindFramebuffer(
      GL_DRAW_FRAMEBUFFER, format_different_draw_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, multisample_sample_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(), GL_INVALID_OPERATION);

  // 8 INVALID_FRAMEBUFFER_OPERATION: Draw buffer is not framebuffer complete.
  GLuint incomplete_draw_buffer;
  GM_CALL(GenFramebuffers(1, &incomplete_draw_buffer));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, incomplete_draw_buffer));
  AllocateAndAttachRenderBuffer(
      GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height);
  // The format for the depth buffer is bad.
  AllocateAndAttachRenderBuffer(
      GL_RGBA4, GL_DEPTH_ATTACHMENT, width, height);
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
      GL_RGBA4, GL_COLOR_ATTACHMENT0, width, height, samples);
  // The format for the depth buffer is bad.
  AllocateAndAttachMultisampleRenderBuffer(
      GL_RGBA4, GL_DEPTH_ATTACHMENT, width, height, samples);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_frame_buffer));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, incomplete_read_buffer));
  GM_ERROR_CALL(ResolveMultisampleFramebuffer(),
                GL_INVALID_FRAMEBUFFER_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, IsExtensionSupportedParsesUnprefixedExtension) {
  gm_->SetExtensionsString("GLX_SGI_swap_control");
  EXPECT_TRUE(gm_->IsExtensionSupported("swap_control"));

  gm_->SetExtensionsString("WGL_EXT_swap_control");
  EXPECT_TRUE(gm_->IsExtensionSupported("swap_control"));

  gm_->SetExtensionsString("FOO_bar_BAZ");
  EXPECT_FALSE(gm_->IsExtensionSupported("FOO_bar_BAZ"));
  EXPECT_FALSE(gm_->IsExtensionSupported("bar"));
  EXPECT_FALSE(gm_->IsExtensionSupported("BAZ"));
  EXPECT_TRUE(gm_->IsExtensionSupported("bar_BAZ"));
}

TEST_F(FakeGraphicsManagerTest, FunctionGroupsAreDisabledByMissingExtensions) {
  // These tests are to increase coverage.
  EXPECT_TRUE(gm_->IsExtensionSupported("debug_label"));
  EXPECT_TRUE(gm_->IsExtensionSupported("discard_framebuffer"));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDebugLabel));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
  gm_->SetExtensionsString("GL_EXT_debug_label GL_EXT_discard_framebuffer");
  EXPECT_TRUE(gm_->IsExtensionSupported("debug_label"));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDebugLabel));
  EXPECT_TRUE(gm_->IsExtensionSupported("discard_framebuffer"));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
  gm_->SetExtensionsString("GL_EXT_discard_framebuffer");
  EXPECT_FALSE(gm_->IsExtensionSupported("debug_label"));
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kDebugLabel));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
  EXPECT_TRUE(gm_->IsExtensionSupported("discard_framebuffer"));
  gm_->SetExtensionsString("GL_EXT_debug_label");
  EXPECT_TRUE(gm_->IsExtensionSupported("debug_label"));
  EXPECT_FALSE(gm_->IsExtensionSupported("discard_framebuffer"));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDebugLabel));
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
  gm_->SetVersionString("1.2 Ion OpenGL");
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));

  // Check some special cases.
  //
  // Check that if GenVertexArrays fails that the extension is disabled.
  gl_context_ = FakeGlContext::Create(kWidth, kHeight);
  portgfx::GlContext::MakeCurrent(gl_context_);
  gm_.Reset(new FakeGraphicsManager());
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kVertexArrays));
  gm_->SetForceFunctionFailure("GenVertexArrays", true);
  gm_->InitGlInfo();
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kVertexArrays));
}

TEST_F(FakeGraphicsManagerTest, GetString) {
  EXPECT_EQ("GL_OES_blend_func_separate", GetStringi(GL_EXTENSIONS, 0));
  EXPECT_EQ("GL_OES_blend_subtract", GetStringi(GL_EXTENSIONS, 1));
  GLint count = GetInt(GL_NUM_EXTENSIONS);
  EXPECT_LT(0, count);
  GM_CALL(GetStringi(GL_EXTENSIONS, count - 1));
  GM_ERROR_CALL(GetStringi(GL_EXTENSIONS, count), GL_INVALID_VALUE);

  // These tests are to increase coverage.
  EXPECT_TRUE(gm_->IsExtensionSupported("mapbuffer"));
  EXPECT_TRUE(gm_->IsExtensionSupported("texture_filter_anisotropic"));
  gm_->SetExtensionsString("test extensions");
  EXPECT_FALSE(gm_->IsExtensionSupported("mapbuffer"));
  EXPECT_FALSE(gm_->IsExtensionSupported("texture_filter_anisotropic"));
  EXPECT_EQ("test extensions", GetString(GL_EXTENSIONS));
  EXPECT_EQ("Google", GetString(GL_VENDOR));
  gm_->SetVendorString("I like turtles");
  EXPECT_EQ("I like turtles", GetString(GL_VENDOR));
  EXPECT_EQ("Ion fake OpenGL / ES", GetString(GL_RENDERER));
  EXPECT_EQ("3.3 Ion OpenGL / ES", GetString(GL_VERSION));
  gm_->SetVersionString("test version");
  EXPECT_EQ("test version", GetString(GL_VERSION));
  EXPECT_EQ("1.10 Ion", GetString(GL_SHADING_LANGUAGE_VERSION));
  GM_ERROR_CALL(GetString(GL_CULL_FACE_MODE), GL_INVALID_ENUM);
  GM_ERROR_CALL(GetString(GL_FRONT), GL_INVALID_ENUM);

  gm_->SetForceFunctionFailure("GetString", true);
  gm_->SetExtensionsString("GLX_SGI_swap_control GL_OES_blend_func_separate");
  count = GetInt(GL_NUM_EXTENSIONS);
  EXPECT_EQ(2, count);

  EXPECT_EQ("GLX_SGI_swap_control", GetStringi(GL_EXTENSIONS, 0));
  EXPECT_EQ("GL_OES_blend_func_separate", GetStringi(GL_EXTENSIONS, 1));
  gm_->SetForceFunctionFailure("GetString", false);
}

TEST_F(FakeGraphicsManagerTest, ProgramAndShaderFunctions) {
  // There is no default program.
  GM_ERROR_CALL(AttachShader(0U, 0U), GL_INVALID_VALUE);

  // GetShaderPrecisionFormat.
  GM_ERROR_CALL(
      GetShaderPrecisionFormat(GL_RED, GL_HIGH_FLOAT, nullptr, nullptr),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(
      GetShaderPrecisionFormat(GL_VERTEX_SHADER, GL_RGB, nullptr, nullptr),
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

  GLuint pid = gm_->CreateProgram();
  GLuint pid2 = gm_->CreateProgram();
  EXPECT_NE(0U, pid);
  EXPECT_NE(0U, pid2);
  EXPECT_EQ(GL_FALSE, gm_->IsProgram(0));
  EXPECT_EQ(GL_TRUE, gm_->IsProgram(pid));
  EXPECT_EQ(GL_TRUE, gm_->IsProgram(pid2));
  EXPECT_EQ(GL_FALSE, gm_->IsProgram(pid + pid2));

  GM_CHECK_NO_ERROR;
  GLuint vid = gm_->CreateShader(GL_VERTEX_SHADER);
  GLuint vid2 = gm_->CreateShader(GL_VERTEX_SHADER);
  GM_CHECK_NO_ERROR;
  EXPECT_NE(0U, vid);
  EXPECT_NE(0U, vid2);
  GLuint gid = gm_->CreateShader(GL_GEOMETRY_SHADER);
  GLuint gid2 = gm_->CreateShader(GL_GEOMETRY_SHADER);
  GM_CHECK_NO_ERROR;
  EXPECT_NE(0U, gid);
  EXPECT_NE(0U, gid2);
  GLuint fid = gm_->CreateShader(GL_FRAGMENT_SHADER);
  GLuint fid2 = gm_->CreateShader(GL_FRAGMENT_SHADER);
  GM_CHECK_NO_ERROR;
  EXPECT_NE(0U, fid);
  EXPECT_NE(0U, fid2);
  // Invalid enum returns 0 for the shader id.
  GLuint bad_id = gm_->CreateShader(GL_FRONT);
  EXPECT_EQ(0U, bad_id);
  GM_CHECK_ERROR(GL_INVALID_ENUM);
  EXPECT_EQ(GL_FALSE, gm_->IsShader(0));
  EXPECT_EQ(GL_TRUE, gm_->IsShader(vid));
  EXPECT_EQ(GL_TRUE, gm_->IsShader(vid2));
  EXPECT_EQ(GL_TRUE, gm_->IsShader(gid));
  EXPECT_EQ(GL_TRUE, gm_->IsShader(gid2));
  EXPECT_EQ(GL_TRUE, gm_->IsShader(fid));
  EXPECT_EQ(GL_TRUE, gm_->IsShader(fid2));
  EXPECT_EQ(GL_FALSE, gm_->IsShader(vid + vid2 + gid + gid2 + fid + fid2));

  // Invalid program ints.
  GM_ERROR_CALL(GetShaderiv(0U, 0U, nullptr), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetShaderiv(8U, 0U, nullptr), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetShaderiv(vid, GL_RENDERER, nullptr), GL_INVALID_ENUM);

  // Check program and shader ints.
  EXPECT_EQ(GL_FALSE, GetProgramInt(pid, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetProgramInt(pid, GL_LINK_STATUS));
  EXPECT_EQ(GL_FALSE, GetProgramInt(pid2, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetProgramInt(pid2, GL_LINK_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(vid, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(vid, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_VERTEX_SHADER, GetShaderInt(vid, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(vid, GL_SHADER_SOURCE_LENGTH));
  EXPECT_EQ(GL_FALSE, GetShaderInt(vid2, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(vid2, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_VERTEX_SHADER, GetShaderInt(vid2, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(vid2, GL_SHADER_SOURCE_LENGTH));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gid, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gid, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_GEOMETRY_SHADER, GetShaderInt(gid, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(gid, GL_SHADER_SOURCE_LENGTH));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gid2, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(gid2, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_GEOMETRY_SHADER, GetShaderInt(gid2, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(gid2, GL_SHADER_SOURCE_LENGTH));
  EXPECT_EQ(GL_FALSE, GetShaderInt(fid, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(fid, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_FRAGMENT_SHADER, GetShaderInt(fid, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(fid, GL_SHADER_SOURCE_LENGTH));
  EXPECT_EQ(GL_FALSE, GetShaderInt(fid2, GL_DELETE_STATUS));
  EXPECT_EQ(GL_FALSE, GetShaderInt(fid2, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_FRAGMENT_SHADER, GetShaderInt(fid2, GL_SHADER_TYPE));
  EXPECT_EQ(0, GetShaderInt(fid2, GL_SHADER_SOURCE_LENGTH));

  const std::string vertex_source(kVertexSource);
  const std::string geometry_source(kGeometrySource);
  const std::string fragment_source(kFragmentSource);

  // Cannot compile invalid shaders.
  GM_ERROR_CALL(CompileShader(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(CompileShader(11U), GL_INVALID_VALUE);
  GM_ERROR_CALL(ShaderSource(0U, 0, nullptr, nullptr), GL_INVALID_VALUE);
  GM_ERROR_CALL(ShaderSource(7U, 0, nullptr, nullptr), GL_INVALID_VALUE);
  GM_ERROR_CALL(ShaderSource(vid, -1, nullptr, nullptr), GL_INVALID_VALUE);
  // Valid source.
  {
    GLint length = static_cast<GLuint>(vertex_source.length());
    const char* ptr = vertex_source.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
  }
  {
    GLint length = static_cast<GLuint>(geometry_source.length());
    const char* ptr = geometry_source.c_str();
    GM_CALL(ShaderSource(gid, 1, &ptr, &length));
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
    GM_ERROR_CALL(GetShaderSource(0U, 0, nullptr, nullptr), GL_INVALID_VALUE);
    GM_ERROR_CALL(GetShaderSource(7U, 0, nullptr, nullptr), GL_INVALID_VALUE);
    GM_ERROR_CALL(GetShaderSource(vid, -1, nullptr, nullptr), GL_INVALID_VALUE);
    // Check vertex source.
    GM_CALL(GetShaderSource(vid, kBufLen, &length, source));
    EXPECT_EQ(static_cast<GLint>(vertex_source.length()) + 1, length);
    EXPECT_EQ(0, vertex_source.compare(source));
    // Check geometry source.
    GM_CALL(GetShaderSource(gid, kBufLen, &length, source));
    EXPECT_EQ(static_cast<GLint>(geometry_source.length()) + 1, length);
    EXPECT_EQ(0, geometry_source.compare(source));
    // Check fragment source.
    GM_CALL(GetShaderSource(fid, kBufLen, &length, source));
    EXPECT_EQ(static_cast<GLint>(fragment_source.length()) + 1, length);
    EXPECT_EQ(0, fragment_source.compare(source));

    EXPECT_EQ(static_cast<GLint>(vertex_source.length()) + 1,
              GetShaderInt(vid, GL_SHADER_SOURCE_LENGTH));
    EXPECT_EQ(static_cast<GLint>(geometry_source.length()) + 1,
              GetShaderInt(gid, GL_SHADER_SOURCE_LENGTH));
    EXPECT_EQ(static_cast<GLint>(fragment_source.length()) + 1,
              GetShaderInt(fid, GL_SHADER_SOURCE_LENGTH));
  }

  // Try to compile shaders.
  GM_CALL(CompileShader(vid));
  GM_CALL(CompileShader(gid));
  GM_CALL(CompileShader(fid));
  EXPECT_EQ(GL_TRUE, GetShaderInt(vid, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_TRUE, GetShaderInt(gid, GL_COMPILE_STATUS));
  EXPECT_EQ(GL_TRUE, GetShaderInt(fid, GL_COMPILE_STATUS));

  // Cannot link a program that does not have valid shaders.
  GM_ERROR_CALL(LinkProgram(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(LinkProgram(pid + pid2), GL_INVALID_VALUE);
  // Cannot validate an invalid program.
  GM_ERROR_CALL(ValidateProgram(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(ValidateProgram(fid + fid2 + gid + gid2 + vid + vid2),
                GL_INVALID_VALUE);

  // Check error case.
  GM_ERROR_CALL(GetProgramiv(pid, GL_TEXTURE_2D, nullptr), GL_INVALID_ENUM);

  // There should be no shaders attached at first.
  EXPECT_EQ(0, GetProgramInt(pid, GL_ATTACHED_SHADERS));

  // Invalid value is set if an invalid value is used.
  GM_ERROR_CALL(AttachShader(pid + pid2, vid), GL_INVALID_VALUE);
  GM_ERROR_CALL(AttachShader(pid, 0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(AttachShader(0U, vid), GL_INVALID_VALUE);
  EXPECT_EQ(0, GetProgramInt(pid, GL_ATTACHED_SHADERS));

  {
    // GetAttachedShaders.
    GLsizei count;
    GLuint shaders[3];
    // Bad calls.
    GM_ERROR_CALL(GetAttachedShaders(0U, 3, &count, shaders), GL_INVALID_VALUE);
    GM_ERROR_CALL(
        GetAttachedShaders(pid, -1, &count, shaders), GL_INVALID_VALUE);

    GM_CALL(GetAttachedShaders(pid, 3, &count, shaders));
    EXPECT_EQ(0, count);

    // Actually attach the shader.
    GM_CALL(AttachShader(pid, vid));
    EXPECT_EQ(1, GetProgramInt(pid, GL_ATTACHED_SHADERS));
    GM_CALL(GetAttachedShaders(pid, 3, &count, shaders));
    EXPECT_EQ(1, count);
    EXPECT_EQ(vid, shaders[0]);

    // Attaching a shader twice is an invalid operation.
    GM_ERROR_CALL(AttachShader(pid, vid), GL_INVALID_OPERATION);
    EXPECT_EQ(1, GetProgramInt(pid, GL_ATTACHED_SHADERS));
    GM_CALL(GetAttachedShaders(pid, 3, &count, shaders));
    EXPECT_EQ(1, count);
    EXPECT_EQ(vid, shaders[0]);

    // Attach two more shaders.
    GM_CALL(AttachShader(pid, gid));
    EXPECT_EQ(2, GetProgramInt(pid, GL_ATTACHED_SHADERS));
    GM_CALL(GetAttachedShaders(pid, 3, &count, shaders));
    EXPECT_EQ(2, count);
    EXPECT_EQ(vid, shaders[0]);
    EXPECT_EQ(gid, shaders[1]);

    GM_CALL(AttachShader(pid, fid));
    EXPECT_EQ(3, GetProgramInt(pid, GL_ATTACHED_SHADERS));
    GM_CALL(GetAttachedShaders(pid, 3, &count, shaders));
    EXPECT_EQ(3, count);
    EXPECT_EQ(vid, shaders[0]);
    EXPECT_EQ(gid, shaders[1]);
    EXPECT_EQ(fid, shaders[2]);
  }

  // Can't use an unlinked program.
  GM_ERROR_CALL(UseProgram(pid2), GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetUniformfv(pid2, 0, nullptr), GL_INVALID_OPERATION);

  // Link the program.
  EXPECT_EQ(GL_FALSE, GetProgramInt(pid, GL_LINK_STATUS));
  GM_CALL(LinkProgram(pid));
  EXPECT_EQ(GL_TRUE, GetProgramInt(pid, GL_LINK_STATUS));
  EXPECT_EQ(GL_FALSE, GetProgramInt(pid, GL_VALIDATE_STATUS));
  GM_CALL(ValidateProgram(pid));
  EXPECT_EQ(GL_TRUE, GetProgramInt(pid, GL_VALIDATE_STATUS));

  // The default program is none.
  EXPECT_EQ(0, GetInt(GL_CURRENT_PROGRAM));

  // Can't set an invalid program.
  GM_ERROR_CALL(UseProgram(5U), GL_INVALID_VALUE);

  // Set a valid program.
  GM_CALL(UseProgram(pid));
  EXPECT_EQ(pid, static_cast<GLuint>(GetInt(GL_CURRENT_PROGRAM)));
  GM_CALL(UseProgram(0));
  EXPECT_EQ(0, GetInt(GL_CURRENT_PROGRAM));
  GM_CALL(UseProgram(pid));
  EXPECT_EQ(pid, static_cast<GLuint>(GetInt(GL_CURRENT_PROGRAM)));

  // Can't get log of invalids.
  GM_ERROR_CALL(GetShaderInfoLog(0U, 0, nullptr, nullptr), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetShaderInfoLog(vid + vid2 + fid + fid2, 0, nullptr, nullptr),
                GL_INVALID_VALUE);
  GM_ERROR_CALL(GetProgramInfoLog(0U, 0, nullptr, nullptr), GL_INVALID_VALUE);
  GM_ERROR_CALL(GetProgramInfoLog(pid + pid2, 0, nullptr, nullptr),
                GL_INVALID_VALUE);

  {
    // Validate calls, but we don't support compilation, so the logs are nullptr
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
  EXPECT_EQ(0, GetProgramInt(pid, GL_INFO_LOG_LENGTH));
  EXPECT_EQ(0, GetShaderInt(vid, GL_INFO_LOG_LENGTH));

  // Deleting invalid ids sets an invalid value error.
  GM_ERROR_CALL(DeleteShader(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(DeleteShader(vid + vid2 + fid + fid2), GL_INVALID_VALUE);
  GM_ERROR_CALL(DeleteProgram(0U), GL_INVALID_VALUE);
  GM_ERROR_CALL(DeleteProgram(pid + pid2), GL_INVALID_VALUE);

  // Delete a valid program and shader.
  GLint dummy = -1;
  GM_CALL(DeleteProgram(pid2));
  EXPECT_FALSE(gm_->IsProgram(pid2));
  // This should fail, since the program is immediately deleted.
  GM_ERROR_CALL(GetProgramiv(pid2, GL_DELETE_STATUS, &dummy),
                GL_INVALID_OPERATION);
  GM_CALL(DeleteShader(vid2));
  EXPECT_FALSE(gm_->IsShader(vid2));
  GM_ERROR_CALL(GetShaderiv(vid2, GL_DELETE_STATUS, &dummy),
                GL_INVALID_OPERATION);
  // Can't set the source of a deleted shader.
  GM_ERROR_CALL(ShaderSource(vid2, 0, nullptr, nullptr), GL_INVALID_OPERATION);
  // Can't compile a deleted shader.
  GM_ERROR_CALL(CompileShader(vid2), GL_INVALID_OPERATION);
  // Can't get a uniform location of a deleted program.
  GM_ERROR_CALL(GetUniformLocation(pid2, "uni_v2f"), GL_INVALID_OPERATION);

  // Can't link a deleted program.
  GM_ERROR_CALL(LinkProgram(pid2), GL_INVALID_OPERATION);
  // Can't use a deleted program.
  GM_ERROR_CALL(UseProgram(pid2), GL_INVALID_OPERATION);
  // Can't validate a deleted program.
  GM_ERROR_CALL(ValidateProgram(pid2), GL_INVALID_OPERATION);

  // Check attribute and uniform counts.
  EXPECT_EQ(7, GetProgramInt(pid, GL_ACTIVE_ATTRIBUTES));
  GM_ERROR_CALL(GetProgramiv(pid2, GL_ACTIVE_ATTRIBUTES, &dummy),
                GL_INVALID_OPERATION);
  EXPECT_EQ(88, GetProgramInt(pid, GL_ACTIVE_UNIFORMS));
  GM_ERROR_CALL(GetProgramiv(pid2, GL_ACTIVE_UNIFORMS, &dummy),
                GL_INVALID_OPERATION);
  // Valid attribute max length.
  EXPECT_EQ(9, GetProgramInt(pid, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH));
  // Valid uniform max length.
  EXPECT_EQ(14, GetProgramInt(pid, GL_ACTIVE_UNIFORM_MAX_LENGTH));

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
  EXPECT_EQ(-1, gm_->GetAttribLocation(pid, "name"));
  EXPECT_EQ(-1, gm_->GetAttribLocation(pid, "gl_Position"));
  EXPECT_EQ(0, gm_->GetAttribLocation(pid, "attr_f"));
  EXPECT_EQ(1, gm_->GetAttribLocation(pid, "attr_v2f"));
  EXPECT_EQ(2, gm_->GetAttribLocation(pid, "attr_v3f"));
  EXPECT_EQ(3, gm_->GetAttribLocation(pid, "attr_v4f"));
  // For matrix attributes, the returned location is the index of the first
  // column of the matrix.
  EXPECT_EQ(4, gm_->GetAttribLocation(pid, "attr_m2f"));
  EXPECT_EQ(6, gm_->GetAttribLocation(pid, "attr_m3f"));
  EXPECT_EQ(9, gm_->GetAttribLocation(pid, "attr_m4f"));
  GM_CHECK_NO_ERROR;

  // Check that no additional attributes were added.
  EXPECT_EQ(7, GetProgramInt(pid, GL_ACTIVE_ATTRIBUTES));

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

TEST_F(FakeGraphicsManagerTest, Uniforms) {
  const std::string vertex_source(kVertexSource);
  const std::string geometry_source(kGeometrySource);
  const std::string fragment_source(kFragmentSource);
  GLuint vid = gm_->CreateShader(GL_VERTEX_SHADER);
  GLuint gid = gm_->CreateShader(GL_GEOMETRY_SHADER);
  GLuint fid = gm_->CreateShader(GL_FRAGMENT_SHADER);
  GLuint vid2 = gm_->CreateShader(GL_VERTEX_SHADER);
  GLuint gid2 = gm_->CreateShader(GL_GEOMETRY_SHADER);
  GLuint fid2 = gm_->CreateShader(GL_FRAGMENT_SHADER);
  GM_CHECK_NO_ERROR;
  {
    GLint length = static_cast<GLuint>(vertex_source.length());
    const char* ptr = vertex_source.c_str();
    GM_CALL(ShaderSource(vid, 1, &ptr, &length));
  }
  {
    GLint length = static_cast<GLuint>(geometry_source.length());
    const char* ptr = geometry_source.c_str();
    GM_CALL(ShaderSource(gid, 1, &ptr, &length));
  }
  {
    GLint length = static_cast<GLuint>(fragment_source.length());
    const char* ptr = fragment_source.c_str();
    GM_CALL(ShaderSource(fid, 1, &ptr, &length));
  }
  GLuint pid = gm_->CreateProgram();
  GLuint pid2 = gm_->CreateProgram();
  GM_CALL(CompileShader(vid));
  GM_CALL(CompileShader(fid));
  GM_CALL(AttachShader(pid, vid));
  GM_CALL(AttachShader(pid, gid));
  GM_CALL(AttachShader(pid, fid));
  GM_CALL(LinkProgram(pid));
  GM_CALL(UseProgram(pid));

  // Uniform tests.
  GM_ERROR_CALL(GetUniformLocation(pid2, "uni_v2f"), GL_INVALID_OPERATION);
  GM_ERROR_CALL(GetUniformLocation(0U, "uni_v2f"), GL_INVALID_VALUE);
  EXPECT_EQ(-1, gm_->GetUniformLocation(0U, "attr_f"));
  gm_->GetError();  // Clear the error.

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
      {-1, -1, -1, -1}},
    {"guni_i", GL_INT, 1, UniformInfo::kInt, -1, {-1, -1, -1, -1}},
    {"guni_u", GL_UNSIGNED_INT, 1, UniformInfo::kUnsignedInt, -1,
      {-1, -1, -1, -1}}
  };
  static const int kNumUniforms = static_cast<int>(ABSL_ARRAYSIZE(uniforms));

  // Get uniform locations.
  for (int i = 0; i < kNumUniforms; ++i) {
    SCOPED_TRACE(::testing::Message() << i << ": " << uniforms[i].name);
    uniforms[i].loc = gm_->GetUniformLocation(pid, uniforms[i].name);
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
        GetActiveUniform(pid, GetProgramInt(pid, GL_ACTIVE_UNIFORMS),
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
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform1f,
                    &GraphicsManager::Uniform1fv);
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform1i,
                    &GraphicsManager::Uniform1iv);
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform1ui,
                    &GraphicsManager::Uniform1uiv);
      } else if (length == 2) {
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform2f,
                    &GraphicsManager::Uniform2fv);
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform2i,
                    &GraphicsManager::Uniform2iv);
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform2ui,
                    &GraphicsManager::Uniform2uiv);
      } else if (length == 3) {
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform3f,
                    &GraphicsManager::Uniform3fv);
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform3i,
                    &GraphicsManager::Uniform3iv);
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform3ui,
                    &GraphicsManager::Uniform3uiv);
      } else {  // length == 4.
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform4f,
                    &GraphicsManager::Uniform4fv);
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform4i,
                    &GraphicsManager::Uniform4iv);
        TestUniform(info, gm_, pid, length, 1, UniformInfo::kUnsignedInt,
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
  GM_ERROR_CALL(DetachShader(pid, gid2), GL_INVALID_OPERATION);
  GM_ERROR_CALL(DetachShader(pid, fid2), GL_INVALID_OPERATION);
  GM_ERROR_CALL(DetachShader(pid2, vid2), GL_INVALID_OPERATION);

  // Detach valid shaders.
  EXPECT_EQ(3, GetProgramInt(pid, GL_ATTACHED_SHADERS));
  GM_CALL(DetachShader(pid, vid));
  EXPECT_EQ(2, GetProgramInt(pid, GL_ATTACHED_SHADERS));
  GM_CALL(DetachShader(pid, gid));
  EXPECT_EQ(1, GetProgramInt(pid, GL_ATTACHED_SHADERS));
  GM_CALL(DetachShader(pid, fid));
  EXPECT_EQ(0, GetProgramInt(pid, GL_ATTACHED_SHADERS));

  // The default program should get reset to none.
  GM_CALL(DeleteProgram(pid));
  EXPECT_TRUE(gm_->IsProgram(pid));
  EXPECT_EQ(static_cast<GLint>(pid), GetInt(GL_CURRENT_PROGRAM));

  // For coverage.
  GM_ERROR_CALL(ReleaseShaderCompiler(), GL_INVALID_OPERATION);
  GM_ERROR_CALL(ShaderBinary(0, nullptr, 0U, nullptr, 0), GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, UniformArrays) {
  const std::string vertex_source(kVertexSource);
  const std::string fragment_source(kFragmentSource);
  GLuint vid = gm_->CreateShader(GL_VERTEX_SHADER);
  GLuint fid = gm_->CreateShader(GL_FRAGMENT_SHADER);
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
  GLuint pid = gm_->CreateProgram();
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
  static const int kNumUniforms = static_cast<size_t>(ABSL_ARRAYSIZE(uniforms));

  // Get uniform array locations.
  for (int i = 0; i < kNumUniforms; ++i) {
    uniforms[i].loc = gm_->GetUniformLocation(pid, uniforms[i].name);
    GM_CHECK_NO_ERROR;
    for (int j = 0; j < 4; ++j) {
      SCOPED_TRACE(::testing::Message() << i << ": " << uniforms[i].name << "["
                                        << j << "]");
      std::ostringstream str;
      str << uniforms[i].name << "[" << j << "]";
      uniforms[i].alocs[j] = gm_->GetUniformLocation(pid, str.str().c_str());
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
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform1f,
                    &GraphicsManager::Uniform1fv);
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform1i,
                    &GraphicsManager::Uniform1iv);
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform1ui,
                    &GraphicsManager::Uniform1uiv);
      } else if (length == 2) {
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform2f,
                    &GraphicsManager::Uniform2fv);
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform2i,
                    &GraphicsManager::Uniform2iv);
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform2ui,
                    &GraphicsManager::Uniform2uiv);
      } else if (length == 3) {
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform3f,
                    &GraphicsManager::Uniform3fv);
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform3i,
                    &GraphicsManager::Uniform3iv);
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kUnsignedInt,
                    &GraphicsManager::GetUniformuiv,
                    &GraphicsManager::Uniform3ui,
                    &GraphicsManager::Uniform3uiv);
      } else {  // length == 4.
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kFloat,
                    &GraphicsManager::GetUniformfv, &GraphicsManager::Uniform4f,
                    &GraphicsManager::Uniform4fv);
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kInt,
                    &GraphicsManager::GetUniformiv, &GraphicsManager::Uniform4i,
                    &GraphicsManager::Uniform4iv);
        TestUniform(info, gm_, pid, length, 4, UniformInfo::kUnsignedInt,
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

TEST_F(FakeGraphicsManagerTest, ImageExternal) {
  gm_->EGLImageTargetTexture2DOES(GL_TEXTURE_2D, nullptr);
}

TEST_F(FakeGraphicsManagerTest, ShaderPreprocessor) {
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

  GLuint vid = gm_->CreateShader(GL_VERTEX_SHADER);
  GLuint fid = gm_->CreateShader(GL_FRAGMENT_SHADER);
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
  GLuint pid = gm_->CreateProgram();
  GM_CALL(CompileShader(vid));
  GM_CALL(CompileShader(fid));
  GM_CALL(AttachShader(pid, vid));
  GM_CALL(AttachShader(pid, fid));
  GM_CALL(LinkProgram(pid));
  GM_CALL(UseProgram(pid));
  GM_CHECK_NO_ERROR;

  EXPECT_EQ(-1, gm_->GetUniformLocation(pid, "uNotAvailableV1"));
  EXPECT_EQ(-1, gm_->GetUniformLocation(pid, "uNotAvailableV2"));
  EXPECT_EQ(-1, gm_->GetUniformLocation(pid, "uNotAvailableV3"));
  EXPECT_EQ(-1, gm_->GetUniformLocation(pid, "uNotAvailableF1"));
  EXPECT_EQ(-1, gm_->GetUniformLocation(pid, "uNotAvailableF2"));
  EXPECT_EQ(-1, gm_->GetUniformLocation(pid, "uNotAvailableF3"));

  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableV1"));
  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableV2"));
  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableV3"));
  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableV4"));
  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableF1"));
  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableF2"));
  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableF3"));
  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableF4"));
  EXPECT_NE(-1, gm_->GetUniformLocation(pid, "uAvailableF5"));
  GM_CHECK_NO_ERROR;
}

TEST_F(FakeGraphicsManagerTest, ShaderPreprocessorUnsupportedFeatures) {
  // The shader preprocessor does not support all features. Upon reading an
  // unsupported clause, we should print a warning and not crash.
  // Boilerplate fragment shader. We just test using the vertex shader.
  const std::string fragment_source("\n");
  GLuint fid = gm_->CreateShader(GL_FRAGMENT_SHADER);
  {
    GLint length = static_cast<GLuint>(fragment_source.length());
    const char* ptr = fragment_source.c_str();
    GM_CALL(ShaderSource(fid, 1, &ptr, &length));
  }
  GM_CALL(CompileShader(fid));

  // Make sure we print a warning and don't crash if we run into #if.
  {
    base::LogChecker log_checker;
    GLuint pid = gm_->CreateProgram();
    GLuint vid = gm_->CreateShader(GL_VERTEX_SHADER);
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
    GLuint pid = gm_->CreateProgram();
    GLuint vid = gm_->CreateShader(GL_VERTEX_SHADER);
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
    GLuint pid = gm_->CreateProgram();
    GLuint vid = gm_->CreateShader(GL_VERTEX_SHADER);
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

TEST_F(FakeGraphicsManagerTest, PlatformCapabilities) {
  GLfloat f4[4];
  GLint i2[2];

  // Defaults.
  EXPECT_EQ(math::Range1f(1.f, 256.f), gm_->GetAliasedLineWidthRange());
  EXPECT_EQ(math::Range1f(1.f, 8192.f), gm_->GetAliasedPointSizeRange());
  EXPECT_EQ(4096, gm_->GetMax3dTextureSize());
  EXPECT_EQ(4096, gm_->GetMaxArrayTextureLayers());
  EXPECT_EQ(96U, gm_->GetMaxCombinedTextureImageUnits());
  EXPECT_EQ(8192, gm_->GetMaxCubeMapTextureSize());
  EXPECT_EQ(1024U, gm_->GetMaxFragmentUniformComponents());
  EXPECT_EQ(256U, gm_->GetMaxFragmentUniformVectors());
  EXPECT_EQ(4096, gm_->GetMaxRenderbufferSize());
  EXPECT_EQ(16, gm_->GetMaxSamples());
  EXPECT_EQ(32U, gm_->GetMaxTextureImageUnits());
  EXPECT_EQ(8192, gm_->GetMaxTextureSize());
  EXPECT_EQ(15U, gm_->GetMaxVaryingVectors());
  EXPECT_EQ(32U, gm_->GetMaxVertexAttribs());
  EXPECT_EQ(32U, gm_->GetMaxVertexTextureImageUnits());
  EXPECT_EQ(1536U, gm_->GetMaxVertexUniformComponents());
  EXPECT_EQ(384U, gm_->GetMaxVertexUniformVectors());
  EXPECT_EQ(math::Point2i(8192, 8192), gm_->GetMaxViewportDims());

  // Set values and check that GL returns them.
  gm_->SetAliasedLineWidthRange(math::Range1f(0.5f, 12.f));
  EXPECT_EQ(math::Range1f(0.5f, 12.f), gm_->GetAliasedLineWidthRange());
  GM_CALL(GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, f4));
  EXPECT_EQ(0.5f, f4[0]);
  GM_CALL(GetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, f4));
  EXPECT_EQ(12.f, f4[1]);
  EXPECT_EQ(math::Range1f(0.5f, 12.f),
            gm_->GetConstant<math::Range1f>(
                GraphicsManager::kAliasedLineWidthRange));

  gm_->SetAliasedPointSizeRange(math::Range1f(0.25f, 31.f));
  EXPECT_EQ(math::Range1f(0.25f, 31.f), gm_->GetAliasedPointSizeRange());
  GM_CALL(GetFloatv(GL_ALIASED_POINT_SIZE_RANGE, f4));
  EXPECT_EQ(0.25f, f4[0]);
  GM_CALL(GetFloatv(GL_ALIASED_POINT_SIZE_RANGE, f4));
  EXPECT_EQ(31.f, f4[1]);
  EXPECT_EQ(math::Range1f(0.25f, 31.f),
            gm_->GetConstant<math::Range1f>(
                GraphicsManager::kAliasedPointSizeRange));

  gm_->SetMax3dTextureSize(256);
  EXPECT_EQ(256, gm_->GetMax3dTextureSize());
  EXPECT_EQ(256, GetInt(GL_MAX_3D_TEXTURE_SIZE));
  EXPECT_EQ(256, gm_->GetConstant<int>(GraphicsManager::kMax3dTextureSize));

  gm_->SetMaxArrayTextureLayers(320);
  EXPECT_EQ(320, gm_->GetMaxArrayTextureLayers());
  EXPECT_EQ(320, GetInt(GL_MAX_ARRAY_TEXTURE_LAYERS));
  EXPECT_EQ(320, gm_->GetConstant<int>(
                     GraphicsManager::kMaxArrayTextureLayers));

  GM_ERROR_CALL(Enable(GL_CLIP_DISTANCE0 + 15), GL_INVALID_ENUM);
  gm_->SetMaxClipDistances(16);
  EXPECT_EQ(16U, gm_->GetMaxClipDistances());
  EXPECT_EQ(16, GetInt(GL_MAX_CLIP_DISTANCES));
  EXPECT_EQ(16, gm_->GetConstant<int>(GraphicsManager::kMaxClipDistances));
  GM_CALL(Enable(GL_CLIP_DISTANCE0 + 15));

  gm_->SetMaxCombinedTextureImageUnits(11U);
  EXPECT_EQ(11U, gm_->GetMaxCombinedTextureImageUnits());
  EXPECT_EQ(11, GetInt(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(11, gm_->GetConstant<int>(
                    GraphicsManager::kMaxCombinedTextureImageUnits));

  gm_->SetMaxCubeMapTextureSize(2048);
  EXPECT_EQ(2048, gm_->GetMaxCubeMapTextureSize());
  EXPECT_EQ(2048, GetInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE));
  EXPECT_EQ(2048, gm_->GetConstant<int>(
                      GraphicsManager::kMaxCubeMapTextureSize));

  gm_->SetMaxFragmentUniformComponents(5896U);
  EXPECT_EQ(5896U, gm_->GetMaxFragmentUniformComponents());
  EXPECT_EQ(5896, GetInt(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS));
  EXPECT_EQ(5896, gm_->GetConstant<int>(
                      GraphicsManager::kMaxFragmentUniformComponents));

  gm_->SetMaxFragmentUniformVectors(8000U);
  EXPECT_EQ(8000U, gm_->GetMaxFragmentUniformVectors());
  EXPECT_EQ(8000, GetInt(GL_MAX_FRAGMENT_UNIFORM_VECTORS));
  EXPECT_EQ(8000, gm_->GetConstant<int>(
                      GraphicsManager::kMaxFragmentUniformVectors));

  gm_->SetMaxSamples(534);
  EXPECT_EQ(534, gm_->GetMaxSamples());
  EXPECT_EQ(534, GetInt(GL_MAX_SAMPLES));
  EXPECT_EQ(534, gm_->GetConstant<int>(GraphicsManager::kMaxSamples));

  gm_->SetMaxRenderbufferSize(768);
  EXPECT_EQ(768, gm_->GetMaxRenderbufferSize());
  EXPECT_EQ(768, GetInt(GL_MAX_RENDERBUFFER_SIZE));
  EXPECT_EQ(768, gm_->GetConstant<int>(GraphicsManager::kMaxRenderbufferSize));

  gm_->SetMaxTextureImageUnits(8U);
  EXPECT_EQ(8U, gm_->GetMaxTextureImageUnits());
  EXPECT_EQ(8, GetInt(GL_MAX_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(8, gm_->GetConstant<int>(GraphicsManager::kMaxTextureImageUnits));

  gm_->SetMaxTextureMaxAnisotropy(4.f);
  EXPECT_EQ(4.f, gm_->GetMaxTextureMaxAnisotropy());
  EXPECT_EQ(4, GetInt(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT));
  EXPECT_EQ(4.f, gm_->GetConstant<float>(
                     GraphicsManager::kMaxTextureMaxAnisotropy));

  gm_->SetMaxTextureSize(64);
  EXPECT_EQ(64, gm_->GetMaxTextureSize());
  EXPECT_EQ(64, GetInt(GL_MAX_TEXTURE_SIZE));
  EXPECT_EQ(64, gm_->GetConstant<int>(GraphicsManager::kMaxTextureSize));

  gm_->SetMaxVaryingVectors(3U);
  EXPECT_EQ(3U, gm_->GetMaxVaryingVectors());
  EXPECT_EQ(3, GetInt(GL_MAX_VARYING_VECTORS));
  EXPECT_EQ(3, gm_->GetConstant<int>(GraphicsManager::kMaxVaryingVectors));

  gm_->SetMaxVertexAttribs(16U);
  EXPECT_EQ(16U, gm_->GetMaxVertexAttribs());
  EXPECT_EQ(16, GetInt(GL_MAX_VERTEX_ATTRIBS));
  EXPECT_EQ(16, gm_->GetConstant<int>(GraphicsManager::kMaxVertexAttribs));

  gm_->SetMaxVertexTextureImageUnits(50U);
  EXPECT_EQ(50U, gm_->GetMaxVertexTextureImageUnits());
  EXPECT_EQ(50, GetInt(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS));
  EXPECT_EQ(50, gm_->GetConstant<int>(
                    GraphicsManager::kMaxVertexTextureImageUnits));

  gm_->SetMaxVertexUniformVectors(356U);
  EXPECT_EQ(356U, gm_->GetMaxVertexUniformVectors());
  EXPECT_EQ(356, GetInt(GL_MAX_VERTEX_UNIFORM_VECTORS));
  EXPECT_EQ(356, gm_->GetConstant<int>(
                    GraphicsManager::kMaxVertexUniformVectors));

  gm_->SetMaxVertexUniformComponents(73U);
  EXPECT_EQ(73U, gm_->GetMaxVertexUniformComponents());
  EXPECT_EQ(73, GetInt(GL_MAX_VERTEX_UNIFORM_COMPONENTS));
  EXPECT_EQ(73, gm_->GetConstant<int>(
                    GraphicsManager::kMaxVertexUniformComponents));

  gm_->SetMaxViewportDims(math::Point2i(4096, 2048));
  EXPECT_EQ(math::Point2i(4096, 2048), gm_->GetMaxViewportDims());
  GM_CALL(GetIntegerv(GL_MAX_VIEWPORT_DIMS, i2));
  EXPECT_EQ(4096, i2[0]);
  EXPECT_EQ(2048, i2[1]);
  EXPECT_EQ(math::Point2i(4096, 2048),
            gm_->GetConstant<math::Point2i>(
                GraphicsManager::kMaxViewportDims));
}

TEST_F(FakeGraphicsManagerTest, ErrorChecking) {
  base::LogChecker log_checker;
  gm_->EnableErrorChecking(true);

  // Should be ok.
  gm_->CullFace(GL_BACK);
  EXPECT_FALSE(log_checker.HasAnyMessages());

  //
  // Each of these should produce a single error of a different type.
  //

  gm_->CullFace(GL_TRIANGLES);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid enumerant"));

  gm_->Clear(static_cast<GLbitfield>(12345));
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid value"));

  gm_->Uniform1f(300, 10.0f);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid operation"));

  {
    gm_->SetMaxBufferSize(1024);
    EXPECT_EQ(1024, gm_->GetMaxBufferSize());
    GLuint bo;
    gm_->GenBuffers(1, &bo);
    gm_->BindBuffer(GL_ARRAY_BUFFER, 1U);
    gm_->BufferData(GL_ARRAY_BUFFER, 1026, nullptr, GL_STATIC_DRAW);
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "out of memory"));
    gm_->DeleteBuffers(1, &bo);
  }

  {
    GLuint fbo;
    gm_->GenFramebuffers(1, &fbo);
    gm_->BindFramebuffer(GL_FRAMEBUFFER, fbo);
    uint8 data[10 * 10 * 4];
    gm_->ReadPixels(0, 0, 10, 10, GL_RGBA, GL_UNSIGNED_BYTE, data);
    EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                       "invalid framebuffer operation"));
    gm_->DeleteFramebuffers(1, &fbo);
  }

  gm_->SetErrorCode(GL_TRIANGLES);
  gm_->Clear(0);
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "unknown error"));
}

TEST_F(FakeGraphicsManagerTest, Tracing) {
  base::LogChecker log_checker;

  {
    // The TraceVerifier has to have a shorter scope than the graphics manager.
    TraceVerifier trace_verifier(gm_.Get());
    // Make function calls with different numbers and types of arguments.
    gm_->Flush();
    gm_->ClearDepthf(0.5f);
    gm_->DepthMask(GL_TRUE);
    gm_->CullFace(GL_FRONT);
    gm_->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Make sure strings are quoted and null pointers are handled.
    GLchar source_string[] = "Source string";
    gm_->GetShaderSource(1U, 128, nullptr, source_string);
    gm_->GetUniformLocation(2U, "SomeName");

    // Make sure bizarre values are handled reasonably.
    gm_->DepthMask(13);
    gm_->Clear(GL_DEPTH_BUFFER_BIT | 0x001);
    gm_->MapBufferRange(GL_ARRAY_BUFFER, 2, 4, GL_MAP_READ_BIT | 0x100);
    math::Matrix3f mat(6.2f, 1.8f, 2.6f,
                       -7.4f, -9.2f, 1.3f,
                       -4.1f, 5.3f, -1.9f);
    gm_->UniformMatrix3fv(1, 1, GL_FALSE, mat.Data());

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
    matrix_string << "UniformMatrix3fv(1, 1, GL_FALSE, "
                  << "[6.2; 1.8; 2.6 | -7.4; -9.2; 1.3 | -4.1; 5.3; -1.9])";
    EXPECT_TRUE(trace_verifier.VerifyCallAtIndex(10U, matrix_string.str()));
  }
  // The UniformMatrix3fv is technically an error since there is no program
  // bound.
  gm_.Reset(nullptr);
  gl_context_.Reset(nullptr);
  portgfx::GlContext::MakeCurrent(portgfx::GlContextPtr());
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "destroyed with uncaught"));
}

TEST_F(FakeGraphicsManagerTest, EnableAndDisableFunctionGroups) {
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kCore));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
  gm_->EnableFeature(GraphicsManager::kDiscardFramebuffer, false);
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kCore));
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
  gm_->EnableFeature(GraphicsManager::kCore, false);
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kCore));
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
  gm_->EnableFeature(GraphicsManager::kDiscardFramebuffer, true);
  EXPECT_FALSE(gm_->IsFeatureAvailable(GraphicsManager::kCore));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
  gm_->EnableFeature(GraphicsManager::kCore, true);
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kCore));
  EXPECT_TRUE(gm_->IsFeatureAvailable(GraphicsManager::kDiscardFramebuffer));
}

TEST_F(FakeGraphicsManagerTest, ForceFailures) {
  // Test Gen* failure cases.
  GLuint id = 0U;

  GM_CALL(GenBuffers(1, &id));
  EXPECT_GT(id, 0U);
  gm_->SetForceFunctionFailure("GenBuffers", true);
  id = 0U;
  GM_ERROR_CALL(GenBuffers(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm_->SetForceFunctionFailure("GenBuffers", false);
  GM_CALL(GenBuffers(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenFramebuffers(1, &id));
  EXPECT_GT(id, 0U);
  gm_->SetForceFunctionFailure("GenFramebuffers", true);
  id = 0U;
  GM_ERROR_CALL(GenFramebuffers(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm_->SetForceFunctionFailure("GenFramebuffers", false);
  GM_CALL(GenFramebuffers(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenRenderbuffers(1, &id));
  EXPECT_GT(id, 0U);
  gm_->SetForceFunctionFailure("GenRenderbuffers", true);
  id = 0U;
  GM_ERROR_CALL(GenRenderbuffers(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm_->SetForceFunctionFailure("GenRenderbuffers", false);
  GM_CALL(GenRenderbuffers(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenSamplers(1, &id));
  EXPECT_GT(id, 0U);
  gm_->SetForceFunctionFailure("GenSamplers", true);
  id = 0U;
  GM_ERROR_CALL(GenSamplers(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm_->SetForceFunctionFailure("GenSamplers", false);
  GM_CALL(GenSamplers(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenTextures(1, &id));
  EXPECT_GT(id, 0U);
  gm_->SetForceFunctionFailure("GenTextures", true);
  id = 0U;
  GM_ERROR_CALL(GenTextures(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm_->SetForceFunctionFailure("GenTextures", false);
  GM_CALL(GenTextures(1, &id));
  EXPECT_GT(id, 0U);

  id = 0U;
  GM_CALL(GenVertexArrays(1, &id));
  EXPECT_GT(id, 0U);
  gm_->SetForceFunctionFailure("GenVertexArrays", true);
  id = 0U;
  GM_ERROR_CALL(GenVertexArrays(1, &id), GL_INVALID_OPERATION);
  EXPECT_EQ(0U, id);
  gm_->SetForceFunctionFailure("GenVertexArrays", false);
  GM_CALL(GenVertexArrays(1, &id));
  EXPECT_GT(id, 0U);
}

TEST_F(FakeGraphicsManagerTest, DebugLabels) {
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
  VerifySetAndGetLabel(GL_TEXTURE, id);

  GM_CALL(GenFramebuffers(1, &id));
  VerifySetAndGetLabel(GL_FRAMEBUFFER, id);

  GM_CALL(GenRenderbuffers(1, &id));
  VerifySetAndGetLabel(GL_RENDERBUFFER, id);

  GM_CALL(GenBuffers(1, &id));
  VerifySetAndGetLabel(GL_BUFFER_OBJECT_EXT, id);

  GM_CALL(GenSamplers(1, &id));
  VerifySetAndGetLabel(GL_SAMPLER, id);

  GM_CALL(GenVertexArrays(1, &id));
  VerifySetAndGetLabel(GL_VERTEX_ARRAY_OBJECT_EXT, id);

  id = gm_->CreateProgram();
  VerifySetAndGetLabel(GL_PROGRAM_OBJECT_EXT, id);

  id = gm_->CreateShader(GL_VERTEX_SHADER);
  VerifySetAndGetLabel(GL_SHADER_OBJECT_EXT, id);

  id = gm_->CreateShader(GL_FRAGMENT_SHADER);
  VerifySetAndGetLabel(GL_SHADER_OBJECT_EXT, id);
}

TEST_F(FakeGraphicsManagerTest, DebugMarkers) {
  base::LogChecker log_checker;
  std::string marker("marker");
  // These functions on their own do nothing visible.
  gm_->InsertEventMarker(static_cast<GLsizei>(marker.length()), marker.c_str());
  gm_->PushGroupMarker(static_cast<GLsizei>(marker.length()), marker.c_str());
  gm_->PopGroupMarker();
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(FakeGraphicsManagerTest, DebugOutput) {
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
  // (|max_debug_logged_messages| - 1) to 0, the last one being an
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
  GLuint message_count = gm_->GetDebugMessageLog(
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

TEST_F(FakeGraphicsManagerTest, DrawBuffer) {
  // Invalid enum.
  GM_ERROR_CALL(DrawBuffer(GL_RED), GL_INVALID_ENUM);

  // Successful calls.
  GM_CALL(DrawBuffer(GL_NONE));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER));
  GM_CALL(DrawBuffer(GL_FRONT_AND_BACK));
  EXPECT_EQ(GL_FRONT_AND_BACK, GetInt(GL_DRAW_BUFFER));
  GM_CALL(DrawBuffer(GL_NONE));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER));

  // Test operation on a framebuffer object.
  GLuint fb;
  GM_CALL(GenFramebuffers(1, &fb));
  EXPECT_NE(0U, fb);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, fb));

  GM_CALL(DrawBuffer(GL_COLOR_ATTACHMENT2));
  EXPECT_EQ(GL_COLOR_ATTACHMENT2, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_COLOR_ATTACHMENT2, GetInt(GL_DRAW_BUFFER0));
  GM_ERROR_CALL(DrawBuffer(GL_FRONT_LEFT), GL_INVALID_ENUM);
  EXPECT_EQ(GL_COLOR_ATTACHMENT2, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_COLOR_ATTACHMENT2, GetInt(GL_DRAW_BUFFER0));

  GLint dummy = 0;
  GM_ERROR_CALL(GetIntegerv(GL_DRAW_BUFFER6, &dummy), GL_INVALID_ENUM);

  GM_CALL(DeleteFramebuffers(1, &fb));
}

TEST_F(FakeGraphicsManagerTest, DrawBufferCompleteness) {
  // Verify that setting invalid draw buffers causes FBO incompleteness.
  GLuint fb;
  GM_CALL(GenFramebuffers(1, &fb));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  AllocateAndAttachRenderBuffer(GL_RGBA4, GL_COLOR_ATTACHMENT2, 100, 100);

  GM_CALL(ReadBuffer(GL_COLOR_ATTACHMENT2));
  GLenum draw_buffers[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_COLOR_ATTACHMENT3};
  GM_CALL(DrawBuffers(3, draw_buffers));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER},
            gm_->CheckFramebufferStatus(GL_READ_FRAMEBUFFER));
  GLenum draw_buffers2[] = {GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT2};
  GM_CALL(DrawBuffers(3, draw_buffers2));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_READ_FRAMEBUFFER));
  GM_CALL(DeleteFramebuffers(1, &fb));
}

TEST_F(FakeGraphicsManagerTest, ReadBuffer) {
  // Invalid enums.
  GM_ERROR_CALL(ReadBuffer(GL_RED), GL_INVALID_ENUM);
  GM_ERROR_CALL(ReadBuffer(GL_FRONT_AND_BACK), GL_INVALID_ENUM);

  // Successful calls.
  GM_CALL(ReadBuffer(GL_NONE));
  EXPECT_EQ(GL_NONE, GetInt(GL_READ_BUFFER));
  GM_CALL(ReadBuffer(GL_FRONT_LEFT));
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_READ_BUFFER));

  // Verify that the state is not affected by binding a draw framebuffer.
  GLuint fb;
  GM_CALL(GenFramebuffers(1, &fb));
  EXPECT_NE(0U, fb);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, fb));
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_READ_BUFFER));

  // Test operation on a framebuffer object.
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, fb));
  GM_CALL(ReadBuffer(GL_COLOR_ATTACHMENT2));
  EXPECT_EQ(GL_COLOR_ATTACHMENT2, GetInt(GL_READ_BUFFER));
  GM_ERROR_CALL(ReadBuffer(GL_FRONT_LEFT), GL_INVALID_ENUM);
  EXPECT_EQ(GL_COLOR_ATTACHMENT2, GetInt(GL_READ_BUFFER));
}

TEST_F(FakeGraphicsManagerTest, ReadBufferCompleteness) {
  // Verify that setting an invalid read buffer causes FBO incompleteness.
  GLuint fb;
  GM_CALL(GenFramebuffers(1, &fb));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  AllocateAndAttachRenderBuffer(GL_RGBA4, GL_COLOR_ATTACHMENT2, 100, 100);

  GLenum draw_buffer = GL_COLOR_ATTACHMENT2;
  GM_CALL(DrawBuffers(1, &draw_buffer));
  GM_CALL(ReadBuffer(GL_COLOR_ATTACHMENT1));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER},
            gm_->CheckFramebufferStatus(GL_READ_FRAMEBUFFER));
  GM_CALL(ReadBuffer(GL_COLOR_ATTACHMENT2));
  EXPECT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
            gm_->CheckFramebufferStatus(GL_READ_FRAMEBUFFER));
  GM_CALL(DeleteFramebuffers(1, &fb));
}

TEST_F(FakeGraphicsManagerTest, MaxColorAttachmentsQuery) {
  EXPECT_EQ(4, gm_->GetConstant<int>(GraphicsManager::kMaxColorAttachments));

  // Capability values are cached, so we need a fresh manager.
  gm_.Reset(new FakeGraphicsManager());
  gm_->EnableFeature(GraphicsManager::kMultipleColorAttachments, false);
  EXPECT_EQ(1, gm_->GetConstant<int>(GraphicsManager::kMaxColorAttachments));
}

TEST_F(FakeGraphicsManagerTest, DrawBuffers) {
  gm_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  // Test operation on the default framebuffer.
  GLenum bufs1[] = {GL_NONE, GL_FRONT_LEFT, GL_NONE};
  GM_CALL(DrawBuffers(3, bufs1));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER3));

  GM_CALL(DrawBuffers(0, nullptr));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER3));

  GLenum bufs2[] = {GL_BACK};
  GM_CALL(DrawBuffers(1, bufs2));
  EXPECT_EQ(GL_BACK, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_BACK, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER3));

  GLenum bufs3[] = {GL_FRONT_LEFT, GL_FRONT_RIGHT, GL_BACK_LEFT, GL_BACK_RIGHT};
  GM_CALL(DrawBuffers(4, bufs3));
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_FRONT_RIGHT, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_BACK_LEFT, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_BACK_RIGHT, GetInt(GL_DRAW_BUFFER3));

  GM_ERROR_CALL(DrawBuffers(-1, bufs1), GL_INVALID_ENUM);
  GM_ERROR_CALL(
      DrawBuffers(
          gm_->GetConstant<int>(GraphicsManager::kMaxDrawBuffers) + 1,
          bufs1),
      GL_INVALID_VALUE);
  GLenum bufs4[] = {GL_BACK, GL_NONE};
  GM_ERROR_CALL(DrawBuffers(2, bufs4), GL_INVALID_OPERATION);
  GLenum bufs5[] = {GL_COLOR_ATTACHMENT0, GL_FRONT_LEFT};
  GM_ERROR_CALL(DrawBuffers(2, bufs5), GL_INVALID_ENUM);
  GLenum bufs6[] = {GL_FRONT_LEFT, GL_FRONT_RIGHT, GL_BACK_LEFT, GL_FRONT_LEFT};
  GM_ERROR_CALL(DrawBuffers(4, bufs6), GL_INVALID_OPERATION);
  GLenum bufs7[] = {GL_FRONT_AND_BACK, GL_NONE};
  GM_ERROR_CALL(DrawBuffers(2, bufs7), GL_INVALID_ENUM);

  // Check that error calls do not change the values.
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_FRONT_RIGHT, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_BACK_LEFT, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_BACK_RIGHT, GetInt(GL_DRAW_BUFFER3));

  // Verify that the state is not affected by binding a read framebuffer.
  GLuint fb;
  GM_CALL(GenFramebuffers(1, &fb));
  EXPECT_NE(0U, fb);
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, fb));
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_FRONT_LEFT, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_FRONT_RIGHT, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_BACK_LEFT, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_BACK_RIGHT, GetInt(GL_DRAW_BUFFER3));

  // Test operation on a framebuffer object.
  GM_CALL(BindFramebuffer(GL_READ_FRAMEBUFFER, 0));
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, fb));
  GM_CALL(DrawBuffers(0, nullptr));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER3));

  GLenum bufs8[] = {GL_COLOR_ATTACHMENT1, GL_NONE, GL_COLOR_ATTACHMENT3,
      GL_COLOR_ATTACHMENT0};
  GM_CALL(DrawBuffers(4, bufs8));
  EXPECT_EQ(GL_COLOR_ATTACHMENT1, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_COLOR_ATTACHMENT1, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_COLOR_ATTACHMENT3, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_COLOR_ATTACHMENT0, GetInt(GL_DRAW_BUFFER3));

  GLenum bufs9[] = {GL_BACK};
  GM_ERROR_CALL(DrawBuffers(1, bufs9), GL_INVALID_ENUM);
  GLenum bufs10[] = {GL_NONE, GL_COLOR_ATTACHMENT0, GL_NONE, GL_FRONT_LEFT};
  GM_ERROR_CALL(DrawBuffers(4, bufs10), GL_INVALID_ENUM);
  GLenum bufs11[] = {GL_COLOR_ATTACHMENT2, GL_NONE, GL_NONE,
      GL_COLOR_ATTACHMENT2};
  GM_ERROR_CALL(DrawBuffers(4, bufs11), GL_INVALID_OPERATION);
  GM_ERROR_CALL(DrawBuffers(-91348, bufs11), GL_INVALID_ENUM);
  GM_ERROR_CALL(DrawBuffers(37, bufs11), GL_INVALID_VALUE);

  // Check that error calls do not change the values.
  EXPECT_EQ(GL_COLOR_ATTACHMENT1, GetInt(GL_DRAW_BUFFER));
  EXPECT_EQ(GL_COLOR_ATTACHMENT1, GetInt(GL_DRAW_BUFFER0));
  EXPECT_EQ(GL_NONE, GetInt(GL_DRAW_BUFFER1));
  EXPECT_EQ(GL_COLOR_ATTACHMENT3, GetInt(GL_DRAW_BUFFER2));
  EXPECT_EQ(GL_COLOR_ATTACHMENT0, GetInt(GL_DRAW_BUFFER3));

  GM_CALL(DeleteFramebuffers(1, &fb));
}

TEST_F(FakeGraphicsManagerTest, Sync) {
  // Invalid parameters for fence creation.
  GM_ERROR_CALL(FenceSync(0, 0), GL_INVALID_ENUM);
  GM_ERROR_CALL(FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 1), GL_INVALID_VALUE);

  // Create a sync object properly.
  GLsync sync = gm_->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  GM_CHECK_NO_ERROR;

  // Create a sync object to delete immediately.  This becomes an invalid sync
  // object.
  GLsync invalid_sync = gm_->FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  GM_CHECK_NO_ERROR;
  GM_CALL(DeleteSync(invalid_sync));

  // Invalid parameters for WaitSync.
  GM_ERROR_CALL(WaitSync(nullptr, 0, GL_TIMEOUT_IGNORED), GL_INVALID_OPERATION);
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
  GM_ERROR_CALL(ClientWaitSync(nullptr, 0, 0), GL_INVALID_VALUE);
  GM_ERROR_CALL(ClientWaitSync(invalid_sync, 0, 0), GL_INVALID_VALUE);
  GM_ERROR_CALL(ClientWaitSync(sync, ~0, 0), GL_INVALID_VALUE);

  // Client wait successfully.
  GM_CALL(ClientWaitSync(sync, 0, 0));
  GM_CALL(ClientWaitSync(sync, 0, 10));
  GM_CALL(ClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0));
  GM_CALL(ClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 10));

  // Invalid parameters to GetSynciv.
  GM_ERROR_CALL(
      GetSynciv(nullptr, GL_OBJECT_TYPE, sizeof(value), &length, &value),
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
  GM_CALL(DeleteSync(nullptr)), GM_CALL(DeleteSync(sync));
  GM_ERROR_CALL(DeleteSync(invalid_sync), GL_INVALID_VALUE);
}

TEST_F(FakeGraphicsManagerTest, DisjointTimerQuery) {
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
  GM_ERROR_CALL(GetQueryiv(GL_TIMESTAMP_EXT, GL_QUERY_OBJECT_EXT, &num),
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
  GM_ERROR_CALL(GetQueryObjectiv(ids[0], GL_QUERY_OBJECT_EXT, &num),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetQueryObjectuiv(ids[0], GL_QUERY_OBJECT_EXT, &unum),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetQueryObjecti64v(ids[0], GL_QUERY_OBJECT_EXT, &num64),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(GetQueryObjectui64v(ids[0], GL_QUERY_OBJECT_EXT, &unum64),
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
  EXPECT_TRUE(gm_->IsQuery(ids[0]));
  EXPECT_TRUE(gm_->IsQuery(ids[1]));
  GM_CALL(DeleteQueries(2, ids));
  EXPECT_FALSE(gm_->IsQuery(ids[0]));
  EXPECT_FALSE(gm_->IsQuery(ids[1]));
}

TEST_F(FakeGraphicsManagerTest, TransformFeedbackFunctions) {
  GLuint ids[] = {1, 2};
  GM_CALL(GenTransformFeedbacks(ABSL_ARRAYSIZE(ids), ids));
  EXPECT_FALSE(gm_->IsTransformFeedback(ids[0]));
  EXPECT_FALSE(gm_->IsTransformFeedback(ids[1]));
  GM_CALL(DeleteTransformFeedbacks(ABSL_ARRAYSIZE(ids), ids));
  // Deleted transform feedback objects.
  EXPECT_FALSE(gm_->IsTransformFeedback(ids[0]));
  EXPECT_FALSE(gm_->IsTransformFeedback(ids[1]));
  GM_ERROR_CALL(BindTransformFeedback(GL_TRANSFORM_FEEDBACK, ids[0]),
                GL_INVALID_OPERATION);
  GM_CALL(GenTransformFeedbacks(ABSL_ARRAYSIZE(ids), ids));
  EXPECT_FALSE(gm_->IsTransformFeedback(ids[0]));
  EXPECT_FALSE(gm_->IsTransformFeedback(ids[1]));

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
  GLuint vid = gm_->CreateShader(GL_VERTEX_SHADER);
  GLuint fid = gm_->CreateShader(GL_FRAGMENT_SHADER);
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
  GLuint pid = gm_->CreateProgram();
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

  // Check that transform feedback IDs are not valid in other contexts.
  {
    portgfx::GlContextPtr share_context =
        FakeGlContext::CreateShared(*gl_context_);
    portgfx::GlContext::MakeCurrent(share_context);
    EXPECT_FALSE(gm_->IsTransformFeedback(ids[0]));
    EXPECT_FALSE(gm_->IsTransformFeedback(ids[1]));
    portgfx::GlContext::MakeCurrent(gl_context_);
  }
}

TEST_F(FakeGraphicsManagerTest, InvalidateFramebuffer) {
  // There are two methods to invalidate framebuffer attachments, one included
  // in Open GL ES 3.0, InvalidateFramebuffer, and the second in an extension
  // that adds support prior to Open GL ES 3.0, DiscardFramebufferEXT. This
  // tests both variations, as well as InvalidateSubFramebuffer.

  // InvalidateFramebuffer: Invalid FB target.
  GM_ERROR_CALL(InvalidateFramebuffer(GL_INCR, 0, nullptr), GL_INVALID_ENUM);
  GM_ERROR_CALL(InvalidateFramebuffer(GL_FRAMEBUFFER, -1, nullptr),
                GL_INVALID_VALUE);

  // DiscardFramebufferEXT: Invalid FB target.
  GM_ERROR_CALL(DiscardFramebufferEXT(GL_INCR, 0, nullptr), GL_INVALID_ENUM);
  GM_ERROR_CALL(DiscardFramebufferEXT(GL_FRAMEBUFFER, -1, nullptr),
                GL_INVALID_VALUE);

  // Test for the default framebuffer.
  GLenum default_buffers[] = { GL_COLOR, GL_DEPTH, GL_STENCIL };
  GLenum default_bad_buffers[] = { GL_COLOR, GL_READ_BUFFER, GL_STENCIL };
  GLenum attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                           GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
  GLenum bad_attachments[] = { GL_COLOR_ATTACHMENT0, GL_READ_BUFFER,
                               GL_STENCIL_ATTACHMENT };
  GLenum depth_stencil_attachments[] = { GL_COLOR_ATTACHMENT0,
                                         GL_DEPTH_STENCIL_ATTACHMENT };
  GLenum out_of_range_attachments[] = { GL_DEPTH_ATTACHMENT,
                                        GL_COLOR_ATTACHMENT15 };

  // InvalidateFramebuffer, InvalidateSubFramebuffer: Default framebuffer.
  GM_CALL(InvalidateFramebuffer(GL_FRAMEBUFFER, 3, default_buffers));
  GM_CALL(InvalidateSubFramebuffer(GL_FRAMEBUFFER, 3, default_buffers, 20, 20,
                                   600, 400));
  GM_ERROR_CALL(InvalidateFramebuffer(GL_FRAMEBUFFER, 3, default_bad_buffers),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(InvalidateFramebuffer(GL_FRAMEBUFFER, 3, attachments),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(InvalidateSubFramebuffer(GL_FRAMEBUFFER, 3, default_bad_buffers,
                                         20, 40, 100, 100),
                GL_INVALID_ENUM);

  // DiscardFramebufferEXT: Default framebuffer.
  GM_CALL(DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, default_buffers));
  GM_ERROR_CALL(DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, default_bad_buffers),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, attachments),
                GL_INVALID_ENUM);

  // Bind a draw framebuffer.
  GLuint fb;
  GM_CALL(GenFramebuffers(1, &fb));
  EXPECT_NE(0U, fb);
  GM_CALL(BindFramebuffer(GL_DRAW_FRAMEBUFFER, fb));

  // InvalidateFramebuffer, InvalidateSubFramebuffer: Test operation on a
  // framebuffer object.
  GM_CALL(InvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 4, attachments));
  GM_CALL(InvalidateSubFramebuffer(GL_DRAW_FRAMEBUFFER, 4, attachments, 20, 30,
                                   110, 150));
  GM_ERROR_CALL(InvalidateFramebuffer(GL_FRAMEBUFFER, 3, bad_attachments),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(InvalidateSubFramebuffer(GL_FRAMEBUFFER, 3, bad_attachments,
                                         100, 100, 200, 300),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(InvalidateFramebuffer(GL_FRAMEBUFFER, 2,
                                      depth_stencil_attachments),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(InvalidateFramebuffer(GL_FRAMEBUFFER, 3, default_buffers),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(InvalidateFramebuffer(GL_FRAMEBUFFER, 3,
                                      out_of_range_attachments),
                GL_INVALID_OPERATION);

  // DiscardFramebufferEXT: Test operation on a framebuffer object.
  GM_CALL(DiscardFramebufferEXT(GL_DRAW_FRAMEBUFFER, 4, attachments));
  GM_ERROR_CALL(DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, bad_attachments),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(
      DiscardFramebufferEXT(GL_FRAMEBUFFER, 2, depth_stencil_attachments),
      GL_INVALID_ENUM);
  GM_ERROR_CALL(DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, default_buffers),
                GL_INVALID_ENUM);
  GM_ERROR_CALL(
      DiscardFramebufferEXT(GL_FRAMEBUFFER, 3, out_of_range_attachments),
      GL_INVALID_OPERATION);
}

TEST_F(FakeGraphicsManagerTest, TiledRendering) {
  GLuint fb;
  GM_CALL(GenFramebuffers(1, &fb));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  GM_ERROR_CALL(StartTilingQCOM(0, 0, 100, 100, 0), GL_INVALID_OPERATION);
  GM_ERROR_CALL(EndTilingQCOM(0), GL_INVALID_OPERATION);
  AllocateAndAttachRenderBuffer(GL_RGBA8, GL_COLOR_ATTACHMENT0, 100, 100);
  GM_ERROR_CALL(EndTilingQCOM(0), GL_INVALID_OPERATION);
  GM_CALL(StartTilingQCOM(0, 0, 100, 100, 0));
  GM_ERROR_CALL(StartTilingQCOM(0, 0, 100, 100, 0), GL_INVALID_OPERATION);
  GM_CALL(EndTilingQCOM(GL_COLOR_BUFFER_BIT0_QCOM));
}

TEST_F(FakeGraphicsManagerTest, FramebufferFoveated) {
  GLuint fb;
  GM_CALL(GenFramebuffers(1, &fb));
  GM_CALL(BindFramebuffer(GL_FRAMEBUFFER, fb));
  GLuint requestedFeatures =
      GL_FOVEATION_ENABLE_BIT_QCOM | GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM;
  GLuint exposedFeatures;
  GLuint layerCount = 1;
  GLuint focalPointCount = 2;
  GLuint layer0 = 0;
  GLuint focalPoint0 = 0;
  GLuint invalidLayer = layerCount;
  GLuint invalidFocalPoint = focalPointCount;
  GLfloat focalX = -.4f;
  GLfloat focalY = 0.0f;
  GLfloat gainX = 10.0f;
  GLfloat gainY = 8.0f;
  GLfloat foveaArea = 3.0f;
  // Calling Parameters before Config fails.
  GM_ERROR_CALL(
      FramebufferFoveationParametersQCOM(fb, layer0, focalPoint0, focalX,
                                         focalY, gainX, gainY, foveaArea),
      GL_INVALID_OPERATION);

  GM_CALL(FramebufferFoveationConfigQCOM(
                    fb, layerCount, focalPointCount,
                    requestedFeatures,
                    &exposedFeatures));
  EXPECT_EQ(requestedFeatures, exposedFeatures);
  // Calling Config a second time fails.
  GM_ERROR_CALL(FramebufferFoveationConfigQCOM(
                    fb, layerCount, focalPointCount,
                    requestedFeatures,
                    &exposedFeatures),
                GL_INVALID_OPERATION);
  GM_ERROR_CALL(
      FramebufferFoveationParametersQCOM(fb, invalidLayer, focalPoint0, focalX,
                                         focalY, gainX, gainY, foveaArea),
      GL_INVALID_VALUE);
  GM_ERROR_CALL(
      FramebufferFoveationParametersQCOM(fb, layer0, invalidFocalPoint, focalX,
                                         focalY, gainX, gainY, foveaArea),
      GL_INVALID_VALUE);
  GM_CALL(
      FramebufferFoveationParametersQCOM(fb, layer0, focalPoint0, focalX,
                                         focalY, gainX, gainY, foveaArea));
  // Parameters can be called multiple times.
  GM_CALL(
      FramebufferFoveationParametersQCOM(fb, layer0, focalPoint0, focalX,
                                         focalY, gainX, gainY, foveaArea));
}

TEST_F(FakeGraphicsManagerTest, TextureFoveated) {
  GLuint tex;
  GM_CALL(GenTextures(1, &tex));
  GM_CALL(BindTexture(GL_TEXTURE_2D, tex));
  GLuint requestedFeatures =
      GL_FOVEATION_ENABLE_BIT_QCOM | GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM;
  GLuint layer0 = 0;
  GLuint focalPoint0 = 0;
  GLfloat focalX = -.4f;
  GLfloat focalY = 0.0f;
  GLfloat gainX = 10.0f;
  GLfloat gainY = 8.0f;
  GLfloat foveaArea = 3.0f;
  // Calling Parameters before Config fails.
  GM_ERROR_CALL(TextureFoveationParametersQCOM(tex, layer0, focalPoint0, focalX,
                                               focalY, gainX, gainY, foveaArea),
                GL_INVALID_OPERATION);

  // Configuring the texture for foveation.
  GM_CALL(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM,
                        requestedFeatures));

  // Trying to remove the foveation setting afterward should fails.
  GM_ERROR_CALL(
      TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM, 0),
      GL_INVALID_OPERATION);

  // A negative pixel density, or a pixel density beyond 1.0 is invalid.
  GM_ERROR_CALL(
      TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM,
                    -0.1f),
      GL_INVALID_OPERATION);
  GM_ERROR_CALL(TexParameterf(GL_TEXTURE_2D,
                              GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM, 1.1f),
                GL_INVALID_OPERATION);

  GM_CALL(TextureFoveationParametersQCOM(tex, layer0, focalPoint0, focalX,
                                         focalY, gainX, gainY, foveaArea));
  // Parameters can be called multiple times.
  GM_CALL(TextureFoveationParametersQCOM(tex, layer0, focalPoint0, focalX,
                                         focalY, gainX, gainY, foveaArea));
}

}  // namespace testing
}  // namespace gfx
}  // namespace ion

#endif  // ION_PRODUCTION
