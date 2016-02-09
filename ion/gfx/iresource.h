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

#ifndef ION_GFX_IRESOURCE_H_
#define ION_GFX_IRESOURCE_H_

#include <stddef.h>

namespace ion {
namespace gfx {

// IResource is an internal abstract base class for managed resources.  For
// example, it allows scene graph objects to contain managed OpenGL resources
// without having to know anything about them.
class IResource {
 public:
  IResource() {}
  virtual ~IResource() {}

  // Each derived class must define these to acquire and release its
  // resource. It must be safe to call these multiple times.

  // Invokes a function to respond to the destruction of the resource.
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
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_IRESOURCE_H_
