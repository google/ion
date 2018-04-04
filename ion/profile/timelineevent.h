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

#ifndef ION_PROFILE_TIMELINEEVENT_H_
#define ION_PROFILE_TIMELINEEVENT_H_

#include <string>

#include "base/integral_types.h"
#include "ion/profile/timelinenode.h"
#include "third_party/jsoncpp/include/json/value.h"

// TimelineEvent is a node in a Timeline that corresponds to a trace event (or a
// start/end pair of events)
class TimelineEvent : public TimelineNode {
 public:
  TimelineEvent(const std::string& name, const uint32 begin,
                const uint32 duration, const Json::Value& args);

  // Set the argument name and value.
  void SetArgs(const Json::Value& args) { args_ = args; }

  Type GetType() const override { return Type::kEvent; }
  const Json::Value& GetArgs() const { return args_; }
  Json::Value& GetArgs() { return args_; }

 private:
  // Arbitrary metadata as Json value.
  Json::Value args_;
};

#endif  // ION_PROFILE_TIMELINEEVENT_H_
