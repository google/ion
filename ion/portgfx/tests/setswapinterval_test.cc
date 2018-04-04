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

#include "ion/portgfx/setswapinterval.h"

#include "ion/base/logging.h"
#define GLCOREARB_PROTOTYPES  // For glGetString() to be defined.
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"
#include "ion/portgfx/isextensionsupported.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

TEST(SetSwapInterval, SetSwapInterval) {
  using ion::portgfx::SetSwapInterval;

  // Setting the swap interval only requires a valid GL context on non-Angle
  // Windows, but always creating one is fine.
  ion::portgfx::GlContextPtr gl_context =
      ion::portgfx::GlContext::CreateGlContext();
  if (!gl_context || !gl_context->IsValid()) {
    LOG(INFO) << "Unable to create an OpenGL context. This test "
              << "cannot run and will now exit.";
    return;
  }
  ion::portgfx::GlContext::MakeCurrent(gl_context);

  // Unit tests on Windows seem to be restricted to pre-OpenGL 2.0
  // functionality, so return early.
#if defined(ION_PLATFORM_WINDOWS)
  return;
#endif

  // Some Mesa implementations do not support changing the swap interval to 0
  // in certain modes.
  if (ion::portgfx::IsExtensionSupported("swap_control")) {
    EXPECT_TRUE(SetSwapInterval(0));
  }
  EXPECT_TRUE(SetSwapInterval(1));
  EXPECT_TRUE(SetSwapInterval(2));
  EXPECT_TRUE(SetSwapInterval(1));
  EXPECT_FALSE(SetSwapInterval(-1));
}
