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

#ifndef ION_GFX_RESOURCEHOLDER_H_
#define ION_GFX_RESOURCEHOLDER_H_

#include <cstring>  // For NULL.
#include <vector>

#include "base/macros.h"
#include "ion/base/invalid.h"
#include "ion/base/lockguards.h"
#include "ion/base/logging.h"
#include "ion/base/notifier.h"
#include "ion/base/readwritelock.h"
#include "ion/base/stlalloc/allocunorderedmap.h"
#include "ion/base/stlalloc/allocvector.h"
#include "ion/gfx/resourcebase.h"
#include "ion/port/atomic.h"

namespace ion {
namespace gfx {

// ResourceHolder is an internal base class for objects that hold resources
// managed by an outside entity, such as ResourceManager, allowing the
// resources to be associated opaquely with an instance of the object. This
// association is done explicitly with a globally unique size_t index.
// Additionally, the outside entity can manage multiple resources for a single
// ResourceHolder by giving them ResourceKeys that are unique within a given
// index. Note that the internal mapping between indices and sets of resources
// is not sparse, so using large indices will needlessly waste memory.
//
// For example, a BufferObject may opaquely contain a derived ResourceBase
// managing a VBO; when the BufferObject instance is destroyed, the VBO can be
// released to OpenGL at some point in the future. Similarly, if the contents
// of the BufferObject are modified, the BufferObject can call its
// ResourceBases' OnChanged() methods so that they can respond.
class ION_API ResourceHolder : public base::Notifier {
 public:
  // All ResourceHolders derived from this should start their own change enums
  // from kNumBaseChanges.
  enum BaseChanges {
    kLabelChanged,
    kResourceChanged,
    kNumBaseChanges
  };

  // Sets the resource at the passed index and key. The size of the internal
  // vector is automatically managed so that it has the smallest possible size.
  void SetResource(size_t index, ResourceKey key, ResourceBase* resource) const;

  // Returns the Resource at the given index and key, or NULL if no resource
  // was previously set at that location.
  ResourceBase* GetResource(size_t index, ResourceKey key) const;

  // Returns the number of resources that this holder holds. Note that this is
  // not necessarily the number of indices that have non-NULL resources. This
  // can be used as a fast trivial check to see if the holder has any resources.
  int GetResourceCount() const {
    return resource_count_;
  }

  // Returns the total amount of GPU memory used by this Holder's resource.
  size_t GetGpuMemoryUsed() const {
    base::ReadLock read_lock(&lock_);
    base::ReadGuard guard(&read_lock);
    size_t total = 0U;
    for (const auto& group : resources_)
      for (const auto& entry : group)
        total += entry.second->GetGpuMemoryUsed();
    return total;
  }

  // Returns/sets the label of this.
  const std::string& GetLabel() const { return label_.Get(); }
  void SetLabel(const std::string& label) { label_.Set(label); }

  // Allow other classes to trigger notifications.
  using Notifier::Notify;

 protected:
  // Base class for Fields (see below).
  class FieldBase {
   public:
    virtual ~FieldBase();

    // Get the change bit.
    int GetBit() const { return change_bit_; }

   protected:
    // The constructor is protected because this is an abstract base class.
    FieldBase(const int change_bit, ResourceHolder* holder)
        : change_bit_(change_bit),
          holder_(holder) {
      if (holder != NULL)
        holder->AddField(this);
    }

    void OnChanged() {
      if (holder_) {
        holder_->OnChanged(change_bit_);
        holder_->Notify();
      }
    }

    // Trigger a change for a specific bit.
    void OnChanged(const int bit) {
      if (holder_) {
        holder_->OnChanged(bit);
        holder_->Notify();
      }
    }

   private:
    int change_bit_;
    const ResourceHolder* holder_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(FieldBase);
  };

  // A generic field that represents some state in the resource.  When the
  // Field value changes, it tells the resource that something has changed.
  template <typename T>
  class Field : public FieldBase {
   public:
    Field(const int change_bit, const T& initial_value, ResourceHolder* holder)
        : FieldBase(change_bit, holder),
          value_(initial_value) {}

    Field(const Field& other)
        : FieldBase(other),
          value_(other.value_) {}

    ~Field() override {}

    // Checks if a proposed value is valid.  Descendant classes can override
    // this to provide validation (e.g. range checking).
    virtual bool IsValid(const T& value) {
      return true;
    }

    // Gets a const version of the current value.
    const T& Get() const { return value_; }

    // Gets an editable version of the current value.  Since the caller could
    // do anything with the reference, also notify the resource.
    T* GetMutable() {
      OnChanged();
      return &value_;
    }

    // Sets the value of the Field if it is valid and tells the resource what
    // has changed. If the value is not valid, an error message is logged.
    // Returns whether the set occurred.
    bool Set(const T& value) {
      if (!IsValid(value)) {
        LOG(ERROR) << "***ION: invalid value passed to Field::Set()";
      } else if (value != value_) {
        value_ = value;
        OnChanged();
        return true;
      }
      return false;
    }

   private:
    T value_;
  };

  // A Field that has a limited valid range of values.
  template <typename T>
  class RangedField : public Field<T> {
   public:
    RangedField(const int change_bit, const T& initial_value,
                const T& min_value, const T& max_value,
                ResourceHolder* holder)
        : Field<T>(change_bit, initial_value, holder),
          min_value_(min_value),
          max_value_(max_value) {
    }

    ~RangedField() override {}

    // Check if the proposed value falls within the correct range.
    bool IsValid(const T& value) override {
      return value >= min_value_ && value <= max_value_;
    }

   private:
    const T min_value_;
    const T max_value_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(RangedField);
  };

  // A Field that holds a vector of up to some number of values. The maximum
  // is necessary for VectorFields that relate to GL state, such as Attributes;
  // different platforms support different numbers of shader attributes, and
  // this allows Ion to gracefully inform users (with an error message) if they
  // try to add too many.
  template <typename T>
  class VectorField : public FieldBase {
   public:
    VectorField(const int change_bit_start, const size_t max_entries,
                ResourceHolder* holder)
        : FieldBase(change_bit_start, holder),
          max_entries_(max_entries),
          entries_(*holder) {
      DCHECK(holder != NULL);
      holder->AddField(this);
    }

    ~VectorField() override {}

    // Adds a value to the vector. If too many Adds have occurred, then an
    // error message is printed instead.
    void Add(const T& value) {
      if (entries_.size() >= max_entries_) {
        LOG(ERROR) << "***ION: Too many entries added to VectorField"
                   << "with " << entries_.size() << " entries";
      } else {
        entries_.push_back(
            Entry(GetBit() + static_cast<int>(entries_.size()), value));
        // Notify the resource that an entry has been added.
        OnChanged(entries_.back().bit);
      }
    }

    // Removes an element from the VectorField, replacing it with the last entry
    // in the VectorField. If there is only one entry in the VectorField then it
    // is cleared. OnChanged() is called on the moved entry, as it may have
    // changed before the call to Remove().
    void Remove(size_t i) {
      if (i < entries_.size()) {
        entries_[i].value = entries_.back().value;
        entries_.resize(entries_.size() - 1U);
        // Call OnChanged() if the removal did not empty the vector.
        if (!entries_.empty())
          OnChanged(entries_[i].bit);
      }
    }

    // Gets a const reference to a value, if the index is valid, otherwise
    // returns an InvalidReference.
    const T& Get(const size_t i) const {
      if (i < entries_.size()) {
        return entries_[i].value;
      } else {
        LogIndexError(i);
        return base::InvalidReference<T>();
      }
    }

    // Gets a non-const pointer to a value and triggers the change.
    T* GetMutable(const size_t i) {
      if (i < entries_.size()) {
        OnChanged(entries_[i].bit);
        return &entries_[i].value;
      } else {
        LogIndexError(i);
        return NULL;
      }
    }

    // If the index i is valid, sets the value at index i and tells the resource
    // what has changed. If the index is not valid, an error message is logged.
    // Returns whether the set occurred.
    bool Set(const size_t i, const T& value) {
      if (i < entries_.size()) {
        if (value != entries_[i].value) {
          entries_[i].value = value;
          OnChanged(entries_[i].bit);
          return true;
        }
      } else {
        LogIndexError(i);
      }
      return false;
    }

    size_t GetCount() const { return entries_.size(); }

   private:
    struct Entry {
      Entry() : bit(-1) {}
      Entry(int bit_in, const T& value_in)
          : bit(bit_in),
            value(value_in) {}
      int bit;
      T value;
    };

    void LogIndexError(const size_t i) const {
      LOG(ERROR) << "***ION: Invalid index " << i << " passed to VectorField "
                 << "with " << entries_.size() << " entries";
    }

    size_t max_entries_;
    base::AllocVector<Entry> entries_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(VectorField);
  };

  // The constructor is protected because this is an abstract base class.
  ResourceHolder();

  // The destructor invokes the resource callback. It is protected because all
  // base::Referent classes must have protected or private destructors.
  ~ResourceHolder() override;

  // Forwards OnChanged to all resources.
  void OnChanged(int bit) const {
    // We use a read lock since Holders should not be modified from multiple
    // threads simultaneously.
    base::ReadLock read_lock(&lock_);
    base::ReadGuard guard(&read_lock);
    for (const auto& group : resources_)
      for (const auto& entry : group)
        entry.second->OnChanged(bit);
  }

 private:
  // Allow FieldBase to call AddField().
  friend class FieldBase;
  // Allow Renderer internals to modify change bits.
  friend class Renderer;

  // Adds a field to the field list.
  void AddField(FieldBase* field) { fields_.push_back(field); }

  typedef base::AllocUnorderedMap<ResourceKey, ResourceBase*> ResourceGroup;

  // The resource vector is declared as mutable because it is really a cache
  // of some external state. This allows SetResource() to be const, meaning
  // that a const ResourceHolder instance can have resources cached in it.
  mutable base::InlinedAllocVector<ResourceGroup, 1> resources_;
  // Protect access to resources_. The lock is mutable so that we can lock in
  // const functions.
  mutable base::ReadWriteLock lock_;
  // Track the number of resources. It is mutable so that we update counts in
  // const functions.
  mutable std::atomic<int> resource_count_;

  // List of fields that the ResourceHolder contains.
  base::AllocVector<FieldBase*> fields_;

  // An identifying name for this holder that can appear in debug streams and
  // printouts of a scene.
  Field<std::string> label_;
};

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_RESOURCEHOLDER_H_
