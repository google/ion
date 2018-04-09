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

#ifndef ION_BASE_ZIPASSETMANAGER_H_
#define ION_BASE_ZIPASSETMANAGER_H_

#include <chrono>  // NOLINT(build/c++11)
#include <map>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"

namespace ion {
namespace base {

// ZipAssetManager manages all zipfile assets in Ion. Assets are registered
// through RegisterAssetData(), which extracts all file names from the zip and
// adds them to its registry. Use GetFileData() to return the data of a file.
// Files are only extracted the first time they are requested through
// GetFileData(); file contents are cached internally after extraction.
//
// Note that zip assets must be explicitly registered through
// RegisterAssetData().
class ION_API ZipAssetManager {
 public:
  // The destructor is public so that the StaticDeleter that destroys the
  // manager can access it.
  ~ZipAssetManager();

  // Registers a data pointer with a certain length with the manager. The data
  // pointer must be valid at least as long as it takes for all of its files
  // to be requested through GetFileData(). Returns if the data is actual zipped
  // data and was successfully registered. Note that if any of the files in the
  // passed data have already been registered, the last version to be registered
  // will be returned from GetFileData().  This replacement of existing data
  // invalidates any use of previous return values of GetFileData() for the
  // replaced file.
  static bool RegisterAssetData(const void* data, size_t data_size);

  // Returns whether the manager contains the passed filename.
  static bool ContainsFile(const std::string& filename);

  // Returns whether the file is cached in the manager. A file is cached once it
  // has been requested with GetFileData. Returns false if the manager does not
  // contain the file.
  static bool IsFileCached(const std::string& filename);

  // Returns the list of registered filenames.
  static std::vector<std::string> GetRegisteredFileNames();

  // Returns the shared pointer to the data of the passed filename if the
  // manager contains it. If there is any error while decompressing the file or
  // if the file has not been registered then returns an empty shared pointer.
  // The returned "string" data in the pointer may change if the file changes.
  static std::shared_ptr<const std::string> GetFileDataPtr(
      const std::string& filename);
  // Returns the data of the passed filename if the manager contains it. If
  // there is any error while decompressing the file or if the file has not been
  // registered then returns an InvalidReference. The returned reference is only
  // valid so long as the registered file does not change.
  static const std::string& GetFileData(const std::string& filename);
  // As above but the decompressed bytes are not internally cached. Returns true
  // if |filename| is found or false otherwise. If file data is already cached
  // for |filename| then this method will clear that cached data.
  static bool GetFileDataNoCache(const std::string& filename, std::string* out);

  // If the source file of a zipped file is available on disk (based on the
  // file's manifest), this function updates the cached unzipped data from the
  // source file if it has changed since the data was registered and the source
  // file is readable. Reads the modification time of the passed filename into
  // |timestamp| and returns true iff the manager contains the file and it is
  // available; false otherwise.
  static bool UpdateFileIfChanged(
      const std::string& filename,
      std::chrono::system_clock::time_point* timestamp);

  // Sets the data of the passed filename if the manager contains it. Returns
  // whether the source was successfully updated (e.g., that the manager
  // contains filename).  Note that changing file data invalidates any existing
  // returned reference from GetFileData for that file.
  static bool SetFileData(const std::string& filename,
                          const std::string& source);

  // Attempts to save the latest cached data of the passed filename back to the
  // original source file on disk, using the name in the internal manifest.
  // Returns whether the file was successfully written. This may fail if the
  // file does not exist, is not writable, or if filename does not appear in the
  // manifest (e.g., if it was registered manually through RegisterAssetData()),
  // or if the manager does not contain the file.
  static bool SaveFileData(const std::string& filename);

  // Resets the manager back to its initial, empty state. This is used
  // primarily for testing and should generally not be necessary elsewhere.
  static void Reset();

 private:
  // Helper struct to associate a file with the zip data it came from and the
  // data it contains.
  struct FileInfo {
    // A zip_handle is the opaque pointer returned by the zlib library that
    // represents an open stream of zipped data.
    void* zip_handle;
    // The last time the file was modified.
    std::chrono::system_clock::time_point timestamp;
    // Empty pointer or pointer to the data of the extracted file if it has
    // been extracted.
    std::shared_ptr<std::string> data_ptr;
    // The original source file name on disk.
    std::string original_name;
  };
  typedef std::map<std::string, FileInfo> FileCache;

  // The constructor is private since this is a singleton class.
  ZipAssetManager();

  // Gets the file data or invalid reference if file doesn't exist.  Assumes
  // the manager mutex is already held.  Decompressed bytes will
  // be written to |out| if non-NULL, or to the internal cache (FileInfo.data)
  // if NULL. Return value is a reference to the decompressed byte string or
  // InvalidReference if the decompress failed.
  const std::string& GetFileDataLocked(const std::string& filename,
                                       std::string* out);

  // Returns whether the manager contains the passed filename.  Assumes the
  // manager mutex is already held.
  bool ContainsFileLocked(const std::string& filename);

  // Returns a pointer to the manager instance.
  static ZipAssetManager* GetManager();

  // Returns whether the cache contains the data of the file in the passed
  // FileInfo.
  static bool FileIsCached(const FileInfo& info);

  // Cache of files that have already been extracted.
  FileCache file_cache_;
  // Set of zipfiles that have been registered with the manager.
  std::set<void*> zipfiles_;

  // Mutex to guard access to asset data.
  std::mutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(ZipAssetManager);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_ZIPASSETMANAGER_H_
