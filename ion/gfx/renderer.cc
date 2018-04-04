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

#include <algorithm>
#include <array>
#include <bitset>
#include <limits>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <vector>

#include "base/integral_types.h"
#include "ion/base/allocationmanager.h"
#include "ion/base/allocationsizetracker.h"
#include "ion/base/enumhelper.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/readwritelock.h"
#include "ion/base/serialize.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/stlalloc/allocset.h"
#include "ion/base/stlalloc/allocunorderedset.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/cubemaptexture.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/image.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/resourcebase.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shape.h"
#include "ion/gfx/texture.h"
#include "ion/gfx/transformfeedback.h"
#include "ion/gfx/updatestatetable.h"
#include "ion/math/matrix.h"
#include "ion/math/matrixutils.h"
#include "ion/math/range.h"
#include "ion/math/rangeutils.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"
#include "ion/port/atomic.h"
#include "ion/port/macros.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"
#if defined(ION_ANALYTICS_ENABLED)
#include "ion/profile/profiling.h"
#endif

namespace ion {
namespace gfx {

using base::DataContainer;
using base::DataContainerPtr;
using math::Point3ui;
using math::Range1i;
using math::Range1ui;

#define SCOPED_RESOURCE_LABEL \
  ScopedResourceLabel label(this, rb, ION_PRETTY_FUNCTION);

namespace {

static const GLuint kInvalidGluint = static_cast<GLuint>(-1);

using StringVector = base::AllocVector<std::string>;

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Returns a ReadWriteLock for protecting access to the map of ResourceBinders.
static base::ReadWriteLock* GetResourceBinderLock() {
  ION_DECLARE_SAFE_STATIC_POINTER(base::ReadWriteLock, lock);
  return lock;
}

// Returns the stride of an attribute, which is 0 for all non-matrix types, and
// the size of a column for matrix types. Also returns the number of slots
// required to store the passed attribute type.
static void GetAttributeSlotCountAndStride(BufferObject::ComponentType type,
                                           GLuint* stride, GLuint* slots) {
  *stride = 0U;
  *slots = 1U;
  switch (type) {
    case BufferObject::kFloatMatrixColumn2:
      *stride = static_cast<GLuint>(2U * sizeof(float));
      *slots = 2U;
      break;
    case BufferObject::kFloatMatrixColumn3:
      *stride = static_cast<GLuint>(3U * sizeof(float));
      *slots = 3U;
      break;
    case BufferObject::kFloatMatrixColumn4:
      *stride = static_cast<GLuint>(4U * sizeof(float));
      *slots = 4U;
      break;
    default:
      break;
  }
}

// Returns the number of slots required for a GL attribute type.
static GLuint GetAttributeSlotCountByGlType(GLenum type) {
  GLuint slots = 1U;
  switch (type) {
    case GL_FLOAT_MAT2:
      slots = 2U;
      break;
    case GL_FLOAT_MAT3:
      slots = 3U;
      break;
    case GL_FLOAT_MAT4:
      slots = 4U;
      break;
    default:
      break;
  }
  return slots;
}

// Returns a string corresponding to a shader type (for error messages).
static const char* GetShaderTypeString(GLenum shader_type) {
  const char* type = "<UNKNOWN>";
  if (shader_type == GL_VERTEX_SHADER) {
    type = "vertex";
  } else if (shader_type == GL_GEOMETRY_SHADER) {
    type = "geometry";
  } else if (shader_type == GL_FRAGMENT_SHADER) {
    type = "fragment";
  }
  return type;
}

// Sets the label of an object if GL supports the operation.
static void SetObjectLabel(GraphicsManager* gm, GLenum type, GLuint id,
                           const std::string& label) {
#if !ION_PRODUCTION
  if (gm->IsFeatureAvailable(GraphicsManager::kDebugLabel)) {
    gm->LabelObject(type, id, static_cast<GLsizei>(label.length()),
                    label.c_str());
  }
#endif
}

static bool ValidateUniformType(const char* name,
                                const Uniform::ValueType spec_type,
                                GLenum type) {
  bool types_equal = false;
  switch (spec_type) {
    case kIntUniform:
      types_equal = type == GL_INT;
      break;
    case kFloatUniform:
      types_equal = type == GL_FLOAT;
      break;
    case kUnsignedIntUniform:
      types_equal = type == GL_UNSIGNED_INT;
      break;
    case kCubeMapTextureUniform:
      types_equal = type == GL_INT_SAMPLER_CUBE ||
                    type == GL_INT_SAMPLER_CUBE_MAP_ARRAY ||
                    type == GL_SAMPLER_CUBE ||
                    type == GL_SAMPLER_CUBE_MAP_ARRAY ||
                    type == GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW ||
                    type == GL_SAMPLER_CUBE_SHADOW ||
                    type == GL_UNSIGNED_INT_SAMPLER_CUBE ||
                    type == GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY;
      break;
    case kTextureUniform:
      types_equal =
          type == GL_INT_SAMPLER_1D || type == GL_INT_SAMPLER_1D_ARRAY ||
          type == GL_INT_SAMPLER_2D || type == GL_INT_SAMPLER_2D_ARRAY ||
          type == GL_INT_SAMPLER_3D || type == GL_SAMPLER_1D ||
          type == GL_SAMPLER_1D_ARRAY || type == GL_SAMPLER_1D_ARRAY_SHADOW ||
          type == GL_SAMPLER_1D_SHADOW || type == GL_SAMPLER_2D ||
          type == GL_SAMPLER_2D_ARRAY || type == GL_SAMPLER_2D_ARRAY_SHADOW ||
          type == GL_SAMPLER_2D_MULTISAMPLE ||
          type == GL_SAMPLER_2D_MULTISAMPLE_ARRAY ||
          type == GL_SAMPLER_2D_SHADOW || type == GL_SAMPLER_3D ||
          type == GL_SAMPLER_EXTERNAL_OES ||
          type == GL_UNSIGNED_INT_SAMPLER_1D ||
          type == GL_UNSIGNED_INT_SAMPLER_1D_ARRAY ||
          type == GL_UNSIGNED_INT_SAMPLER_2D ||
          type == GL_UNSIGNED_INT_SAMPLER_2D_ARRAY ||
          type == GL_UNSIGNED_INT_SAMPLER_3D;
      break;
    case kFloatVector2Uniform:
      types_equal = type == GL_FLOAT_VEC2;
      break;
    case kFloatVector3Uniform:
      types_equal = type == GL_FLOAT_VEC3;
      break;
    case kFloatVector4Uniform:
      types_equal = type == GL_FLOAT_VEC4;
      break;
    case kIntVector2Uniform:
      types_equal = type == GL_INT_VEC2;
      break;
    case kIntVector3Uniform:
      types_equal = type == GL_INT_VEC3;
      break;
    case kIntVector4Uniform:
      types_equal = type == GL_INT_VEC4;
      break;
    case kMatrix2x2Uniform:
      types_equal = type == GL_FLOAT_MAT2;
      break;
    case kMatrix3x3Uniform:
      types_equal = type == GL_FLOAT_MAT3;
      break;
    case kMatrix4x4Uniform:
      types_equal = type == GL_FLOAT_MAT4;
      break;
    case kUnsignedIntVector2Uniform:
      types_equal = type == GL_UNSIGNED_INT_VEC2;
      break;
    case kUnsignedIntVector3Uniform:
      types_equal = type == GL_UNSIGNED_INT_VEC3;
      break;
    case kUnsignedIntVector4Uniform:
      types_equal = type == GL_UNSIGNED_INT_VEC4;
      break;
#if !defined(ION_COVERAGE)  // COV_NF_START
    // A Uniform type is explicitly set to a valid type.
    default:
      break;
#endif  // COV_NF_END
  }

  return types_equal;
}

// Compiles an OpenGL shader, returning the shader id. Logs a message and
// returns 0 on error.
static GLuint CompileShader(const std::string& id_string, GLenum shader_type,
                            const std::string& source, std::string* info_log,
                            GraphicsManager* gm) {
  // Clear the info log. When this function returns it will either be empty,
  // indicating success, or non-empty, signaling an error.
  info_log->clear();
  GLuint id = gm->CreateShader(shader_type);

  if (id) {
    // Send the source to OpenGL and compile the shader.
    const char* source_string = source.c_str();
    gm->ShaderSource(id, 1, &source_string, nullptr);
    gm->CompileShader(id);

    // Test for problems.
    GLint ok = GL_FALSE;
    gm->GetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
      char log[2048];
      log[0] = 0;
      gm->GetShaderInfoLog(id, 2047, nullptr, log);
      *info_log = log;
      LOG(ERROR) << "***ION: Unable to compile "
                 << GetShaderTypeString(shader_type) << " shader for '"
                 << id_string << "': " << log;
      gm->DeleteShader(id);
      id = 0;
    }
  } else {
    LOG(ERROR) << "***ION: Unable to create shader object";
  }
  return id;
}

// Links an OpenGL shader program, returning the program id. Logs a message and
// returns 0 on error.
static GLuint RelinkShaderProgram(const std::string& id_string,
                                  GLuint program_id,
                                  const StringVector& captured_varyings,
                                  std::string* info_log, GraphicsManager* gm) {
  // Clear the info log. When this function returns it will either be empty,
  // indicating success, or non-empty, signaling an error.
  info_log->clear();

  // If transform feedback varyings were specified, tell GL about them now.
  const size_t nvaryings = captured_varyings.size();
  if (nvaryings > 0) {
    std::vector<const char*> raw_varyings(nvaryings);
    auto iter = raw_varyings.begin();
    for (const auto& str : captured_varyings) {
      *iter++ = str.c_str();
    }
    gm->TransformFeedbackVaryings(program_id, static_cast<GLsizei>(nvaryings),
                                  raw_varyings.data(), GL_INTERLEAVED_ATTRIBS);
  }

  // Link the program object.
  gm->LinkProgram(program_id);

  // Test for problems.
  GLint ok = GL_FALSE;
  gm->GetProgramiv(program_id, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    log[0] = 0;
    gm->GetProgramInfoLog(program_id, 2047, nullptr, log);
    *info_log = log;
    LOG(ERROR) << "***ION: Unable to link shader program for '" << id_string
               << "': " << log;
    gm->DeleteProgram(program_id);
    program_id = 0;
  }

  return program_id;
}

// Links an OpenGL shader program, returning the program id. Logs a message and
// returns 0 on error.
static GLuint LinkShaderProgram(const std::string& id_string,
                                GLuint vertex_shader_id,
                                GLuint geometry_shader_id,
                                GLuint fragment_shader_id,
                                GLuint tess_ctrl_shader_id,
                                GLuint tess_eval_shader_id,
                                const StringVector& captured_varyings,
                                std::string* info_log, GraphicsManager* gm) {
  GLuint program_id = gm->CreateProgram();
  if (program_id) {
    if (vertex_shader_id)
      gm->AttachShader(program_id, vertex_shader_id);
    if (geometry_shader_id)
      gm->AttachShader(program_id, geometry_shader_id);
    if (fragment_shader_id)
      gm->AttachShader(program_id, fragment_shader_id);
    if (tess_ctrl_shader_id)
      gm->AttachShader(program_id, tess_ctrl_shader_id);
    if (tess_eval_shader_id)
      gm->AttachShader(program_id, tess_eval_shader_id);
    program_id = RelinkShaderProgram(id_string, program_id,
                                     captured_varyings, info_log, gm);
  } else {
    LOG(ERROR) << "***ION: Unable to create shader program object";
  }

  return program_id;
}

// The following two functions return an Image from a CubeMapTexture or a
// Texture, returning a nullptr if there is no valid Image.
const ImagePtr GetCubeMapTextureImageOrMipmap(const CubeMapTexture& tex,
                                              CubeMapTexture::CubeFace face) {
  // Find a valid mipmap image.
  const size_t mipmap_count = tex.GetImageCount(face);
  for (size_t i = 0; i < mipmap_count; ++i) {
    if (tex.HasImage(face, i))
      return tex.GetImage(face, i);
  }
  return ImagePtr();
}
const ImagePtr GetTextureImageOrMipmap(const Texture& tex) {
  ImagePtr image;
  // Find a valid mipmap image.
  const size_t mipmap_count = tex.GetImageCount();
  for (size_t i = 0; i < mipmap_count; ++i) {
    if (tex.HasImage(i)) {
      image = tex.GetImage(i);
      break;
    }
  }
  return image;
}

// Wrapper class to force obtaining a VertexArrayEmulatorResource.
class AttributeArrayEmulator : public AttributeArray {
 public:
  AttributeArrayEmulator() {}
  ~AttributeArrayEmulator() override {}
};

// Returns the correct pixel format as a substitute for the provided format
// if the latter is not supported.
static Image::PixelFormat GetCompatiblePixelFormat(Image::PixelFormat pf,
                                                   GraphicsManager* gm) {
  // OpenGL 3.0 deprecates luminance textures and 3.1 removes them.
  if (gm->GetGlVersion() >= 30 &&
      gm->GetGlFlavor() == GraphicsManager::kDesktop) {
    if (pf.format == GL_LUMINANCE) {
      pf.format = GL_RED;
      pf.internal_format = GL_R8;
      return pf;
    } else if (pf.format == GL_LUMINANCE_ALPHA) {
      pf.format = GL_RG;
      pf.internal_format = GL_RG8;
      return pf;
    }
  } else if (gm->GetGlVersion() < 30) {
    // OpenGL < 3.0 and OpenGL ES < 3.0 both do allow luminance textures.
    if (pf.format == GL_RED) {
      pf.format = pf.internal_format = GL_LUMINANCE;
      return pf;
    } else if (pf.format == GL_RG) {
      pf.format = pf.internal_format = GL_LUMINANCE_ALPHA;
      return pf;
    }
  }

  // All other OpenGL versions (e.g., OpenGL ES 3.x) support both luminance and
  // red textures.
  return pf;
}

typedef void (GraphicsManager::*UniformMatrixSetter)(
    GLint, GLsizei, GLboolean, const GLfloat*);

template <int Dimension>
inline static void SendMatrixUniform(const Uniform& uniform,
                                     GraphicsManager*gm,
                                     GLint location,
                                     UniformMatrixSetter setter) {
  typedef math::Matrix<Dimension, float> Matrix;
  if (uniform.IsArrayOf<Matrix>()) {
    // We have to transpose each matrix.
    const GLint count = static_cast<GLint>(uniform.GetCount());
    const base::AllocatorPtr& allocator =
        base::AllocationManager::GetDefaultAllocatorForLifetime(
            base::kShortTerm);
    Matrix* mats = static_cast<Matrix*>(allocator->AllocateMemory(
        sizeof(Matrix) * count));
    for (GLint i = 0; i < count; ++i)
      mats[i] = math::Transpose(uniform.GetValueAt<Matrix>(i));
    (gm->*setter)(location, count, GL_FALSE, reinterpret_cast<float*>(mats));
    allocator->DeallocateMemory(mats);
  } else {
    (gm->*setter)(location, 1, GL_FALSE,
                math::Transpose(uniform.GetValue<Matrix>()).Data());
  }
}

static uint64_t GetInvalidateColorFlags() {
  uint64_t result = 0;
  for (size_t i = 0; i < kColorAttachmentSlotCount; ++i) {
    result |= (1ULL << (Renderer::kInvalidateColorAttachment + i));
  }
  return result;
}

}  // anonymous namespace


//-----------------------------------------------------------------------------
//
// Helper typedefs.
//
//-----------------------------------------------------------------------------

template <typename T>
struct Renderer::HolderToResource {};

// Specialize for Resource types.
template <>
struct Renderer::HolderToResource<AttributeArray> {
  typedef Renderer::VertexArrayResource ResourceType;
};
template <>
struct Renderer::HolderToResource<AttributeArrayEmulator> {
  typedef Renderer::VertexArrayEmulatorResource ResourceType;
};
template <>
struct Renderer::HolderToResource<BufferObject> {
  typedef Renderer::BufferResource ResourceType;
};
template <>
struct Renderer::HolderToResource<CubeMapTexture> {
  typedef Renderer::TextureResource ResourceType;
};
template <>
struct Renderer::HolderToResource<FramebufferObject> {
  typedef Renderer::FramebufferResource ResourceType;
};
template <>
struct Renderer::HolderToResource<IndexBuffer> {
  typedef Renderer::BufferResource ResourceType;
};
template <>
struct Renderer::HolderToResource<Sampler> {
  typedef Renderer::SamplerResource ResourceType;
};
template <>
struct Renderer::HolderToResource<Shader> {
  typedef Renderer::ShaderResource ResourceType;
};
template <>
struct Renderer::HolderToResource<ShaderProgram> {
  typedef Renderer::ShaderProgramResource ResourceType;
};
template <>
struct Renderer::HolderToResource<ShaderInputRegistry> {
  typedef Renderer::ShaderInputRegistryResource ResourceType;
};
template <>
struct Renderer::HolderToResource<TextureBase> {
  typedef Renderer::TextureResource ResourceType;
};
template <>
struct Renderer::HolderToResource<Texture> {
  typedef Renderer::TextureResource ResourceType;
};
template <>
struct Renderer::HolderToResource<TransformFeedback> {
  typedef Renderer::TransformFeedbackResource ResourceType;
};

//-----------------------------------------------------------------------------
//
// The Renderer::ResourceManager class manages all OpenGL resources used by the
// Renderer, and is 1:1 with its owning Renderer. It holds all existing
// resources so they can be released and cleaned up when necessary.
//
//-----------------------------------------------------------------------------

class Renderer::ResourceManager : public gfx::ResourceManager {
 public:
  // This is an abstract base class for all resources that can be managed by
  // this manager. It adds an index that makes some operations more efficient.
  class Resource : public ResourceBase, public Allocatable {
   public:
    // Returns the ResourceManager that owns this Resource.
    ResourceManager* GetResourceManager() const { return resource_manager_; }

    // Returns the amount of memory used by this resource.
    size_t GetGpuMemoryUsed() const override { return gpu_memory_used_.load(); }

   protected:
    explicit Resource(ResourceManager* rm, const ResourceHolder* holder,
                      ResourceKey key)
        : ResourceBase(holder, key),
          index_(0),
          resource_manager_(rm),
          gpu_memory_used_(0U) {}

    // Each derived class must define these to update and release its
    // resource. It must be safe to call these multiple times.
    virtual void Release(bool can_make_gl_calls) = 0;
    virtual void Update(ResourceBinder* rb) = 0;
    virtual void Unbind(ResourceBinder* rb) = 0;
    virtual ResourceType GetType() const = 0;

    // Sets the amount of memory used by this Resource. Also updates the running
    // totals in the ResourceManager and optional GPU memory tracker.
    void SetUsedGpuMemory(size_t count) {
      // Remove any old memory information from the ResourceManager and add in
      // the new usage.
      const ResourceType type = GetType();
      const size_t old_used = gpu_memory_used_.load();
      resource_manager_->IncreaseGpuMemoryUsage(type, count);
      resource_manager_->DecreaseGpuMemoryUsage(type, old_used);
      // If resource_manager_ has its own GPU tracker, adjust GPU memory usage
      // in the tracker as well.
      if (resource_manager_->GetAllocator().Get() &&
          resource_manager_->GetAllocator()->GetTracker().Get() &&
          resource_manager_->GetAllocator()
              ->GetTracker()
              ->GetGpuTracker()
              .Get()) {
        const base::AllocationSizeTrackerPtr& global_gpu_tracker =
            resource_manager_->GetAllocator()->GetTracker()->GetGpuTracker();
        UpdateAllocationSizeTracker(global_gpu_tracker, count, old_used);
      }
      // If the Allocator has its own GPU Tracker, adjust GPU memory usage in
      // the tracker as well.
      if (GetAllocator()->GetTracker().Get() &&
          GetAllocator()->GetTracker()->GetGpuTracker().Get()) {
        const base::AllocationSizeTrackerPtr& gpu_tracker =
            GetAllocator()->GetTracker()->GetGpuTracker();
        UpdateAllocationSizeTracker(gpu_tracker, count, old_used);
      }
      gpu_memory_used_ = count;
    }

    // Returns, creating if necessary, a typed resource from a holder. If a
    // resource is created, the passed GL id is used; the default id of 0 means
    // that a new one will be retrieved from OpenGL.
    template <typename HolderType>
    typename HolderToResource<HolderType>::ResourceType* GetResource(
        const HolderType* holder, ResourceBinder* binder, GLuint gl_id = 0) {
      DCHECK(resource_manager_);
      return resource_manager_->GetResource(holder, binder, gl_id);
    }

    virtual void UnbindAll() {
      // Remove this from all ResourceBinders.
      base::ReadLock read_lock(GetResourceBinderLock());
      base::ReadGuard read_guard(&read_lock);
      ResourceBinderMap& binders = GetResourceBinderMap();
      for (ResourceBinderMap::iterator it = binders.begin();
           it != binders.end();
           ++it)
        Unbind(it->second.get());
    }

   private:
    void SetIndex(size_t index) { index_ = index; }
    size_t GetIndex() const { return index_; }

    // Updates the AllocationSizeTracker. Both allocation and de-allocation are
    // tracked.
    void UpdateAllocationSizeTracker(
        const base::AllocationSizeTrackerPtr& tracker, size_t count,
        size_t old_used) {
      if (count) tracker->TrackAllocationSize(count);
      if (old_used) tracker->TrackDeallocationSize(old_used);
    }

    // This is the index within the resources vector in the manager.
    size_t index_;

    // The ResourceManager that owns this resource. It by definition has a
    // longer lifetime than this.
    ResourceManager* resource_manager_;

    // The amount of GPU memory this Resource uses.
    std::atomic<size_t> gpu_memory_used_;

    friend class ResourceBinder;
    friend class Renderer::ResourceManager;
  };

  typedef base::AllocVector<Resource*> ResourceVector;

  class ResourceAccessor;
  // Container for holding resources. It contains a ResourceVector and a mutex
  // for locking access to it.
  class ResourceContainer : public Allocatable {
   public:
    ResourceContainer() : resources_(*this) {}

   private:
    friend class ResourceAccessor;
    std::mutex mutex_;
    ResourceVector resources_;
  };

  // Container for accessing resources. There is a special copy constructor that
  // allows these to be copied safely without releasing the mutex lock on the
  // ResourceContainer.
  class ResourceAccessor {
   public:
    explicit ResourceAccessor(ResourceContainer& container)  // NOLINT
        : container_(container),
          lock_(container_.mutex_) {}

    // Returns the ResourceVector that the container in this holds.
    ResourceVector& GetResources() { return container_.resources_; }

   private:
    ResourceContainer& container_;
    std::unique_lock<std::mutex> lock_;
  };

  // The constructor is passed a GraphicsManager to use for querying resource
  // information.
  explicit ResourceManager(const GraphicsManagerPtr& gm)
      : gfx::ResourceManager(gm),
        resource_index_(AcquireOrReleaseResourceIndex(false, 0U)),
        memory_usage_(*this),
        resources_to_release_(*this),
        check_stamp_(false) {
    memory_usage_.resize(kNumResourceTypes);
    ResourceAccessor(resources_[kAttributeArray]).GetResources().reserve(128U);
    ResourceAccessor(resources_[kBufferObject]).GetResources().reserve(128U);
    ResourceAccessor(resources_[kFramebufferObject]).GetResources().reserve(
        16U);
    ResourceAccessor(resources_[kSampler]).GetResources().reserve(32U);
    ResourceAccessor(resources_[kShaderInputRegistry]).GetResources().reserve(
        16U);
    ResourceAccessor(resources_[kShaderProgram]).GetResources().reserve(16U);
    ResourceAccessor(resources_[kShader]).GetResources().reserve(16U);
    ResourceAccessor(resources_[kTexture]).GetResources().reserve(128U);
    ResourceAccessor(resources_[kTransformFeedback]).GetResources().reserve(1U);
  }

  // The destructor releases and deletes all resources.
  ~ResourceManager() override {
    DestroyOrAbandonAllResources(false);
    AcquireOrReleaseResourceIndex(true, resource_index_);
  }

  // Returns the resource index for this. Use it to get and set resources in
  // ResourceHolders.
  size_t GetResourceIndex() const { return resource_index_; }

  // Enable or disable checking the context stamp on every resource
  // accessibility check.
  void EnableResourceAccessCheck(bool enabled) { check_stamp_ = enabled; }

  bool AreResourcesAccessible() const {
    if (!gl_context_) return true;  // No resources created yet
    if (check_stamp_ && gl_context_->DoesCurrentContextMatch()) {
      return true;
    }
    const portgfx::GlContextPtr current_gl_context =
        portgfx::GlContext::GetCurrent();
    if (!current_gl_context) return false;
    const uintptr_t current_share_group = current_gl_context->GetShareGroupId();
    return current_share_group == gl_context_->GetShareGroupId();
  }

  // Returns a ResourceAccessor for the Resources of the specified type.
  ResourceAccessor AccessResources(ResourceType type) {
    ResourceAccessor accessor(resources_[type]);
    return accessor;
  }

  // Returns a key that uniquely identifies a GL resource associated
  // with an Ion object. Whenever a single Ion object must be represented
  // with multiple GL resources (e.g., when the GL resource cannot be shared
  // between threads and must be created anew for each thread), this function
  // should return a unique key for each instance of the GL resource.
  template <typename T>
  ResourceKey GetResourceKey(ResourceBinder* resource_binder,
                             const ResourceHolder* holder) {
    return reinterpret_cast<ResourceKey>(this);
  }

  // Returns all keys that may have resources assigned. This is required
  // to make info requests work correctly. The key may depend on the state
  // of holders in the scene graph (which is the case for shader programs),
  // and the scene graph is not available when processing info requests.
  template <typename T>
  std::vector<ResourceKey> GetAllResourceKeys(ResourceBinder* resource_binder) {
    std::vector<ResourceKey> keys;
    keys.push_back(GetResourceKey<T>(resource_binder, nullptr));
    return keys;
  }

  // Returns, creating if necessary, a typed resource from a holder.
  template <typename HolderType>
  typename HolderToResource<HolderType>::ResourceType* GetResource(
      const HolderType* holder, ResourceBinder* resource_binder,
      GLuint gl_id = 0);

  // Increases the count of memory usage of the passed type.
  void IncreaseGpuMemoryUsage(ResourceType type, size_t count) {
    memory_usage_[type].value += count;
  }

  // Decreases the amount of memory usage of the passed type.
  void DecreaseGpuMemoryUsage(ResourceType type, size_t count) {
    DCHECK_LE(count, memory_usage_[type].value.load());
    memory_usage_[type].value -= count;
  }

  // Returns the amount of memory usage of the passed type.
  size_t GetGpuMemoryUsage(ResourceType type) const {
    return memory_usage_[type].value.load();
  }

  // Disassociates the passed resource from any VertexArrayResources that use
  // it. This is necessary to prevent the binding of an already deleted element
  // array.
  void DisassociateElementBufferFromArrays(BufferResource* resource);

  // Adds a resource to manage, specialized by type.
  void AddResource(Resource* resource) {
    if (!gl_context_)
      gl_context_ = portgfx::GlContext::GetCurrent();
    ResourceAccessor accessor(resources_[resource->GetType()]);
    ResourceVector& resources = accessor.GetResources();
    resource->SetIndex(resources.size());
    resources.push_back(resource);
  }

  // Marks a resource to release at the next convenient time.
  void MarkForRelease(Resource* resource) {
    std::lock_guard<std::mutex> locker(release_mutex_);
    DCHECK(resource);
    resources_to_release_.push_back(resource);
  }

  // Removes a resource from the vector that holds ownership. The caller is
  // responsible for deleting the resource and reclaiming memory.
  void DestroyResource(Resource* resource) {
    DCHECK(resource);

    // Remove the resource from the main vector by replacing it with the last
    // resource in the vector.
    ResourceAccessor accessor(resources_[resource->GetType()]);
    ResourceVector& resources = accessor.GetResources();
    const size_t num_resources = resources.size();
    if (num_resources > 1U) {
      const size_t index = resource->GetIndex();
      // If DestroyResource is called more than once, the resource will have
      // already been removed.
      if (resources[index] == resource) {
        Resource* moved_resource = resources[num_resources - 1U];
        resources[index] = moved_resource;
        moved_resource->SetIndex(index);
        resources.resize(num_resources - 1U);
      }
    } else if (num_resources == 1U) {
      // If this is the only resource just clear the vector.
      if (resources[0] == resource)
        resources.clear();
    }
  }

  // Marks the Resources contained in the passed holder for release. A current
  // binder is necessary to retrieve the resource since its type is unknown.
  template <typename HolderType>
  void ReleaseResources(const HolderType* holder, ResourceBinder* binder) {
    typedef typename HolderToResource<HolderType>::ResourceType ResourceType;
    if (holder) {
      const std::vector<ResourceKey> keys =
          GetAllResourceKeys<ResourceType>(binder);
      for (ResourceKey key : keys) {
        if (ResourceBase* resource =
            holder->GetResource(resource_index_, key)) {
          resource->OnDestroyed();
        }
      }
      ProcessReleases(binder);
    }
  }

  // Marks all Resources of the passed type for release.
  void ReleaseTypedResources(ResourceType type) {
    ResourceAccessor accessor(resources_[type]);
    ResourceVector& resources = accessor.GetResources();
    for (auto resource : resources) {
      resource->OnDestroyed();
    }
  }

  // Releases all resources waiting to be released, then deletes them.
  void ProcessReleases(ResourceBinder* resource_binder) {
    // This can't be strictly 2-pass because deleting items may cause a resource
    // holder to be ready for release.  Loop until we have no more items to
    // release.
    const bool can_make_gl_calls = AreResourcesAccessible();
    while (true) {
      ResourceVector resources_to_destroy(*this);
      {
        std::lock_guard<std::mutex> lock(release_mutex_);
        // If the vector is empty when we enter the lock, we are done. Putting
        // this here avoids acquiring the mutex twice when there is nothing to
        // release.
        if (resources_to_release_.empty()) return;
        for (Resource* resource : resources_to_release_) {
          resource->Release(can_make_gl_calls);
          // Add the resource to the to-be-destroyed vector.
          resources_to_destroy.push_back(resource);
          DestroyResource(resource);
        }
        resources_to_release_.clear();
      }

      // Deletes all resources waiting to be destroyed. Note that this must be
      // separate from the above block since it is possible for the deletion of
      // a Resource to trigger an addition to the release queue. This can
      // happen, for example, when a ShaderProgram is destroyed: it releases
      // its list of Uniforms, which can trigger a Texture to be marked for
      // release, which requires a mutex lock.
      for (Resource* resource : resources_to_destroy)
        delete resource;
    }
  }

  // Delete all resources owned by this manager. Used for cleaning up on
  // destruction and when abandoning resources.
  void DestroyOrAbandonAllResources(bool force_abandon) {
    {
      std::lock_guard<std::mutex> lock(release_mutex_);
      resources_to_release_.clear();
    }
    bool can_make_gl_calls = force_abandon ? false : AreResourcesAccessible();
    for (int i = 0; i < kNumResourceTypes; ++i) {
      ResourceAccessor accessor(resources_[i]);
      ResourceVector& resources = accessor.GetResources();
      for (Resource* resource : resources) {
        resource->Release(can_make_gl_calls);
        delete resource;
      }
      resources.clear();
    }
    gl_context_.Reset();
  }

  void RestoreGlContext() const {
    portgfx::GlContext::MakeCurrent(gl_context_);
  }

  // Process any outstanding requests for data, e.g., PlatformInfo and
  // TextureImageInfo requests.
  template <typename T> void ProcessDataRequests()  {
    std::lock_guard<std::mutex> guard(this->request_mutex_);
    std::vector<DataRequest<T> >& requests = *GetDataRequestVector<T>();
    const size_t request_count = requests.size();
    std::vector<T> infos(1);
    for (size_t i = 0; i < request_count; ++i) {
      T info;
      FillDataFromRenderer(requests[i].id, &info);
      FillInfoFromOpenGL(&info);
      infos[0] = info;
      // Execute the callback.
      requests[i].callback(infos);
    }
    requests.clear();
  }

  // Process any outstanding requests for a particular Resource type.
  template <typename HolderType, typename InfoType>
  void ProcessInfoRequests(ResourceContainer* resource_container,
                           ResourceBinder* resource_binder) {
    std::lock_guard<std::mutex> guard(this->request_mutex_);
    std::vector<ResourceRequest<HolderType, InfoType> >& requests =
        *GetResourceRequestVector<HolderType, InfoType>();
    const size_t request_count = requests.size();
    for (size_t i = 0; i < request_count; ++i)
      ProcessInfoRequest<HolderType, InfoType>(requests[i], resource_container,
                                               resource_binder);
    requests.clear();
  }

  // Process a single request for information about a particular Resource.
  template <typename HolderType, typename InfoType>
  void ProcessInfoRequest(const ResourceRequest<HolderType, InfoType>& request,
                          ResourceContainer* resource_container,
                          ResourceBinder* resource_binder) {
    typedef typename HolderToResource<HolderType>::ResourceType ResourceType;
    std::vector<InfoType> infos;
    if (request.holder.Get()) {
      // Get the resource for the holder and its info.
      // 
      // Instead, we should simply do nothing for holders that do not have
      // any resources assigned to them yet.
      if (ResourceType* resource =
          GetResource(request.holder.Get(), resource_binder)) {
        AppendResourceInfo(&infos, resource, resource_binder);
      }
    } else {
      // Add an info for each Resource for the current renderer.
      ResourceAccessor accessor(*resource_container);
      ResourceVector& resources = accessor.GetResources();
      const std::vector<ResourceKey> keys =
          GetAllResourceKeys<ResourceType>(resource_binder);
      std::unordered_set<ResourceKey> key_set(keys.begin(), keys.end());
      for (Resource* resource : resources) {
        if (key_set.find(resource->GetKey()) != key_set.end()) {
          ResourceType* typed_resource = static_cast<ResourceType*>(resource);
          AppendResourceInfo(&infos, typed_resource, resource_binder);
        }
      }
    }
    // Execute the callback.
    request.callback(infos);
  }

  // Fills an info struct with information about the passed resource and
  // appends it to the passed vector of infos.
  template <typename InfoType, typename ResourceType>
  void AppendResourceInfo(std::vector<InfoType>* infos,
                          ResourceType* resource, ResourceBinder* rb) {
    InfoType info;
    resource->Bind(rb);
    // Fill info that is stored in the Resource itself.
    info.id = resource->GetId();
    info.label = resource->GetHolder()->GetLabel();
    FillInfoFromResource(&info, resource, rb);
    // Get the rest of the information directly from OpenGL.
    FillInfoFromOpenGL(&info);
    resource->Unbind(rb);
    infos->push_back(info);
  }

  // Fills in resource info fields from a resource.
  template <typename InfoType, typename ResourceType>
  void FillInfoFromResource(InfoType* info, ResourceType* resource,
                            ResourceBinder* rb) {}

  // Fills in data info fields from the renderer.
  template <typename InfoType>
  void FillDataFromRenderer(GLuint id, InfoType* info);

  // Process any outstanding requests for information about Resources.
  void ProcessResourceInfoRequests(ResourceBinder* resource_binder);

 private:
  // Wrapper struct for std::atomic<size_t> that is copy-constructable. This
  // allows the value to be used in an STL container.
  struct AtomicSizeT {
    AtomicSizeT() : value(0U) {}
    AtomicSizeT(const AtomicSizeT& other) : value(other.value.load()) {}
    AtomicSizeT& operator=(const AtomicSizeT& other) {
      value = other.value.load();
      return *this;
    }
    std::atomic<size_t> value;
  };

  // Returns a unique index for a ResourceManager to use when getting and
  // setting resources in ResourceHolders, or releases the index for reuse if
  // is_release is true.
  static size_t AcquireOrReleaseResourceIndex(bool is_release, size_t index) {
    using IndexArray = std::vector<bool>;
    ION_DECLARE_SAFE_STATIC_POINTER(std::mutex, mutex);
    ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
        IndexArray, used_indices, new IndexArray());
    std::lock_guard<std::mutex> guard(*mutex);
    if (is_release) {
      CHECK_GT(used_indices->size(), index)
          << "Encountered invalid resource index " << index;
      (*used_indices)[index] = false;
      // We could shrink the used_indices vector here, but there is no point,
      // since it will only grow to the number of simultaneously present
      // Renderer instances, and this number is practically always less than 10.
      return index;
    } else {
      // Find a new index. This is called very rarely (whenever a Renderer is
      // constructed), so we can get away with using a vector.
      auto it = std::find(used_indices->begin(), used_indices->end(), false);
      if (it == used_indices->end()) {
        used_indices->push_back(true);
        const size_t new_index = used_indices->size() - 1;
        if (new_index >= ResourceHolder::kInlineResourceGroups) {
          LOG(WARNING) << "Application created more than "
                       << ResourceHolder::kInlineResourceGroups
                       << "ion::gfx::Renderers at the same time. "
                       << "Performance may be adversely affected.";
        }
        return new_index;
      }
      *it = true;
      return static_cast<size_t>(std::distance(used_indices->begin(), it));
    }
  }

  // Resource creation helper.
  template <typename HolderType>
  typename HolderToResource<HolderType>::ResourceType* CreateResource(
      const HolderType* holder, ResourceBinder* binder, ResourceKey key,
      GLuint gl_id);

  // The unique index of this.
  size_t resource_index_;

  // Stores all managed resources.
  ResourceContainer resources_[kNumResourceTypes];

  // Stores memory usage by resource type.
  base::AllocVector<AtomicSizeT> memory_usage_;

  // Stores all resources that need to be released at the next convenient
  // time. Because the objects holding the resources may be modified or deleted
  // in any thread, it's possible that the graphics state is not available to
  // free the OpenGL resources at that time.
  ResourceVector resources_to_release_;

  // For locking access to resources_to_release_. This is needed since multiple
  // threads may destroy resources at the same time as holders are destroyed.
  std::mutex release_mutex_;

  // The GL context that was current when the first resource in this manager was
  // created. Ensures that the GlContext object does not disappear if it's
  // a wrapper.
  // 
  portgfx::GlContextPtr gl_context_;

  // Whether to always check the context stamp when checking for resource
  // availability.
  bool check_stamp_;
};

//-----------------------------------------------------------------------------
//
// The Renderer::ResourceBinder manages the binding state of all OpenGL
// resources for a particular OpenGL context. Only a single thread should be
// working with one ResourceBinder at one time.
//
//-----------------------------------------------------------------------------

class Renderer::ResourceBinder : public Allocatable {
 public:
  typedef base::AllocVector<Uniform> UniformStack;

  template <typename T>
  struct ResourceBinding {
    ResourceBinding() : gl_id(0U), resource(nullptr) {}
    GLuint gl_id;
    T* resource;
  };

  typedef ResourceBinding<BufferResource> BufferBinding;
  typedef ResourceBinding<FramebufferResource> FramebufferBinding;
  typedef ResourceBinding<ShaderProgramResource> ShaderProgramBinding;
  typedef ResourceBinding<TransformFeedbackResource> TransformFeedbackBinding;
  typedef ResourceBinding<VertexArrayResource> VertexArrayBinding;

  // An ImageUnit represents an OpenGL image unit.
  struct ImageUnit {
    ImageUnit() : sampler(0U), resource(nullptr), unit_index(-1),
                  next(nullptr), prev(nullptr), available(true) {}
    GLuint sampler;
    TextureResource* resource;
    // Unit can be inferred but is cached for debugging.
    int unit_index;
    // Linked list of ImageUnits where first is LRU and last is MRU.
    // 
    // following linked list code.
    ImageUnit* next;
    ImageUnit* prev;
    // If true, unit is available for reuse.
    bool available;
  };

  // A StreamAnnotator annotates the Renderer's GraphicsManager's tracing stream
  // output using passed labels to help identify what resources lead to what
  // OpenGL calls. In production builds it should optimize away to nothing.
#if ION_PRODUCTION
  class StreamAnnotator : public base::Allocatable {
   public:
    explicit StreamAnnotator(const GraphicsManagerPtr& gm) {}
    void Push(const std::string& label) {}
    void Pop() {}
  };
#else
  class StreamAnnotator : public base::Allocatable {
   public:
    explicit StreamAnnotator(const GraphicsManagerPtr& gm)
        : gm_(gm),
          stream_(gm_->GetTracingStream()),
          gl_supports_markers_(
              gm_->IsFeatureAvailable(GraphicsManager::kDebugMarker)) {}

    // Pushes a label onto the tracing stack and outputs it if the contained
    // GraphicsManager has a tracing stream.
    void Push(const std::string& marker) {
      intptr_t context_id = portgfx::GlContext::GetCurrentId();
      stream_.EnterScope(context_id, marker);
      if (gl_supports_markers_)
        gm_->PushGroupMarker(static_cast<GLsizei>(marker.length()),
                             marker.c_str());
    }

    // Pops the last label (if there is one) off of the tracing stack.
    void Pop() {
      intptr_t context_id = portgfx::GlContext::GetCurrentId();
      if (stream_.Depth(context_id) > 0) {
        if (gl_supports_markers_)
          gm_->PopGroupMarker();
        stream_.ExitScope(context_id);
      }
    }

   private:
    GraphicsManagerPtr gm_;
    TracingStream& stream_;
    const bool gl_supports_markers_;
  };
#endif

  class InfoRequestGuard {
   public:
    explicit InfoRequestGuard(ResourceBinder* rb)
      : rb_(rb) {
      rb_->processing_info_requests_ = true;
    }
    ~InfoRequestGuard() {
      rb_->processing_info_requests_ = false;
    }

   private:
    ResourceBinder* rb_;
  };

  // The constructor is passed a GraphicsManager for updating GL object state.
  explicit ResourceBinder(const GraphicsManagerPtr& gm)
      : graphics_manager_(gm),
        stream_annotator_(new (GetAllocator()) StreamAnnotator(gm)),
        image_units_(*this),
        texture_last_bindings_(*this),
        active_image_unit_(kInvalidGluint),
        resource_manager_(nullptr),
        current_shader_program_(nullptr),
        vertex_array_keys_(*this),
        gl_state_table_(new (GetAllocator()) StateTable(0, 0)),
        client_state_table_(new (GetAllocator()) StateTable(0, 0)),
        traversal_state_tables_(*this),
        current_traversal_index_(0U),
        processing_info_requests_(false) {
    memset(saved_ids_, 0, sizeof(saved_ids_));
    saved_state_table_ = new (GetAllocator()) StateTable();
    active_framebuffer_.gl_id = kInvalidGluint;

    // Enable program point sizes if the platform needs it.
    if (gm->GetGlFlavor() == GraphicsManager::kDesktop) {
      // Point sprites are always on in OpenGL 3.2+ core profiles.
      if (gm->GetGlProfileType() != GraphicsManager::kCoreProfile)
        gm->Enable(GL_POINT_SPRITE);
      gm->Enable(GL_PROGRAM_POINT_SIZE);
    }

    // Get the max number of image units and initialize image_units_.
    const int max_image_units = GetGraphicsManager()->GetConstant<int>(
        GraphicsManager::kMaxTextureImageUnits);
    InitImageUnits(0, max_image_units - 1);

    // Resize indexed buffer targets.
    const int num_attribs = gm->GetConstant<int>(
        GraphicsManager::kMaxTransformFeedbackSeparateAttribs);
    active_indexed_buffers_[BufferObject::kIndexedTransformFeedbackBuffer]
        .resize(std::max(0, num_attribs));

    traversal_state_tables_.resize(16);
    for (size_t i = 0; i < 16U; ++i)
      traversal_state_tables_[i] = new(GetAllocator()) StateTable;
  }

  ~ResourceBinder() override {}

  // Saves the currently bound GL framebuffer as the default framebuffer.
  void UpdateDefaultFramebufferFromOpenGL() {
    GetGraphicsManager()->GetIntegerv(GL_FRAMEBUFFER_BINDING,
                                      GetSavedId(Renderer::kSaveFramebuffer));
  }

  // Returns the GraphicsManager used for the instance.
  const GraphicsManagerPtr& GetGraphicsManager() const {
    return graphics_manager_;
  }

  // Gets/sets the current ResourceManager, which must remain valid as long as
  // this is operating on its resources.
  ResourceManager* GetResourceManager() const { return resource_manager_; }
  void SetResourceManager(ResourceManager* manager) {
    resource_manager_ = manager;
  }

  // Returns the stream annotator for debug tracing.
  StreamAnnotator* GetStreamAnnotator() const {
    return stream_annotator_.get();
  }

  // Returns the last bound FramebufferObject.
  const FramebufferObjectPtr GetCurrentFramebuffer() const {
    return current_fbo_.Acquire();
  }

  // Stores a weak reference to the currently bound FramebufferObject.
  void SetCurrentFramebuffer(const FramebufferObjectPtr& fbo) {
    current_fbo_ = base::WeakReferentPtr<FramebufferObject>(fbo);
  }

  // Returns the currently active shader program resource.
  ShaderProgramResource* GetActiveShaderProgram() const {
    return active_shader_.resource;
  }

  // Returns the currently active transform feedback object.
  TransformFeedbackResource* GetActiveTransformFeedback() const {
    return active_transform_feedback_.resource;
  }

  // Returns the currently active framebuffer resource.
  FramebufferResource* GetActiveFramebuffer() const {
    return active_framebuffer_.resource;
  }

  // Returns the currently active vertex array resource.
  VertexArrayResource* GetActiveVertexArray() const {
    return active_vertex_array_.resource;
  }

  // Sets the currently active vertex array resource to the passed resource.
  // Normal VertexArrayResources must use BindVertexArray(). When we are using
  // emulated vertex arrays, however, we still need to track the latest bound
  // state to avoid duplicate calls.
  void SetActiveVertexArray(VertexArrayResource* resource) {
    active_vertex_array_.resource = resource;
  }

  // Returns the StateTable that the Renderer believes to represent the current
  // state of OpenGL.
  StateTable* GetStateTable() const { return gl_state_table_.Get(); }

  // Binds the passed buffer to target if it is not already bound there.
  void BindBuffer(BufferObject::Target target, GLuint id,
                  BufferResource* resource);
  void BindBufferIndexed(BufferObject::IndexedTarget target, GLuint index,
                         GLuint id, BufferResource* resource);

  // Binds the passed framebuffer if it is not already bound there.
  void BindFramebuffer(GLuint id, FramebufferResource* fbo);

  // Makes the passed program active if it is not already active. Returns
  // whether the program was bound.
  bool BindProgram(GLuint id, ShaderProgramResource* resource);

  // Binds a sampler to the passed image unit if it is not already bound
  // there.
  void BindSamplerToUnit(GLuint id, GLuint unit) {
    DCHECK_LT(unit, image_units_.size());
    if (!id || id != image_units_[unit].sampler) {
      image_units_[unit].sampler = id;
      GetGraphicsManager()->BindSampler(unit, id);
    }
  }

  // Activates the passed texture unit, if it is not already activated.
  void ActivateUnit(GLuint unit_index);

  // Returns the 'best' image unit for txr, given desired_index. If txr is
  // already bound to desired_index or desired_index is available,
  // returns desired_index. Otherwise returns the first available, least
  // recently used unit's index. If no units are available,
  // returns the LRU unit's index.
  int ObtainImageUnit(TextureResource* txr, int desired_index);

  // Makes unit available for reuse.
  void ReleaseImageUnit(int unit_index) {
    DCHECK_LE(0, unit_index);
    if (unit_index < static_cast<int>(image_units_.size()))
      image_units_[unit_index].available = true;
  }

  // Makes unit unavailable for reuse and makes unit the least recently used
  // unit.
  void UseImageUnit(int unit_index, TextureResource* txr);

  // Binds a texture to the passed target at the passed image unit if it is not
  // already bound there.
  void BindTextureToUnit(TextureResource* resource, GLuint unit_index);

  // Bind the given transform feedback object if it is not already bound.
  void BindTransformFeedback(GLuint id, TransformFeedbackResource* tf);

  bool WasTextureEvicted(TextureResource* resource) {
    std::lock_guard<std::mutex> guard(texture_bindings_mutex_);
    auto found = texture_last_bindings_.find(resource);
    if (found == texture_last_bindings_.end())
      return false;
    if (image_units_[found->second].resource == resource)
      return false;
    return true;
  }
  // Returns the image unit to which the texture was last bound, or -1
  // if the texture was not bound anywhere yet.
  int GetLastBoundUnit(TextureResource* resource) {
    std::lock_guard<std::mutex> guard(texture_bindings_mutex_);
    auto found = texture_last_bindings_.find(resource);
    if (found != texture_last_bindings_.end())
      return found->second;
    return -1;
  }

  void SetLastBoundUnit(TextureResource* resource, int unit) {
    std::lock_guard<std::mutex> guard(texture_bindings_mutex_);
    texture_last_bindings_[resource] = unit;
  }

  // Clears the entry in the map used to track texture unit binding.
  void ClearAssignedImageUnit(TextureResource* resource) {
    std::lock_guard<std::mutex> guard(texture_bindings_mutex_);
    texture_last_bindings_.erase(resource);
  }

  // Binds the passed vertex array if it is not already bound.
  void BindVertexArray(GLuint id, VertexArrayResource* resource);

  // Clears the buffer bound to target if it is already bound there.
  void ClearBufferBinding(BufferObject::Target target, GLuint id) {
    if (!id || id == active_buffers_[target].gl_id) {
      active_buffers_[target] = BufferBinding();
    }
  }

  // Checks all cached buffer bindings for the passed ID and clears them if
  // equal. When 0 is passed, clears all buffer bindings.
  void ClearBufferBindings(GLuint id) {
    for (auto& binding : active_buffers_) {
      if (!id || id == binding.gl_id) {
        binding = BufferBinding();
      }
    }
    for (std::vector<BufferBinding>& v : active_indexed_buffers_) {
      for (auto& binding : v) {
        if (!id || id == binding.gl_id) {
          binding = BufferBinding();
        }
      }
    }
  }

  // Clears the framebuffer binding if it is already bound.
  void ClearFramebufferBinding(GLuint id) {
    if (!id || id == active_framebuffer_.gl_id) {
      active_framebuffer_.gl_id = kInvalidGluint;
      active_framebuffer_.resource = nullptr;
    }
  }

  // Clears the passed program binding if it is being used.
  void ClearProgramBinding(GLuint id) {
    if (!id || id == active_shader_.gl_id) {
      active_shader_ = ShaderProgramBinding();
    }
  }

  // Clears the passed sampler from all image units it is bound to.
  void ClearSamplerBindings(GLuint id) {
    const size_t count = image_units_.size();
    for (size_t i = 0; i < count; ++i)
      if (id == image_units_[i].sampler)
        image_units_[i].sampler = 0;
  }

  // Clears the texture bound to the passed image unit if it is already bound
  // there.  If the id is 0, unconditionally clears the image unit.
  void ClearTextureBinding(GLuint id, GLuint unit_index);

  // Clears the specified texture from a range of image units.  If the id is 0,
  // unconditionally clears the range of image units.
  void ClearTextureBindings(GLuint id, GLuint start_unit = 0);

  // Clears the transform feedback binding if it is already bound.
  void ClearTransformFeedbackBinding(GLuint id) {
    if (!id || id == active_transform_feedback_.gl_id) {
      active_transform_feedback_ = TransformFeedbackBinding();
    }
  }

  // Clears the passed vertex array if it is already bound.
  void ClearVertexArrayBinding(GLuint id);

  // Clears all non-framebuffer cached bindings.
  void ClearNonFramebufferCachedBindings();

  template <typename HolderType>
  void BindResource(const HolderType* holder) {
    if (holder) {
      if (typename HolderToResource<HolderType>::ResourceType* resource =
          resource_manager_->GetResource(holder, this)) {
        // Update and bind the resource.
        resource->Update(this);
        resource->Bind(this);
      }
    }
  }

  // Operation type for updating resources. Traversal with this operation
  // will create or update resources for ShaderPrograms, Textures, Shapes,
  // ShaderInputRegistries and vertex arrays that require it. Any
  // disabled subtrees are skipped.
  struct CreateOrUpdateOp {
    template <typename HolderType>
    static void Process(ResourceBinder* rb, const HolderType* holder,
                        GLuint gl_id) {
      if (holder) {
        typename HolderToResource<HolderType>::ResourceType* resource =
            rb->GetResourceManager()->GetResource(holder, rb, gl_id);
        if (resource) {
          // Inform the resource binders that the resource(s) need(s) to be
          // rebound. According to the GL spec, this is sufficient for changes
          // that have finished in one shared context to be available
          // in another.
          resource->UnbindAll();

          // Update the resource.
          if (resource->AnyModifiedBitsSet())
            resource->Update(rb);
        }
      }
    }
  };

  // Operation type for requesting a forced update, which marks all
  // resources in the node graph as modified.
  struct RequestUpdateOp {
    template <typename HolderType>
    static void Process(ResourceBinder* rb, const HolderType* holder,
                        GLuint gl_id) {
      if (holder) {
        Renderer::SetResourceHolderBit(holder,
                                       ResourceHolder::kResourceChanged);
        holder->Notify();
      }
    }
  };

  // Intermediary type that takes care of special cases in processing resource
  // holders. The main reason for the existence of this class is that function
  // templates cannot be partially specialized, while classes can.
  template <typename Operation, typename HolderType>
  struct OperationImpl {
    static void Process(ResourceBinder* rb, const HolderType* holder,
                        GLuint gl_id) {
      Operation::Process(rb, holder, gl_id);
    }
  };

  // Special case for vertex arrays.
  template <typename Operation>
  struct OperationImpl<Operation, AttributeArray> {
    static void Process(ResourceBinder* rb, const AttributeArray* aa,
                        GLuint gl_id) {
      if (aa == nullptr) return;
      // Use a transient set so that a buffer will only be updated once.
      std::set<BufferObject*> buffers;
      const size_t buffer_attribute_count = aa->GetBufferAttributeCount();
      for (size_t i = 0; i < buffer_attribute_count; ++i) {
        const Attribute& a = aa->GetBufferAttribute(i);
        if (BufferObject* bo =
                a.GetValue<BufferObjectElement>().buffer_object.Get()) {
          // We only want to process each buffer once.
          if (buffers.find(bo) == buffers.end()) {
            Operation::Process(rb, bo, gl_id);
            buffers.insert(bo);
          }
        }
      }
      GraphicsManager* gm = rb->GetGraphicsManager().Get();
      if (gm->IsFeatureAvailable(GraphicsManager::kVertexArrays))
        Operation::Process(rb, aa, gl_id);
      else
        Operation::Process(rb,
            reinterpret_cast<const AttributeArrayEmulator*>(aa), gl_id);
    }
  };

  // Convenience template to invoke Process from the correct OperationImpl.
  // This eventually invokes the correct Process function in either
  // CreateOrUpdateOp or RequestUpdateOp.
  template <typename Operation, typename HolderType>
  void Process(const HolderType* holder, GLuint gl_id) {
    OperationImpl<Operation, HolderType>::Process(this, holder, gl_id);
  }

  // Generic function that performs an operation on all holders in a node
  // graph. The actual operation performed is determined by the specialization
  // of the Process() function for the Operation type. The recursive part
  // of the function is defined in Visit().
  template <typename Operation>
  void Traverse(const NodePtr& node, ShaderProgram* default_shader);

  // Generic function for processing the resources associated with a shape.
  template <typename Operation>
  void VisitShape(const ShapePtr& shape);

  // Makes the passed StateTable the active tracked state by making the proper
  // OpenGL calls.
  void ProcessStateTable(const StateTablePtr& state_table);

  // Returns an image of the specified format that contains the contents of the
  // hardware framebuffer. The passed range specifies the area to be read. The
  // passed range specifies the area to be read. The Allocator is used when
  // creating the Image.
  const ImagePtr ReadImage(const math::Range2i& range, Image::Format format,
                           const base::AllocatorPtr& allocator);

  // Sends a uniform value to OpenGL.
  void SendUniform(const Uniform& uniform, int location, GraphicsManager* gm);

  // Pushes uniforms in the passed list onto UniformStacks.
  void PushUniforms(const base::AllocVector<Uniform>& uniforms);

  // Pops the uniforms in the passed list, restoring their values in the shadow
  // state.
  void PopUniforms(const base::AllocVector<Uniform>& uniforms);

  // Returns a pointer to the saved framebuffer.
  GLint* GetSavedId(Renderer::Flag flag) {
    DCHECK_LE(kSaveActiveTexture, flag);
    DCHECK_LE(flag, kSaveVertexArray);
    return &saved_ids_[flag - kSaveActiveTexture];
  }

  // Returns the number of texture bindings (image units) that are supported.
  size_t GetImageUnitCount() const { return image_units_.size(); }

  // Sets the maximum image unit for this binder to use.
  void SetImageUnitRange(const Range1i& units);

  void MapBufferObjectDataRange(const BufferObjectPtr& buffer,
                                BufferObjectDataMapMode mode,
                                const math::Range1ui& range_in);
  void UnmapBufferObjectData(const BufferObjectPtr& buffer);

  // Draws the scene rooted at node.
  void DrawScene(const NodePtr& node, const Flags& flags,
                 ShaderProgram* default_shader);

  // Returns whether this is currently processing info requests. This is used to
  // prevent spurious errors from being generated.
  bool IsProcessingInfoRequests() const { return processing_info_requests_; }

  // Gets a ResourceKey for looking up the vertex array resource associated
  // with the currently active shader program. The key needs to be unique
  // across different ResourceBinders.
  ResourceKey GetVertexArrayKey() {
    auto key_iterator = vertex_array_keys_.find(GetActiveShaderProgram());
    if (key_iterator == vertex_array_keys_.end()) {
      return reinterpret_cast<ResourceKey>(
          &*vertex_array_keys_.insert(GetActiveShaderProgram()).first);
    } else {
      return reinterpret_cast<ResourceKey>(&*key_iterator);
    }
  }
  std::vector<ResourceKey> GetAllVertexArrayKeys() {
    std::vector<ResourceKey> keys;
    for (const auto &i : vertex_array_keys_) {
      keys.push_back(reinterpret_cast<ResourceKey>(&i));
    }
    return keys;
  }

  // Releases the vertex array key of a shader. To be called when the shader
  // is destroyed.
  void EraseVertexArrayKey(ShaderProgramResource *shader) {
    vertex_array_keys_.erase(shader);
  }

  void WrapExternalResource(Texture* holder, uint32 gl_id);
  void WrapExternalResource(FramebufferObject* holder, uint32 gl_id);

 private:
  // Gets the hash key for Resource types. By default this is just the
  // ResourceBinder.

  // Draws a single Node.
  void DrawNode(const Node& node, GraphicsManager* gm);
  // Draws a single Shape.
  void DrawShape(const Shape& shape, GraphicsManager* gm);
  // Draws a single Shape that has an IndexBuffer.
  void DrawIndexedShape(const Shape& shape, const IndexBuffer& ib,
                        GraphicsManager* gm);
  // Draws a single Shape that has no IndexBuffer.
  void DrawNonindexedShape(const Shape& shape, size_t vertex_count,
                           GraphicsManager* gm);

  // Marks the passed attachment's texture targets, if any, as having been
  // implicitly changed by a draw into a framebuffer.
  void MarkAttachmentImplicitlyChanged(
    const FramebufferObject::Attachment& attachment);

  // Generic function that recursively processes all holders in a node graph.
  // Private, since it depends on setup done by the Traverse() function.
  template <typename Operation>
  void Visit(const NodePtr& node);

  // Initializes image_units_ restricting unit usage to [first, last].
  void InitImageUnits(int first, int last);

  // The GraphicsManager used for this instance.
  GraphicsManagerPtr graphics_manager_;

  // Annotates tracing streams.
  std::unique_ptr<StreamAnnotator> stream_annotator_;

  // The last FramebufferObject bound to this Renderer.
  base::WeakReferentPtr<FramebufferObject> current_fbo_;

  // Tracks which sampler and texture are bound to what unit.
  base::AllocVector<ImageUnit> image_units_;
  Range1i image_unit_range_;
  ImageUnit* lru_unit_;  // Least recently used.
  ImageUnit* mru_unit_;  // Most recently used.
  // Keeps track of the last binding place of each texture.
  base::AllocUnorderedMap<TextureResource*, GLuint> texture_last_bindings_;
  std::mutex texture_bindings_mutex_;
  GLuint active_image_unit_;

  // Tracks which buffer objects are currently bound.
  std::array<BufferBinding, BufferObject::kNumTargets> active_buffers_;
  std::array<std::vector<BufferBinding>, BufferObject::kNumIndexedTargets>
      active_indexed_buffers_;

  // Tracks which framebuffer is currently bound.
  // Please note that if active_framebuffer_.gl_id equals
  // kInvalidGluint(static_cast<GLuint>(-1)), then it is invalid framebuffer.
  FramebufferBinding active_framebuffer_;

  // Tracks which shader program is currently active.
  ShaderProgramBinding active_shader_;

  // Tracks the currently-bound transform feedback.
  TransformFeedbackBinding active_transform_feedback_;

  // Tracks which vertex array is currently bound.
  VertexArrayBinding active_vertex_array_;

  // Storage for GL object IDs that are saved when kSave* flags are set, and
  // restored when kRestore* flags are set.
  GLint
      saved_ids_[Renderer::kSaveVertexArray - Renderer::kSaveActiveTexture + 1];
  StateTablePtr saved_state_table_;

  // The ResourceManager that owns the Resources this is operating on. This must
  // be set using SetResourceManager().
  ResourceManager* resource_manager_;

  // The currently active shader program during scene graph traversal.
  ShaderProgram* current_shader_program_;

  // This set is used to keep track of ResourceKeys for vertex arrays which
  // are unique across different shader programs and ResourceBinders.
  // The key is simply the address of the corresponding element stored
  // in the set. std::set is used instead of std::unordered_set, since
  // iterators into the latter can be invalidated by insertions.
  base::AllocSet<ShaderProgramResource*> vertex_array_keys_;

  // StateTable representing the global OpenGL state.
  StateTablePtr gl_state_table_;
  // StateTable representing the client traversal state.
  StateTablePtr client_state_table_;
  // Storage for saving StateTables encountered during traversal.
  base::AllocVector<StateTablePtr> traversal_state_tables_;
  size_t current_traversal_index_;

  // Whether this is currently processing info requests.
  bool processing_info_requests_;

  friend class InfoRequestGuard;
};

// VertexArrayResources must be created per shader program. This will also
// be unique across multiple ResourceBinders.
template <>
ResourceKey Renderer::ResourceManager::GetResourceKey<
    Renderer::VertexArrayResource>(ResourceBinder* resource_binder,
                                   const ResourceHolder* holder) {
  return resource_binder->GetVertexArrayKey();
}
template <>
std::vector<ResourceKey> Renderer::ResourceManager::GetAllResourceKeys<
    Renderer::VertexArrayResource>(ResourceBinder* resource_binder) {
  return resource_binder->GetAllVertexArrayKeys();
}

template <>
ResourceKey Renderer::ResourceManager::GetResourceKey<
    Renderer::VertexArrayEmulatorResource>(ResourceBinder* resource_binder,
                                           const ResourceHolder* holder) {
  return resource_binder->GetVertexArrayKey();
}
template <>
std::vector<ResourceKey> Renderer::ResourceManager::GetAllResourceKeys<
    Renderer::VertexArrayEmulatorResource>(ResourceBinder* resource_binder) {
  return resource_binder->GetAllVertexArrayKeys();
}

template <>
ResourceKey Renderer::ResourceManager::GetResourceKey<
    Renderer::ShaderInputRegistryResource>(ResourceBinder* resource_binder,
                                           const ResourceHolder* holder) {
  return reinterpret_cast<ResourceKey>(resource_binder);
}

template <>
ResourceKey Renderer::ResourceManager::GetResourceKey<
    Renderer::FramebufferResource>(ResourceBinder* resource_binder,
                                   const ResourceHolder* holder) {
  return reinterpret_cast<ResourceKey>(resource_binder);
}

template <>
ResourceKey Renderer::ResourceManager::GetResourceKey<
    Renderer::ShaderProgramResource>(ResourceBinder* resource_binder,
                                     const ResourceHolder* holder) {
  ShaderProgram* program = const_cast<ShaderProgram*>(
      static_cast<const ShaderProgram*>(holder));
  // If per-thread state was not set by the application, call SetConcurrent
  // here to prevent any subsequent modification.
  program->SetConcurrent(program->IsConcurrent());
  if (program->IsConcurrent()) {
    return reinterpret_cast<ResourceKey>(resource_binder);
  } else {
    return reinterpret_cast<ResourceKey>(this);
  }
}
template <>
std::vector<ResourceKey> Renderer::ResourceManager::GetAllResourceKeys<
    Renderer::ShaderProgramResource>(ResourceBinder* resource_binder) {
  std::vector<ResourceKey> keys;
  keys.push_back(reinterpret_cast<ResourceKey>(resource_binder));
  keys.push_back(reinterpret_cast<ResourceKey>(this));
  return keys;
}

void Renderer::ResourceManager::ProcessResourceInfoRequests(
    ResourceBinder* resource_binder) {
  ResourceBinder::InfoRequestGuard guard(resource_binder);

  // Process all requests for each Resource type.
  ProcessInfoRequests<AttributeArray, ArrayInfo>(&resources_[kAttributeArray],
                                                 resource_binder);
  ProcessInfoRequests<BufferObject, BufferInfo>(&resources_[kBufferObject],
                                                resource_binder);
  ProcessInfoRequests<FramebufferObject, FramebufferInfo>(
      &resources_[kFramebufferObject], resource_binder);
  ProcessInfoRequests<Sampler, SamplerInfo>(&resources_[kSampler],
                                            resource_binder);
  ProcessInfoRequests<ShaderProgram, ProgramInfo>(&resources_[kShaderProgram],
                                                  resource_binder);
  ProcessInfoRequests<Shader, ShaderInfo>(&resources_[kShader],
                                          resource_binder);
  ProcessInfoRequests<TextureBase, TextureInfo>(&resources_[kTexture],
                                                resource_binder);
  ProcessInfoRequests<TransformFeedback, TransformFeedbackInfo>(
      &resources_[kTransformFeedback], resource_binder);
  ProcessDataRequests<PlatformInfo>();
  ProcessDataRequests<TextureImageInfo>();
}

// Helper class that wraps a push and pop of a marker onto a stream annotator.
// It should optimize away to nothing in production builds.
#if ION_PRODUCTION
class Renderer::ScopedLabel {
 public:
  ScopedLabel(ResourceBinder* binder, const void* address,
              const std::string& label, const char* method) {}
};
#else
class Renderer::ScopedLabel {
 public:
  ScopedLabel(ResourceBinder* rb, const void* address,
              const std::string& label, const char* method)
      : annotator_(rb->GetStreamAnnotator()),
        needs_pop_(false) {
#if defined(ION_ANALYTICS_ENABLED)
    profile::CallTraceManager* const manager = profile::GetCallTraceManager();
    profile::TraceRecorder* const recorder = manager->GetTraceRecorder();
    const uint32 scope_event_id = recorder->GetScopeEvent(method);
    recorder->EnterScope(scope_event_id);
    if (!label.empty()) {
      recorder->AnnotateCurrentScope(
          std::string("label"), ion::base::QuoteString(label));
    }
#endif
    const auto& stream = rb->GetGraphicsManager()->GetTracingStream();
    if (stream.IsTracing() && !label.empty()) {
      annotator_->Push(label + " [" + base::ValueToString(address) + "]");
      needs_pop_ = true;
    }
  }
  ~ScopedLabel() {
    if (needs_pop_)
      annotator_->Pop();
#if defined(ION_ANALYTICS_ENABLED)
    profile::GetCallTraceManager()->GetTraceRecorder()->LeaveScope();
#endif
  }

 private:
  ResourceBinder::StreamAnnotator* annotator_;
  bool needs_pop_;
};
#endif

//-----------------------------------------------------------------------------
//
// Renderer-specific base Resource class. This is derived from the
// ResourceManager's Resource class so instances can be managed. The class
// stores the ResourceHolder object that owns the resource (in an opaque way).
// It communicates with the ResourceManager to make sure resources are released
// and destroyed when appropriate.
//
// The NumModifiedBits template parameter determines the size of the modified
// state bitset.
//-----------------------------------------------------------------------------

template <int NumModifiedBits>
class Renderer::Resource : public Renderer::ResourceManager::Resource {
 public:
  class ScopedResourceLabel : public ScopedLabel {
   public:
    ScopedResourceLabel(Resource* resource, ResourceBinder* binder,
                        const char* method)
        : ScopedLabel(binder, resource->GetHolder(),
                      resource->GetHolder()->GetLabel(), method) {}
  };

  ~Resource() override { DetachFromHolder(); }

  bool AnyModifiedBitsSet() const { return modified_bits_.any(); }

  void OnDestroyed() override {
    UnbindAll();
    // Detach the containing object from the object holding it.
    DetachFromHolder();
    if (GetResourceManager())
      GetResourceManager()->MarkForRelease(this);
  }

  // Returns the OpenGL object ID for this Resource.
  GLuint GetId() const { return id_; }

 protected:
  typedef Renderer::Resource<NumModifiedBits> BaseResourceType;

  Resource(ResourceManager* rm, const ResourceHolder& holder, ResourceKey key,
           GLuint id)
      : Renderer::ResourceManager::Resource(rm, &holder, key),
        id_(id),
        resource_owns_gl_id_(id == 0U) {
    // Mark that this resource needs to be Update()d by setting all bits to 1.
    SetModifiedBits();
    // We don't need the resource changed bit set when a resource is new; it
    // will by definition be bound anyway.
    modified_bits_.reset(ResourceHolder::kResourceChanged);
  }

  GraphicsManager* GetGraphicsManager() const {
    return GetResourceManager()->GetGraphicsManager().Get();
  }

  // Returns whether the resource has a valid holder.
  bool HasHolder() const { return GetHolder() != nullptr; }

  void Release(bool can_make_gl_calls) override { DetachFromHolder(); }

  // Modified bit accessors.
  void OnChanged(const int bit) override { modified_bits_.set(bit); }

  void ResetModifiedBit(int bit) { modified_bits_.reset(bit); }

  void ResetModifiedBits() { modified_bits_.reset(); }

  void SetModifiedBit(int bit) { modified_bits_.set(bit); }

  void SetModifiedBits() { modified_bits_.set(); }

  bool TestModifiedBit(int bit) const { return modified_bits_.test(bit); }

  // Returns whether any bits in the range [low_bit, high_bit] are set.
  // 
  bool TestModifiedBitRange(int low_bit, int high_bit) const {
    std::bitset<NumModifiedBits> mask;
    // Set mask to all 1s.
    mask.set();
    // The right end of the bitmask now has 0s for the number of bits we want to
    // test.
    mask <<= high_bit + 1 - low_bit;
    // Flip the bits so that the rightmost number of bits we want to test are
    // all 1s.
    mask.flip();
    // Shift the bits to the desired range.
    mask <<= low_bit;
    // Return whether modified_bits_ has any of the same bits set.
    return (mask & modified_bits_).any();
  }

  const std::bitset<NumModifiedBits>& GetBits() const { return modified_bits_; }

  friend class ResourceBinder;

  // The OpenGL ID for this Resource.
  GLuint id_;
  // Whether this Resource manages the OpenGL object ID and should delete it.
  const bool resource_owns_gl_id_;

 private:
  void DetachFromHolder() {
    if (HasHolder()) {
      const ResourceHolder* holder = GetHolder();
      const size_t index = GetResourceManager()->GetResourceIndex();
      // Only change the holder's resource if this is the resource it holds. We
      // always obtain a write lock to prevent any readers from getting the
      // wrong value. This only happens when Resources are being destroyed.
      if (holder->GetResource(index, GetKey()) == this) {
        // Notify any listeners of the holder that something is changing.
        holder->Notify();
        holder->SetResource(index, GetKey(), nullptr);
      }
    }
  }

  std::bitset<NumModifiedBits> modified_bits_;
};

//-----------------------------------------------------------------------------
//
// Renderer::SamplerResource class.
//
//-----------------------------------------------------------------------------

class Renderer::SamplerResource : public Resource<Sampler::kNumChanges> {
 public:
  SamplerResource(ResourceBinder* rb, ResourceManager* rm,
                  const Sampler& sampler, ResourceKey key, GLuint id)
      : Renderer::Resource<Sampler::kNumChanges>(rm, sampler, key, id) {}

  ~SamplerResource() override {
    DCHECK(id_ == 0U || !portgfx::GlContext::GetCurrent());
  }

  void Release(bool can_make_gl_calls) override;
  void Update(ResourceBinder* rb) override;
  ResourceType GetType() const override { return kSampler; }

  // Sampler objects are never bound in the traditional sense, instead they are
  // attached to image units (through BindToUnit()).
  void Bind(ResourceBinder* rb) { Update(rb); }
  void Unbind(ResourceBinder* rb) override;

  // Binds the sampler object to the passed image unit.
  void BindToUnit(ResourceBinder* rb, GLuint unit);

  const Sampler& GetSampler() const {
    return static_cast<const Sampler&>(*GetHolder());
  }
};

void Renderer::SamplerResource::Update(ResourceBinder* rb) {
  if (GetGraphicsManager()->IsFeatureAvailable(
          GraphicsManager::kSamplerObjects)) {
    const Sampler& sampler = GetSampler();

    if (AnyModifiedBitsSet()) {
      SCOPED_RESOURCE_LABEL;
      // Generate the sampler object.
      GraphicsManager* gm = GetGraphicsManager();
      DCHECK(gm);
      if (!id_) gm->GenSamplers(1, &id_);
      if (id_) {
        // Note that we explicitly do not label sampler objects as many GL
        // drivers do not support the GL_SAMPLER label.

        // Make any changes to the sampler.
        if (TestModifiedBit(Sampler::kMaxAnisotropyChanged) &&
            gm->IsFeatureAvailable(
                GraphicsManager::kTextureFilterAnisotropic)) {
          const float aniso =
              std::min(sampler.GetMaxAnisotropy(),
                       gm->GetConstant<float>(
                           GraphicsManager::kMaxTextureMaxAnisotropy));
          gm->SamplerParameterf(id_, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
        }
        if (TestModifiedBit(Sampler::kMinFilterChanged))
          gm->SamplerParameteri(
              id_, GL_TEXTURE_MIN_FILTER,
              base::EnumHelper::GetConstant(sampler.GetMinFilter()));
        if (TestModifiedBit(Sampler::kMagFilterChanged))
          gm->SamplerParameteri(
              id_, GL_TEXTURE_MAG_FILTER,
              base::EnumHelper::GetConstant(sampler.GetMagFilter()));
        if (TestModifiedBit(Sampler::kWrapSChanged))
          gm->SamplerParameteri(
              id_, GL_TEXTURE_WRAP_S,
              base::EnumHelper::GetConstant(sampler.GetWrapS()));
        if (TestModifiedBit(Sampler::kWrapTChanged))
          gm->SamplerParameteri(
              id_, GL_TEXTURE_WRAP_T,
              base::EnumHelper::GetConstant(sampler.GetWrapT()));
        if (TestModifiedBit(Sampler::kCompareFunctionChanged) &&
            gm->IsFeatureAvailable(GraphicsManager::kShadowSamplers))
          gm->SamplerParameteri(
              id_, GL_TEXTURE_COMPARE_FUNC,
              base::EnumHelper::GetConstant(sampler.GetCompareFunction()));
        if (TestModifiedBit(Sampler::kCompareModeChanged) &&
            gm->IsFeatureAvailable(GraphicsManager::kShadowSamplers))
          gm->SamplerParameteri(
              id_, GL_TEXTURE_COMPARE_MODE,
              sampler.GetCompareMode() == Sampler::kCompareToTexture
                  ? GL_COMPARE_REF_TO_TEXTURE
                  : GL_NONE);
        if (TestModifiedBit(Sampler::kMaxLodChanged))
          gm->SamplerParameterf(id_, GL_TEXTURE_MAX_LOD, sampler.GetMaxLod());
        if (TestModifiedBit(Sampler::kMinLodChanged))
          gm->SamplerParameterf(id_, GL_TEXTURE_MIN_LOD, sampler.GetMinLod());
        if (TestModifiedBit(Sampler::kWrapRChanged))
          gm->SamplerParameteri(
              id_, GL_TEXTURE_WRAP_R,
              base::EnumHelper::GetConstant(sampler.GetWrapR()));
        if (TestModifiedBit(ResourceHolder::kLabelChanged))
          SetObjectLabel(gm, GL_SAMPLER, id_, sampler.GetLabel());
        ResetModifiedBits();
      } else {
        LOG(ERROR) << "***ION: Unable to create sampler object";
      }
    }
  }
}

void Renderer::SamplerResource::BindToUnit(ResourceBinder* rb, GLuint unit) {
  Update(rb);
  if (id_)
    rb->BindSamplerToUnit(id_, unit);
}

void Renderer::SamplerResource::Unbind(ResourceBinder* rb) {
  if (id_ && rb)
    rb->ClearSamplerBindings(id_);
}

void Renderer::SamplerResource::Release(bool can_make_gl_calls) {
  BaseResourceType::Release(can_make_gl_calls);
  if (id_) {
    UnbindAll();
    if (resource_owns_gl_id_ && can_make_gl_calls)
      GetGraphicsManager()->DeleteSamplers(1, &id_);
    id_ = 0;
  }
}

//-----------------------------------------------------------------------------
//
// Renderer::TextureResource class.
//
//-----------------------------------------------------------------------------

// We use CubeMapTexture::kNumChanges since it has the most changes, and this
// resource class represents everything derived from TextureBase.
class Renderer::TextureResource : public Resource<CubeMapTexture::kNumChanges> {
 public:
  TextureResource(ResourceBinder* rb, ResourceManager* rm,
                  const TextureBase& texture, ResourceKey key, GLuint id)
      : Renderer::Resource<CubeMapTexture::kNumChanges>(rm, texture, key, id),
        gl_target_(0),
        last_uploaded_components_(0U),
        auto_mipmapping_enabled_(false),
        max_anisotropy_(0.f),
        min_lod_(0.f),
        max_lod_(0.f),
        compare_function_(base::InvalidEnumValue<Sampler::CompareFunction>()),
        compare_mode_(base::InvalidEnumValue<Sampler::CompareMode>()),
        min_filter_(base::InvalidEnumValue<Sampler::FilterMode>()),
        mag_filter_(base::InvalidEnumValue<Sampler::FilterMode>()),
        wrap_r_(base::InvalidEnumValue<Sampler::WrapMode>()),
        wrap_s_(base::InvalidEnumValue<Sampler::WrapMode>()),
        wrap_t_(base::InvalidEnumValue<Sampler::WrapMode>()),
        multisample_enabled_by_renderer_(false) {
    DCHECK_GE(static_cast<int>(CubeMapTexture::kNumChanges),
              static_cast<int>(Texture::kNumChanges));
  }

  ~TextureResource() override {
    DCHECK(id_ == 0U || !portgfx::GlContext::GetCurrent());
  }

  // Updates this TextureResource and binds it to a new unit.
  void Update(ResourceBinder* rb) override;
  // Updates this TextureResource and binds it to the passed unit.
  void UpdateWithUnit(ResourceBinder* rb, GLuint unit);
  void Release(bool can_make_gl_calls) override;

  ResourceType GetType() const override { return kTexture; }

  GLenum GetGlTarget() const { return gl_target_; }

  int GetDimensions() const {
    if (gl_target_ == GL_TEXTURE_3D ||
        gl_target_ == GL_TEXTURE_2D_MULTISAMPLE_ARRAY ||
        gl_target_ == GL_TEXTURE_CUBE_MAP_ARRAY) {
      return 3;
    } else {
      return 2;
    }
  }

  template <typename T> const T& GetTexture() const {
    return static_cast<const T&>(*(this->GetHolder()));
  }

  // Binds the texture object in OpenGL to a new unit.  Use BindToUnit() when
  // possible for performance.
  void Bind(ResourceBinder* rb);
  // Binds the texture object in OpenGL to the passed unit.
  void BindToUnit(ResourceBinder* rb, GLuint unit);
  void Unbind(ResourceBinder* rb) override;

  // Returns whether the "multisample enabled" bit has changed.
  bool SetMultisampleEnabledByRenderer(bool multisample_enabled_by_renderer) {
    const bool changed = multisample_enabled_by_renderer_ !=
        multisample_enabled_by_renderer;
    multisample_enabled_by_renderer_ = multisample_enabled_by_renderer;
    return changed;
  }

 protected:
  // Returns whether the texture is complete and can be updated.
  bool IsComplete() const;

  // Updates the texture's image data.
  void UpdateCubeMapImageState(GraphicsManager* gm);
  void UpdateTextureImageState(GraphicsManager* gm, bool multisample,
                               bool multisample_changed);

  // Creates an immutable texture with TexStorage?D().
  void CreateImmutableTexture(const Image& image, const bool multisample,
                              const size_t samples,
                              const bool fixed_sample_locations, size_t levels,
                              GraphicsManager* gm);

  bool CheckImage(const Image& image, const TextureBase& texture);
  void UploadImage(const Image& image, GLenum target, GLint level,
                   int samples, bool fixed_sample_locations,
                   bool is_full_image, const Point3ui& offset,
                   GraphicsManager* gm);
  void UpdateMemoryUsage(TextureBase::TextureType type);
  void UpdateMipmapGeneration(const Sampler& sampler, bool image_has_changed,
                              GraphicsManager* gm);
  bool UpdateMipmap0Image(const Image& image, const TextureBase& texture,
                          const size_t mipmap_count, GLenum target,
                          int mipmap_changed_bit, GraphicsManager* gm,
                          size_t* required_levels, bool multisample_changed);
  bool UpdateImage(const Image& image0, const Image& mipmap,
                   const TextureBase& texture, GLenum target, int level,
                   GraphicsManager* gm);
  void UpdateState(const TextureBase& texture, ResourceBinder* rb, GLuint unit);
  void UpdateSamplerState(const Sampler& sampler, GraphicsManager* gm);
  void UpdateSubImages(const base::AllocVector<Texture::SubImage>& images,
                       GLenum target, GraphicsManager* gm);
  void UpdateTextureState(const TextureBase& texture, GraphicsManager* gm);

  GLenum gl_target_;
  int last_uploaded_components_;

  // State shadowed from TextureBase's Sampler.
  bool auto_mipmapping_enabled_;
  float max_anisotropy_;
  float min_lod_;
  float max_lod_;
  Sampler::CompareFunction compare_function_;
  Sampler::CompareMode compare_mode_;
  Sampler::FilterMode min_filter_;
  Sampler::FilterMode mag_filter_;
  Sampler::WrapMode wrap_r_;
  Sampler::WrapMode wrap_s_;
  Sampler::WrapMode wrap_t_;

  // Tracking whether multisampling has been enabled by the renderer.
  bool multisample_enabled_by_renderer_;

 private:
  // Updates the texture target to use with OpenGL.
  void UpdateTextureTarget(GraphicsManager* gm, const bool multisample) {
    // Determine the texture target.
    const TextureBase& base = GetTexture<TextureBase>();
    Image* image = base.GetImmutableImage().Get();
    if (base.GetTextureType() == TextureBase::kCubeMapTexture) {
      if (image) {
        UpdateCubeMapTextureTypeFromImage(*image);
      } else {
        const CubeMapTexture& texture = GetTexture<CubeMapTexture>();
        if (texture.HasImage(CubeMapTexture::kNegativeX, 0U)) {
          UpdateCubeMapTextureTypeFromImage(
              *texture.GetImage(CubeMapTexture::kNegativeX, 0U));
        }
      }
    } else {
      if (image) {
        UpdateTextureTypeFromImage(*image, multisample);
      } else {
        const Texture& texture = GetTexture<Texture>();
        if (texture.HasImage(0U))
          UpdateTextureTypeFromImage(*texture.GetImage(0U), multisample);
      }
    }
  }

  // Sets the texture targets for a non-cubemap texture based on an image
  // and whether multisampling is enabled.
  void UpdateTextureTypeFromImage(const Image& image, bool multisample) {
    if (image.GetType() == Image::kEgl) {
      if (image.GetDimensions() == Image::k2d) {
        gl_target_ = GL_TEXTURE_2D;
      } else {
        gl_target_ = GL_TEXTURE_2D_ARRAY;
      }
    } else if (image.GetType() == Image::kExternalEgl) {
      gl_target_ = GL_TEXTURE_EXTERNAL_OES;
    } else {
      if (image.GetDimensions() == Image::k2d) {
        if (image.GetType() == Image::kArray) {
          gl_target_ = GL_TEXTURE_1D_ARRAY;
        } else if (image.GetType() == Image::kDense) {
          if (multisample) {
            gl_target_ = GL_TEXTURE_2D_MULTISAMPLE;
          } else {
            gl_target_ = GL_TEXTURE_2D;
          }
        }
      } else if (image.GetDimensions() == Image::k3d) {
        if (image.GetType() == Image::kArray) {
          if (multisample) {
            gl_target_ = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
          } else {
            gl_target_ = GL_TEXTURE_2D_ARRAY;
          }
        } else if (image.GetType() == Image::kDense) {
          gl_target_ = GL_TEXTURE_3D;
        }
      }
    }
  }

  // Sets the texture targets for a cubemap based on an image.
  void UpdateCubeMapTextureTypeFromImage(const Image& image) {
    if (image.GetType() == Image::kEgl) {
      gl_target_ = GL_TEXTURE_2D;
    } else if (image.GetType() == Image::kExternalEgl) {
      gl_target_ = GL_TEXTURE_EXTERNAL_OES;
    } else {
      if (image.GetDimensions() == Image::k3d ||
          image.GetType() == Image::kArray) {
        gl_target_ = GL_TEXTURE_CUBE_MAP_ARRAY;
      } else if (image.GetType() == Image::kDense) {
        gl_target_ = GL_TEXTURE_CUBE_MAP;
      }
    }
  }
};

inline void Renderer::TextureResource::Bind(ResourceBinder* rb) {
  const GLuint unit = rb->ObtainImageUnit(this, -1);
  BindToUnit(rb, unit);
}

inline void Renderer::TextureResource::BindToUnit(
    ResourceBinder* rb, GLuint unit) {
  UpdateWithUnit(rb, unit);
  if (id_) {
    SCOPED_RESOURCE_LABEL;
    rb->BindTextureToUnit(this, unit);
    Sampler* sampler = GetTexture<TextureBase>().GetSampler().Get();
    if (sampler &&
        GetGraphicsManager()->IsFeatureAvailable(
            GraphicsManager::kSamplerObjects)) {
      // If we are using sampler objects then ensure the sampler is bound to the
      // same image unit as this.
      SamplerResource* sr = GetResource(sampler, rb);
      DCHECK(sr);
      sr->BindToUnit(rb, unit);
    }
  }
}

inline void Renderer::TextureResource::Update(ResourceBinder* rb) {
  const int unit = rb->ObtainImageUnit(this, -1);
  if (AnyModifiedBitsSet() && IsComplete())
    UpdateState(GetTexture<TextureBase>(), rb, unit);
}

inline void Renderer::TextureResource::UpdateWithUnit(
    ResourceBinder* rb, GLuint unit) {
  if (AnyModifiedBitsSet() && IsComplete())
    UpdateState(GetTexture<TextureBase>(), rb, unit);
}

bool Renderer::TextureResource::CheckImage(const Image& image,
                                           const TextureBase& texture) {
  // Special checks for non power of two textures.
  if (Sampler* sampler = texture.GetSampler().Get()) {
    if (!math::IsPowerOfTwo(image.GetWidth()) ||
        !math::IsPowerOfTwo(image.GetHeight())) {
      // From page 88 of the OpenGL ES2 specification, it is an error if the
      // texture is non-power-of-2 "... and either the texture wrap mode is
      // not CLAMP_TO_EDGE, or the minification filter is neither NEAREST nor
      // LINEAR."
      if ((sampler->GetWrapS() == Sampler::kClampToEdge ||
           sampler->GetWrapT() == Sampler::kClampToEdge) &&
          (sampler->GetMinFilter() >= Sampler::kNearestMipmapNearest)) {
        LOG(ERROR)
            << "***ION: Non-power-of-two textures using wrap mode "
            << "CLAMP_TO_EDGE must use either NEAREST or LINEAR minification "
            << "filter modes, use Texture::SetMinFilter(Sampler::kNearest) "
            << "or Texture::SetMinFilter(Sampler::kLinear) to fix this";
        return false;
      }
    }
  }
  return true;
}

void Renderer::TextureResource::UploadImage(const Image& image, GLenum target,
                                            GLint level, int samples,
                                            bool fixed_sample_locations,
                                            bool is_full_image,
                                            const Point3ui& offset,
                                            GraphicsManager* gm) {
  const Image::PixelFormat pf =
      GetCompatiblePixelFormat(Image::GetPixelFormat(image.GetFormat()), gm);
  const int component_count =
      Image::GetNumComponentsForFormat(image.GetFormat());
  if (last_uploaded_components_ &&
      component_count < last_uploaded_components_) {
    LOG(WARNING)
        << "While uploading image data for texture \""
        << GetTexture<TextureBase>().GetLabel()
        << "\", the number of components for this upload is " << component_count
        << " but was " << last_uploaded_components_
        << " the last time data was uploaded. This is likely not what you want "
           "as GL implementations are not guaranteed to provide particular "
           "values for the unset components.";
  }
  last_uploaded_components_ = component_count;

  const DataContainerPtr& container = image.GetData();
  const void* data = container.Get() ? container->GetData() : nullptr;
  gm->PixelStorei(GL_UNPACK_ALIGNMENT, 1);

  if (samples > 0 &&
      !gm->IsFeatureAvailable(GraphicsManager::kTextureMultisample)) {
    LOG(WARNING) << "Multisampling requested for texture \""
        << GetTexture<TextureBase>().GetLabel()
        << "\" but multisampled textures are not supported.  Falling back to "
        << "a non-multisampled format.";
  }

  const bool multisample = (samples > 0) &&
      gm->IsFeatureAvailable(GraphicsManager::kTextureMultisample);

  if (image.GetType() == Image::kEgl ||
      image.GetType() == Image::kExternalEgl) {
    // EGL images are a special case. We only need to link the EGLImage with the
    // bound texture.
    if (data && gm->IsFeatureAvailable(GraphicsManager::kEglImage)) {
      DCHECK(gl_target_ == GL_TEXTURE_EXTERNAL_OES ||
             gl_target_ == GL_TEXTURE_2D_ARRAY ||
             gl_target_ == GL_TEXTURE_2D);
      gm->EGLImageTargetTexture2DOES(gl_target_, const_cast<void*>(data));
    }
  } else if (image.GetWidth() > 0U && image.GetHeight() > 0U &&
             image.GetDepth() > 0U) {
    // Don't actually call a TexImage function if the dimensions of the texture
    // are zero. Even if data is nullptr we will still set the texture size and
    // format.
    if (image.IsCompressed() && data) {
      if (image.GetDimensions() == Image::k2d) {
        const size_t data_size = Image::ComputeDataSize(
            image.GetFormat(), image.GetWidth(), image.GetHeight());
        if (is_full_image) {
          gm->CompressedTexImage2D(target, level, pf.internal_format,
                                   image.GetWidth(), image.GetHeight(), 0,
                                   static_cast<GLsizei>(data_size), data);
        } else {
          gm->CompressedTexSubImage2D(target, level, offset[0], offset[1],
                                      image.GetWidth(), image.GetHeight(),
                                      pf.internal_format,
                                      static_cast<GLsizei>(data_size), data);
        }
      } else if (image.GetDimensions() == Image::k3d) {
        const size_t data_size =
            Image::ComputeDataSize(image.GetFormat(), image.GetWidth(),
                                   image.GetHeight(), image.GetDepth());
        if (gm->IsFeatureAvailable(GraphicsManager::kTexture3d)) {
          if (is_full_image) {
            gm->CompressedTexImage3D(target, level, pf.internal_format,
                                     image.GetWidth(), image.GetHeight(),
                                     image.GetDepth(), 0,
                                     static_cast<GLsizei>(data_size), data);
          } else {
            gm->CompressedTexSubImage3D(
                target, level, offset[0], offset[1], offset[2],
                image.GetWidth(), image.GetHeight(), image.GetDepth(),
                pf.internal_format, static_cast<GLsizei>(data_size), data);
          }
        } else {
          LOG(ERROR) << "***ION: 3D texturing is not supported by the local "
                     << "OpenGL implementation, but Texture \""
                     << GetTexture<TextureBase>().GetLabel()
                     << "\" contains a 3D Image.";
        }
      }
    } else {
      if (image.GetDimensions() == Image::k2d) {
        if (is_full_image) {
          if (multisample) {
            gm->TexImage2DMultisample(target, samples, pf.internal_format,
                                      image.GetWidth(), image.GetHeight(),
                                      fixed_sample_locations);
          } else {
            gm->TexImage2D(target, level, pf.internal_format, image.GetWidth(),
                           image.GetHeight(), 0, pf.format, pf.type, data);
          }
        } else {
          gm->TexSubImage2D(target, level, offset[0], offset[1],
                            image.GetWidth(), image.GetHeight(), pf.format,
                            pf.type, data);
        }
      } else if (image.GetDimensions() == Image::k3d) {
        if (gm->IsFeatureAvailable(GraphicsManager::kTexture3d)) {
          if (is_full_image) {
            if (multisample) {
              gm->TexImage3DMultisample(
                  target, samples, pf.internal_format, image.GetWidth(),
                  image.GetHeight(), image.GetDepth(), fixed_sample_locations);
            } else {
              gm->TexImage3D(target, level, pf.internal_format,
                             image.GetWidth(), image.GetHeight(),
                             image.GetDepth(), 0, pf.format, pf.type, data);
            }
          } else {
            gm->TexSubImage3D(target, level, offset[0], offset[1], offset[2],
                              image.GetWidth(), image.GetHeight(),
                              image.GetDepth(), pf.format, pf.type, data);
          }
        } else {
          LOG(ERROR) << "***ION: 3D texturing is not supported by the local "
                     << "OpenGL implementation, but Texture \""
                     << GetTexture<TextureBase>().GetLabel()
                     << "\" contains a 3D Image.";
        }
      }
    }
  }
  if (data)
    container->WipeData();
}

void Renderer::TextureResource::UpdateMipmapGeneration(const Sampler& sampler,
                                                       bool image_has_changed,
                                                       GraphicsManager* gm) {
  const bool is_auto_mipmapping_enabled =
      sampler.IsAutogenerateMipmapsEnabled();
  const bool auto_mipmapping_changed =
      auto_mipmapping_enabled_ != is_auto_mipmapping_enabled;
  if (auto_mipmapping_changed)
    auto_mipmapping_enabled_ = is_auto_mipmapping_enabled;
  if ((image_has_changed || auto_mipmapping_changed ||
       TestModifiedBit(TextureBase::kContentsImplicitlyChanged)) &&
      auto_mipmapping_enabled_)
    gm->GenerateMipmap(gl_target_);
}

void Renderer::TextureResource::UpdateState(const TextureBase& texture,
                                            ResourceBinder* rb, GLuint unit) {
  if (id_ && !AnyModifiedBitsSet())
    return;

  // Generate the texture object.
  GraphicsManager* gm = GetGraphicsManager();
  DCHECK(gm);
  SCOPED_RESOURCE_LABEL;
  if (!id_)
    gm->GenTextures(1, &id_);
  if (id_) {
    const bool multisample = (texture.GetMultisampleSamples() > 0) &&
        gm->IsFeatureAvailable(GraphicsManager::kTextureMultisample);

    UpdateTextureTarget(gm, multisample);

    // If the resource was changed elsewhere then we need to ensure that it is
    // bound again.
    if (TestModifiedBit(ResourceHolder::kResourceChanged)) {
      rb->ClearTextureBinding(id_, unit);
      unit = rb->ObtainImageUnit(this, unit);
    }

    // Always bind the texture since we are modifying state.
    rb->ActivateUnit(unit);
    rb->BindTextureToUnit(this, unit);

    // Enable or disable multisampling state on the texture, check if its
    // state has changed.
    const bool multisample_changed =
        SetMultisampleEnabledByRenderer(multisample);
    // If the texture is immutable, call TexStorage?D().
    if ((multisample_changed ||
         TestModifiedBit(TextureBase::kImmutableImageChanged)) &&
        gm->IsFeatureAvailable(GraphicsManager::kTextureStorage)) {
      if (Image* image = texture.GetImmutableImage().Get()) {
        CreateImmutableTexture(*image, multisample,
                               texture.GetMultisampleSamples(),
                               texture.IsMultisampleFixedSampleLocations(),
                               texture.GetImmutableLevels(), gm);
      }
    }

    if (texture.GetTextureType() == TextureBase::kCubeMapTexture)
      UpdateCubeMapImageState(gm);
    else
      UpdateTextureImageState(gm, multisample, multisample_changed);
    UpdateMemoryUsage(texture.GetTextureType());
    if (TestModifiedBit(TextureBase::kSamplerChanged) &&
        !GetGraphicsManager()->IsFeatureAvailable(
            GraphicsManager::kSamplerObjects))
      if (Sampler* sampler = GetTexture<TextureBase>().GetSampler().Get())
        UpdateSamplerState(*sampler, gm);
    UpdateTextureState(texture, gm);
    SetObjectLabel(gm, GL_TEXTURE, id_, texture.GetLabel());
    ResetModifiedBits();
  } else {
    LOG(ERROR) << "***ION: Unable to create texture object";
  }
}

void Renderer::TextureResource::UpdateSamplerState(const Sampler& sampler,
                                                   GraphicsManager* gm) {
  // Update the texture state from the sampler and set it directly.
  if (max_anisotropy_ != sampler.GetMaxAnisotropy() &&
      gm->IsExtensionSupported("texture_filter_anisotropic")) {
    max_anisotropy_ = sampler.GetMaxAnisotropy();
    const float aniso = std::min(
        max_anisotropy_, GetGraphicsManager()->GetConstant<float>(
                             GraphicsManager::kMaxTextureMaxAnisotropy));
    gm->TexParameterf(gl_target_, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
  }
  if (min_filter_ != sampler.GetMinFilter()) {
    min_filter_ = sampler.GetMinFilter();
    gm->TexParameteri(gl_target_, GL_TEXTURE_MIN_FILTER,
                      base::EnumHelper::GetConstant(min_filter_));
  }
  if (mag_filter_ != sampler.GetMagFilter()) {
    mag_filter_ = sampler.GetMagFilter();
    gm->TexParameteri(gl_target_, GL_TEXTURE_MAG_FILTER,
                      base::EnumHelper::GetConstant(mag_filter_));
  }
  if (wrap_s_ != sampler.GetWrapS()) {
    wrap_s_ = sampler.GetWrapS();
    gm->TexParameteri(gl_target_, GL_TEXTURE_WRAP_S,
                      base::EnumHelper::GetConstant(wrap_s_));
  }
  if (wrap_t_ != sampler.GetWrapT()) {
    wrap_t_ = sampler.GetWrapT();
    gm->TexParameteri(gl_target_, GL_TEXTURE_WRAP_T,
                      base::EnumHelper::GetConstant(wrap_t_));
  }

  if (gm->IsFeatureAvailable(GraphicsManager::kShadowSamplers)) {
    if (compare_function_ != sampler.GetCompareFunction()) {
      compare_function_ = sampler.GetCompareFunction();
      gm->TexParameteri(gl_target_, GL_TEXTURE_COMPARE_FUNC,
                        base::EnumHelper::GetConstant(compare_function_));
    }
    if (compare_mode_ != sampler.GetCompareMode()) {
      compare_mode_ = sampler.GetCompareMode();
      gm->TexParameteri(gl_target_, GL_TEXTURE_COMPARE_MODE,
                        compare_mode_ == Sampler::kCompareToTexture
                            ? GL_COMPARE_REF_TO_TEXTURE
                            : GL_NONE);
    }
  }
  if (gm->IsFeatureAvailable(GraphicsManager::kTextureLod)) {
    if (max_lod_ != sampler.GetMaxLod()) {
      max_lod_ = sampler.GetMaxLod();
      gm->TexParameterf(gl_target_, GL_TEXTURE_MAX_LOD, max_lod_);
    }
    if (min_lod_ != sampler.GetMinLod()) {
      min_lod_ = sampler.GetMinLod();
      gm->TexParameterf(gl_target_, GL_TEXTURE_MIN_LOD, min_lod_);
    }
  }
  if (gm->IsFeatureAvailable(GraphicsManager::kTexture3d)) {
    // Do not set this if the texture is 2D.
    if (GetDimensions() == 3 && wrap_r_ != sampler.GetWrapR()) {
      wrap_r_ = sampler.GetWrapR();
      gm->TexParameteri(gl_target_, GL_TEXTURE_WRAP_R,
                        base::EnumHelper::GetConstant(wrap_r_));
    }
  }
}

void Renderer::TextureResource::UpdateTextureState(const TextureBase& texture,
                                                   GraphicsManager* gm) {
  if (gm->IsFeatureAvailable(GraphicsManager::kTextureMipmapRange)) {
    if (TestModifiedBit(TextureBase::kBaseLevelChanged))
      gm->TexParameteri(gl_target_, GL_TEXTURE_BASE_LEVEL,
                        texture.GetBaseLevel());
    if (TestModifiedBit(TextureBase::kMaxLevelChanged))
      gm->TexParameteri(gl_target_, GL_TEXTURE_MAX_LEVEL,
                        texture.GetMaxLevel());
  } else if (TestModifiedBitRange(TextureBase::kBaseLevelChanged,
                                  TextureBase::kMaxLevelChanged) &&
             (texture.GetBaseLevel() != 0 ||
              texture.GetMaxLevel() != 1000)) {
    LOG(WARNING) << "***ION: OpenGL implementation does not support "
        "setting texture mipmap ranges, they will be ignored.";
  }
  if (gm->IsFeatureAvailable(GraphicsManager::kTextureSwizzle)) {
    if (TestModifiedBit(TextureBase::kSwizzleRedChanged))
      gm->TexParameteri(
          gl_target_, GL_TEXTURE_SWIZZLE_R,
          base::EnumHelper::GetConstant(texture.GetSwizzleRed()));
    if (TestModifiedBit(TextureBase::kSwizzleGreenChanged))
      gm->TexParameteri(
          gl_target_, GL_TEXTURE_SWIZZLE_G,
          base::EnumHelper::GetConstant(texture.GetSwizzleGreen()));
    if (TestModifiedBit(TextureBase::kSwizzleBlueChanged))
      gm->TexParameteri(
          gl_target_, GL_TEXTURE_SWIZZLE_B,
          base::EnumHelper::GetConstant(texture.GetSwizzleBlue()));
    if (TestModifiedBit(TextureBase::kSwizzleAlphaChanged))
      gm->TexParameteri(
          gl_target_, GL_TEXTURE_SWIZZLE_A,
          base::EnumHelper::GetConstant(texture.GetSwizzleAlpha()));
  } else {
    if (TestModifiedBitRange(TextureBase::kSwizzleRedChanged,
                             TextureBase::kSwizzleAlphaChanged) &&
        (texture.GetSwizzleRed() != TextureBase::kRed ||
         texture.GetSwizzleGreen() != TextureBase::kGreen ||
         texture.GetSwizzleBlue() != TextureBase::kBlue ||
         texture.GetSwizzleAlpha() != TextureBase::kAlpha)) {
      LOG(ERROR) << "***ION: OpenGL implementation does not support texture "
          "swizzles, they will be ignored.";
    }
  }
}

void Renderer::TextureResource::Unbind(ResourceBinder* rb) {
  // If the texture is currently bound to any unit then we need to update our
  // internal state.
  if (rb) {
    rb->ClearTextureBindings(id_);
  }
}

void Renderer::TextureResource::Release(bool can_make_gl_calls) {
  BaseResourceType::Release(can_make_gl_calls);
  if (id_) {
    // Unbind all bindings and remove key from unit mapping.
    base::ReadLock read_lock(GetResourceBinderLock());
    base::ReadGuard read_guard(&read_lock);
    ResourceBinderMap& binders = GetResourceBinderMap();
    for (ResourceBinderMap::iterator it = binders.begin();
         it != binders.end();
         ++it) {
      Unbind(it->second.get());
      it->second->ClearAssignedImageUnit(this);
    }

    if (resource_owns_gl_id_ && can_make_gl_calls)
      GetGraphicsManager()->DeleteTextures(1, &id_);
    SetUsedGpuMemory(0U);
    id_ = 0;
  }
}

bool Renderer::TextureResource::IsComplete() const {
  // Check that the texture has a sampler.
  const TextureBase& base = GetTexture<TextureBase>();
  if (!base.GetSampler().Get()) {
    LOG(WARNING) << "***ION: Texture \""
                 << GetTexture<TextureBase>().GetLabel()
                 << "\" has no Sampler! It will likely appear black.";
    return false;
  }

  // A texture is complete if it is an immutable type.
  if (base.GetImmutableImage().Get())
    return true;

  if (base.GetTextureType() == TextureBase::kCubeMapTexture) {
    const CubeMapTexture& texture = GetTexture<CubeMapTexture>();
    // Check that the cubemap is complete. All faces must either have an image
    // or all must have mipmaps.
    bool valid = true;
    for (int i = 0; i < 6; ++i) {
      const CubeMapTexture::CubeFace face =
          static_cast<CubeMapTexture::CubeFace>(i);
      if (!texture.HasImage(face, 0U)) {
        LOG(WARNING) << "***ION: Cubemap texture face "
                     << base::EnumHelper::GetString(face)
                     << " has no level 0 mipmap.";
        return false;
      }
    }
    return valid;
  } else {
    const Texture& texture = GetTexture<Texture>();
    if (!texture.HasImage(0U)) {
      LOG(WARNING) << "***ION: Texture \"" << texture.GetLabel()
                   << "\" has no level 0 mipmap";
      return false;
    } else {
      return true;
    }
  }
}

bool Renderer::TextureResource::UpdateMipmap0Image(
    const Image& image, const TextureBase& texture, const size_t mipmap_count,
    GLenum target, int mipmap_changed_bit, GraphicsManager* gm,
    size_t* required_levels, bool multisample_changed) {
  const uint32 width = image.GetWidth();
  const uint32 height = image.GetHeight();

  const bool mipmap_changed = TestModifiedBit(mipmap_changed_bit);

  // Update the 0th level image if necessary.
  if ((mipmap_changed || multisample_changed) && CheckImage(image, texture)) {
    const int samples = texture.GetMultisampleSamples();
    const bool fixed_sample_locations =
        texture.IsMultisampleFixedSampleLocations();
    UploadImage(image, target, 0, samples, fixed_sample_locations, true,
                Point3ui(), gm);
  }

  // The number of levels (including the 0th level) required for a full
  // image pyramid.
  *required_levels = std::max(math::Log2(width), math::Log2(height)) + 1;
  // Return whether the caller needs to generate all mipmaps for the texture to
  // fill any gaps. This is true if any mipmap other than the 0th level has been
  // set and not all of the mipmaps were supplied.
  return mipmap_count < *required_levels && mipmap_count > 1U;
}

bool Renderer::TextureResource::UpdateImage(const Image& image0,
                                            const Image& mipmap,
                                            const TextureBase& texture,
                                            GLenum target,
                                            int level,
                                            GraphicsManager* gm) {
  // Validate format and dimensions.
  const Image::Format& format = image0.GetFormat();
  uint32 expected_width = 0;
  uint32 expected_height = 0;
  if (mipmap.GetFormat() != format) {
    LOG(ERROR) << "***ION: Mipmap level " << level << " has different"
               << " format [" << mipmap.GetFormat() << "] from level 0's ["
               << format << "], ignoring";
  } else if (Texture::ExpectedDimensionsForMipmap(mipmap.GetWidth(),
                                                  mipmap.GetHeight(),
                                                  level,
                                                  image0.GetWidth(),
                                                  image0.GetHeight(),
                                                  &expected_width,
                                                  &expected_height)) {
    // We can assume not multisampling.
    UploadImage(mipmap, target, level, 0, false, true, Point3ui(), gm);
    return true;
  }
  return false;
}

void Renderer::TextureResource::UpdateSubImages(
    const base::AllocVector<Texture::SubImage>& images, GLenum target,
    GraphicsManager* gm) {
  const size_t count = images.size();
  for (size_t i = 0; i < count; ++i) {
    const Image& image = *images[i].image.Get();
    // If the image is an array type then we're setting a slice and must use its
    // target.
    if (image.GetType() == Image::kArray)
      target = gl_target_;
    // We can assume not multisampling.
    UploadImage(image, target, static_cast<GLint>(images[i].level), 0, false,
                false, images[i].offset, gm);
  }
}

void Renderer::TextureResource::UpdateMemoryUsage(
    TextureBase::TextureType type) {
  size_t data_size = 0U;
  if (type == TextureBase::kTexture) {
    const Texture& tex = GetTexture<Texture>();
    if (tex.HasImage(0)) {
      const Image& image = *tex.GetImage(0);
      const Sampler* samp = tex.GetSampler().Get();
      const bool auto_mipmap = samp && samp->IsAutogenerateMipmapsEnabled();
      if (tex.GetImageCount() > 1 || auto_mipmap) {
        data_size = Image::ComputeDataSize(image.GetFormat(), image.GetWidth(),
                                           image.GetHeight());
        // Account for the mipmap hierarchy, where we have a geometric series:
        // x + x/4 + x/16 + x/64 + ... = x / (1 - 1/4) = 4 x / 3.
        data_size = (data_size * 4) / 3;
      } else {
        data_size = Image::ComputeDataSize(image.GetFormat(), image.GetWidth(),
                                                 image.GetHeight());
      }
    }
  } else {  // kCubeMapTexture.
    const CubeMapTexture& tex = GetTexture<CubeMapTexture>();
    if (tex.HasImage(CubeMapTexture::kNegativeX, 0)) {
      const Image& image = *tex.GetImage(CubeMapTexture::kNegativeX, 0);
      const Sampler* samp = tex.GetSampler().Get();
      const bool auto_mipmap = samp && samp->IsAutogenerateMipmapsEnabled();
      if (tex.GetImageCount(CubeMapTexture::kNegativeX) > 1 || auto_mipmap) {
        data_size = Image::ComputeDataSize(image.GetFormat(), image.GetWidth(),
                                           image.GetHeight());
        // Account for all faces and mipmaps.
        data_size = data_size * 8;
      } else {
        data_size = Image::ComputeDataSize(image.GetFormat(), image.GetWidth(),
                                           image.GetHeight());
        data_size *= 6;
      }
    }
  }

  SetUsedGpuMemory(data_size);
}

void Renderer::TextureResource::UpdateTextureImageState(
    GraphicsManager* gm, const bool multisample,
    const bool multisample_changed) {
  const Texture& texture = GetTexture<Texture>();
  const bool mipmap_changed = TestModifiedBitRange(
      Texture::kMipmapChanged, Texture::kMipmapChanged + kMipmapSlotCount);

  if ((mipmap_changed || multisample_changed) && texture.HasImage(0) &&
      !texture.GetImmutableImage().Get()) {
    if (multisample) {
      const Image& image0 = *texture.GetImage(0);
      size_t required_levels = 0U;
      UpdateMipmap0Image(image0, texture, texture.GetImageCount(), gl_target_,
                         Texture::kMipmapChanged, gm, &required_levels,
                         multisample_changed);
    } else {
      // Update the mipmap levels that have changed, and update if multisample
      // has changed.
      const Image& image0 = *texture.GetImage(0);
      size_t required_levels = 0U;
      const bool generate_mipmaps = UpdateMipmap0Image(
          image0, texture, texture.GetImageCount(), gl_target_,
          Texture::kMipmapChanged, gm, &required_levels, multisample_changed);
      if (generate_mipmaps || multisample_changed)
        gm->GenerateMipmap(gl_target_);

      for (size_t i = 1; i < required_levels; ++i) {
        if (texture.HasImage(i) &&
            CheckImage(*texture.GetImage(i), texture) &&
            (generate_mipmaps || multisample_changed ||
                TestModifiedBit(static_cast<Texture::Changes>(
                    Texture::kMipmapChanged + i)))) {
          UpdateImage(image0, *texture.GetImage(i), texture, gl_target_,
                      static_cast<int>(i), gm);
        }
      }
    }
  }

  if (!multisample) {
    if (multisample_changed || TestModifiedBit(Texture::kSubImageChanged)) {
      UpdateSubImages(texture.GetSubImages(), gl_target_, gm);
      texture.ClearSubImages();
    }

    // Generate mipmaps if requested and not using client-supplied mipmaps.
    if (Sampler* sampler = texture.GetSampler().Get()) {
      if (texture.HasImage(0U))
        UpdateMipmapGeneration(*sampler, multisample_changed ||
                               TestModifiedBit(Texture::kMipmapChanged), gm);
    }
  }
}

void Renderer::TextureResource::CreateImmutableTexture(
    const Image& image, const bool multisample, const size_t samples,
    const bool fixed_sample_locations, size_t levels, GraphicsManager* gm) {
  Image::PixelFormat pf =
      GetCompatiblePixelFormat(Image::GetPixelFormat(image.GetFormat()), gm);

  // Protection must be set before the texture is specified.  See:
  // https://www.khronos.org/registry/gles/extensions/EXT/EXT_protected_textures.txt
  if (GetTexture<TextureBase>().IsProtected()) {
    if (gm->IsFeatureAvailable(GraphicsManager::kProtectedTextures)) {
      gm->TexParameteri(gl_target_, GL_TEXTURE_PROTECTED_EXT, GL_TRUE);
    } else {
      LOG(WARNING) << "***ION: Texture '"
                   << GetTexture<TextureBase>().GetLabel()
                   << "' requests a protected texture, but the system does not "
                      "support protected textures. This may result in a black "
                      "or green screen, or just garbage on the screen.";
    }
  }

  if (image.GetDimensions() == Image::k2d) {
    if (multisample) {
      gm->TexStorage2DMultisample(gl_target_, static_cast<GLsizei>(samples),
                                  pf.internal_format, image.GetWidth(),
                                  image.GetHeight(),
                                  static_cast<GLboolean>(
                                      fixed_sample_locations));

    } else {
      gm->TexStorage2D(gl_target_, static_cast<GLsizei>(levels),
                       pf.internal_format, image.GetWidth(),
                       image.GetHeight());
    }
  } else if (image.GetDimensions() == Image::k3d) {
    if (multisample) {
      gm->TexStorage3DMultisample(gl_target_, static_cast<GLsizei>(samples),
                                  pf.internal_format, image.GetWidth(),
                                  image.GetHeight(), image.GetDepth(),
                                  static_cast<GLboolean>(
                                      fixed_sample_locations));
    } else {
      gm->TexStorage3D(gl_target_, static_cast<GLsizei>(levels),
                       pf.internal_format, image.GetWidth(), image.GetHeight(),
                       image.GetDepth());
    }
  }
}

void Renderer::TextureResource::UpdateCubeMapImageState(GraphicsManager* gm) {
  // Note that this function is only entered if the cubemap is complete, meaning
  // that all faces have images or mipmaps.
  const CubeMapTexture& texture = GetTexture<CubeMapTexture>();
  static const int kSlotCount = static_cast<int>(kMipmapSlotCount);

  // Update the mipmap levels that have changed.
  bool images_have_changed = false;
  bool need_to_generate_mipmaps = false;
  size_t required_levels[6];
  // Don't try to upload face images if we are using immutable images.
  if (!texture.GetImmutableImage().Get()) {
    for (int i = 0; i < 6; ++i) {
      const CubeMapTexture::CubeFace face =
          static_cast<CubeMapTexture::CubeFace>(i);
      const int base_mipmap_bit =
          CubeMapTexture::kNegativeXMipmapChanged + i * kSlotCount;
      required_levels[i] = 0U;
      if (TestModifiedBitRange(base_mipmap_bit, base_mipmap_bit + kSlotCount) &&
          texture.HasImage(face, 0U)) {
        const Image& image = *texture.GetImage(face, 0U);
        // Cube map arrays must be specified with the array target, not the
        // face.
        const GLenum target = image.GetDimensions() == Image::k3d
                                  ? gl_target_
                                  : base::EnumHelper::GetConstant(face);
        if (image.GetWidth() == image.GetHeight()) {
          if (UpdateMipmap0Image(image, texture, texture.GetImageCount(face),
                                 target, base_mipmap_bit, gm,
                                 &required_levels[i], false)) {
            need_to_generate_mipmaps = true;
            images_have_changed = true;
          }
        } else {
          LOG(ERROR) << "Level 0 mimpap for face "
                     << base::EnumHelper::GetString(face) << " of cubemap \""
                     << texture.GetLabel()
                     << "\" does not have square dimensions. OpenGL requires "
                     << "cubemap faces to have square dimensions";
        }
      }
    }
    // Generate mipmaps if any of the faces were incomplete.
    if (need_to_generate_mipmaps) gm->GenerateMipmap(gl_target_);
  }

  // Override generated mipmaps with user-supplied ones.
  for (int j = 0; j < 6; ++j) {
    const CubeMapTexture::CubeFace face =
        static_cast<CubeMapTexture::CubeFace>(j);
    const int base_subimage_bit = CubeMapTexture::kNegativeXSubImageChanged;
    const int base_mipmap_bit =
        CubeMapTexture::kNegativeXMipmapChanged + j * kSlotCount;
    if (!texture.GetImmutableImage().Get() &&
        TestModifiedBitRange(base_mipmap_bit, base_mipmap_bit + kSlotCount) &&
        texture.HasImage(face, 0U)) {
      const Image& image0 = *texture.GetImage(face, 0U);
      const GLenum target = image0.GetDimensions() == Image::k3d
                                ? gl_target_
                                : base::EnumHelper::GetConstant(face);
      for (size_t i = 1; i < required_levels[j]; ++i) {
        if (texture.HasImage(face, i) &&
            CheckImage(*texture.GetImage(face, i), texture) &&
            (need_to_generate_mipmaps ||
             TestModifiedBit(static_cast<int>(base_mipmap_bit + i)))) {
          if (!UpdateImage(image0, *texture.GetImage(face, i), texture,
                           target, static_cast<int>(i), gm)) {
            // Do not generate mipmaps since we have an invalid one.
            images_have_changed = false;
            break;
          }
        }
      }
    }

    if (TestModifiedBit(base_subimage_bit + j)) {
      const GLenum target = base::EnumHelper::GetConstant(face);
      UpdateSubImages(texture.GetSubImages(face), target, gm);
      texture.ClearSubImages(face);
    }
  }

  // Generate mipmaps if necessary.
  if (Sampler* sampler = texture.GetSampler().Get())
    UpdateMipmapGeneration(*sampler, images_have_changed, gm);
}

void Renderer::ResourceBinder::WrapExternalResource(Texture* holder,
                                                    uint32 gl_id) {
  if (graphics_manager_->IsTexture(gl_id)) {
    auto resource = GetResourceManager()->GetResource(holder, this, gl_id);
    DCHECK(resource);
    resource->ResetModifiedBits();
  }
}

//-----------------------------------------------------------------------------
//
// Renderer::ShaderResource class.
//
//-----------------------------------------------------------------------------

class Renderer::ShaderResource : public Resource<Shader::kNumChanges> {
 public:
  ShaderResource(ResourceBinder* rb, ResourceManager* rm, const Shader& shader,
                 ResourceKey key, GLuint id)
      : Renderer::Resource<Shader::kNumChanges>(rm, shader, key, id),
        shader_type_(GL_INVALID_ENUM) {}

  ~ShaderResource() override {
    DCHECK(id_ == 0U || !portgfx::GlContext::GetCurrent());
  }

  // Updates the resource and returns whether anything changed.
  virtual bool UpdateShader(ResourceBinder* rb);
  void Release(bool can_make_gl_calls) override;
  ResourceType GetType() const override { return kShader; }

  // Sets the type of shader (e.g., GL_VERTEX_SHADER, GL_FRAGMENT_SHADER).
  void SetShaderType(GLenum type) { shader_type_ = type; }

  const Shader& GetShader() const {
    return static_cast<const Shader&>(*GetHolder());
  }

  void Bind(ResourceBinder* rb) {}
  void Unbind(ResourceBinder* rb) override {}
  void Update(ResourceBinder* rb) override {}

 private:
  // The type of shader.
  GLenum shader_type_;
};

bool Renderer::ShaderResource::UpdateShader(ResourceBinder* rb) {
  if (AnyModifiedBitsSet()) {
    SCOPED_RESOURCE_LABEL;
    // For coverage.
    Update(rb);

    const Shader& shader = GetShader();
    const std::string& id_string = shader.GetLabel();
    GraphicsManager* gm = GetGraphicsManager();

    std::string info_log = shader.GetInfoLog();
    bool need_to_update_label = TestModifiedBit(ResourceHolder::kLabelChanged);
    if (TestModifiedBit(Shader::kSourceChanged)) {
      GLuint id = CompileShader(id_string, shader_type_, shader.GetSource(),
                                &info_log, gm);
      // Only update the id if the compilation was successful.
      if (id) {
        id_ = id;
        need_to_update_label = true;
      }
    }

    if (need_to_update_label)
      SetObjectLabel(gm, GL_SHADER_OBJECT_EXT, id_, id_string);

    // Send the info log to the holder.
    shader.SetInfoLog(info_log);
    ResetModifiedBits();

    return true;
  } else {
    return false;
  }
}

void Renderer::ShaderResource::Release(bool can_make_gl_calls) {
  BaseResourceType::Release(can_make_gl_calls);
  GraphicsManager* gm = GetGraphicsManager();
  if (id_) {
    if (resource_owns_gl_id_ && can_make_gl_calls)
      gm->DeleteShader(id_);
    id_ = 0;
  }
}

//-----------------------------------------------------------------------------
//
// Renderer::ShaderInputRegistryResource class.
//
// A ShaderInputRegistryResource tracks the values of uniforms belonging to one
// ShaderInputRegistry when a Node tree is traversed. It handles overriding and
// combining uniform values in child nodes. Unlike the other Resource classes,
// it does create or own any GL objects.
//
//-----------------------------------------------------------------------------

class Renderer::ShaderInputRegistryResource
    : public Resource<ShaderInputRegistry::kNumChanges> {
 public:
  ShaderInputRegistryResource(ResourceBinder* rb, ResourceManager* rm,
                              const ShaderInputRegistry& reg,
                              ResourceKey key, GLuint id)
      : Renderer::Resource<ShaderInputRegistry::kNumChanges>(rm, reg, key, id),
        uniform_stacks_(reg.GetAllocator()) {
    uniform_stacks_.reserve(reg.GetSpecs<Uniform>().size());
  }

  ~ShaderInputRegistryResource() override {}
  ResourceType GetType() const override { return kShaderInputRegistry; }

  // Registers a ShaderProgramResource as interested in changes to the
  // |index_in_registry|th uniform.
  void SetUniformCacheInfo(size_t index_in_registry,
                           ShaderProgramResource* spr,
                           size_t uniform_cache_index) {
    DCHECK_LT(index_in_registry, uniform_stacks_.size());
    uniform_stacks_[index_in_registry]->SetUniformCacheInfo(
        spr, uniform_cache_index);
  }

  size_t GetUniformCacheInfo(size_t index_in_registry,
                             ShaderProgramResource** spr) {
    DCHECK_LT(index_in_registry, uniform_stacks_.size());
    return uniform_stacks_[index_in_registry]->GetUniformCacheInfo(spr);
  }

  // Detaches spr from the |index_in_registry|th UniformStack.
  void DetachShaderProgramResource(
      ShaderProgramResource* spr, size_t index_in_registry) {
    DCHECK_LT(index_in_registry, uniform_stacks_.size());
    UniformStack& ustack = *uniform_stacks_[index_in_registry];
    ustack.DetachShaderProgramResource(spr);
  }

  void Unbind(ResourceBinder* rb) override {
    // Clear the uniform cache. Do not leave stacks in an invalid state
    // (e.g. too few stacks, invalid state of a stack), since unbinding
    // does not mark a resource as modified.
    for (auto& stack : uniform_stacks_)
      stack->Clear();
  }

  // Returns the latest uniform stored at the passed index.
  const Uniform& GetUniform(size_t index) const {
    DCHECK_LT(index, uniform_stacks_.size());
    return *uniform_stacks_[index]->GetTop();
  }

  // Sets the initial value of a Uniform. The Uniform must be valid.
  void SetInitialValue(const Uniform& u) {
    DCHECK_EQ(&u.GetRegistry(), &GetRegistry());
    DCHECK_LT(u.GetIndexInRegistry(), uniform_stacks_.size());
    UniformStack& uis = *uniform_stacks_[u.GetIndexInRegistry()];
    uis.SetBottom(u);
  }

  // Pushes a Uniform onto the shadow state.
  void PushUniform(const Uniform& u) {
    DCHECK_EQ(&u.GetRegistry(), &GetRegistry());
    DCHECK_LT(u.GetIndexInRegistry(), uniform_stacks_.size());
    DCHECK(u.IsValid());
    UniformStack& uis = *uniform_stacks_[u.GetIndexInRegistry()];
    const ShaderInputRegistry::Spec<Uniform>* spec =
        ShaderInputRegistry::GetSpec(u);
    const ShaderInputRegistry::CombineFunction<Uniform>::Type& combine_func =
        spec->combine_function;
    const ShaderInputRegistry::GenerateFunction<Uniform>::Type& generate_func =
        spec->generate_function;
    const Uniform& top = *uis.GetTop();
    // Grab a temp Uniform to potentially be used for combining or merging.
    Uniform* temp = uis.GetTopTempUniform();
    if (combine_func && top.IsValid()) {
      // Combine with the previous uniform on top of the stack.
      *temp = combine_func(top, u);
      uis.PushTempUniform(temp);
    } else if (Uniform::GetMerged(top, u, temp)) {
      // Merge with the previous uniform on top of the stack - for arrays.
      uis.PushTempUniform(temp);
    } else {
      // No need to combine or merge so this is the fast path.
      uis.Push(&u);
    }

    if (generate_func) {
      // Generate the new Uniform.
      std::vector<Uniform> generated = generate_func(*uis.GetTop());
      const size_t count = generated.size();
      for (size_t i = 0; i < count; ++i) {
        const Uniform& gen = generated[i];
        if (gen.IsValid()) {
          // Get the stack for the generated uniform.
          const size_t index_in_registry = gen.GetIndexInRegistry();
          DCHECK_EQ(&u.GetRegistry(), &GetRegistry());
          DCHECK_LT(index_in_registry, uniform_stacks_.size());
          UniformStack& uis = *uniform_stacks_[index_in_registry];
          Uniform* temp = uis.GetTopTempUniform();
          *temp = gen;
          uis.PushTempUniform(temp);
        }
      }
    }
  }
  // Pops a Uniform off of the shadow state.
  void PopUniform(const Uniform& uniform) {
    DCHECK_EQ(&uniform.GetRegistry(), &GetRegistry());
    DCHECK_LT(uniform.GetIndexInRegistry(), uniform_stacks_.size());
    const size_t index = uniform.GetIndexInRegistry();
    uniform_stacks_[index]->Pop();
  }

  void Update(ResourceBinder* rb) override {
    if (AnyModifiedBitsSet()) {
      // Ensure that we have enough space for the uniforms of the associated
      // registry.
      const size_t size = GetRegistry().GetSpecs<Uniform>().size();
      for (size_t i = uniform_stacks_.size(); i < size; ++i) {
        uniform_stacks_.push_back(std::unique_ptr<UniformStack>(
            new(uniform_stacks_.get_allocator().GetAllocator())
            UniformStack(this)));
      }
      ResetModifiedBits();
    }
  }

 private:
  // This class is needed to properly construct the stack of AllocVectors with
  // an allocator and wraps access to the stack. Note that we normally keep
  // pointers to Uniforms for performance and only copy combined or merged
  // Uniforms.
  class UniformStack : public Allocatable {
   public:
    explicit UniformStack(ShaderInputRegistryResource* sirr)
        : sirr_(sirr),
          uniform_stack_(
              base::AllocationManager::GetDefaultAllocatorForLifetime(
                  base::kShortTerm)),
          temp_stack_(base::AllocationManager::GetDefaultAllocatorForLifetime(
              base::kShortTerm)),
          pristine_(true),
          shader_program_resource_(nullptr),
          uniform_cache_index_(ion::base::kInvalidIndex) {
      Init();
    }
    ~UniformStack() override {
      SetUniformCacheInfo(nullptr, base::kInvalidIndex);
    }

    void DetachShaderProgramResource(ShaderProgramResource* spr) {
      if (shader_program_resource_ == spr)
        shader_program_resource_ = nullptr;
    }
    void SetUniformCacheInfo(ShaderProgramResource* spr,
                             size_t uniform_cache_index);
    size_t GetUniformCacheInfo(ShaderProgramResource** spr) {
      if (spr)
        *spr = shader_program_resource_;
      return uniform_cache_index_;
    }
    size_t GetUniformCacheIndex() const {
      return uniform_cache_index_;
    }
    ShaderProgramResource* GetShaderProgramResource() const {
      return shader_program_resource_;
    }
    Uniform* GetTopTempUniform() {
      DCHECK_GT(temp_stack_.size(), 1U);
      return &temp_stack_.back();
    }
    const Uniform* GetTop() const {
      DCHECK(!uniform_stack_.empty());
      return uniform_stack_.back();
    }
    void Pop() {
      DCHECK(!temp_stack_.empty());
      DCHECK(!uniform_stack_.empty());
      if (IsTopATempUniform())
        temp_stack_.pop_back();
      uniform_stack_.pop_back();
      pristine_ = false;
      NotifyUniformChanged();
    }
    void PushTempUniform(Uniform* temp) {
      DCHECK(!uniform_stack_.empty());
      DCHECK(!temp_stack_.empty());
      DCHECK(temp == GetTopTempUniform());
      // Add new temp uniform so GetTopTempUniform has a fresh one.
      temp_stack_.push_back(Uniform());
      uniform_stack_.push_back(temp);
      pristine_ = false;
      NotifyUniformChanged();
    }
    void Push(const Uniform* uniform) {
      DCHECK(!uniform_stack_.empty());
      DCHECK(uniform != GetTopTempUniform());
      uniform_stack_.push_back(uniform);
      pristine_ = false;
      NotifyUniformChanged();
    }
    void SetBottom(const Uniform& uniform) {
      DCHECK(!uniform_stack_.empty());
      DCHECK(!temp_stack_.empty());
      temp_stack_[0] = uniform;
      uniform_stack_[0] = &temp_stack_[0];
      pristine_ = false;
      NotifyUniformChanged();
    }
    void Clear() {
      if (!pristine_) {
        uniform_stack_.clear();
        temp_stack_.clear();
        Init();
        pristine_ = true;
      }
    }
    size_t Size() const { return uniform_stack_.size(); }

   private:
    void Init() {
      // There needs to be at least one entry in the stack for SetBottom().
      temp_stack_.push_back(Uniform());
      uniform_stack_.push_back(&temp_stack_.back());
      // Add another temp Uniform for GetTopTempUniform().
      temp_stack_.push_back(Uniform());
    }
    bool IsTopATempUniform() {
      // Check [size - 2] because [size - 1] is an extra used for
      // GetTopTempUniform().
      const size_t size = temp_stack_.size();
      return (size > 1U &&
              uniform_stack_.back() == &temp_stack_[size - 2]);
    }
    void NotifyUniformChanged();

    ShaderInputRegistryResource* sirr_;
    base::AllocVector<const Uniform*> uniform_stack_;
    // Temporary storage needed to hold Uniforms that have been combined or
    // merged during traversal. Otherwise we just keep pointers to Uniforms to
    // avoid expensive copying. Use a deque-like UniformStack to ensure that
    // pointers to Uniforms in the deque remain valid.
    base::AllocDeque<Uniform> temp_stack_;
    // Whether the stack is in default state.
    bool pristine_;
    // ShaderProgramResource which is notified via OnUniformChanged whenever
    // the uniform changes in the stack.
    ShaderProgramResource* shader_program_resource_;
    // Index in shader_program_resource_'s UniformCacheEntry vector.
    size_t uniform_cache_index_;
  };

  const ShaderInputRegistry& GetRegistry() const {
    return static_cast<const ShaderInputRegistry&>(*GetHolder());
  }

  base::AllocVector<std::unique_ptr<UniformStack>> uniform_stacks_;
};

//-----------------------------------------------------------------------------
//
// Renderer::ShaderProgramResource class.
//
//-----------------------------------------------------------------------------

class Renderer::ShaderProgramResource
    : public Resource<ShaderProgram::kNumChanges> {
 public:
  ShaderProgramResource(ResourceBinder* rb, ResourceManager* rm,
                        const ShaderProgram& shader_program, ResourceKey key,
                        GLuint id)
      : Renderer::Resource<ShaderProgram::kNumChanges>(rm, shader_program, key,
                                                       id),
        attribute_index_map_(shader_program.GetAllocator()),
        uniforms_(shader_program.GetAllocator()),
        uniform_changes_(shader_program.GetAllocator()),
        uniform_change_stamp_(0U),
        vertex_resource_(nullptr),
        geometry_resource_(nullptr),
        fragment_resource_(nullptr),
        tess_ctrl_resource_(nullptr),
        tess_eval_resource_(nullptr) {
    uniforms_.reserve(8);
    uniform_changes_.reserve(8);
  }

  GLint GetAttributeIndex(
      const ShaderInputRegistry::AttributeSpec* spec) const {
    AttributeIndexMap::const_iterator it = attribute_index_map_.find(spec);
    return it == attribute_index_map_.end() ? -1 : it->second;
  }

  ~ShaderProgramResource() override {
    DCHECK(id_ == 0U || !portgfx::GlContext::GetCurrent());
  }


  void OnUniformChanged(ShaderInputRegistryResource* sirr,
                        size_t uniform_cache_index) {
    DCHECK_GT(uniforms_.size(), uniform_cache_index);
    UniformCacheEntry& entry = uniforms_[uniform_cache_index];
    DCHECK_EQ(sirr, entry.sirr);
    // Avoid multiple redundant changes by checking change stamp.
    if (entry.change_stamp != uniform_change_stamp_) {
      entry.change_stamp = uniform_change_stamp_;
      uniform_changes_.push_back(uniform_cache_index);
    }
  }

  void DetachShaderInputRegistryResource(ShaderInputRegistryResource* sirr,
                                         size_t uniform_cache_index) {
    DCHECK_GT(uniforms_.size(), uniform_cache_index);
    if (uniforms_[uniform_cache_index].sirr == sirr)
      uniforms_[uniform_cache_index].sirr = nullptr;
  }

  void Release(bool can_make_gl_calls) override;
  void Update(ResourceBinder* rb) override;
  ResourceType GetType() const override { return kShaderProgram; }

  // Obtain/release image units for all texture uniforms used by this shader
  // program.
  void ObtainImageUnits(ResourceBinder* rb);
  void ReleaseImageUnits(ResourceBinder* rb);

  // Binds or unbinds the ShaderProgram object in OpenGL.
  bool Bind(ResourceBinder* rb);
  void Unbind(ResourceBinder* rb) override;

  const ShaderProgram& GetShaderProgram() const {
    return static_cast<const ShaderProgram&>(*GetHolder());
  }

  void OnShaderInputRegistryResourceDestroyed(
      ShaderInputRegistryResource* sirr) {
    for (auto& entry : uniforms_) {
      if (entry.sirr == sirr)
        entry.sirr = nullptr;
    }
  }

  // Return the resources of the shader stages.
  ShaderResource* GetVertexResource() const { return vertex_resource_; }
  ShaderResource* GetGeometryResource() const { return geometry_resource_; }
  ShaderResource* GetFragmentResource() const { return fragment_resource_; }
  ShaderResource* GetTessControlResource() const { return tess_ctrl_resource_; }
  ShaderResource* GetTessEvaluationResource() const {
    return tess_eval_resource_;
  }

 private:
  struct UniformCacheEntry {
    UniformCacheEntry()
        : spr(nullptr),
          location(-1),
          registry(nullptr),
          index(base::kInvalidIndex),
          sirr(nullptr),
          uniform_stamp(base::kInvalidIndex),
          change_stamp(base::kInvalidIndex),
          unit_associations(
              base::AllocationManager::GetDefaultAllocatorForLifetime(
                  base::kMediumTerm)),
          unit_association(-1),
          is_texture(false) {
    }

    UniformCacheEntry(ShaderProgramResource* spr_in,
                      GLint location_in, GLint array_size,
                      const ShaderInputRegistry::UniformSpec* spec_in)
        : spr(spr_in),
          location(location_in),
          registry(spec_in->registry),
          index(spec_in->index),
          sirr(nullptr),
          uniform_stamp(base::kInvalidIndex),
          change_stamp(base::kInvalidIndex),
          unit_associations(
              base::AllocationManager::GetDefaultAllocatorForLifetime(
                  base::kMediumTerm)),
          unit_association(-1),
          is_texture(spec_in->value_type == kTextureUniform ||
                     spec_in->value_type == kCubeMapTextureUniform) {
      if (is_texture && array_size > 1)
        unit_associations.resize(array_size, -1);
    }

    ~UniformCacheEntry() {
      if (sirr)
        sirr->DetachShaderProgramResource(spr, index);
    }

    bool UpdateUnitAssociations(ResourceBinder* rb) {
      if (!is_texture)
        return false;
      bool changed = false;
      if (unit_associations.empty()) {
        // We have a single texture.
        const int new_unit = rb->ObtainImageUnit(nullptr, unit_association);
        changed = new_unit != unit_association;
        unit_association = new_unit;
        rb->UseImageUnit(new_unit, nullptr);
      } else {
        // We have an array of textures.
        for (auto& unit : unit_associations) {
          const int new_unit = rb->ObtainImageUnit(nullptr, unit);
          changed |= new_unit != unit;
          unit = new_unit;
          rb->UseImageUnit(new_unit, nullptr);
        }
      }
      return changed;
    }

    ShaderProgramResource* spr;
    GLint location;  // If location is -1 then the value is not valid.
    ShaderInputRegistry* registry;
    size_t index;
    ShaderInputRegistryResource* sirr;
    uint64 uniform_stamp;
    uint64 change_stamp;
    // The last units sent for texture uniforms.
    base::AllocVector<GLint> unit_associations;
    GLint unit_association;
    bool is_texture;
  };

  typedef base::AllocUnorderedMap<const ShaderInputRegistry::AttributeSpec*,
                                  GLint> AttributeIndexMap;

  // Gets the active attributes for the shader and sets up their indices. If any
  // attributes are missing from the registry warning messages are logged.
  void PopulateAttributeCache(GLuint id, const std::string& id_string,
                              const ShaderInputRegistryPtr& reg,
                              GraphicsManager* gm);

  // Gets the active uniforms for this shader from OpenGL and sets up their
  // uniform locations in the cache. If any uniforms are missing from the
  // registry warning messages are logged.
  void PopulateUniformCache();

  // Gets the latest uniform values from the resource binder's cache and obtain
  // image units for all texture uniforms used by the shader program.
  void UpdateUniformValuesAndImageUnits(ResourceBinder* rb);

  void UpdateChangedUniformValues(ResourceBinder* rb);
  // Gets the latest uniform values from the resource binder's cache.
  void UpdateUniformValue(GraphicsManager* gm,
                          ResourceBinder* rb,
                          const Uniform& uniform,
                          UniformCacheEntry* entry);

  // Binds texture(s) in u to entry's image unit(s).
  template <typename HolderType>
  void BindTextures(
      const Uniform& u, const UniformCacheEntry& entry, ResourceBinder* rb);

  // Returns whether the Uniform holds a texture type and if any of the textures
  // it contains have been evicted.
  template <typename HolderType>
  bool ContainsAnEvictedTexture(const Uniform& uniform, ResourceBinder* rb);

  // Indices of the attributes in the shader.
  AttributeIndexMap attribute_index_map_;

  // Vector of uniforms that this program uses.
  base::AllocVector<UniformCacheEntry> uniforms_;

  // List of uniform changes that need to be sent to GL on the next draw.
  base::AllocVector<size_t> uniform_changes_;
  uint64 uniform_change_stamp_;

  // Shader stage resources.
  ShaderResource* vertex_resource_;
  ShaderResource* geometry_resource_;
  ShaderResource* fragment_resource_;
  ShaderResource* tess_ctrl_resource_;
  ShaderResource* tess_eval_resource_;
};

void Renderer::ShaderInputRegistryResource::
UniformStack::SetUniformCacheInfo(
    ShaderProgramResource* spr, size_t uniform_cache_index) {
  if (shader_program_resource_ != spr ||
      uniform_cache_index_ != uniform_cache_index) {
    if (shader_program_resource_) {
      shader_program_resource_->DetachShaderInputRegistryResource(
          sirr_, uniform_cache_index_);
    }
    shader_program_resource_ = spr;
    uniform_cache_index_ = uniform_cache_index;
  }
}

inline void
Renderer::ShaderInputRegistryResource::UniformStack::NotifyUniformChanged() {
  if (shader_program_resource_) {
    DCHECK_NE(uniform_cache_index_, base::kInvalidIndex);
    shader_program_resource_->OnUniformChanged(
        sirr_, uniform_cache_index_);
  }
}

void Renderer::ShaderProgramResource::PopulateAttributeCache(
    GLuint id, const std::string& id_string, const ShaderInputRegistryPtr& reg,
    GraphicsManager* gm) {
  GLint max_length = 0;
  GLint attribute_count = 0;
  // Ask OpenGL for the number of attributes.
  gm->GetProgramiv(id, GL_ACTIVE_ATTRIBUTES, &attribute_count);
  if (attribute_count) {
    attribute_index_map_.clear();

    gm->GetProgramiv(id, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_length);
    static const GLint kMaxNameLength = 4096;
    char name[kMaxNameLength];
    // Some platforms (asm.js) return zero for the max length.
    max_length =
        std::min(kMaxNameLength, max_length == 0 ? kMaxNameLength : max_length);

    // Cache all attribute indices. OpenGL prefers to have buffer attributes be
    // first, so we order them accordingly here.
    GLsizei length;
    GLint size;
    GLuint type;
    const base::AllocatorPtr& allocator =
        GetShaderProgram().GetAllocator()->GetAllocatorForLifetime(
            base::kShortTerm);
    base::AllocVector<GLenum> buffer_types(allocator);
    base::AllocVector<GLenum> simple_types(allocator);
    base::AllocVector<const ShaderInputRegistry::AttributeSpec*>
        buffer_attributes(allocator);
    base::AllocVector<const ShaderInputRegistry::AttributeSpec*>
        simple_attributes(allocator);
    for (GLint i = 0; i < attribute_count; ++i) {
      // Get the attribute information from OpenGL.
      name[0] = 0;
      gm->GetActiveAttrib(id, i, max_length, &length, &size, &type, name);
      if (const ShaderInputRegistry::AttributeSpec* spec =
              reg->Find<Attribute>(name)) {
        if (spec->value_type == kBufferObjectElementAttribute) {
          buffer_attributes.push_back(spec);
          buffer_types.push_back(type);
        } else {
          simple_attributes.push_back(spec);
          simple_types.push_back(type);
        }
      } else {
        // The registry does not define this attribute.
        // gl_InstanceID is reported as an attribute by the Nvidia driver
        // if used in the shader; do not generate a warning for it.
        if (strcmp(name, "gl_InstanceID") != 0) {
          LOG(WARNING) << "***ION: Attribute '" << name << "' used in shader '"
                       << GetShaderProgram().GetLabel()
                       << "' does not have a registry entry";
        }
      }
    }

    // Bind the attributes to indices, buffer attributes first.
    GLuint attribute_index = 0;
    GLuint count = static_cast<GLuint>(buffer_attributes.size());
    for (GLuint i = 0; i < count; ++i) {
      attribute_index_map_[buffer_attributes[i]] =
          static_cast<GLint>(attribute_index);
      gm->BindAttribLocation(id, attribute_index,
                             buffer_attributes[i]->name.c_str());
      attribute_index += GetAttributeSlotCountByGlType(buffer_types[i]);
    }
    count = static_cast<GLuint>(simple_attributes.size());
    for (GLuint i = 0; i < count; ++i) {
      attribute_index_map_[simple_attributes[i]] =
          static_cast<GLint>(attribute_index);
      gm->BindAttribLocation(id, attribute_index,
                             simple_attributes[i]->name.c_str());
      attribute_index += GetAttributeSlotCountByGlType(simple_types[i]);
    }
  }
}

void Renderer::ShaderProgramResource::PopulateUniformCache() {
  const ShaderProgram& shader_program = GetShaderProgram();
  const ShaderInputRegistryPtr& reg = shader_program.GetRegistry();
  GraphicsManager* gm = GetGraphicsManager();

  GLint max_length = 0;
  GLuint uniform_count = 0;
  // Ask OpenGL for the number of uniforms.
  gm->GetProgramiv(id_, GL_ACTIVE_UNIFORMS,
                   reinterpret_cast<GLint*>(&uniform_count));
  uniforms_.clear();
  if (uniform_count) {
    gm->GetProgramiv(id_, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_length);
    static const GLint kMaxNameLength = 4096;
    char name[kMaxNameLength];
    // Some platforms (asm.js) return zero for the max length.
    max_length =
        std::min(kMaxNameLength, max_length == 0 ? kMaxNameLength : max_length);
    GLsizei length;
    GLint size;
    uniforms_.reserve(uniform_count);
    for (GLuint i = 0; i < uniform_count; ++i) {
      // Get the uniform information from OpenGL.
      name[0] = 0;
      GLenum type;
      gm->GetActiveUniform(id_, i, max_length, &length, &size, &type, name);

      // We want the base name, so cut off the name at '[' if it exists.
      for (int i = 0; i < kMaxNameLength; i++) {
        if (name[i] == '[' || name[i] == 0) {
          name[i] = 0;
          break;
        }
      }

      // Find this uniform in the registry.
      if (const ShaderInputRegistry::UniformSpec* spec =
              reg->Find<Uniform>(name)) {
        // Validate the type with the registry entry.
        if (!ValidateUniformType(name, spec->value_type, type)) {
          TracingHelper helper;
          LOG(WARNING) << "***ION: Uniform '" << name << "' has a"
                       << " different type from its spec: spec type: "
                       << spec->value_type
                       << ", uniform type: " << helper.ToString("GLenum", type);
        }

        const int location = gm->GetUniformLocation(id_, name);
        // Add this uniform's spec to the vector of uniforms for this shader
        // so that the shader can check for updated values when bound.
        uniforms_.push_back(UniformCacheEntry(this, location, size, spec));
      } else {
        // The registry does not define this uniform.
        LOG(WARNING) << "***ION: Uniform '" << name << "' used in shader '"
                     << shader_program.GetLabel()
                     << "' does not have a registry entry";
      }
    }
  }
}

template <typename HolderType>
bool Renderer::ShaderProgramResource::ContainsAnEvictedTexture(
    const Uniform& uniform, ResourceBinder* rb) {
  using HolderPtr = base::SharedPtr<HolderType>;
  if (!uniform.IsValid()) return false;
  bool ret = false;
  if (const size_t count = uniform.GetCount()) {
    for (size_t i = 0; i < count; ++i) {
      if (HolderType* holder = uniform.GetValueAt<HolderPtr>(i).Get()) {
        TextureResource* txr = GetResource(holder, rb);
        if (txr && rb->WasTextureEvicted(txr)) {
          ret = true;
          break;
        }
      }
    }
  } else if (HolderType* holder = uniform.GetValue<HolderPtr>().Get()) {
    TextureResource* txr = GetResource(holder, rb);
    ret = txr && rb->WasTextureEvicted(txr);
  }
  return ret;
}

template <typename HolderType>
void Renderer::ShaderProgramResource::BindTextures(
    const Uniform& u, const UniformCacheEntry& entry, ResourceBinder* rb) {
  using HolderPtr = base::SharedPtr<HolderType>;
  if (size_t count = u.GetCount()) {
    count = std::min(count, entry.unit_associations.size());
    for (size_t i = 0; i < count; ++i) {
      if (HolderType* holder = u.GetValueAt<HolderPtr>(i).Get()) {
        if (TextureResource* txr = GetResource(holder, rb))
          txr->BindToUnit(rb, entry.unit_associations[i]);
      }
    }
  } else if (HolderType* holder = u.GetValue<HolderPtr>().Get()) {
    if (TextureResource* txr = GetResource(holder, rb))
      txr->BindToUnit(rb, entry.unit_association);
  }
}

void Renderer::ShaderProgramResource::UpdateUniformValuesAndImageUnits(
    ResourceBinder* rb) {
  GraphicsManager* gm = GetGraphicsManager();
  const ShaderInputRegistry* prev_reg = nullptr;
  ShaderInputRegistryResource* sirr = nullptr;
  const size_t uniform_count = uniforms_.size();
  // Iterate over all UniformCacheEntrys.
  for (size_t i = 0; i < uniform_count; ++i) {
    UniformCacheEntry& entry = uniforms_[i];
    const size_t index_in_registry = entry.index;
    // Assign image units to texture uniforms. Subsequent UpdateUniformValue()
    // calls just bind textures.
    if (entry.is_texture && entry.UpdateUnitAssociations(rb)) {
      // Unit association(s) changed so update uniform values.
      if (size_t count = entry.unit_associations.size()) {
        gm->Uniform1iv(entry.location,
                       static_cast<GLsizei>(count),
                       &entry.unit_associations[0]);
      } else {
        gm->Uniform1i(entry.location, entry.unit_association);
      }
    }
    ShaderInputRegistry* reg = entry.registry;
    if (reg != prev_reg) {
      prev_reg = reg;
      sirr = GetResource(reg, rb);
      sirr->Update(rb);
    }
    entry.sirr = sirr;
    // Register this with sirr to receive notifications of uniform changes.
    sirr->SetUniformCacheInfo(index_in_registry, this, i);

    // Grab top of uniform stack - this is the uniform we want to be valid in
    // opengl-land.
    const Uniform& uniform = sirr->GetUniform(index_in_registry);
    UpdateUniformValue(gm, rb, uniform, &entry);
  }
  uniform_changes_.clear();
  ++uniform_change_stamp_;
}

void Renderer::ShaderProgramResource::UpdateChangedUniformValues(
    ResourceBinder* rb) {
  GraphicsManager* gm = GetGraphicsManager();
  const size_t uniform_count = uniform_changes_.size();
  for (size_t i = 0; i < uniform_count; ++i) {
    size_t change = uniform_changes_[i];
    // Grab top of uniform stack - this is the uniform we want to be valid in
    // opengl-land.
    UniformCacheEntry& entry = uniforms_[change];
    const Uniform& uniform = entry.sirr->GetUniform(entry.index);
    UpdateUniformValue(gm, rb, uniform, &entry);
  }
  uniform_changes_.clear();
  ++uniform_change_stamp_;
}

void Renderer::ShaderProgramResource::UpdateUniformValue(
    GraphicsManager* gm, ResourceBinder* rb, const Uniform& uniform,
    UniformCacheEntry* entry) {
  if (!uniform.IsValid()) {
    if (!rb->IsProcessingInfoRequests()) {
        LOG(WARNING) << "***ION: There is no value set for uniform '"
                     << entry->registry->GetSpecs<Uniform>()[entry->index].name
                     << "' for shader program '"
                     << GetShaderProgram().GetLabel() << "', or it "
                     << "was created with the wrong ShaderInputRegistry.  "
                     << "Rendering results may be unexpected.";
      }
      return;
  }
  // Update unit associations and check if any units have changed.
  if (entry->is_texture) {
    if (uniform.GetType() == kTextureUniform)
      BindTextures<Texture>(uniform, *entry, rb);
    else if (uniform.GetType() == kCubeMapTextureUniform)
      BindTextures<CubeMapTexture>(uniform, *entry, rb);
  } else if (entry->uniform_stamp != uniform.GetStamp()) {
    // Note that we don't always check if the actual uniform value has changed
    // because copying the value into our UniformCacheEntry and the operator==
    // can be costly. The uniform stamp mechanism overestimates changes so
    // sometimes we may send uniforms that haven't actually changed.
    ScopedLabel label(rb, &uniform,
                      entry->registry->GetSpecs<Uniform>()[entry->index].name,
                      ION_PRETTY_FUNCTION);
    // Remember stamp to detect any changes next time around.
    entry->uniform_stamp = uniform.GetStamp();
    // Send the value to OpenGL.
    rb->SendUniform(uniform, entry->location, gm);
  }
}

void Renderer::ShaderProgramResource::Update(ResourceBinder* rb) {
  // If shaders have changed then we need to reset their cached resources.
  if (TestModifiedBit(ShaderProgram::kVertexShaderChanged))
    vertex_resource_ = nullptr;
  if (TestModifiedBit(ShaderProgram::kGeometryShaderChanged))
    geometry_resource_ = nullptr;
  if (TestModifiedBit(ShaderProgram::kFragmentShaderChanged))
    fragment_resource_ = nullptr;
  if (TestModifiedBit(ShaderProgram::kTessControlShaderChanged))
     tess_ctrl_resource_ = nullptr;
  if (TestModifiedBit(ShaderProgram::kTessEvaluationShaderChanged))
     tess_eval_resource_ = nullptr;

  // Allow shaders to Update().
  const bool vertex_updated =
      vertex_resource_ && vertex_resource_->UpdateShader(rb);
  const bool geometry_updated =
      geometry_resource_ && geometry_resource_->UpdateShader(rb);
  const bool fragment_updated =
      fragment_resource_ && fragment_resource_->UpdateShader(rb);
  const bool tess_ctrl_updated =
      tess_ctrl_resource_ && tess_ctrl_resource_->UpdateShader(rb);
  const bool tess_eval_updated =
      tess_eval_resource_ && tess_eval_resource_->UpdateShader(rb);
  if (vertex_updated || geometry_updated || fragment_updated ||
      tess_ctrl_updated || tess_eval_updated ||
      AnyModifiedBitsSet()) {
    SCOPED_RESOURCE_LABEL;
    const ShaderProgram& shader_program = GetShaderProgram();

    if (!vertex_resource_) {
      if (Shader* shader = shader_program.GetVertexShader().Get()) {
        if ((vertex_resource_ = GetResource(shader, rb), rb)) {
          vertex_resource_->SetShaderType(GL_VERTEX_SHADER);
          vertex_resource_->UpdateShader(rb);
        }
      }
    }
    if (!geometry_resource_) {
      if (Shader* shader = shader_program.GetGeometryShader().Get()) {
        if ((geometry_resource_ = GetResource(shader, rb))) {
          geometry_resource_->SetShaderType(GL_GEOMETRY_SHADER);
          geometry_resource_->UpdateShader(rb);
        }
      }
    }
    if (!fragment_resource_) {
      if (Shader* shader = shader_program.GetFragmentShader().Get()) {
        if ((fragment_resource_ = GetResource(shader, rb))) {
          fragment_resource_->SetShaderType(GL_FRAGMENT_SHADER);
          fragment_resource_->UpdateShader(rb);
        }
      }
    }
    if (!tess_ctrl_resource_) {
      if (Shader *shader = shader_program.GetTessControlShader().Get()) {
        if ((tess_ctrl_resource_ = GetResource(shader, rb))) {
          tess_ctrl_resource_->SetShaderType(GL_TESS_CONTROL_SHADER);
          tess_ctrl_resource_->UpdateShader(rb);
        }
      }
    }
    if (!tess_eval_resource_) {
      if (Shader *shader = shader_program.GetTessEvalShader().Get()) {
        if ((tess_eval_resource_ = GetResource(shader, rb))) {
          tess_eval_resource_->SetShaderType(GL_TESS_EVALUATION_SHADER);
          tess_eval_resource_->UpdateShader(rb);
        }
      }
    }

    const GLuint vertex_shader_id =
        vertex_resource_ ? vertex_resource_->GetId() : 0;
    const GLuint geometry_shader_id =
        geometry_resource_ ? geometry_resource_->GetId() : 0;
    const GLuint fragment_shader_id =
        fragment_resource_ ? fragment_resource_->GetId() : 0;
    const GLuint tess_ctrl_shader_id =
        tess_ctrl_resource_ ? tess_ctrl_resource_->GetId() : 0;
    const GLuint tess_eval_shader_id =
        tess_eval_resource_ ? tess_eval_resource_->GetId() : 0;

    // Create a program object and attach the two compiled shaders.
    const std::string& id_string = shader_program.GetLabel();
    GraphicsManager* gm = GetGraphicsManager();

    std::string info_log = shader_program.GetInfoLog();
    GLuint id = LinkShaderProgram(id_string, vertex_shader_id,
                                  geometry_shader_id, fragment_shader_id,
                                  tess_ctrl_shader_id, tess_eval_shader_id,
                                  shader_program.GetCapturedVaryings(),
                                  &info_log, gm);

    if (id != 0) {
      // Bind each attribute to its name in the shader, using its order in
      // the registry.
      const ShaderInputRegistryPtr& reg = shader_program.GetRegistry();
      // Check that all inputs are unique in the registry.
      if (!reg->CheckInputsAreUnique()) {
        LOG(WARNING) << "***ION: Registry '" << reg->GetId() << " contains"
                     << " multiple definitions of some inputs, rendering"
                     << " results may be unexpected";
      }

      // Set up the attribute cache.
      PopulateAttributeCache(id, id_string, reg, gm);

      // Relink the program for the bindings to take effect.
      id = RelinkShaderProgram(id_string, id,
                               shader_program.GetCapturedVaryings(), &info_log,
                               gm);
      bool need_to_update_label =
          vertex_updated || geometry_updated || fragment_updated ||
          tess_ctrl_updated || tess_eval_updated ||
          TestModifiedBit(ResourceHolder::kLabelChanged);
      if (id != 0) {
        id_ = id;
        need_to_update_label = true;
      }

      // Get all of the uniforms for this shader from OpenGL and set up their
      // uniform locations in the cache.
      PopulateUniformCache();

      // We need to update the label if it has changed or either of the sources
      // have since a new program object will be generated.
      if (need_to_update_label)
        SetObjectLabel(gm, GL_PROGRAM_OBJECT_EXT, id_,
                       shader_program.GetLabel());
    }

    // Send the info logs to the holder.
    shader_program.SetInfoLog(info_log);
    ResetModifiedBits();
  }
}

void Renderer::ShaderProgramResource::ObtainImageUnits(ResourceBinder* rb) {
  GraphicsManager* gm = GetGraphicsManager();
  for (size_t i = 0, count = uniforms_.size(); i < count; ++i) {
    UniformCacheEntry& entry = uniforms_[i];
    if (entry.is_texture && entry.UpdateUnitAssociations(rb)) {
      // Unit association(s) changed so update uniform values.
      if (size_t count = entry.unit_associations.size()) {
        gm->Uniform1iv(entry.location,
                       static_cast<GLsizei>(count),
                       &entry.unit_associations[0]);
      } else {
        gm->Uniform1i(entry.location, entry.unit_association);
      }
    }
  }
}

void Renderer::ShaderProgramResource::ReleaseImageUnits(ResourceBinder* rb) {
  for (size_t i = 0, count = uniforms_.size(); i < count; ++i) {
    const UniformCacheEntry& entry = uniforms_[i];
    if (entry.is_texture) {
      if (entry.unit_association >= 0) {
        rb->ReleaseImageUnit(entry.unit_association);
      } else {
        for (size_t j = 0, jcount = entry.unit_associations.size();
             j < jcount; ++j) {
          rb->ReleaseImageUnit(entry.unit_associations[j]);
        }
      }
    }
  }
}

bool Renderer::ShaderProgramResource::Bind(ResourceBinder* rb) {
  Update(rb);
  if (id_) {
    SCOPED_RESOURCE_LABEL;
    // Ensure that the latest uniform values are sent to OpenGL.
    ShaderProgramResource* prev_spr = rb->GetActiveShaderProgram();
    if (rb->BindProgram(id_, this)) {
      if (prev_spr)
        prev_spr->ReleaseImageUnits(rb);
      // Update all uniforms used by the program on first bind and obtain
      // image units for texture uniforms.
      UpdateUniformValuesAndImageUnits(rb);
      return true;
    } else {
      // If program hasn't changed, then only update changed uniforms.
      UpdateChangedUniformValues(rb);
      return false;
    }
  }
  return false;
}

void Renderer::ShaderProgramResource::Unbind(ResourceBinder* rb) {
  // This may be legally called from a different resource binder.
  if (rb) {
    ReleaseImageUnits(rb);
    for (auto& entry : uniforms_) {
      if (ShaderInputRegistryResource* sirr = entry.sirr) {
        sirr->DetachShaderProgramResource(this, entry.index);
        entry.sirr = nullptr;
      }
    }
    rb->ClearProgramBinding(id_);
  }
}

  void Renderer::ShaderProgramResource::Release(bool can_make_gl_calls) {
  BaseResourceType::Release(can_make_gl_calls);
  if (id_) {
    // unbind all and remove vertex array keys from all binders
    base::ReadLock read_lock(GetResourceBinderLock());
    base::ReadGuard read_guard(&read_lock);
    ResourceBinderMap& binders = GetResourceBinderMap();
    for (ResourceBinderMap::iterator it = binders.begin();
         it != binders.end();
         ++it) {
      Unbind(it->second.get());
      it->second->EraseVertexArrayKey(this);
    }
    if (resource_owns_gl_id_ && can_make_gl_calls)
      GetGraphicsManager()->DeleteProgram(id_);
    id_ = 0;
  }
}

//-----------------------------------------------------------------------------
//
// Renderer::BufferResource class.
//
//-----------------------------------------------------------------------------

class Renderer::BufferResource : public Resource<BufferObject::kNumChanges> {
 public:
  BufferResource(ResourceBinder* rb, ResourceManager* rm,
                 const BufferObject& buffer_object, ResourceKey key, GLuint id)
      : Renderer::Resource<BufferObject::kNumChanges>(rm, buffer_object, key,
                                                      id),
        initial_target_(buffer_object.GetInitialTarget()),
        latest_target_(buffer_object.GetInitialTarget()),
        was_used_as_element_buffer_(false) {}

  ~BufferResource() override {
    DCHECK(id_ == 0U || !portgfx::GlContext::GetCurrent());
  }

  void Release(bool can_make_gl_calls) override;
  void Update(ResourceBinder* rb) override;
  ResourceType GetType() const override { return kBufferObject; }

  // Binds the buffer "somewhere", i.e., to its initial bind target.
  void Bind(ResourceBinder* rb);
  // Binds the buffer to a specific bind point.
  void BindToTarget(ResourceBinder* rb, BufferObject::Target target);
  void Unbind(ResourceBinder* rb) override;

  // Returns the buffer target to which this buffer was most recently bound.
  GLenum GetGlTarget() const {
    return base::EnumHelper::GetConstant(latest_target_);
  }
  size_t GetSize() const {
    const BufferObject& bo = GetBufferObject();
    return bo.GetStructSize() * bo.GetCount();
  }

  void UploadData();
  void UploadSubData(const Range1ui& range, const void* data) const;
  void CopySubData(ResourceBinder* rb,
                   BufferResource* src_resource,
                   const Range1ui& range,
                   uint32 read_offset);

  void OnDestroyed() override {
    // If the buffer was ever used as an index buffer, clear it from all
    // vertex array objects that reference it.
    if (was_used_as_element_buffer_)
      GetResourceManager()->DisassociateElementBufferFromArrays(this);
    BaseResourceType::OnDestroyed();
  }

 private:
  const BufferObject& GetBufferObject() const {
    return static_cast<const BufferObject&>(*GetHolder());
  }

  BufferObject::Target initial_target_;
  // The target where this buffer was most recently bound.
  BufferObject::Target latest_target_;
  bool was_used_as_element_buffer_;
};

void Renderer::BufferResource::Bind(ResourceBinder* rb) {
  BindToTarget(rb, initial_target_);
}

void Renderer::BufferResource::BindToTarget(ResourceBinder* rb,
                                            BufferObject::Target target) {
  latest_target_ = target;
  Update(rb);
  if (id_) {
    SCOPED_RESOURCE_LABEL;
    if (target == BufferObject::kElementBuffer) {
      was_used_as_element_buffer_ = true;
    }
    rb->BindBuffer(target, id_, this);
  }
}

void Renderer::BufferResource::UploadData() {
  const BufferObject& bo = GetBufferObject();
  const size_t size = bo.GetStructSize() * bo.GetCount();
  SetUsedGpuMemory(size);
  GetGraphicsManager()->BufferData(
      GetGlTarget(), size, bo.GetData() ? bo.GetData()->GetData() : nullptr,
      base::EnumHelper::GetConstant(bo.GetUsageMode()));
}

void Renderer::BufferResource::UploadSubData(const Range1ui& range,
                                             const void* data) const {
  GetGraphicsManager()->BufferSubData(GetGlTarget(), range.GetMinPoint(),
                                      range.GetSize(), data);
}

void Renderer::BufferResource::CopySubData(ResourceBinder* rb,
                                           BufferResource* src_resource,
                                           const Range1ui& range,
                                           uint32 read_offset) {
  if (!src_resource || src_resource == this) {
    // Copy within same BufferObject.
    GetGraphicsManager()->CopyBufferSubData(
        GetGlTarget(), GetGlTarget(), read_offset, range.GetMinPoint(),
        range.GetSize());
  } else {
    // Copy between 2 BufferObjects.
    // Bind src/dst to read/write targets and then copy.
    src_resource->BindToTarget(rb, BufferObject::kCopyReadBuffer);
    BindToTarget(rb, BufferObject::kCopyWriteBuffer);
    GetGraphicsManager()->CopyBufferSubData(
        GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
        read_offset, range.GetMinPoint(), range.GetSize());
  }
}

void Renderer::BufferResource::Update(ResourceBinder* rb) {
  if (!AnyModifiedBitsSet()) {
    return;
  }
  SCOPED_RESOURCE_LABEL;
  // Generate the VBO.
  GraphicsManager* gm = GetGraphicsManager();
  DCHECK(gm);
  if (!id_)
    gm->GenBuffers(1, &id_);
  if (!id_) {
    LOG(ERROR) << "***ION: Unable to create buffer object";
    return;
  }
  // If the resource was changed elsewhere then we need to ensure that it is
  // bound again.
  if (TestModifiedBit(ResourceHolder::kResourceChanged))
    rb->ClearBufferBindings(id_);

  // Send the vertex data.
  const BufferObject& bo = GetBufferObject();
  if (!bo.GetStructSize()) {
    LOG(WARNING) << "***ION: Unable to update buffer object \""
                 << bo.GetLabel() << "\": BufferObject's"
                 << " struct size is 0";
    return;
  }
  if (!bo.GetCount()) {
    LOG(WARNING) << "***ION: Unable to update buffer object \""
                 << bo.GetLabel() << "\": BufferObject's"
                 << " struct count is 0";
    return;
  }
  DCHECK_LT(0U, bo.GetCount());
  rb->BindBuffer(latest_target_, id_, this);

  const bool data_changed = TestModifiedBit(BufferObject::kDataChanged);
  const bool label_changed = TestModifiedBit(ResourceHolder::kLabelChanged);
  const bool subdata_changed = TestModifiedBit(BufferObject::kSubDataChanged);
  // Reset modified bits here in case following code caues re-entrant Update()
  // call, otherwise we get infinite recursion trying to reset modified bits.
  ResetModifiedBits();

  if (data_changed) {
    UploadData();
    // Notify the data container that the data has been used and can be
    // deleted if requested.
    if (bo.GetData().Get())
      bo.GetData()->WipeData();
  }
  if (label_changed)
    SetObjectLabel(gm, GL_BUFFER_OBJECT_EXT, id_, bo.GetLabel());

  if (subdata_changed) {
    const base::AllocVector<BufferObject::BufferSubData>& sub_data =
        bo.GetSubData();
    const size_t count = sub_data.size();
    for (size_t i = 0; i < count; ++i) {
      const BufferObject::BufferSubData& sdata = sub_data[i];
      if (sdata.data.Get() && sdata.data->GetData()) {
        UploadSubData(
            sdata.range, sdata.data->GetData<uint8>() + sdata.read_offset);
        // Notify the data container that the data has been used and can
        // be deleted if requested.
        sdata.data->WipeData();
        continue;
      }
      // At this point we have a CopySubData operation. Treat a null
      // sdata.src as a copy within the single BufferObject, bo.
      BufferResource* src_resource = nullptr;
      if (sdata.src.Get()) {
        src_resource = GetResource(sdata.src.Get(), rb);
        DCHECK(src_resource);
        // Update src BufferObject.
        src_resource->Update(rb);
      }
      if (gm->IsFeatureAvailable(GraphicsManager::kCopyBufferSubData)) {
        CopySubData(rb, src_resource, sdata.range, sdata.read_offset);
        continue;
      }
      // Emulate CopyBufferSubData by mapping the data and then
      // using BufferSubData to perform the copy.
      Range1ui read_range;
      read_range.SetWithSize(sdata.read_offset, sdata.range.GetSize());
      const Range1ui& write_range = sdata.range;
      BufferObjectPtr dst(const_cast<BufferObject*>(&bo));
      BufferObjectPtr src(sdata.src.Get() ? sdata.src : dst);
      Range1ui union_range;
      if (src == dst) {
        union_range = read_range;
        union_range.ExtendByRange(write_range);
        rb->MapBufferObjectDataRange(src, kReadWrite, union_range);
      } else {
        rb->MapBufferObjectDataRange(src, kReadOnly, read_range);
      }
      switch (src->GetMappedData().data_source) {
        case BufferObject::MappedBufferData::kGpuMapped:
        case BufferObject::MappedBufferData::kDataContainer:
          if (src == dst) {
            uint8* data = static_cast<uint8*>(src->GetMappedPointer());
            memcpy(data +
                   write_range.GetMinPoint() - union_range.GetMinPoint(),
                   data +
                   read_range.GetMinPoint() - union_range.GetMinPoint(),
                   write_range.GetSize());
          } else {
            rb->MapBufferObjectDataRange(dst, kWriteOnly, write_range);
            memcpy(dst->GetMappedPointer(), src->GetMappedPointer(),
                   write_range.GetSize());
            // Unmap will upload the copied bytes into dst.
            rb->UnmapBufferObjectData(dst);
          }
          break;
        case BufferObject::MappedBufferData::kAllocated:
          LOG(WARNING) << "***ION: Unable to copy buffer object \""
                       << src->GetLabel() << "\": BufferObject's"
                       << " DataContainer has been wiped and "
                       << " glCopyBufferSubData is not supported.";
          break;
        default:
          LOG(FATAL) << ION_PRETTY_FUNCTION <<
              "Invalid source for mapped BufferObject data";
          break;
      }
      rb->UnmapBufferObjectData(src);
    }
    bo.ClearSubData();
  }
}

void Renderer::BufferResource::Unbind(ResourceBinder* rb) {
  // If the buffer is currently bound then we need to update our internal
  // state. We do not need to explicitly bind another buffer, as if the buffer
  // is deleted while bound then OpenGL will set the current bound buffer to 0,
  // according to the spec.
  if (rb)
    rb->ClearBufferBindings(id_);
}

void Renderer::BufferResource::Release(bool can_make_gl_calls) {
  BaseResourceType::Release(can_make_gl_calls);
  if (id_) {
    UnbindAll();
    if (resource_owns_gl_id_ && can_make_gl_calls)
      GetGraphicsManager()->DeleteBuffers(1, &id_);
    SetUsedGpuMemory(0U);
    id_ = 0;
  }
}

//-----------------------------------------------------------------------------
//
// Renderer::TransformFeedbackResource class.
//
//-----------------------------------------------------------------------------

class Renderer::TransformFeedbackResource
    : public Resource<TransformFeedback::kNumChanges> {
 public:
  TransformFeedbackResource(ResourceBinder* rb, ResourceManager* rm,
                            const TransformFeedback& tf, ResourceKey key,
                            GLuint id)
      : Renderer::Resource<TransformFeedback::kNumChanges>(rm, tf, key, id) {}
  ~TransformFeedbackResource() override {
    DCHECK(id_ == 0U || !portgfx::GlContext::GetCurrent());
  }
  void Release(bool can_make_gl_calls) override;
  void Update(ResourceBinder* rb) override;
  void Bind(ResourceBinder* rb);
  void Unbind(ResourceBinder* rb) override;
  ResourceType GetType() const override { return kTransformFeedback; }
  const TransformFeedback& GetTransformFeedback() const {
    return static_cast<const TransformFeedback&>(*GetHolder());
  }
  void StartCapturing() { capturing_ = true; }
  void StopCapturing() { capturing_ = false; }
  bool IsCapturing() const { return capturing_; }

 private:
  bool capturing_ = false;
};

void Renderer::TransformFeedbackResource::Release(bool can_make_gl_calls) {
  BaseResourceType::Release(can_make_gl_calls);
  if (id_) {
    UnbindAll();
    if (resource_owns_gl_id_ && can_make_gl_calls)
      GetGraphicsManager()->DeleteTransformFeedbacks(1, &id_);
    id_ = 0;
  }
}

void Renderer::TransformFeedbackResource::Update(ResourceBinder* rb) {
  if (GetGraphicsManager()->IsFeatureAvailable(
          GraphicsManager::kTransformFeedback)) {
    const TransformFeedback& tf = GetTransformFeedback();
    if (AnyModifiedBitsSet()) {
      SCOPED_RESOURCE_LABEL;
      GraphicsManager* gm = GetGraphicsManager();
      DCHECK(gm);
      if (!id_) gm->GenTransformFeedbacks(1, &id_);
      if (id_) {
        rb->BindTransformFeedback(id_, this);
        if (TestModifiedBit(TransformFeedback::kCaptureBufferChanged)) {
          const BufferObjectPtr& buf = tf.GetCaptureBuffer();
          BufferResource* buf_resource = nullptr;
          GLuint buf_id = 0;
          if (buf.Get()) {
            buf_resource = GetResource(buf.Get(), rb);
            DCHECK(buf_resource);
            buf_resource->Update(rb);
            buf_id = buf_resource->GetId();
          }
          rb->BindBufferIndexed(BufferObject::kIndexedTransformFeedbackBuffer,
                                0, buf_id, buf_resource);
        }
        if (TestModifiedBit(ResourceHolder::kLabelChanged))
          SetObjectLabel(gm, GL_TRANSFORM_FEEDBACK, id_, tf.GetLabel());
        ResetModifiedBits();
      } else {
        LOG(ERROR) << "***ION: Unable to create transform feedback object";
      }
    }
  }
}

void Renderer::TransformFeedbackResource::Bind(ResourceBinder* rb) {
  Update(rb);
  SCOPED_RESOURCE_LABEL;
  rb->BindTransformFeedback(id_, this);
}

void Renderer::TransformFeedbackResource::Unbind(ResourceBinder* rb) {
  if (rb) rb->ClearTransformFeedbackBinding(id_);
}

//-----------------------------------------------------------------------------
//
// Renderer::FramebufferResource class.
//
//-----------------------------------------------------------------------------

class Renderer::FramebufferResource
    : public Resource<FramebufferObject::kNumChanges> {
 public:
  FramebufferResource(ResourceBinder* rb, ResourceManager* rm,
                      const FramebufferObject& fbo, ResourceKey key, GLuint id)
      : Renderer::Resource<FramebufferObject::kNumChanges>(rm, fbo, key, id),
        color_ids_(*this), depth_id_(0U), stencil_id_(0U),
        packed_depth_stencil_(false),
        implicit_multisample_(false) {
    color_ids_.resize(rm->GetGraphicsManager()->GetConstant<int>(
        GraphicsManager::kMaxColorAttachments));
  }

  ~FramebufferResource() override {
    DCHECK((id_ == 0U && depth_id_ == 0U && stencil_id_ == 0U) ||
           !resource_owns_gl_id_ || !portgfx::GlContext::GetCurrent());
    for (const auto& color_id : color_ids_) {
      DCHECK(color_id == 0U || !portgfx::GlContext::GetCurrent() ||
             !resource_owns_gl_id_);
    }
  }

  void Release(bool can_make_gl_calls) override;
  void Update(ResourceBinder* rb) override;
  ResourceType GetType() const override { return kFramebufferObject; }

  // Binds or unbinds the FBO in OpenGL.
  virtual void Bind(ResourceBinder* rb);
  void Unbind(ResourceBinder* rb) override;

  GLuint GetColorId(size_t index) const { return color_ids_[index]; }
  GLuint GetDepthId() const { return depth_id_; }
  GLuint GetStencilId() const { return stencil_id_; }

 private:
  const FramebufferObject& GetFramebufferObject() const {
    return static_cast<const FramebufferObject&>(*GetHolder());
  }

  void UpdateImplicitMultisampling(GraphicsManager* gm,
                                   const FramebufferObject& fbo);
  void UpdateAttachment(GraphicsManager* gm, ResourceBinder* rb, GLuint* id,
                        GLenum attachment_slot, const FramebufferObject& fbo,
                        const FramebufferObject::Attachment& attachment);

  void UpdateMemoryUsage(const FramebufferObject& fbo);

  // Renderbuffer attachment ids.
  base::AllocVector<GLuint> color_ids_;
  GLuint depth_id_;
  GLuint stencil_id_;
  // Whether the depth_id_ is also attached to the stencil attachment.
  bool packed_depth_stencil_;
  // Whether all attachments use implicit multisampling.
  bool implicit_multisample_;
};

void Renderer::FramebufferResource::Bind(ResourceBinder* rb) {
  Update(rb);
  SCOPED_RESOURCE_LABEL;
  rb->BindFramebuffer(id_, this);
}

void Renderer::FramebufferResource::UpdateAttachment(
    GraphicsManager* gm, ResourceBinder* rb, GLuint* id, GLenum attachment_slot,
    const FramebufferObject& fbo,
    const FramebufferObject::Attachment& attachment) {
  DCHECK(id);
  const GLenum target = GL_FRAMEBUFFER;
  // If we previously had a renderbuffer bound to this attachment, delete it
  // and zero out the ID.
  if (attachment.GetBinding() != FramebufferObject::kRenderbuffer && *id) {
    gm->DeleteRenderbuffers(1, id);
    *id = 0U;
  }
  bool unbind_on_error = false;
  uint32 max_samples = static_cast<uint32>(gm->GetConstant<int>(
      GraphicsManager::kMaxSamples));
  if (attachment.GetSamples() > max_samples) {
    LOG(ERROR) << "***ION: Too many samples in multisampled attachment: "
               << attachment.GetSamples() << " samples requested "
               << "(maximum is " << max_samples << ")";
    unbind_on_error = true;
  }
  if (unbind_on_error) {
    // Do nothing, since we are already in error state. The actual unbinding
    // is done at the bottom of this function.
  } else if (attachment.GetBinding() == FramebufferObject::kRenderbuffer) {
    // Create the renderbuffer if it does not exist.
    if (!*id)
      gm->GenRenderbuffers(1, id);
    if (*id) {
      // Bind the renderbuffer to set its format.
      gm->BindRenderbuffer(GL_RENDERBUFFER, *id);
      if (attachment.GetSamples() > 0) {
        if (implicit_multisample_) {
          gm->RenderbufferStorageMultisampleEXT(
              GL_RENDERBUFFER,
              static_cast<GLsizei>(attachment.GetSamples()),
              Image::GetPixelFormat(attachment.GetFormat()).internal_format,
              fbo.GetWidth(), fbo.GetHeight());
        } else {
          gm->RenderbufferStorageMultisample(
              GL_RENDERBUFFER,
              static_cast<GLsizei>(attachment.GetSamples()),
              Image::GetPixelFormat(attachment.GetFormat()).internal_format,
              fbo.GetWidth(), fbo.GetHeight());
        }
      } else {
        ImagePtr image = attachment.GetImage();
        if (image.Get() && (image->GetType() == Image::kEgl ||
            image->GetType() == Image::kExternalEgl)) {
          const DataContainerPtr& container = image->GetData();
          const void* data = container.Get() ? container->GetData() : nullptr;
          if (data &&
              gm->IsFeatureAvailable(GraphicsManager::kEglImage)) {
            gm->EGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
                                                     const_cast<void*>(data));
          }
        } else {
          gm->RenderbufferStorage(
            GL_RENDERBUFFER,
            Image::GetPixelFormat(attachment.GetFormat()).internal_format,
            fbo.GetWidth(), fbo.GetHeight());
        }
      }
    } else {
      LOG(ERROR) << "***ION: Unable to create renderbuffer object.";
    }
    // Bind the renderbuffer to the attachment.
    gm->FramebufferRenderbuffer(target, attachment_slot,
                                GL_RENDERBUFFER, *id);
  } else if (attachment.GetBinding() != FramebufferObject::kUnbound) {
    // Handle all texture attachments here.
    DCHECK(attachment.GetBinding() == FramebufferObject::kTexture ||
           attachment.GetBinding() == FramebufferObject::kTextureLayer ||
           attachment.GetBinding() == FramebufferObject::kMultiview ||
           attachment.GetBinding() == FramebufferObject::kCubeMapTexture);
    DCHECK(attachment.GetCubeMapTexture().Get() != nullptr ||
           attachment.GetTexture().Get());

    ImagePtr image(nullptr);
    CubeMapTexture::CubeFace face = CubeMapTexture::kPositiveX;
    const uint32 mip_level = attachment.GetMipLevel();
    TextureResource* tr = nullptr;
    std::string label;
    if (attachment.GetBinding() == FramebufferObject::kCubeMapTexture) {
      image = attachment.GetCubeMapTexture()->GetImage(face, mip_level);
      DCHECK(image.Get()) << "Cube map "
          << attachment.GetCubeMapTexture()->GetLabel() << " has no image";
      face = attachment.GetCubeMapFace();
      tr = GetResource(attachment.GetCubeMapTexture().Get(), rb);
    } else {
      image = attachment.GetTexture()->GetImage(mip_level);
      DCHECK(image.Get()) << "Texture " << attachment.GetTexture()->GetLabel()
          << " has no image";
      tr = GetResource(attachment.GetTexture().Get(), rb);
    }
    DCHECK(tr);
    // Validate attachment parameters.
    if (image->GetFormat() != Image::kEglImage &&
        (image->GetWidth() != fbo.GetWidth() ||
         image->GetHeight() != fbo.GetHeight())) {
      LOG(ERROR) << "***ION: Mismatched texture and FBO dimensions: "
                 << image->GetWidth() << " x " << image->GetHeight()
                 << " vs. "
                 << fbo.GetWidth() << " x " << fbo.GetHeight();
    }
    if (attachment.GetBinding() == FramebufferObject::kTextureLayer &&
        attachment.GetLayer() >= image->GetDepth()) {
      LOG(ERROR) << "***ION: Invalid texture layer index: "
                 << attachment.GetLayer() << " in texture with "
                 << image->GetDepth() << " layers";
      unbind_on_error = true;
    }
    if (attachment.GetBinding() == FramebufferObject::kMultiview) {
      uint32 last = attachment.GetBaseViewIndex() + attachment.GetNumViews();
      if (image->GetFormat() != Image::kEglImage && last > image->GetDepth()) {
        LOG(ERROR) << "***ION: Invalid multiview parameters: "
                   << attachment.GetNumViews() << " views with base view index "
                   << attachment.GetBaseViewIndex() << " in texture with "
                   << image->GetDepth() << " layers";
        unbind_on_error = true;
      }
      if (gm->IsFeatureAvailable(GraphicsManager::kMultiview)) {
        uint32 max_views = static_cast<uint32>(gm->GetConstant<int>(
            GraphicsManager::kMaxViews));
        if (attachment.GetNumViews() > max_views) {
          LOG(ERROR) << "***ION: Too many views in multiview attachment: "
                     << attachment.GetNumViews() << " views requested "
                     << "(maximum is " << max_views << ")";
          unbind_on_error = true;
        }
      } else {
        LOG(ERROR) << "***ION: Requested a multiview attachment, but the "
                      "GL_OVR_multiview2 extension is not supported";
        unbind_on_error = true;
      }
    }
    tr->Bind(rb);

    // Bind the texture to the attachment.
    if (unbind_on_error) {
      // Do nothing - this will be handled at the end.
    } else if (attachment.GetBinding() == FramebufferObject::kCubeMapTexture) {
      if (implicit_multisample_) {
        const GLenum face_gl = base::EnumHelper::GetConstant(face);
        gm->FramebufferTexture2DMultisampleEXT(target, attachment_slot,
                                               face_gl, tr->GetId(),
                                               static_cast<GLint>(mip_level),
                                               static_cast<GLsizei>(
                                                   attachment.GetSamples()));
      } else {
        gm->FramebufferTexture2D(target, attachment_slot,
                                 base::EnumHelper::GetConstant(face),
                                 tr->GetId(), static_cast<GLint>(mip_level));
      }
    } else if (attachment.GetBinding() == FramebufferObject::kTextureLayer) {
      if (gm->IsFeatureAvailable(GraphicsManager::kFramebufferTextureLayer)) {
        gm->FramebufferTextureLayer(target, attachment_slot,
                                    tr->GetId(), static_cast<GLint>(mip_level),
                                    static_cast<GLint>(attachment.GetLayer()));
      } else {
        LOG(ERROR) << "***ION: Requested a texture layer attachment, but "
                      "glFramebufferTextureLayer is not supported";
        unbind_on_error = true;
      }
    } else if (attachment.GetBinding() == FramebufferObject::kMultiview) {
      if (implicit_multisample_) {
        if (gm->IsFeatureAvailable(
                GraphicsManager::kMultiviewImplicitMultisample)) {
          // In older versions of OVR_multiview_multisampled_render_to_texture,
          // it was illegal to attach an implicitly multisampled multiview
          // texture to an FBO that is bound to GL_READ_FRAMEBUFFER (and, by
          // implication, GL_FRAMEBUFFER).
          const GLenum target =
              gm->IsFeatureAvailable(GraphicsManager::kFramebufferTargets)
              ? GL_DRAW_FRAMEBUFFER
              : GL_FRAMEBUFFER;
          // The Qualcomm driver in Android N incorrectly reports an incomplete
          // framebuffer if a packed depth-stencil texture is not bound to all
          // three of GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT and
          // GL_DEPTH_STENCIL_ATTACHMENT. (b/34495781)
          // 
          if (attachment_slot == GL_DEPTH_STENCIL_ATTACHMENT) {
            gm->FramebufferTextureMultisampleMultiviewOVR(
                target, GL_DEPTH_ATTACHMENT, tr->GetId(),
                static_cast<GLint>(mip_level),
                static_cast<GLint>(attachment.GetSamples()),
                static_cast<GLint>(attachment.GetBaseViewIndex()),
                static_cast<GLint>(attachment.GetNumViews()));
            gm->FramebufferTextureMultisampleMultiviewOVR(
                target, GL_STENCIL_ATTACHMENT, tr->GetId(),
                static_cast<GLint>(mip_level),
                static_cast<GLint>(attachment.GetSamples()),
                static_cast<GLint>(attachment.GetBaseViewIndex()),
                static_cast<GLint>(attachment.GetNumViews()));
          }
          gm->FramebufferTextureMultisampleMultiviewOVR(target,
              attachment_slot, tr->GetId(), static_cast<GLint>(mip_level),
              static_cast<GLint>(attachment.GetSamples()),
              static_cast<GLint>(attachment.GetBaseViewIndex()),
              static_cast<GLint>(attachment.GetNumViews()));
        } else {
          LOG(ERROR) << "***ION: Requested an implicitly multisampled "
                        "multiview attachment, but the "
                        "GL_OVR_multiview_multisampled_render_to_texture "
                        "extension is not supported";
          unbind_on_error = true;
        }
      } else {
        // This should have already been checked before verifying that the
        // number of views is within the limit, before entering this if/else
        // chain.
        DCHECK(gm->IsFeatureAvailable(GraphicsManager::kMultiview));
        // To avoid the driver bug described in b/34495781, bind the
        // depth-stencil texture to all three attachment points.
        // 
        if (attachment_slot == GL_DEPTH_STENCIL_ATTACHMENT) {
          gm->FramebufferTextureMultiviewOVR(
              target, GL_DEPTH_ATTACHMENT, tr->GetId(),
              static_cast<GLint>(mip_level),
              static_cast<GLint>(attachment.GetBaseViewIndex()),
              static_cast<GLint>(attachment.GetNumViews()));
          gm->FramebufferTextureMultiviewOVR(
              target, GL_STENCIL_ATTACHMENT, tr->GetId(),
              static_cast<GLint>(mip_level),
              static_cast<GLint>(attachment.GetBaseViewIndex()),
              static_cast<GLint>(attachment.GetNumViews()));
        }
        gm->FramebufferTextureMultiviewOVR(target, attachment_slot,
            tr->GetId(), static_cast<GLint>(mip_level),
            static_cast<GLint>(attachment.GetBaseViewIndex()),
            static_cast<GLint>(attachment.GetNumViews()));
      }
    } else {
      if (implicit_multisample_) {
        gm->FramebufferTexture2DMultisampleEXT(target, attachment_slot,
                                               tr->GetGlTarget(), tr->GetId(),
                                               static_cast<GLint>(mip_level),
                                               static_cast<GLsizei>(
                                                   attachment.GetSamples()));
      } else {
        gm->FramebufferTexture2D(target, attachment_slot,
                                 tr->GetGlTarget(), tr->GetId(),
                                 static_cast<GLint>(mip_level));
      }
    }
  }
  // Unbind the attachment if necessary.
  if (attachment.GetBinding() == FramebufferObject::kUnbound ||
      unbind_on_error) {
    gm->FramebufferRenderbuffer(target, attachment_slot,
                                GL_RENDERBUFFER, 0);
  }
}

void Renderer::FramebufferResource::UpdateMemoryUsage(
    const FramebufferObject& fbo) {
  size_t data_size = 0U;
  const size_t num_color_ids = color_ids_.size();
  for (size_t i = 0; i < num_color_ids; ++i) {
    if (color_ids_[i])
      data_size += Image::ComputeDataSize(fbo.GetColorAttachment(i).GetFormat(),
                                          fbo.GetWidth(), fbo.GetHeight());
  }
  if (depth_id_)
    data_size += Image::ComputeDataSize(fbo.GetDepthAttachment().GetFormat(),
                                        fbo.GetWidth(), fbo.GetHeight());
  if (stencil_id_)
    data_size += Image::ComputeDataSize(fbo.GetStencilAttachment().GetFormat(),
                                        fbo.GetWidth(), fbo.GetHeight());
  SetUsedGpuMemory(data_size);
}

void Renderer::FramebufferResource::UpdateImplicitMultisampling(
    GraphicsManager* gm, const FramebufferObject& fbo) {
  bool use_ext = true;
  if (gm->IsFeatureAvailable(GraphicsManager::kImplicitMultisample)) {
    fbo.ForEachAttachment(
        [&use_ext](const FramebufferObject::Attachment& a, int) -> void {
      use_ext &= a.IsImplicitMultisamplingCompatible();
    });
  } else {
    use_ext = false;
  }
  // If implicit multisampling status has changed, mark all renderbuffer
  // attachments for update.
  if (use_ext != implicit_multisample_) {
    implicit_multisample_ = use_ext;
    fbo.ForEachAttachment(
        [this](const FramebufferObject::Attachment& a, int bit) -> void {
      if (a.GetBinding() == FramebufferObject::kRenderbuffer)
        SetModifiedBit(bit);
    });
  }
}

void Renderer::FramebufferResource::Update(ResourceBinder* rb) {
  if (AnyModifiedBitsSet()) {
    SCOPED_RESOURCE_LABEL;

    // Generate the FBO if necessary.
    GraphicsManager* gm = GetGraphicsManager();
    DCHECK(gm);
    if (!id_) gm->GenFramebuffers(1, &id_);
    if (id_) {
      const FramebufferObject& fbo = GetFramebufferObject();
      UpdateImplicitMultisampling(gm, fbo);
      // Bind the framebuffer.
      rb->BindFramebuffer(id_, this);
      // Update any attachments that have changed.
      for (size_t i = 0; i < color_ids_.size(); ++i) {
        const uint32 i32 = static_cast<uint32>(i);
        if (TestModifiedBit(FramebufferObject::kColorAttachmentChanged + i32) ||
            TestModifiedBit(FramebufferObject::kDimensionsChanged))
          UpdateAttachment(gm, rb, &color_ids_[i],
                           GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i),
                           fbo, fbo.GetColorAttachment(i));
      }
      // Handle packed depth stencil renderbuffers.
      if (TestModifiedBit(FramebufferObject::kDepthAttachmentChanged) ||
          TestModifiedBit(FramebufferObject::kStencilAttachmentChanged)) {
         Image::Format format = fbo.GetDepthAttachment().GetFormat();
         bool packed =
             (format == Image::kRenderbufferDepth24Stencil8 ||
              format == Image::kRenderbufferDepth32fStencil8) &&
             fbo.GetDepthAttachment() == fbo.GetStencilAttachment();
         if (packed && packed != packed_depth_stencil_) {
           // Discard previous stencil buffer, if any.
           UpdateAttachment(gm, rb, &stencil_id_, GL_STENCIL_ATTACHMENT, fbo,
                            FramebufferObject::Attachment());
         }
         packed_depth_stencil_ = packed;
      }
      if (TestModifiedBit(FramebufferObject::kDepthAttachmentChanged) ||
          TestModifiedBit(FramebufferObject::kDimensionsChanged)) {
        if (packed_depth_stencil_) {
          UpdateAttachment(gm, rb, &depth_id_, GL_DEPTH_STENCIL_ATTACHMENT, fbo,
                           fbo.GetDepthAttachment());
        } else {
          UpdateAttachment(gm, rb, &depth_id_, GL_DEPTH_ATTACHMENT, fbo,
                           fbo.GetDepthAttachment());
        }
      }
      if (!packed_depth_stencil_ &&
          (TestModifiedBit(FramebufferObject::kStencilAttachmentChanged) ||
           TestModifiedBit(FramebufferObject::kDimensionsChanged)))
        UpdateAttachment(gm, rb, &stencil_id_, GL_STENCIL_ATTACHMENT, fbo,
                         fbo.GetStencilAttachment());
      if (TestModifiedBit(FramebufferObject::kDrawBuffersChanged)) {
        GLenum buffers[kColorAttachmentSlotCount];
        GLuint num_buffers = 1;
        for (size_t i = 0; i < kColorAttachmentSlotCount; ++i) {
          const int32 buf = fbo.GetDrawBuffer(i);
          if (buf < 0) {
            // Negative values mean GL_NONE.
            buffers[i] = GL_NONE;
          } else {
            // Submit the minimum number of buffers, so that this works
            // on implementations with fewer supported buffers.
            buffers[i] = buf + GL_COLOR_ATTACHMENT0;
            num_buffers = static_cast<GLuint>(i + 1);
          }
        }
        if (gm->IsFeatureAvailable(GraphicsManager::kDrawBuffers)) {
          gm->DrawBuffers(num_buffers, buffers);
        } else if (num_buffers != 1 || (buffers[0] != GL_COLOR_ATTACHMENT0 &&
                                        buffers[0] != GL_NONE)) {
          LOG(ERROR) << "Non-default draw buffers set, but DrawBuffers "
                        "is not available!";
        }
      }
      if (TestModifiedBit(FramebufferObject::kReadBufferChanged)) {
        if (gm->IsFeatureAvailable(GraphicsManager::kReadBuffer)) {
          const int32 buffer = fbo.GetReadBuffer();
          gm->ReadBuffer(buffer < 0 ? GL_NONE : GL_COLOR_ATTACHMENT0 + buffer);
        } else if (fbo.GetReadBuffer() != 0 && fbo.GetReadBuffer() != -1) {
          LOG(ERROR) << "Non-default read buffer set, but ReadBuffer "
                        "is not available!";
        }
      }
      UpdateMemoryUsage(fbo);

      if (TestModifiedBit(ResourceHolder::kLabelChanged))
        SetObjectLabel(gm, GL_FRAMEBUFFER, id_, fbo.GetLabel());

      // Check the framebuffer status.  Note that GL_FRAMEBUFFER in this
      // context is equivalent to GL_DRAW_FRAMEBUFFER.
      const GLenum status = gm->CheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE) {
        LOG(ERROR)
            << "***ION: Multisampled framebuffer is not complete.  "
            << "This may be due to an inconsistent sample count across "
            << "attachments.  When mixing renderbuffers with textures, "
            << "be sure to set fixed_sample_locations to TRUE in all "
            << "attached textures.";
      } else if (status != GL_FRAMEBUFFER_COMPLETE) {
        TracingHelper helper;
        LOG(ERROR)
            << "***ION: Framebuffer is not complete (error code: "
            << helper.ToString("GLenum", status)
            << ")! One of the attachments might have a zero width or "
               "height or a non-drawable format for that attachment type. It "
               "is also possible that a texture attachment violates some "
               "GL-implementation specific set of constraints. Check the FBO "
               "dimensions and try changing the texture state of texture "
               "attachments (e.g. try kNearest or kLinear filtering, don't use "
               "kRepeat wrapping, etc.).";
      }
      ResetModifiedBits();
    } else {
      LOG(ERROR) << "***ION: Unable to create framebuffer object.";
    }
  }
}

void Renderer::FramebufferResource::Unbind(ResourceBinder* rb) {
  if (rb)
    rb->ClearFramebufferBinding(id_);
}

void Renderer::FramebufferResource::Release(bool can_make_gl_calls) {
  BaseResourceType::Release(can_make_gl_calls);
  if (id_ && resource_owns_gl_id_) {
    UnbindAll();
    // Delete any renderbuffers.
    if (can_make_gl_calls) {
      // This will silently ignore zeros and invalid values.
      GetGraphicsManager()->DeleteRenderbuffers(
          static_cast<GLsizei>(color_ids_.size()), color_ids_.data());
      if (depth_id_)
        GetGraphicsManager()->DeleteRenderbuffers(1, &depth_id_);
      if (stencil_id_)
        GetGraphicsManager()->DeleteRenderbuffers(1, &stencil_id_);
      // Delete the framebuffer.
      if (resource_owns_gl_id_)
        GetGraphicsManager()->DeleteFramebuffers(1, &id_);
    }
    SetUsedGpuMemory(0U);
    depth_id_ = stencil_id_ = id_ = 0;
    std::fill(color_ids_.begin(), color_ids_.end(), 0);
  }
}

void Renderer::ResourceBinder::WrapExternalResource(FramebufferObject* holder,
                                                    uint32 gl_id) {
  if (graphics_manager_->IsFramebuffer(gl_id)) {
    auto resource = GetResourceManager()->GetResource(holder, this, gl_id);
    DCHECK(resource);
    resource->ResetModifiedBits();
  }
}

//-----------------------------------------------------------------------------
//
// Renderer::VertexArrayResource class.
//
//-----------------------------------------------------------------------------

class Renderer::VertexArrayResource
    : public Resource<AttributeArray::kNumChanges> {
 public:
  VertexArrayResource(ResourceBinder* rb, ResourceManager* rm,
                      const AttributeArray& attribute_array, ResourceKey key,
                      GLuint id)
      : Renderer::Resource<AttributeArray::kNumChanges>(rm, attribute_array,
                                                        key, id),
        buffer_attribute_infos_(attribute_array.GetAllocator()),
        simple_attribute_indices_(attribute_array.GetAllocator()),
        vertex_count_(0U) {
    PopulateAttributeIndices(rb);
  }

  ~VertexArrayResource() override {
    DCHECK(id_ == 0U || !portgfx::GlContext::GetCurrent());
  }

  void Release(bool can_make_gl_calls) override;
  ResourceType GetType() const override { return kAttributeArray; }

  // Unbinds or binds and checks the buffer objects of the VAO in OpenGL.
  // BindAndCheckBuffers() returns whether the vertex array was successfully
  // bound. A call to BindAndCheckBuffers() might fail if a buffer object
  // referenced by an attributes does not have any data, or if a resource cannot
  // be created by OpenGL. Set force_bind to true to bind the array object even
  // if no shader is active; this is needed to get an array's resource
  // information.
  virtual bool BindAndCheckBuffers(bool force_bind, ResourceBinder* rb);
  void Unbind(ResourceBinder* rb) override;

  size_t GetVertexCount() const { return vertex_count_; }
  const ResourceBinder::BufferBinding& GetElementArrayBinding() const {
    return element_array_binding_;
  }
  void SetElementArrayBinding(GLuint id, BufferResource* resource) {
    element_array_binding_.gl_id = id;
    element_array_binding_.resource = resource;
  }

 protected:
  const AttributeArray& GetAttributeArray() const {
    return static_cast<const AttributeArray&>(*GetHolder());
  }

  virtual bool UpdateAndCheckBuffers(ResourceBinder* rb);

  // Binds all single-valued attributes from the associated AttributeArray.
  void BindSimpleAttributes();

  // Binds a BufferObjectElement attribute. Returns if the binding was
  // successful. Binding might fail if a buffer object contained in an Attribute
  // or its data container are nullptr, or if a resource cannot be created by
  // OpenGL. The number of slots that the attribute requires is also assigned
  // if the binding is successful.
  bool BindBufferObjectElementAttribute(GLuint attribute_index,
                                        const Attribute& a, GLuint* slots,
                                        ResourceBinder* rb);

  // Clears the vertex count. It will be updated as attributes are bound.
  void ResetVertexCount() {
    vertex_count_ = std::numeric_limits<std::size_t>::max();
  }

  void UpdateVertexCount(const Attribute& a) {
    const BufferObjectPtr& bo = a.GetValue<BufferObjectElement>().buffer_object;
    if (bo.Get()) {
      if (!a.GetDivisor() ||
          !GetGraphicsManager()->IsFeatureAvailable(
              GraphicsManager::kInstancedArrays)) {
        // Update the vertex count. We can only draw as many vertices as the
        // smallest BufferObject. Note that if we are using instanced attributes
        // we do not update the vertex_count_.
        vertex_count_ = std::min(vertex_count_, bo->GetCount());
      }
    }
  }

 protected:
  struct BufferAttributeInfo {
    BufferAttributeInfo() : index(kInvalidGluint), slots(0U), enabled(false) {}
    GLuint index;
    GLuint slots;
    bool enabled;
  };
  // Buffer attribute information.
  base::AllocVector<BufferAttributeInfo> buffer_attribute_infos_;
  // Simple attribute indices.
  base::AllocVector<GLuint> simple_attribute_indices_;

 private:
  // Prevent other classes from calling Bind() and Update(). They should use
  // BindAndCheckBuffers() and UpdateAndCheckBuffers() instead.
  virtual void Bind(ResourceBinder* rb) {
    Update(rb);
    BindAndCheckBuffers(true, rb);
  }
  void Update(ResourceBinder* rb) override {}

  // Populates the vectors of indices of buffer and simple attributes.
  void PopulateAttributeIndices(ResourceBinder* rb);

  size_t vertex_count_;
  ResourceBinder::BufferBinding element_array_binding_;

  // Allow the resource binder and manager to bind the array.
  friend class ResourceBinder;
  friend class ResourceManager;
};

void Renderer::VertexArrayResource::BindSimpleAttributes() {
  GraphicsManager* gm = GetGraphicsManager();
  const AttributeArray& aa = GetAttributeArray();
  const size_t attribute_count = aa.GetSimpleAttributeCount();
  DCHECK_EQ(attribute_count, simple_attribute_indices_.size());
  for (size_t i = 0; i < attribute_count; ++i) {
    const Attribute& a = aa.GetSimpleAttribute(i);
    DCHECK(a.IsValid());
    const GLuint& attribute_index = simple_attribute_indices_[i];
    if (attribute_index != static_cast<GLuint>(base::kInvalidIndex)) {
      switch (a.GetType()) {
        case kFloatAttribute:
          gm->VertexAttrib1fv(attribute_index, &a.GetValue<float>());
          break;
        case kFloatVector2Attribute:
          gm->VertexAttrib2fv(attribute_index,
                              a.GetValue<math::VectorBase2f>().Data());
          break;
        case kFloatVector3Attribute:
          gm->VertexAttrib3fv(attribute_index,
                              a.GetValue<math::VectorBase3f>().Data());
          break;
        case kFloatVector4Attribute:
          gm->VertexAttrib4fv(attribute_index,
                              a.GetValue<math::VectorBase4f>().Data());
          break;
        // Each column of matrix attributes must be sent separately.
        case kFloatMatrix2x2Attribute: {
          const math::Matrix2f mat =
              math::Transpose(a.GetValue<math::Matrix2f>());
          gm->VertexAttrib2fv(attribute_index, mat.Data());
          gm->VertexAttrib2fv(attribute_index + 1, &mat.Data()[2]);
          break;
        }
        case kFloatMatrix3x3Attribute: {
          const math::Matrix3f mat =
              math::Transpose(a.GetValue<math::Matrix3f>());
          gm->VertexAttrib3fv(attribute_index, mat.Data());
          gm->VertexAttrib3fv(attribute_index + 1, &mat.Data()[3]);
          gm->VertexAttrib3fv(attribute_index + 2, &mat.Data()[6]);
          break;
        }
        case kFloatMatrix4x4Attribute: {
          const math::Matrix4f mat =
              math::Transpose(a.GetValue<math::Matrix4f>());
          gm->VertexAttrib4fv(attribute_index, mat.Data());
          gm->VertexAttrib4fv(attribute_index + 1, &mat.Data()[4]);
          gm->VertexAttrib4fv(attribute_index + 2, &mat.Data()[8]);
          gm->VertexAttrib4fv(attribute_index + 3, &mat.Data()[12]);
          break;
        }
        default:
          break;
      }
    }
  }
}

bool Renderer::VertexArrayResource::BindBufferObjectElementAttribute(
    GLuint attribute_index, const Attribute& a, GLuint* slots,
    ResourceBinder* rb) {
  DCHECK(a.IsValid());
  GraphicsManager* gm = GetGraphicsManager();
  DCHECK(gm);

  // Get the BufferResource that contains the VBO and check its validity.
  const BufferObjectPtr& bo = a.GetValue<BufferObjectElement>().buffer_object;
  if (!bo.Get()) {
    // We cannot draw a shape with a nullptr buffer.
    LOG(WARNING) << "***ION: Unable to draw shape: "
                 << "BufferObject or BufferObject DataContainer is nullptr";
    return false;
  }

  BufferResource* vbo = GetResource(bo.Get(), rb);
  DCHECK(vbo);
  // Bind the buffer object.
  vbo->BindToTarget(rb, BufferObject::kArrayBuffer);

  const size_t spec_index = a.GetValue<BufferObjectElement>().spec_index;
  // OpenGL requires the offsets of the fields within a BufferObject as
  // void* pointers.
  const BufferObject::Spec& spec = bo->GetSpec(spec_index);
  DCHECK(!base::IsInvalidReference(spec));
  const GLenum type = base::EnumHelper::GetConstant(spec.type);

  // Check and issue a warning for single unsigned short type attribute that
  // requires 2-byte padding to make the vertex data 4-byte aligned, which may
  // cause problem with AMD drivers on Windows. See b/31443017 for related
  // discussion.
  if (spec.type == BufferObject::kUnsignedShort && spec.component_count == 1 &&
      (spec.byte_offset & 0x3) == 0) {
    LOG_ONCE(WARNING)
        << "***ION: Vertex attribute " << attribute_index
        << " for BufferObject " << bo->GetLabel()
        << " is a single unsigned short that needs a 2-byte padding to make the"
        << " vertex data 4-byte aligned. It has been found that this may cause"
        << " long draw calls on Windows with certain AMD drivers. If you"
        << " experience this, try changing the attribute to 2 unsigned short"
        << " components so no padding is needed. (Reporting only the first"
        << " occurrence.)";
  }

  // Matrix attributes require a pointer for each column; we assume that the
  // complete matrix is stored contiguously.
  GLuint stride = 0U;
  GetAttributeSlotCountAndStride(spec.type, &stride, slots);
  for (GLuint i = 0; i < *slots; ++i) {
    // Vertex attributes should be aligned to max(4, sizeof(type)) for best
    // performance. We do not use any 8-byte types, so 4 is sufficient.
    // See:
    //  https://www.opengl.org/wiki/Vertex_Specification_Best_Practices
    // and
    //  https://developer.apple.com/library/ios/documentation/3DDrawing/
    //   Conceptual/OpenGLES_ProgrammingGuide/TechniquesforWorkingwithVertexData
    //   /TechniquesforWorkingwithVertexData.html
    if (((spec.byte_offset + i * stride) & 0x3) ||
        (bo->GetStructSize() & 0x3)) {
      LOG_ONCE(WARNING)
          << "***ION: Vertex attribute " << attribute_index
          << " for BufferObject " << bo->GetLabel()
          << " is not 4-byte aligned. This may reduce performance."
          << " (Reporting only the first occurrence.)";
    }
    gm->VertexAttribPointer(
        attribute_index + i, static_cast<GLuint>(spec.component_count), type,
        a.IsFixedPointNormalized() ? GL_TRUE : GL_FALSE,
        static_cast<GLuint>(bo->GetStructSize()),
        reinterpret_cast<const void*>(spec.byte_offset + i * stride));
    if (gm->IsFeatureAvailable(GraphicsManager::kInstancedArrays)) {
      gm->VertexAttribDivisor(attribute_index + i, a.GetDivisor());
    }
  }
  return true;
}

void Renderer::VertexArrayResource::PopulateAttributeIndices(
    ResourceBinder* rb) {
  // Retrieve the attribute indices from the currently bound shader. Since
  // each VertexArrayResource is tied to a particular shader, we only need
  // to perform this check once.
  const AttributeArray& aa = GetAttributeArray();
  const size_t buffer_attribute_count = aa.GetBufferAttributeCount();
  const size_t simple_attribute_count = aa.GetSimpleAttributeCount();
  buffer_attribute_infos_.resize(buffer_attribute_count);
  simple_attribute_indices_.resize(simple_attribute_count, kInvalidGluint);

  if (ShaderProgramResource* spr = rb->GetActiveShaderProgram()) {
    DCHECK(spr);
    for (size_t i = 0; i < buffer_attribute_count; ++i) {
      const Attribute& a = aa.GetBufferAttribute(i);
      DCHECK(a.IsValid());
      const ShaderInputRegistry::AttributeSpec* spec =
          ShaderInputRegistry::GetSpec(a);
      const GLint index = spr->GetAttributeIndex(spec);
      if (index >= 0) {
        buffer_attribute_infos_[i].index = static_cast<GLuint>(index);
      } else if (aa.IsBufferAttributeEnabled(i)) {
        LOG_ONCE(WARNING)
            << "***ION: Attribute array contains buffer attribute '"
            << spec->name << "' but the current shader program '"
            << spr->GetShaderProgram().GetLabel() << "' does not"
            << " declare or use it";
      }
    }

    for (size_t i = 0; i < simple_attribute_count; ++i) {
      const Attribute& a = aa.GetSimpleAttribute(i);
      DCHECK(a.IsValid());
      const ShaderInputRegistry::AttributeSpec* spec =
          ShaderInputRegistry::GetSpec(a);
      const GLint index = spr->GetAttributeIndex(spec);
      if (index >= 0) {
        simple_attribute_indices_[i] = static_cast<GLuint>(index);
      } else {
        LOG_ONCE(WARNING)
            << "***ION: Attribute array contains simple attribute '"
            << spec->name << "' but the current shader program '"
            << spr->GetShaderProgram().GetLabel() << "' does not"
            << " declare or use it";
      }
    }
  }
}

bool Renderer::VertexArrayResource::UpdateAndCheckBuffers(ResourceBinder* rb) {
  if (AnyModifiedBitsSet()) {
    SCOPED_RESOURCE_LABEL;
    // Generate the VAO.
    GraphicsManager* gm = GetGraphicsManager();
    DCHECK(gm);
    if (!id_) gm->GenVertexArrays(1, &id_);

    const AttributeArray& aa = GetAttributeArray();
    DCHECK_EQ(aa.GetBufferAttributeCount(), buffer_attribute_infos_.size());
    DCHECK_EQ(aa.GetSimpleAttributeCount(), simple_attribute_indices_.size());

    ResetVertexCount();

    if (id_) {
      // If the resource was changed elsewhere then we need to ensure that it is
      // bound again.
      if (TestModifiedBit(ResourceHolder::kResourceChanged))
        rb->ClearVertexArrayBinding(id_);

      rb->BindVertexArray(id_, this);
      // Determine which attributes have changed, and rebind them.
      const size_t buffer_attribute_count = aa.GetBufferAttributeCount();
      DCHECK_EQ(buffer_attribute_count, buffer_attribute_infos_.size());
      for (size_t i = 0; i < buffer_attribute_count; ++i) {
        const Attribute& a = aa.GetBufferAttribute(i);
        UpdateVertexCount(a);
        BufferAttributeInfo& info = buffer_attribute_infos_[i];
        if (info.index != static_cast<GLuint>(base::kInvalidIndex)) {
          if (TestModifiedBit(
                  static_cast<int>(AttributeArray::kAttributeChanged + i)) &&
              !BindBufferObjectElementAttribute(info.index, a, &info.slots, rb))
            return false;
          // Update the enabled state if it has changed.
          if (TestModifiedBit(static_cast<int>(
                  AttributeArray::kAttributeEnabledChanged + i))) {
            DCHECK_GT(info.slots, 0U);
            if (aa.IsBufferAttributeEnabled(i)) {
              for (GLuint j = 0; j < info.slots; ++j)
                gm->EnableVertexAttribArray(info.index + j);
              info.enabled = true;
            } else {
              for (GLuint j = 0; j < info.slots; ++j)
                gm->DisableVertexAttribArray(info.index + j);
              info.enabled = false;
            }
          }
        }
      }

      if (TestModifiedBit(ResourceHolder::kLabelChanged))
        SetObjectLabel(gm, GL_VERTEX_ARRAY_OBJECT_EXT, id_, aa.GetLabel());

      ResetModifiedBits();
    } else {
      LOG(ERROR) << "***ION: Unable to create vertex array";
      return false;
    }
  }
  // Simple attributes must always be bound since their state is not saved
  // in the VAO.
  if (GetAttributeArray().GetSimpleAttributeCount()) {
    rb->BindVertexArray(id_, this);
    BindSimpleAttributes();
  }
  return true;
}

bool Renderer::VertexArrayResource::BindAndCheckBuffers(bool force_bind,
                                                        ResourceBinder* rb) {
  // Since UpdateAndCheckBuffers() has side-effects (it may bind the VAO and
  // update internal state), it must come first in the below statement.
  if ((UpdateAndCheckBuffers(rb) || force_bind) && id_) {
    SCOPED_RESOURCE_LABEL;
    rb->BindVertexArray(id_, this);
    return true;
  }
  return false;
}

void Renderer::VertexArrayResource::Unbind(ResourceBinder* rb) {
  if (rb)
    rb->ClearVertexArrayBinding(id_);
}

void Renderer::VertexArrayResource::Release(bool can_make_gl_calls) {
  BaseResourceType::Release(can_make_gl_calls);
  if (id_) {
    UnbindAll();
    if (resource_owns_gl_id_ && can_make_gl_calls)
      GetGraphicsManager()->DeleteVertexArrays(1, &id_);
    id_ = 0;
  }
}

//-----------------------------------------------------------------------------
//
// Renderer::VertexArrayResource class.
//
//-----------------------------------------------------------------------------

class Renderer::VertexArrayEmulatorResource
    : public Renderer::VertexArrayResource {
 public:
  VertexArrayEmulatorResource(ResourceBinder* rb, ResourceManager* rm,
                              const AttributeArray& attribute_array,
                              ResourceKey key, GLuint id)
      : VertexArrayResource(rb, rm, attribute_array, key, id),
        sorted_buffer_indices_(attribute_array.GetAllocator()) {}
  ~VertexArrayEmulatorResource() override {}

  void Release(bool can_make_gl_calls) override;
  bool UpdateAndCheckBuffers(ResourceBinder* rb) override;

  // See comments in VertexArrayResource.
  bool BindAndCheckBuffers(bool force_bind, ResourceBinder* rb) override;
  void Unbind(ResourceBinder* rb) override;

 private:
  typedef base::InlinedAllocVector<GLuint, kAttributeSlotCount> IndexVector;
  // Sorted vector of indices that contain buffer attributes.
  IndexVector sorted_buffer_indices_;
};

bool Renderer::VertexArrayEmulatorResource::UpdateAndCheckBuffers(
    ResourceBinder* rb) {
  // Only resend the vertex array state if this is not the currently bound
  // resource.
  VertexArrayEmulatorResource* last_array =
      static_cast<VertexArrayEmulatorResource*>(rb->GetActiveVertexArray());
  if (last_array != this || AnyModifiedBitsSet()) {
    ResetModifiedBits();
    GraphicsManager* gm = GetGraphicsManager();
    const AttributeArray& aa = GetAttributeArray();

    SCOPED_RESOURCE_LABEL;
    rb->SetActiveVertexArray(this);
    BindSimpleAttributes();
    ResetVertexCount();

    // Since we don't actually have real vertex arrays, we have to bind each
    // attribute pointer every time.
    const size_t buffer_attribute_count = aa.GetBufferAttributeCount();
    DCHECK_EQ(buffer_attribute_count, buffer_attribute_infos_.size());
    sorted_buffer_indices_.clear();
    for (size_t i = 0; i < buffer_attribute_count; ++i) {
      BufferAttributeInfo& info = buffer_attribute_infos_[i];
      if (aa.IsBufferAttributeEnabled(i)) {
        const Attribute& a = aa.GetBufferAttribute(i);
        UpdateVertexCount(a);
        if (info.index != static_cast<GLuint>(base::kInvalidIndex)) {
          if (!BindBufferObjectElementAttribute(info.index, a, &info.slots, rb))
            return false;
          for (GLuint j = 0; j < info.slots; ++j)
            sorted_buffer_indices_.push_back(info.index + j);
        }
      }
    }
    std::sort(sorted_buffer_indices_.begin(), sorted_buffer_indices_.end());
    if (last_array) {
      IndexVector last_indices(aa.GetAllocator(),
                               last_array->sorted_buffer_indices_.begin(),
                               last_array->sorted_buffer_indices_.end());
      IndexVector result(aa.GetAllocator());
      // Indices with buffer attributes in the current array that were
      // previously simple attributes or undefined should be enabled.
      std::set_difference(sorted_buffer_indices_.begin(),
                          sorted_buffer_indices_.end(),
                          last_indices.begin(), last_indices.end(),
                          std::back_inserter(result));
      for (GLuint index : result)
        gm->EnableVertexAttribArray(index);
      result.clear();
      // Indices that are no longer buffer attributes in the current array
      // should be disabled.
      std::set_difference(last_indices.begin(), last_indices.end(),
                          sorted_buffer_indices_.begin(),
                          sorted_buffer_indices_.end(),
                          std::back_inserter(result));
      for (GLuint index : result)
        gm->DisableVertexAttribArray(index);
    } else {
      for (GLuint index : sorted_buffer_indices_)
        gm->EnableVertexAttribArray(index);
    }
  }
  return true;
}

bool Renderer::VertexArrayEmulatorResource::BindAndCheckBuffers(
    bool force_bind, ResourceBinder* rb) {
  return UpdateAndCheckBuffers(rb);
}

void Renderer::VertexArrayEmulatorResource::Unbind(ResourceBinder* rb) {
  const bool can_make_gl_calls = GetResourceManager()->AreResourcesAccessible();
  if (rb && rb->GetActiveVertexArray() == this) {
    GraphicsManager* gm = GetGraphicsManager();
    // Since we don't actually have real vertex arrays, we have to disable each
    // attribute pointer every time so that we don't pollute the global state.
    const size_t buffer_attribute_count = buffer_attribute_infos_.size();
    for (size_t i = 0; i < buffer_attribute_count; ++i) {
      BufferAttributeInfo& info = buffer_attribute_infos_[i];
      if (info.enabled &&
          info.index != static_cast<GLuint>(base::kInvalidIndex)) {
        if (can_make_gl_calls) {
          for (GLuint j = 0; j < info.slots; ++j)
            gm->DisableVertexAttribArray(info.index + j);
        }
        info.enabled = false;
      }
    }
    rb->SetActiveVertexArray(nullptr);
  }
}

void Renderer::VertexArrayEmulatorResource::Release(bool can_make_gl_calls) {
  UnbindAll();
  BaseResourceType::Release(can_make_gl_calls);
}

//-----------------------------------------------------------------------------
//
// Renderer class.
//
//-----------------------------------------------------------------------------

Renderer::Renderer(const GraphicsManagerPtr& gm)
    : flags_(AllProcessFlags()),
      resource_manager_(new (GetAllocator()) ResourceManager(gm)),
      gl_context_change_policy_(kAbort) {
  DCHECK(gm.Get());

  // Create the default shader program and default global uniform settings.
  default_shader_ =
      CreateDefaultShaderProgram(GetAllocatorForLifetime(base::kLongTerm));
}

Renderer::~Renderer() {
  if (gl_context_change_policy_ == kIgnore) {
    ClearAllResources(true);
  }
  uintptr_t context_id;
  ResourceBinder* resource_binder = GetInternalResourceBinder(&context_id);
  // Legacy behavior: if the GL context is NULL on destruction, always abandon
  // resources instead of aborting, no matter what context change policy is set.
  if (context_id == 0) {
    LOG(WARNING) << "***ION: renderer.cc: ~Renderer: No GlContext ID "
                    "(GL context might have been already destroyed)";
  } else {
    CheckContextChange();
  }
  if (resource_binder)
    resource_binder->SetCurrentFramebuffer(FramebufferObjectPtr());
}

const Renderer::Flags& Renderer::AllFlags() {
  static const Flags flags(AllClearFlags() | AllInvalidateFlags() |
                           AllProcessFlags() | AllRestoreFlags() |
                           AllSaveFlags());
  return flags;
}

const Renderer::Flags& Renderer::AllClearFlags() {
  static const Flags flags((1ULL << kClearActiveTexture) |
                           (1ULL << kClearArrayBuffer) |
                           (1ULL << kClearCubemaps) |
                           (1ULL << kClearElementArrayBuffer) |
                           (1ULL << kClearFramebuffer) |
                           (1ULL << kClearShaderProgram) |
                           (1ULL << kClearSamplers) |
                           (1ULL << kClearTextures) |
                           (1ULL << kClearVertexArray));
  return flags;
}

const Renderer::Flags& Renderer::AllInvalidateFlags() {
  static const Flags flags(GetInvalidateColorFlags() |
                           (1ULL << kInvalidateDepthAttachment) |
                           (1ULL << kInvalidateStencilAttachment));
  return flags;
}

const Renderer::Flags& Renderer::AllProcessFlags() {
  static const Flags flags((1ULL << kProcessInfoRequests) |
                           (1ULL << kProcessReleases));
  return flags;
}

const Renderer::Flags& Renderer::AllRestoreFlags() {
  static const Flags flags((1ULL << kRestoreActiveTexture) |
                           (1ULL << kRestoreArrayBuffer) |
                           (1ULL << kRestoreElementArrayBuffer) |
                           (1ULL << kRestoreFramebuffer) |
                           (1ULL << kRestoreShaderProgram) |
                           (1ULL << kRestoreStateTable) |
                           (1ULL << kRestoreVertexArray));
  return flags;
}

const Renderer::Flags& Renderer::AllSaveFlags() {
  static const Flags flags((1ULL << kSaveActiveTexture) |
                           (1ULL << kSaveArrayBuffer) |
                           (1ULL << kSaveElementArrayBuffer) |
                           (1ULL << kSaveFramebuffer) |
                           (1ULL << kSaveShaderProgram) |
                           (1ULL << kSaveStateTable) |
                           (1ULL << kSaveVertexArray));
  return flags;
}

const GraphicsManagerPtr& Renderer::GetGraphicsManager() const {
  return resource_manager_->GetGraphicsManager();
}

gfx::ResourceManager* Renderer::GetResourceManager() const {
  return resource_manager_.get();
}

Renderer::ResourceBinderMap& Renderer::GetResourceBinderMap() {
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
      ResourceBinderMap, binders,
      new ResourceBinderMap(
          base::AllocationManager::GetDefaultAllocatorForLifetime(
              base::kLongTerm)));
  return *binders;
}

void Renderer::DestroyStateCache(const portgfx::GlContextPtr& gl_context) {
  if (gl_context) {
    const size_t id = gl_context->GetId();
    ResourceBinderMap& binders = GetResourceBinderMap();
    base::WriteLock write_lock(GetResourceBinderLock());
    base::WriteGuard write_guard(&write_lock);
    binders.erase(id);
  }
}

void Renderer::DestroyCurrentStateCache() {
  const uintptr_t id = portgfx::GlContext::GetCurrentId();
  ResourceBinderMap& binders = GetResourceBinderMap();
  base::WriteLock write_lock(GetResourceBinderLock());
  base::WriteGuard write_guard(&write_lock);
  binders.erase(id);
}

Renderer::ResourceBinder* Renderer::GetInternalResourceBinder(
    uintptr_t* context_id) const {
  base::ReadLock read_lock(GetResourceBinderLock());
  base::ReadGuard read_guard(&read_lock);
  *context_id = portgfx::GlContext::GetCurrentId();
  ResourceBinderMap& binders = GetResourceBinderMap();
  ResourceBinderMap::iterator it = binders.find(*context_id);

  if (it == binders.end())
    return nullptr;

  ResourceBinder* rb = it->second.get();
  DCHECK(rb);
  rb->SetResourceManager(resource_manager_.get());
  return rb;
}

Renderer::ResourceBinder*
Renderer::GetOrCreateInternalResourceBinder(int line) const {
  uintptr_t context_id = 0;
  ResourceBinder* rb = GetInternalResourceBinder(&context_id);
  if (context_id == 0) {
    LOG(WARNING) << "***ION: renderer.cc:" << line
                 << ": No GlContext ID (invalid GL context?)";
    DCHECK(!rb) << "Unexpected ResourceBinder for GlContext 0.";
    return nullptr;
  }
  if (!rb) {
    ResourceBinderMap& binders = GetResourceBinderMap();
    rb = new(GetAllocator()) ResourceBinder(GetGraphicsManager());
    base::WriteLock write_lock(GetResourceBinderLock());
    base::WriteGuard write_guard(&write_lock);
    DCHECK(binders.find(context_id) == binders.end())
        << "Two threads tried to create ResourceBinders for the same "
           "GlContext!";
    binders[context_id].reset(rb);
  }
  DCHECK(rb);
  rb->SetResourceManager(resource_manager_.get());
  CheckContextChange();
  return rb;
}

void Renderer::CheckContextChange() const {
  if (!resource_manager_->AreResourcesAccessible()) {
    switch (gl_context_change_policy_) {
      case kAbandonResources:
        resource_manager_->DestroyOrAbandonAllResources(true);
        break;
      case kAbort:
        LOG(FATAL) <<
          "OpenGL context has changed and the Renderer's GL resources are no "
          "longer accessible; aborting.\n"
          "If your application is crashing here, the OpenGL context is being "
          "changed (either by you or by the system), but you are reusing the "
          "same Renderer.  Since reusing a Renderer on a different non-shared "
          "OpenGL context requires re-creating the GL resources and we don't "
          "know what to do with the old ones, the only safe thing to do is to "
          "abort the program.  To fix this crash, do one of the following:\n"
          "a) If you are using Android's GLSurfaceView and have no idea what "
             "any of this means, or if you are sure that the old context will "
             "be or already has been destroyed, call:\n"
             "SetContextChangePolicy(Renderer::kAbandonResources)\n"
             "after constructing your renderer.\n"
          "b) If you are switching between different, non-shared OpenGL "
             "contexts, you should use a separate Renderer for each context.\n"
          "c) If you are using a single Renderer with shared contexts, but are "
             "still getting this crash, it means you are creating the shared "
             "contexts outside of Ion.  On most platforms, share group "
             "information cannot be retrieved after context creation, so "
             "contexts created outside Ion are always considered non-shared.  "
             "Use portgfx::GlContext::CreateGlContextInCurrentShareGroup() to "
             "create your contexts to fix this problem.";
        break;
      case kIgnore:
        break;
      default:
        DCHECK(false) << "Unknown context change policy";
    }
  }
}

void Renderer::BindFramebuffer(const FramebufferObjectPtr& fbo) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);

  if (resource_binder) {
    resource_binder->ClearNonFramebufferCachedBindings();
    if (!fbo.Get() || fbo->GetWidth() == 0 || fbo->GetHeight() == 0) {
      resource_binder->BindFramebuffer(
          *resource_binder->GetSavedId(kSaveFramebuffer), nullptr);
    } else {
      FramebufferResource* fbr =
          resource_manager_->GetResource(fbo.Get(), resource_binder);
      DCHECK(fbr);
      fbr->Bind(resource_binder);
    }
    resource_binder->SetCurrentFramebuffer(fbo);

    // If we use Ion only for framebuffer management and never call DrawScene(),
    // we will accumulate unreleased resources over time. To prevent this,
    // release resources here.
    if (flags_.test(kProcessReleases))
      resource_manager_->ProcessReleases(resource_binder);
  }
}

const FramebufferObjectPtr Renderer::GetCurrentFramebuffer() const {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
    return resource_binder ?
        resource_binder->GetCurrentFramebuffer() : FramebufferObjectPtr();
}

FramebufferObjectPtr Renderer::CreateExternalFramebufferProxy(
    const ion::math::Range2i::Size& size,
    ion::gfx::Image::Format color_format,
    ion::gfx::Image::Format depth_format,
    int num_samples) {
  ion::gfx::SamplerPtr sampler(new ion::gfx::Sampler);
  sampler->SetMagFilter(ion::gfx::Sampler::kLinear);
  sampler->SetMinFilter(ion::gfx::Sampler::kLinear);
  sampler->SetWrapS(ion::gfx::Sampler::kClampToEdge);
  sampler->SetWrapT(ion::gfx::Sampler::kClampToEdge);
  const GLuint uninitialized = static_cast<GLuint>(-1);
  GLuint fboid = uninitialized;
  auto graphics_manager = GetGraphicsManager();
  graphics_manager->GetIntegerv(GL_FRAMEBUFFER_BINDING,
                                reinterpret_cast<GLint*>(&fboid));
  CHECK(fboid != 0U) << "Cannot create proxy for the default framebuffer.";
  if (fboid == uninitialized) {
    TracingHelper helper;
    GLenum error = graphics_manager->GetError();
    LOG(FATAL) << "Cannot create framebuffer proxy because "
        "GetInteger has failed: " << helper.ToString("GLenum", error);
  }
  // We wish to know when GetFramebufferAttachmentParameteriv fails to execute,
  // so initialize the attachment type to something that is neither GL_NONE nor
  // GL_RENDERBUFFER, nor GL_TEXTURE.
  GLuint atype = uninitialized;
  graphics_manager->GetFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, reinterpret_cast<GLint*>(&atype));
  if (atype == uninitialized) {
    TracingHelper helper;
    GLenum error = graphics_manager->GetError();
    LOG(FATAL) << "Cannot create framebuffer proxy because "
        "GetFramebufferAttachmentParameter has failed: " <<
        helper.ToString("GLenum", error);
  }
  CHECK(atype != GL_NONE) << "Cannot create proxy from a framebuffer object "
      "that is missing color attachment 0.";
  CHECK(atype != GL_RENDERBUFFER) << "Framebuffer proxies do not yet support "
      "renderbuffer color attachments.";
  CHECK(atype == GL_TEXTURE) << "Non-texture attachments are not supported.";
  GLuint texid;
  graphics_manager->GetFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, reinterpret_cast<GLint*>(&texid));
  ion::gfx::FramebufferObjectPtr fbo(
      new ion::gfx::FramebufferObject(size[0], size[1]));
  ion::gfx::TexturePtr color_texture(new ion::gfx::Texture);
  color_texture->SetLabel("Offscreen Color Texture");
  color_texture->SetSampler(sampler);
  ion::gfx::ImagePtr color_image(new ion::gfx::Image);
  // Calling Set instead of SetEglImage is wrong in some cases, but this FBO is
  // just a proxy object, so it doesn't matter.
  color_image->Set(color_format, size[0], size[1],
                   ion::base::DataContainerPtr());
  color_texture->SetImage(0U, color_image);
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  resource_binder->WrapExternalResource(color_texture.Get(), texid);
  if (num_samples > 1) {
    color_texture->SetMultisampling(num_samples, true);
  }
  fbo->SetColorAttachment(
      0U, ion::gfx::FramebufferObject::Attachment(color_texture));
  fbo->SetDepthAttachment(
      ion::gfx::FramebufferObject::Attachment(depth_format));
  resource_binder->WrapExternalResource(fbo.Get(), fboid);
  return fbo;
}

void Renderer::UpdateDefaultFramebufferFromOpenGL() {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->UpdateDefaultFramebufferFromOpenGL();
}

void Renderer::ClearCachedBindings() {
  // Not used, but the function expects a non-null parameter.
  uintptr_t context_id;
  ResourceBinder* resource_binder = GetInternalResourceBinder(&context_id);
  if (resource_binder) {
    resource_binder->ClearNonFramebufferCachedBindings();
    resource_binder->ClearFramebufferBinding(0U);
  }
}

template <typename T>
ION_API void Renderer::BindResource(T* holder) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder) {
    resource_binder->BindResource(holder);
    if (flags_.test(kProcessReleases))
      resource_manager_->ProcessReleases(resource_binder);
  }
}

// Explicitly instantiate.
template ION_API void Renderer::BindResource<BufferObject>(
    BufferObject*);  // NOLINT
template ION_API void Renderer::BindResource<CubeMapTexture>(
    CubeMapTexture*);  // NOLINT
template ION_API void Renderer::BindResource<FramebufferObject>(
    FramebufferObject*);  // NOLINT
template ION_API void Renderer::BindResource<IndexBuffer>(
    IndexBuffer*);                                                // NOLINT
template ION_API void Renderer::BindResource<Sampler>(Sampler*);  // NOLINT
template ION_API void Renderer::BindResource<ShaderProgram>(
    ShaderProgram*);                                              // NOLINT
template ION_API void Renderer::BindResource<Texture>(Texture*);  // NOLINT

template <typename T>
ION_API void Renderer::CreateOrUpdateResource(T* holder) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->Process<ResourceBinder::CreateOrUpdateOp>(holder, 0U);
}

// Explicitly instantiate.
template ION_API void Renderer::CreateOrUpdateResource<AttributeArray>(
    AttributeArray*);  // NOLINT
template ION_API void Renderer::CreateOrUpdateResource<BufferObject>(
    BufferObject*);  // NOLINT
template ION_API void Renderer::CreateOrUpdateResource<CubeMapTexture>(
    CubeMapTexture*);  // NOLINT
template ION_API void Renderer::CreateOrUpdateResource<FramebufferObject>(
    FramebufferObject*);  // NOLINT
template ION_API void Renderer::CreateOrUpdateResource<IndexBuffer>(
    IndexBuffer*);  // NOLINT
template ION_API void Renderer::CreateOrUpdateResource<Sampler>(
    Sampler*);  // NOLINT
template ION_API void Renderer::CreateOrUpdateResource<ShaderInputRegistry>(
    ShaderInputRegistry*);  // NOLINT
template ION_API void Renderer::CreateOrUpdateResource<ShaderProgram>(
    ShaderProgram*);  // NOLINT
template ION_API void Renderer::CreateOrUpdateResource<Texture>(
    Texture*);  // NOLINT

void Renderer::ProcessStateTable(const StateTablePtr& state_table) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->ProcessStateTable(state_table);
}

template <>
ION_API void Renderer::CreateResourceWithExternallyManagedId<BufferObject>(
    BufferObject* holder, uint32 gl_id) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder && resource_binder->GetGraphicsManager()->IsBuffer(gl_id))
    resource_binder->Process<ResourceBinder::CreateOrUpdateOp>(holder, gl_id);
}

template <>
ION_API void Renderer::CreateResourceWithExternallyManagedId<IndexBuffer>(
    IndexBuffer* holder, uint32 gl_id) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder && resource_binder->GetGraphicsManager()->IsBuffer(gl_id))
    resource_binder->Process<ResourceBinder::CreateOrUpdateOp>(holder, gl_id);
}

template <>
ION_API void Renderer::CreateResourceWithExternallyManagedId<Texture>(
    Texture* holder, uint32 gl_id) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder &&
      resource_binder->GetGraphicsManager()->IsTexture(gl_id))
    resource_binder->Process<ResourceBinder::CreateOrUpdateOp>(holder, gl_id);
}

template <>
ION_API void Renderer::CreateResourceWithExternallyManagedId<CubeMapTexture>(
    CubeMapTexture* holder, uint32 gl_id) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder &&
      resource_binder->GetGraphicsManager()->IsTexture(gl_id))
    resource_binder->Process<ResourceBinder::CreateOrUpdateOp>(holder, gl_id);
}

void Renderer::CreateOrUpdateResources(const NodePtr& node) {
  if (!node.Get() || !node->IsEnabled())
    return;

  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder) {
    resource_binder->Traverse<ResourceBinder::CreateOrUpdateOp>(node,
        default_shader_.Get());
    if (flags_.test(kProcessReleases))
      resource_manager_->ProcessReleases(resource_binder);
  }
}

template <typename Operation>
void Renderer::ResourceBinder::Traverse(const NodePtr& node,
                                        ShaderProgram* default_shader) {
  // We need to keep track of the active shader program in this method,
  // since the GetResourceKey method of vertex array resources depends
  // on the current active program (each program needs its own VAO).
  ShaderProgram* saved_current_program = current_shader_program_;
  ShaderProgramResource* saved_active_resource = active_shader_.resource;
  current_shader_program_ = default_shader;

  Visit<Operation>(node);

  current_shader_program_ = saved_current_program;
  active_shader_.resource = saved_active_resource;
}

template <typename Operation>
void Renderer::ResourceBinder::Visit(const NodePtr& node) {
  if (!node.Get() || !node->IsEnabled())
    return;

  // Get the Node's shader, if any, and update it.
  if (ShaderProgram* shader = node->GetShaderProgram().Get()) {
    Process<Operation>(shader, 0U);
    current_shader_program_ = shader;
  }

  // This is required for the correct operation of GetVertexArrayKey() during
  // update operations. The actual cached shader binding will be restored by the
  // Traverse() function.
  active_shader_.resource = resource_manager_->GetResource(
      current_shader_program_, this);

  // Upload any textures in the node.
  const base::AllocVector<Uniform>& uniforms = node->GetUniforms();
  const size_t num_uniforms = uniforms.size();
  for (size_t i = 0; i < num_uniforms; ++i) {
    Process<Operation>(&uniforms[i].GetRegistry(), 0U);
    if (uniforms[i].GetType() == kTextureUniform)
      Process<Operation>(uniforms[i].GetValue<TexturePtr>().Get(), 0U);
    else if (uniforms[i].GetType() == kCubeMapTextureUniform)
      Process<Operation>(uniforms[i].GetValue<CubeMapTexturePtr>().Get(), 0U);
  }

  // Textures might be in Uniforms in a UniformBlock.
  const base::AllocVector<UniformBlockPtr>& uniform_blocks =
      node->GetUniformBlocks();
  const size_t num_uniform_blocks = uniform_blocks.size();
  for (size_t i = 0; i < num_uniform_blocks; ++i) {
    if (uniform_blocks[i]->IsEnabled()) {
      const base::AllocVector<Uniform>& uniforms =
          uniform_blocks[i]->GetUniforms();
      const size_t num_uniforms = uniforms.size();
      for (size_t i = 0; i < num_uniforms; ++i) {
        Process<Operation>(&uniforms[i].GetRegistry(), 0U);
        if (uniforms[i].GetType() == kTextureUniform)
          Process<Operation>(uniforms[i].GetValue<TexturePtr>().Get(), 0U);
        else if (uniforms[i].GetType() == kCubeMapTextureUniform)
          Process<Operation>(
              uniforms[i].GetValue<CubeMapTexturePtr>().Get(), 0U);
      }
    }
  }

  // Draw shapes.
  const base::AllocVector<ShapePtr>& shapes = node->GetShapes();
  const size_t num_shapes = shapes.size();
  for (size_t i = 0; i < num_shapes; ++i)
    VisitShape<Operation>(shapes[i]);

  ShaderProgram* saved_program = current_shader_program_;

  // Process any children.
  const base::AllocVector<NodePtr>& children = node->GetChildren();
  const size_t num_children = children.size();
  for (size_t i = 0; i < num_children; ++i) {
    Visit<Operation>(children[i]);
    current_shader_program_ = saved_program;
  }
}

template <typename Operation>
void Renderer::ResourceBinder::VisitShape(const ShapePtr& shape) {
  Process<Operation>(shape->GetIndexBuffer().Get(), 0U);
  Process<Operation>(shape->GetAttributeArray().Get(), 0U);
}

void Renderer::CreateOrUpdateShapeResources(const ShapePtr& shape) {
  if (shape.Get()) {
    ResourceBinder* resource_binder =
        GetOrCreateInternalResourceBinder(__LINE__);
    if (resource_binder) {
      resource_binder->VisitShape<ResourceBinder::CreateOrUpdateOp>(shape);
      if (flags_.test(kProcessReleases))
        resource_manager_->ProcessReleases(resource_binder);
    }
  }
}

template <typename T>
ION_API void Renderer::RequestForcedUpdate(T* holder) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->Process<ResourceBinder::RequestUpdateOp>(holder, 0U);
}

// Explicitly instantiate.
template ION_API void Renderer::RequestForcedUpdate<AttributeArray>(
    AttributeArray*);  // NOLINT
template ION_API void Renderer::RequestForcedUpdate<BufferObject>(
    BufferObject*);  // NOLINT
template ION_API void Renderer::RequestForcedUpdate<CubeMapTexture>(
    CubeMapTexture*);  // NOLINT
template ION_API void Renderer::RequestForcedUpdate<FramebufferObject>(
    FramebufferObject*);  // NOLINT
template ION_API void Renderer::RequestForcedUpdate<IndexBuffer>(
    IndexBuffer*);  // NOLINT
template ION_API void Renderer::RequestForcedUpdate<Sampler>(
    Sampler*);  // NOLINT
template ION_API void Renderer::RequestForcedUpdate<ShaderProgram>(
    ShaderProgram*);  // NOLINT
template ION_API void Renderer::RequestForcedUpdate<ShaderInputRegistry>(
    ShaderInputRegistry*);  // NOLINT
template ION_API void Renderer::RequestForcedUpdate<Texture>(
    Texture*);  // NOLINT

void Renderer::RequestForcedUpdates(const NodePtr& node) {
  if (!node.Get() || !node->IsEnabled())
    return;

  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->Traverse<ResourceBinder::RequestUpdateOp>(node,
        default_shader_.Get());
}

void Renderer::RequestForcedShapeUpdates(const ShapePtr& shape) {
  if (shape.Get()) {
    ResourceBinder* resource_binder =
        GetOrCreateInternalResourceBinder(__LINE__);
    if (resource_binder) {
      resource_binder->VisitShape<ResourceBinder::RequestUpdateOp>(shape);
    }
  }
}

void Renderer::ResourceBinder::ProcessStateTable(
    const StateTablePtr& state_table) {
  if (const StateTable* st = state_table.Get()) {
    GraphicsManager* gm = GetGraphicsManager().Get();

    // Merge the node's state table into the client state.
    client_state_table_->MergeValuesFrom(*st, *st);
    ClearFromStateTable(*st, gl_state_table_.Get(), gm);
    UpdateFromStateTable(*st, gl_state_table_.Get(), gm);
    // Update our copy of OpenGL's state.
    gl_state_table_->MergeNonClearValuesFrom(*st, *st);
  }
}

void Renderer::SetInitialUniformValue(const Uniform& u) {
  if (u.IsValid()) {
    ResourceBinder* resource_binder =
        GetOrCreateInternalResourceBinder(__LINE__);
    if (resource_binder) {
      ShaderInputRegistryResource* sirr = resource_manager_->GetResource(
          &u.GetRegistry(), resource_binder);
      sirr->Update(resource_binder);
      sirr->SetInitialValue(u);
    }
  }
}

void Renderer::ResolveMultisampleFramebuffer(
    const FramebufferObjectPtr& ms_fbo,
    const FramebufferObjectPtr& dest_fbo,
    uint32_t mask) {
  uint32_t all_buffer_bits =
      kColorBufferBit | kDepthBufferBit | kStencilBufferBit;
  if (mask == 0) {
    return;
  } else if ((mask & ~all_buffer_bits) != 0) {
    LOG(ERROR) << "Invalid mask argument. Must be a combination of "
               << "kColorBufferBit, kDepthBufferBit and kStencilBufferBit.";
    return;
  }
  const GraphicsManagerPtr& gm = GetGraphicsManager();

  // Check whether related functions are available.
  if (!gm->IsFeatureAvailable(GraphicsManager::kFramebufferBlit) &&
      !gm->IsFeatureAvailable(
          GraphicsManager::kMultisampleFramebufferResolve)) {
    LOG(WARNING) << "No multisampled framebuffer functions available.";
    return;
  }

  // Save the currently bound framebuffer.
  const FramebufferObjectPtr previous_fbo = GetCurrentFramebuffer();

  // Update and bind framebuffers. Note that this may include updating the draw
  // and read buffers, so we need to bind to GL_FRAMEBUFFER.
  BindFramebuffer(ms_fbo);
  BindFramebuffer(dest_fbo);

  // Bind the multisample framebuffer to the read target.
  GLuint ms_fbo_id = GetResourceGlId(ms_fbo.Get());
  gm->BindFramebuffer(GL_READ_FRAMEBUFFER, ms_fbo_id);

  // Resolve the multisampled FBO.
  GLuint gl_mask = ((mask & kColorBufferBit) ? GL_COLOR_BUFFER_BIT : 0x0) |
                   ((mask & kDepthBufferBit) ? GL_DEPTH_BUFFER_BIT : 0x0) |
                   ((mask & kStencilBufferBit) ? GL_STENCIL_BUFFER_BIT : 0x0);
  if (gm->IsFeatureAvailable(GraphicsManager::kFramebufferBlit)) {
    gm->BlitFramebuffer(0, 0, ms_fbo->GetWidth(), ms_fbo->GetHeight(), 0, 0,
                        dest_fbo->GetWidth(), dest_fbo->GetHeight(),
                        gl_mask, GL_NEAREST);
  } else if (gm->IsFeatureAvailable(
                 GraphicsManager::kMultisampleFramebufferResolve)) {
    if (mask & kDepthBufferBit) {
      LOG(WARNING) << "Multisampled depth buffer resolves are not supported by "
                   << "this platform.";
    }
    if (mask & kStencilBufferBit) {
      LOG(WARNING) << "Multisampled stencil buffer resolves are not supported "
                   << "by this platform.";
    }
    gm->ResolveMultisampleFramebuffer();
  }

  // Restored saved framebuffer.
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  resource_binder->ClearFramebufferBinding(0);
  BindFramebuffer(previous_fbo);
}

void Renderer::DrawScene(const NodePtr& node) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder) {
    resource_binder->DrawScene(node, flags_, default_shader_.Get());
    // Process any info requests.
    if (flags_.test(kProcessInfoRequests))
      resource_manager_->ProcessResourceInfoRequests(resource_binder);
  }
}

void Renderer::ResourceBinder::MarkAttachmentImplicitlyChanged(
    const FramebufferObject::Attachment& attachment) {
  if (Texture* tex = attachment.GetTexture().Get()) {
    Renderer::SetResourceHolderBit(tex, Texture::kContentsImplicitlyChanged);
  }
  if (CubeMapTexture* tex = attachment.GetCubeMapTexture().Get()) {
    Renderer::SetResourceHolderBit(tex,
                                   CubeMapTexture::kContentsImplicitlyChanged);
  }
}

void Renderer::ResourceBinder::InitImageUnits(int first, int last) {
  first = std::max(0, first);
  const int max_image_units = GetGraphicsManager()->GetConstant<int>(
      GraphicsManager::kMaxTextureImageUnits);
  last = std::min(last, max_image_units - 1);
  DCHECK_GE(last, first);
  if (last < first)
    return;
  image_units_.resize(max_image_units);
  // Set up linked list within image_units_.
  for (int i = 0, n = static_cast<int>(image_units_.size()); i < n; ++i) {
    ImageUnit& image_unit = image_units_[i];
    if (i <= first || i > last)
      image_unit.prev = nullptr;
    else
      image_unit.prev = &image_units_[i - 1];

    if (i >= last)
      image_unit.next = nullptr;
    else
      image_unit.next = &image_units_[i + 1];
    image_unit.available = i >= first && i <= last;
    image_unit.unit_index = static_cast<int>(i);
    image_unit.resource = nullptr;
    image_unit.sampler = 0U;
  }
  lru_unit_ = &image_units_[first];
  mru_unit_ = &image_units_[last];
  image_unit_range_.Set(first, last);
}

void Renderer::ResourceBinder::SetImageUnitRange(const Range1i& units) {
  // Clear all bindings.
  ClearTextureBindings(0U, 0);
  ClearSamplerBindings(0U);
  InitImageUnits(units.GetMinPoint(), units.GetMaxPoint());
  if (GetActiveShaderProgram())
    GetActiveShaderProgram()->ObtainImageUnits(this);
}

void Renderer::ResourceBinder::MapBufferObjectDataRange(
    const BufferObjectPtr& buffer,
    BufferObjectDataMapMode mode,
    const math::Range1ui& range_in) {
  BufferObject* bo = buffer.Get();
  if (!bo)
    return;
  if (bo->GetMappedPointer()) {
    LOG(WARNING) << "A buffer that is already mapped was passed to"
                 << ION_PRETTY_FUNCTION;
    return;
  }
  if (range_in.IsEmpty()) {
    LOG(WARNING) << "Ignoring empty range passed to"
                 << ION_PRETTY_FUNCTION << ", nothing will be mapped";
    return;
  }
  const Range1ui entire_range(
      0U, static_cast<uint32>(bo->GetStructSize() * bo->GetCount()));
  Range1ui range = range_in;
  void* data = nullptr;
  BufferObject::MappedBufferData::DataSource data_source =
      BufferObject::MappedBufferData::kGpuMapped;
  GraphicsManager* gm = GetGraphicsManager().Get();
  // Prefer MapBufferRange since it has more features.
  if (gm->IsFeatureAvailable(GraphicsManager::kMapBufferRange)) {
    BufferResource* br = resource_manager_->GetResource(bo, this);
    br->Bind(this);
    GLenum access_mode;
    if (mode == kReadOnly)
      access_mode = GL_MAP_READ_BIT;
    else if (mode == kWriteOnly)
      access_mode = GL_MAP_WRITE_BIT;
    else
      access_mode = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
    data = gm->MapBufferRange(br->GetGlTarget(), range.GetMinPoint(),
                              range.GetSize(), access_mode);
  } else if (gm->IsFeatureAvailable(GraphicsManager::kMapBuffer) &&
             range == entire_range) {
    BufferResource* br = resource_manager_->GetResource(bo, this);
    br->Bind(this);
    GLenum access_mode;
    if (mode == kReadOnly)
      access_mode = GL_READ_ONLY;
    else if (mode == kWriteOnly)
      access_mode = GL_WRITE_ONLY;
    else
      access_mode = GL_READ_WRITE;
    data = gm->MapBuffer(br->GetGlTarget(), access_mode);
  } else if (range.GetSize() <= entire_range.GetSize()) {
    // Reuse bo's data if it hasn't been wiped.
    if (bo->GetData().Get() && bo->GetData()->GetData() &&
        bo->GetCount() * bo->GetStructSize() >= range_in.GetMaxPoint()) {
      data = static_cast<void*>(
          bo->GetData()->GetMutableData<uint8>() + range_in.GetMinPoint());
      data_source = BufferObject::MappedBufferData::kDataContainer;
    } else {
      data = bo->GetAllocator()->AllocateMemory(range.GetSize());
      data_source = BufferObject::MappedBufferData::kAllocated;
      if (mode != kWriteOnly) {
        LOG(WARNING) << "MapBufferObjectDataRange() glMapBufferRange not "
            "supported and BufferObject's DataContainer has been wiped so "
            "mapped bytes are uninitialized, i.e., garbage.";
      }
    }
  }
  if (data)
    bo->SetMappedData(range, data, data_source, mode == kReadOnly);
  else
    LOG(ERROR) << "Failed to allocate data for "
               << ION_PRETTY_FUNCTION;
}

void Renderer::ResourceBinder::UnmapBufferObjectData(
    const BufferObjectPtr& buffer) {
  BufferObject* bo = buffer.Get();
  if (!bo)
    return;
  if (void* ptr = bo->GetMappedPointer()) {
    BufferResource* br = resource_manager_->GetResource(bo, this);
    br->Bind(this);
    if (bo->GetMappedData().data_source ==
        BufferObject::MappedBufferData::kGpuMapped &&
        graphics_manager_->IsFeatureAvailable(
            GraphicsManager::kMapBufferBase)) {
      graphics_manager_->UnmapBuffer(br->GetGlTarget());
    } else {
      if (!bo->GetMappedData().read_only)
        br->UploadSubData(bo->GetMappedData().range, ptr);
      if (bo->GetMappedData().data_source ==
          BufferObject::MappedBufferData::kAllocated) {
        // Free the data.
        bo->GetAllocator()->DeallocateMemory(ptr);
      }
    }
    // Clear the data in the BufferObject.
    bo->SetMappedData(
        Range1ui(),
        nullptr,
        base::InvalidEnumValue<BufferObject::MappedBufferData::DataSource>(),
        true);
  } else {
    LOG(WARNING) << "An unmapped BufferObject was passed to"
                 << ION_PRETTY_FUNCTION;
  }
}

void Renderer::ResourceBinder::DrawScene(const NodePtr& node,
                                         const Flags& flags,
                                         ShaderProgram* default_shader) {
  GraphicsManager* gm = GetGraphicsManager().Get();
  DCHECK(gm);

  if ((flags & AllSaveFlags()).any()) {
    // Possibly save existing state.
    if (flags.test(kSaveActiveTexture))
      gm->GetIntegerv(GL_ACTIVE_TEXTURE, GetSavedId(kSaveActiveTexture));
    if (flags.test(kSaveArrayBuffer))
      gm->GetIntegerv(GL_ARRAY_BUFFER_BINDING, GetSavedId(kSaveArrayBuffer));
    if (flags.test(kSaveElementArrayBuffer))
      gm->GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,
                      GetSavedId(kSaveElementArrayBuffer));
    if (flags.test(kSaveFramebuffer))
      gm->GetIntegerv(GL_FRAMEBUFFER_BINDING, GetSavedId(kSaveFramebuffer));
    if (flags.test(kSaveStateTable)) {
      UpdateStateTable(0, 0, gm, saved_state_table_.Get());
      // Ensure that "default" capabilities are explicitly set.
      const base::IndexMap<StateTable::Capability, GLenum> capability_map =
          base::EnumHelper::GetIndexMap<StateTable::Capability>();
      const size_t num_capabilities = capability_map.GetCount();
      for (size_t i = 0; i < num_capabilities; ++i) {
        const StateTable::Capability st_cap =
            static_cast<StateTable::Capability>(i);
        // The default value for all capabilities except dithering is false.
        if (!saved_state_table_->IsCapabilitySet(st_cap))
          saved_state_table_->Enable(st_cap, st_cap == StateTable::kDither);
      }
    }
    if (flags.test(kSaveShaderProgram))
      gm->GetIntegerv(GL_CURRENT_PROGRAM, GetSavedId(kSaveShaderProgram));
    if (flags.test(kSaveVertexArray) &&
        gm->IsFeatureAvailable(GraphicsManager::kVertexArrays))
      gm->GetIntegerv(GL_VERTEX_ARRAY_BINDING, GetSavedId(kSaveVertexArray));
  }

  // Make sure the active framebuffer is up to date.
  if (FramebufferResource* fbr = GetActiveFramebuffer()) fbr->Bind(this);

  // Release any resources waiting to be released or destroyed.
  if (flags.test(kProcessReleases)) resource_manager_->ProcessReleases(this);

  // If there are no shaders before the next draw call then we will bind a
  // default one.
  current_shader_program_ = default_shader;

  // Draw.
  current_traversal_index_ = 0;
  if (node.Get()) {
    DrawNode(*node, gm);
    // If we have a framebuffer bound, then after the frame is drawn any
    // textures bound to the framebuffer's attachment need to be notified that
    // their contents have changed (and maybe update mipmaps).
    if (FramebufferObject* fbo = GetCurrentFramebuffer().Get()) {
      for (size_t i = 0U; i < kColorAttachmentSlotCount; ++i)
        MarkAttachmentImplicitlyChanged(fbo->GetColorAttachment(i));
      MarkAttachmentImplicitlyChanged(fbo->GetDepthAttachment());
      MarkAttachmentImplicitlyChanged(fbo->GetStencilAttachment());
    }
  }

  // Possibly invalidate framebuffer attachments.
  if (gm->IsFeatureAvailable(GraphicsManager::kInvalidateFramebuffer) &&
      (flags & AllInvalidateFlags()).any()) {
    GLenum attachments[kColorAttachmentSlotCount + 2];
    GLsizei count = 0;
    if (active_framebuffer_.gl_id == 0U) {
      if (flags.test(kInvalidateColorAttachment)) {
        attachments[count++] = GL_COLOR;
      }
      if (flags.test(kInvalidateDepthAttachment)) {
        attachments[count++] = GL_DEPTH;
      }
      if (flags.test(kInvalidateStencilAttachment)) {
        attachments[count++] = GL_STENCIL;
      }
    } else {
      for (size_t i = 0; i < kColorAttachmentSlotCount; ++i) {
        if (flags.test(kInvalidateColorAttachment + i)) {
          attachments[count++] = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        }
      }
      if (flags.test(kInvalidateDepthAttachment)) {
        attachments[count++] = GL_DEPTH_ATTACHMENT;
      }
      if (flags.test(kInvalidateStencilAttachment)) {
        attachments[count++] = GL_STENCIL_ATTACHMENT;
      }
    }
    gm->InvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, count, attachments);
  }

  // Possibly restore state.
  if ((flags & (AllRestoreFlags() | AllClearFlags())).any()) {
    // Array buffer.
    if (flags.test(kRestoreArrayBuffer))
      BindBuffer(BufferObject::kArrayBuffer, *GetSavedId(kSaveArrayBuffer),
                 nullptr);
    else if (flags.test(kClearArrayBuffer))
      BindBuffer(BufferObject::kArrayBuffer, 0U, nullptr);
    // Element array buffer.
    if (flags.test(kRestoreElementArrayBuffer))
      BindBuffer(BufferObject::kElementBuffer,
                 *GetSavedId(kSaveElementArrayBuffer), nullptr);
    else if (flags.test(kClearElementArrayBuffer))
      BindBuffer(BufferObject::kElementBuffer, 0U, nullptr);
    // Framebuffer.
    if (flags.test(kRestoreFramebuffer)) {
      BindFramebuffer(*GetSavedId(kSaveFramebuffer), nullptr);
      SetCurrentFramebuffer(FramebufferObjectPtr());
    } else if (flags.test(kClearFramebuffer)) {
      BindFramebuffer(0U, nullptr);
      SetCurrentFramebuffer(FramebufferObjectPtr());
    }
    // Shader program.
    if (flags.test(kRestoreShaderProgram)) {
      GLuint program_id = static_cast<GLuint>(*GetSavedId(kSaveShaderProgram));
      // Check the saved ID, since we might have saved a program marked for
      // deletion, and we won't be able to bind it again. If that's the case,
      // reset the used program to zero.
      if (gm->IsProgram(program_id))
        BindProgram(*GetSavedId(kSaveShaderProgram), nullptr);
      else
        BindProgram(0U, nullptr);
    } else if (flags.test(kClearShaderProgram)) {
      BindProgram(0U, nullptr);
    }
    // StateTable.
    if (flags.test(kRestoreStateTable)) {
      UpdateFromStateTable(*saved_state_table_, gl_state_table_.Get(), gm);
      gl_state_table_->MergeNonClearValuesFrom(*saved_state_table_,
                                               *saved_state_table_);
    }
    // Vertex array.
    if (gm->IsFeatureAvailable(GraphicsManager::kVertexArrays)) {
      if (flags.test(kRestoreVertexArray))
        BindVertexArray(*GetSavedId(kSaveVertexArray), nullptr);
      else if (flags.test(kClearVertexArray))
        BindVertexArray(0U, nullptr);
    }

    // Other clear flags.
    if (flags.test(kClearCubemaps)) {
      ClearTextureBindings(0U);
      const GLuint count = static_cast<GLuint>(image_units_.size());
      for (GLuint i = 0; i < count; ++i) {
        ActivateUnit(i);
        gm->BindTexture(GL_TEXTURE_CUBE_MAP, 0);
        if (gm->IsFeatureAvailable(GraphicsManager::kTextureCubeMapArray)) {
          gm->BindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
        }
      }
    }
    if (flags.test(kClearTextures)) {
      ClearTextureBindings(0U);
      const GLuint count = static_cast<GLuint>(image_units_.size());
      for (GLuint i = 0; i < count; ++i) {
        ActivateUnit(i);
        gm->BindTexture(GL_TEXTURE_2D, 0);
        if (gm->IsFeatureAvailable(GraphicsManager::kTextureArray1d)) {
          gm->BindTexture(GL_TEXTURE_1D_ARRAY, 0);
        }
        if (gm->IsFeatureAvailable(GraphicsManager::kTextureArray2d)) {
          gm->BindTexture(GL_TEXTURE_2D_ARRAY, 0);
        }
        if (gm->IsFeatureAvailable(GraphicsManager::kTexture3d)) {
          gm->BindTexture(GL_TEXTURE_3D, 0);
        }
        if (gm->IsExtensionSupported("image_external")) {
          gm->BindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        }
      }
    }
    if (flags.test(kClearSamplers)) {
      const GLuint count = static_cast<GLuint>(image_units_.size());
      for (GLuint i = 0; i < count; ++i)
        BindSamplerToUnit(0U, i);
    }
    // This has to come after clearing textures.
    if (flags.test(kRestoreActiveTexture))
      ActivateUnit(*GetSavedId(kSaveActiveTexture) - GL_TEXTURE0);
    else if (flags.test(kClearActiveTexture))
      ActivateUnit(0U);
  }
}

void Renderer::ProcessResourceInfoRequests() {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_manager_->ProcessResourceInfoRequests(resource_binder);
}

void Renderer::UpdateStateFromOpenGL(int window_width, int window_height) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    UpdateStateTable(window_width, window_height, GetGraphicsManager().Get(),
                     resource_binder->GetStateTable());
}

void Renderer::UpdateStateFromStateTable(const StateTablePtr& state_table) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->GetStateTable()->CopyFrom(*state_table);
}

const StateTable& Renderer::GetStateTable() const {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (!resource_binder) {
    ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
        StateTablePtr, kDefaultStateTable, (new StateTablePtr(new StateTable)));
    LOG(WARNING) << "***ION: No ResourceBinder (invalid GL Context?): "
                 << " using default StateTable";
    return *kDefaultStateTable->Get();
  }
  return *resource_binder->GetStateTable();
}

void Renderer::MapBufferObjectData(const BufferObjectPtr& buffer,
                                   BufferObjectDataMapMode mode) {
  if (BufferObject* bo = buffer.Get()) {
    // Map the entire buffer.
    const Range1ui range(
        0U, static_cast<uint32>(bo->GetStructSize() * bo->GetCount()));
    MapBufferObjectDataRange(buffer, mode, range);
  } else {
    LOG(WARNING) << "A NULL BufferObject was passed to"
                 << ION_PRETTY_FUNCTION;
  }
}

void Renderer::MapBufferObjectDataRange(const BufferObjectPtr& buffer,
                                        BufferObjectDataMapMode mode,
                                        const math::Range1ui& range_in) {
  if (buffer.Get()) {
    if (ResourceBinder* resource_binder =
        GetOrCreateInternalResourceBinder(__LINE__)) {
      resource_binder->MapBufferObjectDataRange(buffer, mode, range_in);
    }
  } else {
    LOG(WARNING) << "A NULL BufferObject was passed to"
                 << ION_PRETTY_FUNCTION;
  }
}

void Renderer::UnmapBufferObjectData(const BufferObjectPtr& buffer) {
  if (buffer.Get()) {
    if (ResourceBinder* resource_binder =
        GetOrCreateInternalResourceBinder(__LINE__)) {
      resource_binder->UnmapBufferObjectData(buffer);
    }
  } else {
    LOG(WARNING) << "A NULL BufferObject was passed to"
                 << ION_PRETTY_FUNCTION;
  }
}

const ImagePtr Renderer::ReadImage(const math::Range2i& range,
                                   Image::Format format,
                                   const base::AllocatorPtr& allocator) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  return resource_binder ?
      resource_binder->ReadImage(range, format, allocator) : ImagePtr();
}

#if ION_PRODUCTION
void Renderer::PushDebugMarker(const std::string& label) {}
void Renderer::PopDebugMarker() {}
#else
void Renderer::PushDebugMarker(const std::string& marker) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->GetStreamAnnotator()->Push(marker);
}
void Renderer::PopDebugMarker() {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->GetStreamAnnotator()->Pop();
}
#endif

template <typename T>
uint32 Renderer::GetResourceGlId(T* holder) {
  uint32 id = 0U;
  if (holder) {
    typedef typename HolderToResource<T>::ResourceType ResourceType;
    ResourceBinder* resource_binder =
        GetOrCreateInternalResourceBinder(__LINE__);
    if (resource_binder) {
      ResourceType* resource =
          resource_manager_->GetResource(holder, resource_binder);
      if (resource) {
        resource->Update(resource_binder);
        id = resource->GetId();
      }
    }
  }
  return id;
}

// Explicitly instantiate.
template ION_API uint32
Renderer::GetResourceGlId<BufferObject>(BufferObject*);  // NOLINT
template ION_API uint32
Renderer::GetResourceGlId<CubeMapTexture>(CubeMapTexture*);  // NOLINT
template ION_API uint32
Renderer::GetResourceGlId<FramebufferObject>(FramebufferObject*);  // NOLINT
template ION_API uint32
Renderer::GetResourceGlId<IndexBuffer>(IndexBuffer*);                  // NOLINT
template ION_API uint32 Renderer::GetResourceGlId<Sampler>(Sampler*);  // NOLINT
template ION_API uint32 Renderer::GetResourceGlId<Shader>(Shader*);    // NOLINT
template ION_API uint32
Renderer::GetResourceGlId<ShaderProgram>(ShaderProgram*);              // NOLINT
template ION_API uint32 Renderer::GetResourceGlId<Texture>(Texture*);  // NOLINT

void Renderer::SetTextureImageUnitRange(const Range1i& units) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_binder->SetImageUnitRange(units);
}

template <typename T>
void Renderer::ClearResources(const T* holder) {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_manager_->ReleaseResources(holder, resource_binder);
}

// Explicitly instantiate.
template ION_API void Renderer::ClearResources<AttributeArray>(
    const AttributeArray*);
template ION_API void Renderer::ClearResources<BufferObject>(
    const BufferObject*);
template ION_API void Renderer::ClearResources<CubeMapTexture>(
    const CubeMapTexture*);
template ION_API void Renderer::ClearResources<FramebufferObject>(
    const FramebufferObject*);
template ION_API void Renderer::ClearResources<Sampler>(const Sampler*);
template ION_API void Renderer::ClearResources<Shader>(const Shader*);
template ION_API void Renderer::ClearResources<ShaderProgram>(
    const ShaderProgram*);
template ION_API void Renderer::ClearResources<Texture>(const Texture*);

void Renderer::ClearAllResources(bool force_abandon) {
  resource_manager_->DestroyOrAbandonAllResources(force_abandon);
}

void Renderer::ClearTypedResources(ResourceType type) {
  resource_manager_->ReleaseTypedResources(type);
  ReleaseResources();
}

void Renderer::ReleaseResources() {
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder)
    resource_manager_->ProcessReleases(resource_binder);
}

size_t Renderer::GetGpuMemoryUsage(ResourceType type) const {
  return resource_manager_->GetGpuMemoryUsage(type);
}

void Renderer::BeginTransformFeedback(const TransformFeedbackPtr& tf) {
  DCHECK(GetGraphicsManager()->IsFeatureAvailable(
      GraphicsManager::kTransformFeedback));
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder) {
    TransformFeedbackResource* tfr =
        resource_manager_->GetResource(tf.Get(), resource_binder);
    DCHECK(tfr);
    // Bind the transform feedback object, but do not call the low level
    // BeginTransformFeedback function yet.  We defer it until Node rendering
    // because that's when we'll have a valid shader program.
    tfr->Bind(resource_binder);
  }
}

void Renderer::EndTransformFeedback() {
  GetGraphicsManager()->EndTransformFeedback();
  ResourceBinder* resource_binder = GetOrCreateInternalResourceBinder(__LINE__);
  if (resource_binder) {
    TransformFeedbackResource* tfr =
        resource_binder->GetActiveTransformFeedback();
    DCHECK(tfr);
    // OpenGL doesn't require us to unbind the transform feedback object,
    // but we need to do so because use the binding as an internal signal to
    // start the capture in DrawNode.
    resource_binder->BindTransformFeedback(0, nullptr);
    tfr->StopCapturing();
  }
}

void Renderer::EnableResourceAccessCheck(bool enabled) {
  resource_manager_->EnableResourceAccessCheck(enabled);
}

const ShaderProgramPtr Renderer::CreateDefaultShaderProgram(
    const base::AllocatorPtr& allocator) {
  static const char* kDefaultVertexShaderString =
      "uniform mat4 uProjectionMatrix;\n"
      "uniform mat4 uModelviewMatrix;\n"
      "attribute vec3 aVertex;\n"
      "\n"
      "void main(void) {\n"
      "  gl_Position = uProjectionMatrix * uModelviewMatrix *\n"
      "      vec4(aVertex, 1.);\n"
      "}\n";

  static const char* kDefaultFragmentShaderString =
      "#ifdef GL_ES\n"
      "precision mediump float;\n"
      "#endif\n"
      "\n"
      "uniform vec4 uBaseColor;\n"
      "\n"
      "void main(void) {\n"
      "  gl_FragColor = uBaseColor;\n"
      "}\n";

  // Create an empty ShaderInputRegistry for the shader, since it uses only
  // global uniforms and attributes.
  ShaderInputRegistryPtr empty_registry(new(allocator) ShaderInputRegistry);
  empty_registry->IncludeGlobalRegistry();

  ShaderProgramPtr program(new(allocator) ShaderProgram(empty_registry));
  program->SetLabel("Default Renderer shader");
  program->SetVertexShader(
      ShaderPtr(new(allocator) Shader(kDefaultVertexShaderString)));
  program->SetFragmentShader(
      ShaderPtr(new(allocator) Shader(kDefaultFragmentShaderString)));
  program->GetVertexShader()->SetLabel("Default Renderer vertex shader");
  program->GetFragmentShader()->SetLabel("Default Renderer fragment shader");
  return program;
}

void Renderer::SetResourceHolderBit(const ResourceHolder* holder, int bit) {
  holder->OnChanged(bit);
}

void Renderer::ResourceBinder::DrawNode(const Node& node, GraphicsManager* gm) {
  if (!node.IsEnabled())
    return;

  ScopedLabel label(this, &node, node.GetLabel(), ION_PRETTY_FUNCTION);

  if (const StateTable* st = node.GetStateTable().Get()) {
    // Store the current client state; it will be restored after drawing and
    // processing any children.
    traversal_state_tables_[current_traversal_index_]->CopyFrom(
        *client_state_table_.Get());
    // Point child nodes at the next entry in the StateTable shadow vector.
    if (++current_traversal_index_ >= traversal_state_tables_.size())
      traversal_state_tables_.push_back(StateTablePtr(new StateTable));

    // Merge the node's state table into the client state.
    client_state_table_->MergeValuesFrom(*st, *st);
    ClearFromStateTable(*st, gl_state_table_.Get(), gm);
    if (st->AreSettingsEnforced()) {
      UpdateFromStateTable(*st, gl_state_table_.Get(), gm);
      // Update our copy of OpenGL's state.
      gl_state_table_->MergeNonClearValuesFrom(*st, *st);
    }
  }

  // Get the Node's shader, if any.
  if (ShaderProgram* shader = node.GetShaderProgram().Get())
    current_shader_program_ = shader;
  DCHECK(current_shader_program_);

  // Process all Uniforms.
  PushUniforms(node.GetUniforms());
  const base::AllocVector<UniformBlockPtr>& uniform_blocks =
      node.GetUniformBlocks();
  const size_t num_uniform_blocks = uniform_blocks.size();
  for (size_t i = 0; i < num_uniform_blocks; ++i) {
    if (uniform_blocks[i]->IsEnabled())
      PushUniforms(uniform_blocks[i]->GetUniforms());
  }

  // See if there are any shapes to draw.
  const base::AllocVector<ShapePtr>& shapes = node.GetShapes();
  if (const size_t num_shapes = shapes.size()) {
    // Send global state changes relative to current GL state to OpenGL.
    UpdateFromStateTable(*client_state_table_, gl_state_table_.Get(), gm);

    // Bind the shader program to use. Note that it may already be bound in
    // OpenGL, but we still need to update the resource.
    resource_manager_->GetResource(current_shader_program_, this)->Bind(this);

    // If requested, start transform feedback.
    TransformFeedbackResource* tfr = GetActiveTransformFeedback();
    if (tfr && !tfr->IsCapturing()) {
      DCHECK(!current_shader_program_->GetCapturedVaryings().empty()) <<
          node.GetLabel() << " has a shader program with no captured varyings.";
      // In the future, we could add support for capturing multiple shapes, but
      // only if they all have the same primitive type.  Note that clients would
      // need to create a large enough transform feedback buffer to hold
      // aggregated data across all shapes.
      DCHECK_EQ(1, num_shapes) << "Transform feedback is active for "
                               << node.GetLabel()
                               << ", but it has more than one shape.";
      // Child nodes might have shapes with differing primitive types, so we
      // can't allow transform feedback for them either.
      DCHECK_EQ(0, node.GetChildren().size())
          << "Transform feedback is active for " << node.GetLabel()
          << ", but it has children.";
      GetGraphicsManager()->BeginTransformFeedback(
          base::EnumHelper::GetConstant(shapes[0]->GetPrimitiveType()));
      tfr->StartCapturing();
    }

    // Draw shapes.
    for (size_t i = 0; i < num_shapes; ++i)
      DrawShape(*shapes[i], gm);

    // Update our copy of OpenGL's state.
    gl_state_table_->MergeNonClearValuesFrom(*client_state_table_,
                                             *client_state_table_);
  }

  // Store the current shader since it needs to be restored after drawing
  // children.
  ShaderProgram* saved_shader_program = current_shader_program_;

  // Recurse on children.
  const base::AllocVector<NodePtr>& children = node.GetChildren();
  const size_t num_children = children.size();
  for (size_t i = 0; i < num_children; ++i) {
    DrawNode(*children[i], gm);
    // Restore the previous shader as the next one to use.
    current_shader_program_ = saved_shader_program;
  }

  // Reverse the changes made by the local StateTable.
  if (const StateTable* st = node.GetStateTable().Get()) {
    --current_traversal_index_;
    client_state_table_->MergeNonClearValuesFrom(
        *traversal_state_tables_[current_traversal_index_].Get(), *st);
  }

  // Restore uniform values.
  PopUniforms(node.GetUniforms());
  for (size_t i = 0; i < num_uniform_blocks; ++i) {
    if (uniform_blocks[i]->IsEnabled())
      PopUniforms(uniform_blocks[i]->GetUniforms());
  }
}

void Renderer::ResourceBinder::DrawShape(const Shape& shape,
                                         GraphicsManager* gm) {
  if (!shape.GetAttributeArray().Get())
      return;

  const AttributeArray& attribute_array = *shape.GetAttributeArray();

  if (attribute_array.GetAttributeCount() == 0U &&
      shape.GetVertexRangeCount() != 1U)
      return;

  if (shape.GetIndexBuffer() &&
      shape.GetIndexBuffer()->GetCount() == 0U)
      return;

  ScopedLabel label(this, &shape, shape.GetLabel(), ION_PRETTY_FUNCTION);

  // Bind the vertex array. We can only use vertex arrays if they are
  // available on the local platform. If they are not, we simply use the
  // default global array that is always active.
  VertexArrayResource* var;
  if (gm->IsFeatureAvailable(GraphicsManager::kVertexArrays))
    var = resource_manager_->GetResource(&attribute_array, this);
  else
    var = resource_manager_->GetResource(
        reinterpret_cast<const AttributeArrayEmulator*>(&attribute_array),
        this);
  DCHECK(var);
  if (var && !var->BindAndCheckBuffers(false, this))
    return;

  if (shape.GetPrimitiveType() == Shape::kPatches) {
    gm->PatchParameteri(GL_PATCH_VERTICES, shape.GetPatchVertices());
  }

  // Draw the shape.
  if (IndexBuffer* ib = shape.GetIndexBuffer().Get()) {
    DrawIndexedShape(shape, *ib, gm);
  } else {
    // Even if the shape has no associated per-vertex attributes, it might still
    // make sense to issue a draw call. The specific vertex attributes might be
    // determined implicitly in a vertex or geometry shader (for example, using
    // gl_VertexID).
    const size_t vertex_count =
        attribute_array.GetAttributeCount() > 0U
          ? var->GetVertexCount()
          : (shape.GetVertexRange(0).GetSize())[0];
    DrawNonindexedShape(shape, vertex_count, gm);
  }
}

void Renderer::ResourceBinder::DrawIndexedShape(const Shape& shape,
                                                const IndexBuffer& ib,
                                                GraphicsManager* gm) {
  // Bind the index buffer.
  BufferResource* br = resource_manager_->GetResource(&ib, this);
  DCHECK(br);
  br->BindToTarget(this, BufferObject::kElementBuffer);

  // The index type is in the first spec.
  // 
  // BufferObject::Spec is finalized.
  const BufferObject::Spec& spec = ib.GetSpec(0);
  DCHECK(!base::IsInvalidReference(spec));
  const GLenum data_type = base::EnumHelper::GetConstant(spec.type);
  if (!gm->IsFeatureAvailable(GraphicsManager::kElementIndex32Bit) &&
      (data_type == GL_INT || data_type == GL_UNSIGNED_INT)) {
    LOG(ERROR) << "***ION: Unable to draw shape '" << shape.GetLabel()
               << "' using index buffer: "
               << "32-bit element indices are not supported on this platform";
  }
  const GLenum prim_type =
      base::EnumHelper::GetConstant(shape.GetPrimitiveType());
  if (const size_t range_count = shape.GetVertexRangeCount()) {
    // Draw all enabled ranges.
    for (size_t i = 0; i < range_count; ++i) {
      if (shape.IsVertexRangeEnabled(i)) {
        const Range1i& range = shape.GetVertexRange(i);
        const int start_index = range.GetMinPoint()[0];
        const int count = range.GetSize();
        DCHECK_GT(count, 0);
        const int instance_count = shape.GetVertexRangeInstanceCount(i);
        // The start_index is taken into account as an offset into the buffer.
        if (instance_count &&
            gm->IsFeatureAvailable(GraphicsManager::kDrawInstanced)) {
          gm->DrawElementsInstanced(prim_type, count, data_type,
                                    reinterpret_cast<const GLvoid*>(
                                        start_index * ib.GetStructSize()),
                                    instance_count);
        } else {
          if (instance_count) {
            LOG_ONCE(WARNING) << "***ION: Instanced drawing is not "
                "available. The vertex ranges in Shape: "
                              << shape.GetLabel()
                              << " will be drawn only once.";
          }
          gm->DrawElements(prim_type, count, data_type,
                           reinterpret_cast<const GLvoid*>(
                               start_index * ib.GetStructSize()));
        }
      }
    }
  } else {
    // No vertex ranges, so draw all indices.
    const int instance_count = shape.GetInstanceCount();
    if (instance_count &&
        gm->IsFeatureAvailable(GraphicsManager::kDrawInstanced)) {
      gm->DrawElementsInstanced(
          prim_type, static_cast<GLsizei>(ib.GetCount()), data_type,
          reinterpret_cast<const GLvoid*>(0), instance_count);
    } else {
      if (instance_count) {
        LOG_ONCE(WARNING)
            << "***ION: Instanced drawing is not available. Shape: "
            << shape.GetLabel() << " will be drawn only once.";
      }
      gm->DrawElements(prim_type, static_cast<GLsizei>(ib.GetCount()),
                       data_type, reinterpret_cast<const GLvoid*>(0));
    }
  }
}

void Renderer::ResourceBinder::DrawNonindexedShape(const Shape& shape,
                                                   size_t vertex_count,
                                                   GraphicsManager* gm) {
  const GLenum prim_type =
      base::EnumHelper::GetConstant(shape.GetPrimitiveType());
  if (const size_t range_count = shape.GetVertexRangeCount()) {
    // Draw all enabled ranges.
    for (size_t i = 0; i < range_count; ++i) {
      if (shape.IsVertexRangeEnabled(i)) {
        const Range1i& range = shape.GetVertexRange(i);
        const int start_index = range.GetMinPoint()[0];
        const int count = range.GetSize();
        DCHECK_GT(count, 0);
        const int instance_count = shape.GetVertexRangeInstanceCount(i);
        if (instance_count &&
            gm->IsFeatureAvailable(GraphicsManager::kDrawInstanced)) {
          gm->DrawArraysInstanced(prim_type, start_index, count,
                                  instance_count);
        } else {
          if (instance_count) {
            LOG_ONCE(WARNING) << "***ION: Instanced drawing is not available. "
                                 "The vertex ranges in Shape: "
                              << shape.GetLabel()
                              << " will be drawn only once.";
          }
          gm->DrawArrays(prim_type, start_index, count);
        }
      }
    }
  } else {
    // No vertex ranges, so draw all indices.
    const int instance_count = shape.GetInstanceCount();
    if (instance_count &&
        gm->IsFeatureAvailable(GraphicsManager::kDrawInstanced)) {
      gm->DrawArraysInstanced(prim_type, 0, static_cast<GLsizei>(vertex_count),
                              instance_count);
    } else {
      if (instance_count) {
        LOG_ONCE(WARNING)
            << "***ION: Instanced drawing is not available. Shape: "
            << shape.GetLabel() << " will be drawn only once.";
      }
      gm->DrawArrays(prim_type, 0, static_cast<GLsizei>(vertex_count));
    }
  }
}

void Renderer::ResourceBinder::BindBuffer(BufferObject::Target target,
                                          GLuint id, BufferResource* resource) {
  if (id != active_buffers_[target].gl_id) {
    active_buffers_[target].gl_id = id;
    active_buffers_[target].resource = resource;
    const GLenum gltarget = base::EnumHelper::GetConstant(target);
    GetGraphicsManager()->BindBuffer(gltarget, id);
    if (target == BufferObject::kElementBuffer && active_vertex_array_.resource)
      active_vertex_array_.resource->SetElementArrayBinding(id, resource);
  }
}

void Renderer::ResourceBinder::BindBufferIndexed(
    BufferObject::IndexedTarget target, GLuint index, GLuint id,
    BufferResource* resource) {
  if (id != active_indexed_buffers_[target].at(index).gl_id) {
    active_indexed_buffers_[target][index].gl_id = id;
    active_indexed_buffers_[target][index].resource = resource;
    const GLenum gltarget = base::EnumHelper::GetConstant(target);
    GetGraphicsManager()->BindBufferBase(gltarget, index, id);
  }
}

void Renderer::ResourceBinder::BindFramebuffer(GLuint id,
                                               FramebufferResource* fbo) {
  if (id != active_framebuffer_.gl_id) {
    DCHECK(!fbo || id == fbo->GetId());
    active_framebuffer_.gl_id = id;
    active_framebuffer_.resource = fbo;
    GetGraphicsManager()->BindFramebuffer(GL_FRAMEBUFFER, id);
  }
}

bool Renderer::ResourceBinder::BindProgram(GLuint id,
                                           ShaderProgramResource* resource) {
  if (id != active_shader_.gl_id) {
    DCHECK(!resource || id == resource->GetId());
    active_shader_.gl_id = id;
    GetGraphicsManager()->UseProgram(id);
    active_shader_.resource = resource;
    return true;
  } else {
    return false;
  }
}

void Renderer::ResourceBinder::BindVertexArray(GLuint id,
                                               VertexArrayResource* resource) {
  if (id != active_vertex_array_.gl_id) {
    DCHECK(!resource || id == resource->GetId());
    active_vertex_array_.gl_id = id;
    active_vertex_array_.resource = resource;
    // Element buffer binding is part of vertex array object state.
    // However, some drivers are buggy and treat it as part of global state.
    // As a workaround, we always clear the element buffer binding
    // when binding a new VAO.
    ClearBufferBinding(BufferObject::kElementBuffer, 0U);
    GetGraphicsManager()->BindVertexArray(id);
  }
}

inline void Renderer::ResourceBinder::ActivateUnit(GLuint unit_index) {
  DCHECK_LT(unit_index, static_cast<GLuint>(image_units_.size()));
  if (unit_index != active_image_unit_) {
    active_image_unit_ = unit_index;
    GetGraphicsManager()->ActiveTexture(GL_TEXTURE0 + unit_index);
  }
}

void Renderer::ResourceBinder::UseImageUnit(
    int unit_index, TextureResource* txr) {
  DCHECK_LT(unit_index, static_cast<int>(image_units_.size()));
  ImageUnit* image_unit = &image_units_[unit_index];
  image_unit->available = false;
  if (txr)
    image_unit->resource = txr;
  if (image_unit == mru_unit_)
    return;
  // Move image_unit to MRU.
  if (image_unit->prev) {
    image_unit->prev->next = image_unit->next;
  } else {
    lru_unit_ = image_unit->next;
  }
  DCHECK(image_unit->next);
  image_unit->next->prev = image_unit->prev;
  image_unit->prev = mru_unit_;
  mru_unit_->next = image_unit;
  image_unit->next = nullptr;
  mru_unit_ = image_unit;
}

int Renderer::ResourceBinder::ObtainImageUnit(
    TextureResource* txr, int desired_index) {
  if (desired_index < 0 && txr)
    desired_index = GetLastBoundUnit(txr);

  if (desired_index < 0 ||
      desired_index < image_unit_range_.GetMinPoint() ||
      desired_index > image_unit_range_.GetMaxPoint() ||
      (image_units_[desired_index].resource != txr &&
       !image_units_[desired_index].available)) {
    // Find first available but least recently used unit.
    for (ImageUnit* u = lru_unit_; ; u = u->next) {
      if (!u) {
        // We have no available units so reuse the LRU.
        desired_index = lru_unit_->unit_index;
        break;
      }
      if (u->available) {
        desired_index = u->unit_index;
        break;
      }
    }
  }
  DCHECK_LE(0, desired_index);
  return desired_index;
}

inline void Renderer::ResourceBinder::BindTextureToUnit(
    TextureResource* resource, GLuint unit_index) {
  DCHECK_LT(unit_index, image_units_.size());
  if (image_units_[unit_index].resource != resource) {
    ActivateUnit(unit_index);
    // If a TextureResource is evicted from its unit, it will need to be
    // rebound there later.
    SetLastBoundUnit(resource, unit_index);
    const GLuint id = resource->GetId();
    const GLenum target = resource->GetGlTarget();
    GetGraphicsManager()->BindTexture(target, id);
  }
  UseImageUnit(unit_index, resource);
}

void Renderer::ResourceBinder::BindTransformFeedback(
    GLuint id, TransformFeedbackResource* tf) {
  if (id != active_transform_feedback_.gl_id) {
    DCHECK(!tf || id == tf->GetId());
    active_transform_feedback_.gl_id = id;
    active_transform_feedback_.resource = tf;
    GetGraphicsManager()->BindTransformFeedback(GL_TRANSFORM_FEEDBACK, id);
  }
}

void Renderer::ResourceBinder::ClearTextureBindings(
    GLuint id, GLuint start_unit) {
  std::lock_guard<std::mutex> locker(texture_bindings_mutex_);
  const GLuint count = static_cast<GLuint>(GetImageUnitCount());
  for (GLuint unit_index = start_unit; unit_index < count; ++unit_index) {
    if (TextureResource* resource = image_units_[unit_index].resource) {
      if (!id || (resource && id == resource->GetId())) {
        ReleaseImageUnit(unit_index);
        image_units_[unit_index].resource = nullptr;
      }
    }
  }
}

void Renderer::ResourceBinder::ClearTextureBinding(
    GLuint id, GLuint unit_index) {
  std::lock_guard<std::mutex> locker(texture_bindings_mutex_);
  if (unit_index >= image_units_.size())
    return;
  if (TextureResource* resource = image_units_[unit_index].resource) {
    if (!id || (resource && id == resource->GetId())) {
      ReleaseImageUnit(unit_index);
      image_units_[unit_index].resource = nullptr;
    }
  }
}

void Renderer::ResourceBinder::ClearVertexArrayBinding(GLuint id) {
  if (!id || id == active_vertex_array_.gl_id) {
    active_vertex_array_.gl_id = 0;
    if (active_vertex_array_.resource &&
        active_vertex_array_.resource->GetElementArrayBinding().gl_id) {
      ClearBufferBinding(
          BufferObject::kElementBuffer,
          active_vertex_array_.resource->GetElementArrayBinding().gl_id);
    }
    active_vertex_array_.resource = nullptr;
  }
}

void Renderer::ResourceBinder::ClearNonFramebufferCachedBindings() {
  ClearBufferBindings(0U);
  ClearProgramBinding(0U);
  ClearTextureBindings(0U);
  const GLuint count = static_cast<GLuint>(GetImageUnitCount());
  for (GLuint i = 0U; i < count; ++i) {
    image_units_[i].sampler = 0;
  }
  active_image_unit_ = static_cast<GLuint>(image_units_.size() + 1U);
  ClearVertexArrayBinding(0U);
}

const ImagePtr Renderer::ResourceBinder::ReadImage(
    const math::Range2i& range, Image::Format format,
    const base::AllocatorPtr& allocator) {
  ImagePtr image(new(allocator) Image());
  const int x = range.GetMinPoint()[0];
  const int y = range.GetMinPoint()[1];
  const int width = range.GetSize()[0];
  const int height = range.GetSize()[1];

  GraphicsManager* gm = GetGraphicsManager().Get();
  Image::PixelFormat pf =
      GetCompatiblePixelFormat(Image::GetPixelFormat(format), gm);
  const size_t data_size = Image::ComputeDataSize(format, width, height);
  DataContainerPtr data = DataContainer::CreateOverAllocated<uint8>(
      data_size, nullptr, image->GetAllocator());
  memset(data->GetMutableData<uint8>(), 0, data_size);

  gm->PixelStorei(GL_PACK_ALIGNMENT, 1);
  gm->ReadPixels(x, y, width, height, pf.format, pf.type,
                 data->GetMutableData<uint8>());
  image->Set(format, width, height, data);

  return image;
}

void Renderer::ResourceBinder::SendUniform(const Uniform& uniform, int location,
                                           GraphicsManager* gm) {
#define SEND_VECTOR_UNIFORM(type, elem_type, num_elements, setter)             \
  if (uniform.IsArrayOf<type>()) {                                             \
    /* Check that the values are packed in the uniform data. */                \
    if (uniform.GetCount() > 1) {                                              \
      DCHECK_EQ(                                                               \
          reinterpret_cast<const elem_type*>(&uniform.GetValueAt<type>(1)),    \
          reinterpret_cast<const elem_type*>(&uniform.GetValueAt<type>(0)) +   \
              num_elements);                                                   \
    }                                                                          \
    gm->setter(                                                                \
        location, static_cast<GLsizei>(uniform.GetCount()),                    \
        reinterpret_cast<const elem_type*>(&uniform.GetValueAt<type>(0)));     \
  } else {                                                                     \
    gm->setter(location, 1,                                                    \
               reinterpret_cast<const elem_type*>(&uniform.GetValue<type>())); \
  }

#define SEND_TEXTURE_UNIFORM(type)                                             \
  /* Get the texture resource from each holder. */                             \
  if (uniform.IsArrayOf<type##Ptr>()) {                                        \
    const size_t count = uniform.GetCount();                                   \
    const base::AllocatorPtr& allocator =                                      \
        base::AllocationManager::GetDefaultAllocatorForLifetime(               \
            base::kShortTerm);                                                 \
    base::AllocVector<GLint> ids(allocator);                                   \
    ids.reserve(count);                                                        \
    int non_null_texture_count = 0;                                            \
    /* Each resource holds its own id. */                                      \
    for (size_t i = 0; i < count; ++i) {                                       \
      /* Null values in sampler arrays will be assigned the value of zero.  */ \
      /* This is not perfect, but we don't know what will happen if we send */ \
      /* an invalid value, such as -1. The array will be sent if at least   */ \
      /* one non-null texture is set.                                       */ \
      TextureResource* txr = nullptr;                                          \
      type* holder = uniform.GetValueAt<type##Ptr>(i).Get();                   \
      if (holder) txr = resource_manager_->GetResource(holder, this);          \
      ids.push_back(txr ? GetLastBoundUnit(txr) : 0);                          \
      if (txr) ++non_null_texture_count;                                       \
    }                                                                          \
    if (non_null_texture_count > 0)                                            \
      gm->Uniform1iv(location, static_cast<GLsizei>(count), &ids[0]);          \
  } else {                                                                     \
    TextureResource* txr = nullptr;                                            \
    type* holder = uniform.GetValue<type##Ptr>().Get();                        \
    /* Setting a null Texture as uniform value does not send anything. */      \
    if (holder) txr = resource_manager_->GetResource(holder, this);            \
    if (txr) gm->Uniform1i(location, GetLastBoundUnit(txr));                   \
  }

  // Ion stores matrices in row-major order and OpenGL expects column-major, so
  // transpose when sending them. Note that passing GL_TRUE as the transpose
  // argument to OpenGL does not work in GLES2.
  switch (uniform.GetType()) {
    case kIntUniform:
      SEND_VECTOR_UNIFORM(int, int, 1, Uniform1iv);
      break;
    case kFloatUniform:
      SEND_VECTOR_UNIFORM(float, float, 1, Uniform1fv);
      break;
    case kUnsignedIntUniform:
      SEND_VECTOR_UNIFORM(uint32, uint32, 1, Uniform1uiv);
      break;
    case kCubeMapTextureUniform:
      SEND_TEXTURE_UNIFORM(CubeMapTexture);
      break;
    case kTextureUniform:
      SEND_TEXTURE_UNIFORM(Texture);
      break;
    case kFloatVector2Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase2f, float, 2, Uniform2fv);
      break;
    case kFloatVector3Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase3f, float, 3, Uniform3fv);
      break;
    case kFloatVector4Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase4f, float, 4, Uniform4fv);
      break;
    case kIntVector2Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase2i, int, 2, Uniform2iv);
      break;
    case kIntVector3Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase3i, int, 3, Uniform3iv);
      break;
    case kIntVector4Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase4i, int, 4, Uniform4iv);
      break;
    case kUnsignedIntVector2Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase2ui, uint32, 2, Uniform2uiv);
      break;
    case kUnsignedIntVector3Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase3ui, uint32, 3, Uniform3uiv);
      break;
    case kUnsignedIntVector4Uniform:
      SEND_VECTOR_UNIFORM(math::VectorBase4ui, uint32, 4, Uniform4uiv);
      break;
    case kMatrix2x2Uniform:
      SendMatrixUniform<2>(uniform, gm, location,
                           &GraphicsManager::UniformMatrix2fv);
      break;
    case kMatrix3x3Uniform:
      SendMatrixUniform<3>(uniform, gm, location,
                           &GraphicsManager::UniformMatrix3fv);
      break;
    case kMatrix4x4Uniform: {
      SendMatrixUniform<4>(uniform, gm, location,
                           &GraphicsManager::UniformMatrix4fv);
      break;
    }
#if !defined(ION_COVERAGE)  // COV_NF_START
    // A Uniform type is explicitly set to a valid type.
    default:
      break;
#endif  // COV_NF_END
  }
#undef SEND_VECTOR_UNIFORM
#undef SEND_TEXTURE_UNIFORM
}

void Renderer::ResourceManager::DisassociateElementBufferFromArrays(
    BufferResource* resource) {
  ResourceAccessor accessor(resources_[kAttributeArray]);
  ResourceVector& resources = accessor.GetResources();
  const size_t count = resources.size();
  for (size_t i = 0; i < count; ++i) {
    VertexArrayResource* res =
        reinterpret_cast<VertexArrayResource*>(resources[i]);
    if (res->GetElementArrayBinding().resource == resource)
      res->SetElementArrayBinding(0, nullptr);
  }
}

void Renderer::ResourceBinder::PushUniforms(
    const base::AllocVector<Uniform>& uniforms) {
  const size_t num_uniforms = uniforms.size();
  const ShaderInputRegistry* prev_reg = nullptr;
  ShaderInputRegistryResource* sirr = nullptr;
  for (size_t i = 0; i < num_uniforms; ++i) {
    const Uniform& u = uniforms[i];
    const ShaderInputRegistry* reg = &u.GetRegistry();
    // Only call GetResource() and Update() if the current Uniform's
    // ShaderInputRegistry is differerent from the previous Uniform.
    if (reg != prev_reg) {
      prev_reg = reg;
      sirr = resource_manager_->GetResource(reg, this);
      CHECK(sirr);
      sirr->Update(this);
    }
    sirr->PushUniform(u);
  }
}

void Renderer::ResourceBinder::PopUniforms(
    const base::AllocVector<Uniform>& uniforms) {
  const size_t num_uniforms = uniforms.size();
  const ShaderInputRegistry* prev_reg = nullptr;
  ShaderInputRegistryResource* sirr = nullptr;
  for (size_t i = 0; i < num_uniforms; ++i) {
    const Uniform& u = uniforms[i];
    // Only call GetResource() if the current Uniform's ShaderInputRegistry
    // is differerent from the previous Uniform.
    const ShaderInputRegistry* reg = &u.GetRegistry();
    if (reg != prev_reg) {
      prev_reg = reg;
      sirr = resource_manager_->GetResource(reg, this);
      CHECK(sirr);
    }
    sirr->PopUniform(u);
  }
}

template <typename HolderType>
typename Renderer::HolderToResource<HolderType>::ResourceType*
Renderer::ResourceManager::CreateResource(const HolderType* holder,
                                          Renderer::ResourceBinder* binder,
                                          ResourceKey key, GLuint gl_id) {
  typedef typename HolderToResource<HolderType>::ResourceType ResourceType;
  // Use the holder's Allocator if it has one.
  const base::AllocatorPtr& allocator =
      holder->GetAllocator().Get() ? holder->GetAllocator()
                                   : GetAllocatorForLifetime(base::kMediumTerm);
  ResourceType* new_resource =
      new (allocator) ResourceType(binder, this, *holder, key, gl_id);
  AddResource(new_resource);
  return new_resource;
}

template <typename HolderType>
typename Renderer::HolderToResource<HolderType>::ResourceType*
Renderer::ResourceManager::GetResource(
    const HolderType* holder, Renderer::ResourceBinder* resource_binder,
    GLuint gl_id) {
  typedef typename HolderToResource<HolderType>::ResourceType ResourceType;

  DCHECK(holder);
  ResourceType* resource = nullptr;
  if (holder) {
    // There are two possibilities here:
    // 1) holder_resource is the resource we want.
    // 2) holder_resource is null, and we must create a resource.
    const ResourceKey key =
        GetResourceKey<ResourceType>(resource_binder, holder);
    if (Resource* holder_resource =
            static_cast<Resource*>(holder->GetResource(resource_index_, key))) {
      resource = static_cast<ResourceType*>(holder_resource);
    } else {
      // The holder does not have any resources associated with it.
      resource = CreateResource(holder, resource_binder, key, gl_id);
      holder->SetResource(resource_index_, key, resource);
    }
  }
  return resource;
}

// Specializations to fill data infos with information.
template <>
void Renderer::ResourceManager::FillDataFromRenderer(GLuint id,
                                                     PlatformInfo* info) {}

template <>
void Renderer::ResourceManager::FillDataFromRenderer(GLuint id,
                                                     TextureImageInfo* info) {
  // Look for the texture identified by the info.
  ResourceAccessor accessor(resources_[kTexture]);
  ResourceVector& resources = accessor.GetResources();
  if (const size_t count = resources.size()) {
    for (size_t i = 0; i < count; ++i) {
      TextureResource* tr = reinterpret_cast<TextureResource*>(resources[i]);
      if (static_cast<GLuint>(tr->GetId()) == id) {
        // This const_cast is ugly, but there doesn't seem to be a better way.
        info->texture.Reset(
            const_cast<TextureBase*>(&tr->GetTexture<TextureBase>()));
        if (info->texture->GetTextureType() == TextureBase::kCubeMapTexture) {
          const CubeMapTexture& tex = tr->GetTexture<CubeMapTexture>();
          for (int f = 0; f < 6; ++f) {
            info->images.push_back(GetCubeMapTextureImageOrMipmap(
                tex, static_cast<CubeMapTexture::CubeFace>(f)));
          }
        } else {
          const Texture& tex = tr->GetTexture<Texture>();
          info->images.push_back(GetTextureImageOrMipmap(tex));
        }
      }
    }
  }
}

// Specializations to fill ResourceInfos with Resource-specific information.
template <>
void Renderer::ResourceManager::FillInfoFromResource(
    ArrayInfo* info, VertexArrayResource* resource, ResourceBinder* rb) {
  info->vertex_count = resource->GetVertexCount();
}

template <>
void Renderer::ResourceManager::FillInfoFromResource(BufferInfo* info,
                                                     BufferResource* resource,
                                                     ResourceBinder* rb) {
  info->target = resource->GetGlTarget();
}

template <>
void Renderer::ResourceManager::FillInfoFromResource(
    FramebufferInfo* info, FramebufferResource* resource, ResourceBinder* rb) {
  const int max_attachments = GetGraphicsManager()->GetConstant<int>(
      GraphicsManager::kMaxColorAttachments);
  info->color.resize(max_attachments);
  info->color_renderbuffers.resize(max_attachments);
  for (int i = 0; i < max_attachments; ++i) {
    info->color_renderbuffers[i].id = resource->GetColorId(i);
  }
  info->depth_renderbuffer.id = resource->GetDepthId();
  info->stencil_renderbuffer.id = resource->GetStencilId();
}

template <>
void Renderer::ResourceManager::FillInfoFromResource(
    ProgramInfo* info, ShaderProgramResource* resource, ResourceBinder* rb) {
  if (ShaderResource* shader = resource->GetVertexResource())
    info->vertex_shader = shader->GetId();
  if (ShaderResource* shader = resource->GetGeometryResource())
    info->geometry_shader = shader->GetId();
  if (ShaderResource* shader = resource->GetTessControlResource())
    info->tess_ctrl_shader = shader->GetId();
  if (ShaderResource* shader = resource->GetTessEvaluationResource())
    info->tess_eval_shader = shader->GetId();
  if (ShaderResource* shader = resource->GetFragmentResource())
    info->fragment_shader = shader->GetId();
}

template <>
void Renderer::ResourceManager::FillInfoFromResource(
    TextureInfo* info, TextureResource* resource, ResourceBinder* rb) {
  info->unit = GL_TEXTURE0 + rb->GetLastBoundUnit(resource);
  info->target = resource->GetGlTarget();
  const Texture::TextureType type =
      resource->GetTexture<TextureBase>().GetTextureType();
  info->width = info->height = 0U;
  info->format = base::InvalidEnumValue<Image::Format>();
  Image* image = nullptr;
  if (type == TextureBase::kTexture) {
    const Texture& tex = resource->GetTexture<Texture>();
    if (tex.HasImage(0U))
      image = tex.GetImage(0U).Get();
  } else {  // kCubeMapTexture.
    const CubeMapTexture& tex = resource->GetTexture<CubeMapTexture>();
    if (tex.HasImage(CubeMapTexture::kNegativeX, 0))
      image = tex.GetImage(CubeMapTexture::kNegativeX, 0).Get();
  }
  if (image) {
    info->format = image->GetFormat();
    info->width = image->GetWidth();
    info->height = image->GetHeight();
  }
}

template <>
void Renderer::ResourceManager::FillInfoFromResource(
    TransformFeedbackInfo* info, TransformFeedbackResource* resource,
    ResourceBinder* rb) {
  const TransformFeedback& tf = resource->GetTransformFeedback();
  if (BufferObject* buf = tf.GetCaptureBuffer().Get()) {
    auto bufresource = GetResource(buf, rb);
    info->buffer = bufresource->GetId();
  } else {
    info->buffer = 0;
  }
  info->active = resource->IsCapturing();
}

// Instantiate the GetResource() function for supported resource types.
template Renderer::BufferResource* Renderer::ResourceManager::GetResource(
    const BufferObject*, Renderer::ResourceBinder*, GLuint);
template Renderer::TextureResource* Renderer::ResourceManager::GetResource(
    const CubeMapTexture*, Renderer::ResourceBinder*, GLuint);
template Renderer::FramebufferResource* Renderer::ResourceManager::GetResource(
    const FramebufferObject*, Renderer::ResourceBinder*, GLuint);
template Renderer::SamplerResource* Renderer::ResourceManager::GetResource(
    const Sampler*, Renderer::ResourceBinder*, GLuint);
template Renderer::ShaderResource* Renderer::ResourceManager::GetResource(
    const Shader*, Renderer::ResourceBinder*, GLuint);
template Renderer::ShaderProgramResource*
Renderer::ResourceManager::GetResource(const ShaderProgram*,
                                       Renderer::ResourceBinder*, GLuint);
template Renderer::TextureResource* Renderer::ResourceManager::GetResource(
    const Texture*, Renderer::ResourceBinder*, GLuint);
template Renderer::TransformFeedbackResource*
Renderer::ResourceManager::GetResource(const TransformFeedback*,
                                       Renderer::ResourceBinder*, GLuint);
template Renderer::VertexArrayResource* Renderer::ResourceManager::GetResource(
    const AttributeArray*, Renderer::ResourceBinder*, GLuint);

}  // namespace gfx
}  // namespace ion
