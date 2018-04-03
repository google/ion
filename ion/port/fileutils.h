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

#ifndef ION_PORT_FILEUTILS_H_
#define ION_PORT_FILEUTILS_H_

#include <chrono>  // NOLINT
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace ion {
namespace port {

// Returns a canonical version of a file path string. Canonical form uses
// Unix-style separators ('/') on all platforms. Note that this changes paths
// only on platforms that don't use Unix-style separators.
ION_API std::string GetCanonicalFilePath(const std::string& path);

// Reads the last modification time of the passed file path into |time| and
// returns true, iff the file exists.  Otherwise returns false.
ION_API bool GetFileModificationTime(
    const std::string& path, std::chrono::system_clock::time_point* time);

// Returns a platform-dependent string that names the temporary directory.
// This is mostly useful for tests. If you need an actual file you can write,
// use GetTemporaryFilename.
ION_API std::string GetTemporaryDirectory();

// Returns a platform-dependent string that names a valid filename which may be
// opened for reading or writing. This creates an empty file, so make sure you
// call RemoveFile when you no longer need it.
ION_API std::string GetTemporaryFilename();

// Returns a platform-dependent string that is the current working directory.
ION_API std::string GetCurrentWorkingDirectory();

// Opens the file at path and returns a FILE pointer suitable for passing to
// fread, fwrite, fclose, etc. The mode parameter should have the same format as
// that passed to fopen ("w", "rb", etc.). Returns NULL if there is any error
// opening the file.
ION_API FILE* OpenFile(const std::string& path, const std::string& mode);

// Opens the file at path and read the contents of the file into a string.
// Returns false if there is any error opening the file.
ION_API bool ReadDataFromFile(const std::string& path, std::string* out);

// Attempts to remove the file at path and returns whether the file was
// successfully removed.
ION_API bool RemoveFile(const std::string& path);

// Returns the contents of |path|, non-recursively.  Only "." and ".." are
// excluded.
ION_API std::vector<std::string> ListDirectory(const std::string& path);

// Returns true if |path| exists.
ION_API bool FileExists(const std::string& path);

// Returns true if |path| is a directory.
ION_API bool IsDirectory(const std::string& path);

// Creates directory including all intermediate directories that do not exist.
// Permissions of the directory is platform dependent:
//   * For Unix-based file systems, directory has permissions read, write,
//     execute/search by owner.
//   * For Windows-based file systems, directory inherits ACL of parent
//     directory.
//
// Returns true if directory creation succeeded or directory already exists.
ION_API bool MakeDirectory(const std::string& directory);

// Deletes an empty directory. Returns true if directory successfully deleted.
ION_API bool RemoveEmptyDirectory(const std::string& directory);

// Deletes a directory and all of its contents. Returns false if the directory
// does not exist or an error occurred trying to delete a child file or
// directory. Function stops traversing contents if an error occurs.
ION_API bool RemoveDirectoryRecursively(const std::string& directory);

// A function that is called by DeleteTopLevelFiles() to determine whether
// the file or directory should be deleted. Return true if |path| should
// be removed.
typedef std::function<bool(const std::string& path)> ShouldDeleteFileFunc;

// Deletes top-level files and directories under |directory| that satisfy
// the predicate |should_delete_fn|. Predicate returns true if |path| should be
// deleted.
//
// Returns false if an error occurred trying to delete a file or if |directory|
// does not exist.
ION_API bool DeleteTopLevelFiles(
    const std::string& path,
    const std::function<bool(const std::string& path)>& should_delete_fn);

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_FILEUTILS_H_
