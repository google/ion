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

#ifndef ION_PROFILE_TIMELINETHREAD_H_
#define ION_PROFILE_TIMELINETHREAD_H_

#include <string>
#include <thread>  // NOLINT(build/c++11)

#include "ion/profile/timelinenode.h"

// TimelineThread is a timeline node that stores all events for a thread.
class TimelineThread : public TimelineNode {
 public:
  TimelineThread(const std::string& thread_name, std::thread::id thread_id)
      : TimelineNode(thread_name), thread_id_(thread_id) {}

  Type GetType() const override { return Type::kThread; }
  std::thread::id GetThreadId() const { return thread_id_; }

 private:
  std::thread::id thread_id_;
};

#endif  // ION_PROFILE_TIMELINETHREAD_H_
