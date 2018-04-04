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

#ifndef ION_GFX_RESOURCEBASE_H_
#define ION_GFX_RESOURCEBASE_H_

#include <stddef.h>
#include <cstdint>

namespace ion {
namespace gfx {

class ResourceHolder;

// Type of identifiers used to disambiguate between multiple resources
// created for the same Ion object by the renderer.
typedef intptr_t ResourceKey;

// ResourceBase is an internal abstract base class for managed resources.  For
// example, it allows scene graph objects to contain managed OpenGL resources
// without having to know anything about them.
class ResourceBase {
 public:
  // The constructor accepts a holder parameter to simplify control flow
  // during construction. However, the value returned by GetHolder() is managed
  // by the ResourceHolder, not by the ResourceManager.
  explicit ResourceBase(const ResourceHolder* holder, ResourceKey key)
      : holder_(holder),
        key_(key) {}
  virtual ~ResourceBase() {}

  // Retrieve the holder for which this resource was created.
  const ResourceHolder* GetHolder() const { return holder_; }

  // Retrieve a key that disambiguates between multiple resources created
  // for the same holder by the same resource manager.
  ResourceKey GetKey() const { return key_; }

  // Each derived class must define these to acquire and release its
  // resource. It must be safe to call these multiple times.

  // Invoked before a resource is released. If this is called when holder_
  // is null, it means this resource's holder is being destroyed.
  // If holder_ is non-null, it means the resource is destroyed for another
  // reason and should remove itself from the holder.
  virtual void OnDestroyed() = 0;

  // Informs the resource that something has changed and that it needs to update
  // itself.
  virtual void OnChanged(const int bit) = 0;

  // Returns the amount of GPU memory in bytes that this resource uses. Note
  // that this may be zero, in particular if the Resource has never been drawn.
  // The count does not include the memory used to hold simple state (e.g., the
  // OpenGL id) only memory used by objects such as textures, framebuffers, and
  // buffer objects.
  virtual size_t GetGpuMemoryUsed() const = 0;

 private:
  const ResourceHolder* holder_;
  const ResourceKey key_;

  friend class ResourceHolder;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_RESOURCEBASE_H_
