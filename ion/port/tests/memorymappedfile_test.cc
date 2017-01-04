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

#include "ion/port/memorymappedfile.h"

#include "ion/base/logging.h"
#include "ion/port/fileutils.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace port {

TEST(MemoryMappedFile, FailedConstructionWorksEverywhere) {
  MemoryMappedFile foo("/InvalidPath/Doesn'tExist");
  EXPECT_EQ(foo.GetData(), static_cast<const void*>(nullptr));
  EXPECT_EQ(foo.GetLength(), 0U);
}

#if !defined(ION_PLATFORM_NACL)
TEST(MemoryMappedFile, Map) {
  const std::string filename = GetTemporaryFilename();
  FILE* file = OpenFile(filename, "w");
  ASSERT_TRUE(file);
  std::string msg = "Hello, world!";
  ASSERT_EQ(msg.size(), fwrite(msg.data(), 1U, msg.size(), file));
  ASSERT_EQ(0, fclose(file));
  {
    // Create the MemoryMappedFile in a limited scope so that it is torn down
    // before the RemoveFile() below runs (needed for Windows).
    MemoryMappedFile mapped(filename);
    ASSERT_EQ(msg.size(), mapped.GetLength());
    ASSERT_TRUE(mapped.GetData());
    std::string mapped_msg(reinterpret_cast<const char*>(mapped.GetData()),
                           mapped.GetLength());
    ASSERT_EQ(msg, mapped_msg);
  }
  CHECK(RemoveFile(filename));
}

#endif  // !WINDOWS && !NACL

}  // namespace port
}  // namespace ion
