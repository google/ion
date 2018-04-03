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

#include "ion/remote/remoteserver.h"

#include <memory>

#include "ion/base/logchecker.h"
#include "ion/base/zipassetmanager.h"
#include "ion/gfx/renderer.h"
#include "ion/gfx/tests/fakeglcontext.h"
#include "ion/gfx/tests/fakegraphicsmanager.h"
#include "ion/gfxutils/frame.h"
#include "ion/gfxutils/shadermanager.h"
#include "ion/portgfx/glcontext.h"
#include "ion/remote/tests/getunusedport.h"
#include "ion/remote/tests/httpservertest.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace remote {

// Save some typing by typedefing the handler pointer.
typedef HttpServer::RequestHandlerPtr RequestHandlerPtr;

TEST_F(RemoteServerTest, FailedServer) {
  // Check that a server fails to start if we pass it bad startup parameters.
  base::LogChecker log_checker;

  std::unique_ptr<RemoteServer> server(new RemoteServer(-1));
  EXPECT_FALSE(server->IsRunning());
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "Unable to start Remote server"));
}

TEST_F(RemoteServerTest, ServeRoot) {
  GetUri("");
  EXPECT_EQ(200, response_.status);
  EXPECT_NE(std::string::npos,
            response_.data.find("window.location = \"/ion/settings"));

  GetUri("/");
  EXPECT_EQ(200, response_.status);
  EXPECT_NE(std::string::npos,
            response_.data.find("window.location = \"/ion/settings"));

  GetUri("/index.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_NE(std::string::npos,
            response_.data.find("window.location = \"/ion/settings"));

  GetUri("/a");
  Verify404(__LINE__);

  GetUri("/ion/does/not/exist");
  Verify404(__LINE__);

  GetUri("/ion");
  EXPECT_EQ(200, response_.status);
  EXPECT_NE(std::string::npos,
            response_.data.find("window.location = \"/ion/settings"));

  GetUri("/ion/css/style.css");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(base::ZipAssetManager::GetFileData("ion/css/style.css"),
            response_.data);

  GetUri("/ion/index.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_NE(std::string::npos,
            response_.data.find("window.location = \"/ion/settings"));
}

#if !(defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL))
TEST_F(RemoteServerTest, SucceedServer) {
  portgfx::GlContextPtr gl_context =
      gfx::testing::FakeGlContext::Create(64, 64);
  portgfx::GlContext::MakeCurrent(gl_context);
  gfx::GraphicsManagerPtr graphics_manager(
      new gfx::testing::FakeGraphicsManager());
  gfx::RendererPtr renderer(new gfx::Renderer(graphics_manager));
  gfxutils::ShaderManagerPtr shader_manager(new gfxutils::ShaderManager());
  gfxutils::FramePtr frame(new gfxutils::Frame());

  std::unique_ptr<RemoteServer> server(new RemoteServer(
      renderer, shader_manager, frame, testing::GetUnusedPort(500)));
  EXPECT_TRUE(server->IsRunning());

  const HttpServer::HandlerMap handler_map = server->GetHandlers();
  EXPECT_NE(handler_map.end(), handler_map.find("/ion/nodegraph"));
  EXPECT_NE(handler_map.end(), handler_map.find("/ion/calltrace"));
  EXPECT_NE(handler_map.end(), handler_map.find("/ion/resources"));
  EXPECT_NE(handler_map.end(), handler_map.find("/ion/settings"));
  EXPECT_NE(handler_map.end(), handler_map.find("/ion/shaders"));
  EXPECT_NE(handler_map.end(), handler_map.find("/ion/tracing"));
}

TEST_F(RemoteServerTest, AddNode) {
  portgfx::GlContextPtr gl_context =
      gfx::testing::FakeGlContext::Create(64, 64);
  portgfx::GlContext::MakeCurrent(gl_context);
  gfx::GraphicsManagerPtr graphics_manager(
      new gfx::testing::FakeGraphicsManager());
  gfx::RendererPtr renderer(new gfx::Renderer(graphics_manager));
  gfxutils::ShaderManagerPtr shader_manager(new gfxutils::ShaderManager());
  gfxutils::FramePtr frame(new gfxutils::Frame());

  std::unique_ptr<RemoteServer> server(new RemoteServer(
      renderer, shader_manager, frame, testing::GetUnusedPort(500)));

  HttpServer::HandlerMap handlers = server->GetHandlers();
  auto iter = handlers.find("/ion/nodegraph");
  EXPECT_NE(handlers.end(), iter);
  NodeGraphHandlerPtr handler(
      static_cast<NodeGraphHandler*>(iter->second.Get()));
  const gfx::NodePtr node1(new gfx::Node);
  server->AddNode(node1);
  EXPECT_TRUE(handler->IsNodeTracked(node1));
  EXPECT_EQ(1, static_cast<int>(handler->GetTrackedNodeCount()));
  const gfx::NodePtr node2(new gfx::Node);
  server->AddNode(node2);
  EXPECT_TRUE(handler->IsNodeTracked(node2));
  EXPECT_EQ(2, static_cast<int>(handler->GetTrackedNodeCount()));
}

TEST_F(RemoteServerTest, RemoveNode) {
  portgfx::GlContextPtr gl_context =
      gfx::testing::FakeGlContext::Create(64, 64);
  portgfx::GlContext::MakeCurrent(gl_context);
  gfx::GraphicsManagerPtr graphics_manager(
      new gfx::testing::FakeGraphicsManager());
  gfx::RendererPtr renderer(new gfx::Renderer(graphics_manager));
  gfxutils::ShaderManagerPtr shader_manager(new gfxutils::ShaderManager());
  gfxutils::FramePtr frame(new gfxutils::Frame());

  std::unique_ptr<RemoteServer> server(new RemoteServer(
      renderer, shader_manager, frame, testing::GetUnusedPort(500)));

  HttpServer::HandlerMap handlers = server->GetHandlers();
  auto iter = handlers.find("/ion/nodegraph");
  EXPECT_NE(handlers.end(), iter);
  NodeGraphHandlerPtr handler(
      static_cast<NodeGraphHandler*>(iter->second.Get()));
  const gfx::NodePtr node1(new gfx::Node);
  EXPECT_EQ(1, node1->GetRefCount());
  server->AddNode(node1);
  EXPECT_EQ(2, node1->GetRefCount());
  EXPECT_TRUE(handler->IsNodeTracked(node1));
  EXPECT_EQ(1, static_cast<int>(handler->GetTrackedNodeCount()));
  const gfx::NodePtr node2(new gfx::Node);
  EXPECT_EQ(1, node2->GetRefCount());
  server->AddNode(node2);
  EXPECT_EQ(2, node2->GetRefCount());
  EXPECT_TRUE(handler->IsNodeTracked(node2));
  EXPECT_EQ(2, static_cast<int>(handler->GetTrackedNodeCount()));
  bool removed = server->RemoveNode(node1);
  EXPECT_TRUE(removed);
  EXPECT_FALSE(handler->IsNodeTracked(node1));
  EXPECT_EQ(1, node1->GetRefCount());
  EXPECT_EQ(1, static_cast<int>(handler->GetTrackedNodeCount()));
  removed = server->RemoveNode(node2);
  EXPECT_TRUE(removed);
  EXPECT_FALSE(handler->IsNodeTracked(node2));
  EXPECT_EQ(1, node2->GetRefCount());
  EXPECT_EQ(0, static_cast<int>(handler->GetTrackedNodeCount()));

  // Remove a node that isn't in the handler.
  const gfx::NodePtr node3(new gfx::Node);
  removed = server->RemoveNode(node3);
  EXPECT_FALSE(removed);
}
#endif

}  // namespace remote
}  // namespace ion

#endif
