/**
Copyright 2016 Google Inc. All Rights Reserved.

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

namespace ion {
namespace gfx {

ResourceHolder::ResourceHolder()
    : resources_(*this),
      resource_count_(0),
      fields_(*this),
      label_(kLabelChanged, std::string(), this) {}

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
  for (const auto& group : resources_)
    for (const auto& entry : group)
      entry.second->holder_ = nullptr;

  for (const auto& group : resources_)
    for (const auto& entry : group)
      entry.second->OnDestroyed();
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

  ResourceBase* old_resource = nullptr;
  base::WriteLock write_lock(&lock_);
  base::WriteGuard guard(&write_lock);

  // Increase the size of resources_ if necessary, or reduce its size if
  // possible.
  if (index >= resources_.size()) {
    if (resource) {
      // Only resize the vector if we are actually setting a non-NULL resource.
      resources_.resize(index + 1U, ResourceGroup(*this));
    } else {
      // If resource is null and the index is out of current range,
      // there is nothing to unset, so we can return early.
      return;
    }
  }

  // Look up the previously set value at this index and key, if any.
  auto found = resources_[index].find(key);
  old_resource = (found == resources_[index].end() ? nullptr : found->second);

  // Add or remove the resource.
  if (resource) {
    // We are adding a resource to the holder or replacing an existing one.
    resource->holder_ = this;
    if (old_resource) {
      // Remove the old resource. We can reuse the found iterator to avoid
      // a second hash map lookup.
      old_resource->holder_ = nullptr;
      found->second = resource;
    } else {
      // No resource was previously set here.
      resources_[index].emplace(key, resource);
    }
  } else if (old_resource) {
    // We are removing a resource from the holder.
    old_resource->holder_ = nullptr;
    resources_[index].erase(found);
    if (index + 1 == resources_.size()) {
      // Removing a resource could have caused the
      auto first_nonempty = std::find_if(resources_.rbegin(), resources_.rend(),
          [](const ResourceGroup& group) { return !group.empty(); });
      resources_.resize(std::distance(first_nonempty, resources_.rend()),
                        ResourceGroup(*this));
    }
  }
  // If both resource and old_resource are null, there is nothing to do.

  // Increase the count if we are setting a new resource at this index, and
  // didn't have one there before. Decrease it if we are setting to NULL an
  // index that had a resource.
  if (resource && !old_resource)
    ++resource_count_;
  else if (!resource && old_resource)
    --resource_count_;

  // Sanity check: the group with the highest index must be non-empty.
  DCHECK(resources_.empty() || !resources_.back().empty());
}

ResourceBase* ResourceHolder::GetResource(size_t index, ResourceKey key) const {
  const size_t count = resources_.size();
  if (index >= count) return nullptr;
  auto found = resources_[index].find(key);
  return found == resources_[index].end() ? nullptr : found->second;
}

ResourceHolder::FieldBase::~FieldBase() {}

}  // namespace gfx
}  // namespace ion
