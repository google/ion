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

#ifndef ION_BASE_MEMORYZIPSTREAM_H_
#define ION_BASE_MEMORYZIPSTREAM_H_

#include <memory>
#include <string>

#include "base/integral_types.h"
#include "ion/base/allocatable.h"
#include "ion/base/stlalloc/allocvector.h"

namespace ion {
namespace base {

// A MemoryZipStream represents ZIP data in memory; the data may represent one
// or more files or directories in ZIP format.
class MemoryZipStream : public Allocatable {
 public:
  typedef AllocVector<uint8> DataVector;

  // Helper struct to track the zip stream.
  struct ZipStreamInfo;

  // Constructs an empty MemoryZipStream.
  MemoryZipStream();
  // Constructs a MemoryZipStream using pre-existing zip data. This will make a
  // copy of the data. If the data is not valid zip data then errors will occur
  // when trying to decompress files. No additional validation is done.
  explicit MemoryZipStream(const DataVector& data);
  // Constructs a MemoryZipStream using pre-existing zip data. This will take
  // ownership of the data via a vector swap(). If the data is not valid zip
  // data then errors will occur when trying to decompress files. No additional
  // validation is done.
  explicit MemoryZipStream(DataVector* data);
  ~MemoryZipStream() override;

  // Compresses and adds a vector of data to this, associating it with the
  // passed filename. The filename may be any relative path, e.g.,
  // "foo/bar/bat.ext".
  void AddFile(const std::string& filename, const DataVector& data);
  // Compresses and adds string data to this, associating it with the passed
  // filename. The filename may be any relative path, e.g.,
  // "foo/bar/bat.ext".
  void AddFile(const std::string& filename, const std::string& data);

  // Returns whether this contains filename.
  bool ContainsFile(const std::string& filename);

  // Returns the file data for filename. If the file does not exist, then the
  // returned vector is empty.
  const DataVector GetFileData(const std::string& filename);

  // Gets the memory buffer backing this.
  const DataVector& GetData() const;

 private:
  // Initializes the callbacks used to work with zip streams.
  void InitCallbacks();

  std::unique_ptr<ZipStreamInfo> info_;
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_MEMORYZIPSTREAM_H_
