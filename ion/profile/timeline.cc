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

#include "ion/profile/timeline.h"

#include "ion/base/logging.h"
#include "ion/profile/timelinenode.h"

// Pre-order traversal with back-tracking. We don't use a stack here to keep the
// iterator light-weight and fast to copy, because some STL algorithms assume
// this.
Timeline::const_iterator Timeline::const_iterator::operator++() {
  if (node_->GetChildren().empty()) {
    // Back-track if current node is a leaf.
    while (true) {
      if (node_ == root_) {
        node_ = nullptr;
        break;
      }
      // Find the node in its parent.
      const TimelineNode* parent = node_->GetParent();
      auto iter = parent->GetChildren().begin();
      while (iter != parent->GetChildren().end()) {
        if ((*iter).get() == node_) break;
        ++iter;
      }
      CHECK(iter != parent->GetChildren().end());
      // Go to next sibling.
      ++iter;
      // Move up if current node was the last child.
      if (iter != parent->GetChildren().end()) {
        node_ = (*iter).get();
        break;
      } else {
        node_ = parent;
      }
    }
  } else {
    // Go to first child if current node is an internal node.
    node_ = node_->GetChildren().front().get();
  }

  return *this;
}
