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

#include "ion/gfx/attributearray.h"

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfx {

AttributeArray::AttributeArray()
    : attribute_indices_(*this),
      buffer_attributes_(kAttributeChanged, kAttributeSlotCount, this),
      simple_attributes_(*this),
      enables_(kAttributeEnabledChanged, kAttributeSlotCount, this) {
}

AttributeArray::~AttributeArray() {
  const size_t num_attributes = buffer_attributes_.GetCount();
  for (size_t i = 0; i < num_attributes; ++i) {
    const Attribute& a = buffer_attributes_.Get(i);
    BufferObject* bo =
        a.GetValue<BufferObjectElement>().buffer_object.Get();
    if (bo) bo->RemoveReceiver(this);
  }
}

// Returns the index of the attribute with the passed name.
size_t AttributeArray::GetAttributeIndexByName(const std::string& name) {
  size_t index = base::kInvalidIndex;
  const size_t num_attributes = attribute_indices_.size();
  for (size_t i = 0; i < num_attributes; ++i) {
    const Index& attr_index = attribute_indices_[i];
    const size_t idx = attr_index.index;
    const Attribute& a = attribute_indices_[i].type == Index::kBuffer
                             ? buffer_attributes_.Get(idx)
                             : simple_attributes_[idx];
    if (const ShaderInputRegistry::AttributeSpec* spec =
            ShaderInputRegistry::GetSpec(a)) {
      if (spec->name == name) {
        index = i;
        break;
      }
    }
  }
  return index;
}

size_t AttributeArray::AddAttribute(const Attribute& attribute) {
  if (!attribute.IsValid())
    return base::kInvalidIndex;

  // Check that an attribute with the same registry and spec index does not
  // already exist in the array.
  const size_t num_attributes = attribute_indices_.size();
  const size_t registry_id = attribute.GetRegistry().GetId();
  const size_t index = attribute.GetIndexInRegistry();
  for (size_t i = 0; i < num_attributes; ++i) {
    const Index& attr_index = attribute_indices_[i];
    const size_t idx = attr_index.index;
    const Attribute& a = attribute_indices_[i].type == Index::kBuffer ?
        buffer_attributes_.Get(idx) : simple_attributes_[idx];
    if (a.GetRegistry().GetId() == registry_id &&
        a.GetIndexInRegistry() == index) {
      return i;
    }
  }
  // Create an Index and add the Attribute to the correct vector.
  if (attribute.GetType() == kBufferObjectElementAttribute) {
    Index new_index(Index::kBuffer, buffer_attributes_.GetCount());
    attribute_indices_.push_back(new_index);
    buffer_attributes_.Add(attribute);
    if (BufferObject* bo =
            attribute.GetValue<BufferObjectElement>().buffer_object.Get())
      bo->AddReceiver(this);
    // Attributes are enabled by default.
    enables_.Add(true);
  } else {
    Index new_index(Index::kSimple, simple_attributes_.size());
    attribute_indices_.push_back(new_index);
    simple_attributes_.push_back(attribute);
  }
  // Return the index of the Index.
  return num_attributes;
}

bool AttributeArray::ReplaceAttribute(size_t index,
                                      const Attribute& attribute) {
  if (!attribute.IsValid())
    return false;

  const size_t num_attributes = attribute_indices_.size();
  if (index >= num_attributes) {
    return false;
  } else {
    Index& attr_index = attribute_indices_[index];
    if (attr_index.type == Index::kBuffer) {
      if (attribute.GetType() == kBufferObjectElementAttribute) {
        if (BufferObject* new_bo =
                attribute.GetValue<BufferObjectElement>().buffer_object.Get())
          new_bo->AddReceiver(this);
        // Just replace the attribute since it is the same type.
        return buffer_attributes_.Set(attr_index.index, attribute);
      } else {
        // We are replacing a buffer attribute with a simple one.
        RemoveAttribute(attr_index);

        // Add the attribute and update its internal index.
        attr_index.index = simple_attributes_.size();
        attr_index.type = Index::kSimple;
        simple_attributes_.push_back(attribute);
        return true;
      }
    } else {
      if (attribute.GetType() == kBufferObjectElementAttribute) {
        if (BufferObject* new_bo =
                attribute.GetValue<BufferObjectElement>().buffer_object.Get())
          new_bo->AddReceiver(this);

        // We are replacing a simple attribute with a buffer attribute.
        RemoveAttribute(attr_index);

        // Add the attribute and update its internal index.
        attr_index.index = buffer_attributes_.GetCount();
        attr_index.type = Index::kBuffer;
        buffer_attributes_.Add(attribute);
        // The buffer attribute is enabled by default.
        enables_.Add(true);
        return true;
      } else {
        // Only assign if the attributes are different. This keeps the return
        // behavior for both buffer and simple attributes consistent.
        if (simple_attributes_[attr_index.index] == attribute) {
          return false;
        } else {
          simple_attributes_[attr_index.index] = attribute;
          return true;
        }
      }
    }
  }
}

void AttributeArray::EnableAttribute(const size_t attribute_index,
                                     bool enabled) {
  if (attribute_index < attribute_indices_.size()) {
    const Index& attr_index = attribute_indices_[attribute_index];
    // We can only set the enabled state of buffer attributes; simple attributes
    // are always enabled.
    if (attr_index.type == Index::kBuffer)
      enables_.Set(attr_index.index, enabled);
  }
}

bool AttributeArray::IsAttributeEnabled(const size_t attribute_index) const {
  DCHECK_EQ(buffer_attributes_.GetCount(), enables_.GetCount());
  if (attribute_index < attribute_indices_.size()) {
    const Index& attr_index = attribute_indices_[attribute_index];
    return attr_index.type == Index::kSimple ||
        (enables_.Get(attr_index.index) &&
         buffer_attributes_.Get(attr_index.index).IsValid());
  } else {
    return false;
  }
}

const Attribute& AttributeArray::GetAttribute(
    const size_t attribute_index) const {
  if (attribute_index < attribute_indices_.size()) {
    const Index& attr_index = attribute_indices_[attribute_index];
    if (attr_index.type == Index::kBuffer)
      return buffer_attributes_.Get(attr_index.index);
    else
      return simple_attributes_[attr_index.index];
  } else {
    return base::InvalidReference<Attribute>();
  }
}

Attribute* AttributeArray::GetMutableAttribute(const size_t attribute_index) {
  if (attribute_index < attribute_indices_.size()) {
    const Index& attr_index = attribute_indices_[attribute_index];
    if (attr_index.type == Index::kBuffer)
      return GetMutableBufferAttribute(attr_index.index);
    else
      return GetMutableSimpleAttribute(attr_index.index);
  } else {
    return nullptr;
  }
}

AttributeArray::Index* AttributeArray::FindIndexOfAttribute(
    AttributeArray::Index::Type type, size_t index) {
  const Index target(type, index);
  base::AllocVector<Index>::iterator it =
      std::find(attribute_indices_.begin(), attribute_indices_.end(), target);
  return it == attribute_indices_.end() ? nullptr : &(*it);
}

void AttributeArray::RemoveAttribute(const Index& attr_index) {
  if (attr_index.type == Index::kBuffer) {
    DCHECK(buffer_attributes_.GetCount());

    // We will move the current end of the buffer attributes vector to
    // attr_index.index. Update its Index to the index of the attribute at the
    // end of the vector, since we will swap the last attribute with the one
    // we are removing.
    Index* old_index = FindIndexOfAttribute(
        Index::kBuffer, buffer_attributes_.GetCount() - 1U);
    DCHECK(old_index);
    old_index->index = attr_index.index;

    // Remove the old buffer attribute and its enable. This swaps the
    // attribute at the end of the vectors with the one at attr_index.index.
    if (BufferObject* old_bo = buffer_attributes_.Get(attr_index.index)
                                   .GetValue<BufferObjectElement>()
                                   .buffer_object.Get()) {
      // We only want to remove this from the receiver if we don't have any
      // other buffer attributes with the same buffer.
      bool perform_remove = true;
      const size_t num_attributes = buffer_attributes_.GetCount();
      for (size_t i = 0; i < num_attributes; ++i) {
        if (i != attr_index.index) {
          const Attribute& a = buffer_attributes_.Get(i);
          BufferObject* bo =
              a.GetValue<BufferObjectElement>().buffer_object.Get();
          if (bo == old_bo) {
            perform_remove = false;
            break;
          }
        }
      }
      if (perform_remove) old_bo->RemoveReceiver(this);
    }
    buffer_attributes_.Remove(attr_index.index);
    enables_.Remove(attr_index.index);
  } else {
    DCHECK(!simple_attributes_.empty());

    // We will move the current end of the simple attributes vector to
    // attr_index.index. Update its Index to the index of the attribute at the
    // end of the vector, since we will swap the last attribute with the one
    // we are removing.
    Index* old_index = FindIndexOfAttribute(
        Index::kSimple, simple_attributes_.size() - 1U);
    DCHECK(old_index);
    old_index->index = attr_index.index;

    // Remove the old simple attribute. Swap the attribute at the end of
    // the vectors with the one at attr_index.index to replicate the same
    // behavior as VectorField::Remove().
    simple_attributes_[attr_index.index] = simple_attributes_.back();
    simple_attributes_.resize(simple_attributes_.size() - 1U);
  }
}

void AttributeArray::OnNotify(const base::Notifier* notifier) {
  // Only buffer attributes trigger notifications.
  if (GetResourceCount()) {
    const size_t num_attributes = buffer_attributes_.GetCount();
    for (size_t i = 0; i < num_attributes; ++i) {
      const Attribute& a = buffer_attributes_.Get(i);
      DCHECK(a.IsValid());
      BufferObject* bo =
          a.GetValue<BufferObjectElement>().buffer_object.Get();
      if (bo == notifier)
        OnChanged(static_cast<Changes>(kAttributeChanged + i));
    }
  }
}

}  // namespace gfx
}  // namespace ion
