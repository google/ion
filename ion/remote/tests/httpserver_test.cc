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

#include "ion/remote/httpserver.h"

#include <sstream>

#include "ion/base/logchecker.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/remote/tests/httpservertest.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

// Resources for tests.
ION_REGISTER_ASSETS(IonTestRemoteRoot);

namespace ion {
namespace remote {

// Save some typing by typedefing the handler pointer.
typedef HttpServer::RequestHandlerPtr RequestHandlerPtr;

namespace {

class HeaderFooterHandler : public HttpServer::RequestHandler {
 public:
  explicit HeaderFooterHandler(const std::string& base_path)
      : RequestHandler(base_path) {}
  ~HeaderFooterHandler() override {}
  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    if (path.empty()) {
      static const char text[] = "<!--HEADER--><!--FOOTER-->";
      *content_type = "text/html";
      return text;
    } else {
      return std::string();
    }
  }
};

class IndexHandler : public HttpServer::RequestHandler {
 public:
  explicit IndexHandler(const std::string& base_path)
      : RequestHandler(base_path) {}
  ~IndexHandler() override {}
  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    if (path.empty() || path == "index.html") {
      *content_type = "text/html";
      return base::ZipAssetManager::GetFileData("index.html");
    } else {
      return std::string();
    }
  }
};

class TextHandler : public HttpServer::RequestHandler {
 public:
  explicit TextHandler(const std::string& base_path)
      : RequestHandler(base_path) {}
  ~TextHandler() override {}
  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    // Since the handler is for only a single file, the relative path to the
    // file is empty.
    if (path.empty()) {
      static const char text[] = "text";
      return text;
    } else {
      return std::string();
    }
  }
};

class PathHandler : public HttpServer::RequestHandler {
 public:
  explicit PathHandler(const std::string& base_path)
      : RequestHandler(base_path) {}
  ~PathHandler() override {}
  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    // .htpasswd is a special case that must be NULL or a valid file.
    if (path.empty())
      return "self";
    else if (base::EndsWith(path, ".htpasswd"))
      return std::string();
    else
      return path;
  }
};

class QueryArgsHandler : public HttpServer::RequestHandler {
 public:
  explicit QueryArgsHandler(const std::string& base_path)
      : RequestHandler(base_path) {}
  ~QueryArgsHandler() override {}
  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    // Construct a query string.
    std::stringstream response_stream;
    response_stream.str("");
    response_stream << "?";
    for (HttpServer::QueryMap::const_iterator it = args.begin();
         it != args.end(); ++it)
      response_stream << "&" << it->first << "=" << it->second;
    return response_stream.str();
  }
};

class EmbedHandler : public HttpServer::RequestHandler {
 public:
  explicit EmbedHandler(const std::string& base_path)
      : RequestHandler(base_path) {}
  ~EmbedHandler() override {}
  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    // Construct a query string.
    if (path == "img_test.html") {
      return "<body>\n <img src='/embed/image.png' >";
    } else if (path == "img_test2.html") {
      return "<body>\n<img src=/does/not/exist.jpg>"
             "<img src=\"/embed/image.jpg\"><\body>";
    } else if (path == "image.png") {
      return "foo";
    } else if (path == "image.jpg") {
      return "bar";
    } else if (path == "link_test.html") {
      return "<head><link rel=\"stylesheet\" href=\"/embed/style.css\"></head>";
    } else if (path == "link_test2.html") {
      return
          "<head><link rel=\"stylesheet\" href=\"/embed/style.css\">"
          "<link rel=\"stylesheet\" href=\"/no/such/style.css\"></head>";
    } else if (path == "style.css") {
      return "body {\n  color: #fff;\n}";
    } else if (path == "script_test.html") {
      return "<body><script src=\"/embed/source.js\"></script></head>";
    } else if (path == "script_test2.html") {
      return
          "<body><script src=\"/embed/source.js\"></script>\n"
          "<script src=\"/no/such/source.js\"></script>"
          "</head>";
    } else if (path == "source.js") {
      return "function inc(arg) {\n  return arg + 1;\n}";
    } else {
      return std::string();
    }
  }
};

}  // anonymous namespace

TEST_F(HttpServerTest, FailedServer) {
  // Check that a server fails to start if we pass it bad startup parameters.
  base::LogChecker log_checker;

  std::unique_ptr<HttpServer> server(new HttpServer(-1, 1));
  EXPECT_FALSE(server->IsRunning());
  EXPECT_TRUE(log_checker.HasMessage("ERROR", "invalid port spec"));

  server = absl::make_unique<HttpServer>(0, 1);
  EXPECT_FALSE(server->IsRunning());
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST_F(HttpServerTest, ServerResponds) {
  // Check that we can connect to the server. Since we do not allow direct file
  // access any file request without any installed handlers will return a 404.
  GetUri("");
  Verify404(__LINE__);

  GetUri("/index.html");
  Verify404(__LINE__);

#if !defined(ION_PLATFORM_ASMJS) && !defined(ION_PLATFORM_NACL)
  response_ = client_.Put(localhost_ + "/index.html", "some data");
  EXPECT_EQ(401, response_.status);
  EXPECT_TRUE(response_.data.empty());
  EXPECT_EQ(2U, response_.headers.size());
  EXPECT_EQ("0", response_.headers["Content-Length"]);
  // The authenticate request will have a random nonce value.
  EXPECT_TRUE(base::StartsWith(response_.headers["WWW-Authenticate"],
                               "Digest"));
#endif
}

TEST_F(HttpServerTest, PauseAndUnpause) {
  // Test that pausing and resuming the server works.
#if !defined(ION_PLATFORM_ASMJS) && !defined(ION_PLATFORM_NACL)
  server_->Pause();
  EXPECT_FALSE(server_->IsRunning());

  server_->Resume();
  EXPECT_TRUE(server_->IsRunning());
#endif
}

TEST_F(HttpServerTest, RequestHandlers) {
  // Register some asset data to serve.
  EXPECT_TRUE(IonTestRemoteRoot::RegisterAssets());

  // This should return a 404 since there are no registered handlers.
  GetUri("");
  Verify404(__LINE__);

  // Install a request handler for the root.
  server_->RegisterHandler(RequestHandlerPtr(new IndexHandler("/")));
  // Install a request handler for a file.
  server_->RegisterHandler(
      RequestHandlerPtr(new TextHandler("/test/path/to/file.txt")));

  // Since the handler handler handles both / and /index.html it should return
  // index.html for both files.
  GetUri("");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("text/html", response_.headers["Content-Type"]);
  EXPECT_FALSE(response_.data.empty());
  EXPECT_EQ(base::ZipAssetManager::GetFileData("index.html"), response_.data);
  EXPECT_EQ(base::ZipAssetManager::GetFileData("index.html"),
            server_->GetUriData(""));

  GetUri("/");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("text/html", response_.headers["Content-Type"]);
  EXPECT_FALSE(response_.data.empty());
  EXPECT_EQ(base::ZipAssetManager::GetFileData("index.html"), response_.data);
  EXPECT_EQ(base::ZipAssetManager::GetFileData("index.html"),
            server_->GetUriData("/"));

  GetUri("/index.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("text/html", response_.headers["Content-Type"]);
  EXPECT_FALSE(response_.data.empty());
  EXPECT_EQ(base::ZipAssetManager::GetFileData("index.html"), response_.data);
  EXPECT_EQ(base::ZipAssetManager::GetFileData("index.html"),
            server_->GetUriData("index.html"));

#if !defined(ION_PLATFORM_ASMJS) && !defined(ION_PLATFORM_NACL)
  const size_t instance_length = response_.data.size();
  // Get part of a file. The length should be 9 bytes since the range is
  // inclusive.
  response_ = client_.GetRange(localhost_ + "/index.html", 5, 80);
  std::string index_range =
      base::ZipAssetManager::GetFileData("index.html").substr(5, 76);
  EXPECT_EQ(206, response_.status);
  EXPECT_FALSE(response_.data.empty());
  EXPECT_EQ("text/html", response_.headers["Content-Type"]);
  std::stringstream content_range;
  content_range << "bytes 5-80/" << instance_length;
  EXPECT_EQ(content_range.str(), response_.headers["Content-Range"]);
  EXPECT_EQ(index_range, response_.data);
#endif

  GetUri("/index.php");
  EXPECT_TRUE(server_->GetUriData("index.php").empty());
  Verify404(__LINE__);

  GetUri("/test/path/to/file.txt");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("text/plain", response_.headers["Content-Type"]);
  EXPECT_FALSE(response_.data.empty());
  EXPECT_EQ("text", response_.data);

  // Unregister handler for "file.txt". Requests which were status 200 become
  // status 404 because the handler is gone.
  server_->UnregisterHandler("/test/path/to/file.txt");
  GetUri("/test/path/to/file.txt");
  EXPECT_EQ(404, response_.status);

  // Check that a few variations still give a 404.
  GetUri("/test/path/to");
  Verify404(__LINE__);
  EXPECT_TRUE(server_->GetUriData("/test/path/to").empty());

  GetUri("/test/path/to/");
  Verify404(__LINE__);
  EXPECT_TRUE(server_->GetUriData("/test/path/to/").empty());

  GetUri("/test/path//");
  Verify404(__LINE__);
  EXPECT_TRUE(server_->GetUriData("/test/path//").empty());

  GetUri("//test/path");
  Verify404(__LINE__);
  EXPECT_TRUE(server_->GetUriData("//test/path").empty());

  GetUri("/test/path.ext");
  Verify404(__LINE__);
  EXPECT_TRUE(server_->GetUriData("/test/path.ext").empty());

  // Install a request handler a special path and make sure paths are stripped
  // properly.
  server_->RegisterHandler(RequestHandlerPtr(new PathHandler("/path/")));

  GetUri("/path/");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("self", response_.data);
  EXPECT_EQ("self", server_->GetUriData("path"));
  EXPECT_EQ("self", server_->GetUriData("/path"));

  GetUri("/path/to/file.txt");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("to/file.txt", response_.data);
  EXPECT_EQ("to/file.txt", server_->GetUriData("/path/to/file.txt"));

  GetUri("/path/file.txt");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("file.txt", response_.data);
  EXPECT_EQ("file.txt", server_->GetUriData("path/file.txt"));

  GetUri("/path/to/a/dir");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("to/a/dir", response_.data);
  EXPECT_EQ("to/a/dir", server_->GetUriData("path/to/a/dir"));
}

TEST_F(HttpServerTest, QueryArgs) {
  server_->RegisterHandler(
      RequestHandlerPtr(new QueryArgsHandler("/query.html")));

  GetUri("/query.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("?", response_.data);
  EXPECT_EQ("?", server_->GetUriData("/query.html"));

  // Args get sorted in alpha order since they are in a std::map.
  GetUri("/query.html?arg1=1&2nd=3");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("?&2nd=3&arg1=1", response_.data);
  EXPECT_EQ("?&2nd=3&arg1=1", server_->GetUriData("/query.html?arg1=1&2nd=3"));

  GetUri("/query.html?var&var2=value");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("?&var=&var2=value", response_.data);
  EXPECT_EQ("?&var=&var2=value",
            server_->GetUriData("/query.html?var&var2=value"));
}

TEST_F(HttpServerTest, HeaderAndFooter) {
  server_->RegisterHandler(
      RequestHandlerPtr(new HeaderFooterHandler("/hf/")));
  EXPECT_EQ(std::string(), server_->GetHeaderHtml());
  EXPECT_EQ(std::string(), server_->GetFooterHtml());

  std::string header("header");
  std::string footer("footer");
  server_->SetHeaderHtml(header);
  server_->SetFooterHtml(footer);
  EXPECT_EQ(header, server_->GetHeaderHtml());
  EXPECT_EQ(footer, server_->GetFooterHtml());

  GetUri("/hf");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("headerfooter", response_.data);

  header = "another header";
  footer = "another footer";
  server_->SetHeaderHtml(header);
  server_->SetFooterHtml(footer);
  EXPECT_EQ(header, server_->GetHeaderHtml());
  EXPECT_EQ(footer, server_->GetFooterHtml());

  GetUri("/hf");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("another headeranother footer", response_.data);
}

TEST_F(HttpServerTest, EmbeddingLocalFiles) {
  server_->RegisterHandler(
      RequestHandlerPtr(new EmbedHandler("/embed/")));
  server_->SetEmbedLocalSourcedFiles(true);
  GetUri("/embed/img_test.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("<body>\n <img src='data:image/png;base64,Zm9v'>", response_.data);

  GetUri("/embed/img_test2.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("<body>\n<img src=/does/not/exist.jpg>"
            "<img src='data:image/jpeg;base64,YmFy'><\body>", response_.data);

  GetUri("/embed/link_test.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("<head><style>\nbody {\n  color: #fff;\n}\n</style>\n</head>",
            response_.data);

  GetUri("/embed/link_test2.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("<head><style>\nbody {\n  color: #fff;\n}\n</style>\n"
            "<link rel=\"stylesheet\" href=\"/no/such/style.css\"></head>",
            response_.data);

  GetUri("/embed/script_test.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("<body><script>\nfunction inc(arg) {\n  return arg + 1;\n}\n"
            "</script></head>", response_.data);

  GetUri("/embed/script_test2.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("<body><script>\nfunction inc(arg) {\n  return arg + 1;\n}\n"
            "</script>\n<script src=\"/no/such/source.js\"></script></head>",
            response_.data);
}

}  // namespace remote
}  // namespace ion

#endif
