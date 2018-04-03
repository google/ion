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

#include "ion/port/fileutils.h"

#if defined(ION_PLATFORM_IOS)
#include <Foundation/Foundation.h>
#endif
#if defined(ION_PLATFORM_WINDOWS)
#include <windows.h>
#else
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#endif

#include <cassert>

#include "base/integral_types.h"
#include "ion/port/string.h"
#include "absl/base/macros.h"

namespace ion {
namespace port {

namespace {

bool MakeSingleDirectory(const std::string& directory) {
  if (directory.empty()) {
    return false;
  }
#if defined(ION_PLATFORM_WINDOWS)
  if (directory.size() > MAX_PATH) {
    return false;
  }
  const std::wstring wide = ion::port::Utf8ToWide(directory);
  // Use default security file descriptor. The ACLs from the parent directory
  // are inherited.
  if (::CreateDirectoryW(wide.c_str(), NULL)) {
    return true;
  }
  // Don't return false if directory already exists.
  return ::GetLastError() == ERROR_ALREADY_EXISTS;
#elif !defined(ION_PLATFORM_NACL)
  if (directory.size() > PATH_MAX) {
    return false;
  }
  errno = 0;
  // Make directory with permissions read, write, execute/search by owner.
  if (mkdir(directory.c_str(), S_IRWXU) == 0) {
    return true;
  }
  // Don't return false if directory already exists.
  return errno == EEXIST;
#endif
  return false;
}

}  // namespace

std::string GetCanonicalFilePath(const std::string& path) {
#if defined(ION_PLATFORM_WINDOWS)
  // Most Windows APIs accept slashes, so as part of canonicalization, we
  // convert backslashes to slashes.
  std::string canonical_path = path;
  const size_t length = canonical_path.length();
  for (size_t i = 0; i < length; ++i)
    if (canonical_path[i] == '\\')
      canonical_path[i] = '/';
  return canonical_path;

#else
  // Leave the path alone on other platforms.
  return path;
#endif
}

std::string GetCurrentWorkingDirectory() {
#if defined(ION_PLATFORM_WINDOWS)
  WCHAR pwd[MAX_PATH] = L"";
  const int result = ::GetCurrentDirectoryW(MAX_PATH, pwd);
  assert(0 < result && result < MAX_PATH);
  (void) result;  // for opt builds that elide the assert
  return GetCanonicalFilePath(WideToUtf8(pwd));
#else
  static const int kExpectedPathLength = 2048;
  std::vector<char> path(kExpectedPathLength);
  while (!getcwd(&path[0], static_cast<int>(path.size()))) {
    path.resize(path.size() * 2);
  }
  return &path[0];
#endif
}

bool GetFileModificationTime(const std::string& path,
                             std::chrono::system_clock::time_point* time) {
#if defined(ION_PLATFORM_WINDOWS)
  // GetFileTime returns a timestamp based on the Windows epoch of 01 January
  // 1601, but std::chrono::system_clock has the Unix epoch of 01 January 1970.
  // This shift corresponds to 280 years of 365 days and 89 years of 366 days
  // according to the gregorian calendar.  |kEpochOffset| is this shift in
  // GetFileTime() tick periods (100 nanoseconds).
  static const int64 kEpochOffset = 116444736000000000ULL;

  bool retval = false;
  const std::wstring wide = Utf8ToWide(path);
  HANDLE handle = ::CreateFileW(wide.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
  FILETIME create_time, access_time, write_time;
  // FILETIME structures have 100 nanosecond resolution.
  if (handle != INVALID_HANDLE_VALUE) {
    if (GetFileTime(handle, &create_time, &access_time, &write_time)) {
      const std::chrono::duration<int64, std::ratio<1, 10000000>> file_duration(
          (static_cast<int64>(write_time.dwHighDateTime) << 32) +
          write_time.dwLowDateTime - kEpochOffset);
      *time = std::chrono::system_clock::time_point(file_duration);
      retval = true;
    }
    CloseHandle(handle);
  }
  return retval;

#else
  struct stat info;
#  if defined(ION_PLATFORM_IOS) || defined(ION_PLATFORM_MAC)
#    define ION_STAT_SECONDS static_cast<uint64>(info.st_mtimespec.tv_sec)
#    define ION_STAT_NSECONDS static_cast<uint64>(info.st_mtimespec.tv_nsec)
#  elif defined(ION_PLATFORM_ANDROID) || defined(ION_GOOGLE_INTERNAL)
#    define ION_STAT_SECONDS static_cast<uint64>(info.st_mtime)
#    define ION_STAT_NSECONDS static_cast<uint64>(info.st_mtime_nsec)
#  elif defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_QNX) || \
        defined(ION_PLATFORM_NACL)
#    define ION_STAT_SECONDS static_cast<uint64>(info.st_mtime)
#    define ION_STAT_NSECONDS 0
#  elif defined(ION_PLATFORM_LINUX)
#    define ION_STAT_SECONDS static_cast<uint64>(info.st_mtim.tv_sec)
#    define ION_STAT_NSECONDS static_cast<uint64>(info.st_mtim.tv_nsec)
#  else
#    error No valid platform defined!
#  endif

  if (!stat(path.c_str(), &info)) {
    *time = std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::seconds(ION_STAT_SECONDS) +
            std::chrono::nanoseconds(ION_STAT_NSECONDS)));
    return true;
  }
  return false;

# undef ION_STAT_SECONDS
# undef ION_STAT_NSECONDS
#endif
}

std::string GetTemporaryDirectory() {
#if defined(ION_PLATFORM_ANDROID)
  // Android requires an absolute path.
  return std::string("/data/local/tmp");
#elif defined(ION_PLATFORM_IOS)
  return std::string([NSTemporaryDirectory() fileSystemRepresentation]);
#elif defined(ION_PLATFORM_WINDOWS)
  WCHAR temp_path[MAX_PATH] = L"";
  int size = ::GetTempPathW(MAX_PATH, temp_path);
  assert(0 < size && size < MAX_PATH);
  if (size > 0 && temp_path[size - 1] == L'\\') {
    temp_path[size - 1] = L'\0';
  }
  return GetCanonicalFilePath(WideToUtf8(temp_path));
#else
  // Other POSIX platforms must use /tmp.
  return std::string("/tmp");
#endif
}

std::string GetTemporaryFilename() {
  std::string return_path;
#if defined(ION_PLATFORM_WINDOWS)
  WCHAR temp_path[MAX_PATH] = L"";
  int size = ::GetTempPathW(MAX_PATH, temp_path);
  assert(0 < size && size < MAX_PATH);
  (void) size;
  WCHAR wide_path[MAX_PATH] = L"";
  const UINT unique = ::GetTempFileNameW(temp_path, L"ion", 0, wide_path);
  assert(unique != 0);
  (void) unique;
  return_path = GetCanonicalFilePath(WideToUtf8(wide_path));
#elif !defined(ION_PLATFORM_NACL)
  std::string path = GetTemporaryDirectory() + "/ionXXXXXX";
  // POSIX platforms actually create the file.
  const int fd = mkstemp(&path[0]);
  if (fd != -1) {
    close(fd);
    return_path = path;
  }
#endif
  return return_path;
}

FILE* OpenFile(const std::string& path, const std::string& mode) {
  const std::string canonical_path = GetCanonicalFilePath(path);
#if defined(ION_PLATFORM_WINDOWS)
  const std::wstring wide_path = Utf8ToWide(canonical_path);
  const std::wstring wide_mode = Utf8ToWide(mode);
  return _wfopen(wide_path.c_str(), wide_mode.c_str());
#else
  return fopen(path.c_str(), mode.c_str());
#endif
}

bool ReadDataFromFile(const std::string& path, std::string* out) {
  if (FILE* file = ion::port::OpenFile(path, "rb")) {
    // Determine the length of the file.
    fseek(file, 0, SEEK_END);
    const size_t length = ftell(file);
    if (length == 0U) {
      *out = "";
      fclose(file);
      return true;
    }
    rewind(file);

    // Load the data.
    out->resize(length);
    fseek(file, 0, SEEK_SET);
    fread(&out->at(0), sizeof(char), length, file);
    fclose(file);
    return true;
  }

  return false;
}

bool RemoveFile(const std::string& path) {
#if defined(ION_PLATFORM_NACL)
  return false;
#elif defined(ION_PLATFORM_WINDOWS)
  return ::DeleteFileW(Utf8ToWide(path).c_str()) != 0;
#else
  return unlink(path.c_str()) == 0;
#endif
}

std::vector<std::string> ListDirectory(const std::string& path) {
  std::vector<std::string> files;
#if defined(ION_PLATFORM_WINDOWS)
  std::wstring wild = ion::port::Utf8ToWide(path) + L"/*";
  WIN32_FIND_DATAW find_data;
  HANDLE find_handle = ::FindFirstFileW(wild.c_str(), &find_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    do {
      if (!lstrcmpW(find_data.cFileName, L".") ||
          !lstrcmpW(find_data.cFileName, L"..")) {
        continue;
      }
      files.push_back(ion::port::WideToUtf8(find_data.cFileName));
    } while (::FindNextFileW(find_handle, &find_data));
    ::FindClose(find_handle);
  }
#elif !defined(ION_PLATFORM_NACL)
  // This impl works on POSIX platforms that have decent filesystem support.
  // Add a NaCl impl if/when needed.
  if (DIR* dir = opendir(path.c_str())) {
    struct dirent dent_buf;
    struct dirent* dent;
    while (readdir_r(dir, &dent_buf, &dent) == 0 && dent) {
      const std::string file = dent->d_name;
      if (file != "." && file != "..")
        files.push_back(file);
    }
    closedir(dir);
  }
#endif
  return files;
}

bool FileExists(const std::string& path) {
#if defined(ION_PLATFORM_NACL)
  return false;
#elif defined(ION_PLATFORM_WINDOWS)
  const std::wstring wide = ion::port::Utf8ToWide(path);
  return ::GetFileAttributesW(wide.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
  struct stat info;
  return stat(path.c_str(), &info) == 0;
#endif
}

bool IsDirectory(const std::string& path) {
#if defined(ION_PLATFORM_NACL)
  return false;
#elif defined(ION_PLATFORM_WINDOWS)
  const std::wstring wide = ion::port::Utf8ToWide(path);
  const auto attr = ::GetFileAttributesW(wide.c_str());
  return attr != INVALID_FILE_ATTRIBUTES &&
      (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode) != 0;
#endif
}

bool MakeDirectory(const std::string& directory) {
  if (directory.empty()) {
    return false;
  }
  if (IsDirectory(directory)) {
    return true;
  }
  const std::string canonical_dir = GetCanonicalFilePath(directory);
  std::vector<std::string::size_type> slash_positions;

  // Find all slashes starting from the end of the path, and check if the
  // directory already exists. If not, save it in |slash_positions|.
  auto pos = canonical_dir.rfind('/');
  while (pos != std::string::npos &&
         !IsDirectory(canonical_dir.substr(0, pos + 1))) {
    slash_positions.push_back(pos);
    if (pos == 0) {
      break;
    }
    pos = canonical_dir.rfind('/', pos - 1);
  }

  // At this point, |slash_positions| contains all the directories we need to
  // create before |directory|, starting from the immediate parent. To create
  // them, we start from the last one.
  //
  // We do this since in some cases the user will have permissions to see a
  // directory but not its parent directory. Going in this order ensures we
  // don't think a parent directory is missing and try to create it when the
  // user just doesn't have permission to see it.
  for (auto it = slash_positions.rbegin(); it != slash_positions.rend(); ++it) {
    if (!MakeSingleDirectory(canonical_dir.substr(0, *it + 1))) {
      return false;
    }
  }

  // Make the final directory.
  return MakeSingleDirectory(canonical_dir);
}

bool RemoveEmptyDirectory(const std::string& directory) {
#if defined(ION_PLATFORM_NACL)
  return false;
#elif defined(ION_PLATFORM_WINDOWS)
  return ::RemoveDirectoryW(Utf8ToWide(directory).c_str()) != 0;
#else
  return rmdir(directory.c_str()) == 0;
#endif
}

bool RemoveDirectoryRecursively(const std::string& directory) {
#if defined(ION_PLATFORM_NACL)
  return false;
#else
  const std::vector<std::string> contents = ListDirectory(directory);
  for (const std::string& filename : contents) {
    const std::string filepath = directory + "/" + filename;
    const bool success = IsDirectory(filepath) ?
                             RemoveDirectoryRecursively(filepath) :
                             RemoveFile(filepath);
    if (!success) {
      return false;
    }
  }
  // At this point, all contents under this directory should have been removed.
  return RemoveEmptyDirectory(directory);
#endif
}

bool DeleteTopLevelFiles(const std::string& path,
    const std::function<bool(const std::string& path)>& should_delete_fn) {
#if defined(ION_PLATFORM_NACL)
  return false;
#else
  // IsDirectory will also return false if |parent_dir| does not exist.
  if (!IsDirectory(path)) {
    return false;
  }

  const std::vector<std::string> contents = ListDirectory(path);
  for (const std::string& filename : contents) {
    const std::string filepath = path + "/" + filename;
    if (should_delete_fn(filepath)) {
      const bool success = IsDirectory(filepath) ?
                               RemoveDirectoryRecursively(filepath) :
                               RemoveFile(filepath);
      if (!success) {
        return false;
      }
    }
  }
  return true;
#endif
}

}  // namespace port
}  // namespace ion
