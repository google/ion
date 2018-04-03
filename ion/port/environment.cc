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

#include "ion/port/environment.h"

#include <cstdlib>

namespace ion {
namespace port {

#if defined(ION_PLATFORM_WINDOWS)
#undef GetEnvironmentVariable
#endif

const std::string GetEnvironmentVariableValue(const std::string& name) {
  char* var = getenv(name.c_str());
  return var ? var : std::string();
}

void SetEnvironmentVariableValue(const std::string& name,
                                 const std::string& value) {
#if defined(ION_PLATFORM_WINDOWS)
  const std::string env = name + "=" + value;
  _putenv(env.c_str());
#else
  setenv(name.c_str(), value.c_str(), 1);
#endif
}

}  // namespace port
}  // namespace ion
