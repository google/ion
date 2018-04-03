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

#include "ion/base/memoryzipstream.h"

#include <algorithm>
#include <string>

#include "ion/base/allocationmanager.h"
#include "ion/base/logchecker.h"
#include "ion/base/logging.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

TEST(MemoryZipStream, Empty) {
  MemoryZipStream stream;
  // An "empty" zip stream is still valid zip data.
  EXPECT_TRUE(stream.GetData().empty());
  EXPECT_FALSE(stream.ContainsFile("foo.txt"));
  EXPECT_TRUE(stream.GetFileData("foo").empty());
}

TEST(MemoryZipStream, ZipStreams) {
  const std::string filename1("foo.txt");
  const std::string data1("Some file\ndata\n\nin a string.");
  const std::string filename2("foo2.txt");
  const std::string data2("Some more file\ndata\n\nin a string.");

  MemoryZipStream stream1;
  EXPECT_TRUE(stream1.GetData().empty());
  EXPECT_FALSE(stream1.ContainsFile(filename1));
  stream1.AddFile(filename1, data1);
  EXPECT_FALSE(stream1.GetData().empty());
  EXPECT_TRUE(stream1.ContainsFile(filename1));

  MemoryZipStream stream2;
  const MemoryZipStream::DataVector data1_vec(stream2.GetAllocator(),
                                              data1.begin(), data1.end());
  const MemoryZipStream::DataVector data2_vec(stream2.GetAllocator(),
                                              data2.begin(), data2.end());
  EXPECT_FALSE(stream2.ContainsFile(filename1));
  EXPECT_TRUE(stream2.GetData().empty());
  stream2.AddFile(filename1, data1_vec);
  EXPECT_TRUE(stream2.ContainsFile(filename1));
  EXPECT_FALSE(stream2.GetData().empty());

  // Check that both AddFile() routines add the same data.
  EXPECT_EQ(stream1.GetData(), stream2.GetData());

  // Check that both streams can decompress the data.
  EXPECT_EQ(data1_vec, stream1.GetFileData(filename1));
  EXPECT_EQ(data1_vec, stream2.GetFileData(filename1));

  // Add another file.
  EXPECT_FALSE(stream1.ContainsFile(filename2));
  stream1.AddFile(filename2, data2_vec);
  EXPECT_NE(stream1.GetData(), stream2.GetData());
  EXPECT_EQ(data1_vec, stream1.GetFileData(filename1));
  EXPECT_EQ(data2_vec, stream1.GetFileData(filename2));

  // Check that a stream can be constructed from valid zip data.
  MemoryZipStream stream3(stream1.GetData());
  EXPECT_FALSE(stream3.GetData().empty());
  EXPECT_EQ(stream1.GetData(), stream3.GetData());
  EXPECT_NE(stream2.GetData(), stream3.GetData());
  EXPECT_EQ(data1_vec, stream3.GetFileData(filename1));
  EXPECT_EQ(data2_vec, stream3.GetFileData(filename2));

  // Check that a stream can be constructed from valid zip data.
  MemoryZipStream stream4(stream2.GetData());
  EXPECT_FALSE(stream4.GetData().empty());
  EXPECT_NE(stream1.GetData(), stream4.GetData());
  EXPECT_EQ(stream2.GetData(), stream4.GetData());
  EXPECT_EQ(data1_vec, stream4.GetFileData(filename1));
  EXPECT_FALSE(stream4.ContainsFile(filename2));

  // Check that a stream can be swapped from valid zip data.
  MemoryZipStream::DataVector tmp_vec = stream2.GetData();
  MemoryZipStream stream5(&tmp_vec);
  EXPECT_FALSE(stream5.GetData().empty());
  // The temp vector should be empty since it was swap()ped.
  EXPECT_TRUE(tmp_vec.empty());
  EXPECT_NE(stream1.GetData(), stream5.GetData());
  EXPECT_EQ(stream2.GetData(), stream5.GetData());
  EXPECT_EQ(data1_vec, stream4.GetFileData(filename1));
  EXPECT_FALSE(stream5.ContainsFile(filename2));
}

TEST(MemoryZipStream, ZipErrors) {
  LogChecker log_checker;
  MemoryZipStream::DataVector vec(AllocationManager::GetDefaultAllocator());
  // Create a bad data vector.
  vec.push_back(15U);
  vec.push_back(27U);
  MemoryZipStream stream(vec);
  // Trying to perform an operation will trigger errors.
  EXPECT_FALSE(stream.ContainsFile("filename"));
  EXPECT_TRUE(log_checker.HasMessage("ERROR",
                                     "An error occurred in a MemoryZipStream"));

  // Create a truncated stream.
  const std::string filename1("foo.txt");
  const std::string data1("Some file\ndata\n\nin a string.");

  MemoryZipStream trunc_stream;
  trunc_stream.AddFile(filename1, data1);
  EXPECT_TRUE(trunc_stream.ContainsFile(filename1));
  // Corrupt the stream data.
  MemoryZipStream::DataVector* trunc_vec =
      const_cast<MemoryZipStream::DataVector*>(&trunc_stream.GetData());
  trunc_vec->resize(trunc_vec->size() / 2U);
  EXPECT_FALSE(trunc_stream.ContainsFile(filename1));
}

}  // namespace base
}  // namespace ion
