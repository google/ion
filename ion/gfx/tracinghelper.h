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

#ifndef ION_GFX_TRACINGHELPER_H_
#define ION_GFX_TRACINGHELPER_H_

#include <string>
#include <unordered_map>

namespace ion {
namespace gfx {

// This internal class is used by the GraphicsManager to print argument values
// when tracing OpenGL calls.
class ION_API TracingHelper {
 public:
  // The constructor sets up the data necessary to map OpenGL constant values
  // to mnemonic names.
  TracingHelper();

  // This templated function is used to print each OpenGL function argument in
  // a more readable way. The unspecialized version just converts the type to a
  // string in the conventional way. There are specialized versions to handle
  // quoting strings, replacing numbers with names, etc..
  template <typename T> const std::string ToString(const char* arg_type, T arg);

 private:
  // Indexed vector mapping OpenGL constant values to constant names.
  std::unordered_map<int, std::string> constants_;
};

#if !ION_PRODUCTION
// Specialize the ToString() function for types that have special processing.
template <> ION_API const std::string TracingHelper::ToString(
    const char* arg_type, char* arg);
template <> ION_API const std::string TracingHelper::ToString(
    const char* arg_type, char** arg);
template <> ION_API const std::string TracingHelper::ToString(
    const char* arg_type, const char* arg);
template <> ION_API const std::string TracingHelper::ToString(
    const char* arg_type, const char** arg);
template <> ION_API const std::string TracingHelper::ToString(
    const char* arg_type, unsigned char arg);
template <> ION_API const std::string TracingHelper::ToString(
    const char* arg_type, unsigned int arg);
#endif

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TRACINGHELPER_H_
