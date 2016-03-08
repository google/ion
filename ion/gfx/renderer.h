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

#ifndef ION_GFX_RENDERER_H_
#define ION_GFX_RENDERER_H_

#include <bitset>
#include <memory>

#include "base/integral_types.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocunorderedmap.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/graphicsmanager.h"
#include "ion/gfx/image.h"
#include "ion/gfx/node.h"
#include "ion/gfx/resourcemanager.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfx/statetable.h"
#include "ion/gfx/uniform.h"
#include "ion/math/range.h"

namespace ion {
namespace portgfx {
class Visual;
}  // namespace portgfx
namespace gfx {

class IndexBuffer;
class Shape;

// The Renderer class handles rendering ION scene graphs using OpenGL. It is
// also responsible for setting up the default shader program and global
// uniform values.
class ION_API Renderer : public base::Referent {
 public:
  // The below flags determine what operations will be performed in a call to
  // DrawScene(). The default set of flags are the kProcess* flags.
  enum Flag {
    // Process any outstanding requests for information about internal resources
    // that have been made through this Renderer's ResourceManager.
    kProcessInfoRequests,
    // Release any internal resources that have been marked for destruction,
    // including OpenGL objects.
    kProcessReleases,

    // Whether to clear (set to 0) certain GL objects when drawing is finished.
    // Both Ion's internal state _and_ OpenGL are cleared. Note that the Restore
    // flags below take precedence over these.
    kClearActiveTexture,  // Sets the active image unit to unit 0.
    kClearArrayBuffer,
    kClearCubemaps,  // Clears cubemaps from _all_ image units.
    kClearElementArrayBuffer,
    kClearFramebuffer,
    kClearSamplers,  // Clears samplers from _all_ image units.
    kClearShaderProgram,
    kClearTextures,  // Clears textures from _all_ image units.
    kClearVertexArray,

    // Whether to restore certain GL state types when drawing is finished. Note
    // that there is no support for restoring texture object state (what
    // textures are bound where).
    kRestoreActiveTexture,
    kRestoreArrayBuffer,
    kRestoreElementArrayBuffer,
    kRestoreFramebuffer,
    kRestoreShaderProgram,
    kRestoreStateTable,
    kRestoreVertexArray,

    // Whether to query OpenGL for current states and save them, which can be
    // restored using the above kRestore* flags. Note that GL query operations
    // can be slow, so these flags should be used sparingly.
    kSaveActiveTexture,
    kSaveArrayBuffer,
    kSaveElementArrayBuffer,
    kSaveFramebuffer,
    kSaveShaderProgram,
    kSaveStateTable,
    kSaveVertexArray,
  };
  static const int kNumFlags = kSaveVertexArray + 1;
  typedef std::bitset<kNumFlags> Flags;

  // The types of resources created by the renderer.
  enum ResourceType {
    kAttributeArray,
    kBufferObject,
    kFramebufferObject,
    kSampler,
    kShaderInputRegistry,
    kShaderProgram,
    kShader,
    kTexture,
  };
  static const int kNumResourceTypes = kTexture + 1;

  // The possible ways a BufferObject's data can be mapped. Buffers mapped
  // read-only must not be written to, and those mapped write-only must not be
  // read from. Violating these rules may cause crashes or other unpredictable
  // behavior.
  enum BufferObjectDataMapMode {
    kReadOnly,
    kReadWrite,
    kWriteOnly
  };

  // The constructor is passed a GraphicsManager instance to use for rendering.
  explicit Renderer(const GraphicsManagerPtr& gm);

  // Returns/sets rendering flags.
  const Flags& GetFlags() const { return flags_; }
  void ClearFlag(Flag flag) { flags_.reset(flag); }
  void ClearFlags(const Flags& flags) { flags_ &= ~flags; }
  void SetFlag(Flag flag) { flags_.set(flag); }
  void SetFlags(const Flags& flags) { flags_ |= flags; }

  // Convenience functions that return std::bitsets of Flags.
  static const Flags& AllFlags();
  static const Flags& AllClearFlags();
  static const Flags& AllProcessFlags();
  static const Flags& AllRestoreFlags();
  static const Flags& AllSaveFlags();

  // Returns the GraphicsManager passed to the constructor.
  const GraphicsManagerPtr& GetGraphicsManager() const;

  // Returns the ResourceManager for this renderer. The returned pointer has the
  // same lifetime as this Renderer.
  gfx::ResourceManager* GetResourceManager() const;

  // Binds the passed FramebufferObject; all future calls to DrawScene() will be
  // drawn into it. Passing a NULL pointer or an incomplete FramebufferObject
  // will bind the default framebuffer. The default framebuffer is the one bound
  // in OpenGL when the Renderer is created. Note that the Renderer stores only
  // a weak reference to the framebuffer object, so if all FramebufferObjectPtrs
  // to it are destroyed, the Renderer will revert to the default framebuffer.
  void BindFramebuffer(const FramebufferObjectPtr& fbo);

  // Returns the currently bound FramebufferObject. Note that this is related to
  // the currently bound Visual or GL context, not simply the Renderer instance
  // the function is called on. A return value of NULL indicates that either no
  // framebuffer is bound or that the system default framebuffer (the
  // framebuffer bound when the Renderer was created) is active. Note that
  // calling BindFramebuffer() with a NULL FramebufferObject binds this default
  // framebuffer (see above).
  const FramebufferObjectPtr GetCurrentFramebuffer() const;

  // Immediately creates internal GL resources for the passed holder, uploading
  // any data and binding the resource. Note that unlike
  // CreateOrUpdateResource() below, BindResource() only triggers an update if
  // any changes are pending, or a resource needs to be created. At minimum, a
  // resource will be bound (e.g., binding a texture or a buffer). This function
  // can only be called with BufferObject, FramebufferObject, IndexBuffer,
  // ShaderProgram, or Texture objects.
  //
  // Note that dependent or associated resources are not updated; only the
  // passed holder's resources are bound.
  template <typename T>
  void BindResource(T* holder);

  // This function is useful for uploading data to the hardware at a time
  // convenient to the caller when using multiple contexts. It updates the
  // resource associated with the passed holder or creates one if none yet
  // exists. It also ensures that the resource will be rebound the next time it
  // is traversed so that it can be bound on another context. If you only want
  // to ensure that an object has resources and is bound, use BindResource().
  //
  // This function also ensures that any dependents of holder are also updated.
  // For example, calling CreateOrUpdateResource() on a Texture will trigger an
  // update on any dependent FramebufferObjects.
  //
  // This function can only be called with AttributeArray, BufferObject,
  // IndexBuffer, ShaderProgram, or Texture objects. Any data associated with
  // the passed holder is sent to the graphics hardware; buffer and texture data
  // is uploaded, shader programs are compiled and linked, etc., but no uniform
  // values are sent nor vertex arrays created.
  template <typename T>
  void CreateOrUpdateResource(T* holder);
  // Traverses the scene rooted by the given node and creates or updates
  // resources for ShaderPrograms, Textures, and Shapes that require it. Any
  // disabled subtrees are skipped.
  void CreateOrUpdateResources(const NodePtr& node);
  // Creates or updates any resources necessary to draw the passed Shape, i.e.,
  // buffer data is uploaded.
  void CreateOrUpdateShapeResources(const ShapePtr& shape);

  // Mark an object for a forced update of GL resources. Calling this function
  // is equivalent to modifying the object and then reverting it back to the
  // initial state. See the documentation of CreateOrUpdateResource for a
  // description of what a resource update entails.
  template <typename T>
  void RequestForcedUpdate(T* holder);
  // Mark the passed object and its descendants for a forced resource update
  // the next time CreateOrUpdateResources or DrawScene is called.
  void RequestForcedUpdates(const NodePtr& node);
  // Mark a shape's resources for a forced update the next time
  // CreateOrUpdateResources or DrawScene is called.
  void RequestForcedShapeUpdates(const ShapePtr& shape);

  // Immediately updates OpenGL state with the settings in the passed
  // StateTable. Note that if the passed StateTable has a clear color, depth, or
  // stencil value set, the clear will be immediately executed.
  void ProcessStateTable(const StateTablePtr& state_table);

  // This function is useful for wrapping an OpenGL object created outside of
  // Ion, thus allowing it to be used with Ion objects. This function behaves
  // similarly to CreateOrUpdateResource() in that the resource is immediately
  // created and data uploaded, etc. This function can only be called with
  // BufferObject, IndexBuffer, or Texture objects. Calling this function with
  // an invalid ID has no effect (nothing is created).
  template <typename T>
  void CreateResourceWithExternallyManagedId(T* holder, uint32 gl_id);

  // Sets the initial value of a Uniform to the passed value. This value will be
  // used when neither a Node being rendered nor any of its ancestors override
  // the Uniform. In other words, a Uniform in a Node will override this value.
  // All Uniforms start with an invalid initial value.
  void SetInitialUniformValue(const Uniform& u);

  // Resolve a multisampled framebuffer 'ms_fbo' into a single sampled
  // framebuffer 'dest_fbo'. Caller is responsible to make sure ms_fbo and
  // dest_fbo are compatible for resolving.
  void ResolveMultisampleFramebuffer(const FramebufferObjectPtr& ms_fbo,
                                     const FramebufferObjectPtr& dest_fbo);

  // Draws the scene rooted by the given node into the currently bound
  // framebuffer.
  virtual void DrawScene(const NodePtr& node);
  // Process any outstanding requests for information about internal resources
  // that have been made through this Renderer's ResourceManager.
  void ProcessResourceInfoRequests();

  // Returns the OpenGL ID for the passed resource. A new resource will be
  // created if one does not already exist. This function must be called with a
  // valid OpenGL context bound. Note that calling this function on an
  // AttributeArray is a compile-time error.
  template <typename T>
  uint32 GetResourceGlId(T* holder);

  // Sets the _inclusive_ range of texture image units that the Renderer should
  // use. This is useful to ensure that the Renderer does not interfere with
  // other users of OpenGL. The Renderer will use the specified units if they
  // are valid, or log an error message and make no changes otherwise. Note that
  // to use all texture units, simply pass a range from 0 to a number greater
  // than the maximum number of units (e.g., INT_MAX).
  //
  // Note: Attempting to use a ShaderProgram that uses more textures than the
  // size of |units| may result in unexpected rendering behavior, as not all
  // textures may be bound properly.
  void SetTextureImageUnitRange(const math::Range1i& units);

  // In the below MapBuffer functions the passed BufferObject is assigned a
  // DataContainer (retrievable via BufferObject::GetMappedData()) with a
  // pointer that is mapped to memory allocated by the graphics driver if the
  // platform supports it. If the platform does not support mapped buffers then
  // a pointer is allocated using the BufferObject's Allocator instead. The
  // BufferObject must be unmapped with UnmapBufferObjectData() for the data to
  // be available for drawing. The mapped DataContainer's pointer is mapped
  // using the passed access mode; do not violate the mode or unexpected
  // behavior, such as crashes, may occur. No expectations should be made about
  // the content of write-only buffers.
  //
  // Note that a mapped buffer must be unmapped using UnmapBufferObjectData()
  // before it can be mapped again. Calling the below MapBuffer functions with
  // an already mapped BufferObject does nothing but log a warning message.
  //
  // This function maps a DataContainer of the same size as the BufferObject.
  void MapBufferObjectData(const BufferObjectPtr& buffer,
                           BufferObjectDataMapMode mode);
  // This function maps a DataContainer with the size of the passed range.
  // Passing an empty range or a range larger than the size of the BufferObject
  // does nothing but log a warning message. If the local platform does not
  // support mapping ranges then a local pointer is allocated, and will be
  // uploaded to the GPU via BufferSubData.
  void MapBufferObjectDataRange(const BufferObjectPtr& buffer,
                                BufferObjectDataMapMode mode,
                                const math::Range1ui& range);
  // Unmaps a previously mapped BufferObject and makes the data available to the
  // graphics hardware. Does nothing but log a warning message if called with an
  // unmapped BufferObject.
  void UnmapBufferObjectData(const BufferObjectPtr& buffer);

  // Returns the default shader program that the Renderer uses if no other
  // shader program is set.
  const ShaderProgramPtr& GetDefaultShaderProgram() const {
    return default_shader_;
  }

  // A Renderer manages graphics state using a StateTable. The Renderer
  // initializes its StateTable to the OpenGL default settings and assumes that
  // these settings are correct. If an application modifies these settings
  // without the Renderer's knowledge (even if another Renderer is doing it),
  // the application should call this function to let the Renderer update its
  // state from OpenGL. The window dimensions must be passed in so that
  // viewport and scissor box values can be set properly.
  void UpdateStateFromOpenGL(int window_width, int window_height);

  // This is more efficient than UpdateStateFromOpenGL(). Rather than querying
  // OpenGL to learn what the current state is, the passed StateTable is used to
  // update the Renderer's idea of what the current state of OpenGL is.
  void UpdateStateFromStateTable(const StateTablePtr& state_to_clear);

  // Returns the StateTable that the Renderer believes to represent the current
  // state of OpenGL.
  const StateTable& GetStateTable() const;

  // Updates the system default framebuffer to whatever framebuffer is currently
  // bound. This allows applications to create new system framebuffers as needed
  // and notify Ion that the default framebuffer has changed.
  void UpdateDefaultFramebufferFromOpenGL();

  // Notifies the Renderer that it cannot rely on internally cached bindings;
  // the next objects it encounters in a traversal will be rebound (even if they
  // already are in OpenGL). This function is also useful when changes are made
  // to OpenGL state outside of the Renderer, for example directly through a
  // GraphicsManager or another API, or if state changes are made in a different
  // thread via a shared context.
  void ClearCachedBindings();

  // Returns an image of the specified format that contains the contents of the
  // hardware framebuffer. The passed range specifies the area to be read. The
  // Allocator is used when creating the Image.
  const ImagePtr ReadImage(const math::Range2i& range, Image::Format format,
                           const base::AllocatorPtr& allocator);

  // In non-production builds, pushes |marker| onto the Renderer's tracing
  // stream marker stack, outputting the marker and indenting all calls until
  // the next call to PopDebugMarker(). Does nothing in production builds.
  void PushDebugMarker(const std::string& marker);
  // In non-production builds, pops a label off of the Renderer's tracing stream
  // marker stack. If the stack is empty and in production builds this does
  // nothing.
  void PopDebugMarker();

  // Immediately clears the internal resources of the passed ResourceHolder.
  template <typename HolderType>
  void ClearResources(const HolderType* holder);

  // Immediately clears all internal resources of the Renderer.
  void ClearAllResources();

  // Immediately releases all internal resources of the Renderer which are
  // pending release.
  void ReleaseResources();

  // Immediately clears the internal resources of all ResourceHolders of the
  // passed type.
  void ClearTypedResources(ResourceType type);

  // Destroys the internal state cache associated with the passed Visual. Does
  // nothing if the visual has no associated state cache.
  static void DestroyStateCache(const portgfx::Visual* visual);

  // Destroys the internal state cache associated with the current Visual or GL
  // context.
  static void DestroyCurrentStateCache();

  // Returns the amount of GPU memory used by the passed resource type. Note
  // that only BufferObjects, FrameBufferObjects, and (CubeMap)Textures are
  // considered to use GPU memory.
  size_t GetGpuMemoryUsage(ResourceType type) const;

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~Renderer() override;

 private:
  // Internal nested classes that define and manage resources for the Renderer.
  template <int NumModifiedBits> class Resource;
  class BufferResource;
  class FramebufferResource;
  class ResourceBinder;
  class ResourceManager;
  class SamplerResource;
  class ScopedLabel;
  class ShaderInputRegistryResource;
  class ShaderProgramResource;
  class ShaderResource;
  class TextureResource;
  class VertexArrayResource;
  class VertexArrayEmulatorResource;

  // Helper struct to map ResourceHolder to Resource types.
  template <typename T>
  struct HolderToResource;

  // Ideally this would be a map of unique_ptrs, but gcc < 4.5 (QNX) does not
  // allow containers of unique_ptrs.
  // TODO(user): Make this unique_ptr when all toolchains support it.
  typedef base::AllocUnorderedMap<size_t, std::shared_ptr<ResourceBinder> >
      ResourceBinderMap;
  static ResourceBinderMap& GetResourceBinderMap();
  ResourceBinder* GetOrCreateInternalResourceBinder(int line) const;
  ResourceBinder* GetInternalResourceBinder(size_t* visual_id) const;

  static const ShaderProgramPtr CreateDefaultShaderProgram(
      const base::AllocatorPtr& allocator);
  static void SetResourceHolderBit(const ResourceHolder* holder, int bit);

  // Flags that control Renderer behavior.
  Flags flags_;

  // Graphics resource management.
  std::unique_ptr<ResourceManager> resource_manager_;

  // The default shader program.
  ShaderProgramPtr default_shader_;
};

// Convenience typedef for shared pointer to a Renderer.
typedef base::ReferentPtr<Renderer>::Type RendererPtr;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_RENDERER_H_
