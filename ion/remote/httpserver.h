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

#ifndef ION_REMOTE_HTTPSERVER_H_
#define ION_REMOTE_HTTPSERVER_H_

#include <map>
#include <mutex>  // NOLINT(build/c++11)
#include <string>

#include "ion/base/referent.h"

struct mg_connection;
struct mg_context;

namespace ion {
namespace remote {

class ION_API HttpServer {
 public:
  typedef std::map<std::string, std::string> QueryMap;

  // Opaque helper to avoid exposing implementation details (i.e. Mongoose).
  class WebsocketHelper;
  typedef std::map<void*, WebsocketHelper*> WebsocketMap;

  // Represents the server side of a connected Websocket.  Subclasses override
  // ReceiveData() to customize how to react to incoming messages.  Subclasses
  // of RequestHandler may override ConnectWebsocket() to customize the
  // instantiated Websocket for a specific connection request.
  class Websocket : public base::Referent {
   public:
    Websocket() : helper_(nullptr) {}
    ~Websocket() override {}

    // Override to take some action when the connection is first established.
    virtual void ConnectionReady() {}

    // All subclasses must implement this to respond to incoming messages.
    virtual int ReceiveData(char* data, size_t data_len, bool is_binary) = 0;

   protected:
    // Send a TEXT or BINARY frame with the specified data.
    void SendData(const char* data, size_t data_len, bool is_binary);

   private:
    void SetHelper(WebsocketHelper* helper) { helper_ = helper; }
    friend class WebsocketHelper;
    WebsocketHelper* helper_;
  };
  using WebsocketPtr = base::SharedPtr<Websocket>;

  // RequestHandlers handle requests for a file or path.
  class RequestHandler : public base::Referent {
   public:
    // The HandleRequest() function is passed the path (relative to its base
    // path) of the file or directory to serve, and any query arguments
    // associated with the request. The handler should return an empty string if
    // it cannot handle the request. The handler may optionally set a specific
    // content type to be returned in the response headers. If the handler does
    // not set a content type then a suitable one will be chosen based on the
    // extension of the requested file.
    //
    // Note that if a handler is registered to serve just a single filename then
    // the path passed to it will be "", since that is the relative path from a
    // file to itself.
    virtual const std::string HandleRequest(const std::string& path,
                                            const QueryMap& args,
                                            std::string* content_type) = 0;

    // By default, RequestHandlers don't support websocket connections.
    virtual const WebsocketPtr ConnectWebsocket(const std::string& path,
                                                const QueryMap& args) {
      return WebsocketPtr();
    }

    // Returns the path this handler is registered at.
    const std::string& GetBasePath() const { return base_path_; }

   protected:
    // The constructor is protected since this is an abstract base class. It
    // takes the absolute path that this handler will be registered at. The
    // registration may be for a specific file or for a directory hierarchy. If
    // the handler's path is a path to a file, then handler is invoked when the
    // file is requested. If path is a directory, then HandleRequest() is
    // invoked for any request in or below that directory, unless another
    // handler overrides a file or sub-directory within it.
    //
    // Note that if a handler is registered to return data for a directory then
    // it must correctly handle .htpasswd files by returning either an empty
    // string or a valid .htpasswd file. Failure to do this will likely result
    // in the server returning 401 errors.
    explicit RequestHandler(const std::string& base_path);

    // The destructor is protected since this derived from base::Referent.
    ~RequestHandler() override;

   private:
    const std::string base_path_;
  };
  using RequestHandlerPtr = base::SharedPtr<RequestHandler>;
  typedef std::map<std::string, RequestHandlerPtr> HandlerMap;

  // Starts a HttpServer on the passed port with the passed number of handler
  // threads. Passing a negative port is an error, but passing port 0 will be
  // silently ignored (the server will not be reachable over a network interface
  // but there will be no reported startup errors).
  HttpServer(int port, int num_threads);
  virtual ~HttpServer();

  // Returns the data of the requested URI, or returns an empty string if it
  // does not exist.
  const std::string GetUriData(const std::string& uri) const;

  // Returns whether the server is running.
  bool IsRunning() const;

  // Disables the server and frees up the port, but keeps enough information
  // around to re-initialize it.
  void Pause();

  // Recreates the server and claims the port.
  void Resume();

  // Registers the passed handler at the path returned by
  // handler->GetBasePath().
  void RegisterHandler(const RequestHandlerPtr& handler);
  // Unregisters the handler at |path|.
  void UnregisterHandler(const std::string& path);

  // Returns the handlers registered with this server.
  HandlerMap GetHandlers() const;

  // Gets, sets whether local sourced files (tags with src=... such as img and
  // script that reference files starting with '/') should be embedded in
  // returned HTML pages.
  bool EmbedLocalSourcedFiles() const { return embed_local_sourced_files_; }
  void SetEmbedLocalSourcedFiles(bool embed) {
    embed_local_sourced_files_ = embed;
  }

  // Return the number of currently-connected websockets.
  size_t WebsocketCount();

  // Sets/gets the header and footer HTML, which are both empty by default. When
  // RequestHandlers return HTML pages, the special string <!--HEADER--> is
  // replaced by the header HTML, while <!--FOOTER--> is replaced by the footer
  // HTML.
  const std::string& GetFooterHtml() const { return footer_; }
  const std::string& GetHeaderHtml() const { return header_; }
  void SetFooterHtml(const std::string& str) { footer_ = str; }
  void SetHeaderHtml(const std::string& str) { header_ = str; }

 private:
  // These methods and fields are all concerned with keeping track of active
  // websocket connections.  None of this would be necessary if Mongoose's
  // mg_connection held a per-connection user_data void* that could be used to
  // directly access the websocket handler.
  friend class WebsocketHelper;
  void RegisterWebsocket(void* key, WebsocketHelper* helper);
  void UnregisterWebsocket(void* key);
  WebsocketHelper* FindWebsocket(void* key);
  WebsocketMap websockets_;
  std::mutex websocket_mutex_;

  int port_;
  int num_threads_;

  mg_context* context_;
  // Registered request handlers. Guarded under a mutex so that RequestHandler
  // objects may be added/removed while requests are being serviced on other
  // threads.
  HandlerMap handlers_;
  mutable std::mutex handlers_mutex_;

  // Header and footer HTML.
  std::string header_;
  std::string footer_;

  // Whether files referenced by a src tag should be embedded in returned HTML
  // files.
  bool embed_local_sourced_files_;
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_HTTPSERVER_H_
