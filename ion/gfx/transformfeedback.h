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

#ifndef ION_GFX_TRANSFORMFEEDBACK_H_
#define ION_GFX_TRANSFORMFEEDBACK_H_

#include "base/macros.h"
#include "ion/base/referent.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/shaderprogram.h"

namespace ion {
namespace gfx {

// This corresponds to the OpenGL concept of a "transform feedback object",
// which can be used to capture the data produced by the vertex shader.
// For now, this is limited to single-buffer capture (INTERLEAVED_ATTRIBS),
// although we may extend this in the future.
class ION_API TransformFeedback : public ResourceHolder {
 public:
  // By convention, types that descend from ResourceHolder have a Changes enum,
  // with one constant per Field.
  enum Changes { kCaptureBufferChanged = kNumBaseChanges, kNumChanges };

  TransformFeedback()
      : capture_buffer_(kCaptureBufferChanged, BufferObjectPtr(), this) {}

  explicit TransformFeedback(const BufferObjectPtr& buffer)
      : capture_buffer_(kCaptureBufferChanged, buffer, this) {}

  void SetCaptureBuffer(const BufferObjectPtr& buffer) {
    capture_buffer_.Set(buffer);
  }
  BufferObjectPtr GetCaptureBuffer() const { return capture_buffer_.Get(); }

 private:
  // Note that we are not overriding OnNotify, which we'd need to do if we cared
  // about when the captured buffer contents change.
  ~TransformFeedback() override = default;
  Field<BufferObjectPtr> capture_buffer_;
};

// Convenience typedef for shared pointer to a TransformFeedback.
using TransformFeedbackPtr = base::SharedPtr<TransformFeedback>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TRANSFORMFEEDBACK_H_
