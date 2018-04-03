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

#include "ion/remote/remoteserver.h"

#if !ION_PRODUCTION

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/once.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/remote/calltracehandler.h"
#include "ion/remote/nodegraphhandler.h"
#include "ion/remote/resourcehandler.h"
#include "ion/remote/settinghandler.h"
#include "ion/remote/shaderhandler.h"
#include "ion/remote/tracinghandler.h"

ION_REGISTER_ASSETS(IonRemoteGetUri);
ION_REGISTER_ASSETS(IonRemoteRoot);

#endif  // !ION_PRODUCTION

namespace ion {
namespace remote {

namespace {

#if !ION_PRODUCTION

static const int kRemoteThreads = 8;
static const char kRootPage[] =
    "<!DOCTYPE html>"
    "<html>\n"
    "<head>\n"
    "  <title>Ion Remote</title>\n"
    "  <link rel=\"stylesheet\" href=\"/ion/css/style.css\">\n"
    "    <script type=\"text/javascript\">\n"
    "      window.location = \"/ion/settings/#^\"\n"
    "    </script>\n"
    "</head>\n"
    "<body></body>\n"
    "</html>\n";

class IonRootHandler : public HttpServer::RequestHandler {
 public:
  IonRootHandler() : RequestHandler("/ion") {}
  ~IonRootHandler() override {}

  const std::string HandleRequest(const std::string& path_in,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    const std::string path = path_in.empty() ? "index.html" : path_in;
    if (path == "index.html") {
      return kRootPage;
    } else {
      const std::string& data =
          base::ZipAssetManager::GetFileData("ion/" + path);
      return base::IsInvalidReference(data) ? std::string() : data;
    }
  }
};

// Override / to redirect to /ion so that when clients connect to the root they
// will not get a 404.
class RootHandler : public HttpServer::RequestHandler {
 public:
  RootHandler() : RequestHandler("/") {}
  ~RootHandler() override {}

  const std::string HandleRequest(const std::string& path_in,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    if (path_in.empty() || path_in == "index.html") {
      *content_type = "text/html";
      return kRootPage;
    } else {
      return std::string();
    }
  }
};

static void RegisterAssetsForRemoteServer() {
  IonRemoteGetUri::RegisterAssets();
  IonRemoteRoot::RegisterAssets();
}

#else

static const int kRemoteThreads = 0;

#endif  // !ION_PRODUCTION

}  // anonymous namespace

RemoteServer::RemoteServer(int port)
    : HttpServer(port, kRemoteThreads) {
  Init(port);
}

RemoteServer::RemoteServer(
    const gfx::RendererPtr& renderer,
    const gfxutils::ShaderManagerPtr& shader_manager,
    const gfxutils::FramePtr& frame,
    int port) : HttpServer(port, kRemoteThreads) {
#if !ION_PRODUCTION
  Init(port);
  // Create and register NodeGraphHandler.
  node_graph_handler_.Reset(new NodeGraphHandler);
  node_graph_handler_->SetFrame(frame);
  RegisterHandler(node_graph_handler_);

  // Register all other handles.
  RegisterHandler(
      HttpServer::RequestHandlerPtr(
          new CallTraceHandler()));
  RegisterHandler(
      HttpServer::RequestHandlerPtr(
          new ResourceHandler(renderer)));
  RegisterHandler(
      HttpServer::RequestHandlerPtr(
          new SettingHandler()));
  RegisterHandler(
      HttpServer::RequestHandlerPtr(
          new ShaderHandler(shader_manager, renderer)));
  RegisterHandler(
      HttpServer::RequestHandlerPtr(
          new TracingHandler(frame, renderer)));
#endif
}

void RemoteServer::Init(int port) {
#if !ION_PRODUCTION
  ION_STATIC_ONCE(RegisterAssetsForRemoteServer);

  static const char kHeaderHtml[] =
      "<div class=\"ion_header\">\n"
      "<span><a href=\"/ion/resources/\">OpenGL resources</a></span>\n"
      "<span><a href=\"/ion/settings/#^\">Settings</a></span>\n"
      "<span><a href=\"/ion/shaders/shader_editor\">Shader editor</a></span>\n"
      "<span><a href=\"/ion/nodegraph\">Node graph display</a></span>\n"
      "<span><a href=\"/ion/tracing\">OpenGL tracing</a></span>\n"
      "<span><a href=\"/ion/calltrace\">Run-time profile "
      "diagram</a></span></div>\n";

  SetHeaderHtml(kHeaderHtml);

  // Register the root handler.
  if (!IsRunning() && port) {
    LOG(ERROR) << "*** ION: Unable to start Remote server.";
  } else {
    RegisterHandler(
        HttpServer::RequestHandlerPtr(new(base::kLongTerm) RootHandler));
    RegisterHandler(
        HttpServer::RequestHandlerPtr(new(base::kLongTerm) IonRootHandler));
  }
#endif
}

void RemoteServer::AddNode(const gfx::NodePtr& node) const {
#if !ION_PRODUCTION
  HttpServer::HandlerMap handler_map = GetHandlers();
  auto iter = handler_map.find("/ion/nodegraph");
  if (iter != handler_map.end()) {
    // Safe because we know a priori that this RequestHandler can be
    // downcasted to a NodeGraphHandler.
    static_cast<NodeGraphHandler*>(iter->second.Get())->AddNode(node);
  }
#endif
}

bool RemoteServer::RemoveNode(const gfx::NodePtr& node) const {
#if !ION_PRODUCTION
  HttpServer::HandlerMap handler_map = GetHandlers();
  auto iter = handler_map.find("/ion/nodegraph");
  if (iter != handler_map.end()) {
    // Safe because we know a priori that this RequestHandler can be
    // downcasted to a NodeGraphHandler.
    return static_cast<NodeGraphHandler*>(iter->second.Get())->RemoveNode(node);
  }
#endif

  return false;
}

RemoteServer::~RemoteServer() {}

}  // namespace remote
}  // namespace ion

