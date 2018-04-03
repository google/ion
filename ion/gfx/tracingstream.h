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

#ifndef ION_GFX_TRACINGSTREAM_H_
#define ION_GFX_TRACINGSTREAM_H_

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "ion/base/logging.h"
#include "ion/portgfx/glcontext.h"

namespace ion {
namespace gfx {

// Collection of per-context call traces for a particular GraphicsManager.
// Stores an indentation depth and output stream for each active GlContext.
// The stream is initially disabled and can be enabled via "StartTracing".
// For now, this is hardcoded to use stringstreams. In the future we may wish
// to make this into a base class, allowing clients to subclass it to use
// custom streams.  In addition to the per-context string streams, calls
// can be dumped out to the INFO log; this is useful when connected to
// an Android device.
class ION_API TracingStream {
 public:
  // Helper class that ensures that each log entry contains one call.
  class Proxy {
   public:
    Proxy(TracingStream* ts, bool active)
        : tracing_stream_(ts), active_(active) {}
    ~Proxy() {
      if (active_)
        tracing_stream_->Append(
            static_cast<intptr_t>(portgfx::GlContext::GetCurrentId()),
            output_stream_.str());
    }
    Proxy(Proxy&& other)
        : tracing_stream_(other.tracing_stream_),
          active_(other.active_) {
      other.active_ = false;
      // operator<< is used, since calling str(const std::string&) does not
      // seem to have any effect on MSVC.
      output_stream_ << other.output_stream_.str();
    }

    template <typename T>
    const Proxy& operator<<(const T& t) const;

   private:
    friend class TracingStream;

    TracingStream* tracing_stream_;
    mutable std::ostringstream output_stream_;
    bool active_;
  };

  // In addition to the per-context string streams, clients can provide
  // a custom forwarding stream, similar to the Unix 'tee' command.
  // This is especially useful in unit tests.
  void SetForwardedStream(std::ostream* forwarded_stream);

  // Retrieve the current forwarding stream, or null if one isn't attached.
  std::ostream* GetForwardedStream() const;

  // Clear the stream for every context (does not reset the indentation levels).
  void Clear();

  // Fetch the call trace log for a particular context.
  std::string String(intptr_t context_id) const;

  // Fetch the call trace log for the current GL context.
  std::string String() const;

  // Get a list of ids for all contexts that made GL calls during the trace.
  std::vector<intptr_t> Keys() const;

  // Enable tracing from all contexts.
  void StartTracing();

  // Disable tracing from all contexts.
  void StopTracing();

  // Check if tracing is enabled.
  bool IsTracing() const;

  // Send output to the INFO log when tracing, optionally for all GL contexts.
  void EnableLogging(intptr_t context_id = 0LL);

  // Stop sending output to the INFO log, optionally for all GL contexts.
  void DisableLogging(intptr_t context_id = 0LL);

  bool IsLogging() const;

  // Called within Ion to append a string to the stream associated with
  // the given GL context.
  void Append(intptr_t context_id, const std::string& s);

  template <typename T>
  Proxy operator<<(const T& t);

  std::string GetIndent();

  // Called within Ion to increase the indentation level.
  void EnterScope(intptr_t context_id, const std::string& marker);

  // Called within Ion to decrease the indentation level.
  void ExitScope(intptr_t context_id);

  // Get the number of scopes that have been entered but not exited.
  int Depth(intptr_t context_id) const;

#if !ION_PRODUCTION

 private:
  std::ostream* forwarded_stream_ = nullptr;
  // These three maps really do need to be independent.
  std::map<intptr_t, std::ostringstream> streams_;
  std::map<intptr_t, int> depths_;
  std::map<intptr_t, bool> logging_;
  bool active_ = false;

#endif
};

#if !ION_PRODUCTION

template <typename T>
TracingStream::Proxy TracingStream::operator<<(const T& t) {
  Proxy proxy(this, active_);
  proxy.output_stream_ << GetIndent() << t;
  return proxy;
}

template <typename T>
const TracingStream::Proxy& TracingStream::Proxy::operator<<(const T& t) const {
  output_stream_ << t;
  return *this;
}

#else

template <typename T>
TracingStream::Proxy TracingStream::operator<<(const T&) {
  Proxy proxy(this, false);
  return proxy;
}

template <typename T>
const TracingStream::Proxy& TracingStream::Proxy::operator<<(const T&) const {
  return *this;
}

inline void TracingStream::Append(intptr_t context_id, const std::string& s) {}
inline std::string TracingStream::GetIndent() { return ""; }
inline void TracingStream::SetForwardedStream(std::ostream* forwarded_stream) {}
inline std::ostream* TracingStream::GetForwardedStream() const {
  return nullptr;
}
inline void TracingStream::Clear() {}
inline std::string TracingStream::String(intptr_t context_id) const {
  return "";
}
inline std::string TracingStream::String() const { return ""; }
inline std::vector<intptr_t> TracingStream::Keys() const {
  return std::vector<intptr_t>();
}
inline void TracingStream::StartTracing() {}
inline void TracingStream::StopTracing() {}
inline bool TracingStream::IsTracing() const { return false; }
inline void TracingStream::EnableLogging(intptr_t context_id) {}
inline void TracingStream::DisableLogging(intptr_t context_id) {}
inline bool TracingStream::IsLogging() const { return false; }
inline void TracingStream::EnterScope(intptr_t context_id,
                                      const std::string& marker) {}
inline void TracingStream::ExitScope(intptr_t context_id) {}
inline int TracingStream::Depth(intptr_t context_id) const { return 0; }

#endif  // ION_PRODUCTION

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_TRACINGSTREAM_H_
