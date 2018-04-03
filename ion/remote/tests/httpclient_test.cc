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

#include "ion/remote/httpclient.h"

#include <string.h>

#include "ion/base/logchecker.h"
#include "ion/base/logging.h"
#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/remote/tests/getunusedport.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"
#include "third_party/mongoose/mongoose.h"

namespace ion {
namespace remote {
namespace testing {

namespace {

// Contents of index.html.
static const char* kIndexHtml = "<html><body>Hello world!</body></html>\n";

static const char* OpenFileCallback(
    const mg_connection*, const char* path, size_t* data_len) {
  *data_len = 0;
  if (!strcmp("./index.html", path)) {
    *data_len = strlen(kIndexHtml);
    return kIndexHtml;
  } else {
    return nullptr;
  }
}

static int RequestCallback(mg_connection* conn) {
  // Typical response headers.
  static const char* kResponseOk = "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/plain\r\n"
                                   "Connection: close\r\n\r\n";

  const mg_request_info* request_info = mg_get_request_info(conn);
  if (!strcmp(request_info->uri, "/")) {
    // Override handling of "/".
    mg_printf(conn, "%s", kResponseOk);
    return 1;
  } else {
    // Let mongoose handle the request.
    return 0;
  }
}

}  // anonymous namespace

class HttpClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Start a mongoose server.
    const int port = GetUnusedPort(500);
    const std::string port_string = base::ValueToString(port);
    localhost_ = "localhost:" + port_string;
    const char *options[] = {"listening_ports", port_string.c_str(), nullptr};
    mg_callbacks callbacks;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.begin_request = RequestCallback;
    callbacks.open_file = OpenFileCallback;
    context_ = mg_start(&callbacks, nullptr, options);
  }

  void TearDown() override {
    // Shutdown the server.
    mg_stop(context_);
  }

  void Verify404(int line) {
    SCOPED_TRACE(::testing::Message() << "Verifying 404 response from line "
                                      << line);
    EXPECT_EQ(404, response_.status);
    EXPECT_EQ(2U, response_.headers.size());
    EXPECT_EQ("close", response_.headers["Connection"]);
    EXPECT_EQ(35U, response_.data.length());
    EXPECT_EQ("35", response_.headers["Content-Length"]);
    EXPECT_EQ("Error 404: Not Found\nFile not found", response_.data);
  }

  void VerifyEmptyResponse(int line) {
    SCOPED_TRACE(::testing::Message() << "Verifying 404 response from line "
                                      << line);
    EXPECT_EQ(200, response_.status);
    EXPECT_TRUE(response_.data.empty());
    EXPECT_EQ(2U, response_.headers.size());
    EXPECT_EQ("text/plain", response_.headers["Content-Type"]);
    EXPECT_EQ("close", response_.headers["Connection"]);
  }

  void VerifyIndexHtmlResponse(int line) {
    SCOPED_TRACE(::testing::Message() << "Verifying index response from line "
                                      << line);
    EXPECT_EQ(200, response_.status);
    EXPECT_FALSE(response_.data.empty());
    EXPECT_EQ(7U, response_.headers.size());
    EXPECT_EQ("text/html", response_.headers["Content-Type"]);
    EXPECT_EQ("close", response_.headers["Connection"]);
    EXPECT_EQ(39U, response_.data.length());
    EXPECT_EQ("39", response_.headers["Content-Length"]);
    EXPECT_EQ(kIndexHtml, response_.data);
  }

  mg_context* context_;
  HttpClient client_;
  HttpClient::Response response_;
  std::string localhost_;
};

TEST_F(HttpClientTest, Url) {
  // Check that HttpClient::Uri can correctly parse typical urls.
  HttpClient::Url url;
  EXPECT_FALSE(url.IsValid());

  url.Set("");
  EXPECT_FALSE(url.IsValid());
  EXPECT_EQ(80, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("", url.hostname);
  EXPECT_EQ("", url.path);
  EXPECT_EQ(0U, url.args.size());

  url.Set("localhost");
  EXPECT_TRUE(url.IsValid());
  EXPECT_EQ(80, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("localhost", url.hostname);
  EXPECT_EQ("/", url.path);
  EXPECT_EQ(0U, url.args.size());

  url.Set("localhost:0");
  EXPECT_FALSE(url.IsValid());
  EXPECT_EQ(0, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("localhost", url.hostname);
  EXPECT_EQ("/", url.path);
  EXPECT_EQ(0U, url.args.size());

  url.Set("localhost:-1");
  EXPECT_FALSE(url.IsValid());
  EXPECT_EQ(-1, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("localhost", url.hostname);
  EXPECT_EQ("/", url.path);
  EXPECT_EQ(0U, url.args.size());

  url.Set("google.com/foo");
  EXPECT_TRUE(url.IsValid());
  EXPECT_EQ(80, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("google.com", url.hostname);
  EXPECT_EQ("/foo", url.path);
  EXPECT_EQ(0U, url.args.size());

  url.Set("https://slashdot.org/foo");
  EXPECT_TRUE(url.IsValid());
  EXPECT_EQ(443, url.port);
  EXPECT_TRUE(url.is_https);
  EXPECT_EQ("slashdot.org", url.hostname);
  EXPECT_EQ("/foo", url.path);
  EXPECT_EQ(0U, url.args.size());

  url.Set("localhost:8080");
  EXPECT_TRUE(url.IsValid());
  EXPECT_EQ(8080, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("localhost", url.hostname);
  EXPECT_EQ("/", url.path);
  EXPECT_EQ(0U, url.args.size());

  url.Set("google.com/search/search2?&foo=1");
  EXPECT_TRUE(url.IsValid());
  EXPECT_EQ(80, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("google.com", url.hostname);
  EXPECT_EQ("/search/search2", url.path);
  EXPECT_EQ(1U, url.args.size());
  EXPECT_EQ("1", url.args["foo"]);

  url.Set("google.com/search/search2?&foo=");
  EXPECT_TRUE(url.IsValid());
  EXPECT_EQ(80, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("google.com", url.hostname);
  EXPECT_EQ("/search/search2", url.path);
  EXPECT_EQ(1U, url.args.size());
  EXPECT_EQ("", url.args["foo"]);

  url.Set("localhost.localdomain:1024?foo=1:2:3");
  EXPECT_TRUE(url.IsValid());
  EXPECT_EQ(1024, url.port);
  EXPECT_FALSE(url.is_https);
  EXPECT_EQ("localhost.localdomain", url.hostname);
  EXPECT_EQ("/", url.path);
  EXPECT_EQ(1U, url.args.size());
  EXPECT_EQ("1:2:3", url.args["foo"]);

  url.Set("https://localhost:1234/foo.html?q=1&q2=foo");
  EXPECT_TRUE(url.IsValid());
  EXPECT_EQ(1234, url.port);
  EXPECT_TRUE(url.is_https);
  EXPECT_EQ("localhost", url.hostname);
  EXPECT_EQ("/foo.html", url.path);
  EXPECT_EQ(2U, url.args.size());
  EXPECT_EQ("1", url.args["q"]);
  EXPECT_EQ("foo", url.args["q2"]);

  {
    // Test protocols other than http and https produce an error message.
    base::LogChecker log_checker;
    url.Set("ftp://hostname:21");
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Unknown protocol 'ftp'"));
    EXPECT_TRUE(url.IsValid());
    EXPECT_EQ(21, url.port);
    EXPECT_FALSE(url.is_https);
    EXPECT_EQ("hostname", url.hostname);
    EXPECT_EQ("/", url.path);
    EXPECT_EQ(0U, url.args.size());

    url.Set("rtp://streaming.com/stream_service");
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Unknown protocol 'rtp'"));
    EXPECT_TRUE(url.IsValid());
    EXPECT_EQ(80, url.port);
    EXPECT_FALSE(url.is_https);
    EXPECT_EQ("streaming.com", url.hostname);
    EXPECT_EQ("/stream_service", url.path);
    EXPECT_EQ(0U, url.args.size());

    // Test that "file" is not understood and produces an invalid url.
    url.Set("file:///tmp/file.txt");
    EXPECT_TRUE(log_checker.HasMessage("ERROR", "Unknown protocol 'file'"));
    EXPECT_FALSE(url.IsValid());
    EXPECT_EQ(80, url.port);
    EXPECT_FALSE(url.is_https);
    EXPECT_EQ("", url.hostname);
    EXPECT_EQ("/tmp/file.txt", url.path);
    EXPECT_EQ(0U, url.args.size());
  }
}

TEST_F(HttpClientTest, Get) {
  response_ = client_.Get(localhost_);
  VerifyEmptyResponse(__LINE__);

  // Get a non-existent page.
  response_ = client_.Get(localhost_ + "/does/not/exist");
  Verify404(__LINE__);

  // Get a fake file.
  response_ = client_.Get(localhost_ + "/index.html");
  VerifyIndexHtmlResponse(__LINE__);

  // 
#if !defined(ION_PLATFORM_ANDROID)
  // Get part of a file.
  response_ = client_.GetRange(localhost_ + "/index.html", 2, 10);
  // The length should be 9 bytes since the range is inclusive.
  std::string index_range = std::string(kIndexHtml).substr(2, 9);
  EXPECT_EQ(206, response_.status);
  EXPECT_FALSE(response_.data.empty());
  EXPECT_EQ(8U, response_.headers.size());
  EXPECT_EQ("text/html", response_.headers["Content-Type"]);
  EXPECT_EQ("bytes 2-10/39", response_.headers["Content-Range"]);
  EXPECT_EQ("close", response_.headers["Connection"]);
  EXPECT_EQ(9U, response_.data.length());
  EXPECT_EQ("9", response_.headers["Content-Length"]);
  EXPECT_EQ(index_range, response_.data);
#endif
}

TEST_F(HttpClientTest, Head) {
  response_ = client_.Head(localhost_);
  VerifyEmptyResponse(__LINE__);

  // Get a non-existent page. This returns data even for a HEAD request.
  response_ = client_.Head(localhost_ + "/does/not/exist");
  Verify404(__LINE__);

  // Get a fake file. The headers are returned but no data since this is a
  // HEAD request.
  response_ = client_.Head(localhost_ + "/index.html");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(response_.data.empty());
  EXPECT_EQ(7U, response_.headers.size());
  EXPECT_EQ("text/html", response_.headers["Content-Type"]);
  EXPECT_EQ("close", response_.headers["Connection"]);
}

TEST_F(HttpClientTest, Post) {
  // This will succeed since we have overriden handling of "/".
  response_ = client_.Post(localhost_, "post request");
  VerifyEmptyResponse(__LINE__);

  // A POST response is just like GET.
  response_ = client_.Post(localhost_ + "/index.html", "post request");
  VerifyIndexHtmlResponse(__LINE__);

  // This fails because the file is not found.
  response_ = client_.Post(localhost_ + "/does_not_exist", "post request");
  Verify404(__LINE__);

  // This fails because the path is not found.
  response_ = client_.Post(localhost_ + "/does/not/exist", "post request");
  Verify404(__LINE__);
}

TEST_F(HttpClientTest, Put) {
  // This will succeed since we have overridden handling of "/".
  response_ = client_.Put(localhost_, "put request");
  VerifyEmptyResponse(__LINE__);

  // A PUT on index.html will fail because we do not have permission to upload
  // to a memory file.
  response_ = client_.Put(localhost_ + "/index.html", "put request");
  EXPECT_EQ(401, response_.status);
  EXPECT_TRUE(response_.data.empty());
  EXPECT_EQ(2U, response_.headers.size());
  EXPECT_EQ("0", response_.headers["Content-Length"]);
  // The authenticate request will have a random nonce value.
  EXPECT_TRUE(base::StartsWith(response_.headers["WWW-Authenticate"],
                               "Digest"));

  response_ = client_.Post(localhost_ + "/does_not_exist", "put request");
  Verify404(__LINE__);

  // This fails because the path is not found.
  response_ = client_.Post(localhost_ + "/does/not/exist", "put request");
  Verify404(__LINE__);
}

}  // namespace testing
}  // namespace remote
}  // namespace ion
