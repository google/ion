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

#include "ion/gfx/shaderinput.h"
#include <limits>

namespace ion {
namespace gfx {

uint64 ShaderInputBase::GetNewStamp() {
  // Since not all platforms support 64-bit atomic integers, use two uints to
  // get 64 bits of precision. 2^32 is probably not enough IDs for a single
  // long-running instance of a program. Start at 1U since 0U is reserved for
  // invalid Uniforms.
  static std::atomic<uint32> s_stamp1_counter(1U);
  static std::atomic<uint32> s_stamp2_counter(0U);

  uint64 stamp_loworder = s_stamp1_counter++;  // Post-increment
  uint64 stamp_highorder;
  // Increment the second counter only if the first just looped.
  if (stamp_loworder == std::numeric_limits<uint32>::max()) {
    stamp_highorder = s_stamp2_counter++;
  } else {
    stamp_highorder = s_stamp2_counter;
  }
  return stamp_loworder | (stamp_highorder << 32);
}

}  // namespace gfx
}  // namespace ion
