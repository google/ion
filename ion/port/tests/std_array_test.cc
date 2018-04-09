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

#include <array>
#include <string>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

// These tests are to verify that array works on the local platform.

TEST(Array, StringArray) {
  typedef std::array<std::string, 5> ArrayType;
  ArrayType myarray;

  myarray[0] = "Barbara";
  myarray[1] = "Lisa";
  myarray[2] = "John";

  std::string name = myarray[0];
  myarray[3] = name;

  myarray[0] = myarray[1];

  name = myarray[4];  // Non-existing element: should be empty.

  EXPECT_EQ("Lisa", myarray[0]);
  EXPECT_EQ("Lisa", myarray[1]);
  EXPECT_EQ("John", myarray[2]);
  EXPECT_EQ("Barbara", myarray[3]);
  EXPECT_EQ("", name);
}

// This test is based on:
// http://www.cplusplus.com/reference/array/array/operator[]/
TEST(Array, Int) {
  typedef std::array<int, 10> ArrayType;
  ArrayType myarray;

  // Assign some values.
  for (int i = 0; i < 10; i++)
    myarray[i] = i;

  // Check content.
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(i, myarray[i]);
}
