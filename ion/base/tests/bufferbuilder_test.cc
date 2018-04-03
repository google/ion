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

#include "ion/base/bufferbuilder.h"

#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {
namespace {

TEST(BufferBuilderTest, TestConstructionAndSwap) {
  BufferBuilder b1;
  EXPECT_EQ(0U, b1.Size());
  EXPECT_EQ(std::string(), b1.Build());
  b1.AppendArray("test", 4);
  EXPECT_EQ("test", b1.Build());

  BufferBuilder b2(b1);
  EXPECT_EQ("test", b1.Build());
  EXPECT_EQ("test", b2.Build());

  BufferBuilder b3(std::move(b2));
  EXPECT_EQ("test", b1.Build());
  EXPECT_EQ(0U, b2.Size());  // NOLINT: testing use-after-move semantics
  EXPECT_EQ(std::string(), b2.Build());
  EXPECT_EQ("test", b3.Build());

  BufferBuilder b4;
  b4.AppendArray("test2", 5);
  EXPECT_EQ("test2", b4.Build());
  b4.swap(b2);
  EXPECT_EQ("test2", b2.Build());
  EXPECT_EQ("", b4.Build());

  {
    using std::swap;
    swap(b2, b4);
    EXPECT_EQ("", b2.Build());
    EXPECT_EQ("test2", b4.Build());
  }

  EXPECT_EQ("test", b1.Build());
  EXPECT_EQ("", b2.Build());
  EXPECT_EQ("test", b3.Build());
  EXPECT_EQ("test2", b4.Build());

  EXPECT_EQ(4U, b1.Size());
  EXPECT_EQ(0U, b2.Size());
  EXPECT_EQ(4U, b3.Size());
  EXPECT_EQ(5U, b4.Size());
}

TEST(BufferBuilderTest, TestAppend) {
  BufferBuilder b1;
  b1.AppendArray("foo", 3);
  BufferBuilder b2;
  b2.AppendArray("bar", 3);

  b1.Append(b2);
  EXPECT_EQ("foobar", b1.Build());
  EXPECT_EQ("bar", b2.Build());

  b1.Append(std::move(b2));
  EXPECT_EQ("foobarbar", b1.Build());
  EXPECT_EQ(0U, b2.Size());  // NOLINT: testing use-after-move semantics
  EXPECT_EQ(std::string(), b2.Build());

  b1.Append('c');
  EXPECT_EQ("foobarbarc", b1.Build());
}

TEST(BufferBuilderTest, TestLargeAppend) {
  static const int kNumStrings = 4 * 1024;
  static const size_t kStringLength = 37;

  std::mt19937 generator;
  std::uniform_int_distribution<int> random(' ', '~');
  std::vector<std::string> strings;
  for (int i = 0; i < kNumStrings; ++i) {
    std::string str;
    str.reserve(kStringLength);
    for (size_t j = 0; j < kStringLength; ++j) {
      str.push_back(static_cast<char>(random(generator)));
    }
  }

  BufferBuilder builder;
  for (const std::string& str : strings) {
    builder.AppendArray(str.data(), str.size());
  }

  EXPECT_EQ(std::accumulate(strings.begin(), strings.end(), std::string()),
            builder.Build());
}

}  // namespace
}  // namespace base
}  // namespace ion
