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

// These tests rely on trace streams, which are disabled in production builds.
#if !ION_PRODUCTION

#include "ion/gfx/tests/renderer_common.h"
#include "ion/gfx/bufferobject.h"

namespace ion {
namespace gfx {

template <typename IndexType>
BufferObject::ComponentType GetComponentType();

template <>
BufferObject::ComponentType GetComponentType<uint16>() {
  return BufferObject::ComponentType::kUnsignedShort;
}

template <>
BufferObject::ComponentType GetComponentType<uint32>() {
  return BufferObject::ComponentType::kUnsignedInt;
}

}  // namespace gfx
}  // namespace ion

#endif  // ION_PRODUCTION
