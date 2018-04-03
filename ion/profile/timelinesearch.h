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

#ifndef ION_PROFILE_TIMELINESEARCH_H_
#define ION_PROFILE_TIMELINESEARCH_H_

#include <string>
#include <thread>  // NOLINT(build/c++11)

#include "ion/profile/timeline.h"
#include "ion/profile/timelineevent.h"
#include "ion/profile/timelinenode.h"
#include "ion/profile/timelinethread.h"

typedef std::function<bool(const TimelineNode*)> Predicate;

// Search all nodes in a timeline that match a predicate. Threads are visited in
// arbitray order (actually the order in which their TraceRecorders were
// created). Nodes under a thread are visited in order of increasing begin
// timestamps.
class TimelineSearch {
 public:
  // Searches nodes by type.
  TimelineSearch(const Timeline& timeline, TimelineNode::Type node_type);
  // Searches nodes by type and name.
  TimelineSearch(const Timeline& timeline, TimelineNode::Type node_type,
                 const std::string& node_name);
  // Searches nodes by type and time range. Only nodes that start and end in
  // the given range are returned.
  TimelineSearch(const Timeline& timeline, TimelineNode::Type node_type,
                 uint32 begin, uint32 end);
  // Searches nodes by type, name and time range. Only nodes that start and end
  // in the given range are returned.
  TimelineSearch(const Timeline& timeline, TimelineNode::Type node_type,
                 const std::string& node_name, uint32 begin, uint32 end);
  // Searches threads by id. Returned nodes are guaranteed to be threads.
  TimelineSearch(const Timeline& timeline, std::thread::id thread_id);
  // Searches by arbitrary predicate.
  TimelineSearch(const Timeline& timeline, const Predicate& predicate);

  class const_iterator {
   public:
    const_iterator(Timeline::const_iterator iter,
                   const TimelineSearch* search_results);
    const TimelineNode* operator*() const { return *iter_; }
    const_iterator operator++();
    bool operator==(const const_iterator& other) const;
    bool operator!=(const const_iterator& other) const;

   private:
    Timeline::const_iterator iter_;
    const TimelineSearch* search_results_;
  };

  const Predicate& predicate() const { return predicate_; }
  const Timeline& timeline() const { return timeline_; }
  bool empty() const { return begin() == end(); }
  const_iterator begin() const;
  const_iterator end() const;

 private:
  const Timeline& timeline_;
  Predicate predicate_;
};

inline TimelineSearch::const_iterator TimelineSearch::const_iterator::
operator++() {
  do {
    ++iter_;
  } while (iter_ != search_results_->timeline().end() &&
           !search_results_->predicate()(*iter_));
  return *this;
}

inline TimelineSearch::const_iterator::const_iterator(
    Timeline::const_iterator iter, const TimelineSearch* search_results)
    : iter_(iter), search_results_(search_results) {}

inline bool TimelineSearch::const_iterator::operator==(
    const const_iterator& other) const {
  return (iter_ == other.iter_) && (search_results_ == other.search_results_);
}

inline bool TimelineSearch::const_iterator::operator!=(
    const const_iterator& other) const {
  return !(*this == other);
}

inline TimelineSearch::TimelineSearch(const Timeline& timeline,
                                      TimelineNode::Type node_type)
    : timeline_(timeline),
      predicate_([node_type](const TimelineNode* node) {
        return node->GetType() == node_type;
      }) {}

inline TimelineSearch::TimelineSearch(const Timeline& timeline,
                                      TimelineNode::Type node_type,
                                      const std::string& node_name)
    : timeline_(timeline),
      predicate_([node_type, node_name](const TimelineNode* node) {
        return node->GetType() == node_type && node->GetName() == node_name;
      }) {}

inline TimelineSearch::TimelineSearch(const Timeline& timeline,
                                      TimelineNode::Type node_type,
                                      uint32 begin, uint32 end)
    : timeline_(timeline),
      predicate_([node_type, begin, end](const TimelineNode* node) {
        return node->GetType() == node_type && node->GetBegin() >= begin &&
               node->GetEnd() <= end;
      }) {}

inline TimelineSearch::TimelineSearch(const Timeline& timeline,
                                      TimelineNode::Type node_type,
                                      const std::string& node_name,
                                      uint32 begin, uint32 end)
    : timeline_(timeline),
      predicate_([node_type, node_name, begin, end](const TimelineNode* node) {
        return node->GetType() == node_type && node->GetName() == node_name &&
               node->GetBegin() >= begin && node->GetEnd() <= end;
      }) {}

inline TimelineSearch::TimelineSearch(const Timeline& timeline,
                                      std::thread::id thread_id)
    : timeline_(timeline), predicate_([thread_id](const TimelineNode* node) {
        if (node->GetType() != TimelineNode::Type::kThread) return false;
        const TimelineThread* thread = static_cast<const TimelineThread*>(node);
        return thread->GetThreadId() == thread_id;
      }) {}

inline TimelineSearch::TimelineSearch(const Timeline& timeline,
                                      const Predicate& predicate)
    : timeline_(timeline), predicate_(predicate) {}

inline TimelineSearch::const_iterator TimelineSearch::begin() const {
  auto iter = timeline_.begin();
  while (iter != timeline_.end() && !predicate_(*iter)) {
    ++iter;
  }
  return const_iterator(iter, this);
}

inline TimelineSearch::const_iterator TimelineSearch::end() const {
  return const_iterator(timeline_.end(), this);
}

#endif  // ION_PROFILE_TIMELINESEARCH_H_
