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

#ifndef ION_REMOTE_TESTS_GETUNUSEDPORT_H_
#define ION_REMOTE_TESTS_GETUNUSEDPORT_H_

#include <string>

struct sockaddr_storage;

namespace ion {
namespace remote {
namespace testing {
//
// GetUnusedPort tries up to |num_ports_to_try| ports before failing, returning
// the first available port it finds. If there is any problem trying to find an
// open port, it returns 0.
int GetUnusedPort(int num_ports_to_try);

}  // namespace testing
}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_TESTS_GETUNUSEDPORT_H_
