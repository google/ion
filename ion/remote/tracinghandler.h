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

#ifndef ION_REMOTE_TRACINGHANDLER_H_
#define ION_REMOTE_TRACINGHANDLER_H_

#include <list>
#include <mutex>  // NOLINT(build/c++11)
#include <ostream>
#include <string>

#include "base/integral_types.h"
#include "ion/gfx/renderer.h"
#include "ion/gfxutils/frame.h"
#include "ion/remote/httpserver.h"

namespace ion {
namespace remote {

// TracingHandler serves files related to OpenGL tracing. It can generate a
// tree of OpenGL calls made during frames. The TracingHandler installs a
// tracing stream in the GraphicsManager while the handler is actively tracing
// and restores the previous stream when it is not active.
//
// /   or /index.html        - Tracing display interface
// /clear                    - Clears the current trace string, returns "clear".
// /trace_next_frame         - Returns a string containing the OpenGL trace,
//                             first appending the trace of the next frame.
class ION_API TracingHandler : public HttpServer::RequestHandler {
 public:
  // The constructor is passed a Frame instance that allows the handler to know
  // when frames begin and end and the Renderer that is issuing the graphics
  // calls.
  TracingHandler(const gfxutils::FramePtr& frame,
                 const gfx::RendererPtr& renderer);

  ~TracingHandler() override;

  const std::string HandleRequest(const std::string& path_in,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override;

 private:
  class TraceRequest;

  // Traces the next frame.  Returns an HTML representation of all frames traced
  // since the last request to clear.
  std::string TraceNextFrame(std::string resources_to_delete,
                             bool block_until_frame_rendered);

  // Frame callbacks.
  void BeginFrame(const gfxutils::Frame& frame);
  void EndFrame(const gfxutils::Frame& frame);

  // Frame passed to constructor.
  gfxutils::FramePtr frame_;
  // Renderer passed to constructor.
  gfx::RendererPtr renderer_;

  // Mutex for |pending_requests_|.
  std::mutex pending_requests_mutex_;
  // List of TraceRequest instances pending for the next frame.
  std::list<TraceRequest*> pending_requests_;

  // List of outstanding TraceRequests being processed for this frame.
  std::list<TraceRequest*> frame_active_requests_;

  // Mutex for |html_string_|.
  std::mutex html_string_mutex_;
  // String containing the HTML to display.
  std::string html_string_;
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_TRACINGHANDLER_H_
