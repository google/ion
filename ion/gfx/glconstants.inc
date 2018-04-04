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

// Macros wrapping all OpenGL implementation limits (constant queries).

#if !defined(ION_WRAP_GL_VALUE)
#  error "ION_WRAP_GL_VALUE must be defined to include glconstants.inc"
#endif
#if !defined(ION_WRAP_GL_LIST)
#define ION_WRAP_GL_LIST(camel_name, snake_name, gl_enum, gl_count_enum) \
    ION_WRAP_GL_VALUE(camel_name, snake_name, gl_enum, std::vector<GLenum>, ())
#endif

// Expected macro signature:
// ION_WRAP_GL_VALUE(camel_name, snake_name, gl_enum, type, initializer)
// where:
// - camel_name is the name of the constant value in CamelCase.
// - snake_name is the name of the value in snake_case.
// - gl_enum is the GL constant of the corresponding query, such as
//   GL_MAX_3D_TEXTURE_SIZE.
// - type is one of: GLint, GLfloat, ion::math::Range1f, ion::math::Point2i,
//   ion::math::Vector3i. The last one is used only for the compute work group
//   limits and queried using indexed gets.
// - initializer is the default initializer of the limit for FakeGlContext.

ION_WRAP_GL_VALUE(AliasedLineWidthRange,
                  aliased_line_width_range,
                  GL_ALIASED_LINE_WIDTH_RANGE,
                  ion::math::Range1f, (1.f, 256.f))
ION_WRAP_GL_VALUE(AliasedPointSizeRange,
                  aliased_point_size_range,
                  GL_ALIASED_POINT_SIZE_RANGE,
                  ion::math::Range1f, (1.f, 8192.f))
ION_WRAP_GL_VALUE(Max3dTextureSize,
                  max_3d_texture_size,
                  GL_MAX_3D_TEXTURE_SIZE,
                  GLint, (4096))
ION_WRAP_GL_VALUE(MaxArrayTextureLayers,
                  max_array_texture_layers,
                  GL_MAX_ARRAY_TEXTURE_LAYERS,
                  GLint, (4096))
ION_WRAP_GL_VALUE(MaxClipDistances,
                  max_clip_distances,
                  GL_MAX_CLIP_DISTANCES,
                  GLuint, (8))
ION_WRAP_GL_VALUE(MaxColorAttachments,
                  max_color_attachments,
                  GL_MAX_COLOR_ATTACHMENTS,
                  GLuint, (4))
ION_WRAP_GL_VALUE(MaxCombinedComputeUniformComponents,
                  max_combined_compute_uniform_components,
                  GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS,
                  GLuint, (1024))

ION_WRAP_GL_VALUE(MaxCombinedTessControlUniformComponents,
                  max_combined_tess_control_uniform_components,
                  GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS,
                  GLuint,
                  (50176))
ION_WRAP_GL_VALUE(MaxCombinedTessEvaluationUniformComponents,
                  max_combined_tess_evaluation_uniform_components,
                  GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS,
                  GLuint,
                  (50176))
ION_WRAP_GL_VALUE(MaxCombinedTextureImageUnits,
                  max_combined_texture_image_units,
                  GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                  GLuint, (96))
ION_WRAP_GL_VALUE(MaxComputeImageUniforms,
                  max_compute_image_uniforms,
                  GL_MAX_COMPUTE_IMAGE_UNIFORMS,
                  GLuint, (8))
ION_WRAP_GL_VALUE(MaxComputeSharedMemorySize,
                  max_compute_shared_memory_size,
                  GL_MAX_COMPUTE_SHARED_MEMORY_SIZE,
                  GLuint, (32768))
ION_WRAP_GL_VALUE(MaxComputeTextureImageUnits,
                  max_compute_texture_image_units,
                  GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS,
                  GLuint, (16))
ION_WRAP_GL_VALUE(MaxComputeUniformBlocks,
                  max_compute_uniform_blocks,
                  GL_MAX_COMPUTE_UNIFORM_BLOCKS,
                  GLuint, (12))
ION_WRAP_GL_VALUE(MaxComputeUniformComponents,
                  max_compute_uniform_components,
                  GL_MAX_COMPUTE_UNIFORM_COMPONENTS,
                  GLuint, (512))
ION_WRAP_GL_VALUE(MaxComputeWorkGroupCount,
                  max_compute_work_group_count,
                  GL_MAX_COMPUTE_WORK_GROUP_COUNT,
                  ion::math::Vector3i, (65535, 65535, 65535))
ION_WRAP_GL_VALUE(MaxComputeWorkGroupInvocations,
                  max_compute_work_group_invocations,
                  GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                  GLuint, (1024))
ION_WRAP_GL_VALUE(MaxComputeWorkGroupSize,
                  max_compute_work_group_size,
                  GL_MAX_COMPUTE_WORK_GROUP_SIZE,
                  ion::math::Vector3i, (1024, 1024, 64))
ION_WRAP_GL_VALUE(MaxCubeMapTextureSize,
                  max_cube_map_texture_size,
                  GL_MAX_CUBE_MAP_TEXTURE_SIZE,
                  GLint, (8192))
ION_WRAP_GL_VALUE(MaxDebugLoggedMessages,
                  max_debug_logged_messages,
                  GL_MAX_DEBUG_LOGGED_MESSAGES,
                  GLint, (16))
ION_WRAP_GL_VALUE(MaxDebugMessageLength,
                  max_debug_message_length,
                  GL_MAX_DEBUG_MESSAGE_LENGTH,
                  GLint, (65535))
ION_WRAP_GL_VALUE(MaxDrawBuffers,
                  max_draw_buffers,
                  GL_MAX_DRAW_BUFFERS,
                  GLuint, (4))
ION_WRAP_GL_VALUE(MaxFragmentUniformComponents,
                  max_fragment_uniform_components,
                  GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
                  GLuint, (1024))
ION_WRAP_GL_VALUE(MaxFragmentUniformVectors,
                  max_fragment_uniform_vectors,
                  GL_MAX_FRAGMENT_UNIFORM_VECTORS,
                  GLuint, (256))
ION_WRAP_GL_VALUE(MaxPatchVertices,
                  max_patch_vertices,
                  GL_MAX_PATCH_VERTICES,
                  GLuint, (32))
ION_WRAP_GL_VALUE(MaxRenderbufferSize,
                  max_renderbuffer_size,
                  GL_MAX_RENDERBUFFER_SIZE,
                  GLint, (4096))
ION_WRAP_GL_VALUE(MaxSampleMaskWords,
                  max_sample_mask_words,
                  GL_MAX_SAMPLE_MASK_WORDS,
                  GLuint, (16))
ION_WRAP_GL_VALUE(MaxSamples,
                  max_samples,
                  GL_MAX_SAMPLES,
                  GLint, (16))
ION_WRAP_GL_VALUE(MaxServerWaitTimeout,
                  max_server_wait_timeout,
                  GL_MAX_SERVER_WAIT_TIMEOUT,
                  GLuint64, (0xFFFFFFFFFFFFFFFF))

ION_WRAP_GL_VALUE(MaxTessControlInputComponents,
                  max_tess_control_input_components,
                  GL_MAX_TESS_CONTROL_INPUT_COMPONENTS,
                  GLuint,
                  (128))
ION_WRAP_GL_VALUE(MaxTessControlOutputComponents,
                  max_tess_control_output_components,
                  GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS,
                  GLuint,
                  (128))
ION_WRAP_GL_VALUE(MaxTessControlTextureImageUnits,
                  max_tess_control_texture_image_units,
                  GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS,
                  GLuint,
                  (16))
ION_WRAP_GL_VALUE(MaxTessControlTotalOutputComponents,
                  max_tess_control_total_output_components,
                  GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS,
                  GLuint,
                  (4096))
ION_WRAP_GL_VALUE(MaxTessControlUniformBlocks,
                  max_tess_control_uniform_blocks,
                  GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS,
                  GLuint,
                  (12))
ION_WRAP_GL_VALUE(MaxTessControlUniformComponents,
                  max_tess_control_uniform_components,
                  GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS,
                  GLuint,
                  (1024))
ION_WRAP_GL_VALUE(MaxTessEvaluationInputComponents,
                  max_tess_evaluation_input_components,
                  GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS,
                  GLuint,
                  (128))
ION_WRAP_GL_VALUE(MaxTessEvaluationOutputComponents,
                  max_tess_evaluation_output_components,
                  GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS,
                  GLuint,
                  (128))
ION_WRAP_GL_VALUE(MaxTessEvaluationTextureImageUnits,
                  max_tess_evaluation_texture_image_units,
                  GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS,
                  GLuint,
                  (16))
ION_WRAP_GL_VALUE(MaxTessEvaluationUniformBlocks,
                  max_tess_evaluation_uniform_blocks,
                  GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS,
                  GLuint,
                  (12))
ION_WRAP_GL_VALUE(MaxTessEvaluationUniformComponents,
                  max_tess_evaluation_uniform_components,
                  GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS,
                  GLuint,
                  (1024))
ION_WRAP_GL_VALUE(MaxTessGenLevel,
                  max_tess_gen_level,
                  GL_MAX_TESS_GEN_LEVEL,
                  GLuint,
                  (64))
ION_WRAP_GL_VALUE(MaxTessPatchComponents,
                  max_tess_patch_components,
                  GL_MAX_TESS_PATCH_COMPONENTS,
                  GLuint,
                  (120))
ION_WRAP_GL_VALUE(MaxTextureImageUnits,
                  max_texture_image_units,
                  GL_MAX_TEXTURE_IMAGE_UNITS,
                  GLuint, (32))
ION_WRAP_GL_VALUE(MaxTextureMaxAnisotropy,
                  max_texture_max_anisotropy,
                  GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT,
                  GLfloat, (16.f))
ION_WRAP_GL_VALUE(MaxTextureSize,
                  max_texture_size,
                  GL_MAX_TEXTURE_SIZE,
                  GLint, (8192))
ION_WRAP_GL_VALUE(MaxTransformFeedbackInterleavedComponents,
                  max_transform_feedback_interleaved_components,
                  GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
                  GLint, (-1))
ION_WRAP_GL_VALUE(MaxTransformFeedbackSeparateAttribs,
                  max_transform_feedback_separate_attribs,
                  GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                  GLuint, (4))
ION_WRAP_GL_VALUE(MaxTransformFeedbackSeparateComponents,
                  max_transform_feedback_separate_components,
                  GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
                  GLint, (-1))
ION_WRAP_GL_VALUE(MaxUniformBufferBindings,
                  max_uniform_buffer_bindings,
                  GL_MAX_UNIFORM_BUFFER_BINDINGS,
                  GLuint, (8))
ION_WRAP_GL_VALUE(MaxVaryingVectors,
                  max_varying_vectors,
                  GL_MAX_VARYING_VECTORS,
                  GLuint, (15))
ION_WRAP_GL_VALUE(MaxVertexAttribs,
                  max_vertex_attribs,
                  GL_MAX_VERTEX_ATTRIBS,
                  GLuint, (32))
ION_WRAP_GL_VALUE(MaxVertexTextureImageUnits,
                  max_vertex_texture_image_units,
                  GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
                  GLuint, (32))
ION_WRAP_GL_VALUE(MaxVertexUniformComponents,
                  max_vertex_uniform_components,
                  GL_MAX_VERTEX_UNIFORM_COMPONENTS,
                  GLuint, (1536))
ION_WRAP_GL_VALUE(MaxVertexUniformVectors,
                  max_vertex_uniform_vectors,
                  GL_MAX_VERTEX_UNIFORM_VECTORS,
                  GLuint, (384))
ION_WRAP_GL_VALUE(MaxViewportDims,
                  max_viewport_dims,
                  GL_MAX_VIEWPORT_DIMS,
                  ion::math::Point2i, (8192, 8192))
ION_WRAP_GL_VALUE(MaxViews,
                  max_views,
                  GL_MAX_VIEWS_OVR,
                  GLint, (4))
ION_WRAP_GL_VALUE(TransformFeedbackVaryingMaxLength,
                  transform_feedback_varying_max_length,
                  GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH,
                  GLint, (-1))

// Expected macro signature:
// ION_WRAP_GL_VALUE(camel_name, snake_name, gl_enum, gl_count_enum)
// where:
// - camel_name is the name of the list in CamelCase
// - snake_name is the name of the list in snake_case
// - gl_enum is the GL constant of the corresponding query
// - gl_count_enum is the GL constant of the query that gives the number of
//   results that will be returned by the main query
ION_WRAP_GL_LIST(CompressedTextureFormats,
                 compressed_texture_formats,
                 GL_COMPRESSED_TEXTURE_FORMATS,
                 GL_NUM_COMPRESSED_TEXTURE_FORMATS)
ION_WRAP_GL_LIST(ShaderBinaryFormats,
                 shader_binary_formats,
                 GL_SHADER_BINARY_FORMATS,
                 GL_NUM_SHADER_BINARY_FORMATS)

#undef ION_WRAP_GL_VALUE
#undef ION_WRAP_GL_LIST
