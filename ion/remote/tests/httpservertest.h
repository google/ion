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

#ifndef ION_REMOTE_TESTS_HTTPSERVERTEST_H_
#define ION_REMOTE_TESTS_HTTPSERVERTEST_H_

#include <memory>
#include <utility>

#include "ion/base/logging.h"
#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/remote/httpclient.h"
#include "ion/remote/remoteserver.h"
#include "ion/remote/tests/getunusedport.h"
#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"
#include "third_party/mongoose/mongoose.h"

namespace ion {
namespace remote {

// Test framework for tests that need an HttpServer.
class HttpServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL)
    const int port = 0;
#else
    // Start a mongoose server.
    const int port = testing::GetUnusedPort(500);
#endif
    const std::string port_string = base::ValueToString(port);
    localhost_ = "localhost:" + port_string;
    server_ = absl::make_unique<HttpServer>(port, 4);
#if !defined(ION_PLATFORM_ASMJS) && !defined(ION_PLATFORM_NACL)
    EXPECT_TRUE(server_->IsRunning());
#endif
  }

  // Returns a URI from the server.
  void GetUri(const std::string& uri) {
#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL)
    response_ = HttpClient::Response();
    const std::string decoded_uri = base::UrlDecodeString(uri);
    response_.url = HttpClient::Url(decoded_uri);
    response_.data = server_->GetUriData(decoded_uri);
    if (response_.data.empty()) {
      response_.status = 404;
      response_.headers["Connection"] = "close";
      response_.data =
          "Error 404: Not Found\nThe requested file was not found.";
    } else {
      response_.status = 200;
      if (response_.data.find("htm") != std::string::npos)
        response_.headers["Content-Type"] = "text/html";
      else
        response_.headers["Content-Type"] = "text/plain";
    }
    response_.headers["Content-Length"] =
        base::ValueToString(response_.data.length());
#else
    response_ = client_.Get(localhost_ + uri);
#endif
  }

  void TearDown() override {
    // Shutdown the server.
    server_.reset(nullptr);
  }

  // Logs headers from a response to the tracing stream.
  void LogHeaders(const HttpClient::Response& response) {
    for (std::map<std::string, std::string>::const_iterator it =
         response.headers.begin(); it != response.headers.end(); ++it) {
      LOG(INFO) << "headers[" << it->first << "] = " << it->second;
    }
  }

  void Verify404(int line) {
    SCOPED_TRACE(::testing::Message() << "Verifying 404 response from line "
                                      << line);
    EXPECT_EQ(404, response_.status);
    EXPECT_EQ(2U, response_.headers.size());
    EXPECT_EQ("close", response_.headers["Connection"]);
    EXPECT_EQ(54U, response_.data.length());
    EXPECT_EQ("54", response_.headers["Content-Length"]);
    EXPECT_EQ("Error 404: Not Found\nThe requested file was not found.",
              response_.data);
  }

  std::unique_ptr<HttpServer> server_;
  HttpClient client_;
  HttpClient::Response response_;
  std::string localhost_;
};

class HttpServerTestRequestHandler : public HttpServer::RequestHandler {
 public:
  explicit HttpServerTestRequestHandler(
      const HttpServer::RequestHandlerPtr& inner)
      : HttpServer::RequestHandler(inner->GetBasePath()), inner_(inner) {}
  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    if (pre_handler_) {
      pre_handler_();
    }
    std::string result = inner_->HandleRequest(path, args, content_type);
    if (post_handler_) {
      post_handler_();
    }
    return result;
  }

  // By default, RequestHandlers don't support websocket connections.
  const HttpServer::WebsocketPtr ConnectWebsocket(
      const std::string& path, const HttpServer::QueryMap& args) override {
    return inner_->ConnectWebsocket(path, args);
  }

  void SetPreHandler(std::function<void()> handler) {
    pre_handler_ = std::move(handler);
  }
  void SetPostHandler(std::function<void()> handler) {
    post_handler_ = std::move(handler);
  }

 private:
  HttpServer::RequestHandlerPtr inner_;
  std::function<void()> pre_handler_;
  std::function<void()> post_handler_;
};

// Similar to the above, but sets up a RemoteServer.
class RemoteServerTest : public HttpServerTest {
 protected:
  void SetUp() override {
#if defined(ION_PLATFORM_ASMJS) || defined(ION_PLATFORM_NACL)
    const int port = 0;
#else
    // Start a mongoose server.
    const int port = testing::GetUnusedPort(500);
#endif
    const std::string port_string = base::ValueToString(port);
    localhost_ = "localhost:" + port_string;
    server_ = absl::make_unique<RemoteServer>(port);
#if !defined(ION_PLATFORM_ASMJS) && !defined(ION_PLATFORM_NACL)
    EXPECT_TRUE(server_->IsRunning());
#endif
  }
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_TESTS_HTTPSERVERTEST_H_
