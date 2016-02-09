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
  // We are done when all entries in resources_ are NULL. It is, however,
  // possible for the vector to resize smaller when OnDestroyed() is called, so
  // don't cache the size of the vector.
  for (size_t i = 0; i < resources_.size(); ++i)
    if (resources_[i])
      resources_[i]->OnDestroyed();
}

void ResourceHolder::SetResource(size_t index, IResource* resource) const {
  if (resource) {
    // Trigger all changes for this new resource.
    const size_t num_fields = fields_.size();
    for (size_t i = 0; i < num_fields; ++i)
      resource->OnChanged(fields_[i]->GetBit());
  }

  IResource* old_resource = NULL;

  // Add or remove the resource.
  base::WriteLock write_lock(&lock_);
  base::WriteGuard guard(&write_lock);
  const size_t count = resources_.size();
  // Increase the size of resources_ if necessary, or reduce its size if
  // possible.
  if (index >= count) {
    // Only resize the vector if we are actually setting a non-NULL resource.
    if (resource) {
      resources_.resize(index + 1U);
      resources_[index] = resource;
    }
  } else if (count) {
    // Replace the old resource.
    old_resource = resources_[index];
    resources_[index] = resource;
    if (!resource) {
      // Attempt to shorten the vector if we are removing a resource. Find the
      // last index that contains a resource.
      size_t last = count - 1U;
      for (; last > index; --last)
        if (resources_[last])
          break;
      resources_.resize(last + 1U);
    }
  }

  // Increase the count if we are setting a new resource at this index, and
  // didn't have one there before. Decrease it if we are setting to NULL an
  // index that didn't have a resource.
  if (resource && !old_resource)
    ++resource_count_;
  else if (!resource && old_resource)
    --resource_count_;
}

ResourceHolder::FieldBase::~FieldBase() {}

}  // namespace gfx
}  // namespace ion
