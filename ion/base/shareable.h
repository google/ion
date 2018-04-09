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

#ifndef ION_BASE_SHAREABLE_H_
#define ION_BASE_SHAREABLE_H_

#include "base/macros.h"
#include "ion/base/logging.h"
#include "ion/port/atomic.h"

#if defined(ION_TRACK_SHAREABLE_REFERENCES)
#include <mutex>  // NOLINT(build/c++11)
#include <string>
#include <unordered_map>
#include "ion/port/stacktrace.h"
#endif

namespace ion {
namespace base {

// Shareable is an abstract base class for any object that can be shared via
// the SharedPtr class. It supports the reference counting interface required
// by SharedPtr.
class Shareable {
 public:
  // GetRefCount() is part of the interface necessary for SharedPtr.
  int GetRefCount() const { return ref_count_; }

#if defined(ION_TRACK_SHAREABLE_REFERENCES)
  // Returns a debug string for all current references to this Shareable. The
  // string contains the address of the Shareable address as well as the address
  // of each current SharedPtr and a stacktrace for the point where the
  // reference was added. An empty string implies either reference tracking is
  // disabled or there are no outstanding references.
  std::string GetReferencesDebugString() const;
#endif

 protected:
#if defined(ION_TRACK_SHAREABLE_REFERENCES)
  // If ION_TRACK_SHAREABLE_REFERENCES is defined via the build configuration,
  // reference tracking is possible on a per-Shareable basis.  When enabled for
  // a particular Shareable, a stack-trace is recorded each time a SharedPtr
  // references the Shareable, and is removed when the reference disappears. At
  // any time, you can query the stack-traces where all of the current
  // references to the Shareable were obtained.
  Shareable() : ref_count_(0), track_references_enabled_(false) {}
  // Adjust setting of reference tracking by a child class, after construction
  // of the Shareable, but before any references have been created.
  void SetTrackReferencesEnabled(bool track_references_enabled);
#else
  Shareable() : ref_count_(0) {}
#endif

  // The destructor is protected because all instances should be managed
  // through SharedPtr.
  virtual ~Shareable() { DCHECK_EQ(ref_count_.load(), 0); }

 private:
  // These are part of the interface necessary for SharedPtr. They are private
  // to prevent anyone messing with the reference count.
  void IncrementRefCount(const void* ref) const {
    ++ref_count_;
#if defined(ION_TRACK_SHAREABLE_REFERENCES)
    if (track_references_enabled_) {
      std::lock_guard<std::mutex> stacktraces_lock(stacktraces_mutex_);
      stacktraces_.insert(std::make_pair(ref, port::StackTrace()));
    }
#endif
  }
  void DecrementRefCount(const void* ref) const {
#if defined(ION_TRACK_SHAREABLE_REFERENCES)
    if (track_references_enabled_) {
      std::lock_guard<std::mutex> stacktraces_lock(stacktraces_mutex_);
      stacktraces_.erase(ref);
    }
#endif
    const int new_count = --ref_count_;
    DCHECK_GE(new_count, 0);
    if (new_count == 0) {
      OnZeroRefCount();
    }
  }

  virtual void OnZeroRefCount() const { delete this; }

  // The reference count is atomic to provide thread safety. It is mutable so
  // that const instances can be managed.
  mutable std::atomic<int> ref_count_;

#if defined(ION_TRACK_SHAREABLE_REFERENCES)
  std::atomic<bool> track_references_enabled_;
  mutable std::mutex stacktraces_mutex_;
  mutable std::unordered_map<const void*, port::StackTrace> stacktraces_;
#endif

  // Allow SharedPtr to modify the reference count.
  template <typename T> friend class SharedPtr;
  friend class WeakReferent;

  DISALLOW_COPY_AND_ASSIGN(Shareable);
};

#if defined(ION_TRACK_SHAREABLE_REFERENCES)
inline std::string Shareable::GetReferencesDebugString() const {
  std::lock_guard<std::mutex> stacktraces_lock(stacktraces_mutex_);
  if (track_references_enabled_ && !stacktraces_.empty()) {
    DCHECK_EQ(static_cast<int>(stacktraces_.size()), GetRefCount());
    std::stringstream ss;
    ss << "Outstanding SharedPtrs for Shareable = "
       << std::hex << this << std::dec << std::endl;
    for (const auto& stacktrace : stacktraces_) {
      ss << "SharedPtr = " << std::hex << stacktrace.first << std::dec
         << " with stacktrace:\n" << stacktrace.second.GetSymbolString();
    }
    return ss.str();
  }
  return std::string();
}

inline void Shareable::SetTrackReferencesEnabled(
    bool track_references_enabled) {
  if (track_references_enabled_ == track_references_enabled)
    return;
  DCHECK_EQ(0, GetRefCount());
  DCHECK_EQ(0U, stacktraces_.size());
  track_references_enabled_ = track_references_enabled;
}
#endif  // ION_TRACK_SHAREABLE_REFERENCES

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SHAREABLE_H_
