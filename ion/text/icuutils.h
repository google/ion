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

#ifndef ION_TEXT_ICUUTILS_H_
#define ION_TEXT_ICUUTILS_H_

#include <string>

namespace ion {
namespace text {

// Initializes ICU data by searching the given directory for an icudtXXX.dat
// file and loading data from there. This is exposed so that applications can
// initialize ICU themselves if they have a data file in a known location. Other
// applications may choose not to call this function and rely on it being called
// automatically by the first code that needs ICU in order to run.
//
// If |icu_data_directory_path| is empty, then attempts to find an ICU data file
// in known system directories or in the directory specified by the ION_ICU_DIR
// environment variable. Initialization is guaranteed to be attempted only once.
// Subsequent calls to this method will return the same value as the original
// call, regardless of passing different arguments. This method is threadsafe,
// but which invocation will actually attempt initialization is arbitrary.
//
// In builds where ION_USE_ICU is not defined this function will be a no-op and
// always return true.
//
// Returns true if initialization succeeded.
ION_API bool InitializeIcu(const std::string& icu_data_directory_path);

}  // namespace text
}  // namespace ion

#endif  // ION_TEXT_ICUUTILS_H_
