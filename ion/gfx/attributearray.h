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

#ifndef ION_GFX_ATTRIBUTEARRAY_H_
#define ION_GFX_ATTRIBUTEARRAY_H_

#include "ion/base/referent.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/attribute.h"
#include "ion/gfx/resourceholder.h"

namespace ion {
namespace gfx {

// As of early 2013, the newest graphics hardware supports 32 attribute slots.
static const size_t kAttributeSlotCount = 32;

// An AttributeArray represents a collection of Attributes used to describe the
// vertices of a Shape. For the purposes of an AttributeArray, a "buffer"
// Attribute is an attribute of type kBufferObjectElementAttribute, while all
// other Attributes are "simple."
class ION_API AttributeArray : public ResourceHolder {
 public:
  // Changes that affect the resource.
  enum Changes {
    kAttributeChanged = kNumBaseChanges,
    // The entries between kAttributeChanged and kAttributeEnabledChanged are
    // reserved for determining which attribute has changed.
    kAttributeEnabledChanged = kAttributeChanged + kAttributeSlotCount,
    kNumChanges = kAttributeEnabledChanged + kAttributeSlotCount
  };

  AttributeArray();

  // Returns the index of the first Attribute with the passed name. This index
  // can be passed to ReplaceAttribte(), EnableAttribute(),
  // IsAttributeEnabled(), etc. Returns base::kInvalidIndex if there is no
  // matching Attribute.
  size_t GetAttributeIndexByName(const std::string& name);

  // Adds an Attribute to this AttributeArray. Returns a static index that
  // refers to the Attribute, to be used when enabling or disabling attributes
  // below. Attempting to add an Attribute that already exists in the array is a
  // noop and returns the index of the existing Attribute. Returns
  // base::kInvalidIndex if an attempt is made to add an invalid attribute.
  size_t AddAttribute(const Attribute& attribute);
  // Replaces the attribute at an index with the supplied Attribute if both the
  // index and the attribute are valid. Returns if the replacement was
  // successful.
  bool ReplaceAttribute(size_t index, const Attribute& attribute);

  // Enables or disables attributes. Disabled Attributes are ignored during
  // rendering. Invalid Attributes are considered disabled. Only Attributes of
  // type kBufferObjectElementAttribute can be disabled. The passed index should
  // be the index returned by AddAttribute().
  void EnableAttribute(const size_t attribute_index, bool enabled);
  // Returns if the Attribute at the passed index is enabled, or false if an
  // invalid index is passed.
  bool IsAttributeEnabled(const size_t attribute_index) const;
  // Enables or disables the ith buffer Attribute. Note that i is not
  // necessarily the index returned by AddAttribute().
  void EnableBufferAttribute(const size_t i, bool enabled) {
    enables_.Set(i, enabled);
  }
  // Returns if the ith buffer Attribute is enabled, or false if i is not a
  // valid index. Note that i is not necessarily the index returned by
  // AddAttribute().
  bool IsBufferAttributeEnabled(const size_t i) const {
    // Passing an invalid index here will generate a warning and return a
    // kInvalidReference, which will erroneously evaluate to true.
    const bool& value = enables_.Get(i);
    return base::IsInvalidReference(value) ? false : value;
  }

  // Gets the total number of Attributes in the AttributeArray.
  size_t GetAttributeCount() const {
    return attribute_indices_.size();
  }
  // Gets the number of buffer Attributes in the AttributeArray.
  size_t GetBufferAttributeCount() const {
    return buffer_attributes_.GetCount();
  }
  // Gets the number of simple Attributes in the AttributeArray.
  size_t GetSimpleAttributeCount() const {
    return simple_attributes_.size();
  }

  // Returns the Attribute at the passed index. If attribute_index or i is not a
  // valid index then returns an InvalidReference. Note that attribute_index
  // is the index returned from AddAttribute(), but i is just the ith entry. The
  // value of i may not always refer to the same attribute across calls.
  const Attribute& GetAttribute(const size_t attribute_index) const;
  const Attribute& GetBufferAttribute(const size_t i) const {
    return buffer_attributes_.Get(i);
  }
  const Attribute& GetSimpleAttribute(const size_t i) const {
    if (i < simple_attributes_.size()) {
      return simple_attributes_[i];
    } else {
      return base::InvalidReference<Attribute>();
    }
  }

  // Returns a pointer to the Attribute at the passed index. If attribute_index
  // or i is not a valid index then returns NULL. Note that i is not
  // necessarily the index returned by AddAttribute(), and may not always refer
  // to the same attribute across calls. Triggers the AttributeArray's
  // Resource's OnChanged() method on success.
  Attribute* GetMutableAttribute(const size_t attribute_index);
  Attribute* GetMutableBufferAttribute(const size_t i) {
    return buffer_attributes_.GetMutable(i);
  }
  Attribute* GetMutableSimpleAttribute(const size_t i) {
    if (i < simple_attributes_.size()) {
      return &simple_attributes_[i];
    } else {
      return nullptr;
    }
  }

 protected:
  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~AttributeArray() override;

 private:
  // Internal structure to map an attribute index to an index into the vector
  // that contains the attribute.
  struct Index {
    enum Type {
      kBuffer,
      kSimple
    };
    Index(Type type_in, size_t index_in) : type(type_in), index(index_in) {}

    // For std::find().
    bool operator==(const Index& other) const {
      return type == other.type && index == other.index;
    }
    Type type;
    size_t index;
  };

  // Returns the Index of the Attribute of the passed type and index.
  Index* FindIndexOfAttribute(Index::Type type, size_t index);

  // Removes the attribute at the passed index. This function does _not_ update
  // attribute_indices_, so it should not be used on its own.
  void RemoveAttribute(const Index& attr_index);

  // Called when any BufferObject that this depends on changes.
  void OnNotify(const base::Notifier* notifier) override;

  // Internal map of a global attribute index to the type and local index in
  // the array of buffer or simple attributes.
  base::AllocVector<Index> attribute_indices_;

  // Attribute storage. We use a FieldVector so that each Attribute can
  // trigger its own state change.
  VectorField<Attribute> buffer_attributes_;

  // A VectorField is not needed here since simple attributes always need to be
  // bound.
  base::AllocVector<Attribute> simple_attributes_;

  // Mirror vector of enabled bools. This is separate from the attributes so
  // that changing an enabled state flips its own change bit.
  VectorField<bool> enables_;
};

// Convenience typedef for shared pointer to a AttributeArray.
using AttributeArrayPtr = base::SharedPtr<AttributeArray>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_ATTRIBUTEARRAY_H_
