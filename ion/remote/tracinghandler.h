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

#ifndef ION_REMOTE_TRACINGHANDLER_H_
#define ION_REMOTE_TRACINGHANDLER_H_

#include <functional>
#include <sstream>
#include <string>

#include "base/integral_types.h"
#include "ion/gfx/renderer.h"
#include "ion/gfxutils/frame.h"
#include "ion/port/semaphore.h"
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

  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override;

 private:
  // This enum indicates what state the handler is in.
  enum State {
    kInactive,              // Not actively tracing.
    kWaitingForBeginFrame,  // Waiting for BeginFrame() to be called.
    kWaitingForEndFrame,    // Waiting for EndFrame() to be called.
  };

  // Traces the next frame.
  void TraceNextFrame(bool block_until_frame_rendered);

  // Frame callbacks.
  void BeginFrame(const gfxutils::Frame& frame);
  void EndFrame(const gfxutils::Frame& frame);

  // Returns the stream that the TracingHandler installs in the GraphicsManager
  // to capture the OpenGL trace. This is used for testing.
  std::ostream* GetTracingStream() { return &tracing_stream_; }

  // Frame passed to constructor.
  gfxutils::FramePtr frame_;
  // Renderer passed to constructor.
  gfx::RendererPtr renderer_;
  // Saves the previous ostream from the GraphicsManager so it can be restored.
  std::ostream* prev_stream_;
  // Stream used to get tracing data in GraphicsManager.
  std::ostringstream tracing_stream_;
  // String containing the HTML to display.
  std::string html_string_;
  // For blocking until end of frame.
  port::Semaphore semaphore_;
  // Current state of tracing.
  State state_;
  // Stores the frame counter when the last trace was added.
  uint64 frame_counter_;
  // String containing the names of renderer resources to delete before the
  // next frame (may be empty).
  std::string resources_to_delete_;

  // Allow the tests to access GetTracingStream().
  friend class TracingHandlerTest;
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_TRACINGHANDLER_H_
