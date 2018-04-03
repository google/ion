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

#include <cstring>

#include "ion/base/logging.h"
#include "ion/base/staticsafedeclare.h"
#include "third_party/unzip/unzip.h"
#include "third_party/zlib/src/contrib/minizip/zip.h"

namespace ion {
namespace base {

// Wrapper struct around a zlib function info struct and the backing store of
// the zipfile.
struct MemoryZipStream::ZipStreamInfo : public Allocatable {
  ZipStreamInfo() : position(0U), buffer(GetAllocator()) {}
  zlib_filefunc_def def;
  size_t position;
  DataVector buffer;
};

namespace {

// The zip API requires using long, which lint doesn't like.
typedef long ZipLong;  // NOLINT

// "Opens" a zip stream. Really just returns the opaque pointer to a
// ZipStreamInfo.
static voidpf ZCALLBACK
    OpenZipStream(voidpf opaque, const char* filename, int mode) {
  if (MemoryZipStream::ZipStreamInfo* info =
        reinterpret_cast<MemoryZipStream::ZipStreamInfo*>(opaque))
    info->position = 0U;
  return opaque;
}

// Reads data from a zip stream and returns the number of bytes read.
static uLong ZCALLBACK
ReadFromZipStream(voidpf opaque, voidpf stream, void* buf, uLong size) {
  uLong bytes_read = 0;
  if (MemoryZipStream::ZipStreamInfo* info =
      reinterpret_cast<MemoryZipStream::ZipStreamInfo*>(opaque)) {
    if (info->position + size > info->buffer.size()) {
       const int64 available = info->buffer.size() - info->position;
       size = available < 0 ? 0 : static_cast<uLong>(available);
    }
    if (size) {
      memcpy(buf, &info->buffer[info->position], size);
      info->position += size;
      bytes_read = size;
    }
  }
  return bytes_read;
}

// Writes data to a zip stream and returns the number of bytes written. If the
// stream is too short to hold the data it is resized.
static uLong ZCALLBACK WriteToZipStream(
    voidpf opaque, voidpf stream, const void* buf, uLong size) {
  uLong bytes_written = 0;
  if (MemoryZipStream::ZipStreamInfo* info =
      reinterpret_cast<MemoryZipStream::ZipStreamInfo*>(opaque)) {
    if (info->position + size > info->buffer.size())
      info->buffer.resize(info->position + size);

    memcpy(&info->buffer[info->position], buf, size);
    info->position += size;

    bytes_written = size;
  }
  return bytes_written;
}

// Returns the current position in the zip stream.
static ZipLong ZCALLBACK TellZipStream(voidpf opaque, voidpf stream) {
  ZipLong position = 0;
  if (MemoryZipStream::ZipStreamInfo* info =
        reinterpret_cast<MemoryZipStream::ZipStreamInfo*>(opaque))
    position = static_cast<ZipLong>(info->position);
  return position;
}

// Seeks to a position in the sip ztream.
static ZipLong ZCALLBACK
SeekZipStream(voidpf opaque, voidpf stream, uLong offset, int origin) {
  bool ok = true;
  if (MemoryZipStream::ZipStreamInfo* info =
        reinterpret_cast<MemoryZipStream::ZipStreamInfo*>(opaque)) {
    ok = false;
    const int64 size = info->buffer.size();
    if (origin == SEEK_SET) {
      ok = true;
      info->position = offset;
    } else if (origin == SEEK_CUR) {
      ok = (static_cast<size_t>(offset) + info->position) <=
           static_cast<size_t>(size);
      if (ok)
        info->position += offset;
    } else if (origin == SEEK_END) {
      const int64 new_pos = size - static_cast<int64>(offset);
      ok = new_pos >= 0 && new_pos <= size;
      if (ok)
        info->position = offset + static_cast<size_t>(size);
    }
  }
  return ok ? 0 : -1;
}

// "Closes" a zip stream.
static int ZCALLBACK CloseZipStream(voidpf opaque, voidpf stream) {
  return 0;
}

// Reports an error in the zip stream.
static int ZCALLBACK ErrorZipStream(voidpf opaque, voidpf stream) {
  // There isn't much information available about what the error was.
  LOG(ERROR) << "An error occurred in a MemoryZipStream";
  return -1;
}

struct ScopedUnzip {
  explicit ScopedUnzip(MemoryZipStream::ZipStreamInfo* info)
      : handle(!info->buffer.empty() ? unzAttach(&info->def, &info->def)
                                     : nullptr) {}
  ~ScopedUnzip() {
    if (handle)
      unzDetach(&handle);
  }
  explicit operator bool() { return handle != nullptr; }

  unzFile handle;
};

struct ScopedZip {
  explicit ScopedZip(MemoryZipStream::ZipStreamInfo* info)
      : handle(!info->buffer.empty()
                   ? zipOpen2("", APPEND_STATUS_ADDINZIP, nullptr, &info->def)
                   : zipOpen2("", APPEND_STATUS_CREATE, nullptr, &info->def)) {}
  ~ScopedZip() {
    if (handle)
      zipClose(handle, "");
  }
  explicit operator bool() { return handle != nullptr; }

  zipFile handle;
};

}  // anonymous namespace

MemoryZipStream::MemoryZipStream()
    : info_(new(GetAllocator()) ZipStreamInfo()) {
  InitCallbacks();
}

MemoryZipStream::MemoryZipStream(const DataVector& data)
    : info_(new(GetAllocator()) ZipStreamInfo()) {
  info_->buffer = data;
  InitCallbacks();
}

MemoryZipStream::MemoryZipStream(DataVector* data)
    : info_(new(GetAllocator()) ZipStreamInfo()) {
  info_->buffer.swap(*data);
  InitCallbacks();
}

MemoryZipStream::~MemoryZipStream() {}

void MemoryZipStream::AddFile(const std::string& filename,
                              const DataVector& data) {
  if (ScopedZip zip = ScopedZip(info_.get())) {
    zipOpenNewFileInZip(
        zip.handle, filename.c_str(), nullptr, nullptr, 0, nullptr, 0, nullptr,
        Z_DEFLATED, Z_BEST_COMPRESSION);
    zipWriteInFileInZip(zip.handle, data.data(),
                        static_cast<unsigned int>(data.size()));
    zipCloseFileInZip(zip.handle);
  }
}

void MemoryZipStream::AddFile(const std::string& filename,
                              const std::string& data) {
  DataVector vec(*this, data.begin(), data.end());
  AddFile(filename, vec);
}

bool MemoryZipStream::ContainsFile(const std::string& filename) {
  bool contains = false;
  if (ScopedUnzip unz = ScopedUnzip(info_.get()))
    contains = unzLocateFile(unz.handle, filename.c_str(), 0) == UNZ_OK;
  return contains;
}

const MemoryZipStream::DataVector MemoryZipStream::GetFileData(
    const std::string& filename) {
  DataVector data(*this);
  if (ScopedUnzip unz = ScopedUnzip(info_.get())) {
    // Try to extract the file from the zip.
    unz_file_info info;
    // This corresponds to the definition of UNZ_MAXFILENAMEINZIP in unzip.c.
    static const int kStringLength = 256;
    char extra_field[kStringLength + 1];
    char comment[kStringLength + 1];
    if (unzLocateFile(unz.handle, filename.c_str(), 0) == UNZ_OK &&
        unzOpenCurrentFile(unz.handle) == UNZ_OK &&
        unzGetCurrentFileInfo(unz.handle, &info, nullptr, 0, extra_field,
                              kStringLength, comment, kStringLength) ==
            UNZ_OK) {
      // Resize the output buffer to accommodate the data.
      data.resize(info.uncompressed_size);
      // Decompress the file.
      unzReadCurrentFile(unz.handle, &data[0],
                         static_cast<unsigned int>(info.uncompressed_size));
    }
  }
  return data;
}

const MemoryZipStream::DataVector& MemoryZipStream::GetData() const {
  return info_->buffer;
}

void MemoryZipStream::InitCallbacks() {
  // Set up our callbacks.
  info_->def.zopen_file = OpenZipStream;
  info_->def.zread_file = ReadFromZipStream;
  info_->def.zwrite_file = WriteToZipStream;
  info_->def.ztell_file = TellZipStream;
  info_->def.zseek_file = SeekZipStream;
  info_->def.zclose_file = CloseZipStream;
  info_->def.zerror_file = ErrorZipStream;
  info_->def.opaque = info_.get();
}

}  // namespace base
}  // namespace ion
