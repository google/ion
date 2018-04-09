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

#ifndef ION_GFX_GLFUNCTIONS_INC_
#define ION_GFX_GLFUNCTIONS_INC_

#if !defined(ION_WRAP_GL_FUNC)
#  error "ION_WRAP_GL_FUNC must be defined to include glfunctions.inc"
// The line below pacifies IDEs when opening this file
#  define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace)
#endif

#if !defined(ION_TRACE_ARG)
#  define ION_TRACE_ARG(...)
#endif

// -----------------------------------------------------------------------------
// Macros wrapping all OpenGL functions.
#define ION_WRAP_GL_FUNC0(group, name, return_type)             \
  ION_WRAP_GL_FUNC(group, name, return_type, (), (), "")

#define ION_WRAP_GL_FUNC1(group, name, return_type, t1, a1) \
  ION_WRAP_GL_FUNC(group,                                   \
                   name,                                    \
                   return_type,                             \
                   (t1 a1),                                 \
                   (a1),                                    \
                   ION_TRACE_ARG(name, t1, a1))

#define ION_WRAP_GL_FUNC2(group, name, return_type,             \
                          t1, a1, t2, a2)                       \
  ION_WRAP_GL_FUNC(group, name, return_type,                    \
                   (t1 a1, t2 a2),                              \
                   (a1, a2),                                    \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<       \
                   ION_TRACE_ARG(name, t2, a2))

#define ION_WRAP_GL_FUNC3(group, name, return_type,             \
                          t1, a1, t2, a2, t3, a3)               \
  ION_WRAP_GL_FUNC(group, name, return_type,                    \
                   (t1 a1, t2 a2, t3 a3),                       \
                   (a1, a2, a3),                                \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<       \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<       \
                   ION_TRACE_ARG(name, t3, a3))

#define ION_WRAP_GL_FUNC4(group, name, return_type,             \
                          t1, a1, t2, a2, t3, a3, t4, a4)       \
  ION_WRAP_GL_FUNC(group, name, return_type,                    \
                   (t1 a1, t2 a2, t3 a3, t4 a4),                \
                   (a1, a2, a3, a4),                            \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<       \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<       \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<       \
                   ION_TRACE_ARG(name, t4, a4))

#define ION_WRAP_GL_FUNC5(group, name, return_type,                     \
                          t1, a1, t2, a2, t3, a3, t4, a4, t5, a5)       \
  ION_WRAP_GL_FUNC(group, name, return_type,                            \
                   (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5),                 \
                   (a1, a2, a3, a4, a5),                                \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<               \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<               \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<               \
                   ION_TRACE_ARG(name, t4, a4) << ", " <<               \
                   ION_TRACE_ARG(name, t5, a5))

#define ION_WRAP_GL_FUNC6(group, name, return_type,                           \
                          t1, a1, t2, a2, t3, a3, t4, a4, t5, a5, t6, a6)     \
  ION_WRAP_GL_FUNC(group, name, return_type,                                  \
                   (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6),                \
                   (a1, a2, a3, a4, a5, a6),                                  \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<                     \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<                     \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<                     \
                   ION_TRACE_ARG(name, t4, a4) << ", " <<                     \
                   ION_TRACE_ARG(name, t5, a5) << ", " <<                     \
                   ION_TRACE_ARG(name, t6, a6))

#define ION_WRAP_GL_FUNC7(group, name, return_type,                     \
                          t1, a1, t2, a2, t3, a3, t4, a4, t5, a5,       \
                          t6, a6, t7, a7)                               \
  ION_WRAP_GL_FUNC(group, name, return_type,                            \
                   (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7),   \
                   (a1, a2, a3, a4, a5, a6, a7),                        \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<               \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<               \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<               \
                   ION_TRACE_ARG(name, t4, a4) << ", " <<               \
                   ION_TRACE_ARG(name, t5, a5) << ", " <<               \
                   ION_TRACE_ARG(name, t6, a6) << ", " <<               \
                   ION_TRACE_ARG(name, t7, a7))

#define ION_WRAP_GL_FUNC8(group, name, return_type,                           \
                          t1, a1, t2, a2, t3, a3, t4, a4, t5, a5,             \
                          t6, a6, t7, a7, t8, a8)                             \
  ION_WRAP_GL_FUNC(group, name, return_type,                                  \
                   (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8),  \
                   (a1, a2, a3, a4, a5, a6, a7, a8),                          \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<                     \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<                     \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<                     \
                   ION_TRACE_ARG(name, t4, a4) << ", " <<                     \
                   ION_TRACE_ARG(name, t5, a5) << ", " <<                     \
                   ION_TRACE_ARG(name, t6, a6) << ", " <<                     \
                   ION_TRACE_ARG(name, t7, a7) << ", " <<                     \
                   ION_TRACE_ARG(name, t8, a8))

#define ION_WRAP_GL_FUNC9(group, name, return_type,                     \
                          t1, a1, t2, a2, t3, a3, t4, a4, t5, a5,       \
                          t6, a6, t7, a7, t8, a8, t9, a9)               \
  ION_WRAP_GL_FUNC(group, name, return_type,                            \
                   (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7,    \
                    t8 a8, t9 a9),                                      \
                   (a1, a2, a3, a4, a5, a6, a7, a8, a9),                \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<               \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<               \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<               \
                   ION_TRACE_ARG(name, t4, a4) << ", " <<               \
                   ION_TRACE_ARG(name, t5, a5) << ", " <<               \
                   ION_TRACE_ARG(name, t6, a6) << ", " <<               \
                   ION_TRACE_ARG(name, t7, a7) << ", " <<               \
                   ION_TRACE_ARG(name, t8, a8) << ", " <<               \
                   ION_TRACE_ARG(name, t9, a9))

#define ION_WRAP_GL_FUNC10(group, name, return_type,                    \
                          t1, a1, t2, a2, t3, a3, t4, a4, t5, a5,       \
                          t6, a6, t7, a7, t8, a8, t9, a9, t10, a10)     \
  ION_WRAP_GL_FUNC(group, name, return_type,                            \
                   (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7,    \
                    t8 a8, t9 a9, t10 a10),                             \
                   (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10),           \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<               \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<               \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<               \
                   ION_TRACE_ARG(name, t4, a4) << ", " <<               \
                   ION_TRACE_ARG(name, t5, a5) << ", " <<               \
                   ION_TRACE_ARG(name, t6, a6) << ", " <<               \
                   ION_TRACE_ARG(name, t7, a7) << ", " <<               \
                   ION_TRACE_ARG(name, t8, a8) << ", " <<               \
                   ION_TRACE_ARG(name, t9, a9) << ", " <<               \
                   ION_TRACE_ARG(name, t10, a10))

#define ION_WRAP_GL_FUNC11(group, name, return_type,                    \
                          t1, a1, t2, a2, t3, a3, t4, a4, t5, a5,       \
                          t6, a6, t7, a7, t8, a8, t9, a9, t10, a10,     \
                          t11, a11)                                     \
  ION_WRAP_GL_FUNC(group, name, return_type,                            \
                   (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7,    \
                    t8 a8, t9 a9, t10 a10, t11 a11),                    \
                   (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11),      \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<               \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<               \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<               \
                   ION_TRACE_ARG(name, t4, a4) << ", " <<               \
                   ION_TRACE_ARG(name, t5, a5) << ", " <<               \
                   ION_TRACE_ARG(name, t6, a6) << ", " <<               \
                   ION_TRACE_ARG(name, t7, a7) << ", " <<               \
                   ION_TRACE_ARG(name, t8, a8) << ", " <<               \
                   ION_TRACE_ARG(name, t9, a9) << ", " <<               \
                   ION_TRACE_ARG(name, t10, a10) << ", " <<             \
                   ION_TRACE_ARG(name, t11, a11))

#define ION_WRAP_GL_FUNC12(group, name, return_type,                    \
                          t1, a1, t2, a2, t3, a3, t4, a4, t5, a5,       \
                          t6, a6, t7, a7, t8, a8, t9, a9, t10, a10,     \
                          t11, a11, t12, a12)                           \
  ION_WRAP_GL_FUNC(group, name, return_type,                            \
                   (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7,    \
                    t8 a8, t9 a9, t10 a10, t11 a11, t12 a12),           \
                   (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12), \
                   ION_TRACE_ARG(name, t1, a1) << ", " <<               \
                   ION_TRACE_ARG(name, t2, a2) << ", " <<               \
                   ION_TRACE_ARG(name, t3, a3) << ", " <<               \
                   ION_TRACE_ARG(name, t4, a4) << ", " <<               \
                   ION_TRACE_ARG(name, t5, a5) << ", " <<               \
                   ION_TRACE_ARG(name, t6, a6) << ", " <<               \
                   ION_TRACE_ARG(name, t7, a7) << ", " <<               \
                   ION_TRACE_ARG(name, t8, a8) << ", " <<               \
                   ION_TRACE_ARG(name, t9, a9) << ", " <<               \
                   ION_TRACE_ARG(name, t10, a10) << ", " <<             \
                   ION_TRACE_ARG(name, t11, a11) << ", " <<             \
                   ION_TRACE_ARG(name, t12, a12))

// Each of these implements the corresponding standard or extended OpenGL
// function. All functions in the Core group are guaranteed to be present on all
// systems.
ION_WRAP_GL_FUNC1(Core, ActiveTexture, void, GLenum, texture)
ION_WRAP_GL_FUNC2(Core, AttachShader, void, GLuint, program, GLuint, shader)
ION_WRAP_GL_FUNC3(Core, BindAttribLocation, void, GLuint, program, GLuint,
                  index, const GLchar*, name)
ION_WRAP_GL_FUNC2(Core, BindBuffer, void, GLenum, target, GLuint, buffer)
ION_WRAP_GL_FUNC2(Core, BindFramebuffer, void, GLenum, target, GLuint,
                  framebuffer)
ION_WRAP_GL_FUNC2(Core, BindRenderbuffer, void, GLenum, target, GLuint,
                  renderbuffer)
ION_WRAP_GL_FUNC2(Core, BindTexture, void, GLenum, target, GLuint, texture)
ION_WRAP_GL_FUNC4(Core, BlendColor, void, GLfloat, red, GLfloat, green, GLfloat,
                  blue, GLfloat, alpha)
ION_WRAP_GL_FUNC1(Core, BlendEquation, void, GLenum, mode)
ION_WRAP_GL_FUNC2(Core, BlendEquationSeparate, void, GLenum, mode_rgb, GLenum,
                  mode_alpha)
ION_WRAP_GL_FUNC2(Core, BlendFunc, void, GLblendenum, sfactor, GLblendenum,
                  dfactor)
ION_WRAP_GL_FUNC4(Core, BlendFuncSeparate, void, GLblendenum, sfactor_rgb,
                  GLblendenum, dfactor_rgb, GLblendenum, sfactor_alpha,
                  GLblendenum, dfactor_alpha)
ION_WRAP_GL_FUNC4(Core, BufferData, void, GLenum, target, GLsizeiptr, size,
                  const GLvoid*, data, GLenum, usage)
ION_WRAP_GL_FUNC4(Core, BufferSubData, void, GLenum, target, GLintptr, offset,
                  GLsizeiptr, size, const GLvoid*, data)
ION_WRAP_GL_FUNC1(Core, CheckFramebufferStatus, GLenum, GLenum, target)
ION_WRAP_GL_FUNC1(Core, Clear, void, GLbitfield, mask)
ION_WRAP_GL_FUNC4(Core, ClearColor, void, GLfloat, red, GLfloat, green, GLfloat,
                  blue, GLfloat, alpha)
ION_WRAP_GL_FUNC1(Core, ClearDepthf, void, GLfloat, depth)
ION_WRAP_GL_FUNC1(Core, ClearStencil, void, GLint, s)
ION_WRAP_GL_FUNC4(Core, ColorMask, void, GLboolean, red, GLboolean, green,
                  GLboolean, blue, GLboolean, alpha)
ION_WRAP_GL_FUNC1(Core, CompileShader, void, GLuint, shader)
ION_WRAP_GL_FUNC8(Core, CompressedTexImage2D, void, GLenum, target, GLint,
                  level, GLenum, internal_format, GLsizei, width, GLsizei,
                  height, GLint, border, GLsizei, image_size, const GLvoid*,
                  data)
ION_WRAP_GL_FUNC9(Core, CompressedTexSubImage2D, void, GLenum, target, GLint,
                  level, GLint, xoffset, GLint, yoffset, GLsizei, width,
                  GLsizei, height, GLenum, format, GLsizei, imageSize,
                  const GLvoid*, data)
ION_WRAP_GL_FUNC8(Core, CopyTexImage2D, void, GLenum, target, GLint, level,
                  GLenum, internalformat, GLint, x, GLint, y, GLsizei, width,
                  GLsizei, height, GLint, border)
ION_WRAP_GL_FUNC8(Core, CopyTexSubImage2D, void, GLenum, target, GLint, level,
                  GLint, xoffset, GLint, yoffset, GLint, x, GLint, y, GLsizei,
                  width, GLsizei, height)
ION_WRAP_GL_FUNC0(Core, CreateProgram, GLuint)
ION_WRAP_GL_FUNC1(Core, CreateShader, GLuint, GLenum, type)
ION_WRAP_GL_FUNC1(Core, CullFace, void, GLenum, mode)
ION_WRAP_GL_FUNC2(Core, DeleteBuffers, void, GLsizei, n, const GLuint*,
                  buffers)
ION_WRAP_GL_FUNC2(Core, DeleteFramebuffers, void, GLsizei, n, const GLuint*,
                  framebuffers)
ION_WRAP_GL_FUNC1(Core, DeleteProgram, void, GLuint, program)
ION_WRAP_GL_FUNC2(Core, DeleteRenderbuffers, void, GLsizei, n, const GLuint*,
                  renderbuffers)
ION_WRAP_GL_FUNC1(Core, DeleteShader, void, GLuint, shader)
ION_WRAP_GL_FUNC2(Core, DeleteTextures, void, GLsizei, n, const GLuint*,
                  textures)
ION_WRAP_GL_FUNC1(Core, DepthFunc, void, GLenum, func)
ION_WRAP_GL_FUNC1(Core, DepthMask, void, GLboolean, flag)
ION_WRAP_GL_FUNC2(Core, DepthRangef, void, GLfloat, near_val, GLfloat, far_val)
ION_WRAP_GL_FUNC2(Core, DetachShader, void, GLuint, program, GLuint, shader)
ION_WRAP_GL_FUNC1(Core, Disable, void, GLenum, cap)
ION_WRAP_GL_FUNC1(Core, DisableVertexAttribArray, void, GLuint, index)
ION_WRAP_GL_FUNC3(Core, DrawArrays, void, GLenum, mode, GLint, first, GLsizei,
                  count)
ION_WRAP_GL_FUNC4(Core, DrawElements, void, GLenum, mode, GLsizei, count,
                  GLenum, type, const GLvoid*, indices)
ION_WRAP_GL_FUNC1(Core, Enable, void, GLenum, cap)
ION_WRAP_GL_FUNC1(Core, EnableVertexAttribArray, void, GLuint, index)
ION_WRAP_GL_FUNC0(Core, Finish, void)
ION_WRAP_GL_FUNC0(Core, Flush, void)
ION_WRAP_GL_FUNC4(Core, FramebufferRenderbuffer, void, GLenum, target, GLenum,
                  attachment, GLenum, renderbuffertarget, GLuint, renderbuffer)
ION_WRAP_GL_FUNC5(Core, FramebufferTexture2D, void, GLenum, target, GLenum,
                  attachment, GLenum, textarget, GLuint, texture, GLint, level)
ION_WRAP_GL_FUNC1(Core, FrontFace, void, GLenum, mode)
ION_WRAP_GL_FUNC2(Core, GenBuffers, void, GLsizei, n, GLuint*, buffers)
ION_WRAP_GL_FUNC1(Core, GenerateMipmap, void, GLenum, target)
ION_WRAP_GL_FUNC2(Core, GenFramebuffers, void, GLsizei, n, GLuint*,
                  framebuffers)
ION_WRAP_GL_FUNC2(Core, GenRenderbuffers, void, GLsizei, n, GLuint*,
                  renderbuffers)
ION_WRAP_GL_FUNC2(Core, GenTextures, void, GLsizei, n, GLuint*, textures)
ION_WRAP_GL_FUNC7(Core, GetActiveAttrib, void, GLuint, program, GLuint, index,
                  GLsizei, bufSize, GLsizei*, length, GLint*, size, GLenum*,
                  type, GLchar*, name)
ION_WRAP_GL_FUNC7(Core, GetActiveUniform, void, GLuint, program, GLuint, index,
                  GLsizei, bufSize, GLsizei*, length, GLint*, size, GLenum*,
                  type, GLchar*, name)
ION_WRAP_GL_FUNC4(Core, GetAttachedShaders, void, GLuint, program, GLsizei,
                  maxCount, GLsizei*, count, GLuint*, shaders)
ION_WRAP_GL_FUNC2(Core, GetAttribLocation, GLint, GLuint, program,
                  const GLchar*, name)
ION_WRAP_GL_FUNC2(Core, GetBooleanv, void, GLenum, pname, GLboolean*, params)
ION_WRAP_GL_FUNC3(Core, GetBufferParameteriv, void, GLenum, target, GLenum,
                  value, GLint*, data)
#ifndef ION_WRAP_SKIP_GetError
ION_WRAP_GL_FUNC0(Core, GetError, GLenum)
#endif
ION_WRAP_GL_FUNC2(Core, GetFloatv, void, GLenum, pname, GLfloat*, params)
ION_WRAP_GL_FUNC4(Core, GetFramebufferAttachmentParameteriv, void, GLenum,
                  target, GLenum, attachment, GLenum, pname, GLint*, params)
ION_WRAP_GL_FUNC2(Core, GetIntegerv, void, GLenum, pname, GLint*, params)
ION_WRAP_GL_FUNC4(Core, GetProgramInfoLog, void, GLuint, program, GLsizei,
                  buf_size, GLsizei*, length, GLchar*, info_log)
ION_WRAP_GL_FUNC3(Core, GetProgramiv, void, GLuint, program, GLenum, pname,
                  GLint*, params)
ION_WRAP_GL_FUNC3(Core, GetRenderbufferParameteriv, void, GLenum, target,
                  GLenum, pname, GLint*, params)
ION_WRAP_GL_FUNC4(Core, GetShaderInfoLog, void, GLuint, shader, GLsizei,
                  buf_size, GLsizei*, length, GLchar*, info_log)
ION_WRAP_GL_FUNC4(Core, GetShaderPrecisionFormat, void, GLenum, shaderType,
                  GLenum, precisionType, GLint*, range, GLint*, precision)
ION_WRAP_GL_FUNC4(Core, GetShaderSource, void, GLuint, shader, GLsizei,
                  buf_size, GLsizei*, length, GLchar*, source)
ION_WRAP_GL_FUNC3(Core, GetShaderiv, void, GLuint, shader, GLenum, pname,
                  GLint*, params)
ION_WRAP_GL_FUNC1(Core, GetString, const GLubyte*, GLenum, name)
ION_WRAP_GL_FUNC3(Core, GetTexParameterfv, void, GLenum, target, GLenum, pname,
                  GLfloat*, params)
ION_WRAP_GL_FUNC3(Core, GetTexParameteriv, void, GLenum, target, GLenum, pname,
                  GLint*, params)
ION_WRAP_GL_FUNC3(Core, GetUniformfv, void, GLuint, program, GLint, location,
                  GLfloat*, params)
ION_WRAP_GL_FUNC3(Core, GetUniformiv, void, GLuint, program, GLint, location,
                  GLint*, params)
ION_WRAP_GL_FUNC2(Core, GetUniformLocation, GLint, GLuint, program,
                  const GLchar*, name)
ION_WRAP_GL_FUNC3(Core, GetVertexAttribfv, void, GLuint, index, GLenum, pname,
                  GLfloat*, params)
ION_WRAP_GL_FUNC3(Core, GetVertexAttribiv, void, GLuint, index, GLenum, pname,
                  GLint*, params)
ION_WRAP_GL_FUNC3(Core, GetVertexAttribPointerv, void, GLuint, index, GLenum,
                  pname, GLvoid**, pointer)
ION_WRAP_GL_FUNC2(Core, Hint, void, GLenum, target, GLenum, hint)
ION_WRAP_GL_FUNC1(Core, IsBuffer, GLboolean, GLuint, buffer)
ION_WRAP_GL_FUNC1(Core, IsEnabled, GLboolean, GLenum, value)
ION_WRAP_GL_FUNC1(Core, IsFramebuffer, GLboolean, GLuint, framebuffer)
ION_WRAP_GL_FUNC1(Core, IsProgram, GLboolean, GLuint, program)
ION_WRAP_GL_FUNC1(Core, IsRenderbuffer, GLboolean, GLuint, renderbuffer)
ION_WRAP_GL_FUNC1(Core, IsShader, GLboolean, GLuint, shader)
ION_WRAP_GL_FUNC1(Core, IsTexture, GLboolean, GLuint, texture)
ION_WRAP_GL_FUNC1(Core, LineWidth, void, GLfloat, width)
ION_WRAP_GL_FUNC1(Core, LinkProgram, void, GLuint, program)
ION_WRAP_GL_FUNC2(Core, PixelStorei, void, GLenum, pname, GLint, param)
ION_WRAP_GL_FUNC2(Core, PolygonOffset, void, GLfloat, factor, GLfloat, units)
ION_WRAP_GL_FUNC7(Core, ReadPixels, void, GLint, x, GLint, y, GLsizei, width,
                  GLsizei, height, GLenum, format, GLenum, type, GLvoid*, data)
ION_WRAP_GL_FUNC0(Core, ReleaseShaderCompiler, void)
ION_WRAP_GL_FUNC4(Core, RenderbufferStorage, void, GLenum, target, GLenum,
                  internalformat, GLsizei, width, GLsizei, height)
ION_WRAP_GL_FUNC2(Core, SampleCoverage, void, GLfloat, value, GLboolean,
                  invert)
ION_WRAP_GL_FUNC4(Core, Scissor, void, GLint, x, GLint, y, GLsizei, width,
                  GLsizei, height)
ION_WRAP_GL_FUNC5(Core, ShaderBinary, void, GLsizei, n, const GLuint*, shaders,
                  GLenum, binaryformat, const void*, binary, GLsizei, length)
ION_WRAP_GL_FUNC4(Core, ShaderSource, void, GLuint, shader, GLsizei, count,
                  const GLchar**, string, const GLint*, length)
ION_WRAP_GL_FUNC3(Core, StencilFunc, void, GLenum, func, GLint, ref, GLuint,
                  mask)
ION_WRAP_GL_FUNC4(Core, StencilFuncSeparate, void, GLenum, face, GLenum, func,
                  GLint, ref, GLuint, mask)
ION_WRAP_GL_FUNC1(Core, StencilMask, void, GLuint, mask)
ION_WRAP_GL_FUNC2(Core, StencilMaskSeparate, void, GLenum, face, GLuint, mask)
ION_WRAP_GL_FUNC3(Core, StencilOp, void, GLstencilenum, sfail, GLstencilenum,
                  dpfail, GLstencilenum, dppass)
ION_WRAP_GL_FUNC4(Core, StencilOpSeparate, void, GLenum, face, GLstencilenum,
                  sfail, GLstencilenum, dpfail, GLstencilenum, dppass)
ION_WRAP_GL_FUNC9(Core, TexImage2D, void, GLintenum, target, GLint, level,
                  GLenum, internal_format, GLsizei, width, GLsizei, height,
                  GLint, border, GLenum, format, GLenum, type, const GLvoid*,
                  pixels)
ION_WRAP_GL_FUNC3(Core, TexParameterf, void, GLenum, target, GLenum, pname,
                  GLfloat, param)
ION_WRAP_GL_FUNC3(Core, TexParameterfv, void, GLenum, target, GLenum, pname,
                  const GLfloat*, params)
ION_WRAP_GL_FUNC3(Core, TexParameteri, void, GLenum, target, GLenum, pname,
                  GLtextureenum, param)
ION_WRAP_GL_FUNC3(Core, TexParameteriv, void, GLenum, target, GLenum, pname,
                  const GLint*, params)
ION_WRAP_GL_FUNC9(Core, TexSubImage2D, void, GLenum, target, GLint, level,
                  GLint, xoffset, GLint, yoffset, GLsizei, width, GLsizei,
                  height, GLenum, format, GLenum, type, const GLvoid*, data)
ION_WRAP_GL_FUNC2(Core, Uniform1f, void, GLint, location, GLfloat, v0)
ION_WRAP_GL_FUNC3(Core, Uniform1fv, void, GLint, location, GLsizei, count,
                  const GLfloat1*, value)
ION_WRAP_GL_FUNC2(Core, Uniform1i, void, GLint, location, GLint, v0)
ION_WRAP_GL_FUNC3(Core, Uniform1iv, void, GLint, location, GLsizei, count,
                  const GLint1*, value)
ION_WRAP_GL_FUNC3(Core, Uniform2f, void, GLint, location, GLfloat, v0, GLfloat,
                  v1)
ION_WRAP_GL_FUNC3(Core, Uniform2fv, void, GLint, location, GLsizei, count,
                  const GLfloat2*, value)
ION_WRAP_GL_FUNC3(Core, Uniform2i, void, GLint, location, GLint, v0, GLint, v1)
ION_WRAP_GL_FUNC3(Core, Uniform2iv, void, GLint, location, GLsizei, count,
                  const GLint2*, value)
ION_WRAP_GL_FUNC4(Core, Uniform3f, void, GLint, location, GLfloat, v0, GLfloat,
                  v1, GLfloat, v2)
ION_WRAP_GL_FUNC3(Core, Uniform3fv, void, GLint, location, GLsizei, count,
                  const GLfloat3*, value)
ION_WRAP_GL_FUNC4(Core, Uniform3i, void, GLint, location, GLint, v0, GLint, v1,
                  GLint, v2)
ION_WRAP_GL_FUNC3(Core, Uniform3iv, void, GLint, location, GLsizei, count,
                  const GLint3*, value)
ION_WRAP_GL_FUNC5(Core, Uniform4f, void, GLint, location, GLfloat, v0, GLfloat,
                  v1, GLfloat, v2, GLfloat, v3)
ION_WRAP_GL_FUNC3(Core, Uniform4fv, void, GLint, location, GLsizei, count,
                  const GLfloat4*, value)
ION_WRAP_GL_FUNC5(Core, Uniform4i, void, GLint, location, GLint, v0, GLint, v1,
                  GLint, v2, GLint, v3)
ION_WRAP_GL_FUNC3(Core, Uniform4iv, void, GLint, location, GLsizei, count,
                  const GLint4*, value)
ION_WRAP_GL_FUNC4(Core, UniformMatrix2fv, void, GLint, location, GLsizei, count,
                  GLboolean, transpose, const GLmatrix2*, value)
ION_WRAP_GL_FUNC4(Core, UniformMatrix3fv, void, GLint, location, GLsizei, count,
                  GLboolean, transpose, const GLmatrix3*, value)
ION_WRAP_GL_FUNC4(Core, UniformMatrix4fv, void, GLint, location, GLsizei, count,
                  GLboolean, transpose, const GLmatrix4*, value)
ION_WRAP_GL_FUNC1(Core, UseProgram, void, GLuint, program)
ION_WRAP_GL_FUNC1(Core, ValidateProgram, void, GLuint, program)
ION_WRAP_GL_FUNC2(Core, VertexAttrib1f, void, GLint, index, GLfloat, v0)
ION_WRAP_GL_FUNC2(Core, VertexAttrib1fv, void, GLuint, index, const GLfloat1*,
                  value)
ION_WRAP_GL_FUNC3(Core, VertexAttrib2f, void, GLint, index, GLfloat, v0,
                  GLfloat, v1)
ION_WRAP_GL_FUNC2(Core, VertexAttrib2fv, void, GLuint, index, const GLfloat2*,
                  value)
ION_WRAP_GL_FUNC4(Core, VertexAttrib3f, void, GLint, index, GLfloat, v0,
                  GLfloat, v1, GLfloat, v2)
ION_WRAP_GL_FUNC2(Core, VertexAttrib3fv, void, GLuint, index, const GLfloat3*,
                  value)
ION_WRAP_GL_FUNC5(Core, VertexAttrib4f, void, GLint, index, GLfloat, v0,
                  GLfloat, v1, GLfloat, v2, GLfloat, v3)
ION_WRAP_GL_FUNC2(Core, VertexAttrib4fv, void, GLuint, index, const GLfloat4*,
                  value)
ION_WRAP_GL_FUNC6(Core, VertexAttribPointer, void, GLuint, index, GLint, size,
                  GLenum, type, GLboolean, normalized, GLsizei, stride,
                  const GLvoid*, pointer)
ION_WRAP_GL_FUNC4(Core, Viewport, void, GLint, x, GLint, y, GLsizei, width,
                  GLsizei, height)

// ComputeShader group.
ION_WRAP_GL_FUNC3(ComputeShader, DispatchCompute, void, GLuint, num_groups_x,
                  GLuint, num_groups_y, GLuint, num_groups_z)
ION_WRAP_GL_FUNC1(ComputeShader, DispatchComputeIndirect, void, GLintptr,
                  indirect)

// CopyBufferSubData group.
ION_WRAP_GL_FUNC5(CopyBufferSubData, CopyBufferSubData,
                  void, GLenum, read_target, GLenum, write_target,
                  GLintptr, read_offset, GLintptr, write_offset,
                  GLsizeiptr, size)

// DebugLabel group.
ION_WRAP_GL_FUNC5(DebugLabel, GetObjectLabel, void, GLenum, type, GLuint,
                  object, GLsizei, bufSize, GLsizei*, length, GLchar*, label)
ION_WRAP_GL_FUNC4(DebugLabel, LabelObject, void, GLenum, type, GLuint, object,
                  GLsizei, length, const GLchar*, label)

// DebugMarker group.
ION_WRAP_GL_FUNC2(DebugMarker, InsertEventMarker, void, GLsizei, length,
                  const GLchar*, marker)
ION_WRAP_GL_FUNC0(DebugMarker, PopGroupMarker, void)
ION_WRAP_GL_FUNC2(DebugMarker, PushGroupMarker, void, GLsizei, length,
                  const GLchar*, marker)

// DebugOutput group.
ION_WRAP_GL_FUNC2(DebugOutput, DebugMessageCallback, void, GLDEBUGPROC,
                  callback, const void*, userParam)
ION_WRAP_GL_FUNC6(DebugOutput, DebugMessageControl, void, GLenum, source,
                  GLenum, type, GLenum, severity, GLsizei, count, GLuint*, ids,
                  GLboolean, enabled)
ION_WRAP_GL_FUNC6(DebugOutput, DebugMessageInsert, void, GLenum, source, GLenum,
                  type, GLuint, id, GLenum, severity, GLsizei, length,
                  const GLchar*, buf)
ION_WRAP_GL_FUNC8(DebugOutput, GetDebugMessageLog, GLuint, GLuint, count,
                  GLsizei, bufSize, GLenum*, sources, GLenum*, types, GLuint*,
                  ids, GLenum*, severities, GLsizei*, lengths, GLchar*,
                  messageLog)
ION_WRAP_GL_FUNC2(DebugOutput, GetPointerv, void, GLenum, pname, void**,
                  params)

// DiscardFramebuffer group
// EXT_discard_framebuffer extension. Equivalent to InvalidateFramebuffer but
// works prior to OpenGL ES 3.0 if the extension is available.
ION_WRAP_GL_FUNC3(DiscardFramebuffer, DiscardFramebufferEXT, void, GLenum,
                  target, GLsizei, numAttachments, const GLenum*, attachments)

// DrawBuffer group.
ION_WRAP_GL_FUNC1(DrawBuffer, DrawBuffer, void, GLenum, buffer)

// DrawBuffers group.
ION_WRAP_GL_FUNC2(DrawBuffers, DrawBuffers, void, GLsizei, n, const GLenum*,
                  bufs)

// DrawInstanced group.
ION_WRAP_GL_FUNC4(DrawInstanced, DrawArraysInstanced, void, GLenum, mode,
                  GLint, first, GLsizei, count, GLsizei, primCount)
ION_WRAP_GL_FUNC5(DrawInstanced, DrawElementsInstanced, void, GLenum, mode,
                  GLsizei, count, GLenum, type, const GLvoid*, indices, GLsizei,
                  primCount)

// EglImage group.
ION_WRAP_GL_FUNC2(EglImage, EGLImageTargetTexture2DOES, void, GLenum, target,
                  void*, image)
ION_WRAP_GL_FUNC2(EglImage, EGLImageTargetRenderbufferStorageOES, void,
                  GLenum, target, void*, image)

// FramebufferBlit group.
ION_WRAP_GL_FUNC10(FramebufferBlit, BlitFramebuffer, void, GLint, srcX0, GLint,
                   srcY0, GLint, srcX1, GLint, srcY1, GLint, dstX0, GLint,
                   dstY0, GLint, dstX1, GLint, dstY1, GLbitfield, mask, GLenum,
                   filter)

// FramebufferTextureLayer group.
ION_WRAP_GL_FUNC5(FramebufferTextureLayer, FramebufferTextureLayer, void,
                  GLenum, target, GLenum, attachment, GLuint, texture, GLint,
                  level, GLint, layer)

// GetString group.
ION_WRAP_GL_FUNC2(GetString, GetStringi, const GLubyte*, GLenum, name, GLuint,
                  index)

// GpuShader4 group.
ION_WRAP_GL_FUNC3(GpuShader4, GetUniformuiv, void, GLuint, program, GLint,
                  location, GLuint*, params)
ION_WRAP_GL_FUNC2(GpuShader4, Uniform1ui, void, GLint, location, GLuint, v0)
ION_WRAP_GL_FUNC3(GpuShader4, Uniform1uiv, void, GLint, location, GLsizei,
                  count, const GLuint1*, value)
ION_WRAP_GL_FUNC3(GpuShader4, Uniform2ui, void, GLint, location, GLuint, v0,
                  GLuint, v1)
ION_WRAP_GL_FUNC3(GpuShader4, Uniform2uiv, void, GLint, location, GLsizei,
                  count, const GLuint2*, value)
ION_WRAP_GL_FUNC4(GpuShader4, Uniform3ui, void, GLint, location, GLuint, v0,
                  GLuint, v1, GLuint, v2)
ION_WRAP_GL_FUNC3(GpuShader4, Uniform3uiv, void, GLint, location, GLsizei,
                  count, const GLuint3*, value)
ION_WRAP_GL_FUNC5(GpuShader4, Uniform4ui, void, GLint, location, GLuint, v0,
                  GLuint, v1, GLuint, v2, GLuint, v3)
ION_WRAP_GL_FUNC3(GpuShader4, Uniform4uiv, void, GLint, location, GLsizei,
                  count, const GLuint4*, value)
// 
// BindFragDataLocation, and GetFragDataLocation.

// ImplicitMultisample group, i.e., EXT_multisampled_render_to_texture. Note
// that the second function is NOT equivalent to the core OpenGL ES 3.1 variant.
ION_WRAP_GL_FUNC6(ImplicitMultisample, FramebufferTexture2DMultisampleEXT, void,
                  GLenum, target, GLenum, attachment, GLenum, textarget, GLuint,
                  texture, GLint, level, GLsizei, samples)
ION_WRAP_GL_FUNC5(ImplicitMultisample, RenderbufferStorageMultisampleEXT, void,
                  GLenum, target, GLsizei, samples, GLenum, internalformat,
                  GLsizei, width, GLsizei, height)

// BindBufferIndexed group.
ION_WRAP_GL_FUNC3(BindBufferIndexed, BindBufferBase, void, GLenum, target,
                  GLuint, index, GLuint, buffer)
ION_WRAP_GL_FUNC5(BindBufferIndexed, BindBufferRange, void, GLenum, target,
                  GLuint, index, GLuint, buffer, GLintptr, offset, GLsizeiptr,
                  size)

// InstancedArrays group.
ION_WRAP_GL_FUNC2(InstancedArrays, VertexAttribDivisor, void, GLuint, index,
                  GLuint, divisor)

// InvalidateFramebuffer group.
ION_WRAP_GL_FUNC3(InvalidateFramebuffer, InvalidateFramebuffer, void, GLenum,
                  target, GLsizei, numAttachments, const GLenum*, attachments)
ION_WRAP_GL_FUNC7(InvalidateFramebuffer, InvalidateSubFramebuffer, void, GLenum,
                  target, GLsizei, numAttachments, const GLenum*, attachments,
                  GLint, x, GLint, y, GLsizei, width, GLsizei, height)

// MapBuffer group.
ION_WRAP_GL_FUNC2(MapBuffer, MapBuffer, void*, GLenum, target, GLenum, access)

// MapBufferBase group.
ION_WRAP_GL_FUNC3(MapBufferBase, GetBufferPointerv, void, GLenum, target,
                  GLenum, pname, GLvoid**, params)
ION_WRAP_GL_FUNC1(MapBufferBase, UnmapBuffer, void, GLenum, target)

// MapBufferRange group.
ION_WRAP_GL_FUNC3(MapBufferRange, FlushMappedBufferRange, void, GLenum,
                  target, GLintptr, offset, GLsizeiptr, length)
ION_WRAP_GL_FUNC4(MapBufferRange, MapBufferRange, void*, GLenum, target,
                  GLintptr, offset, GLsizeiptr, length, GLmapaccess, access)
// MultisampleFramebufferResolve group.
ION_WRAP_GL_FUNC0(
    MultisampleFramebufferResolve, ResolveMultisampleFramebuffer, void)

// Multiview group.
ION_WRAP_GL_FUNC6(Multiview, FramebufferTextureMultiviewOVR, void, GLenum,
                  target, GLenum, attachment, GLuint, texture, GLint, level,
                  GLint, baseViewIndex, GLsizei, numViews)

// MultiviewMultisample group.
ION_WRAP_GL_FUNC7(MultiviewImplicitMultisample,
                  FramebufferTextureMultisampleMultiviewOVR, void, GLenum,
                  target, GLenum, attachment, GLuint, texture, GLint, level,
                  GLsizei, samples, GLint, baseViewIndex, GLsizei, numViews)

// PointSize group.
ION_WRAP_GL_FUNC1(PointSize, PointSize, void, GLfloat, size)

// ReadBuffer group.
ION_WRAP_GL_FUNC1(ReadBuffer, ReadBuffer, void, GLenum, buffer)

// RenderbufferMultisample group.
ION_WRAP_GL_FUNC5(RenderbufferMultisample, RenderbufferStorageMultisample, void,
                  GLenum, target, GLsizei, samples, GLenum, internal_format,
                  GLsizei, width, GLsizei, height)

// SamplerObjects group.
ION_WRAP_GL_FUNC2(SamplerObjects, BindSampler, void, GLuint, unit, GLuint,
                  sampler)
ION_WRAP_GL_FUNC2(SamplerObjects, DeleteSamplers, void, GLsizei, n,
                  const GLuint*, samplers)
ION_WRAP_GL_FUNC2(SamplerObjects, GenSamplers, void, GLsizei, n, GLuint*,
                  samplers)
ION_WRAP_GL_FUNC3(SamplerObjects, GetSamplerParameterfv, void, GLuint, sampler,
                  GLenum, pname, GLfloat*, params)
ION_WRAP_GL_FUNC3(SamplerObjects, GetSamplerParameteriv, void, GLuint, sampler,
                  GLenum, pname, GLint*, params)
ION_WRAP_GL_FUNC1(SamplerObjects, IsSampler, GLboolean, GLuint, id)
ION_WRAP_GL_FUNC3(SamplerObjects, SamplerParameterf, void, GLuint, sampler,
                  GLenum, pname, GLfloat, param)
ION_WRAP_GL_FUNC3(SamplerObjects, SamplerParameterfv, void, GLuint, sampler,
                  GLenum, pname, const GLfloat*, params)
ION_WRAP_GL_FUNC3(SamplerObjects, SamplerParameteri, void, GLuint, sampler,
                  GLenum, pname, GLtextureenum, param)
ION_WRAP_GL_FUNC3(SamplerObjects, SamplerParameteriv, void, GLuint, sampler,
                  GLenum, pname, const GLint*, params)

// SampleShading group.
ION_WRAP_GL_FUNC1(SampleShading, MinSampleShading, void, GLfloat, value)

// Sync objects group.
ION_WRAP_GL_FUNC3(Sync, ClientWaitSync, GLenum, GLsync, sync, GLbitfield, flags,
                  GLuint64, timeout)
ION_WRAP_GL_FUNC1(Sync, DeleteSync, void, GLsync, sync)
ION_WRAP_GL_FUNC2(Sync, FenceSync, GLsync, GLenum, condition, GLbitfield,
                  flags)
ION_WRAP_GL_FUNC5(Sync, GetSynciv, void, GLsync, sync, GLenum, pname, GLsizei,
                  bufSize, GLsizei*, length, GLint*, values)
ION_WRAP_GL_FUNC3(Sync, WaitSync, void, GLsync, sync, GLbitfield, flags,
                  GLuint64, timeout)

// Texture3d group.
ION_WRAP_GL_FUNC9(Texture3d, CompressedTexImage3D, void, GLenum, target, GLint,
                  level, GLenum, internal_format, GLsizei, width, GLsizei,
                  height, GLsizei, depth, GLint, border, GLsizei, image_size,
                  const GLvoid*, data)
ION_WRAP_GL_FUNC11(Texture3d, CompressedTexSubImage3D, void, GLenum, target,
                   GLint, level, GLint, xoffset, GLint, yoffset, GLint, zoffset,
                   GLsizei, width, GLsizei, height, GLsizei, depth, GLenum,
                   format, GLsizei, imageSize, const GLvoid*, data)
ION_WRAP_GL_FUNC9(Texture3d, CopyTexSubImage3D, void, GLenum, target, GLint,
                  level, GLint, xoffset, GLint, yoffset, GLint, zoffset, GLint,
                  x, GLint, y, GLsizei, width, GLsizei, height)
ION_WRAP_GL_FUNC10(Texture3d, TexImage3D, void, GLintenum, target, GLint, level,
                   GLenum, internal_format, GLsizei, width, GLsizei, height,
                   GLsizei, depth, GLint, border, GLenum, format, GLenum, type,
                   const GLvoid*, pixels)
ION_WRAP_GL_FUNC11(Texture3d, TexSubImage3D, void, GLenum, target, GLint, level,
                   GLint, xoffset, GLint, yoffset, GLint, zoffset, GLsizei,
                   width, GLsizei, height, GLsizei, depth, GLenum, format,
                   GLenum, type, const GLvoid*, data)

// TextureBarrier group.
ION_WRAP_GL_FUNC0(TextureBarrier, TextureBarrier, void)

// TextureMultisample group.
ION_WRAP_GL_FUNC6(TextureMultisample, TexImage2DMultisample, void, GLenum,
                  target, GLsizei, samples, GLenum, internal_format, GLsizei,
                  width, GLsizei, height, GLboolean, fixed_sample_locations)
ION_WRAP_GL_FUNC7(TextureMultisample, TexImage3DMultisample, void, GLenum,
                  target, GLsizei, samples, GLenum, internal_format, GLsizei,
                  width, GLsizei, height, GLsizei, depth, GLboolean,
                  fixed_sample_locations)
ION_WRAP_GL_FUNC3(TextureMultisample, GetMultisamplefv, void, GLenum, pname,
                  GLuint, index, GLfloat*, val)
ION_WRAP_GL_FUNC2(TextureMultisample, SampleMaski, void, GLuint, index,
                  GLbitfield, mask)

// TextureStorage group.
ION_WRAP_GL_FUNC5(TextureStorage, TexStorage2D, void, GLenum, target, GLsizei,
                  levels, GLenum, internalformat, GLsizei, width, GLsizei,
                  height)
ION_WRAP_GL_FUNC6(TextureStorage, TexStorage3D, void, GLenum, target, GLsizei,
                  levels, GLenum, internalformat, GLsizei, width, GLsizei,
                  height, GLsizei, depth)

// TextureStorageMultisample group.
ION_WRAP_GL_FUNC6(TextureStorageMultisample, TexStorage2DMultisample, void,
                  GLenum, target, GLsizei, samples, GLenum, internal_format,
                  GLsizei, width, GLsizei, height, GLboolean,
                  fixed_sample_locations)
ION_WRAP_GL_FUNC7(TextureStorageMultisample, TexStorage3DMultisample, void,
                  GLenum, target, GLsizei, samples, GLenum, internal_format,
                  GLsizei, width, GLsizei, height, GLsizei, depth, GLboolean,
                  fixed_sample_locations)

// TiledRendering group.
ION_WRAP_GL_FUNC5(TiledRendering, StartTilingQCOM, void, GLuint, x, GLuint, y,
                  GLuint, width, GLuint, height, GLbitfield, preserveMask)
ION_WRAP_GL_FUNC1(TiledRendering, EndTilingQCOM, void, GLbitfield, preserveMask)

// FramebufferFoveated group.
// https://www.khronos.org/registry/OpenGL/extensions/QCOM/QCOM_framebuffer_foveated.txt
ION_WRAP_GL_FUNC5(FramebufferFoveated, FramebufferFoveationConfigQCOM, void,
                  GLuint, fbo, GLuint, numLayers, GLuint, focalPointsPerLayer,
                  GLuint, requestedFeatures, GLuint*, providedFeatures)
ION_WRAP_GL_FUNC8(FramebufferFoveated, FramebufferFoveationParametersQCOM, void,
                  GLuint, fbo, GLuint, layer, GLuint, focalPoint, GLfloat,
                  focalX, GLfloat, focalY, GLfloat, gainX, GLfloat, gainY,
                  GLfloat, foveaArea)

// TextureFoveated group.
// https://www.khronos.org/registry/OpenGL/extensions/QCOM/QCOM_texture_foveated.txt
ION_WRAP_GL_FUNC8(TextureFoveated, TextureFoveationParametersQCOM, void,
                  GLuint, texture, GLuint, layer, GLuint, focalPoint, GLfloat,
                  focalX, GLfloat, focalY, GLfloat, gainX, GLfloat, gainY,
                  GLfloat, foveaArea)

// TransformFeedback group.
ION_WRAP_GL_FUNC1(TransformFeedback, BeginTransformFeedback, void, GLenum,
                  primitive_mode)
ION_WRAP_GL_FUNC2(TransformFeedback, BindTransformFeedback, void, GLenum,
                  target, GLuint, id)
ION_WRAP_GL_FUNC2(TransformFeedback, DeleteTransformFeedbacks, void, GLsizei, n,
                  const GLuint*, ids)
ION_WRAP_GL_FUNC0(TransformFeedback, EndTransformFeedback, void)
ION_WRAP_GL_FUNC2(TransformFeedback, GenTransformFeedbacks, void, GLsizei, n,
                  GLuint*, ids)
ION_WRAP_GL_FUNC7(TransformFeedback, GetTransformFeedbackVarying, void, GLuint,
                  program, GLuint, index, GLsizei, buf_size, GLsizei*, length,
                  GLsizei*, size, GLenum*, type, GLchar*, name)
ION_WRAP_GL_FUNC1(TransformFeedback, IsTransformFeedback, GLboolean, GLuint,
                  id)
ION_WRAP_GL_FUNC0(TransformFeedback, PauseTransformFeedback, void)
ION_WRAP_GL_FUNC0(TransformFeedback, ResumeTransformFeedback, void)
ION_WRAP_GL_FUNC4(TransformFeedback, TransformFeedbackVaryings, void, GLuint,
                  program, GLsizei, count, const GLchar**, varyings, GLenum,
                  buffer_mode)

// VertexArrays group.
ION_WRAP_GL_FUNC1(VertexArrays, BindVertexArray, void, GLuint, array)
ION_WRAP_GL_FUNC2(VertexArrays, DeleteVertexArrays, void, GLsizei, n,
                  const GLuint*, arrays)
ION_WRAP_GL_FUNC2(VertexArrays, GenVertexArrays, void, GLsizei, n, GLuint*,
                  arrays)
ION_WRAP_GL_FUNC1(VertexArrays, IsVertexArray, GLboolean, GLuint, array)

// Raw group.  The functions here belong to two categories:
// a) They have GraphicsManager loading support and may have FakeGlContext
//    support, but are not used anywhere else in Ion.
// b) They part of more than one feature. (For example, multiple extensions
//    define the function GetIntegeri_v, and it is present when any of those are
//    present.)
ION_WRAP_GL_FUNC7(Raw, BindImageTexture, void, GLuint, unit, GLuint, texture,
                  GLint, level, GLboolean, layered, GLint, layer,
                  GLenum, access, GLenum, format)
ION_WRAP_GL_FUNC2(Raw, GetInteger64v, void, GLenum, pname, GLint64*, params)
ION_WRAP_GL_FUNC3(Raw, GetInteger64i_v, void, GLenum, pname, GLuint, index,
                       GLint64*, params)
ION_WRAP_GL_FUNC3(Raw, GetIntegeri_v, void, GLenum, pname, GLuint, index,
                       GLint*, params)
ION_WRAP_GL_FUNC3(Raw, TexBuffer, void, GLintenum, target, GLenum,
                  internal_format, GLint, buffer)
// Raw group.  Timer queries.
ION_WRAP_GL_FUNC2(Raw, BeginQuery, void, GLenum, target, GLuint, id)
ION_WRAP_GL_FUNC2(Raw, DeleteQueries, void, GLsizei, n, const GLuint*, ids)
ION_WRAP_GL_FUNC1(Raw, EndQuery, void, GLenum, target)
ION_WRAP_GL_FUNC2(Raw, GenQueries, void, GLsizei, n, GLuint*, ids)
ION_WRAP_GL_FUNC3(Raw, GetQueryiv, void, GLenum, target, GLenum, pname,
                  GLint*, params)
ION_WRAP_GL_FUNC3(Raw, GetQueryObjecti64v, void, GLuint, id, GLenum, pname,
                  GLint64*, params)
ION_WRAP_GL_FUNC3(Raw, GetQueryObjectiv, void, GLuint, id, GLenum, pname,
                  GLint*, params)
ION_WRAP_GL_FUNC3(Raw, GetQueryObjectui64v, void, GLuint, id, GLenum, pname,
                  GLuint64*, params)
ION_WRAP_GL_FUNC3(Raw, GetQueryObjectuiv, void, GLuint, id, GLenum, pname,
                  GLuint*, params)
ION_WRAP_GL_FUNC1(Raw, IsQuery, GLboolean, GLuint, id)
ION_WRAP_GL_FUNC2(Raw, QueryCounter, void, GLuint, id, GLenum, target)
// Memory barriers.
#if defined(MemoryBarrier)
// NOTE: winnt.h defines a MemoryBarrier macro that interferes with
// properly loading glMemoryBarrier. We can't undef it, but we can
// use the push/pop macro pragma, which is non-standard but supported
// by all three of GCC, MSVC and Clang.
#pragma push_macro("MemoryBarrier")
#undef MemoryBarrier
ION_WRAP_GL_FUNC1(Raw, MemoryBarrier, void, GLbitfield, barriers)
#pragma pop_macro("MemoryBarrier")
#else
ION_WRAP_GL_FUNC1(Raw, MemoryBarrier, void, GLbitfield, barriers)
#endif  // defined(MemoryBarrier)

// Tessellation group.
ION_WRAP_GL_FUNC2(TessellationShader, PatchParameteri, void, GLenum, pname,
                  GLint, value)
ION_WRAP_GL_FUNC2(TessellationShader, PatchParameterfv, void, GLenum, pname,
                  GLfloat*, values)

#undef ION_TRACE_ARG
#undef ION_WRAP_GL_FUNC
#undef ION_WRAP_GL_FUNC0
#undef ION_WRAP_GL_FUNC1
#undef ION_WRAP_GL_FUNC2
#undef ION_WRAP_GL_FUNC3
#undef ION_WRAP_GL_FUNC4
#undef ION_WRAP_GL_FUNC5
#undef ION_WRAP_GL_FUNC6
#undef ION_WRAP_GL_FUNC7
#undef ION_WRAP_GL_FUNC8
#undef ION_WRAP_GL_FUNC9
#undef ION_WRAP_GL_FUNC10
#undef ION_WRAP_GL_FUNC11
#undef ION_WRAP_GL_FUNC12

#endif  // ION_GFX_GLFUNCTIONS_INC_

// Allow this file to be included again.
#undef ION_GFX_GLFUNCTIONS_INC_
