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

#ifndef ION_REMOTE_RESOURCEHANDLER_H_
#define ION_REMOTE_RESOURCEHANDLER_H_

#include <string>

#include "ion/gfx/renderer.h"
#include "ion/remote/httpserver.h"

namespace ion {
namespace remote {

// ResourceHandler serves files related to OpenGL resources.
//
// /   or /index.html    - Resource inspector interface
// /resources_by_type?types=t1,t2...
//                       - Gets a JSON struct representing all of the GL
//                             resources of the queried types
// /texture_data&id=#    - Gets a PNG image of the texture with the passed
//                             OpenGL texture ID
class ION_API ResourceHandler : public HttpServer::RequestHandler {
 public:
  explicit ResourceHandler(const gfx::RendererPtr& renderer);
  ~ResourceHandler() override;

  const std::string HandleRequest(const std::string& path_in,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override;

 private:
  gfx::RendererPtr renderer_;
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_RESOURCEHANDLER_H_
