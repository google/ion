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

#ifndef ION_PORT_STRING_H_
#define ION_PORT_STRING_H_

#include <cstring>
#include <string>

#if defined(ION_PLATFORM_QNX)
inline size_t strnlen(const char *s, size_t maxlen) {
  const char* end = static_cast<const char *>(memchr(s, '\0', maxlen));
  if (end)
    return end - s;
  return maxlen;
}
#endif

namespace ion {
namespace port {

#if defined(ION_PLATFORM_WINDOWS)
// Ion and friends assume std::strings are UTF-8.  For Windows, we must convert
// to and from UTF-16 in order to use non-ASCII strings in with Windows APIs.
ION_API std::wstring Utf8ToWide(const std::string& utf8);
ION_API std::string WideToUtf8(const std::wstring& wide);
#endif

}  // namespace port
}  // namespace ion

#endif  // ION_PORT_STRING_H_
