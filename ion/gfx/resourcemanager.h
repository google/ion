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

#ifndef ION_GFX_RESOURCEMANAGER_H_
#define ION_GFX_RESOURCEMANAGER_H_

#include <functional>
#include <mutex>  // NOLINT(build/c++11)
#include <string>
#include <vector>

#include "ion/base/allocatable.h"
#include "ion/base/lockguards.h"
#include "ion/base/referent.h"
#include "ion/gfx/image.h"
#include "ion/gfx/openglobjects.h"
#include "ion/math/range.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {

// Holder types.
class AttributeArray;
class BufferObject;
class FramebufferObject;
class GraphicsManager;
class Sampler;
class Shader;
class ShaderProgram;
class TextureBase;
class TransformFeedback;
using AttributeArrayPtr = base::SharedPtr<AttributeArray>;
using BufferObjectPtr = base::SharedPtr<BufferObject>;
using FramebufferObjectPtr = base::SharedPtr<FramebufferObject>;
using GraphicsManagerPtr = base::SharedPtr<GraphicsManager>;
using SamplerPtr = base::SharedPtr<Sampler>;
using ShaderPtr = base::SharedPtr<Shader>;
using ShaderProgramPtr = base::SharedPtr<ShaderProgram>;
using TextureBasePtr = base::SharedPtr<TextureBase>;
using TransformFeedbackPtr = base::SharedPtr<TransformFeedback>;

// A ResourceManager is an interface for getting information about a Renderer's
// internal resources. Some of the work is performed by this class, and some
// must be done by a subclass before it calls functions here. Subclasses should
// call FillInfoFromOpenGL() to finish the info filling and call the callback
// passed to GetInfo().
//
// ResourceManager is somewhat of a testing and debugging class, as it obtains
// information about internal OpenGL state. It is useful for verifying that
// Renderer has created expected state objects, and for seeing if an application
// is wasting resources or if resources are not being destroyed as expected. The
// returned OpenGL object ids may also be passed to other OpenGL state debugging
// libraries.
//
// Note that the resource information is returned for the current Renderer. If
// more than one Renderer exists, each may potentially return different
// information depending on which OpenGL context they are created and used in.
class ION_API ResourceManager : public base::Allocatable {
 public:
  //---------------------------------------------------------------------------
  //
  // Resource info types.
  //
  //---------------------------------------------------------------------------
  //
  // Base struct for resource information types. Each type of resource has an
  // associated OpenGL object id. Derived structs contain additional fields
  // for resources beyond those defined by the ::*Info structs (defined in
  // gfx/openglobjects.h).
  struct ResourceInfo {
    ResourceInfo() : id(0) {}
    // OpenGL object id.
    GLuint id;
    // The label of the ResourceHolder that owns the Resource.
    std::string label;
  };

  typedef gfx::ProgramInfo<ResourceInfo> ProgramInfo;
  typedef gfx::RenderbufferInfo<ResourceInfo> RenderbufferInfo;
  typedef gfx::SamplerInfo<ResourceInfo> SamplerInfo;
  typedef gfx::ShaderInfo<ResourceInfo> ShaderInfo;

  // The below info types contain additional informative fields about resources.
  struct ArrayResourceInfo : ResourceInfo {
    // The total number of vertices calculated the last time the array was
    // rendered.
    size_t vertex_count;
  };
  struct BufferTargetInfo : ResourceInfo {
    // The buffer's target, either GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER.
    GLuint target;
  };
  struct FramebufferResourceInfo : ResourceInfo {
    // The renderbuffers attached to the framebuffer, if any.
    std::vector<RenderbufferInfo> color_renderbuffers;
    RenderbufferInfo depth_renderbuffer;
    RenderbufferInfo stencil_renderbuffer;
  };
  struct TextureResourceInfo : ResourceInfo {
    // The texture unit the texture is bound to.
    GLenum unit;
    // The sampler that is currently bound to the same unit as the texture.
    GLuint sampler;
    // The dimensions of the texture.
    GLuint width;
    GLuint height;
    // The format of the texture.
    Image::Format format;
  };

  // The actual info types.
  typedef gfx::ArrayInfo<ArrayResourceInfo> ArrayInfo;
  typedef gfx::BufferInfo<BufferTargetInfo> BufferInfo;
  typedef gfx::FramebufferInfo<FramebufferResourceInfo> FramebufferInfo;
  typedef gfx::TextureInfo<TextureResourceInfo> TextureInfo;
  typedef gfx::TransformFeedbackInfo<ResourceInfo> TransformFeedbackInfo;

  // Struct for getting information about the local OpenGL platform.
  struct PlatformInfo {
    PlatformInfo() : major_version(0), minor_version(0), glsl_version(0) {}
    // Versions.
    GLuint major_version;
    GLuint minor_version;
    GLuint glsl_version;

#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init) Type sname;
#include "ion/gfx/glconstants.inc"

    // Strings.
    std::string extensions;
    std::string renderer;
    std::string vendor;
    std::string version_string;
  };

  // Struct containing information about a texture and its image(s). There will
  // be exactly one Image pointer for a Texture and exactly six Image pointers
  // for a CubeMapTexture. Any of the pointers may be NULL.
  struct TextureImageInfo {
    TextureImageInfo()
        : images(base::AllocationManager::GetDefaultAllocatorForLifetime(
              base::kMediumTerm)) {}
    TextureBasePtr texture;
    base::AllocVector<ImagePtr> images;
  };

  // Callbacks called when requested resource information is available. If info
  // about a specific resource was requested then the vector will have size 1.
  template <typename T> struct InfoCallback {
    typedef std::function<void(const std::vector<T>& infos)> Type;
  };

  //---------------------------------------------------------------------------
  //
  // Functions.
  //
  //---------------------------------------------------------------------------
  //
  // Returns the GraphicsManager used for the instance.
  const GraphicsManagerPtr& GetGraphicsManager() const {
    return graphics_manager_;
  }

  // Requests information about a particular resource if the ReferentPtr is
  // non-NULL. The passed callback function will be called with the info about
  // the resource, and will execute on the same thread as the Renderer that owns
  // this ResourceManager. It will be called at the next call to
  // Renderer::DrawScene or Renderer::ProcessResourceInfoRequests.
  template <typename HolderType, typename InfoType>
  void RequestResourceInfo(
      const base::SharedPtr<HolderType>& holder,
      const typename InfoCallback<InfoType>::Type& callback) {
    if (holder.Get()) {
      std::lock_guard<std::mutex> lock_guard(this->request_mutex_);
      GetResourceRequestVector<HolderType, InfoType>()->push_back(
          ResourceRequest<HolderType, InfoType>(holder, callback));
    }
  }

  // Requests information about all resources of the passed type. See the
  // comment for RequestInfoForResource() for details about the callback.
  template <typename HolderType, typename InfoType>
  void RequestAllResourceInfos(
      const typename InfoCallback<InfoType>::Type& callback) {
    std::lock_guard<std::mutex> lock_guard(this->request_mutex_);
    GetResourceRequestVector<HolderType, InfoType>()->push_back(
        ResourceRequest<HolderType, InfoType>(base::SharedPtr<HolderType>(),
                                              callback));
  }

  // Requests information about the local OpenGL platform. See the comment for
  // RequestInfoForResource() for details about the callback.
  void RequestPlatformInfo(const InfoCallback<PlatformInfo>::Type& callback);

  // Executes the callback passing a TextureImageInfo that contains a pointer
  // to the texture that has the passed OpenGL ID. The callback will receive a
  // NULL TextureBasePtr if the ID is not valid. If it is valid, the images
  // vector will contain a single Image pointer for a regular Texture and
  // exactly 6 Image pointers for a CubeMapTexture. Any of the Image pointers
  // may be NULL. See above comments for when the callback will be executed.
  void RequestTextureImage(
      GLuint id, const InfoCallback<TextureImageInfo>::Type& callback);

 protected:
  // Wrapper struct for data requests.
  template <typename InfoType>
  struct DataRequest {
    DataRequest(
        GLuint id_in,
        const typename InfoCallback<InfoType>::Type& callback_in)
        : id(id_in), callback(callback_in) {}
    GLuint id;
    typename InfoCallback<InfoType>::Type callback;
  };

  // Wrapper struct for resource info requests.
  template <typename HolderType, typename InfoType>
  struct ResourceRequest {
    ResourceRequest(const base::SharedPtr<HolderType>& holder_in,
                    const typename InfoCallback<InfoType>::Type& callback_in)
        : holder(holder_in), callback(callback_in) {}
    base::SharedPtr<HolderType> holder;
    typename InfoCallback<InfoType>::Type callback;
  };

  // A valid GraphicsManagerPtr must be passed to the constructor. The
  // constructor and destructor are protected since this is an abstract base
  // class.
  explicit ResourceManager(const GraphicsManagerPtr& gm);
  ~ResourceManager() override;

  // Returns a pointer to the vector of requests for the templated holder and
  // info types.
  template <typename HolderType, typename InfoType>
  std::vector<ResourceRequest<HolderType, InfoType> >*
      GetResourceRequestVector();

  // Returns a pointer to the vector of data requests for the templated info
  // type.
  template <typename InfoType>
  std::vector<DataRequest<InfoType> >* GetDataRequestVector();

  // Performs OpenGL calls to fill in info details, specialized for each
  // type derived from ResourceInfo. Should only be called on the same thread
  // that the OpenGL context was created from. This function assumes that the
  // resource being queried is currently bound in OpenGL.
  template <typename InfoType>
  void FillInfoFromOpenGL(InfoType* info);

  // For locking access to request vectors.
  std::mutex request_mutex_;

 private:
  GraphicsManagerPtr graphics_manager_;

  // Queues for resource info requests.
  std::vector<ResourceRequest<AttributeArray, ArrayInfo> > array_requests_;
  std::vector<ResourceRequest<BufferObject, BufferInfo> > buffer_requests_;
  std::vector<ResourceRequest<FramebufferObject, FramebufferInfo> >
      framebuffer_requests_;
  std::vector<DataRequest<PlatformInfo> > platform_requests_;
  std::vector<ResourceRequest<ShaderProgram, ProgramInfo> >
      program_requests_;
  std::vector<ResourceRequest<Sampler, SamplerInfo> > sampler_requests_;
  std::vector<ResourceRequest<Shader, ShaderInfo> > shader_requests_;
  std::vector<DataRequest<TextureImageInfo> > texture_image_requests_;
  std::vector<ResourceRequest<TextureBase, TextureInfo> > texture_requests_;
  std::vector<ResourceRequest<TransformFeedback, TransformFeedbackInfo> >
      transform_feedback_requests_;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_RESOURCEMANAGER_H_
