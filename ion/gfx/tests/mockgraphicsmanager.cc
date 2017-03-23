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

#include "ion/gfx/tests/mockgraphicsmanager.h"

#include <algorithm>
#include <bitset>
#include <set>
#include <string>
#include <vector>

#include "ion/base/allocator.h"
#include "ion/base/logging.h"
#include "ion/base/stringutils.h"
#include "ion/gfx/framebufferobject.h"
#include "ion/gfx/openglobjects.h"
#include "ion/gfx/tests/mockvisual.h"
#include "ion/math/range.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {
namespace testing {

//-----------------------------------------------------------------------------
//
// MockGraphicsManager class functions.
//
//-----------------------------------------------------------------------------
int64 MockGraphicsManager::GetCallCount() {
  return MockVisual::GetCallCount();
}

void MockGraphicsManager::ResetCallCount() {
  MockVisual::ResetCallCount();
}

void MockGraphicsManager::SetMaxBufferSize(GLsizeiptr size_in_bytes) {
  MockVisual::GetCurrent()->SetMaxBufferSize(size_in_bytes);
}

GLsizeiptr MockGraphicsManager::GetMaxBufferSize() const {
  return MockVisual::GetCurrent()->GetMaxBufferSize();
}

void MockGraphicsManager::SetForceFunctionFailure(
    const std::string& func_name, bool always_fails) {
  MockVisual::GetCurrent()->SetForceFunctionFailure(func_name, always_fails);
}

void MockGraphicsManager::SetErrorCode(GLenum error_code) {
  MockVisual::GetCurrent()->SetErrorCode(error_code);
}

void MockGraphicsManager::SetExtensionsString(const std::string& extensions) {
  MockVisual::GetCurrent()->SetExtensionsString(extensions);
  InitGlInfo();
}

void MockGraphicsManager::SetVendorString(const std::string& vendor) {
  MockVisual::GetCurrent()->SetVendorString(vendor);
  InitGlInfo();
}

void MockGraphicsManager::SetRendererString(const std::string& renderer) {
  MockVisual::GetCurrent()->SetRendererString(renderer);
  InitGlInfo();
}

void MockGraphicsManager::SetVersionString(const std::string& version) {
  MockVisual::GetCurrent()->SetVersionString(version);
  InitGlInfo();
}

void MockGraphicsManager::SetContextProfileMask(int mask) {
  MockVisual::GetCurrent()->SetContextProfileMask(mask);
  InitGlInfo();
}

void MockGraphicsManager::SetContextFlags(int value) {
  MockVisual::GetCurrent()->SetContextFlags(value);
  InitGlInfo();
}

// Global platform capability values.
#define ION_PLATFORM_CAP(type, name) \
  type MockGraphicsManager::Get ## name() const { \
    return MockVisual::GetCurrent()->Get ## name(); \
  } \
  void MockGraphicsManager::Set ## name(type value) { \
    MockVisual::GetCurrent()->Set ## name(value); \
    ClearCapabilityCache(); \
  }
#include "ion/gfx/tests/glplatformcaps.inc"

}  // namespace testing
}  // namespace gfx
}  // namespace ion
