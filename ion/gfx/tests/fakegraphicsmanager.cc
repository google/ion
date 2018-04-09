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

#include "ion/gfx/tests/fakegraphicsmanager.h"

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
#include "ion/gfx/tests/fakeglcontext.h"
#include "ion/math/range.h"
#include "ion/math/utils.h"
#include "ion/math/vector.h"

namespace ion {
namespace gfx {
namespace testing {

//-----------------------------------------------------------------------------
//
// FakeGraphicsManager class functions.
//
//-----------------------------------------------------------------------------
int64 FakeGraphicsManager::GetCallCount() {
  return FakeGlContext::GetCallCount();
}

void FakeGraphicsManager::ResetCallCount() {
  FakeGlContext::ResetCallCount();
}

void FakeGraphicsManager::SetMaxBufferSize(GLsizeiptr size_in_bytes) {
  FakeGlContext::GetCurrent()->SetMaxBufferSize(size_in_bytes);
}

GLsizeiptr FakeGraphicsManager::GetMaxBufferSize() const {
  return FakeGlContext::GetCurrent()->GetMaxBufferSize();
}

void FakeGraphicsManager::SetForceFunctionFailure(
    const std::string& func_name, bool always_fails) {
  FakeGlContext::GetCurrent()->SetForceFunctionFailure(func_name, always_fails);
}

void FakeGraphicsManager::EnableInvalidGlEnumState(bool enable) {
  FakeGlContext::GetCurrent()->EnableInvalidGlEnumState(enable);
}

void FakeGraphicsManager::SetErrorCode(GLenum error_code) {
  // If error checking was enabled, there might be a cached last error code.
  // Call GetError to clear it before setting a code to return next time.
  GetError();
  FakeGlContext::GetCurrent()->SetErrorCode(error_code);
}

void FakeGraphicsManager::SetExtensionsString(const std::string& extensions) {
  FakeGlContext::GetCurrent()->SetExtensionsString(extensions);
  InitGlInfo();
}

void FakeGraphicsManager::SetVendorString(const std::string& vendor) {
  FakeGlContext::GetCurrent()->SetVendorString(vendor);
  InitGlInfo();
}

void FakeGraphicsManager::SetRendererString(const std::string& renderer) {
  FakeGlContext::GetCurrent()->SetRendererString(renderer);
  InitGlInfo();
}

void FakeGraphicsManager::SetVersionString(const std::string& version) {
  FakeGlContext::GetCurrent()->SetVersionString(version);
  InitGlInfo();
}

void FakeGraphicsManager::SetContextProfileMask(int mask) {
  FakeGlContext::GetCurrent()->SetContextProfileMask(mask);
  InitGlInfo();
}

void FakeGraphicsManager::SetContextFlags(int value) {
  FakeGlContext::GetCurrent()->SetContextFlags(value);
  InitGlInfo();
}

// Global platform capability values.
#define ION_WRAP_GL_VALUE(name, sname, gl_enum, Type, init)    \
  Type FakeGraphicsManager::Get##name() const {                \
    return FakeGlContext::GetCurrent()->Get##name();           \
  }                                                            \
  void FakeGraphicsManager::Set##name(Type value) {            \
    FakeGlContext::GetCurrent()->Set##name(std::move(value));  \
    ClearConstantCache();                                      \
  }
#include "ion/gfx/glconstants.inc"

}  // namespace testing
}  // namespace gfx
}  // namespace ion
