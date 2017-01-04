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

#if !ION_PRODUCTION

#include "ion/remote/calltracehandler.h"

#include "ion/base/invalid.h"
#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/profile/profiling.h"

ION_REGISTER_ASSETS(IonRemoteCallTraceRoot);

namespace ion {
namespace remote {

CallTraceHandler::CallTraceHandler()
  : ion::remote::HttpServer::RequestHandler("/ion/calltrace") {
  IonRemoteCallTraceRoot::RegisterAssetsOnce();
}

CallTraceHandler::~CallTraceHandler() {}

const std::string CallTraceHandler::HandleRequest(
    const std::string& path_in, const ion::remote::HttpServer::QueryMap& args,
    std::string* content_type) {
  const std::string path = path_in.empty() ? "index.html" : path_in;

  if (path == "call.wtf-trace") {
    *content_type = "application/x-extension-wtf-trace";
    ion::profile::CallTraceManager* tm = ion::profile::GetCallTraceManager();
    return tm->SnapshotCallTraces();
  } else {
    const std::string& data =
        base::ZipAssetManager::GetFileData("ion/calltrace/" + path);
    if (!base::IsInvalidReference(data)) {
      // Ensure the content type is set if the editor HTML is requested.
      if (base::EndsWith(path, "html"))
        *content_type = "text/html";
      return data;
    }
  }
  return std::string();
}

}  // namespace remote
}  // namespace ion

#endif
