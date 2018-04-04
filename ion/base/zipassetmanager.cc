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

#include <cstring>
#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/scopedallocation.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/stringutils.h"
#include "ion/port/fileutils.h"

#include "third_party/unzip/unzip.h"
// unzip.h must be included before ioapi.h for types to be defined correctly.
#include "third_party/zlib/src/contrib/minizip/ioapi.h"

namespace ion {
namespace base {

namespace {

static const char kManifestFilename[] = "__asset_manifest__.txt";

}  // anonymous namespace

ZipAssetManager::ZipAssetManager() {}

ZipAssetManager::~ZipAssetManager() {
  Reset();
}

bool ZipAssetManager::RegisterAssetData(const void* data, size_t data_size) {
  // Open the zip data and add entries for its files to the cache.
  zlib_filefunc_def def;
  voidpf mem_zipfile =
      mem_simple_create_file(&def, const_cast<void*>(data), data_size);
  if (void* zipfile = unzAttach(mem_zipfile, &def)) {
    // Associate each file with the zipfile it comes from.
    FileInfo file_info;
    file_info.zip_handle = zipfile;
    ZipAssetManager* manager = GetManager();
    std::lock_guard<std::mutex> guard(manager->mutex_);
    manager->zipfiles_.insert(zipfile);

    bool contains_manifest = false;
    {
      // Buffer for filenames.
      static const int kBufSize = 1024;
      base::ScopedAllocation<char> buf(base::kShortTerm, kBufSize);
      unz_file_info info;
      do {
        if (unzGetCurrentFileInfo(zipfile, &info, buf.Get(), kBufSize, nullptr,
                                  0, nullptr, 0) == UNZ_OK) {
#if !ION_PRODUCTION
          if (manager->file_cache_.find(buf.Get()) !=
              manager->file_cache_.end()) {
            DLOG(WARNING)
                << "Same file registered multiple times risks use after free "
                << "if the result of GetFileData is still in use. "
                << "Duplicate entry: " << buf.Get();
          }
#endif
          manager->file_cache_[buf.Get()] = file_info;
          manager->file_cache_[buf.Get()].data_ptr.reset(new std::string());
          if (strcmp(kManifestFilename, buf.Get()) == 0)
            contains_manifest = true;
        }
      } while (unzGoToNextFile(zipfile) == UNZ_OK);
    }

    // Save the manifest mappings from local filenames to zip names.
    if (contains_manifest) {
      // Must not release the lock before erasing the manifest or another
      // registration may replace it while it is being read.
      const std::vector<std::string> mappings =
          base::SplitString(
              manager->GetFileDataLocked(kManifestFilename, nullptr), "\n");
      const size_t count = mappings.size();
      for (size_t i = 0; i < count; ++i) {
        std::vector<std::string> mapping =
            base::SplitString(mappings[i], "|");
        DCHECK_EQ(2U, mapping.size());
        while (base::StartsWith(mapping[0], "/"))
          base::RemovePrefix("/", &mapping[0]);
        FileCache::iterator it =
            manager->file_cache_.find(port::GetCanonicalFilePath(mapping[0]));
        DCHECK(it != manager->file_cache_.end());
        it->second.original_name = mapping[1];
        it->second.timestamp = std::chrono::system_clock::time_point();
        port::GetFileModificationTime(mapping[1], &it->second.timestamp);
      }
      // We don't need to save the manifest file data.
      manager->file_cache_.erase(kManifestFilename);
    }
    return true;
  } else {
    free(mem_zipfile);
    return false;
  }
}

bool ZipAssetManager::ContainsFile(const std::string& filename) {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  return manager->ContainsFileLocked(filename);
}

bool ZipAssetManager::ContainsFileLocked(const std::string& filename) {
  return file_cache_.find(filename) != file_cache_.end();
}

bool ZipAssetManager::IsFileCached(const std::string& filename) {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  FileCache::const_iterator it = manager->file_cache_.find(filename);
  return it != manager->file_cache_.end() &&
      manager->FileIsCached(it->second);
}

std::vector<std::string> ZipAssetManager::GetRegisteredFileNames() {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  std::vector<std::string> filenames;
  for (const auto& file : manager->file_cache_)
    filenames.push_back(file.first);
  return filenames;
}

std::shared_ptr<const std::string> ZipAssetManager::GetFileDataPtr(
    const std::string& filename) {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  if (!IsInvalidReference(manager->GetFileDataLocked(filename, nullptr))) {
    FileCache::iterator it = manager->file_cache_.find(filename);
    return it->second.data_ptr;
  }
  return std::shared_ptr<const std::string>();
}

const std::string& ZipAssetManager::GetFileData(const std::string& filename) {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  return manager->GetFileDataLocked(filename, nullptr);
}

bool ZipAssetManager::GetFileDataNoCache(const std::string& filename,
                                         std::string* out) {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  return !IsInvalidReference(manager->GetFileDataLocked(filename, out));
}

const std::string& ZipAssetManager::GetFileDataLocked(
    const std::string& filename, std::string* out) {
  if (!ContainsFileLocked(filename)) {
    // Return an empty string if the file does not exist in the archive.
    return InvalidReference<const std::string>();
  } else {
    FileCache::iterator it = file_cache_.find(filename);
    if (!FileIsCached(it->second)) {
      // If |out| is NULL, then we write to FileInfo cache.
      if (!out)
        out = it->second.data_ptr.get();
      else
        out->clear();
      // Try to extract the file from the zip.
      unz_file_info info;
      if (unzLocateFile(it->second.zip_handle, filename.c_str(), 0) == UNZ_OK &&
          unzOpenCurrentFile(it->second.zip_handle) == UNZ_OK &&
          unzGetCurrentFileInfo(it->second.zip_handle, &info, nullptr, 0,
                                nullptr, 0, nullptr, 0) == UNZ_OK) {
        // Resize the string to accommodate the data.
        out->resize(info.uncompressed_size);
        // Decompress the file.
        unzReadCurrentFile(it->second.zip_handle, &(*out)[0],
                           static_cast<unsigned int>(info.uncompressed_size));
      }
      return *out;
    } else if (out) {
      // |out| steals cached data, clearing it.
      out->clear();
      out->swap(*it->second.data_ptr);
      return *out;
    } else {
      return *it->second.data_ptr;
    }
  }
}

bool ZipAssetManager::SetFileData(const std::string& filename,
                                  const std::string& source) {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  if (!manager->ContainsFileLocked(filename)) {
    return false;
  } else {
    FileCache::iterator it = manager->file_cache_.find(filename);
    *it->second.data_ptr = source;
    it->second.timestamp = std::chrono::system_clock::now();
    return true;
  }
}

bool ZipAssetManager::SaveFileData(const std::string& filename) {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  const std::string& data = manager->GetFileDataLocked(filename, nullptr);
  // Find the filename in the cache.
  FileCache::const_iterator it = manager->file_cache_.find(filename);
  if (it != manager->file_cache_.end() &&
      !base::IsInvalidReference(data) && !it->second.original_name.empty()) {
    // Attempt to overwrite the file.
    if (FILE* fp = port::OpenFile(it->second.original_name, "wb")) {
      const size_t count =
          fwrite(data.c_str(), sizeof(data[0]), data.length(), fp);
      fclose(fp);
      return count == data.length();
    }
  }
  return false;
}

void ZipAssetManager::Reset() {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  for (std::set<void*>::iterator it = manager->zipfiles_.begin();
       it != manager->zipfiles_.end(); it++)
    unzClose(*it);
  manager->file_cache_.clear();
  manager->zipfiles_.clear();
}

bool ZipAssetManager::UpdateFileIfChanged(
    const std::string& filename,
    std::chrono::system_clock::time_point* timestamp) {
  ZipAssetManager* manager = GetManager();
  std::lock_guard<std::mutex> guard(manager->mutex_);
  std::chrono::system_clock::time_point new_timestamp;
  FileCache::iterator it = manager->file_cache_.find(filename);
  if (it != manager->file_cache_.end() && !it->second.original_name.empty()) {
    const bool found =
        port::GetFileModificationTime(it->second.original_name, &new_timestamp);
    if (found && new_timestamp > it->second.timestamp) {
      it->second.timestamp = new_timestamp;
      // Reload the file's source data.
      if (FILE* fp = port::OpenFile(it->second.original_name, "rb")) {
        // Determine the length of the file.
        fseek(fp, 0, SEEK_END);
        const size_t length = ftell(fp);
        rewind(fp);

        // Load the data.
        it->second.data_ptr->resize(length);
        fread(&((*it->second.data_ptr)[0]), sizeof(char), length, fp);
        fclose(fp);
      }
      *timestamp = new_timestamp;
      return true;
    }
  }

  return false;
}

ZipAssetManager* ZipAssetManager::GetManager() {
  // This ensures that the manager will be safely destroyed when the program
  // exits.
  ION_DECLARE_SAFE_STATIC_POINTER(ZipAssetManager, s_manager);
  return s_manager;
}

bool ZipAssetManager::FileIsCached(const FileInfo& info) {
  return !info.data_ptr->empty();
}

}  // namespace base
}  // namespace ion
