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

#include "ion/port/string.h"

#if defined(ION_PLATFORM_WINDOWS)
#include <windows.h>
#endif

namespace ion {
namespace port {

#if defined(ION_PLATFORM_WINDOWS)
// Ion and friends assume std::string paths are UTF-8.  For Windows, we must
// convert to and from UTF-16 in order to use the native Windows APIs.
std::wstring Utf8ToWide(const std::string& utf8) {
  std::wstring wide;
  if (!utf8.empty()) {
    wide.resize(utf8.size());  // Worst case:  one wchar_t for each byte
    const int size =
        ::MultiByteToWideChar(CP_UTF8, 0,
                              &utf8[0], static_cast<int>(utf8.size()),
                              &wide[0], static_cast<int>(wide.size()));
    wide.resize(size);
  }
  return wide;
}

std::string WideToUtf8(const std::wstring& wide) {
  std::string utf8;
  if (!wide.empty()) {
    utf8.resize(4 * wide.size());  // Worst case:  4 bytes for each wchar_t
    const int size =
        ::WideCharToMultiByte(CP_UTF8, 0,
                              &wide[0], static_cast<int>(wide.size()),
                              &utf8[0], static_cast<int>(utf8.size()),
                              nullptr, nullptr);
    utf8.resize(size);
  }
  return utf8;
}
#endif

}  // namespace port
}  // namespace ion

