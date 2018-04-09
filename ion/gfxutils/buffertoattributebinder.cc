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

#include "ion/gfxutils/buffertoattributebinder.h"

#include "base/integral_types.h"
#include "ion/base/logging.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfxutils {

#define DefineComponentFunctionPair(c_type, bo_type, count) \
  template <>                                               \
  ION_API gfx::BufferObject::ComponentType                  \
  GetComponentType<c_type>() {                              \
    return gfx::BufferObject::bo_type;                      \
  }                                                         \
  template <>                                               \
  ION_API size_t GetComponentCount<c_type>() {              \
    return count;                                           \
  }

#define DefineComponentFunctions(c_type, bo_type, suffix)              \
  DefineComponentFunctionPair(c_type, bo_type, 1U);                    \
  DefineComponentFunctionPair(math::VectorBase1##suffix, bo_type, 1U); \
  DefineComponentFunctionPair(math::VectorBase2##suffix, bo_type, 2U); \
  DefineComponentFunctionPair(math::VectorBase3##suffix, bo_type, 3U); \
  DefineComponentFunctionPair(math::VectorBase4##suffix, bo_type, 4U);

DefineComponentFunctions(char, kByte, i8);
DefineComponentFunctions(unsigned char, kUnsignedByte, ui8);
DefineComponentFunctions(int16, kShort, i16);
DefineComponentFunctions(uint16, kUnsignedShort, ui16);
DefineComponentFunctions(int32, kInt, i);
DefineComponentFunctions(uint32, kUnsignedInt, ui);
DefineComponentFunctions(float, kFloat, f);
DefineComponentFunctionPair(math::Matrix2f, kFloatMatrixColumn2, 2U);
DefineComponentFunctionPair(math::Matrix3f, kFloatMatrixColumn3, 3U);
DefineComponentFunctionPair(math::Matrix4f, kFloatMatrixColumn4, 4U);

#undef DefineComponentFunctions
#undef DefineComponentFunctionPair

}  // namespace gfxutils
}  // namespace ion
