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

#include "ion/gfx/resourceholder.h"
#include <algorithm>
#include "ion/port/macros.h"

namespace ion {
namespace gfx {

ResourceHolder::ResourceHolder()
    : overflow_resources_(*this),
      resource_count_(0),
      fields_(*this),
      label_(kLabelChanged, std::string(), this) {
  std::fill(resources_.begin(), resources_.end(),
            ResourceGroup(GetAllocator()));
}

ResourceHolder::~ResourceHolder() {
  // We don't need to lock because if anyone else is accessing resources_ there
  // must be a reference to this, in which case the destructor would not have
  // been called.
  //
  // Do the destruction in two stages: first, set holder_ to null in all
  // resources, then call their OnDestroyed() functions. If we don't unset the
  // holder_, the resource will try to remove itself from the holder (which is
  // the correct thing to do when the ResourceManager initiates the
  // destruction). This would cause modifications to resources_ while we iterate
  // over them, leading to crashes.
  IterateOverAllResources(NullHolder);
  IterateOverAllResources(OnDestroyed);
}

void ResourceHolder::SetResource(size_t index, ResourceKey key,
                                 ResourceBase* resource) const {
  if (resource) {
    DCHECK_EQ(resource->GetKey(), key);
    DCHECK(resource->holder_ == nullptr || resource->holder_ == this);
    // Trigger all changes for this new resource.
    const size_t num_fields = fields_.size();
    for (size_t i = 0; i < num_fields; ++i)
      resource->OnChanged(fields_[i]->GetBit());
  }

  base::WriteLock write_lock(&overflow_lock_);
  base::WriteGuard guard(&write_lock, base::kDeferLock);
  if (index >= kInlineResourceGroups) {
    guard.Lock();
  }
  ResourceBase* old_resource = nullptr;
  ResourceGroup* group = GetResourceGroupLocked(index, true);
  DCHECK(group);
  // Look up the previously set value at this index and key, if any.
  ResourceMap::iterator found;
  if (!group->map.empty()) {
    found = group->map.find(key);
    old_resource = (found == group->map.end() ?
                    nullptr : found->second);
  } else if (group && group->cached_resource != nullptr &&
             group->cached_resource_key == key) {
    old_resource = group->cached_resource;
  }

  // Add or remove the resource.
  if (resource) {
    // We are adding a resource to the holder or replacing an existing one.
    resource->holder_ = this;
    if (old_resource) {
      // We are replacing. Remove the old resource.
      old_resource->holder_ = nullptr;
      if (!group->map.empty())
        found->second = resource;
      group->cached_resource = resource;
      group->cached_resource_key = key;
    } else {
      // Here we are adding a new resource.
      if (group->IsEmpty()) {
        // Common case where have a single resource so avoid creating
        // ResourceMap.
        group->cached_resource = resource;
        group->cached_resource_key = key;
      } else if (!group->map.empty()) {
        group->map.emplace(key, resource);
      } else {
        // Start using ResourceMap when we have > 1 resources and add initial
        // resource to map.
        DCHECK(group->cached_resource);
        group->map.emplace(group->cached_resource_key, group->cached_resource);
        group->map.emplace(key, resource);
      }
    }
  } else if (old_resource) {
    // We are removing a resource from the holder.
    old_resource->holder_ = nullptr;
    if (!group->map.empty()) {
      group->map.erase(found);
      if (group->map.size() == 1U) {
        // If we only have 1 resource, store it in cached_resource and delete
        // map.
        group->cached_resource = group->map.begin()->second;
        group->cached_resource_key = group->map.begin()->first;
        group->map.clear();
      }
    } else {
      DCHECK_EQ(old_resource, group->cached_resource);
      group->cached_resource = nullptr;
      group->cached_resource_key = ~0;
    }
    ShrinkResourceGroupsLocked();
  }
  // If both resource and old_resource are null, there is nothing to do.

  // Increase the count if we are setting a new resource at this index, and
  // didn't have one there before. Decrease it if we are setting to NULL an
  // index that had a resource.
  if (resource && !old_resource)
    ++resource_count_;
  else if (!resource && old_resource)
    --resource_count_;
}

void ResourceHolder::IterateOverAllResources(const IterateFunc& func) const {
  for (const ResourceGroup& group : resources_) {
    if (!group.map.empty()) {
      for (const auto& entry : group.map)
        func(entry.second);
    } else if (group.cached_resource) {
      func(group.cached_resource);
    }
  }
  base::ReadLock read_lock(&overflow_lock_);
  base::ReadGuard guard(&read_lock);
  for (const ResourceGroup& group : overflow_resources_) {
    if (!group.map.empty()) {
      for (const auto& entry : group.map)
        func(entry.second);
    } else if (group.cached_resource) {
      func(group.cached_resource);
    }
  }
}

void ResourceHolder::ShrinkResourceGroupsLocked() const {
  if (overflow_resources_.empty()) return;
  size_t overflow_size = overflow_resources_.size();
  while (overflow_size > 0 &&
         overflow_resources_[overflow_size - 1].IsEmpty()) {
    --overflow_size;
  }
  overflow_resources_.resize(overflow_size);
  // Sanity check: the group with the highest index must be non-empty.
  DCHECK(overflow_size == 0 ||
         !overflow_resources_[overflow_size - 1].IsEmpty());
}

ResourceHolder::FieldBase::~FieldBase() {}

}  // namespace gfx
}  // namespace ion
