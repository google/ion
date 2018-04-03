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

#ifndef ION_PROFILE_TIMELINENODE_H_
#define ION_PROFILE_TIMELINENODE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/integral_types.h"

// Base class for nodes in a Timeline (see timeline.h).
class TimelineNode {
 public:
  typedef std::vector<std::unique_ptr<TimelineNode>> Children;
  enum class Type : char { kNode, kEvent, kThread, kFrame, kScope, kRange };

  explicit TimelineNode(const std::string& name);
  TimelineNode(const std::string& name, const uint32 begin,
               const uint32 duration);
  virtual ~TimelineNode();

  // Add a node to the children. It becomes the last child.
  void AddChild(std::unique_ptr<TimelineNode> child);
  // Update the duration of the event given a new end timestamp.
  void UpdateDuration(const uint32 end) { duration_ = end - begin_; }

  const std::string& GetName() const { return name_; }
  virtual Type GetType() const { return Type::kNode; }
  uint32 GetBegin() const { return begin_; }
  uint32 GetEnd() const { return begin_ + duration_; }
  uint32 GetDuration() const { return duration_; }
  double GetBeginMs() const { return begin_ * 0.001; }
  double GetEndMs() const { return (begin_ + duration_) * 0.001; }
  double GetDurationMs() const { return duration_ * 0.001; }
  const TimelineNode* GetParent() const { return parent_; }
  TimelineNode* GetParent() { return parent_; }
  const Children& GetChildren() const { return children_; }
  const TimelineNode* GetChild(const size_t i) const {
    return children_[i].get();
  }

 private:
  // Name of the event.
  std::string name_;
  // Time at the beginning of the event.
  uint32 begin_;
  // Duration of the event.
  uint32 duration_;
  // Parent of this event.
  TimelineNode* parent_;
  // Children of this event (e.g. nested scopes)
  Children children_;
};

inline void TimelineNode::AddChild(std::unique_ptr<TimelineNode> child) {
  child->parent_ = this;
  children_.push_back(std::move(child));
}

#endif  // ION_PROFILE_TIMELINENODE_H_
