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

#include <algorithm>
#include <sstream>
#include <vector>

#include "ion/base/logging.h"
#include "ion/base/stringutils.h"
#include "third_party/mongoose/mongoose.h"

namespace ion {
namespace remote {

namespace {

// Returns a range request header for the passed range.
static std::string BuildRangeRequestHeader(int64 start, int64 end) {
  std::stringstream headers;
  headers << "Range: bytes=" << start << "-" << end << "\r\n\r\n";
  return headers.str();
}

static std::string BuildUploadHeaders(const std::string& data) {
  std::stringstream headers;
  headers << "Content-Type: text/plain\r\n";
  headers << "Content-Length: " << data.size() << "\r\n\r\n";
  headers << data;
  return headers.str();
}

// Builds a URI from the path and query arguments of a Url.
static std::string BuildUri(const HttpClient::Url& url) {
  typedef std::map<std::string, std::string>::const_iterator iterator;
  std::string uri = url.path;
  if (!url.args.empty()) {
    uri.append("?");
    for (iterator it = url.args.begin(); it != url.args.end(); ++it) {
      if (it != url.args.begin())
        uri.append("&");
      uri.append(it->first);
      uri.append("=");
      uri.append(it->second);
    }
  }
  return uri;
}

static const HttpClient::Response SendRequest(const HttpClient::Url& url,
                                              const char* method,
                                              const std::string& headers) {
  HttpClient::Response response;
  response.url = url;
  if (url.IsValid()) {
    static const int kErrorStringLength = 2048;
    char error[kErrorStringLength];
    std::ostringstream header_str;
    header_str << headers;
    header_str << "Host: " + url.hostname << "\r\n";
    const std::string header_string = header_str.str();

    // Have mongoose connect to the server.
    if (mg_connection* connection = mg_download(url.hostname.c_str(),
                                                url.port,
                                                url.is_https ? 1 : 0,
                                                error,
                                                kErrorStringLength,
                                                "%s %s HTTP/1.1\r\n%s\r\n",
                                                method,
                                                BuildUri(url).c_str(),
                                                header_string.c_str())) {
      // Copy response info into a Response struct.
      mg_request_info* info = mg_get_request_info(connection);
      // Mongoose places the returned status code as a string in the uri.
      if (info->uri)
        response.status = base::StringToInt32(info->uri);
      for (int i = 0; i < info->num_headers; ++i) {
        response.headers[info->http_headers[i].name] =
            info->http_headers[i].value;
      }
      // Read the rest of the server's response.
      static const int kBufferSize = 512;
      char buf[kBufferSize];
      int bytes_read;
      response.data.reserve(kBufferSize);
      // This may block if a download of data is in progress.
      while ((bytes_read = mg_read(connection, buf, sizeof(buf))) > 0) {
        response.data.insert(response.data.end(), buf, &buf[bytes_read]);
      }
      mg_close_connection(connection);
    }
  }
  return response;
}

}  // anonymous namespace

HttpClient::Url::Url() : port(-1), is_https(false) {
}

HttpClient::Url::Url(const std::string& url) : port(-1), is_https(false) {
  Set(url);
}

void HttpClient::Url::Set(const std::string& url) {
  typedef std::string::const_iterator iterator;

  // Assume an HTTP connection on port 80.
  port = 80;
  is_https = false;
  hostname.clear();
  path.clear();
  args.clear();

  if (url.empty())
    return;

  iterator end = url.end();

  // Query arguments occur after the first '?'.
  iterator query_pos = std::find(url.begin(), end, '?');

  // Check if the URL contains a protocol.
  iterator proto_start = url.begin();
  iterator proto_end = std::find(proto_start, end, ':');

  if (proto_end != end) {
    const std::string prot = &*(proto_end);
    if ((prot.length() > 3) && (prot.substr(0, 3) == "://")) {
      const std::string protocol(proto_start, proto_end);
      if (protocol == "https") {
        port = 443;
        is_https = true;
      } else if (protocol != "http") {
        LOG(ERROR) << "Unknown protocol '" << protocol
                   << "', defaulting to http";
      }
      proto_end += 3;  // Skip the "://".
    } else {
      // The URL contains a ":" but not for a protocol.
      proto_end = url.begin();
    }
  } else {
    // The URL does not contain a ":", and does not explicitly state a protocol.
    proto_end = url.begin();
  }

  // The host comes after the protocol specification, and ends with the first
  // '/' or the end of the string.
  iterator host_start = proto_end;
  iterator path_start = std::find(host_start, end, '/');

  // If there is a port, it comes after a ':'.
  iterator hostEnd =
      std::find(proto_end, (path_start != end) ? path_start : query_pos, ':');

  hostname = std::string(host_start, hostEnd);

  // Extract the port.
  if ((hostEnd != end) && ((&*(hostEnd))[0] == ':')) {
    hostEnd++;
    iterator portEnd = (path_start != end) ? path_start : query_pos;
    const std::string port_string(hostEnd, portEnd);
    port = base::StringToInt32(port_string);
  }

  // Extract the path.
  if (path_start != end)
    path = std::string(path_start, query_pos);
  else
    path = "/";

  // Extract any query arguments.
  if (query_pos != end) {
    query_pos++;
    std::vector<std::string> queries =
        base::SplitString(std::string(query_pos, url.end()), "&");
    const size_t num_queries = queries.size();
    for (size_t i = 0; i < num_queries; ++i) {
      const std::vector<std::string> pairs =
          base::SplitString(queries[i], "=");
      // Query args are not required to have an '=' if they have no value.
      DCHECK(pairs.size() == 1 || pairs.size() == 2);
      if (pairs.size() > 1)
        args[pairs[0]] = pairs[1];
      else
        args[pairs[0]] = "";
    }
  }
}

bool HttpClient::Url::IsValid() const {
  return port > 0 && !hostname.empty();
}

HttpClient::Response::Response() : status(-1) {
}

HttpClient::HttpClient() {
}

HttpClient::~HttpClient() {
}

const HttpClient::Response HttpClient::Get(const std::string& url) const {
  return SendRequest(Url(url), "GET", "");
}

const HttpClient::Response HttpClient::GetRange(
    const std::string& url, int64 start, int64 end) const {
  return SendRequest(Url(url), "GET", BuildRangeRequestHeader(start, end));
}

const HttpClient::Response HttpClient::Head(const std::string& url) const {
  return SendRequest(Url(url), "HEAD", "");
}

const HttpClient::Response HttpClient::Post(const std::string& url,
                                            const std::string& data) {
  return SendRequest(Url(url), "POST", BuildUploadHeaders(data));
}

const HttpClient::Response HttpClient::Put(const std::string& url,
                                           const std::string& data) {
  return SendRequest(Url(url), "PUT", BuildUploadHeaders(data));
}

}  // namespace remote
}  // namespace ion
