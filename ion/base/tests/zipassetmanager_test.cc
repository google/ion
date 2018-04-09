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

#include "ion/base/zipassetmanager.h"

#include <algorithm>
#include <chrono>  // NOLINT
#include <string>

#include "ion/base/invalid.h"
#include "ion/base/logchecker.h"
#include "ion/base/logging.h"
#include "ion/base/memoryzipstream.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/port/fileutils.h"
#include "ion/port/timer.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

ION_REGISTER_ASSETS(ZipAssetTest);

namespace ion {
namespace base {

namespace {

// Returns the contents of a file on disk.
static const std::string GetFileContents(const std::string& filename) {
  FILE* fp = port::OpenFile(filename, "rb");
  EXPECT_FALSE(fp == nullptr);
  char buffer[2048];
  const size_t count = fread(buffer, sizeof(buffer[0]), 2048, fp);
  fclose(fp);
  buffer[count] = 0;
  return buffer;
}

}  // anonymous namespace

TEST(ZipAssetManager, InvalidData) {
  const char data[] = "abcdefghijklmnop";
  EXPECT_FALSE(ZipAssetManager::RegisterAssetData(data, sizeof(data)));
}

TEST(ZipAssetManager, LoadAssetsAndReset) {
  // The asset file must be manually registered.
  ZipAssetTest::RegisterAssets();

  // Check that all of the files exist.
  EXPECT_TRUE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
  EXPECT_TRUE(ZipAssetManager::ContainsFile("zipasset_file2.txt"));
  EXPECT_TRUE(ZipAssetManager::ContainsFile("dir/file1.txt"));
  EXPECT_TRUE(ZipAssetManager::ContainsFile("dir/file2.txt"));
  EXPECT_TRUE(ZipAssetManager::ContainsFile("path/file1.txt"));
  EXPECT_TRUE(ZipAssetManager::ContainsFile("path/file2.txt"));

  EXPECT_FALSE(ZipAssetManager::ContainsFile("does_not_exist"));

  EXPECT_FALSE(ZipAssetManager::IsFileCached("zipasset_file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("zipasset_file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file2.txt"));

  // Check file contents.
  std::string f1_data("This is\nFile 1");
  std::string f2_data("This is\nFile\n2");
  EXPECT_EQ_ML(f1_data, ZipAssetManager::GetFileData("zipasset_file1.txt"));
  EXPECT_EQ_ML(f1_data, *ZipAssetManager::GetFileDataPtr("zipasset_file1.txt"));
  EXPECT_TRUE(ZipAssetManager::IsFileCached("zipasset_file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("zipasset_file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("__asset_manifest__.txt"));
  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("zipasset_file2.txt"));
  EXPECT_EQ_ML(f2_data, *ZipAssetManager::GetFileDataPtr("zipasset_file2.txt"));
  EXPECT_TRUE(ZipAssetManager::IsFileCached("zipasset_file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("__asset_manifest__.txt"));
  EXPECT_EQ_ML(f1_data, ZipAssetManager::GetFileData("dir/file1.txt"));
  EXPECT_EQ_ML(f1_data, *ZipAssetManager::GetFileDataPtr("dir/file1.txt"));

  EXPECT_TRUE(ZipAssetManager::IsFileCached("dir/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("__asset_manifest__.txt"));
  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("dir/file2.txt"));

  EXPECT_EQ_ML(f2_data, *ZipAssetManager::GetFileDataPtr("dir/file2.txt"));
  EXPECT_TRUE(ZipAssetManager::IsFileCached("dir/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("__asset_manifest__.txt"));
  EXPECT_EQ_ML(f1_data, ZipAssetManager::GetFileData("path/file1.txt"));
  EXPECT_EQ_ML(f1_data, *ZipAssetManager::GetFileDataPtr("path/file1.txt"));
  EXPECT_TRUE(ZipAssetManager::IsFileCached("path/file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("path/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("__asset_manifest__.txt"));
  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("path/file2.txt"));

  EXPECT_EQ_ML(f2_data, *ZipAssetManager::GetFileDataPtr("path/file2.txt"));
  EXPECT_TRUE(ZipAssetManager::IsFileCached("path/file2.txt"));

  EXPECT_TRUE(
      IsInvalidReference(ZipAssetManager::GetFileData("does_not_exist")));
  EXPECT_TRUE(!ZipAssetManager::GetFileDataPtr("does_not_exist"));

  // Verify that Reset() works.
  ZipAssetManager::Reset();
  EXPECT_FALSE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("zipasset_file1.txt"));
}

TEST(ZipAssetManager, RegisterAssetsOnce) {
  // The asset file must be manually registered.
  LogChecker log_checker;
  EXPECT_FALSE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
  ZipAssetTest::RegisterAssets();
  EXPECT_TRUE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
  EXPECT_FALSE(log_checker.HasAnyMessages());
  ZipAssetTest::RegisterAssets();
  EXPECT_TRUE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
#if !ION_PRODUCTION
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "registered multiple times"));
#endif
  ZipAssetManager::Reset();

  EXPECT_FALSE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
  ZipAssetTest::RegisterAssetsOnce();
  EXPECT_TRUE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
  ZipAssetTest::RegisterAssetsOnce();
  EXPECT_TRUE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
  ZipAssetTest::RegisterAssets();
  EXPECT_TRUE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
#if !ION_PRODUCTION
  EXPECT_TRUE(log_checker.HasMessage("WARNING", "registered multiple times"));
#endif
  ZipAssetManager::Reset();

  // This call will do nothing since the asset data has already been registered
  // once.
  ZipAssetTest::RegisterAssetsOnce();
  EXPECT_FALSE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
}

TEST(ZipAssetManager, GetFileDataNoCache) {
  // The asset file must be manually registered.
  ZipAssetTest::RegisterAssets();

  // Check expected initial conditions.
  EXPECT_TRUE(ZipAssetManager::ContainsFile("zipasset_file1.txt"));
  EXPECT_TRUE(ZipAssetManager::ContainsFile("dir/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::ContainsFile("does_not_exist"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("zipasset_file1.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file2.txt"));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("does_not_exist"));

  // Check file contents.
  // If this fails on Windows because the actual file has CR+LF instead of '\n',
  // then Git changed the line endings on you.  To disable this feature in Git:
  //    $ git config --global core.autocrlf false
  // And then rebuild your repository.
  // Details:  https://help.github.com/articles/dealing-with-line-endings
  std::string f1_data("This is\nFile 1");
  std::string f2_data("This is\nFile\n2");
  std::string out;
  EXPECT_TRUE(ZipAssetManager::GetFileDataNoCache("zipasset_file1.txt", &out));
  EXPECT_EQ_ML(f1_data, out);
  EXPECT_FALSE(ZipAssetManager::IsFileCached("zipasset_file1.txt"));

  EXPECT_TRUE(ZipAssetManager::GetFileDataNoCache("dir/file2.txt", &out));
  EXPECT_EQ_ML(f2_data, out);
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file2.txt"));

  EXPECT_FALSE(ZipAssetManager::GetFileDataNoCache("does_not_exist", &out));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("does_not_exist"));

  // Now populate file cache.
  EXPECT_EQ_ML(f1_data, ZipAssetManager::GetFileData("zipasset_file1.txt"));
  EXPECT_TRUE(ZipAssetManager::IsFileCached("zipasset_file1.txt"));

  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("dir/file2.txt"));
  EXPECT_TRUE(ZipAssetManager::IsFileCached("dir/file2.txt"));

  EXPECT_TRUE(IsInvalidReference(
      ZipAssetManager::GetFileData("does_not_exist")));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("does_not_exist"));

  // GetFileDataNoCache should return bytes and also clear cache.
  EXPECT_TRUE(ZipAssetManager::GetFileDataNoCache("zipasset_file1.txt", &out));
  EXPECT_EQ_ML(f1_data, out);
  EXPECT_FALSE(ZipAssetManager::IsFileCached("zipasset_file1.txt"));

  EXPECT_TRUE(ZipAssetManager::GetFileDataNoCache("dir/file2.txt", &out));
  EXPECT_EQ_ML(f2_data, out);
  EXPECT_FALSE(ZipAssetManager::IsFileCached("dir/file2.txt"));

  EXPECT_FALSE(ZipAssetManager::GetFileDataNoCache("does_not_exist", &out));
  EXPECT_FALSE(ZipAssetManager::IsFileCached("does_not_exist"));
  ZipAssetManager::Reset();
}

TEST(ZipAssetManager, SetFileData) {
  // The asset file must be manually registered.
  ZipAssetTest::RegisterAssets();
  std::string f1_data("This is\nFile 1");
  std::string f2_data("This is\nFile\n2");
  EXPECT_EQ_ML(f1_data, ZipAssetManager::GetFileData("zipasset_file1.txt"));
  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("zipasset_file2.txt"));
  EXPECT_EQ_ML(f1_data, ZipAssetManager::GetFileData("path/file1.txt"));
  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("path/file2.txt"));

  f1_data = "This is some new data for file 1.";
  EXPECT_TRUE(ZipAssetManager::SetFileData("zipasset_file1.txt", f1_data));
  EXPECT_EQ_ML(f1_data, ZipAssetManager::GetFileData("zipasset_file1.txt"));
  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("zipasset_file2.txt"));
  EXPECT_NEQ_ML(f1_data, ZipAssetManager::GetFileData("path/file1.txt"));
  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("path/file2.txt"));
  f2_data = "This is some new data for file 2.";
  EXPECT_TRUE(ZipAssetManager::SetFileData("zipasset_file2.txt", f2_data));
  EXPECT_EQ_ML(f1_data, ZipAssetManager::GetFileData("zipasset_file1.txt"));
  EXPECT_EQ_ML(f2_data, ZipAssetManager::GetFileData("zipasset_file2.txt"));
  EXPECT_NEQ_ML(f1_data, ZipAssetManager::GetFileData("path/file1.txt"));
  EXPECT_NEQ_ML(f2_data, ZipAssetManager::GetFileData("path/file2.txt"));

  EXPECT_FALSE(ZipAssetManager::SetFileData("does not exist", f1_data));
  ZipAssetManager::Reset();
}

TEST(ZipAssetManager, GetRegisteredFileNames) {
  // The asset file must be manually registered.
  ZipAssetTest::RegisterAssets();

  // List of all the files in the original reference IAD file
  const std::vector<std::string> filenames_reference = {
      "zipasset_file1.txt", "zipasset_file2.txt", "dir/file1.txt",
      "dir/file2.txt",      "path/file1.txt",     "path/file2.txt",
  };

  auto FilenameIsPresent = [](
      const std::vector<std::string>& list,
      const std::string& name) -> bool {
    return std::find(list.begin(), list.end(), name) != list.end();
  };

  std::vector<std::string> filenames =
      ZipAssetManager::GetRegisteredFileNames();
  EXPECT_EQ(filenames_reference.size(), filenames.size());
  for (const auto& name : filenames_reference) {
    EXPECT_TRUE(FilenameIsPresent(filenames, name));
  }

  std::string no_such_file = "no_such_file.txt";
  EXPECT_FALSE(FilenameIsPresent(filenames, no_such_file));

  // Registered filenames should be empty after a Reset().
  std::vector<std::string> empty_list;
  ZipAssetManager::Reset();
  EXPECT_EQ(empty_list, ZipAssetManager::GetRegisteredFileNames());
}

#if !ION_PRODUCTION
TEST(ZipAssetManager, DuplicateRegister) {
  LogChecker checker;
  ZipAssetTest::RegisterAssets();
  EXPECT_FALSE(checker.HasAnyMessages());
  ZipAssetTest::RegisterAssets();
  std::vector<std::string> warnings = checker.GetAllMessages();
  EXPECT_FALSE(warnings.empty());
  for (size_t i = 0; i < warnings.size(); ++i) {
    const std::string& warning = warnings[i];
    EXPECT_TRUE(warning.find("WARNING") == 0 &&
                warning.find("Duplicate entry:") != std::string::npos);
  }
  checker.ClearLog();
  ZipAssetManager::Reset();
}
#endif

// NaCl has no file support.
#if !defined(ION_PLATFORM_NACL)
TEST(ZipAssetManager, SaveFileDataUpdateFileIfChanged) {
  LogChecker checker;
  // Create a file and a manifest.
  const std::string zip_filename = "testfile.txt";
  const std::string temp_filename = port::GetTemporaryFilename();
  EXPECT_FALSE(temp_filename.empty());
  const std::string data = "Some file\ndata\nto tests with\n";
  const std::string new_data = "Some new file\ndata\nto test some more\n";
  // Open the temp file and write data into it.
  FILE* fp = port::OpenFile(temp_filename, "wb");
  EXPECT_FALSE(fp == nullptr);
  EXPECT_EQ(data.length(),
            fwrite(data.c_str(), sizeof(data[0]), data.length(), fp));
  fclose(fp);

  // Verify that the file has the data we wrote.
  EXPECT_EQ(data, GetFileContents(temp_filename));

  std::chrono::system_clock::time_point timestamp;
  EXPECT_TRUE(port::GetFileModificationTime(temp_filename, &timestamp));

  // Create a memory zipstream with the temp file and register it.
  MemoryZipStream zipstream;
  zipstream.AddFile(zip_filename, data);
  zipstream.AddFile("__asset_manifest__.txt",
                  zip_filename + "|" + temp_filename);

  ZipAssetManager::RegisterAssetData(zipstream.GetData().data(),
                                     zipstream.GetData().size());

  // Check that the manager has the file data and that we can change it.
  EXPECT_TRUE(ZipAssetManager::ContainsFile(zip_filename));
  EXPECT_EQ(data, ZipAssetManager::GetFileData(zip_filename));
  EXPECT_TRUE(ZipAssetManager::SetFileData(zip_filename, new_data));
  EXPECT_EQ(new_data, ZipAssetManager::GetFileData(zip_filename));
  // The file on disk should not have changed.
  EXPECT_EQ(data, GetFileContents(temp_filename));

  // Save the file, which should change the file on disk.
  EXPECT_TRUE(ZipAssetManager::SaveFileData(zip_filename));
  EXPECT_EQ(new_data, GetFileContents(temp_filename));

  // Sleep so that the files will have different modification times.
  port::Timer::SleepNSeconds(1);
  // Modify the file directly.
  const std::string changed_data = "A brave new\nworld\n";
  // Open the temp file and write some new data into it.
  fp = port::OpenFile(temp_filename, "wb");
  EXPECT_FALSE(fp == nullptr);
  EXPECT_EQ(changed_data.length(),
            fwrite(changed_data.c_str(), sizeof(changed_data[0]),
                   changed_data.length(), fp));
  fclose(fp);

  // Verify that the file has the data we wrote.
  EXPECT_EQ(changed_data, GetFileContents(temp_filename));
  // Ask the manager to reload anything that has changed.
  std::chrono::system_clock::time_point new_timestamp;
  EXPECT_TRUE(
      ZipAssetManager::UpdateFileIfChanged(zip_filename, &new_timestamp));
  EXPECT_GT(new_timestamp, timestamp);
  // We should now be able to get the new data.
  EXPECT_EQ(changed_data, ZipAssetManager::GetFileData(zip_filename));

  // Cleanup.
  EXPECT_TRUE(port::RemoveFile(temp_filename));

  // Check that we cannot save a non-existent file.
  EXPECT_FALSE(ZipAssetManager::SaveFileData("doesn't exist"));

  // Add a manifest that does not refer to a file on disk.
  MemoryZipStream zipstream2;
  zipstream2.AddFile(zip_filename, data);
  zipstream2.AddFile("__asset_manifest__.txt",
                   zip_filename + "|not/a/real/path/to/a/file.txt");
  ZipAssetManager::RegisterAssetData(zipstream2.GetData().data(),
                                     zipstream2.GetData().size());
  // Try to save the file, which should fail because the path does not exist.
  EXPECT_FALSE(ZipAssetManager::SaveFileData(zip_filename));
  checker.ClearLog();
  ZipAssetManager::Reset();
}
#endif

}  // namespace base
}  // namespace ion
