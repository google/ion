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

#ifndef ION_PROFILE_TIMELINE_H_
#define ION_PROFILE_TIMELINE_H_

#include <memory>

#include "base/macros.h"
#include "ion/base/logging.h"
#include "ion/profile/timelineevent.h"
#include "ion/profile/timelinenode.h"

// A hierarchical representation of tracing data.
//
// Example usage:
//   Timeline timeline = ion::profile::GetCallTraceManager()->GetTimeline();
//
//   // Iterate over all nodes (skips the root).
//   for (auto node : timeline) {
//     // Do something with the node
//     std::cout << node->GetName() << " "
//               << node->GetDuration() << std::endl;
//   }
//
//   // Search for events named "Foo" and iterate over them.
//   TimelineSearch search(timeline, "Foo");
//   for (auto event : search) {
//     // Do something with the event
//     std::cout << event->GetName() << " "
//               << event->GetDuration() << std::endl;
//   }
class Timeline {
 public:
  Timeline() : root_(new TimelineNode("root")) {}
  explicit Timeline(std::unique_ptr<TimelineNode> root)
      : root_(std::move(root)) {}
  Timeline(Timeline&& other) : root_(std::move(other.root_)) {}

  // Traverses the hierarchy in pre-order. Events are visited in increasing
  // begin-timestamp order.
  class const_iterator {
   public:
    const_iterator(const TimelineNode* node, const TimelineNode* root)
        : node_(node), root_(root) {}
    const TimelineNode* operator*() const { return node_; }
    const_iterator operator++();
    bool operator==(const const_iterator& other) const {
      return node_ == other.node_;
    }
    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

   private:
    const TimelineNode* node_;
    const TimelineNode* root_;
  };

  // Returns a const iterator over the timeline. The root node is skipped.
  const_iterator begin() const  {
    return ++const_iterator(root_.get(), root_.get());
  }
  // Returns a const iterator to the end of the timeline.
  const_iterator end() const { return const_iterator(nullptr, root_.get()); }

  // Returns a pointer to the root node. The root node is not an event and is
  // skipped by the iterator.
  const TimelineNode* GetRoot() const { return root_.get(); }

 private:
  std::unique_ptr<TimelineNode> root_;
  DISALLOW_COPY_AND_ASSIGN(Timeline);
};

#endif  // ION_PROFILE_TIMELINE_H_
