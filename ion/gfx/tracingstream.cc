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

#include "ion/gfx/tracingstream.h"

namespace ion {
namespace gfx {

#if !ION_PRODUCTION

void TracingStream::SetForwardedStream(std::ostream* forwarded_stream) {
  forwarded_stream_ = forwarded_stream;
}

std::ostream* TracingStream::GetForwardedStream() const {
  return forwarded_stream_;
}

void TracingStream::Clear() { streams_.clear(); }

std::string TracingStream::String(intptr_t visual_id) const {
  auto iter = streams_.find(visual_id);
  return iter == streams_.end() ? "" : iter->second.str();
}

std::string TracingStream::String() const {
  intptr_t visual_id = ion::portgfx::Visual::GetCurrentId();
  return String(visual_id);
}

std::vector<intptr_t> TracingStream::Keys() const {
  std::vector<intptr_t> keys;
  keys.reserve(streams_.size());
  for (const auto& iter : streams_) {
    keys.push_back(iter.first);
  }
  return keys;
}

void TracingStream::StartTracing() { active_ = true; }

void TracingStream::StopTracing() { active_ = false; }

bool TracingStream::IsTracing() const { return active_; }

void TracingStream::EnableLogging(intptr_t visual_id) {
  logging_[visual_id] = true;
}

void TracingStream::DisableLogging(intptr_t visual_id) {
  logging_[visual_id] = false;
}

bool TracingStream::IsLogging() const {
  auto it = logging_.find(portgfx::Visual::GetCurrentId());
  if (it != logging_.end()) return it->second;
  return false;
}

void TracingStream::Append(intptr_t visual_id, const std::string& s) {
  if (active_) {
    if (forwarded_stream_ != nullptr) {
      (*forwarded_stream_) << s;
    }
    if (logging_[0LL]) {
      LOG(INFO) << visual_id << " " << s;
    } else if (logging_[visual_id]) {
      LOG(INFO) << s;
    }
    streams_[visual_id] << s;
  }
}

std::string TracingStream::GetIndent() {
  intptr_t visual_id = ion::portgfx::Visual::GetCurrentId();
  std::string prefix(depths_[visual_id] * 2, ' ');
  return prefix;
}

void TracingStream::EnterScope(intptr_t visual_id, const std::string& marker) {
  if (active_) {
    std::string header =
        std::string(depths_[visual_id] * 2, '-') + ">" + marker + ":";
    streams_[visual_id] << header << "\n";
    if (forwarded_stream_ != nullptr) {
      (*forwarded_stream_) << header << "\n";
    }
    if (logging_[0LL]) {
      LOG(INFO) << visual_id << " " << header;
    } else if (logging_[visual_id]) {
      LOG(INFO) << header;
    }
    depths_[visual_id]++;
  }
}

void TracingStream::ExitScope(intptr_t visual_id) {
  if (active_ && depths_[visual_id] > 0) {
    depths_[visual_id]--;
  }
}

int TracingStream::Depth(intptr_t visual_id) const {
  auto iter = depths_.find(visual_id);
  return iter == depths_.end() ? 0 : iter->second;
}

#endif

}  // namespace gfx
}  // namespace ion
