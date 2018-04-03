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

#include "ion/profile/timelinenode.h"

#include <limits>
#include <string>

#include "base/integral_types.h"

TimelineNode::TimelineNode(const std::string& name)
    : TimelineNode(name, 0, std::numeric_limits<uint32>::max()) {}

TimelineNode::TimelineNode(const std::string& name, const uint32 begin,
                           const uint32 duration)
    : name_(name), begin_(begin), duration_(duration), parent_(nullptr) {}

TimelineNode::~TimelineNode() {}
