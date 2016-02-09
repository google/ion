/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#ifndef ION_PORTGFX_GETGLPROCADDRESS_H_
#define ION_PORTGFX_GETGLPROCADDRESS_H_

namespace ion {
namespace portgfx {

// Returns a generic pointer to an OpenGL function or OpenGL extension function
// with the given name. Returns NULL if the function is not found. The caller
// must know whether the function is a "core" function or not, as they must be
// looked up differently from extensions.
ION_API void* GetGlProcAddress(const char* name, bool is_core);

}  // namespace portgfx
}  // namespace ion

#endif  // ION_PORTGFX_GETGLPROCADDRESS_H_
