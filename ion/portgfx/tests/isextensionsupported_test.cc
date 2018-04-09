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

#include "ion/portgfx/isextensionsupported.h"

#include "ion/base/logging.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(IsExtensionSupported, All) {
  using ion::portgfx::IsExtensionSupported;

  // OpenGL requires a context to be current to query strings.
  ion::portgfx::GlContextPtr gl_context =
      ion::portgfx::GlContext::CreateGlContext();
  ion::portgfx::GlContext::MakeCurrent(gl_context);
  if (!gl_context || !gl_context->IsValid()) {
    LOG(INFO) << "Unable to create an OpenGL context. This test "
              << "cannot run and will now exit.";
    return;
  }

  // Unit tests on Windows seem to be restricted to pre-OpenGL 2.0
  // functionality, so return early.
#if defined(ION_PLATFORM_WINDOWS)
  return;
#endif

  EXPECT_FALSE(IsExtensionSupported("not a real extension"));
  // For coverage, check for a couple of valid extensions. In general this is
  // difficult to test in a cross-platform way because different hardware will
  // support different extensions.
#if defined(ION_PLATFORM_LINUX)
  EXPECT_TRUE(IsExtensionSupported("vertex_array_object"));
  EXPECT_TRUE(IsExtensionSupported("occlusion_query"));
  // Expect that substrings of the extension do not match.  If either of these
  // test strings actually do become valid extensions, this test will need to be
  // updated.
  EXPECT_FALSE(IsExtensionSupported(""));
  EXPECT_FALSE(IsExtensionSupported("_"));
  EXPECT_FALSE(IsExtensionSupported("array"));
  EXPECT_FALSE(IsExtensionSupported("occlusion"));
#endif
}
