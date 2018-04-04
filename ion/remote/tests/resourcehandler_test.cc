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

#include "absl/memory/memory.h"
#if !ION_PRODUCTION

#include "ion/remote/resourcehandler.h"

#include <atomic>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/base/stringutils.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/zipassetmanager.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/node.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfx/tests/testscene.h"
#include "ion/gfx/texture.h"
#include "ion/image/conversionutils.h"
#include "ion/port/semaphore.h"
#include "ion/portgfx/glcontext.h"
#include "ion/remote/tests/httpservertest.h"
#include "absl/base/macros.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace remote {

namespace {

using gfx::FramebufferObject;
using gfx::FramebufferObjectPtr;
using gfx::Image;
using gfx::ImagePtr;
using gfx::NodePtr;
using gfx::Renderer;
using gfx::RendererPtr;
using gfx::TexturePtr;
using gfx::testing::FakeGraphicsManager;
using gfx::testing::FakeGraphicsManagerPtr;
using gfx::testing::FakeGlContext;
using gfx::testing::TestScene;
using portgfx::GlContext;
using portgfx::GlContextPtr;

static const char kPlatformJson[] = R"JSON(
  "platform": [
    {
      "renderer": "Ion fake OpenGL / ES",
      "vendor": "Google",
      "version_string": "3.3 Ion OpenGL / ES",
      "gl_version": 3.3,
      "glsl_version": 110,
      "aliased_line_width_range": "1 - 256",
      "aliased_point_size_range": "1 - 8192",
      "max_3d_texture_size": 4096,
      "max_array_texture_layers": 4096,
      "max_clip_distances": 8,
      "max_color_attachments": 4,
      "max_combined_compute_uniform_components": 1024,
      "max_combined_tess_control_uniform_components": 50176,
      "max_combined_tess_evaluation_uniform_components": 50176,
      "max_combined_texture_image_units": 96,
      "max_compute_image_uniforms": 8,
      "max_compute_shared_memory_size": 32768,
      "max_compute_texture_image_units": 16,
      "max_compute_uniform_blocks": 12,
      "max_compute_uniform_components": 512,
      "max_compute_work_group_count": "65535 x 65535 x 65535",
      "max_compute_work_group_invocations": 1024,
      "max_compute_work_group_size": "1024 x 1024 x 64",
      "max_cube_map_texture_size": 8192,
      "max_debug_logged_messages": 16,
      "max_debug_message_length": 65535,
      "max_draw_buffers": 4,
      "max_fragment_uniform_components": 1024,
      "max_fragment_uniform_vectors": 256,
      "max_patch_vertices": 32,
      "max_renderbuffer_size": 4096,
      "max_sample_mask_words": 16,
      "max_samples": 16,
      "max_server_wait_timeout": 18446744073709551615,
      "max_tess_control_input_components": 128,
      "max_tess_control_output_components": 128,
      "max_tess_control_texture_image_units": 16,
      "max_tess_control_total_output_components": 4096,
      "max_tess_control_uniform_blocks": 12,
      "max_tess_control_uniform_components": 1024,
      "max_tess_evaluation_input_components": 128,
      "max_tess_evaluation_output_components": 128,
      "max_tess_evaluation_texture_image_units": 16,
      "max_tess_evaluation_uniform_blocks": 12,
      "max_tess_evaluation_uniform_components": 1024,
      "max_tess_gen_level": 64,
      "max_tess_patch_components": 120,
      "max_texture_image_units": 32,
      "max_texture_max_anisotropy": 16,
      "max_texture_size": 8192,
      "max_transform_feedback_interleaved_components": -1,
      "max_transform_feedback_separate_attribs": 4,
      "max_transform_feedback_separate_components": -1,
      "max_uniform_buffer_bindings": 8,
      "max_varying_vectors": 15,
      "max_vertex_attribs": 32,
      "max_vertex_texture_image_units": 32,
      "max_vertex_uniform_components": 1536,
      "max_vertex_uniform_vectors": 384,
      "max_viewport_dims": "8192 x 8192",
      "max_views": 4,
      "transform_feedback_varying_max_length": -1,
      "compressed_texture_formats": [
        "GL_COMPRESSED_RGB_S3TC_DXT1_EXT",
        "GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG",
        "GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG",
        "GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG",
        "GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG",
        "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT",
        "GL_ETC1_RGB8_OES",
        "GL_COMPRESSED_RGB8_ETC2",
        "GL_COMPRESSED_RGBA8_ETC2_EAC",
        "GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2"
      ],
      "shader_binary_formats": [
        "0xbadf00d"
      ],
      "extensions": [
        "GL_OES_blend_func_separate",
        "GL_OES_blend_subtract",
        "GL_APPLE_clip_distance",
        "GL_OES_compressed_ETC1_RGB8_texture",
        "GL_ARB_compute_shader",
        "GL_EXT_debug_label",
        "GL_EXT_debug_marker",
        "GL_ARB_debug_output",
        "GL_OES_depth24",
        "GL_OES_depth32",
        "GL_OES_depth_texture",
        "GL_EXT_discard_framebuffer",
        "GL_EXT_disjoint_timer_query",
        "GL_EXT_draw_buffers",
        "GL_EXT_draw_instanced",
        "GL_OES_EGL_image",
        "GL_OES_EGL_image_external",
        "GL_OES_element_index_uint",
        "GL_OES_fbo_render_mipmap",
        "GL_EXT_frag_depth",
        "GL_OES_fragment_precision_high",
        "GL_EXT_framebuffer_blit",
        "GL_QCOM_framebuffer_foveated",
        "GL_APPLE_framebuffer_multisample",
        "GL_EXT_framebuffer_multisample",
        "GL_OES_framebuffer_object",
        "GL_ARB_geometry_shader4",
        "GL_EXT_gpu_shader4",
        "GL_EXT_instanced_arrays",
        "GL_OES_map_buffer_range",
        "GL_OES_mapbuffer",
        "GL_ARB_multisample",
        "GL_EXT_multisampled_render_to_texture",
        "GL_OVR_multiview",
        "GL_OVR_multiview2",
        "GL_OVR_multiview_multisampled_render_to_texture",
        "GL_OES_packed_depth_stencil",
        "GL_EXT_protected_textures",
        "GL_OES_rgb8_rgba8",
        "GL_OES_sample_shading",
        "GL_EXT_shader_texture_lod",
        "GL_NV_sRGB_formats",
        "GL_OES_standard_derivatives",
        "GL_OES_stencil8",
        "GL_ARB_sync",
        "GL_OES_texture_3D",
        "GL_EXT_texture_array",
        "GL_NV_texture_barrier",
        "GL_EXT_texture_compression_dxt1",
        "GL_ANGLE_texture_compression_dxt5",
        "GL_IMG_texture_compression_pvrtc",
        "GL_EXT_texture_compression_s3tc",
        "GL_NV_texture_compression_s3tc",
        "GL_OES_texture_cube_map",
        "GL_ARB_texture_cube_map_array",
        "GL_EXT_texture_filter_anisotropic",
        "GL_OES_texture_float",
        "GL_QCOM_texture_foveated",
        "GL_OES_texture_half_float",
        "GL_EXT_texture_lod_bias",
        "GL_APPLE_texture_max_level",
        "GL_OES_texture_mirrored_repeat",
        "GL_ARB_texture_multisample",
        "GL_EXT_texture_rg",
        "GL_OES_texture_stencil8",
        "GL_EXT_texture_storage",
        "GL_ARB_texture_storage_multisample",
        "GL_ARB_texture_swizzle",
        "GL_EXT_texture_type_2_10_10_10_REV",
        "GL_QCOM_tiled_rendering",
        "GL_ARB_transform_feedback2",
        "GL_OES_vertex_array_object"
      ]
    }
  ])JSON";

// A string that represents no resources.
static const char kNoResourcesJson[] = R"JSON(
  "buffers": [
  ],
  "framebuffers": [
  ],
  "programs": [
  ],
  "samplers": [
  ],
  "shaders": [
  ],
  "textures": [
  ],
  "vertex_arrays": [
  ])JSON";

static const char kBuffersJson[] = R"JSON(
  "buffers": [
    {
      "object_id": 1,
      "label": "",
      "size": {vertex_buffer_size},
      "usage": "GL_STATIC_DRAW",
      "mapped_pointer": "NULL",
      "target": "GL_ARRAY_BUFFER"
    },
    {
      "object_id": 2,
      "label": "Vertex buffer",
      "size": {vertex_buffer_size},
      "usage": "GL_STATIC_DRAW",
      "mapped_pointer": "NULL",
      "target": "GL_ARRAY_BUFFER"
    },
    {
      "object_id": 3,
      "label": "Indices #0",
      "size": 24,
      "usage": "GL_STATIC_DRAW",
      "mapped_pointer": "NULL",
      "target": "GL_ELEMENT_ARRAY_BUFFER"
    }
  ])JSON";

static const char kFramebuffersJson[] = R"JSON(
  "framebuffers": [
    {
      "object_id": 1,
      "label": "",
      "attachment_color0": {
        "type": "GL_TEXTURE",
        "texture_glid": 1,
        "mipmap_level": 0,
        "cube_face": "GL_NONE",
        "layer": 0,
        "num_views": 0,
        "texture_samples": 0
      },
      "attachment_color1": {
        "type": "GL_NONE"
      },
      "attachment_color2": {
        "type": "GL_NONE"
      },
      "attachment_color3": {
        "type": "GL_NONE"
      },
      "attachment_depth": {
        "type": "GL_RENDERBUFFER",
        "renderbuffer": {
          "object_id": 1,
          "label": "",
          "width": 2,
          "height": 2,
          "internal_format": "GL_DEPTH_COMPONENT16",
          "red_size": 0,
          "green_size": 0,
          "blue_size": 0,
          "alpha_size": 0,
          "depth_size": 16,
          "stencil_size": 0
        }
      },
      "attachment_stencil": {
        "type": "GL_NONE"
      },
      "draw_buffers": "GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_NONE",
      "read_buffer": "GL_COLOR_ATTACHMENT0"
    }
  ])JSON";

// Note that each column of a matrix attribute gets its own index.
static const char kProgramsJson[] = R"JSON(
  "programs": [
    {
      "object_id": 1,
      "label": "Default Renderer shader",
      "vertex_shader_glid": 1,
      "geometry_shader_glid": 0,
      "fragment_shader_glid": 2,
      "delete_status": "GL_FALSE",
      "link_status": "GL_TRUE",
      "validate_status": "GL_FALSE",
      "attributes": [
        {
          "name": "aVertex",
          "index": 0,
          "size": 1,
          "type": "GL_FLOAT_VEC3"
        }
      ],
      "uniforms": [
        {
          "value": "M[1, 2, 3, 4 ; 5, 1, 7, 8 ; 9, 1, 1, 3 ; 4, 5, 6, 1]",
          "name": "uProjectionMatrix",
          "index": 0,
          "size": 1,
          "type": "GL_FLOAT_MAT4"
        },
        {
          "value": "M[4, 2, 3, 4 ; 5, 4, 7, 8 ; 9, 1, 4, 3 ; 4, 5, 6, 4]",
          "name": "uModelviewMatrix",
          "index": 1,
          "size": 1,
          "type": "GL_FLOAT_MAT4"
        },
        {
          "value": "V[4, 3, 2, 1]",
          "name": "uBaseColor",
          "index": 2,
          "size": 1,
          "type": "GL_FLOAT_VEC4"
        }
      ],
      "info_log": ""
    },
    {
      "object_id": 2,
      "label": "Dummy Shader",
      "vertex_shader_glid": 3,
      "geometry_shader_glid": 4,
      "fragment_shader_glid": 5,
      "delete_status": "GL_FALSE",
      "link_status": "GL_TRUE",
      "validate_status": "GL_FALSE",
      "attributes": [
        {
          "name": "aFloat",
          "index": 0,
          "size": 1,
          "type": "GL_FLOAT"
        },
        {
          "name": "aFV2",
          "index": 1,
          "size": 1,
          "type": "GL_FLOAT_VEC2"
        },
        {
          "name": "aFV3",
          "index": 2,
          "size": 1,
          "type": "GL_FLOAT_VEC3"
        },
        {
          "name": "aFV4",
          "index": 3,
          "size": 1,
          "type": "GL_FLOAT_VEC4"
        },
        {
          "name": "aMat2",
          "index": 4,
          "size": 1,
          "type": "GL_FLOAT_MAT2"
        },
        {
          "name": "aMat3",
          "index": 6,
          "size": 1,
          "type": "GL_FLOAT_MAT3"
        },
        {
          "name": "aMat4",
          "index": 9,
          "size": 1,
          "type": "GL_FLOAT_MAT4"
        },
        {
          "name": "aBOE1",
          "index": 13,
          "size": 1,
          "type": "GL_FLOAT_VEC2"
        },
        {
          "name": "aBOE2",
          "index": 14,
          "size": 1,
          "type": "GL_FLOAT_VEC3"
        }
      ],
      "uniforms": [
        {
          "value": "13",
          "name": "uInt",
          "index": 0,
          "size": 1,
          "type": "GL_INT"
        },
        {
          "value": "1.5",
          "name": "uFloat",
          "index": 1,
          "size": 1,
          "type": "GL_FLOAT"
        },
        {
          "value": "27",
          "name": "uIntGS",
          "index": 2,
          "size": 1,
          "type": "GL_INT"
        },
        {
          "value": "33",
          "name": "uUintGS",
          "index": 3,
          "size": 1,
          "type": "GL_UNSIGNED_INT"
        },
        {
          "value": "V[2, 3]",
          "name": "uFV2",
          "index": 4,
          "size": 1,
          "type": "GL_FLOAT_VEC2"
        },
        {
          "value": "V[4, 5, 6]",
          "name": "uFV3",
          "index": 5,
          "size": 1,
          "type": "GL_FLOAT_VEC3"
        },
        {
          "value": "V[7, 8, 9, 10]",
          "name": "uFV4",
          "index": 6,
          "size": 1,
          "type": "GL_FLOAT_VEC4"
        },
        {
          "value": "15",
          "name": "uUint",
          "index": 7,
          "size": 1,
          "type": "GL_UNSIGNED_INT"
        },
        {
          "value": "1",
          "name": "uCubeMapTex",
          "index": 8,
          "size": 1,
          "type": "GL_SAMPLER_CUBE"
        },
        {
          "value": "2",
          "name": "uTex",
          "index": 9,
          "size": 1,
          "type": "GL_SAMPLER_2D"
        },
        {
          "value": "V[2, 3]",
          "name": "uIV2",
          "index": 10,
          "size": 1,
          "type": "GL_INT_VEC2"
        },
        {
          "value": "V[4, 5, 6]",
          "name": "uIV3",
          "index": 11,
          "size": 1,
          "type": "GL_INT_VEC3"
        },
        {
          "value": "V[7, 8, 9, 10]",
          "name": "uIV4",
          "index": 12,
          "size": 1,
          "type": "GL_INT_VEC4"
        },
        {
          "value": "V[2, 3]",
          "name": "uUV2",
          "index": 13,
          "size": 1,
          "type": "GL_UNSIGNED_INT_VEC2"
        },
        {
          "value": "V[4, 5, 6]",
          "name": "uUV3",
          "index": 14,
          "size": 1,
          "type": "GL_UNSIGNED_INT_VEC3"
        },
        {
          "value": "V[7, 8, 9, 10]",
          "name": "uUV4",
          "index": 15,
          "size": 1,
          "type": "GL_UNSIGNED_INT_VEC4"
        },
        {
          "value": "M[1, 2 ; 3, 4]",
          "name": "uMat2",
          "index": 16,
          "size": 1,
          "type": "GL_FLOAT_MAT2"
        },
        {
          "value": "M[1, 2, 3 ; 4, 5, 6 ; 7, 8, 9]",
          "name": "uMat3",
          "index": 17,
          "size": 1,
          "type": "GL_FLOAT_MAT3"
        },
        {
          "value": "M[1, 2, 3, 4 ; 5, 6, 7, 8 ; 9, 1, 2, 3 ; 4, 5, 6, 7]",
          "name": "uMat4",
          "index": 18,
          "size": 1,
          "type": "GL_FLOAT_MAT4"
        },
        {
          "value": "[1, 2]",
          "name": "uIntArray",
          "index": 19,
          "size": 2,
          "type": "GL_INT"
        },
        {
          "value": "[3, 4]",
          "name": "uUintArray",
          "index": 21,
          "size": 2,
          "type": "GL_UNSIGNED_INT"
        },
        {
          "value": "[1, 2]",
          "name": "uFloatArray",
          "index": 23,
          "size": 2,
          "type": "GL_FLOAT"
        },
        {
          "value": "[3, 4]",
          "name": "uCubeMapTexArray",
          "index": 25,
          "size": 2,
          "type": "GL_SAMPLER_CUBE"
        },
        {
          "value": "[5, 6]",
          "name": "uTexArray",
          "index": 27,
          "size": 2,
          "type": "GL_SAMPLER_2D"
        },
        {
          "value": "[V[1, 2], V[3, 4]]",
          "name": "uFV2Array",
          "index": 29,
          "size": 2,
          "type": "GL_FLOAT_VEC2"
        },
        {
          "value": "[V[1, 2, 3], V[4, 5, 6]]",
          "name": "uFV3Array",
          "index": 31,
          "size": 2,
          "type": "GL_FLOAT_VEC3"
        },
        {
          "value": "[V[1, 2, 3, 4], V[5, 6, 7, 8]]",
          "name": "uFV4Array",
          "index": 33,
          "size": 2,
          "type": "GL_FLOAT_VEC4"
        },
        {
          "value": "[V[1, 2], V[3, 4]]",
          "name": "uIV2Array",
          "index": 35,
          "size": 2,
          "type": "GL_INT_VEC2"
        },
        {
          "value": "[V[1, 2, 3], V[4, 5, 6]]",
          "name": "uIV3Array",
          "index": 37,
          "size": 2,
          "type": "GL_INT_VEC3"
        },
        {
          "value": "[V[1, 2, 3, 4], V[5, 6, 7, 8]]",
          "name": "uIV4Array",
          "index": 39,
          "size": 2,
          "type": "GL_INT_VEC4"
        },
        {
          "value": "[V[1, 2], V[3, 4]]",
          "name": "uUV2Array",
          "index": 41,
          "size": 2,
          "type": "GL_UNSIGNED_INT_VEC2"
        },
        {
          "value": "[V[1, 2, 3], V[4, 5, 6]]",
          "name": "uUV3Array",
          "index": 43,
          "size": 2,
          "type": "GL_UNSIGNED_INT_VEC3"
        },
        {
          "value": "[V[1, 2, 3, 4], V[5, 6, 7, 8]]",
          "name": "uUV4Array",
          "index": 45,
          "size": 2,
          "type": "GL_UNSIGNED_INT_VEC4"
        },
        {
          "value": "[M[1, 0 ; 0, 1], M[2, 0 ; 0, 2]]",
          "name": "uMat2Array",
          "index": 47,
          "size": 2,
          "type": "GL_FLOAT_MAT2"
        },
        {
          "value": "[M[1, 0, 0 ; 0, 1, 0 ; 0, 0, 1], M[2, 0, 0 ; 0, 2, 0 ; 0, 0, 2]]",
          "name": "uMat3Array",
          "index": 49,
          "size": 2,
          "type": "GL_FLOAT_MAT3"
        },
        {
          "value": "[M[1, 0, 0, 0 ; 0, 1, 0, 0 ; 0, 0, 1, 0 ; 0, 0, 0, 1], M[2, 0, 0, 0 ; 0, 2, 0, 0 ; 0, 0, 2, 0 ; 0, 0, 0, 2]]",
          "name": "uMat4Array",
          "index": 51,
          "size": 2,
          "type": "GL_FLOAT_MAT4"
        }
      ],
      "info_log": ""
    }
  ])JSON";

static const char kSamplersJson[] = R"JSON(
  "samplers": [
    {
      "object_id": 1,
      "label": "Sampler",
      "compare_function": "GL_NEVER",
      "compare_mode": "GL_COMPARE_REF_TO_TEXTURE",
      "max_anisotropy": 1,
      "min_lod": -0.5,
      "max_lod": 0.5,
      "min_filter": "GL_LINEAR_MIPMAP_LINEAR",
      "mag_filter": "GL_NEAREST",
      "wrap_r": "GL_MIRRORED_REPEAT",
      "wrap_s": "GL_MIRRORED_REPEAT",
      "wrap_t": "GL_CLAMP_TO_EDGE"
    },
    {
      "object_id": 2,
      "label": "Cubemap Sampler",
      "compare_function": "GL_NEVER",
      "compare_mode": "GL_COMPARE_REF_TO_TEXTURE",
      "max_anisotropy": 1,
      "min_lod": -1.5,
      "max_lod": 1.5,
      "min_filter": "GL_LINEAR_MIPMAP_LINEAR",
      "mag_filter": "GL_NEAREST",
      "wrap_r": "GL_CLAMP_TO_EDGE",
      "wrap_s": "GL_MIRRORED_REPEAT",
      "wrap_t": "GL_CLAMP_TO_EDGE"
    },
    {
      "object_id": 3,
      "label": "Sampler",
      "compare_function": "GL_NEVER",
      "compare_mode": "GL_COMPARE_REF_TO_TEXTURE",
      "max_anisotropy": 1,
      "min_lod": -0.5,
      "max_lod": 0.5,
      "min_filter": "GL_LINEAR_MIPMAP_LINEAR",
      "mag_filter": "GL_NEAREST",
      "wrap_r": "GL_MIRRORED_REPEAT",
      "wrap_s": "GL_MIRRORED_REPEAT",
      "wrap_t": "GL_CLAMP_TO_EDGE"
    },
    {
      "object_id": 4,
      "label": "Cubemap Sampler",
      "compare_function": "GL_NEVER",
      "compare_mode": "GL_COMPARE_REF_TO_TEXTURE",
      "max_anisotropy": 1,
      "min_lod": -1.5,
      "max_lod": 1.5,
      "min_filter": "GL_LINEAR_MIPMAP_LINEAR",
      "mag_filter": "GL_NEAREST",
      "wrap_r": "GL_CLAMP_TO_EDGE",
      "wrap_s": "GL_MIRRORED_REPEAT",
      "wrap_t": "GL_CLAMP_TO_EDGE"
    },
    {
      "object_id": 5,
      "label": "Cubemap Sampler",
      "compare_function": "GL_NEVER",
      "compare_mode": "GL_COMPARE_REF_TO_TEXTURE",
      "max_anisotropy": 1,
      "min_lod": -1.5,
      "max_lod": 1.5,
      "min_filter": "GL_LINEAR_MIPMAP_LINEAR",
      "mag_filter": "GL_NEAREST",
      "wrap_r": "GL_CLAMP_TO_EDGE",
      "wrap_s": "GL_MIRRORED_REPEAT",
      "wrap_t": "GL_CLAMP_TO_EDGE"
    },
    {
      "object_id": 6,
      "label": "Sampler",
      "compare_function": "GL_NEVER",
      "compare_mode": "GL_COMPARE_REF_TO_TEXTURE",
      "max_anisotropy": 1,
      "min_lod": -0.5,
      "max_lod": 0.5,
      "min_filter": "GL_LINEAR_MIPMAP_LINEAR",
      "mag_filter": "GL_NEAREST",
      "wrap_r": "GL_MIRRORED_REPEAT",
      "wrap_s": "GL_MIRRORED_REPEAT",
      "wrap_t": "GL_CLAMP_TO_EDGE"
    },
    {
      "object_id": 7,
      "label": "Sampler",
      "compare_function": "GL_NEVER",
      "compare_mode": "GL_COMPARE_REF_TO_TEXTURE",
      "max_anisotropy": 1,
      "min_lod": -0.5,
      "max_lod": 0.5,
      "min_filter": "GL_LINEAR_MIPMAP_LINEAR",
      "mag_filter": "GL_NEAREST",
      "wrap_r": "GL_MIRRORED_REPEAT",
      "wrap_s": "GL_MIRRORED_REPEAT",
      "wrap_t": "GL_CLAMP_TO_EDGE"
    }
  ])JSON";

static const char kShadersJson[] = R"JSON(
  "shaders": [
    {
      "object_id": 1,
      "label": "Default Renderer vertex shader",
      "type": "GL_VERTEX_SHADER",
      "delete_status": "GL_FALSE",
      "compile_status": "GL_TRUE",
      "source": "PHByZT48Y29kZT51bmlmb3JtIG1hdDQgdVByb2plY3Rpb25NYXRyaXg7CnVuaWZvcm0gbWF0NCB1TW9kZWx2aWV3TWF0cml4OwphdHRyaWJ1dGUgdmVjMyBhVmVydGV4OwoKdm9pZCBtYWluKHZvaWQpIHsKICBnbF9Qb3NpdGlvbiA9IHVQcm9qZWN0aW9uTWF0cml4ICogdU1vZGVsdmlld01hdHJpeCAqCiAgICAgIHZlYzQoYVZlcnRleCwgMS4pOwp9CjwvY29kZT48L3ByZT4=",
      "info_log": ""
    },
    {
      "object_id": 2,
      "label": "Default Renderer fragment shader",
      "type": "GL_FRAGMENT_SHADER",
      "delete_status": "GL_FALSE",
      "compile_status": "GL_TRUE",
      "source": "PHByZT48Y29kZT4jaWZkZWYgR0xfRVMKcHJlY2lzaW9uIG1lZGl1bXAgZmxvYXQ7CiNlbmRpZgoKdW5pZm9ybSB2ZWM0IHVCYXNlQ29sb3I7Cgp2b2lkIG1haW4odm9pZCkgewogIGdsX0ZyYWdDb2xvciA9IHVCYXNlQ29sb3I7Cn0KPC9jb2RlPjwvcHJlPg==",
      "info_log": ""
    },
    {
      "object_id": 3,
      "label": "Vertex shader",
      "type": "GL_VERTEX_SHADER",
      "delete_status": "GL_FALSE",
      "compile_status": "GL_TRUE",
      "source": "PHByZT48Y29kZT5hdHRyaWJ1dGUgZmxvYXQgYUZsb2F0OwphdHRyaWJ1dGUgdmVjMiBhRlYyOwphdHRyaWJ1dGUgdmVjMyBhRlYzOwphdHRyaWJ1dGUgdmVjNCBhRlY0OwphdHRyaWJ1dGUgbWF0MiBhTWF0MjsKYXR0cmlidXRlIG1hdDMgYU1hdDM7CmF0dHJpYnV0ZSBtYXQ0IGFNYXQ0OwphdHRyaWJ1dGUgdmVjMiBhQk9FMTsKYXR0cmlidXRlIHZlYzMgYUJPRTI7CnVuaWZvcm0gaW50IHVJbnQ7CnVuaWZvcm0gZmxvYXQgdUZsb2F0Owo8L2NvZGU+PC9wcmU+",
      "info_log": ""
    },
    {
      "object_id": 4,
      "label": "Geometry shader",
      "type": "GL_GEOMETRY_SHADER",
      "delete_status": "GL_FALSE",
      "compile_status": "GL_TRUE",
      "source": "PHByZT48Y29kZT51bmlmb3JtIGludCB1SW50R1M7CnVuaWZvcm0gdWludCB1VWludEdTOwp1bmlmb3JtIHZlYzIgdUZWMjsKdW5pZm9ybSB2ZWMzIHVGVjM7CnVuaWZvcm0gdmVjNCB1RlY0Owo8L2NvZGU+PC9wcmU+",
      "info_log": ""
    },
    {
      "object_id": 5,
      "label": "Fragment shader",
      "type": "GL_FRAGMENT_SHADER",
      "delete_status": "GL_FALSE",
      "compile_status": "GL_TRUE",
      "source": "PHByZT48Y29kZT51bmlmb3JtIGludCB1SW50Owp1bmlmb3JtIHVpbnQgdVVpbnQ7CnVuaWZvcm0gZmxvYXQgdUZsb2F0Owp1bmlmb3JtIHNhbXBsZXJDdWJlIHVDdWJlTWFwVGV4Owp1bmlmb3JtIHNhbXBsZXIyRCB1VGV4Owp1bmlmb3JtIHZlYzIgdUZWMjsKdW5pZm9ybSB2ZWMzIHVGVjM7CnVuaWZvcm0gdmVjNCB1RlY0Owp1bmlmb3JtIGl2ZWMyIHVJVjI7CnVuaWZvcm0gaXZlYzMgdUlWMzsKdW5pZm9ybSBpdmVjNCB1SVY0Owp1bmlmb3JtIHV2ZWMyIHVVVjI7CnVuaWZvcm0gdXZlYzMgdVVWMzsKdW5pZm9ybSB1dmVjNCB1VVY0Owp1bmlmb3JtIG1hdDIgdU1hdDI7CnVuaWZvcm0gbWF0MyB1TWF0MzsKdW5pZm9ybSBtYXQ0IHVNYXQ0Owp1bmlmb3JtIGludCB1SW50QXJyYXlbMl07CnVuaWZvcm0gdWludCB1VWludEFycmF5WzJdOwp1bmlmb3JtIGZsb2F0IHVGbG9hdEFycmF5WzJdOwp1bmlmb3JtIHNhbXBsZXJDdWJlIHVDdWJlTWFwVGV4QXJyYXlbMl07CnVuaWZvcm0gc2FtcGxlcjJEIHVUZXhBcnJheVsyXTsKdW5pZm9ybSB2ZWMyIHVGVjJBcnJheVsyXTsKdW5pZm9ybSB2ZWMzIHVGVjNBcnJheVsyXTsKdW5pZm9ybSB2ZWM0IHVGVjRBcnJheVsyXTsKdW5pZm9ybSBpdmVjMiB1SVYyQXJyYXlbMl07CnVuaWZvcm0gaXZlYzMgdUlWM0FycmF5WzJdOwp1bmlmb3JtIGl2ZWM0IHVJVjRBcnJheVsyXTsKdW5pZm9ybSB1dmVjMiB1VVYyQXJyYXlbMl07CnVuaWZvcm0gdXZlYzMgdVVWM0FycmF5WzJdOwp1bmlmb3JtIHV2ZWM0IHVVVjRBcnJheVsyXTsKdW5pZm9ybSBtYXQyIHVNYXQyQXJyYXlbMl07CnVuaWZvcm0gbWF0MyB1TWF0M0FycmF5WzJdOwp1bmlmb3JtIG1hdDQgdU1hdDRBcnJheVsyXTsKPC9jb2RlPjwvcHJlPg==",
      "info_log": ""
    }
  ])JSON";

static const char kTexturesJson[] = R"JSON(
  "textures": [
    {
      "object_id": 1,
      "label": "Texture",
      "width": 2,
      "height": 2,
      "format": "Rgb888",
      "sampler_glid": 1,
      "base_level": 10,
      "max_level": 100,
      "compare_function": "GL_LESS",
      "compare_mode": "GL_NONE",
      "is_protected": "GL_FALSE",
      "max_anisotropy": 1,
      "min_lod": -1000,
      "max_lod": 1000,
      "min_filter": "GL_NEAREST_MIPMAP_LINEAR",
      "mag_filter": "GL_LINEAR",
      "swizzle_red": "GL_ALPHA",
      "swizzle_green": "GL_BLUE",
      "swizzle_blue": "GL_GREEN",
      "swizzle_alpha": "GL_RED",
      "wrap_r": "GL_REPEAT",
      "wrap_s": "GL_REPEAT",
      "wrap_t": "GL_REPEAT",
      "target": "GL_TEXTURE_2D",
      "last_image_unit": "GL_TEXTURE{texture_unit1}"
    },
    {
      "object_id": 2,
      "label": "Cubemap",
      "width": 2,
      "height": 2,
      "format": "Rgb888",
      "sampler_glid": 2,
      "base_level": 10,
      "max_level": 100,
      "compare_function": "GL_LESS",
      "compare_mode": "GL_NONE",
      "is_protected": "GL_FALSE",
      "max_anisotropy": 1,
      "min_lod": -1000,
      "max_lod": 1000,
      "min_filter": "GL_NEAREST_MIPMAP_LINEAR",
      "mag_filter": "GL_LINEAR",
      "swizzle_red": "GL_ALPHA",
      "swizzle_green": "GL_BLUE",
      "swizzle_blue": "GL_GREEN",
      "swizzle_alpha": "GL_RED",
      "wrap_r": "GL_REPEAT",
      "wrap_s": "GL_REPEAT",
      "wrap_t": "GL_REPEAT",
      "target": "GL_TEXTURE_CUBE_MAP",
      "last_image_unit": "GL_TEXTURE{texture_unit3}"
    },
    {
      "object_id": 3,
      "label": "Texture",
      "width": 2,
      "height": 2,
      "format": "Rgb888",
      "sampler_glid": 3,
      "base_level": 10,
      "max_level": 100,
      "compare_function": "GL_LESS",
      "compare_mode": "GL_NONE",
      "is_protected": "GL_FALSE",
      "max_anisotropy": 1,
      "min_lod": -1000,
      "max_lod": 1000,
      "min_filter": "GL_NEAREST_MIPMAP_LINEAR",
      "mag_filter": "GL_LINEAR",
      "swizzle_red": "GL_ALPHA",
      "swizzle_green": "GL_BLUE",
      "swizzle_blue": "GL_GREEN",
      "swizzle_alpha": "GL_RED",
      "wrap_r": "GL_REPEAT",
      "wrap_s": "GL_REPEAT",
      "wrap_t": "GL_REPEAT",
      "target": "GL_TEXTURE_2D",
      "last_image_unit": "GL_TEXTURE{texture_unit2}"
    },
    {
      "object_id": 4,
      "label": "Cubemap",
      "width": 2,
      "height": 2,
      "format": "Rgb888",
      "sampler_glid": 4,
      "base_level": 10,
      "max_level": 100,
      "compare_function": "GL_LESS",
      "compare_mode": "GL_NONE",
      "is_protected": "GL_FALSE",
      "max_anisotropy": 1,
      "min_lod": -1000,
      "max_lod": 1000,
      "min_filter": "GL_NEAREST_MIPMAP_LINEAR",
      "mag_filter": "GL_LINEAR",
      "swizzle_red": "GL_ALPHA",
      "swizzle_green": "GL_BLUE",
      "swizzle_blue": "GL_GREEN",
      "swizzle_alpha": "GL_RED",
      "wrap_r": "GL_REPEAT",
      "wrap_s": "GL_REPEAT",
      "wrap_t": "GL_REPEAT",
      "target": "GL_TEXTURE_CUBE_MAP",
      "last_image_unit": "GL_TEXTURE{texture_unit4}"
    },
    {
      "object_id": 5,
      "label": "Cubemap",
      "width": 2,
      "height": 2,
      "format": "Rgb888",
      "sampler_glid": 5,
      "base_level": 10,
      "max_level": 100,
      "compare_function": "GL_LESS",
      "compare_mode": "GL_NONE",
      "is_protected": "GL_FALSE",
      "max_anisotropy": 1,
      "min_lod": -1000,
      "max_lod": 1000,
      "min_filter": "GL_NEAREST_MIPMAP_LINEAR",
      "mag_filter": "GL_LINEAR",
      "swizzle_red": "GL_ALPHA",
      "swizzle_green": "GL_BLUE",
      "swizzle_blue": "GL_GREEN",
      "swizzle_alpha": "GL_RED",
      "wrap_r": "GL_REPEAT",
      "wrap_s": "GL_REPEAT",
      "wrap_t": "GL_REPEAT",
      "target": "GL_TEXTURE_CUBE_MAP",
      "last_image_unit": "GL_TEXTURE{texture_unit5}"
    },
    {
      "object_id": 6,
      "label": "Texture",
      "width": 2,
      "height": 2,
      "format": "Rgb888",
      "sampler_glid": 6,
      "base_level": 10,
      "max_level": 100,
      "compare_function": "GL_LESS",
      "compare_mode": "GL_NONE",
      "is_protected": "GL_FALSE",
      "max_anisotropy": 1,
      "min_lod": -1000,
      "max_lod": 1000,
      "min_filter": "GL_NEAREST_MIPMAP_LINEAR",
      "mag_filter": "GL_LINEAR",
      "swizzle_red": "GL_ALPHA",
      "swizzle_green": "GL_BLUE",
      "swizzle_blue": "GL_GREEN",
      "swizzle_alpha": "GL_RED",
      "wrap_r": "GL_REPEAT",
      "wrap_s": "GL_REPEAT",
      "wrap_t": "GL_REPEAT",
      "target": "GL_TEXTURE_2D",
      "last_image_unit": "GL_TEXTURE{texture_unit6}"
    },
    {
      "object_id": 7,
      "label": "Texture",
      "width": 2,
      "height": 2,
      "format": "Rgb888",
      "sampler_glid": 7,
      "base_level": 10,
      "max_level": 100,
      "compare_function": "GL_LESS",
      "compare_mode": "GL_NONE",
      "is_protected": "GL_FALSE",
      "max_anisotropy": 1,
      "min_lod": -1000,
      "max_lod": 1000,
      "min_filter": "GL_NEAREST_MIPMAP_LINEAR",
      "mag_filter": "GL_LINEAR",
      "swizzle_red": "GL_ALPHA",
      "swizzle_green": "GL_BLUE",
      "swizzle_blue": "GL_GREEN",
      "swizzle_alpha": "GL_RED",
      "wrap_r": "GL_REPEAT",
      "wrap_s": "GL_REPEAT",
      "wrap_t": "GL_REPEAT",
      "target": "GL_TEXTURE_2D",
      "last_image_unit": "GL_TEXTURE{texture_unit7}"
    }
  ])JSON";


// MSVC cannot handle string literals longer than 16380 bytes, so we need to
// split this into two literals in the middle.
// https://msdn.microsoft.com/en-us/library/dddywwsc.aspx
static const char kVertexArraysJson[] = R"JSON(
  "vertex_arrays": [
    {
      "object_id": 2,
      "label": "",
      "vertex_count": 3,
      "attributes": [
        {
          "buffer_glid": 1,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": {vertex_buffer_stride},
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 1,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 2,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 2, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 2, 3, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 2, 3, 4]"
        },

        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 2,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 3, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 2,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[2, 4, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 4, 7, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[2, 5, 8, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[3, 6, 9, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 5, 9, 4]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[2, 6, 1, 5]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[3, 7, 2, 6]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[4, 8, 3, 7]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        }
      ]
    },)JSON" R"JSON(
    {
      "object_id": 3,
      "label": "Vertex array",
      "vertex_count": 3,
      "attributes": [
        {
          "buffer_glid": 2,
          "enabled": "GL_TRUE",
          "size": 1,
          "stride": {vertex_buffer_stride},
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 2,
          "enabled": "GL_TRUE",
          "size": 2,
          "stride": {vertex_buffer_stride},
          "type": "GL_FLOAT",
          "normalized": "GL_TRUE",
          "pointer_or_offset": "{pointer_or_offset}",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 1,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 2,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 2, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 2, 3, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 2, 3, 4]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 2,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 3, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 2,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[2, 4, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 4, 7, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[2, 5, 8, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 3,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[3, 6, 9, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[1, 5, 9, 4]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[2, 6, 1, 5]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[3, 7, 2, 6]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_TRUE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[4, 8, 3, 7]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        },
        {
          "buffer_glid": 0,
          "enabled": "GL_FALSE",
          "size": 4,
          "stride": 0,
          "type": "GL_FLOAT",
          "normalized": "GL_FALSE",
          "pointer_or_offset": "NULL",
          "value": "V[0, 0, 0, 1]"
        }
      ]
    }
  ])JSON";

static const char kJoinJson[] = ",\n";
static const char kPrefixJson[] = "{\n";
static const char kSuffixJson[] = "\n}\n";

static const char* SkipInitialNewline(const char* json_string) {
  return json_string && json_string[0] == '\n' ? json_string + 1 : json_string;
}

#if !ION_PRODUCTION
// Returns a string containing a hex representation of a size_t.
static const std::string ToHexString(size_t n) {
  std::ostringstream out;
  out << "0x" << std::hex << n;
  return out.str();
}
#endif

// Returns a PNG representation of a blank 2x3[x3] image.
static const std::string GetTestImagePng() {
  // Since FakeGraphicsManager does not actually write data, use an array of
  // zeros.
  static const int kNumBytes = 2 * 2 * 3;
  uint8 pixels[kNumBytes];
  memset(pixels, 0, kNumBytes);
  ImagePtr image(new Image);
  image->Set(Image::kRgb888, 2, 2,
             base::DataContainer::CreateAndCopy<uint8>(pixels, kNumBytes, false,
                                                       image->GetAllocator()));

  const std::vector<uint8> png_data =
      image::ConvertToExternalImageData(image, image::kPng, false);
  return base::MimeBase64EncodeString(
      std::string(png_data.begin(), png_data.end()));
}

static const std::string GetTestCubeMapImagePng() {
  // Since FakeGraphicsManager does not actually write data, use an array of
  // zeros.
  static const int kNumBytes = 6 * 8 * 3;
  uint8 pixels[kNumBytes];
  memset(pixels, 0, kNumBytes);
  ImagePtr image(new Image);
  image->Set(Image::kRgb888, 6, 8,
             base::DataContainer::CreateAndCopy<uint8>(pixels, kNumBytes, false,
                                                       image->GetAllocator()));

  const std::vector<uint8> png_data =
      image::ConvertToExternalImageData(image, image::kPng, false);
  return base::MimeBase64EncodeString(
      std::string(png_data.begin(), png_data.end()));
}

//-----------------------------------------------------------------------------
//
// ResourceHandlerTest chassis.
//
//-----------------------------------------------------------------------------
class ResourceHandlerTest : public RemoteServerTest {
 protected:
  ResourceHandlerTest() : renderer_thread_quit_flag_(false) {}

  void SetUp() override {
    RemoteServerTest::SetUp();
    server_->SetHeaderHtml("");
    server_->SetFooterHtml("");
  }

  void TearDown() override {
    ASSERT_EQ(std::thread::id(), renderer_thread_.get_id());
    RemoteServerTest::TearDown();
  }

  // The thread which processes the Renderer's info requests must be the same
  // thread on which the scene was drawn.  Since we use blocking info requests
  // on the main test thread, we cannot use the main test thread to also operate
  // the renderer (as that would deadlock).  Hence, we draw the scene and
  // service info requests on |renderer_thread_|.
  // |scene| is the Node with the scene contents to draw.  |fbo_texture| is a
  // texture to bind to the FBO when rendering.
  void StartDrawScene(const NodePtr& scene, const TexturePtr& fbo_texture) {
    ASSERT_EQ(std::thread::id(), renderer_thread_.get_id());
    renderer_scene_ = scene;
    renderer_fbo_texture_ = fbo_texture;
    renderer_thread_quit_flag_.store(false, std::memory_order_relaxed);
    render_thread_start_ = absl::make_unique<ion::port::Semaphore>();
    renderer_thread_ = std::thread(&ResourceHandlerTest::DrawSceneFunc, this);
    render_thread_start_->Wait();
    render_thread_start_.reset();
  }

  void StopDrawScene() {
    ASSERT_NE(std::thread::id(), renderer_thread_.get_id());
    renderer_thread_quit_flag_.store(true, std::memory_order_relaxed);
    renderer_thread_.join();
    renderer_thread_ = std::thread();
  }

  bool DrawSceneFunc() {
    GlContextPtr gl_context = FakeGlContext::Create(800, 800);
    GlContext::MakeCurrent(gl_context);
    FakeGraphicsManagerPtr mock_graphics_manager(new FakeGraphicsManager());
    RendererPtr renderer(new Renderer(mock_graphics_manager));
    HttpServer::RequestHandlerPtr rh(new ResourceHandler(renderer));
    server_->RegisterHandler(rh);

    // Notify the thread calling StartDrawScene() that |renderer| is set up.
    render_thread_start_->Post();

    // Bind an texture-backed FBO, if requested.
    FramebufferObjectPtr fbo;
    if (renderer_fbo_texture_.Get() != nullptr) {
      fbo.Reset(new FramebufferObject(2, 2));
      fbo->SetColorAttachment(
          0U, FramebufferObject::Attachment(renderer_fbo_texture_));
      fbo->SetDepthAttachment(
          FramebufferObject::Attachment(Image::kRenderbufferDepth16));
      renderer->BindFramebuffer(fbo);
    }

    // Draw the scene on this thread.
    renderer->DrawScene(renderer_scene_);
    // TestScene includes some invalid index buffer types.
    mock_graphics_manager->SetErrorCode(GL_NO_ERROR);

    // Now service info requests on this thread.
    while (!renderer_thread_quit_flag_.load(std::memory_order_relaxed)) {
      renderer->ProcessResourceInfoRequests();
    }

    // Clean up renderer state.
    renderer_fbo_texture_.Reset();
    renderer_scene_.Reset();
    server_->UnregisterHandler(rh->GetBasePath());
    GlContext::MakeCurrent(GlContextPtr());
    return true;
  }

  // State supporting the renderer thread.
  std::thread renderer_thread_;
  std::unique_ptr<ion::port::Semaphore> render_thread_start_;
  std::atomic<bool> renderer_thread_quit_flag_;

  // The Node representing the scene to render.  May be nullptr.
  NodePtr renderer_scene_;

  // A texture to attach to the FBO during render.  May be nullptr.
  TexturePtr renderer_fbo_texture_;
};

}  // anonymous namespace

TEST_F(ResourceHandlerTest, ServeResourceRoot) {
  StartDrawScene(NodePtr(), TexturePtr());

  GetUri("/ion/resources/does/not/exist");
  Verify404(__LINE__);

  GetUri("/ion/resources/index.html");
  const std::string& index = base::ZipAssetManager::GetFileData(
      "ion/resources/index.html");
  EXPECT_FALSE(base::IsInvalidReference(index));
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  GetUri("/ion/resources/");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  GetUri("/ion/resources");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  StopDrawScene();
}

// Disabled in production builds.
#if !ION_PRODUCTION
TEST_F(ResourceHandlerTest, GetResources) {
  StartDrawScene(NodePtr(), TexturePtr());

  GetUri(
      "/ion/resources/"
      "resources_by_type?types=platform,buffers,framebuffers,programs,samplers,"
      "shaders,textures,vertex_arrays");
  EXPECT_EQ(200, response_.status);
  // There should be no resources without a scene.
  std::vector<std::string> strings;
  strings.push_back(kPrefixJson);
  strings.push_back(SkipInitialNewline(kPlatformJson));
  strings.push_back(kJoinJson);
  strings.push_back(SkipInitialNewline(kNoResourcesJson));
  strings.push_back(kSuffixJson);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      base::JoinStrings(strings, ""), response_.data));

  StopDrawScene();

  // Build and draw a scene to create resources.
  TestScene scene;
  StartDrawScene(scene.GetScene(), scene.CreateTexture());

  // Invalid label.
  GetUri("/ion/resources/resources_by_type?types=not_a_label");
  EXPECT_EQ(200, response_.status);
  strings.clear();
  strings.push_back(kPrefixJson);
  strings.push_back(kSuffixJson);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      base::JoinStrings(strings, ""), response_.data));

  // Platform only.
  GetUri("/ion/resources/resources_by_type?types=platform");
  EXPECT_EQ(200, response_.status);
  strings.clear();
  strings.push_back(kPrefixJson);
  strings.push_back(SkipInitialNewline(kPlatformJson));
  strings.push_back(kSuffixJson);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      base::JoinStrings(strings, ""), response_.data));

  // Buffers and Shaders only.
  GetUri("/ion/resources/resources_by_type?types=buffers,shaders");
  EXPECT_EQ(200, response_.status);
  strings.clear();
  strings.push_back(kPrefixJson);
  strings.push_back(base::ReplaceString(
      SkipInitialNewline(kBuffersJson), "{vertex_buffer_size}",
      base::ValueToString(scene.GetBufferSize())));
  strings.push_back(kJoinJson);
  strings.push_back(SkipInitialNewline(kShadersJson));
  strings.push_back(kSuffixJson);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      base::JoinStrings(strings, ""), response_.data));

  // Textures, Framebuffers, invalid, and Samplers.
  GetUri(
      "/ion/resources/"
      "resources_by_type?types=textures,framebuffers,invalid,samplers");
  EXPECT_EQ(200, response_.status);
  strings.clear();
  strings.push_back(kPrefixJson);
  std::string textures = SkipInitialNewline(kTexturesJson);
  static const int kTextureUnits[] = {0, 2, 1, 3, 4, 5, 6};
  for (int i = 0; i < static_cast<int>(ABSL_ARRAYSIZE(kTextureUnits)); ++i) {
    textures = base::ReplaceString(
        textures, "{texture_unit" + base::ValueToString(i + 1) + "}",
        base::ValueToString(kTextureUnits[i]));
  }
  strings.push_back(textures);
  strings.push_back(kJoinJson);
  strings.push_back(SkipInitialNewline(kFramebuffersJson));
  strings.push_back(kJoinJson);
  strings.push_back(SkipInitialNewline(kSamplersJson));
  strings.push_back(kSuffixJson);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      base::JoinStrings(strings, ""), response_.data));

  // All resources.
  GetUri(
      "/ion/resources/"
      "resources_by_type?types=platform,buffers,"
      "framebuffers,programs,samplers,shaders,textures,"
      "vertex_arrays");
  EXPECT_EQ(200, response_.status);
  strings.clear();
  strings.push_back(kPrefixJson);
  strings.push_back(SkipInitialNewline(kPlatformJson));
  strings.push_back(kJoinJson);
  strings.push_back(base::ReplaceString(
      SkipInitialNewline(kBuffersJson), "{vertex_buffer_size}",
      base::ValueToString(scene.GetBufferSize())));
  strings.push_back(kJoinJson);
  strings.push_back(SkipInitialNewline(kFramebuffersJson));
  strings.push_back(kJoinJson);
  strings.push_back(SkipInitialNewline(kProgramsJson));
  strings.push_back(kJoinJson);
  strings.push_back(SkipInitialNewline(kSamplersJson));
  strings.push_back(kJoinJson);
  strings.push_back(SkipInitialNewline(kShadersJson));
  strings.push_back(kJoinJson);
  strings.push_back(textures);
  strings.push_back(kJoinJson);
  strings.push_back(base::ReplaceString(
      base::ReplaceString(SkipInitialNewline(kVertexArraysJson),
                          "{vertex_buffer_stride}",
                          base::ValueToString(scene.GetBufferStride())),
      "{pointer_or_offset}",
      ToHexString(TestScene::GetSecondBoeAttributeOffset())));
  strings.push_back(kSuffixJson);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      base::JoinStrings(strings, ""), response_.data));

  StopDrawScene();
}
#endif  // ION_PRODUCTION

TEST_F(ResourceHandlerTest, GetBufferData) {
  GetUri("/ion/resources/buffer_data");
  Verify404(__LINE__);
}

TEST_F(ResourceHandlerTest, GetTextureData) {
  GetUri("/ion/resources/texture_data");
  Verify404(__LINE__);

  GetUri("/ion/resources/texture_data?id=-1");
  Verify404(__LINE__);

  GetUri("/ion/resources/texture_data?id=2345345");
  Verify404(__LINE__);

  TestScene scene;

  StartDrawScene(scene.GetScene(), TexturePtr());
  GetUri("/ion/resources/texture_data?id=2");
  EXPECT_EQ(GetTestImagePng(), response_.data);
  StopDrawScene();

  StartDrawScene(scene.GetScene(), TexturePtr());
  GetUri("/ion/resources/texture_data?id=1");
  EXPECT_EQ(GetTestCubeMapImagePng(), response_.data);
  StopDrawScene();
}

}  // namespace remote
}  // namespace ion

#endif
