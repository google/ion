#ifndef EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD
#define EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD

// This code comes from:
// https://github.com/dhbaird/easywsclient
//
// To get the latest version:
// wget https://raw.github.com/dhbaird/easywsclient/master/easywsclient.hpp
// wget https://raw.github.com/dhbaird/easywsclient/master/easywsclient.cpp

#include <stdio.h>

#include <string>

namespace easywsclient {

class WebSocket {
  public:
    // Google change: expose opcode in header file.
    enum Opcode {
        CONTINUATION = 0x0,
        TEXT_FRAME = 0x1,
        BINARY_FRAME = 0x2,
        CLOSE = 0x8,
        PING = 0x9,
        PONG = 0xa,
    };

    typedef WebSocket * pointer;
    typedef enum readyStateValues { CLOSING, CLOSED, CONNECTING, OPEN } readyStateValues;

    // Factories:
    static pointer create_dummy();
    static pointer from_url(const std::string& url, const std::string& origin = std::string());
    static pointer from_url_no_mask(const std::string& url, const std::string& origin = std::string());

    // Interfaces:
    virtual ~WebSocket() { }
    virtual void poll(int timeout = 0) = 0; // timeout in milliseconds
    virtual void send(const std::string& message) = 0;
    virtual void sendPing() = 0;
    virtual void close() = 0;
    virtual readyStateValues getReadyState() const = 0;
    template<class Callable>
    void dispatch(Callable callable) { // N.B. this is compatible with both C++11 lambdas, functors and C function pointers
        struct _Callback : public Callback {
            Callable& callable;
            _Callback(Callable& callable) : callable(callable) { }
            void operator()(const std::string& message) { callable(message); }
        };
        _Callback callback(callable);
        _dispatch(callback);
    }

    // Google change: provide low-level frame-sending.  Not totally comprehensive:
    // for example, it doesn't allow a mask to be specified.  Doesn't do any
    // verification; it's your responsibility to ensure that the frames you send
    // are valid with respect to RFC 6455... for example: don't send a continuation
    // frame without first sending a binary/text frame without the FIN bit set.
    virtual void sendData(Opcode opcode, const std::string& message, bool fin) = 0;

    // Google change: provide a way to reroute all info/error messages. Setting
    // to NULL disables all output.
    static void setMessageStream(FILE *stream) { messageStream = stream; }

  protected:
    struct Callback {
        // Google change: add virtual destructor.
        virtual ~Callback() {}
        virtual void operator()(const std::string& message) = 0;
    };
    static FILE *messageStream;
    virtual void _dispatch(Callback& callable) = 0;
};

} // namespace easywsclient

#endif /* EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD */
