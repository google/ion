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

#ifndef ION_GFXUTILS_FRAME_H_
#define ION_GFXUTILS_FRAME_H_

#include <functional>
#include <string>

#include "base/integral_types.h"
#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocmap.h"

namespace ion {
namespace gfxutils {

// Frame manages an application-defined frame of execution. It can be used to
// install pre- and post-frame callbacks for tracing, timing, and so on.
class ION_API Frame : public base::Referent {
 public:
  // Callback that can be invoked at the beginning or end of a frame. It is
  // passed a pointer to the Frame instance.
  typedef std::function<void(const Frame& frame)> Callback;

  // The constructor initializes the frame counter to 0.
  Frame();

  // Gets/resets the frame counter. The counter is incremented when End() is
  // called.
  uint64 GetCounter() const { return counter_; }
  void ResetCounter() { counter_ = 0; }

  // Begins a new frame. All pre-frame callbacks are invoked at this point.
  // This does nothing but generate an error message if Begin() was already
  // called with no matching End().
  void Begin();

  // Ends the current frame. All post-frame callbacks are invoked at this point
  // and the frame counter is incremented by 1. This does nothing but generate
  // an error message if Begin() was not called.
  void End();

  // Returns true if Begin() was called and End() was not.
  bool IsInFrame() const { return in_frame_; }

  // Adds a callback to be invoked when Begin() or End() is called. The
  // callback is identified by the passed key.
  void AddPreFrameCallback(const std::string& key, const Callback& callback);
  void AddPostFrameCallback(const std::string& key, const Callback& callback);

  // Removes a callback added previously with AddPreFrameCallback() or
  // AddPostFrameCallback(), identified by the key. These do nothing but return
  // false if the callback was not found.
  bool RemovePreFrameCallback(const std::string& key);
  bool RemovePostFrameCallback(const std::string& key);

 private:
  typedef base::AllocMap<std::string, Callback> CallbackMap;

  // The destructor is private because this is derived from base::Referent.
  ~Frame() override;

  uint64 counter_;
  bool in_frame_;
  CallbackMap pre_frame_callbacks_;
  CallbackMap post_frame_callbacks_;
};

// Convenience typedef for shared pointer to a Frame.
using FramePtr = base::SharedPtr<Frame>;

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_FRAME_H_
