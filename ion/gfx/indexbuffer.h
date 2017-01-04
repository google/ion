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

#ifndef ION_GFX_INDEXBUFFER_H_
#define ION_GFX_INDEXBUFFER_H_

#include "ion/gfx/bufferobject.h"

namespace ion {
namespace gfx {

// An IndexBuffer is a type of BufferObject that contains the element indices of
// an array, e.g., a vertex index array.
class ION_API IndexBuffer : public BufferObject {
 public:
  IndexBuffer();

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~IndexBuffer() override;
};

using IndexBufferPtr = base::SharedPtr<IndexBuffer>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_INDEXBUFFER_H_
