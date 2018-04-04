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

#ifndef ION_GFX_UNIFORMBLOCK_H_
#define ION_GFX_UNIFORMBLOCK_H_

#include "ion/gfx/resourceholder.h"
#include "ion/gfx/uniformholder.h"

namespace ion {
namespace gfx {

// A UniformBlock is a grouping of uniforms that can be easily shared between
// multiple Nodes; changing a Uniform in a UniformBlock will thus automatically
// change it for all Nodes that share the block. Note that adding a Uniform adds
// a _copy_ of the instance; to modify a uniform value use ReplaceUniform() or
// SetUniformValue[At]().
//
// 
// 3.1+/ES3+, UniformBlocks also share the same storage buffer object, even
// across multiple shader programs; simplifying the sending of uniform values to
// GL for multiple programs. This can vastly increase the speed of switching
// large sets of uniforms at once since the cost is only the const of a single
// value change.
class ION_API UniformBlock : public ResourceHolder, public UniformHolder {
 public:
  // Changes that affect this resource.
  enum Changes {
    kNumChanges = kNumBaseChanges,
  };

  // 
  // make UniformBlocks require a block name and possibly require a BufferObject
  // in the constructor.
  UniformBlock();

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~UniformBlock() override;
};

// Convenience typedef for shared pointer to a UniformBlock.
using UniformBlockPtr = base::SharedPtr<UniformBlock>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_UNIFORMBLOCK_H_
