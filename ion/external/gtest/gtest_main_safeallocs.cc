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

#include <stdio.h>

#include "ion/base/staticsafedeclare.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

#if defined(ION_PLATFORM_NACL)
#include "ppapi/cpp/module.h"

namespace pp {
Module* CreateModule() {
  // Do nothing.  We don't (yet) have any test code requiring a module.
  return nullptr;
}
}  // namespace pp
#endif

GTEST_API_ int main(int argc, char **argv) {
  printf("Running main() from gtest_main.cc\n");
  testing::InitGoogleTest(&argc, argv);
#if defined(ION_GOOGLE_INTERNAL) && defined(ION_PLATFORM_LINUX)
  ::InitGoogle(argv[0], &argc, &argv, false);
#endif
  const int ret = RUN_ALL_TESTS();
  ion::base::StaticDeleterDeleter::DestroyInstance();
  return ret;
}
