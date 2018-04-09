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

#include "ion/remote/httpserver.h"

#if !ION_PRODUCTION

#include <stdio.h>

#include <algorithm>
#include <cstring>
#include <mutex>  // NOLINT(build/c++11)
#include <sstream>

#include "ion/base/logging.h"
#include "ion/base/scopedallocation.h"
#include "ion/base/stringutils.h"

#include "third_party/mongoose/mongoose.h"

#endif  // !ION_PRODUCTION

namespace ion {
namespace remote {

#if !ION_PRODUCTION

// Encapsulates the websocket interface.
class HttpServer::WebsocketHelper {
 public:
  explicit WebsocketHelper(mg_connection* conn)
      : connection_(conn), ready_(false), binary_(false) {
  }
  ~WebsocketHelper() {
    websocket_->helper_ = nullptr;
  }

  void SetWebsocket(const WebsocketPtr& websocket) {
    websocket_ = websocket;
    websocket_->SetHelper(this);
  }
  void Register(HttpServer* server) {
    server->RegisterWebsocket(connection_, this);
  }
  void Unregister(HttpServer* server) {
    server->UnregisterWebsocket(connection_);
  }

  void ConnectionReady() {
    ready_ = true;
    websocket_->ConnectionReady();
  }

  enum Opcode {
    CONTINUATION = 0,
    TEXT         = 1,
    BINARY       = 2,
    CLOSE        = 8,
    PING         = 9,
    PONG         = 10
  };

  int ReceiveData(uint8 bits, char* data, size_t data_len);
  void SendData(Opcode opcode, const char* data, size_t data_len);

  // Since we're already friends with HttpServer, this static method
  // allows eg: WebsocketData() to access HttpServer::FindWebsocket()
  // without making it public.
  static WebsocketHelper* FindWebsocket(HttpServer* server,
                                        mg_connection* connection) {
    return server->FindWebsocket(connection);
  }

 private:
  int BeginContinuation(bool is_binary, char* data, size_t data_len);
  void AppendContinuationData(char data[], size_t data_len);

  mg_connection* connection_;
  WebsocketPtr websocket_;
  bool ready_;
  // Accumulates fragmented messages until a fragment with the FIN bit arrives.
  std::vector<char> continuation_;
  // If currently accumulating a fragmented message, remember whether the
  // message is in binary or text mode.
  bool binary_;
  std::mutex mutex_;
};

int HttpServer::WebsocketHelper::ReceiveData(
    uint8 bits, char* data, size_t data_len) {

  Opcode opcode = static_cast<Opcode>(bits & 0xF);
  bool fin = bits & 0x80;

  switch (opcode) {
    case CONTINUATION:
      if (continuation_.empty()) {
        // Continuation frame received, but no previous data...
        // close the connection!
        return 0;
      }
      AppendContinuationData(data, data_len);
      if (fin) {
        // Whole message is assembled; pass it up for processing.
        int result = websocket_->ReceiveData(
            &(continuation_[0]), continuation_.size(), binary_);
        continuation_.clear();
        return result;
      } else {
        // Wait for more data.
        return 1;
      }
    case TEXT:
      if (fin) {
        // Entire message is in this one frame.
        return websocket_->ReceiveData(data, data_len, false);
      } else {
        return BeginContinuation(false, data, data_len);
      }
    case BINARY:
      if (fin) {
        // Entire message is in this one frame.
        return websocket_->ReceiveData(data, data_len, true);
      } else {
        return BeginContinuation(true, data, data_len);
      }
    case CLOSE:
      return 0;
    case PING:
      // Answer with a PONG containing the same data.
      SendData(PONG, data, data_len);
      return 1;
    case PONG:
      // Unexpected, since PONG should only be sent in response to PING,
      // and we never send PING.  Close the connection.
      return 0;
    default:
      LOG(WARNING) << "Unrecognized websocket opcode: " << opcode;
      return 0;
  }
}

int HttpServer::WebsocketHelper::BeginContinuation(
    bool is_binary, char* data, size_t data_len) {
  if (!continuation_.empty()) {
    // Unfinished continuation already exists... close the connection!
    continuation_.clear();
    return 0;
  }
  binary_ = is_binary;
  AppendContinuationData(data, data_len);
  return 1;
}

void HttpServer::WebsocketHelper::AppendContinuationData(
    char data[], size_t data_len) {
  continuation_.insert(continuation_.end(), data, data + data_len);
}

void HttpServer::WebsocketHelper::SendData(
    Opcode opcode, const char* data, size_t data_len) {
  // The header will be 2, 4, or 10 bytes, depending on data_len.
  // See http://tools.ietf.org/html/rfc6455#section-5.2.
  uint8 header[10];
  size_t header_size;

  // Set the opcode and the FIN bit (send all of the data in a single frame).
  header[0] = static_cast<uint8>(0x80 | (opcode & 0xF));

  // Store data_len in the header.  Since we're the server, we MUST NOT mask
  // any frames that we send to the client, so we don't set the mask bit.
  if (data_len < 126) {
    // data_len fits in 7 bits
    header[1] = static_cast<uint8>(data_len);
    header_size = 2;
  } else if (data_len <= 0xFFFF) {
    // data_len fits in 2 bytes; store this after the "126" code.
    header[1] = 126;
    header[2] = static_cast<uint8>(data_len >> 8);
    header[3] = static_cast<uint8>(data_len & 0xFF);
    header_size = 4;
  } else {
    // data_len fits in 8 bytes; store this after the "127" code.
    header[1] = 127;
    size_t shifted_len = data_len;
    for (int i = 7; i >= 0; i--) {
      header[2+i] = static_cast<uint8>(shifted_len & 0xFF);
      shifted_len >>= 8;
    }
    header_size = 10;
  }

  // Synchronize write access.
  std::lock_guard<std::mutex> lock(mutex_);
  mg_write(connection_, header, header_size);
  mg_write(connection_, data, data_len);
}

namespace {

// Pipe all mongoose log messages through Ion's logging. Mongoose only emits
// messages when an error occurs.
static int LogCallback(const mg_connection*, const char* message) {
  LOG(ERROR) << "Mongoose: " << message;
  return 1;
}

// Returns a QueryMap based on the arguments in the passed string.
static HttpServer::QueryMap BuildQueryMap(const char* query_string) {
  HttpServer::QueryMap args;
  if (query_string) {
    const std::vector<std::string> queries =
        base::SplitString(query_string, "&");
    const size_t num_queries = queries.size();
    for (size_t i = 0; i < num_queries; ++i) {
      const std::vector<std::string> pairs =
          base::SplitString(queries[i], "=");
      // Query args are not required to have an '=' if they have no value.
      DCHECK(pairs.size() == 1 || pairs.size() == 2);
      if (pairs.size() > 1) {
        // URL-decode the variable data.
        // Ensure the buffer is large enough if the string is completely
        // encoded.
        const size_t buffer_length = pairs[1].length() * 5;
        base::ScopedAllocation<char> buffer(base::kShortTerm, buffer_length);
        mg_url_decode(pairs[1].c_str(), static_cast<int>(pairs[1].length()),
                      buffer.Get(), static_cast<int>(buffer_length), 1);

        args[pairs[0]] = buffer.Get();
      } else {
        args[pairs[0]] = "";
      }
    }
  }
  return args;
}

// Return a path relative to the handler's base path.  Strips any leading or
// trailing '/' to produce a relative path that does not end in '/'.
static const std::string MakeRelativePath(
  const HttpServer::RequestHandlerPtr& handler, const std::string& full_path) {
  // Handle the case where full_path is shorter than base_path.  For example,
  // we might have full_path == "/path" and base_path == "/path/".
  const std::string& base_path = handler->GetBasePath();
  const std::string path = full_path.length() < base_path.length()
      ? std::string()
      : full_path.substr(base_path.length(),
                         full_path.length() - base_path.length());

  size_t start_pos = path.find_first_not_of('/');
  if (start_pos == std::string::npos)
    start_pos = 0;
  if (path == "/")
    start_pos = 1;
  size_t end_pos = path.find_last_not_of('/');
  if (end_pos == std::string::npos)
    end_pos = path.length();
  else
    end_pos++;

  return path.substr(start_pos, end_pos - start_pos);
}

// Prototype for EmbedAllLocalTags(), since it calls and is called by
// GetFileData().
static const std::string EmbedAllLocalTags(
    const std::string& source, const HttpServer::HandlerMap& handlers);


static HttpServer::RequestHandlerPtr FindHandlerForPath(
    const std::string& path, const HttpServer::HandlerMap& handlers) {
  // Find a handler for path. First try the path itself, then ascend up
  // the directories it contains until we either find a handler or try the root
  // directory.
  std::string search_path = path;
  while (search_path.length()) {
    HttpServer::HandlerMap::const_iterator found = handlers.find(search_path);
    // See if there is a registered handler for search_path.
    if (found != handlers.end()) {
      // Found a handler; return it.
      return found->second;
    } else {
      // Try shorten the path to find a handler.
      const size_t pos = search_path.rfind("/");
      if (pos == 0U && search_path.length() > 1) {
        // The path is /<file>, so search for a root handler next.
        search_path = "/";
      } else {
        // Shorten the path by the last '/';
        search_path = search_path.substr(0, pos);
      }
    }
  }
  // No handler was found
  static const HttpServer::RequestHandlerPtr no_handler;
  return no_handler;
}

static const std::string GetFileData(const std::string& path,
                                     const char* query_string,
                                     const std::string& header_html,
                                     const std::string& footer_html,
                                     const HttpServer::HandlerMap& handlers,
                                     std::string* content_type,
                                     bool embed_local_sourced_files) {
  HttpServer::RequestHandlerPtr handler = FindHandlerForPath(path, handlers);
  if (!handler.Get()) {
    // If no request handler exists then mongoose will attempt to serve
    // the path directly, which will result in a 404 error since we use a
    // dummy root path.
    return std::string();
  }

  // Place any query arguments into the QueryMap.
  const HttpServer::QueryMap args = BuildQueryMap(query_string);

  // Call the handler, stripping out the handler's base path.
  std::string response = handler->HandleRequest(
      MakeRelativePath(handler, path), args, content_type);

  // If we are embedding files into HTML, then we have to parse the response
  // and call GetFileData on any local files.
  if (*content_type == "text/html") {
    // Replace the header and footer tags with the header and footer html.
    if (!header_html.empty())
      response = base::ReplaceString(response, "<!--HEADER-->", header_html);
    if (!footer_html.empty())
      response = base::ReplaceString(response, "<!--FOOTER-->", footer_html);
    if (embed_local_sourced_files)
      response = EmbedAllLocalTags(response, handlers);
  }
  return response;
}

// Searches html from pos for all tags of the passed type that contain the
// passed attribute. If one is found, then then pos is updated to the start
// of the found tag, the entire tag is returned in target, the data of the
// referenced file is returned in data (if the file exists) and the content type
// of the file is also set (based on the file's extension). Returns whether the
// tag was found.
static bool FindLocallyReferencedTag(
    const std::string& html, const HttpServer::HandlerMap& handlers,
    const std::string& tag_in, const std::string& attribute, size_t* pos,
    std::string* target, std::string* data, std::string* content_type) {
  data->clear();
  target->clear();
  const std::string tag = "<" + tag_in;
  *pos = html.find(tag, *pos);
  if (*pos == std::string::npos) {
    return false;
  } else {
    // Find the end of the current tag.
    const size_t end = html.find(">", *pos);
    if (end != std::string::npos) {
      // Store the entire tag in target.
      *target = html.substr(*pos, end - *pos + 1U);
      // Find all of the tag's attributes.
      const std::vector<std::string> attributes =
          base::SplitString(*target, " =>");
      const size_t count = attributes.size();
      // Look for the passed attribute.
      for (size_t i = 0; i < count; ++i) {
        if (attributes[i] == attribute && i < count - 1U) {
          // Remove any quotes and extract the path to the file.
          const std::string& attr = attributes[i + 1U];
          const std::string path = attr.substr(attr.find_first_not_of("\"'"),
                                               attr.find_last_not_of("\"'"));
          // Only embed local files, which we assume all start with '/'.
          if (base::StartsWith(path, "/")) {
            *content_type = mg_get_builtin_mime_type(path.c_str());
            *data = GetFileData(path, "", "", "", handlers, content_type, true);
          }
          break;
        }
      }
    }
    return true;
  }
}

// The below functions return valid HTML tags that have the passed data embedded
// into the tag.
static const std::string FormatImgTag(const std::string& content_type,
                                      const std::string& data) {
  // <img src="path" -> <img src="<type>;base64,base64encoded[src]"
  return "<img src='data:" + content_type + ";base64," +
      base::MimeBase64EncodeString(data) + "'>";
}
static const std::string FormatLinkTag(const std::string& content_type,
                                       const std::string& data) {
  // <link rel="stylesheet" href="path"> -> <style> [href] </style>
  return "<style>\n" + data + "\n</style>\n";
}
static const std::string FormatScriptTag(const std::string& content_type,
                                         const std::string& data) {
  // <script type="text/javascript" src="path"> -> <script> [src] </script>
  return "<script>\n" + data + "\n";
}

// Returns a string where all tags of the passed type with the passed attribute
// that reference local files (files starting with '/') are embedded directly
// into the HTML. The embedding is done by replacing the tag containing the path
// to the file with a new tag containing the data itself. The details of how
// exactly to replace the string are handled by the passed formatting function.
static const std::string EmbedLocalTags(
    const std::string& source,
    const HttpServer::HandlerMap& handlers,
    const std::string& tag,
    const std::string& attribute,
    const std::string (*formatter)(const std::string& content_type,
                                   const std::string& data)) {
  std::string html = source;
  size_t pos = 0U;
  std::string data;
  std::string target;
  std::string content_type;
  while (FindLocallyReferencedTag(
      html, handlers, tag, attribute, &pos, &target, &data, &content_type)) {
    if (!data.empty()) {
      // Format the returned data into a valid tag.
      const std::string replacement = formatter(content_type, data);
      html = base::ReplaceString(html, target, replacement);
      // Skip the length of the replacement; do not recurse into it.
      pos += replacement.length();
    } else {
      // Skip the tag that was found, since we aren't doing any replacement.
      pos += target.length();
    }
  }
  return html;
}

// Returns a string where all <img>, <link>, and <script> tags that reference
// local files (files starting with '/') are embedded directly into the HTML.
static const std::string EmbedAllLocalTags(
    const std::string& source, const HttpServer::HandlerMap& handlers) {
  std::string html = source;
  html = EmbedLocalTags(html, handlers, "img", "src", FormatImgTag);
  html = EmbedLocalTags(html, handlers, "link", "href", FormatLinkTag);
  html = EmbedLocalTags(html, handlers, "script", "src", FormatScriptTag);
  return html;
}

// Sends an HTTP status across the passed connection and closes it. The passed
// status should have the form "<code> <message>", for example "404 Not Found"
// or "501 Not Implemented". The passed text should be a brief yet more
// descriptive message such as "The requested file was not found."
static void SendStatusCode(mg_connection* connection, const std::string& status,
                           const std::string& text) {
  std::stringstream header_stream;
  header_stream << "HTTP/1.1 " << status << "\r\n"
                << "Content-Length: " << text.length() << "\r\n"
                << "Connection: close\r\n\r\n" << text;
  const std::string headers = header_stream.str();
  mg_printf(connection, "%s", headers.c_str());
}

// Extracts a range request from a Range header, given a content_length for a
// requested file. If the header is valid, then sets range_start and range_end
// to the first and last byte to be sent, and sets range to an appropriate
// Content-Range header. If the range request header is malformed then range is
// unmodified. Returns whether the request was valid and range was modified.
static bool ParseRangeRequest(const char* range_header,
                              int64 content_length,
                              int64* range_start,
                              int64* range_end,
                              std::string* range) {
  static const size_t kEqualPos = 5U;

  bool valid_request = false;
  // Parse the range out of the header, which should have the form:
  // bytes=(\d+)-(\d+). We can't use sscanf since it is dangerous and broken on
  // some versions of Android, while std::regex is not implemented on some older
  // toolchains. Since the header is simple enough, just manually parse the
  // header and ensure it has the proper format.
  const std::string header_string(range_header);
  if (header_string.find("=") == kEqualPos &&
      header_string.substr(0, kEqualPos) == "bytes") {
    std::istringstream header(header_string.substr(kEqualPos + 1U));
    int64 start = 0, end = 0;
    header >> start >> end;
    valid_request = !header.fail() && end < 0;
    if (valid_request) {
      *range_start = start;
      *range_end = std::min(-end, content_length);
      std::stringstream range_stream;
      range_stream << "Content-Range: bytes " << *range_start << "-"
                   << *range_end << "/" << content_length << "\r\n";
      *range = range_stream.str();
    }
  }
  return valid_request;
}

// Sends the passed file data across the passed connection.
static void SendFileData(mg_connection* connection,
                         const std::string& method,  // GET, HEAD, or POST.
                         const std::string& content_type,
                         const std::string& data) {
  // We have data to return, so the status is ok.
  std::string status("200 OK");
  // If the client asked for a certain range then extract the range and
  // set the proper status and header.
  int64 range_start = 0;
  int64 range_end = static_cast<int64>(data.length());
  // If the client requested a byte range then we send a Partial Content status
  // code. If there is no range request then range will be empty, which is ok.
  std::string range;
  if (const char* header = mg_get_header(connection, "Range"))
    if (ParseRangeRequest(header, range_end, &range_start, &range_end, &range))
      status = "206 Partial Content";

  // Create the headers that describe the file data.
  std::stringstream header_stream;
  header_stream << "HTTP/1.1 " << status << "\r\n"
                << "Content-Type: " << content_type << "\r\n"
                << "Content-Length: " << data.length() << "\r\n"
                << "Connection: close\r\n"
                << "Accept-Ranges: bytes\r\n" << range << "\r\n";

  // Write headers to the connection.
  const std::string headers = header_stream.str();
  mg_printf(connection, "%s", headers.c_str());
  // Servicing a HEAD request requires only headers, not a body.
  if (method != "HEAD") {
    const size_t size = static_cast<size_t>(range_end - range_start + 1);
    mg_write(connection, &data[static_cast<size_t>(range_start)], size);
  }
}

// Handles an HTTP HEAD, GET, or POST request by searching for a file handler
// and invoking it on the requested path. Sends a 404 Not Found error if there
// is no registered handler for the requested path.
static int BeginRequestCallback(mg_connection* connection) {
  const mg_request_info* info = mg_get_request_info(connection);
  // Mongoose doesn't differentiate between Websocket upgrade requests and
  // other requests when calling the begin_request callback (it detects the
  // upgrade request later), so we need to avoid sending any data back before
  // Mongoose does the Websocket handshake.
  if (nullptr != mg_get_header(connection, "Sec-WebSocket-Key")) {
    return 0;
  }
  // Extract out the request method.
  const std::string method(info->request_method);
  if (method == "GET" || method == "POST" || method == "HEAD") {
    // The server is stored in the user_data field since it was passed to
    // mg_start().
    HttpServer* server = reinterpret_cast<HttpServer*>(info->user_data);
    DCHECK(server);
    // Get a default content type based on the requested path.
    std::string content_type = mg_get_builtin_mime_type(info->uri);
    // Try to get data for the requested path.
    const std::string data =
        GetFileData(info->uri, info->query_string, server->GetHeaderHtml(),
                    server->GetFooterHtml(), server->GetHandlers(),
                    &content_type, server->EmbedLocalSourcedFiles());

    if (data.empty()) {
      SendStatusCode(connection, "404 Not Found",
                     "Error 404: Not Found\nThe requested file was not found.");
    } else {
      SendFileData(connection, method, content_type, data);
    }
    return 1;
  } else {
    return 0;
  }
}

// Mongoose "websocket_connect" callback function.  Called when websocket
// request is received, before handshake is performed.
static int WebsocketConnect(const mg_connection* connection) {
  mg_connection* unconst_connection = const_cast<mg_connection*>(connection);
  // We won't modify the conneciton, but need to deconstify it
  // in order to pass it to mg_get_request_info().
  const mg_request_info* info = mg_get_request_info(unconst_connection);

  // The server is stored in the user_data field since it was passed to
  // mg_start().
  HttpServer* server = reinterpret_cast<HttpServer*>(info->user_data);
  HttpServer::RequestHandlerPtr handler =
      FindHandlerForPath(info->uri, server->GetHandlers());
  if (!handler.Get()) {
    // No request-handler exists, so reject connection by returning non-zero.
    return -1;
  }

  // Place any query arguments into the QueryMap.
  const HttpServer::QueryMap args = BuildQueryMap(info->query_string);
  const std::string path = MakeRelativePath(handler, info->uri);

  HttpServer::WebsocketPtr websocket = handler->ConnectWebsocket(path, args);
  if (!websocket.Get()) {
    // The request-handler chose not to allow a websocket connection.
    return -1;
  }
  HttpServer::WebsocketHelper* helper =
      new HttpServer::WebsocketHelper(unconst_connection);
  helper->SetWebsocket(websocket);
  helper->Register(server);
  // OK to proceed with the websocket handshake.
  return 0;
}

// Mongoose "websocket_ready" callback function.  Called after handshake
// is finished, and connection is ready for bi-directional data exchange.
static void WebsocketReady(mg_connection* connection) {
  const mg_request_info* info = mg_get_request_info(connection);
  // The server is stored in the user_data field since it was passed
  // to mg_start().
  HttpServer* server = reinterpret_cast<HttpServer*>(info->user_data);
  HttpServer::WebsocketHelper::FindWebsocket(server, connection)->
      ConnectionReady();
}

// Mongoose "websocket_data" callback function.  Called when a data frame
// is received from the client.
static int WebsocketData(mg_connection* connection, int bits,
                         char* data, size_t data_len) {
  const mg_request_info* info = mg_get_request_info(connection);
  // The server is stored in the user_data field since it was passed to
  // mg_start().
  HttpServer* server = reinterpret_cast<HttpServer*>(info->user_data);
  HttpServer::WebsocketHelper* helper =
      HttpServer::WebsocketHelper::FindWebsocket(server, connection);
  DCHECK(helper) << "WebsocketData(): failed websocket lookup";
  int result = 0;
  if (helper) {
    result = helper->ReceiveData(static_cast<uint8>(bits), data, data_len);
    if (result == 0) {
      helper->Unregister(server);
      delete helper;
    }
  }
  return result;
}

}  // anonymous namespace

HttpServer::RequestHandler::RequestHandler(const std::string& base_path)
    : base_path_(base_path) {}

HttpServer::RequestHandler::~RequestHandler() {}

HttpServer::HttpServer(int port, int num_threads)
    : port_(port),
      num_threads_(num_threads),
      context_(nullptr),
      embed_local_sourced_files_(false) {
  Resume();
}

void HttpServer::Pause() {
  if (context_) {
    mg_stop(context_);
    context_ = nullptr;
  }
}

void HttpServer::Resume() {
  if (port_) {
    std::stringstream int_to_string;
    int_to_string << port_;
    // Directly passing int_to_string.str() to mongoose does not work on all
    // platforms, but assigning it to a string first does.
    const std::string port_cstr = int_to_string.str();
    int_to_string.str("");
    int_to_string << num_threads_;
    const std::string num_threads_cstr = int_to_string.str();

    mg_callbacks callbacks;
    const char* options[] = { "listening_ports", port_cstr.c_str(),
                              "num_threads", num_threads_cstr.c_str(),
                              nullptr };

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.log_message = LogCallback;
    callbacks.begin_request = BeginRequestCallback;
    callbacks.websocket_connect = WebsocketConnect;
    callbacks.websocket_ready = WebsocketReady;
    callbacks.websocket_data = WebsocketData;
    // Store this server in the user_data field.
    context_ = mg_start(&callbacks, this, options);
  }
}

HttpServer::~HttpServer() {
  if (context_) {
    mg_stop(context_);
    context_ = nullptr;
  }
}

const std::string HttpServer::GetUriData(const std::string& uri) const {
  std::string content_type = mg_get_builtin_mime_type(uri.c_str());

  // Query arguments occur after the first '?'.
  const size_t query_pos = uri.find('?');
  const std::string path = uri.substr(0, query_pos);
  const std::string query_string = query_pos == std::string::npos ?
      "" : uri.substr(query_pos + 1U, std::string::npos);

  // Since paths must be absolute, prepend a '/' if it is missing.
  return GetFileData(base::StartsWith(path, "/") ? path : '/' + path,
                     query_string.c_str(), header_, footer_, GetHandlers(),
                     &content_type, embed_local_sourced_files_);
}

bool HttpServer::IsRunning() const {
  return context_ != nullptr;
}

void HttpServer::RegisterHandler(const RequestHandlerPtr& handler) {
  // Strip any trailing '/' from path, unless the handler is for '/' (the root).
  DCHECK(handler.Get());
  std::string chomped_path = handler->GetBasePath();
  while (chomped_path.length() > 1 && base::RemoveSuffix("/", &chomped_path)) {}

  std::lock_guard<std::mutex> lock(handlers_mutex_);
  handlers_[chomped_path] = handler;
}

void HttpServer::UnregisterHandler(const std::string& path) {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  handlers_.erase(path);
}

HttpServer::HandlerMap HttpServer::GetHandlers() const {
  std::lock_guard<std::mutex> lock(handlers_mutex_);
  return handlers_;
}

void HttpServer::RegisterWebsocket(void* key, WebsocketHelper* helper) {
  std::lock_guard<std::mutex> lock(websocket_mutex_);
  DCHECK(websockets_.find(key) == websockets_.end());
  websockets_[key] = helper;
}

HttpServer::WebsocketHelper* HttpServer::FindWebsocket(void* key) {
  std::lock_guard<std::mutex> lock(websocket_mutex_);
  WebsocketMap::iterator it = websockets_.find(key);
  return (it == websockets_.end())
      ? static_cast<HttpServer::WebsocketHelper*>(nullptr)
      : it->second;
}

void HttpServer::UnregisterWebsocket(void* key) {
  std::lock_guard<std::mutex> lock(websocket_mutex_);
  WebsocketMap::iterator it = websockets_.find(key);
  DCHECK(it != websockets_.end()) << "could not find websocket to unregister";
  if (it != websockets_.end()) {
    websockets_.erase(it);
  }
}

void HttpServer::Websocket::SendData(
    const char* data, size_t data_len, bool is_binary) {
  HttpServer::WebsocketHelper::Opcode opcode = is_binary
      ? HttpServer::WebsocketHelper::BINARY
      : HttpServer::WebsocketHelper::TEXT;
  helper_->SendData(opcode, data, data_len);
}

size_t HttpServer::WebsocketCount() {
  std::lock_guard<std::mutex> lock(websocket_mutex_);
  return websockets_.size();
}

#else

HttpServer::HttpServer(int port, int num_threads)
    : context_(nullptr),
      embed_local_sourced_files_(false) {}

HttpServer::~HttpServer() {}

// Stub all the public functions to prevent DLL errors in production builds.
const std::string HttpServer::GetUriData(const std::string& uri) const {
  return std::string();
}
bool HttpServer::IsRunning() const { return false; }
void HttpServer::Pause() {}
void HttpServer::Resume() {}
void HttpServer::RegisterHandler(const RequestHandlerPtr& handler) {}
void HttpServer::UnregisterHandler(const std::string& path) {}
HttpServer::HandlerMap HttpServer::GetHandlers() const { return HandlerMap(); }
size_t HttpServer::WebsocketCount() { return 0; }

#endif  // !ION_PRODUCTION

}  // namespace remote
}  // namespace ion
