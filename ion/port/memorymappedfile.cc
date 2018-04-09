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

// Exclude tricky-to-implement platforms until we need them.
#if defined(ION_PLATFORM_WINDOWS)
#include "ion/port/fileutils.h"
#include "ion/port/string.h"
#elif !defined(ION_PLATFORM_NACL)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define POSIX_LIKE_ENOUGH
#endif  // !WINDOWS && !NACL

namespace ion {
namespace port {

MemoryMappedFile::MemoryMappedFile(const std::string& path)
    : data_(nullptr), length_(0) {
#if defined(ION_PLATFORM_WINDOWS)
  const std::wstring wide = Utf8ToWide(path);
  HANDLE handle = ::CreateFileW(wide.c_str(), GENERIC_READ,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, 0, nullptr);
  if (handle != INVALID_HANDLE_VALUE) {
    mapping_ = ::CreateFileMapping(handle, nullptr, PAGE_READONLY, 0, 0,
                                   nullptr);
    if (mapping_) {
      data_ = ::MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0);
      LARGE_INTEGER size;
      if (::GetFileSizeEx(handle, &size)) {
        length_ = static_cast<size_t>(size.QuadPart);
      } else {
        ::UnmapViewOfFile(data_);
        data_ = nullptr;
      }
    }
    ::CloseHandle(handle);
  }
#elif defined(POSIX_LIKE_ENOUGH)
  struct stat path_stat = {0};
  if (stat(path.c_str(), &path_stat))
    return;
  length_ = static_cast<size_t>(path_stat.st_size);
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0)
    return;
  data_ = mmap(nullptr, length_, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_ == MAP_FAILED || close(fd)) {
    data_ = nullptr;
    return;
  }
#endif
}

MemoryMappedFile::~MemoryMappedFile() {
#if defined(ION_PLATFORM_WINDOWS)
  if (data_)
    ::UnmapViewOfFile(data_);
  if (mapping_)
    ::CloseHandle(mapping_);
#elif defined(POSIX_LIKE_ENOUGH)
  if (data_)
    munmap(data_, length_);
#endif  // POSIX_LIKE_ENOUGH
}

const void* MemoryMappedFile::GetData() const {
  return data_;
}

size_t MemoryMappedFile::GetLength() const {
  return length_;
}

}  // namespace port
}  // namespace ion
