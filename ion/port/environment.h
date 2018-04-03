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

#ifndef ION_PORT_ENVIRONMENT_H_
#define ION_PORT_ENVIRONMENT_H_

#include <string>

namespace ion {
namespace port {

// Returns the value of the named environment variable. Returns an empty string
// if the variable does not exist.
ION_API const std::string GetEnvironmentVariableValue(const std::string& name);

// Sets the named environment variable to the passed value.
ION_API void SetEnvironmentVariableValue(const std::string& name,
                                         const std::string& value);

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_ENVIRONMENT_H_
