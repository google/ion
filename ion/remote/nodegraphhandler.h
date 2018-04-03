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

#ifndef ION_REMOTE_NODEGRAPHHANDLER_H_
#define ION_REMOTE_NODEGRAPHHANDLER_H_

#include <string>
#include <vector>

#include "ion/gfx/node.h"
#include "ion/gfxutils/frame.h"
#include "ion/gfxutils/printer.h"
#include "ion/remote/httpserver.h"

namespace ion {
namespace remote {

// NodeGraphHandler serves files to display Ion node graphs as text or HTML
// using the gfxutils::Printer class.
//
// /   or /index.html  - Display interface
// /update             - Updates graphs for all currently-tracked nodes.
class ION_API NodeGraphHandler : public HttpServer::RequestHandler {
 public:
  NodeGraphHandler();
  ~NodeGraphHandler() override;

  // Sets/returns the Frame object used to access the current frame. If the
  // Frame is not NULL, the header for the output will contain the frame
  // counter.
  void SetFrame(const gfxutils::FramePtr& frame) { frame_ = frame; }
  const gfxutils::FramePtr& GetFrame() const { return frame_; }

  // Adds a Node to track if it is not NULL or already tracked.
  void AddNode(const gfx::NodePtr& node);

  // Removes a Node from being tracked. If the Node was not added, this does
  // nothing but return false.
  bool RemoveNode(const gfx::NodePtr& node);

  // Returns true if the given Node is being tracked.
  bool IsNodeTracked(const gfx::NodePtr& node) const;

  // Returns the number of nodes being tracked. (Useful for testing.)
  size_t GetTrackedNodeCount() const { return nodes_.size(); }

  const std::string HandleRequest(const std::string& path_in,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override;

 private:
  // Sets up a gfxutils::Printer from options in the QueryMap.
  void SetUpPrinter(const HttpServer::QueryMap& args,
                    gfxutils::Printer* printer);

  // Uses a gfxutils::Printer to generate the response data string.
  const std::string GetPrintString(gfxutils::Printer* printer);

  // Nodes to print.
  std::vector<gfx::NodePtr> nodes_;
  // Optional Frame used to access frame counter.
  gfxutils::FramePtr frame_;
};
using NodeGraphHandlerPtr = base::SharedPtr<NodeGraphHandler>;

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_NODEGRAPHHANDLER_H_
