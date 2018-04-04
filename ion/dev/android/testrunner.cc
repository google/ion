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

#include <jni.h>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

//-----------------------------------------------------------------------------
//
// Java interface functions.
//
//-----------------------------------------------------------------------------

#define JNI_EXPORT extern "C" __attribute__((visibility("default")))

JNI_EXPORT int Java___runner_name___TestRunner_nativeRun(
    JNIEnv* env, jobject thiz) {
  // Run the test.
  int argc = 2;
  char* argv[2];
  argv[0] = "__app_name__";
  argv[1] = "--gunit_output=xml:/data/test.xml";

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
