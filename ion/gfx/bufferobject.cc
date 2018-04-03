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

#include "ion/gfx/bufferobject.h"

#include "ion/base/enumhelper.h"
#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/portgfx/glheaders.h"
#include "absl/base/macros.h"

namespace ion {
namespace gfx {

BufferObject::BufferObject()
    : specs_(*this),
      data_(kDataChanged, BufferData(), this),
      initial_target_(kArrayBuffer),
      sub_data_(*this),
      sub_data_changed_(kSubDataChanged, false, this) {}

BufferObject::BufferObject(Target target)
    : specs_(*this),
      data_(kDataChanged, BufferData(), this),
      initial_target_(target),
      sub_data_(*this),
      sub_data_changed_(kSubDataChanged, false, this) {}

BufferObject::~BufferObject() {
  if (base::DataContainer* data = GetData().Get()) data->RemoveReceiver(this);
}

size_t BufferObject::AddSpec(const ComponentType type,
                             const size_t component_count,
                             const size_t byte_offset) {
  if (component_count > 4U) {
    LOG(ERROR) << "***ION: Elements must have no more than four components.";
    return base::kInvalidIndex;
  } else {
    Spec spec(type, component_count, byte_offset);
    size_t index = specs_.size();
    // See if a spec with the same definition already exists.
    for (size_t i = 0; i < index; ++i) {
      if (spec == specs_[i]) {
        index = i;
        break;
      }
    }
    // Only push a new spec if the parameters are unique.
    if (index == specs_.size())
      specs_.push_back(spec);
    return index;
  }
}

const BufferObject::Spec& BufferObject::GetSpec(
    const size_t element_index) const {
  if (element_index >= specs_.size()) {
    LOG(ERROR) << "***ION: Invalid element index " << element_index
               << " passed to BufferObject with " << specs_.size()
               << " elements.";
    return base::InvalidReference<BufferObject::Spec>();
  } else {
    return specs_[element_index];
  }
}

void BufferObject::OnNotify(const base::Notifier* notifier) {
  if (GetResourceCount()) {
    if (notifier == GetData().Get()) {
      OnChanged(kDataChanged);
      Notify();
    }
  }
}

// This destructor is in the .cc file because BufferSubData contains
// a BufferObjectPtr. Putting this destructor in the .h file would trigger
// the generation of a destructor for BufferObjectPtr at a point where
// BufferObject is still an incomplete type, which would cause either
// a compiler error or undefined behavior at runtime.
BufferObject::BufferSubData::~BufferSubData() {}

}  // namespace gfx

namespace base {

using gfx::BufferObject;

// Specialize for BufferObject::ComponentType.
template <> ION_API const EnumHelper::EnumData<BufferObject::ComponentType>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_INVALID_ENUM, GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT,
    GL_INT, GL_UNSIGNED_INT, GL_FLOAT, GL_FLOAT, GL_FLOAT, GL_FLOAT
  };
  static const char* kStrings[] = {
      "Invalid", "Byte", "Unsigned Byte", "Short", "Unsigned Short", "Int",
      "Unsigned Int", "Float", "Float Matrix Column 2", "Float Matrix Column 3",
      "Float Matrix Column 4"};
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<BufferObject::ComponentType>(
      base::IndexMap<BufferObject::ComponentType, GLenum>(
          kValues, ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

template <> ION_API const EnumHelper::EnumData<BufferObject::Target>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, GL_COPY_READ_BUFFER,
    GL_COPY_WRITE_BUFFER, GL_TRANSFORM_FEEDBACK_BUFFER
  };
  static const char* kStrings[] = {
    "ArrayBuffer", "Elementbuffer", "CopyReadBuffer", "CopyWriteBuffer",
    "TransformFeedbackBuffer"
  };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<BufferObject::Target>(
      base::IndexMap<BufferObject::Target, GLenum>(kValues,
                                                   ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

template <> ION_API const EnumHelper::EnumData<BufferObject::IndexedTarget>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_TRANSFORM_FEEDBACK_BUFFER
  };
  static const char* kStrings[] = {
    "TransformFeedbackBuffer"
  };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<BufferObject::IndexedTarget>(
      base::IndexMap<BufferObject::IndexedTarget, GLenum>(
          kValues, ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

// Specialize for BufferObject::UsageMode.
template <> ION_API const EnumHelper::EnumData<BufferObject::UsageMode>
EnumHelper::GetEnumData() {
  static const GLenum kValues[] = {
    GL_DYNAMIC_DRAW, GL_STATIC_DRAW, GL_STREAM_DRAW
  };
  static const char* kStrings[] = { "DynamicDraw", "StaticDraw", "StreamDraw" };
  ION_STATIC_ASSERT(ABSL_ARRAYSIZE(kValues) == ABSL_ARRAYSIZE(kStrings),
                    "EnumHelper size mismatch");
  return EnumData<BufferObject::UsageMode>(
      base::IndexMap<BufferObject::UsageMode, GLenum>(kValues,
                                                      ABSL_ARRAYSIZE(kValues)),
      kStrings);
}

}  // namespace base

}  // namespace ion
