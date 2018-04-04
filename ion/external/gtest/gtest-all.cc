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

// Sometimes it's desirable to build Google Test by compiling a single file.
// This file serves this purpose.

// This line ensures that gtest.h can be compiled on its own, even
// when it's fused.
#include "third_party/googletest/googletest/include/gtest/gtest.h"

// The following lines pull in the real gtest *.cc files.
#include "third_party/googletest/googletest/src/gtest.cc"
#include "third_party/googletest/googletest/src/gtest-death-test.cc"
#include "third_party/googletest/googletest/src/gtest-filepath.cc"
#include "third_party/googletest/googletest/src/gtest-port.cc"
#include "third_party/googletest/googletest/src/gtest-printers.cc"
#include "third_party/googletest/googletest/src/gtest-test-part.cc"
#include "third_party/googletest/googletest/src/gtest-typed-test.cc"
