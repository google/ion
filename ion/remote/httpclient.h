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

#ifndef ION_REMOTE_HTTPCLIENT_H_
#define ION_REMOTE_HTTPCLIENT_H_

#include <map>
#include <string>

#include "base/integral_types.h"

namespace ion {
namespace remote {

// HttpClient is a very basic class that sends HTTP requests and returns the
// server's response. It is not very intelligent about parsing URLs, especially
// those with complex encodings, but it can handle simple GETs and the like.
class HttpClient {
 public:
  // Simple wrapper around a URL.
  struct Url {
    // Constructs an empty, invalid Url.
    Url();
    // Constructs the Url from the passed value.
    explicit Url(const std::string& url);
    // Sets the url from the passed value.
    void Set(const std::string& url);

    // Returns whether this Url is valid. A Url is considered valid if it
    // contains a valid port and a non-empty hostname.
    bool IsValid() const;

    // The remote host's port.
    int port;
    // Whether the connection is HTTPS or HTTP.
    bool is_https;
    // The remote host's name.
    std::string hostname;
    // The path on the remote host.
    std::string path;
    // Any query arguments in the Url. For example, ?arg1=val1&arg2=val2
    // produces args[arg1] = val1, args[arg2] = val2.
    std::map<std::string, std::string> args;
  };

  struct Response {
    Response();
    // The requested Url that produced the response.
    Url url;
    // The status code received from the remote host (e.g., 200, 404);
    int status;
    // Data received from the last interaction with the remote host.
    std::string data;
    // Headers returned by the remote host. They take the form
    // headers["header name"] = "value", for example
    // headers["Content-Type"] = "text/html"
    std::map<std::string, std::string> headers;
  };

  HttpClient();
  virtual ~HttpClient();

  // Sends a GET request for a URL and returns the remote host's response.
  virtual const Response Get(const std::string& url) const;
  // Sends a GET request for the passed byte range and returns the remote host's
  // response.
  virtual const Response GetRange(const std::string& url, int64 start,
                                  int64 end) const;
  // Sends a HEAD request for a URL and returns the remote host's response.
  virtual const Response Head(const std::string& url) const;
  // POSTs data to URL and returns the remote host's response.
  virtual const Response Post(const std::string& url, const std::string& data);
  // PUTs data to URL and returns the remote host's response.
  virtual const Response Put(const std::string& url, const std::string& data);
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_HTTPCLIENT_H_
