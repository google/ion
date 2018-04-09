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

#include <fstream>  // NOLINT
#include <memory>
#include <sstream>  // NOLINT

#include "ion/base/logchecker.h"
#include "ion/base/logging.h"
#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/port/fileutils.h"
#include "ion/remote/httpclient.h"
#include "ion/remote/httpserver.h"
#include "ion/remote/tests/getunusedport.h"

#include "absl/memory/memory.h"
#include "third_party/easywsclient/easywsclient.hpp"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace remote {
namespace testing {

// Save some typing by typedefing the handler pointer.
typedef HttpServer::RequestHandlerPtr RequestHandlerPtr;

using std::placeholders::_1;
using std::bind;

namespace {

class HttpServerWebsocketTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Start a mongoose server.
    const int port = GetUnusedPort(500);
    const std::string port_string = base::ValueToString(port);
    localhost_ = "localhost:" + port_string;
    server_ = absl::make_unique<HttpServer>(port, 4);
    server_->SetHeaderHtml("");
    server_->SetFooterHtml("");
    EXPECT_TRUE(server_->IsRunning());

    // Send easywsclient messages to a file.
    easyws_file_name = port::GetTemporaryFilename();
    SetUpEasyWsStream();
  }

  void SetUpEasyWsStream() {
    easyws_fp = port::OpenFile(easyws_file_name, "w");
    ASSERT_FALSE(easyws_fp == nullptr);
    easywsclient::WebSocket::setMessageStream(easyws_fp);
  }


  // Returns a vector of strings containing all of the messages from
  // easywsclient (from a temporary file), also clearing the messages.
  const std::vector<std::string> GetEasywsclientMessages() {
    // Close for writing.
    fclose(easyws_fp);
    std::vector<std::string> msgs;
    {
      // Open for reading and read.
      std::ifstream in(easyws_file_name.c_str());
      msgs = base::SplitString(std::string(std::istreambuf_iterator<char>(in),
                                           std::istreambuf_iterator<char>()),
                               "\n");
    }
    // Open again for writing.
    SetUpEasyWsStream();
    return msgs;
  }

  void TearDown() override {
    // Use stderr for future easywsclient messages.
    easywsclient::WebSocket::setMessageStream(stderr);

    // Verify that there are no unexpected easywsclient messages and remove the
    // temporary file.
    EXPECT_TRUE(GetEasywsclientMessages().empty());
    port::RemoveFile(easyws_file_name);

    // Shutdown the server.
    EXPECT_TRUE(server_->IsRunning());
    server_.reset(nullptr);
  }

  std::unique_ptr<HttpServer> server_;
  std::string localhost_;
  std::string easyws_file_name;
  FILE *easyws_fp;
};

// When a message is received, treat it as a 32-bit unsigned int, and
// echo back an ASCII string with the same number.
class BinaryToAsciiWebsocket : public HttpServer::Websocket {
  int ReceiveData(char* data, size_t data_len, bool is_binary) override {
    if (data_len != 4) {
      // expected a 32-bit number... close the connection
      return 0;
    }
    DCHECK(is_binary)
        << "BinaryToAsciiWebsocket only supports binary messages.";
    uint8* d = reinterpret_cast<uint8*>(data);
    uint32 num = (d[0] << 24) + (d[1] << 16) + (d[2] << 8) + d[3];
    std::ostringstream s;
    s << num;
    SendData(s.str().c_str(), s.str().size(), false);
    return 1;
  }
};

// When a message is received, concatenate it with a prefix and suffix,
// and echo it back.
class PrefixSuffixWebsocket : public HttpServer::Websocket {
 public:
  PrefixSuffixWebsocket(const std::string& prefix, const std::string& suffix)
      : prefix_(prefix), suffix_(suffix) {
  }

  int ReceiveData(char* data, size_t data_len, bool is_binary) override {
    DCHECK(!is_binary)
        << "PrefixSuffixWebsocket does not support binary messages.";

    if (data_len == 0) {
      return 0;  // close connection
    } else {
      std::string msg = prefix_ + std::string(data, data_len) + suffix_;
      SendData(msg.c_str(), msg.size(), false);
      return 1;  // keep connection open
    }
  }

 private:
  std::string prefix_;
  std::string suffix_;
};


// Handler which responds to Websocket upgrade requests by setting up a new
// instance of the appropriate HttpServer::Websocket subclass.
class WebsocketTestHandler : public HttpServer::RequestHandler {
 public:
  explicit WebsocketTestHandler(const std::string& path)
      : HttpServer::RequestHandler(path) {}

  // This class doesn't handle general HTTP requests, only websocket requests.
  const std::string HandleRequest(const std::string& path,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override {
    return std::string("");
  }

  const HttpServer::WebsocketPtr ConnectWebsocket(
      const std::string& path,
      const HttpServer::QueryMap& const_args) override {
    // For convenience, so we can use operator[].
    HttpServer::QueryMap& args = const_cast<HttpServer::QueryMap&>(const_args);

    if (path == "prefix_suffix") {
      return HttpServer::WebsocketPtr(
          new PrefixSuffixWebsocket(args["prefix"], args["suffix"]));
    } else if (path == "binary") {
      return HttpServer::WebsocketPtr(new BinaryToAsciiWebsocket);
    } else {
      return HttpServer::RequestHandler::ConnectWebsocket(path, const_args);
    }
  }
};

// Each of these tests whether a string is a particular type of easywsclient
// message.
static bool IsConnectingMessage(const std::string& msg) {
  return base::StartsWith(msg, "easywsclient: connecting:");
}
static bool IsConnectedMessage(const std::string& msg) {
  return base::StartsWith(msg, "Connected to:");
}
static bool IsConnectionClosedMessage(const std::string& msg) {
  return msg == "Connection closed!";
}

}  // anonymous namespace

typedef easywsclient::WebSocket ClientSocket;

// Verify that no websocket connection is made when:
// - no handler is found for the specified path
// - when a handler is found, but it chooses to reject the connection based
//   on some arbitrary criteria (in this case, we ask for "bad_socket_type"
//   instead of "prefix_suffix").
TEST_F(HttpServerWebsocketTest, BadPath) {
  RequestHandlerPtr handler(new WebsocketTestHandler("/test_handler"));
  server_->RegisterHandler(handler);

  const std::string base("ws://" + localhost_);
  ClientSocket* socket1 = ClientSocket::from_url(base + "/bad_handler_path");
  ClientSocket* socket2 = ClientSocket::from_url(
      base + "/test_handler/bad_socket_type");

  EXPECT_TRUE(socket1 == nullptr);
  EXPECT_TRUE(socket2 == nullptr);

  const std::vector<std::string> msgs = GetEasywsclientMessages();
  EXPECT_EQ(2U, msgs.size());
  EXPECT_TRUE(IsConnectingMessage(msgs[0]));
  EXPECT_TRUE(IsConnectingMessage(msgs[1]));
}

// Store the message in a vector so that we can later verify that the correct
// messages were received in the correct order.
void ReceiveMessage(const std::string& msg, std::vector<std::string>* recv) {
  recv->push_back(msg);
}

// Verify that the server can establish websocket connections, and respond
// appropriately to messages.
TEST_F(HttpServerWebsocketTest, SendAndReceive) {
  RequestHandlerPtr handler(new WebsocketTestHandler("/test_handler"));
  server_->RegisterHandler(handler);

  const std::string base("ws://" + localhost_);
  ClientSocket* socket1 = ClientSocket::from_url(base +
      "/test_handler/prefix_suffix?prefix=socket1----&suffix=----socket1");
  ClientSocket* socket2 = ClientSocket::from_url(base +
      "/test_handler/prefix_suffix?prefix=socket2----&suffix=----socket2");

  // Ensure that connections were made successfully... if these fail,
  // the code below would surely crash.
  ASSERT_TRUE(socket1 != nullptr);
  ASSERT_TRUE(socket2 != nullptr);
  EXPECT_EQ(server_->WebsocketCount(), 2U);

  // Send messages that will be bounced back after being wrapped
  // in the prefix/suffix.
  socket1->send("   MSG_ONE   ");
  socket2->send("   MSG_ONE   ");
  socket2->send("   MSG_TWO   ");
  socket1->send("   MSG_TWO   ");

  // Request to close connection.
  socket1->send("");
  socket2->send("");

  std::vector<std::string> socket1_received_messages, socket2_received_messages;
  while (socket1->getReadyState() != ClientSocket::CLOSED ||
         socket2->getReadyState() != ClientSocket::CLOSED) {
    socket1->poll();
    socket2->poll();
    socket1->dispatch(bind(ReceiveMessage, _1, &socket1_received_messages));
    socket2->dispatch(bind(ReceiveMessage, _1, &socket2_received_messages));
  }

  EXPECT_EQ(socket1_received_messages[0],
            "socket1----   MSG_ONE   ----socket1");
  EXPECT_EQ(socket1_received_messages[1],
            "socket1----   MSG_TWO   ----socket1");
  EXPECT_EQ(socket2_received_messages[0],
            "socket2----   MSG_ONE   ----socket2");
  EXPECT_EQ(socket2_received_messages[1],
            "socket2----   MSG_TWO   ----socket2");

  EXPECT_EQ(server_->WebsocketCount(), 0U);

  delete socket1;
  delete socket2;

  const std::vector<std::string> msgs = GetEasywsclientMessages();
  EXPECT_EQ(6U, msgs.size());
  EXPECT_TRUE(IsConnectingMessage(msgs[0]));
  EXPECT_TRUE(IsConnectedMessage(msgs[1]));
  EXPECT_TRUE(IsConnectingMessage(msgs[2]));
  EXPECT_TRUE(IsConnectedMessage(msgs[3]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[4]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[5]));
}

// Verify that we handle frames of various sizes... depending on the size,
// the length is encoded differently.
TEST_F(HttpServerWebsocketTest, MultipleFrameSizes) {
  RequestHandlerPtr handler(new WebsocketTestHandler("/test_handler"));
  server_->RegisterHandler(handler);

  ClientSocket* socket = ClientSocket::from_url("ws://" + localhost_ +
                                                "/test_handler/prefix_suffix");
  ASSERT_TRUE(socket != nullptr);

  std::string medium(50000, 'X');  // encode length in 2 bytes
  std::string large(100000, 'Y');  // encode length in 8 bytes

  socket->send(medium);
  socket->send(large);
  socket->send("");

  std::vector<std::string> received_messages;
  while (socket->getReadyState() != ClientSocket::CLOSED) {
    socket->poll();
    socket->dispatch(bind(ReceiveMessage, _1, &received_messages));
  }

  ASSERT_EQ(received_messages.size(), 2U);
  EXPECT_EQ(received_messages[0], medium);
  EXPECT_EQ(received_messages[1].size(), large.size());

  delete socket;

  const std::vector<std::string> msgs = GetEasywsclientMessages();
  EXPECT_EQ(3U, msgs.size());
  EXPECT_TRUE(IsConnectingMessage(msgs[0]));
  EXPECT_TRUE(IsConnectedMessage(msgs[1]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[2]));
}

// Return a string containing a binary big-endian encoding of the provided int.
std::string ToBigEndian(uint32 integer) {
  uint8 c[4];
  c[3] = integer & 0xFF;
  integer >>= 8;
  c[2] = integer & 0xFF;
  integer >>= 8;
  c[1] = integer & 0xFF;
  integer >>= 8;
  c[0] = integer & 0xFF;

  return std::string(reinterpret_cast<char*>(c), 4);
}

// Verify that server can properly handle binary data.
TEST_F(HttpServerWebsocketTest, TestBinary) {
  RequestHandlerPtr handler(new WebsocketTestHandler("/test_handler"));
  server_->RegisterHandler(handler);

  ClientSocket* socket =
      ClientSocket::from_url("ws://" + localhost_ + "/test_handler/binary");

  socket->sendData(ClientSocket::BINARY_FRAME, ToBigEndian(1234U), true);
  socket->sendData(ClientSocket::BINARY_FRAME, ToBigEndian(1584372126U), true);
  socket->sendData(ClientSocket::BINARY_FRAME, ToBigEndian(475934U), true);
  socket->send("");

  std::vector<std::string> received_messages;
  while (socket->getReadyState() != ClientSocket::CLOSED) {
    socket->poll();
    socket->dispatch(bind(ReceiveMessage, _1, &received_messages));
  }

  ASSERT_EQ(received_messages.size(), 3U);
  EXPECT_EQ(received_messages[0], "1234");
  EXPECT_EQ(received_messages[1], "1584372126");
  EXPECT_EQ(received_messages[2], "475934");

  delete socket;

  const std::vector<std::string> msgs = GetEasywsclientMessages();
  EXPECT_EQ(3U, msgs.size());
  EXPECT_TRUE(IsConnectingMessage(msgs[0]));
  EXPECT_TRUE(IsConnectedMessage(msgs[1]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[2]));
}

// Verify that server can handle multi-frame messages.
TEST_F(HttpServerWebsocketTest, MultiFrameMessages) {
  RequestHandlerPtr handler(new WebsocketTestHandler("/test_handler"));
  server_->RegisterHandler(handler);

  const std::string base("ws://" + localhost_);
  ClientSocket* text_socket = ClientSocket::from_url(base +
      "/test_handler/prefix_suffix?prefix=MULTI----&suffix=----FRAME");
  ClientSocket* binary_socket = ClientSocket::from_url(base +
      "/test_handler/binary");

  // Ensure that connections were made successfully... if these fail,
  // the code below would surely crash.
  ASSERT_TRUE(text_socket != nullptr);
  ASSERT_TRUE(binary_socket != nullptr);
  EXPECT_EQ(server_->WebsocketCount(), 2U);

  // Send 2 text messages and 2 binary messages, each broken into 4 frames.
  text_socket->sendData(ClientSocket::TEXT_FRAME, "AA", false);
  text_socket->sendData(ClientSocket::CONTINUATION, "BB", false);
  text_socket->sendData(ClientSocket::CONTINUATION, "CC", false);
  text_socket->sendData(ClientSocket::CONTINUATION, "DD", true);
  text_socket->sendData(ClientSocket::TEXT_FRAME, "EE", false);
  text_socket->sendData(ClientSocket::CONTINUATION, "FF", false);
  text_socket->sendData(ClientSocket::CONTINUATION, "GG", false);
  text_socket->sendData(ClientSocket::CONTINUATION, "HH", true);
  std::string msg1 = ToBigEndian(123456789U);
  std::string msg2 = ToBigEndian(987654321U);
  binary_socket->sendData(ClientSocket::BINARY_FRAME, msg1.substr(0, 1), false);
  binary_socket->sendData(ClientSocket::CONTINUATION, msg1.substr(1, 1), false);
  binary_socket->sendData(ClientSocket::CONTINUATION, msg1.substr(2, 1), false);
  binary_socket->sendData(ClientSocket::CONTINUATION, msg1.substr(3, 1), true);
  binary_socket->sendData(ClientSocket::BINARY_FRAME, msg2.substr(0, 1), false);
  binary_socket->sendData(ClientSocket::CONTINUATION, msg2.substr(1, 1), false);
  binary_socket->sendData(ClientSocket::CONTINUATION, msg2.substr(2, 1), false);
  binary_socket->sendData(ClientSocket::CONTINUATION, msg2.substr(3, 1), true);

  // Instead of explicitly requesting socket-closure, send intentionally-bad
  // frame sequences.  In one case send a continuation frame with no previous
  // data frame, and in the second start a new message without closing the
  // previous continuation.  Both of these should cause the socket to close.
  text_socket->sendData(ClientSocket::CONTINUATION, "XX", true);
  binary_socket->sendData(ClientSocket::BINARY_FRAME, msg1.substr(0, 1), false);
  binary_socket->sendData(ClientSocket::BINARY_FRAME, msg1.substr(0, 1), false);

  // Receive responses from the server.
  std::vector<std::string> received_text_messages, received_binary_messages;
  while (text_socket->getReadyState() != ClientSocket::CLOSED ||
         binary_socket->getReadyState() != ClientSocket::CLOSED) {
    text_socket->poll();
    text_socket->dispatch(
        bind(ReceiveMessage, _1, &received_text_messages));
    binary_socket->poll();
    binary_socket->dispatch(
        bind(ReceiveMessage, _1, &received_binary_messages));
  }

  ASSERT_EQ(2U, received_text_messages.size());
  ASSERT_EQ(2U, received_binary_messages.size());

  EXPECT_EQ(received_text_messages[0], "MULTI----AABBCCDD----FRAME");
  EXPECT_EQ(received_text_messages[1], "MULTI----EEFFGGHH----FRAME");
  EXPECT_EQ(received_binary_messages[0], "123456789");
  EXPECT_EQ(received_binary_messages[1], "987654321");

  EXPECT_EQ(server_->WebsocketCount(), 0U);

  delete text_socket;
  delete binary_socket;

  const std::vector<std::string> msgs = GetEasywsclientMessages();
  EXPECT_EQ(6U, msgs.size());
  EXPECT_TRUE(IsConnectingMessage(msgs[0]));
  EXPECT_TRUE(IsConnectedMessage(msgs[1]));
  EXPECT_TRUE(IsConnectingMessage(msgs[2]));
  EXPECT_TRUE(IsConnectedMessage(msgs[3]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[4]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[5]));
}

// Verify correct responses to Ping, Pong, and Close messages, as well as
// messages with invalid opcodes.
TEST_F(HttpServerWebsocketTest, PingPongCloseBad) {
  RequestHandlerPtr handler(new WebsocketTestHandler("/test_handler"));
  server_->RegisterHandler(handler);
  base::LogChecker log_checker;

  const std::string binary_address("ws://" + localhost_ +
                                   "/test_handler/binary");
  ClientSocket* socket1 =
      ClientSocket::from_url(binary_address);
  ClientSocket* socket2 =
      ClientSocket::from_url(binary_address);
  ClientSocket* socket3 =
      ClientSocket::from_url(binary_address);
  ASSERT_TRUE(socket1);
  ASSERT_TRUE(socket2);
  ASSERT_TRUE(socket3);

  // Should result in a ping.
  socket1->sendData(ClientSocket::PING, "ignore", true);
  // Connection should have been left open after ping/pong, so we should
  // be able to send another message.
  socket1->sendData(ClientSocket::BINARY_FRAME, ToBigEndian(31337U), true);
  // Should result in a closed connection (since we asked).
  socket1->sendData(ClientSocket::CLOSE, "ignore", true);
  // Should result in a closed connection (since server sent no ping).
  socket2->sendData(ClientSocket::PONG, "ignore", true);
  // Should result in a closed connection (since opcode is unknown).
  socket3->sendData(static_cast<ClientSocket::Opcode>(3), "ignore", true);

  std::vector<std::string> received_messages;
  while (socket1->getReadyState() != ClientSocket::CLOSED ||
         socket2->getReadyState() != ClientSocket::CLOSED ||
         socket3->getReadyState() != ClientSocket::CLOSED) {
    socket1->poll();
    socket2->poll();
    socket3->poll();
    socket1->dispatch(bind(ReceiveMessage, _1, &received_messages));
    socket2->dispatch(bind(ReceiveMessage, _1, &received_messages));
    socket3->dispatch(bind(ReceiveMessage, _1, &received_messages));
  }
  EXPECT_EQ(0U, server_->WebsocketCount());
  ASSERT_EQ(1U, received_messages.size());
  EXPECT_EQ(received_messages[0], "31337");
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "Unrecognized websocket opcode"));

  delete socket1;
  delete socket2;
  delete socket3;

  const std::vector<std::string> msgs = GetEasywsclientMessages();
  EXPECT_EQ(9U, msgs.size());
  EXPECT_TRUE(IsConnectingMessage(msgs[0]));
  EXPECT_TRUE(IsConnectedMessage(msgs[1]));
  EXPECT_TRUE(IsConnectingMessage(msgs[2]));
  EXPECT_TRUE(IsConnectedMessage(msgs[3]));
  EXPECT_TRUE(IsConnectingMessage(msgs[4]));
  EXPECT_TRUE(IsConnectedMessage(msgs[5]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[6]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[7]));
  EXPECT_TRUE(IsConnectionClosedMessage(msgs[8]));

  EXPECT_FALSE(log_checker.HasAnyMessages());
}

// Improve code-coverage stats.
TEST_F(HttpServerWebsocketTest, MakeCoverageHappy) {
  RequestHandlerPtr handler(new WebsocketTestHandler("/test_handler"));
  server_->RegisterHandler(handler);
  HttpClient client;
  HttpClient::Response response =
      client.Get("http://" + localhost_ + "/test_handler");
  EXPECT_EQ(404, response.status);
}

}  // namespace testing
}  // namespace remote
}  // namespace ion

#endif
