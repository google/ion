/**
Copyright 2016 Google Inc. All Rights Reserved.

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

#include "ion/demos/demobase.h"

#if !ION_PRODUCTION
#include "ion/remote/remoteserver.h"

std::unique_ptr<ion::remote::RemoteServer> DemoBase::remote_;
#endif


#if defined(ION_PLATFORM_ASMJS)
extern "C" {
char* IonRemoteGet(const char* page) {
#if !ION_PRODUCTION
  const std::string content = DemoBase::GetRemoteServer() ?
      DemoBase::GetRemoteServer()->GetUriData(page) : "";
  // Emscripten should clean up the memory.
  char* content_cstr = reinterpret_cast<char*>(malloc(content.length() + 1));
  memcpy(content_cstr, content.c_str(), content.length());
  content_cstr[content.length()] = 0;
  return content_cstr;
#else
  return "";
#endif
}
}
#endif

DemoBase::~DemoBase() {
#if !ION_PRODUCTION
  remote_.reset(NULL);
#endif
}
