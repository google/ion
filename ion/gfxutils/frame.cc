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

#include "ion/gfxutils/frame.h"

#include <algorithm>

#include "ion/base/logging.h"

namespace ion {
namespace gfxutils {

Frame::Frame()
    : counter_(0),
      in_frame_(false),
      pre_frame_callbacks_(*this),
      post_frame_callbacks_(*this) {}

Frame::~Frame() {}

void Frame::Begin() {
  if (in_frame_) {
    LOG(ERROR) << "Frame::Begin() called while already in a frame.";
  } else {
    // Invoke all pre-frame callbacks.
    for (CallbackMap::const_iterator it = pre_frame_callbacks_.begin();
         it != pre_frame_callbacks_.end(); ++it)
      it->second(*this);
    in_frame_ = true;
  }
}

void Frame::End() {
  if (!in_frame_) {
    LOG(ERROR) << "Frame::End() called while not in a frame.";
  } else {
    // Invoke all post-frame callbacks.
    for (CallbackMap::const_iterator it = post_frame_callbacks_.begin();
         it != post_frame_callbacks_.end(); ++it)
      it->second(*this);
    in_frame_ = false;
    ++counter_;
  }
}

void Frame::AddPreFrameCallback(const std::string& key,
                                const Callback& callback) {
  if (callback)
    pre_frame_callbacks_[key]  = callback;
}

void Frame::AddPostFrameCallback(const std::string& key,
                                 const Callback& callback) {
  if (callback)
    post_frame_callbacks_[key]  = callback;
}

bool Frame::RemovePreFrameCallback(const std::string& key) {
  CallbackMap::iterator it = pre_frame_callbacks_.find(key);
  if (it != pre_frame_callbacks_.end()) {
    pre_frame_callbacks_.erase(it);
    return true;
  }
  return false;
}

bool Frame::RemovePostFrameCallback(const std::string& key) {
  CallbackMap::iterator it = post_frame_callbacks_.find(key);
  if (it != post_frame_callbacks_.end()) {
    post_frame_callbacks_.erase(it);
    return true;
  }
  return false;
}

}  // namespace gfxutils
}  // namespace ion
