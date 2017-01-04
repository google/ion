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

#include "ion/text/icuutils.h"

#if defined(ION_USE_ICU)
// If ION_USE_ICU is undefined, this file is full of no-op functions, so wrap
// all the includes inside ION_USE_ICU being defined so they don't add any
// build time overhead when ION_USE_ICU is undefined.

#include <mutex>  // NOLINT(build/c++11): only using std::call_once, not mutex.
#include <vector>

#include "ion/base/stringutils.h"
#include "ion/port/environment.h"
#include "ion/port/fileutils.h"
#include "ion/port/memorymappedfile.h"

#include "third_party/icu/icu4c/source/common/unicode/udata.h"
#endif  // ION_USE_ICU

namespace ion {
namespace text {

#if defined(ION_USE_ICU)

namespace {

std::once_flag icu_initialize_once_flag;

// If |status| indicates a problem, log the error string and return
// false. Otherwise return true to indicate no error.
static bool CheckIcuStatus(UErrorCode status) {
  if (U_FAILURE(status)) {
    LOG(ERROR) << "ICU library error: " << u_errorName(status);
    return false;
  } else {
    return true;
  }
}

static void TryInitializeIcu(
    const std::string& icu_data_directory_path, bool* success) {
  *success = false;

  std::string icu_data_file_path;
  std::vector<std::string> files = port::ListDirectory(icu_data_directory_path);
  for (auto it = files.begin(); icu_data_file_path.empty() && it != files.end();
      ++it) {
    if (base::StartsWith(*it, "icudt") && base::EndsWith(*it, ".dat"))
      icu_data_file_path = icu_data_directory_path + *it;
  }
  if (icu_data_file_path.empty()) {
    LOG(ERROR) << "Unable to find ICU data file in: "
               << icu_data_directory_path;
    return;
  }

  port::MemoryMappedFile icu_mmap(icu_data_file_path);
  if (!icu_mmap.GetData() || !icu_mmap.GetLength()) {
    LOG(ERROR) << "Unable to memory map ICU data file: " << icu_data_file_path;
    return;
  }

  UErrorCode error = U_ZERO_ERROR;
  udata_setAppData(icu_data_file_path.c_str(), icu_mmap.GetData(), &error);
  CHECK(CheckIcuStatus(error));

  *success = true;
}

}  // namespace

bool InitializeIcu(const std::string& icu_data_directory_path_in) {
  static bool icu_initialized = false;
  std::call_once(
      icu_initialize_once_flag,
      [&]() {
        std::string icu_data_directory_path = icu_data_directory_path_in;
        if (icu_data_directory_path.empty()) {
          // On Android, the ICU data file is in /system/usr/icu/, but the
          // filename can change from system to system (e.g. icudt51l.dat on a
          // Moto X but icudt46l.dat on a Galaxy S3). We list the files in
          // that directory, and use what we find.
          // On Mac, there are ICU data file(s) in /usr/share/icu/. List and
          // use what we find.
          // Elsewhere, assume we're a developer and assume an environment
          // variable (set in a test or manually) will tell us where to look.
          icu_data_directory_path =
#if defined(ION_PLATFORM_ANDROID)
              "/system/usr/icu/";
#elif defined(ION_PLATFORM_MAC)
              "/usr/share/icu/";
#else
              port::GetEnvironmentVariableValue("ION_ICU_DIR");
#endif
        }
        if (icu_data_directory_path.empty()) {
          LOG(ERROR) << "No known ICU data directory.";
          return;
        }

        if (!base::EndsWith(icu_data_directory_path, "/") &&
            !base::EndsWith(icu_data_directory_path, "\\")) {
          icu_data_directory_path += "/";
        }

        TryInitializeIcu(icu_data_directory_path, &icu_initialized);
      });
  return icu_initialized;
}

#else  // ION_USE_ICU

bool InitializeIcu(const std::string&) { return true; }

#endif  // ION_USE_ICU

}  // namespace text
}  // namespace ion
