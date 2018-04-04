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

#ifndef ION_PORTGFX_ISEXTENSIONSUPPORTED_H_
#define ION_PORTGFX_ISEXTENSIONSUPPORTED_H_

#include <string>

namespace ion {
namespace portgfx {

// Returns whether the currently bound OpenGL implementation supports the named
// extension. Names are generally of the form GL_<BODY>_name, where <BODY> is
// usually one of APPLE, AMD, ARB, ATI, EXT, INTEL, KHR, NV, OES, SGI[SX],
// WEBGL. For maximum compatibility with various implementations, it is often
// best to only pass the name without the GL or <BODY>.
ION_API bool IsExtensionSupported(const std::string& unprefixed_extension,
                                  const std::string& extensions_string);

// A convenience wrapper around the above which takes the unprefixed extension
// directly as a C string.
// Note that to call this function a valid OpenGL context must be bound.
ION_API bool IsExtensionSupported(const char* unprefixed_extension);

}  // namespace portgfx
}  // namespace ion

#endif  // ION_PORTGFX_ISEXTENSIONSUPPORTED_H_
