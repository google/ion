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

#ifndef ION_PORT_MEMORYMAPPEDFILE_H_
#define ION_PORT_MEMORYMAPPEDFILE_H_

#include "base/macros.h"

#if defined(ION_PLATFORM_WINDOWS)
#include <windows.h>
#include <winnt.h>
#endif

namespace ion {
namespace port {

// Read-only in-memory view of an entire file on disk.
class ION_API MemoryMappedFile {
 public:
  // Maps the file at |path|.  In case of error GetData() will be NULL.
  // Mappings start at offset 0, extend the length of the file, and are
  // read-only.
  explicit MemoryMappedFile(const std::string& path);
  ~MemoryMappedFile();

  // Returns a pointer to the head of the mapped region.
  const void* GetData() const;

  // Returns the length of the mapped region.
  size_t GetLength() const;

 private:
  void* data_;  // Beginning of the mapped region.
  size_t length_;  // Length of the mapped region.
#if defined(ION_PLATFORM_WINDOWS)
  HANDLE mapping_;
#endif

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryMappedFile);
};

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_MEMORYMAPPEDFILE_H_
