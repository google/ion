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

#ifndef ION_PROFILE_TIMELINERANGE_H_
#define ION_PROFILE_TIMELINERANGE_H_

#include <string>

#include "base/integral_types.h"
#include "ion/profile/timelineevent.h"
#include "third_party/jsoncpp/include/json/value.h"

// This node type represents a time range event from a WTF trace in a timeline.
class TimelineRange : public TimelineEvent {
 public:
  TimelineRange(const std::string& name, uint32 begin, uint32 duration,
                const Json::Value& args)
      : TimelineEvent(name, begin, duration, args) {}

  Type GetType() const override { return Type::kRange; }
};

#endif  // ION_PROFILE_TIMELINERANGE_H_
