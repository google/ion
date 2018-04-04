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

#if !ION_PRODUCTION

#include "ion/remote/resourcehandler.h"

#include <cctype>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "ion/base/allocator.h"
#include "ion/base/invalid.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/gfx/image.h"
#include "ion/gfx/resourcemanager.h"
#include "ion/gfx/tracinghelper.h"
#include "ion/gfxutils/resourcecallback.h"
#include "ion/image/conversionutils.h"
#include "ion/image/renderutils.h"

ION_REGISTER_ASSETS(IonRemoteResourcesRoot);

namespace ion {
namespace remote {

namespace {

using gfx::AttributeArray;
using gfx::BufferObject;
using gfx::CubeMapTexture;
using gfx::CubeMapTexturePtr;
using gfx::FramebufferObject;
using gfx::Image;
using gfx::ImagePtr;
using gfx::Renderer;
using gfx::RendererPtr;
using gfx::ResourceManager;
using gfx::Sampler;
using gfx::Shader;
using gfx::ShaderProgram;
using gfx::TextureBase;
using gfx::Texture;
using gfx::TexturePtr;
using gfxutils::ArrayCallback;
using gfxutils::BufferCallback;
using gfxutils::FramebufferCallback;
using gfxutils::PlatformCallback;
using gfxutils::ProgramCallback;
using gfxutils::SamplerCallback;
using gfxutils::ShaderCallback;
using gfxutils::TextureCallback;
using gfxutils::TextureImageCallback;
using std::placeholders::_1;

typedef ResourceManager::ArrayInfo ArrayInfo;
typedef ResourceManager::BufferInfo BufferInfo;
typedef ResourceManager::FramebufferInfo FramebufferInfo;
typedef ResourceManager::RenderbufferInfo RenderbufferInfo;
typedef ResourceManager::PlatformInfo PlatformInfo;
typedef ResourceManager::ProgramInfo ProgramInfo;
typedef ResourceManager::SamplerInfo SamplerInfo;
typedef ResourceManager::ShaderInfo ShaderInfo;
typedef ResourceManager::TextureImageInfo TextureImageInfo;
typedef ResourceManager::TextureInfo TextureInfo;

//-----------------------------------------------------------------------------
//
// Helper class for consistent indentation.
//
//-----------------------------------------------------------------------------

class Indent {
 public:
  explicit Indent(size_t spaces) : indent_(spaces, ' '), spaces_(spaces) {}

  // Outputs the indentation held by this indent to the stream.
  friend std::ostream& operator<<(std::ostream& out, const Indent& indent) {
    out << indent.indent_;
    return out;
  }

  // The + operator increases the indentation.
  friend const Indent operator+(const Indent& indent, size_t spaces) {
    return Indent(indent.spaces_ + spaces);
  }

 private:
  const std::string indent_;
  const size_t spaces_;
};

// Escapes ", \, and \n.
static const std::string EscapeJson(const std::string& str) {
  std::string ret = base::ReplaceString(str, "\\", "\\\\");
  ret = base::ReplaceString(ret, "\"", "\\\"");
  ret = base::EscapeNewlines(ret);
  return ret;
}

//-----------------------------------------------------------------------------
//
// Helper class derived from TextureImageCallback that first renders textures
// into images so that the images show up correctly regardless of whether the
// textures' images had data wiped.
//
//-----------------------------------------------------------------------------

class RenderTextureCallback : public TextureImageCallback {
 public:
  using RefPtr = base::SharedPtr<RenderTextureCallback>;

  explicit RenderTextureCallback(const RendererPtr& renderer)
      : TextureImageCallback(), renderer_(renderer) {}

  // Renders texture images and then calls the version in the base class.
  void Callback(const std::vector<TextureImageInfo>& data);

 private:
  // The constructor is private because this class is derived from Referent.
  ~RenderTextureCallback() override {}

  // Each of these uses the renderer to render images from a Texture or
  // CubeMapTexture in the passed TextureImageInfo, replacing the images in it.
  void RenderTextureImage(const RendererPtr& renderer, TextureImageInfo* info);
  void RenderCubeMapTextureImages(const RendererPtr& renderer,
                                  TextureImageInfo* info);

  // Renderer used to render images.
  const RendererPtr& renderer_;
};

void RenderTextureCallback::Callback(
    const std::vector<TextureImageInfo>& data) {
  // Keep this instance from being deleted when Callback() finishes.
  RefPtr holder(this);

  // Make sure the Renderer doesn't try to process this (unfinished)
  // request before rendering the image.
  const bool flag_was_set =
      renderer_->GetFlags().test(Renderer::kProcessInfoRequests);
  renderer_->ClearFlag(Renderer::kProcessInfoRequests);

  // If any of the returned images is missing data, render it into an image.
  std::vector<TextureImageInfo> new_data = data;
  const size_t data_count = new_data.size();
  for (size_t i = 0; i < data_count; ++i) {
    TextureImageInfo* info = &new_data[i];
    if (info->texture.Get()) {
      if (info->texture->GetTextureType() == TextureBase::kTexture)
        RenderTextureImage(renderer_, info);
      else
        RenderCubeMapTextureImages(renderer_, info);
    }
  }

  if (flag_was_set)
    renderer_->SetFlag(Renderer::kProcessInfoRequests);

  // Let the base class do its work.
  TextureImageCallback::Callback(new_data);
}

void RenderTextureCallback::RenderTextureImage(
    const RendererPtr& renderer, TextureImageInfo* info) {
  DCHECK(info);
  DCHECK_EQ(info->texture->GetTextureType(), TextureBase::kTexture);
  DCHECK_EQ(info->images.size(), 1U);

  const ImagePtr& input_image = info->images[0];
  if (input_image.Get()) {
    TexturePtr tex(static_cast<Texture*>(info->texture.Get()));
    const base::AllocatorPtr& sta =
        tex->GetAllocator()->GetAllocatorForLifetime(base::kShortTerm);
    ImagePtr output_image = image::RenderTextureImage(
        tex, input_image->GetWidth(), input_image->GetHeight(), renderer, sta);
    if (output_image.Get())
      info->images[0] = output_image;
  }
}

void RenderTextureCallback::RenderCubeMapTextureImages(
    const RendererPtr& renderer, TextureImageInfo* info) {
  DCHECK(info);
  DCHECK_EQ(info->texture->GetTextureType(), TextureBase::kCubeMapTexture);
  DCHECK_EQ(info->images.size(), 6U);

  CubeMapTexturePtr tex(static_cast<CubeMapTexture*>(info->texture.Get()));
  const base::AllocatorPtr& sta =
      tex->GetAllocator()->GetAllocatorForLifetime(base::kShortTerm);
  for (int i = 0; i < 6; ++i) {
    const ImagePtr& input_image = info->images[i];
    const CubeMapTexture::CubeFace face =
        static_cast<CubeMapTexture::CubeFace>(i);
    ImagePtr output_image =
        image::RenderCubeMapTextureFaceImage(
            tex, face, input_image->GetWidth(), input_image->GetHeight(),
            renderer, sta);
    if (output_image.Get())
      info->images[i] = output_image;
  }
}

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

template <typename T,
          typename std::enable_if<std::is_fundamental<T>::value, int>::type = 0>
void ConvertValueToJson(std::ostream& out, T v) {
  out << v;
}
void ConvertValueToJson(std::ostream& out, const math::Range1f& range) {
  out << '"' << range.GetMinPoint()[0] << " - " << range.GetMaxPoint()[0]
      << '"';
}
void ConvertValueToJson(std::ostream& out, const math::Point2i& point) {
  out << '"' << point[0] << " x " << point[1] << '"';
}
void ConvertValueToJson(std::ostream& out, const math::Vector3i& vec) {
  out << '"' << vec[0] << " x " << vec[1] << " x " << vec[2] << '"';
}

// Returns a JSON string representation of a FramebufferInfo::Attachment.
static const std::string ConvertAttachmentToJson(
    const Indent& indent,
    const FramebufferInfo::Attachment& info,
    const RenderbufferInfo& rb_info) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  const Indent indent2 = indent + 2;

  if (info.type == GL_NONE) {
    str << indent << "\"type\": \"GL_NONE\"\n";
  } else {
    str << indent << "\"type\": \""
        << (info.type ? helper.ToString("GLenum", info.type) : "GL_NONE")
        << "\",\n";
    if (info.type == GL_TEXTURE) {
      str << indent << "\"texture_glid\": " << info.value << ",\n";
      str << indent << "\"mipmap_level\": " << info.level << ",\n";
      str << indent << "\"cube_face\": \""
          << (info.cube_face ? helper.ToString("GLenum", info.cube_face)
                             : "GL_NONE") << "\",\n";
      str << indent << "\"layer\": " << info.level << ",\n";
      str << indent << "\"num_views\": " << info.num_views << ",\n";
      str << indent << "\"texture_samples\": " << info.texture_samples << "\n";
    } else {
      DCHECK_EQ(info.type, GL_RENDERBUFFER);
      str << indent << "\"renderbuffer\": {\n";
      str << indent2 << "\"object_id\": " << rb_info.id << ",\n";
      str << indent2 << "\"label\": " << "\"" << rb_info.label << "\",\n";
      str << indent2 << "\"width\": " << rb_info.width << ",\n";
      str << indent2 << "\"height\": " << rb_info.height << ",\n";
      str << indent2 << "\"internal_format\": \""
          << helper.ToString("GLenum", rb_info.internal_format) << "\",\n";
      str << indent2 << "\"red_size\": " << rb_info.red_size << ",\n";
      str << indent2 << "\"green_size\": " << rb_info.green_size << ",\n";
      str << indent2 << "\"blue_size\": " << rb_info.blue_size << ",\n";
      str << indent2 << "\"alpha_size\": " << rb_info.alpha_size << ",\n";
      str << indent2 << "\"depth_size\": " << rb_info.depth_size << ",\n";
      str << indent2 << "\"stencil_size\": " << rb_info.stencil_size << "\n";
      str << indent << "}\n";
    }
  }
  return str.str();
}

const std::string ConvertEnumVectorToJson(const Indent& indent,
                                          const std::vector<GLenum>& vec) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  const size_t count = vec.size();
  for (size_t i = 0; i < count; ++i) {
    if (i)
      str << ",\n";
    str << indent << "\"" << helper.ToString("GLenum", vec[i]) << "\"";
  }
  str << "\n";
  return str.str();
}

static const std::string ConvertExtensionStringToJson(const Indent& indent,
    const std::string& extension_string) {
  std::ostringstream str;
  std::istringstream extstream(extension_string);
  std::vector<std::string> extensions;
  std::string extension;
  while (extstream >> extension) {
    extensions.push_back(extension);
  }
  // Sort first by extension name and then by vendor prefix.
  std::sort(extensions.begin(), extensions.end(),
      [](const std::string& a, const std::string& b) {
    size_t ai = a.find('_', 3) + 1, bi = b.find('_', 3) + 1;
    if (a.size() - ai == b.size() - bi &&
        std::equal(a.begin() + ai, a.end(), b.begin() + bi)) {
      // If the name of the extension is the same, compare vendor prefix only.
      return std::lexicographical_compare(a.begin(), a.begin() + ai,
                                          b.begin(), b.begin() + bi);
    } else {
      // If extension names differ, compare them and ignore the vendor prefix.
      return std::lexicographical_compare(a.begin() + ai, a.end(),
                                          b.begin() + bi, b.end(),
                                          [](char ac, char bc) {
        return std::tolower(ac) < std::tolower(bc);
      });
    }
  });
  const size_t count = extensions.size();
  for (size_t i = 0; i < count; ++i) {
    if (i)
      str << ",\n";
    str << indent << "\"" << extensions[i] << "\"";
  }
  str << "\n";
  return str.str();
}

// Returns a JSON string representation of a ProgramInfo shader input.
template <typename T>
static const std::string ConvertShaderInputToJson(const Indent& indent,
                                                  const T& input) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  str << indent << "\"name\": \"" << input.name << "\",\n";
  str << indent << "\"index\": " << input.index << ",\n";
  str << indent << "\"size\": " << input.size << ",\n";
  str << indent << "\"type\": \"" << helper.ToString("GLenum", input.type)
      << "\"\n";
  return str.str();
}

// Returns a JSON string representation of a ProgramInfo::Attribute.
static const std::string ConvertProgramAttributesToJson(
    const Indent& indent, const std::vector<ProgramInfo::Attribute>& attrs) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  const size_t count = attrs.size();
  for (size_t i = 0; i < count; ++i) {
    str << indent << "{\n";
    str << ConvertShaderInputToJson(indent + 2, attrs[i]);
    str << indent << "}" << (i < count - 1U ? "," : "") << "\n";
  }
  return str.str();
}

// Outputs the passed uniform to a stream.
template <typename T>
static void StreamProgramUniform(const ProgramInfo::Uniform& uniform,
                                 std::ostream& str) {  // NOLINT
  str << "\"";
  if (uniform.size > 1) {
    str << "[";
    for (GLint i = 0; i < uniform.size; i++) {
      if (i)
        str << ", ";
      str << uniform.value.GetValueAt<T>(i);
    }
    str << "]";
  } else {
    str << uniform.value.Get<T>();
  }
  str << "\"";
}

// Outputs the passed uniform vector to a stream.
template <typename T>
static void StreamProgramUniformVector(const ProgramInfo::Uniform& uniform,
                                       std::ostream& str) {  // NOLINT
  str << "\"";
  if (uniform.size > 1) {
    str << "[";
    for (GLint i = 0; i < uniform.size; i++) {
      if (i)
        str << ", ";
      uniform.value.GetValueAt<T>(i).Print(str, 'V');
    }
    str << "]";
  } else {
    uniform.value.Get<T>().Print(str, 'V');
  }
  str << "\"";
}

// Returns a JSON string representation of a ProgramInfo::Uniform.
static const std::string ConvertProgramUniformsToJson(
    const Indent& indent, const std::vector<ProgramInfo::Uniform>& uniforms) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  const Indent indent2 = indent + 2;

  const size_t count = uniforms.size();
  for (size_t i = 0; i < count; ++i) {
    const ProgramInfo::Uniform& u = uniforms[i];
    str << indent << "{\n";
    str << indent2 << "\"value\": ";
    switch (u.type) {
      case GL_FLOAT:
        StreamProgramUniform<float>(u, str);
        break;
      case GL_FLOAT_VEC2:
        StreamProgramUniformVector<math::VectorBase2f>(u, str);
        break;
      case GL_FLOAT_VEC3:
        StreamProgramUniformVector<math::VectorBase3f>(u, str);
        break;
      case GL_FLOAT_VEC4:
        StreamProgramUniformVector<math::VectorBase4f>(u, str);
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
      case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:
        StreamProgramUniform<int>(u, str);
        break;
      case GL_INT_VEC2:
        StreamProgramUniformVector<math::VectorBase2i>(u, str);
        break;
      case GL_INT_VEC3:
        StreamProgramUniformVector<math::VectorBase3i>(u, str);
        break;
      case GL_INT_VEC4:
        StreamProgramUniformVector<math::VectorBase4i>(u, str);
        break;
      case GL_UNSIGNED_INT:
        StreamProgramUniform<uint32>(u, str);
        break;
      case GL_UNSIGNED_INT_VEC2:
        StreamProgramUniformVector<math::VectorBase2ui>(u, str);
        break;
      case GL_UNSIGNED_INT_VEC3:
        StreamProgramUniformVector<math::VectorBase3ui>(u, str);
        break;
      case GL_UNSIGNED_INT_VEC4:
        StreamProgramUniformVector<math::VectorBase4ui>(u, str);
        break;
      case GL_FLOAT_MAT2:
        StreamProgramUniform<math::Matrix2f>(u, str);
        break;
      case GL_FLOAT_MAT3:
        StreamProgramUniform<math::Matrix3f>(u, str);
        break;
      case GL_FLOAT_MAT4:
        StreamProgramUniform<math::Matrix4f>(u, str);
        break;
#if !defined(ION_COVERAGE)  // COV_NF_START
      default:
        break;
#endif  // COV_NF_END
    }
    str << ",\n";
    str << ConvertShaderInputToJson(indent + 2, u);
    str << indent << "}" << (i < count - 1U ? "," : "") << "\n";
  }
  return str.str();
}

// The below functions convert various structs into JSON parseable strings
// suitable for reconstruction by a web browser.
//
// Default unspecialized version which should never be called.
template <typename InfoType>
static const std::string ConvertInfoToJson(const Indent& indent,
                                           const InfoType& info) {
  LOG(FATAL) << "Unspecialized ConvertInfoToJson() called.";
  return std::string();
}

template <>
const std::string ConvertInfoToJson(const Indent& indent,
                                    const PlatformInfo& info) {
  std::ostringstream str;
  const Indent indent2 = indent + 2;
  const Indent indent4 = indent + 4;

  str << indent << "\"renderer\": \"" << info.renderer << "\",\n";
  str << indent << "\"vendor\": \"" << info.vendor << "\",\n";
  str << indent << "\"version_string\": \"" << info.version_string << "\",\n";
  str << indent << "\"gl_version\": " << info.major_version << "."
      << info.minor_version << ",\n";
  str << indent << "\"glsl_version\": " << info.glsl_version << ",\n";

#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init) \
  str << indent << "\"" #sname "\": ";                      \
  ConvertValueToJson(str, info.sname);             \
  str << ",\n";
#define ION_WRAP_GL_LIST(name, sname, gl_enum, gl_count_enum) \
  str << indent << "\"" #sname "\": [\n"                      \
      << ConvertEnumVectorToJson(indent2, info.sname)         \
      << indent << "],\n";
#include "ion/gfx/glconstants.inc"
  str << indent << "\"extensions\": [\n"
      << ConvertExtensionStringToJson(indent2, info.extensions)
      << indent << "]\n";
  return str.str();
}


template <>
const std::string ConvertInfoToJson(const Indent& indent,
                                    const ArrayInfo& info) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  const Indent indent2 = indent + 2;
  const Indent indent4 = indent + 4;

  str << indent << "\"object_id\": " << info.id << ",\n";
  str << indent << "\"label\": " << "\"" << info.label << "\",\n";
  str << indent << "\"vertex_count\": " << info.vertex_count << ",\n";
  str << indent << "\"attributes\": [\n";
  const size_t count = info.attributes.size();
  for (size_t i = 0; i < count; ++i) {
    str << indent2 << "{\n";
    str << indent4 << "\"buffer_glid\": " << info.attributes[i].buffer << ",\n";
    str << indent4 << "\"enabled\": \""
        << helper.ToString("GLboolean", info.attributes[i].enabled) << "\",\n";
    str << indent4 << "\"size\": " << info.attributes[i].size << ",\n";
    str << indent4 << "\"stride\": " << info.attributes[i].stride << ",\n";
    str << indent4 << "\"type\": \""
        << helper.ToString("GLenum", info.attributes[i].type) << "\",\n";
    str << indent4 << "\"normalized\": \""
        << helper.ToString("GLboolean", info.attributes[i].normalized)
        << "\",\n";
    str << indent4 << "\"pointer_or_offset\": \""
        << helper.ToString("GLvoid*", info.attributes[i].pointer) << "\",\n";
    str << indent4 << "\"value\": \"" << info.attributes[i].value << "\"\n";
    str << indent2 << "}" << (i < count - 1U ? "," : "") << "\n";
  }
  str << indent << "]\n";

  return str.str();
}

template <>
const std::string ConvertInfoToJson(const Indent& indent,
                                    const BufferInfo& info) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  str << indent << "\"object_id\": " << info.id << ",\n";
  str << indent << "\"label\": " << "\"" << info.label << "\",\n";
  str << indent << "\"size\": " << info.size << ",\n";
  str << indent << "\"usage\": \"" << helper.ToString("GLenum", info.usage)
      << "\",\n";
  str << indent << "\"mapped_pointer\": \""
      << helper.ToString("GLvoid*", info.mapped_data) << "\",\n";
  str << indent << "\"target\": \"" << helper.ToString("GLenum", info.target)
      << "\"\n";
  return str.str();
}

template <>
const std::string ConvertInfoToJson(const Indent& indent,
                                    const FramebufferInfo& info) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  const Indent indent2 = indent + 2;
  str << indent << "\"object_id\": " << info.id << ",\n";
  str << indent << "\"label\": " << "\"" << info.label << "\",\n";
  for (size_t i = 0; i < info.color.size(); ++i) {
    str << indent << "\"attachment_color" << i << "\": {\n";
    str << ConvertAttachmentToJson(
        indent2, info.color[i], info.color_renderbuffers[i]);
    str << indent << "},\n";
  }
  str << indent << "\"attachment_depth\": {\n";
  str << ConvertAttachmentToJson(indent2, info.depth, info.depth_renderbuffer);
  str << indent << "},\n";
  str << indent << "\"attachment_stencil\": {\n";
  str << ConvertAttachmentToJson(
             indent2, info.stencil, info.stencil_renderbuffer);
  str << indent << "},\n";
  str << indent << "\"draw_buffers\": \"";
  for (size_t i = 0; i < info.draw_buffers.size(); ++i) {
    str << helper.ToString("GLbufferenum", info.draw_buffers[i]);
    if (i != info.draw_buffers.size() - 1)
      str << ", ";
  }
  str << "\",\n";
  str << indent << "\"read_buffer\": \""
      << helper.ToString("GLbufferenum", info.read_buffer) << "\"\n";
  return str.str();
}

template <>
const std::string ConvertInfoToJson(const Indent& indent,
                                    const ProgramInfo& info) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  const Indent indent2 = indent + 2;
  str << indent << "\"object_id\": " << info.id << ",\n";
  str << indent << "\"label\": " << "\"" << info.label << "\",\n";
  str << indent << "\"vertex_shader_glid\": " << info.vertex_shader << ",\n";
  str << indent << "\"geometry_shader_glid\": " << info.geometry_shader
      << ",\n";
  str << indent << "\"fragment_shader_glid\": " << info.fragment_shader
      << ",\n";
  str << indent << "\"delete_status\": \""
      << helper.ToString("GLboolean", info.delete_status) << "\",\n";
  str << indent << "\"link_status\": \""
      << helper.ToString("GLboolean", info.link_status) << "\",\n";
  str << indent << "\"validate_status\": \""
      << helper.ToString("GLboolean", info.validate_status) << "\",\n";
  str << indent << "\"attributes\": [\n";
  str << ConvertProgramAttributesToJson(indent2, info.attributes);
  str << indent << "],\n";
  str << indent << "\"uniforms\": [\n";
  str << ConvertProgramUniformsToJson(indent2, info.uniforms);
  str << indent << "],\n";
  str << indent << "\"info_log\": \"" << EscapeJson(info.info_log)
      << "\"\n";
  return str.str();
}

template <>
const std::string ConvertInfoToJson(const Indent& indent,
                                    const SamplerInfo& info) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  str << indent << "\"object_id\": " << info.id << ",\n";
  str << indent << "\"label\": " << "\"" << info.label << "\",\n";
  str << indent << "\"compare_function\": \""
      << helper.ToString("GLtextureenum", info.compare_func) << "\",\n";
  str << indent << "\"compare_mode\": \""
      << helper.ToString("GLtextureenum", info.compare_mode) << "\",\n";
  str << indent << "\"max_anisotropy\": " << info.max_anisotropy << ",\n";
  str << indent << "\"min_lod\": " << info.min_lod << ",\n";
  str << indent << "\"max_lod\": " << info.max_lod << ",\n";
  str << indent << "\"min_filter\": \""
      << helper.ToString("GLenum", info.min_filter) << "\",\n";
  str << indent << "\"mag_filter\": \""
      << helper.ToString("GLenum", info.mag_filter) << "\",\n";
  str << indent << "\"wrap_r\": \"" << helper.ToString("GLenum", info.wrap_r)
      << "\",\n";
  str << indent << "\"wrap_s\": \"" << helper.ToString("GLenum", info.wrap_s)
      << "\",\n";
  str << indent << "\"wrap_t\": \"" << helper.ToString("GLenum", info.wrap_t)
      << "\"\n";
  return str.str();
}

template <>
const std::string ConvertInfoToJson(const Indent& indent,
                                    const ShaderInfo& info) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  str << indent << "\"object_id\": " << info.id << ",\n";
  str << indent << "\"label\": " << "\"" << info.label << "\",\n";
  str << indent << "\"type\": \"" << helper.ToString("GLenum", info.type)
      << "\",\n";
  str << indent << "\"delete_status\": \""
      << helper.ToString("GLboolean", info.delete_status) << "\",\n";
  str << indent << "\"compile_status\": \""
      << helper.ToString("GLboolean", info.compile_status) << "\",\n";
  str << indent << "\"source\": \""
      << base::MimeBase64EncodeString("<pre><code>" + info.source +
                                      "</code></pre>") << "\",\n";
  str << indent << "\"info_log\": \"" << EscapeJson(info.info_log)
      << "\"\n";
  return str.str();
}

template <>
const std::string ConvertInfoToJson(const Indent& indent,
                                    const TextureInfo& info) {
  gfx::TracingHelper helper;
  std::ostringstream str;
  str << indent << "\"object_id\": " << info.id << ",\n";
  str << indent << "\"label\": " << "\"" << info.label << "\",\n";
  str << indent << "\"width\": " << info.width << ",\n";
  str << indent << "\"height\": " << info.height<< ",\n";
  str << indent << "\"format\": \"" << Image::GetFormatString(info.format)
      << "\",\n";
  str << indent << "\"sampler_glid\": " << info.sampler << ",\n";
  str << indent << "\"base_level\": " << info.base_level << ",\n";
  str << indent << "\"max_level\": " << info.max_level << ",\n";
  str << indent << "\"compare_function\": \""
      << helper.ToString("GLtextureenum", info.compare_func) << "\",\n";
  str << indent << "\"compare_mode\": \""
      << helper.ToString("GLtextureenum", info.compare_mode) << "\",\n";
  str << indent << "\"is_protected\": \""
      << helper.ToString("GLboolean", info.is_protected) << "\",\n";
  str << indent << "\"max_anisotropy\": " << info.max_anisotropy << ",\n";
  str << indent << "\"min_lod\": " << info.min_lod << ",\n";
  str << indent << "\"max_lod\": " << info.max_lod << ",\n";
  str << indent << "\"min_filter\": \""
      << helper.ToString("GLenum", info.min_filter) << "\",\n";
  str << indent << "\"mag_filter\": \""
      << helper.ToString("GLenum", info.mag_filter) << "\",\n";
  str << indent << "\"swizzle_red\": \""
      << helper.ToString("GLtextureenum", info.swizzle_r) << "\",\n";
  str << indent << "\"swizzle_green\": \""
      << helper.ToString("GLtextureenum", info.swizzle_g) << "\",\n";
  str << indent << "\"swizzle_blue\": \""
      << helper.ToString("GLtextureenum", info.swizzle_b) << "\",\n";
  str << indent << "\"swizzle_alpha\": \""
      << helper.ToString("GLtextureenum", info.swizzle_a) << "\",\n";
  str << indent << "\"wrap_r\": \"" << helper.ToString("GLenum", info.wrap_r)
      << "\",\n";
  str << indent << "\"wrap_s\": \"" << helper.ToString("GLenum", info.wrap_s)
      << "\",\n";
  str << indent << "\"wrap_t\": \"" << helper.ToString("GLenum", info.wrap_t)
      << "\",\n";
  str << indent << "\"target\": \"" << helper.ToString("GLenum", info.target)
      << "\",\n";
  str << indent << "\"last_image_unit\": \""
      << helper.ToString("GLenum", info.unit) << "\"\n";
  return str.str();
}

// Uses the passed ResourceManager to request information about OpenGL
// resources.
template <typename ResourceType, typename InfoType>
void RequestInfo(
    ResourceManager* manager,
    const typename gfxutils::ResourceCallback<InfoType>::RefPtr& callback) {
  manager->RequestAllResourceInfos<ResourceType, InfoType>(std::bind(
      &gfxutils::ResourceCallback<InfoType>::Callback, callback.Get(), _1));
}

// Specialization for PlatformInfo, which does not require a ResourceType and
// has a different function signature in the ResourceManager.
template <>
void RequestInfo<PlatformInfo, PlatformInfo>(
    ResourceManager* manager,
    const gfxutils::ResourceCallback<PlatformInfo>::RefPtr& callback) {
  manager->RequestPlatformInfo(std::bind(
      &gfxutils::ResourceCallback<PlatformInfo>::Callback, callback.Get(), _1));
}

// Builds and returns a JSON struct for the named resource type.
template <typename ResourceType, typename InfoType>
const std::string BuildJsonStruct(const RendererPtr& renderer,
                                  const std::string& name,
                                  const Indent& indent) {
  // Get resource information out of the Renderer's ResourceManager.
  ResourceManager* manager = renderer->GetResourceManager();
  typedef gfxutils::ResourceCallback<InfoType> Callback;
  typename Callback::RefPtr callback(new Callback());
  RequestInfo<ResourceType, InfoType>(manager, callback);

  std::vector<InfoType> infos;
  callback->WaitForCompletion(&infos);
  std::ostringstream str;

  const Indent indent2 = indent + 2;
  const Indent indent4 = indent + 4;

  // Start the array.
  str << indent << "\"" << name << "\": [\n";
  const size_t count = infos.size();
  for (size_t i = 0; i < count; ++i) {
    str << indent2 << "{\n";
    str << ConvertInfoToJson<InfoType>(indent4, infos[i]);
    str << indent2 << "}" << (i < count - 1U ? "," : "") << "\n";
  }

  // End the array.
  str << indent << "]";
  return str.str();
}

// Returns a string containing a JSON object containing lists of JSON structs
// for the types queried in the "types" argument in args.
static const std::string GetResourceList(const RendererPtr& renderer,
                                         const HttpServer::QueryMap& args) {
  HttpServer::QueryMap::const_iterator it = args.find("types");

  std::ostringstream str;
  if (it != args.end()) {
    str << "{\n";
    const Indent indent(2);
    const std::vector<std::string> types = base::SplitString(it->second, ",");
    const size_t count = types.size();
    for (size_t i = 0; i < count; ++i) {
      if (types[i] == "platform") {
        str << BuildJsonStruct<PlatformInfo, PlatformInfo>(renderer, "platform",
                                                           indent);
      } else if (types[i] == "buffers") {
        str << BuildJsonStruct<BufferObject, BufferInfo>(renderer, "buffers",
                                                         indent);
      } else if (types[i] == "framebuffers") {
        str << BuildJsonStruct<FramebufferObject, FramebufferInfo>(
            renderer, "framebuffers", indent);
      } else if (types[i] == "programs") {
        str << BuildJsonStruct<ShaderProgram, ProgramInfo>(renderer, "programs",
                                                           indent);
      } else if (types[i] == "samplers") {
        str << BuildJsonStruct<Sampler, SamplerInfo>(renderer, "samplers",
                                                     indent);
      } else if (types[i] == "shaders") {
        str << BuildJsonStruct<Shader, ShaderInfo>(renderer, "shaders", indent);
      } else if (types[i] == "textures") {
        str << BuildJsonStruct<TextureBase, TextureInfo>(renderer, "textures",
                                                         indent);
      } else if (types[i] == "vertex_arrays") {
        str << BuildJsonStruct<AttributeArray, ArrayInfo>(
            renderer, "vertex_arrays", indent);
      } else {
        // Ignore invalid labels.
        continue;
      }

      // Every struct but the last requires a trailing comma.
      if (i < count - 1)
        str << ",\n";
      else
        str << "\n";
    }
    str << "}\n";
  }
  return str.str();
}

static const std::string GetBufferData(const RendererPtr& renderer,
                                       const HttpServer::QueryMap& args) {
  // 
  // data.
  return std::string();
}

// Writes a face of a cube map into the map at the passed offsets. This inverts
// the Y coordinate to counteract OpenGL rendering.
static void WriteFaceIntoCubeMap(uint32 x_offset,
                                 uint32 y_offset,
                                 const ImagePtr& face,
                                 const ImagePtr& cubemap) {
  const uint32 x_offset_bytes = x_offset * 3;
  const uint32 face_height = face->GetHeight();
  const uint32 face_row_bytes = face->GetWidth() * 3;
  const uint32 cube_row_bytes = cubemap->GetWidth() * 3;
  const uint8* in = face->GetData()->GetData<uint8>();
  uint8* out = cubemap->GetData()->GetMutableData<uint8>();
  DCHECK(in);
  DCHECK(out);
  for (uint32 row = 0; row < face_height; ++row) {
    const uint32 y = face_height - row - 1;
    memcpy(&out[(y_offset + y) * cube_row_bytes + x_offset_bytes],
           &in[y * face_row_bytes], face_row_bytes);
  }
}

// Returns a string that contains PNG data for the ID passed as a query arg.
static const std::string GetTextureData(const RendererPtr& renderer,
                                        const HttpServer::QueryMap& args) {
  std::string data;
  HttpServer::QueryMap::const_iterator id_it = args.find("id");
  if (id_it != args.end()) {
    if (GLuint id = static_cast<GLuint>(base::StringToInt32(id_it->second))) {
      // Request the info.
      ResourceManager* manager = renderer->GetResourceManager();
      RenderTextureCallback::RefPtr callback(
          new RenderTextureCallback(renderer));
      manager->RequestTextureImage(
          id, std::bind(&RenderTextureCallback::Callback, callback.Get(), _1));

      // Wait for the callback to be triggered.
      std::vector<TextureImageInfo> infos;
      callback->WaitForCompletion(&infos);

      // There should only be one info, and it contains the texture image.
      if (!infos.empty() && !infos[0].images.empty()) {
        if (infos[0].images.size() == 1U) {
          // Convert the image to png. Flip Y to counteract OpenGL rendering.
          const std::vector<uint8> png_data = image::ConvertToExternalImageData(
              infos[0].images[0], image::kPng, true);
          data = base::MimeBase64EncodeString(
              std::string(png_data.begin(), png_data.end()));
        } else {
          // Make a vertical cube map cross image.
          DCHECK_EQ(6U, infos[0].images.size());
          uint32 face_width = 0;
          uint32 face_height = 0;
          for (int i = 0; i < 6; ++i) {
            face_width = std::max(face_width, infos[0].images[0]->GetWidth());
            face_height =
                std::max(face_height, infos[0].images[0]->GetHeight());
          }

          // Allocate the data.
          ImagePtr cubemap(new Image);
          const uint32 num_bytes = face_width * 3U * face_height * 4U * 3U;
          base::DataContainerPtr cubemap_data =
              base::DataContainer::CreateOverAllocated<uint8>(
                  num_bytes, nullptr, cubemap->GetAllocator());
          cubemap->Set(
              Image::kRgb888, face_width * 3U, face_height * 4U, cubemap_data);
          memset(cubemap_data->GetMutableData<uint8>(), 0, num_bytes);

          // Copy the images into the cubemap. The output map should look like:
          //     ----
          //     |+Y|
          //  ----------
          //  |-X|+Z|+X|
          //  ----------
          //     |-Y|
          //     ----
          //     |-Z|
          //     ----
          WriteFaceIntoCubeMap(face_width, 0U, infos[0].images[4], cubemap);
          WriteFaceIntoCubeMap(0U, face_height, infos[0].images[0], cubemap);
          WriteFaceIntoCubeMap(
              face_width, face_height, infos[0].images[5], cubemap);
          WriteFaceIntoCubeMap(
              face_width * 2U, face_height, infos[0].images[3], cubemap);
          WriteFaceIntoCubeMap(
              face_width, face_height * 2U, infos[0].images[1], cubemap);
          WriteFaceIntoCubeMap(
              face_width, face_height * 3U, infos[0].images[2], cubemap);

          // Send the image back.
          const std::vector<uint8> png_data =
              image::ConvertToExternalImageData(cubemap, image::kPng, false);
          data = base::MimeBase64EncodeString(
              std::string(png_data.begin(), png_data.end()));
        }
      }
    }
  }

  return data;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// ResourceHandler functions.
//
//-----------------------------------------------------------------------------

ResourceHandler::ResourceHandler(const gfx::RendererPtr& renderer)
    : HttpServer::RequestHandler("/ion/resources"),
      renderer_(renderer) {
  IonRemoteResourcesRoot::RegisterAssetsOnce();
}

ResourceHandler::~ResourceHandler() {}

const std::string ResourceHandler::HandleRequest(
    const std::string& path_in, const HttpServer::QueryMap& args,
    std::string* content_type) {
  const std::string path = path_in.empty() ? "index.html" : path_in;

  if (path == "buffer_data") {
    return GetBufferData(renderer_, args);
  } else if (path == "resources_by_type") {
    *content_type = "application/json";
    return GetResourceList(renderer_, args);
  } else if (path == "texture_data") {
    *content_type = "image/png";
    return GetTextureData(renderer_, args);
  } else {
    const std::string& data = base::ZipAssetManager::GetFileData(
        "ion/resources/" + path);
    if (base::IsInvalidReference(data)) {
      return std::string();
    } else {
      // Ensure the content type is set if the editor HTML is requested.
      if (base::EndsWith(path, "html"))
        *content_type = "text/html";
      return data;
    }
  }
}

}  // namespace remote
}  // namespace ion

#endif
