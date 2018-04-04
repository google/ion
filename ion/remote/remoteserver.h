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

#ifndef ION_REMOTE_REMOTESERVER_H_
#define ION_REMOTE_REMOTESERVER_H_

#include "ion/remote/httpserver.h"

#include "ion/gfx/renderer.h"
#include "ion/gfxutils/frame.h"
#include "ion/gfxutils/shadermanager.h"

#if !ION_PRODUCTION
#include "ion/remote/nodegraphhandler.h"
#include "ion/remote/remoteserver.h"
#endif

namespace ion {
namespace remote {

// A RemoteServer starts an HttpServer with a predefined handler for the /ion
// subdirectory.
class ION_API RemoteServer : public HttpServer {
 public:
  // Starts a RemoteServer on the passed port.
  explicit RemoteServer(int port);
  // Starts a RemoteServer and takes shared ownership of some key
  // Ion objects of the application. It instantiates all Ion handlers.
  RemoteServer(const gfx::RendererPtr& renderer,
               const gfxutils::ShaderManagerPtr& shader_manager,
               const gfxutils::FramePtr& frame,
               int port);
  ~RemoteServer() override;

  // Adds a Node to the NodeGraphHandler which allows Remote to inspect it in
  // the web interface. Note that this only happens if there _is_ a
  // NodeGraphHandler added to Remote.
  void AddNode(const gfx::NodePtr& node) const;

  // Removes a Node from the NodeGraphHandler. Note that this only happens if
  // there _is_ a NodeGraphHandler added to Remote.
  // If the Node was not added, this does nothing but return false.
  bool RemoveNode(const gfx::NodePtr& node) const;

 private:
  // Init the remote server, called by both constructors.
  void Init(int port);
#if !ION_PRODUCTION
  NodeGraphHandlerPtr node_graph_handler_;
#endif
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_REMOTESERVER_H_
