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

#ifndef ION_BASE_SETTING_H_
#define ION_BASE_SETTING_H_

#include <functional>
#include <map>
#include <sstream>
#include <string>

#include "base/macros.h"
#include "ion/base/serialize.h"
#include "ion/base/shareable.h"
#include "ion/base/sharedptr.h"
#include "ion/base/stringutils.h"
#include "ion/port/atomic.h"
#include "ion/port/environment.h"

namespace ion {
namespace base {

// Base class for Setting, which encapsulates the name of the setting and any
// functors that should be called via NotifyListeners(). The name represents
// both the name of the Setting as well as the groups it belongs to. For example
// the Setting with the name "Ion/RenderOptions/CacheUniforms" belongs to the
// "Ion" and "RenderOptions" groups. See SettingManager for how to listen for
// changes to groups.
class ION_API SettingBase {
 public:
  // A function that is called when the value changes.
  typedef std::function<void(SettingBase* setting)> Listener;
  struct ListenerInfo {
    ListenerInfo() : enabled(false) {}
    ListenerInfo(const Listener& listener_in, bool enabled_in)
        : listener(listener_in), enabled(enabled_in) {}
    Listener listener;
    bool enabled;
  };

  // Returns the name associated with this.
  const std::string& GetName() const { return name_; }

  // Returns the documentation string associated with this.
  const std::string& GetDocString() const { return doc_string_; }

  // Sets/returns a string containing information about the Setting's type.
  // This string can be used, for example, in an interactive program to present
  // a specialized interface for displaying or modifying the settings. See the
  // remote::SettingHandler documentation for examples.
  void SetTypeDescriptor(const std::string& desc) { type_descriptor_ = desc; }
  const std::string& GetTypeDescriptor() const { return type_descriptor_; }

  // Adds a function that will be called when this setting's value changes. The
  // function is identified by the passed key. The same key must be used to
  // remove the listener.
  void RegisterListener(const std::string& key, const Listener& listener);
  // Enables or disables the listener identified by key, if one exists.
  void EnableListener(const std::string& key, bool enable);
  // Removes the listener identified by key, if one exists.
  void UnregisterListener(const std::string& key);

  // Notify listeners that this setting has changed.
  void NotifyListeners();

  // Returns a string version of this setting. The same string may be passed to
  // FromString to reconstruct an identical setting.
  virtual const std::string ToString() const = 0;
  // Parses the passed string to set the value of this and returns whether the
  // parsing was successful. If the parsing does not succeed then nothing should
  // change in this.
  virtual bool FromString(const std::string& str) = 0;

 protected:
  // The constructor and destructor are protected because this is an abstract
  // base class.
  SettingBase(const std::string& name, const std::string& doc_string);
  virtual ~SettingBase();

 private:
  typedef std::map<std::string, ListenerInfo> ListenerMap;

  std::string name_;
  std::string doc_string_;
  std::string type_descriptor_;
  ListenerMap listeners_;

  // Holds on to a reference to SettingData. We don't need to access it
  // directly through this pointer, just keep it alive.
  friend class SettingManager;
  SharedPtr<Shareable> data_ref_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SettingBase);
};

// A SettingGroup is a convenience class to hold Settings that are in the same
// hierarchical group. It can be passed to the constructor of a Setting (see
// below) to place that setting in the group. Since SettingGroup is a simple
// wrapper, it does not have to have the same lifetime as the Settings in it.
// See SettingBase (above) for a discussion of groups.
class ION_API SettingGroup {
 public:
  explicit SettingGroup(const std::string& name) : group_(name) {
    while (base::EndsWith(group_, "/"))
      base::RemoveSuffix("/", &group_);
  }
  SettingGroup(const SettingGroup& parent_group, std::string name) {
    while (base::StartsWith(name, "/"))
      base::RemovePrefix("/", &name);
    while (base::EndsWith(name, "/"))
      base::RemoveSuffix("/", &name);
    group_ = parent_group.GetGroupName() + "/" + name;
  }
  ~SettingGroup() {}

  // Returns the name of the group that this wraps.
  const std::string GetGroupName() const { return group_; }

 private:
  std::string group_;
};

// Forward references.
template <typename T> class Setting;
template <typename SettingType>
void SetTypeDescriptorForType(SettingType* setting);

// A Setting holds a value of type T, and supports listeners that are notified
// when the setting's value changes. The only restrictions on T are that it must
// have a default constructor and support the insertion and extraction operators
// (<< and >>). POD types, strings, and most STL containers are supported
// through Ion's StringToValue and ValueToString serialization routines.
//
// Every setting creates an entry in the global SettingManager, from which they
// can be retrieved and modified. This is particularly useful for editing them
// at runtime, for example through a configuration file, scripting interface, or
// remote tweak interface. A setting might, for example, control what rendering
// algorithm to use based on the local CPU or GPU speed, or selectively enable
// or disable certain features for testing or localization.
template <typename T>
class Setting : public SettingBase {
 public:
  // Creates a new setting with the passed name, initial value, and
  // documentation string.
  Setting(const std::string& name, const T& value,
          const std::string& doc_string)
      : SettingBase(name, doc_string), value_(value) {
    SetTypeDescriptorForType(this);
  }

  // Same as above, but places the setting in the passed group.
  Setting(const SettingGroup* group, const std::string& name, const T& value,
          const std::string& doc_string)
      : SettingBase(group->GetGroupName() + '/' + name, doc_string),
        value_(value) {
    SetTypeDescriptorForType(this);
  }

  // Convenience constructor that does not require a documentation string.
  Setting(const std::string& name, const T& value)
      : SettingBase(name, std::string()), value_(value) {
    SetTypeDescriptorForType(this);
  }

  // Same as above, but places the setting in the passed group.
  Setting(const SettingGroup* group, const std::string& name, const T& value)
      : SettingBase(group->GetGroupName()  + '/' + name, std::string()),
        value_(value) {
    SetTypeDescriptorForType(this);
  }

  ~Setting() override {}

  const std::string ToString() const override { return ValueToString(value_); }

  bool FromString(const std::string& str) override {
    T value = T();
    std::istringstream in(str);
    if (StringToValue(in, &value)) {
      SetValue(value);
      return true;
    }
    return false;
  }

  // Direct value mutators.
  T* GetMutableValue() { return &value_; }
  const T& GetValue() const { return value_; }
  void SetValue(const T& value) {
    value_ = value;
    NotifyListeners();
  }
  operator T() const { return value_; }
  void operator=(const T& value) {
    value_ = value;
    NotifyListeners();
  }

  // Equality testers.
  bool operator==(const T& value) const { return value_ == value; }
  friend bool operator==(const T& value, const Setting<T>& setting) {
    return setting.value_ == value;
  }

 private:
  T value_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Setting<T>);
};

// Specialize for std::atomic types.
template <typename T>
class Setting<std::atomic<T> > : public SettingBase {
 public:
  // Creates a new setting with the passed name, initial value, and
  // documentation string.
  Setting(const std::string& name, const T& value,
          const std::string& doc_string)
      : SettingBase(name, doc_string), value_(value) {
    SetTypeDescriptorForType(this);
  }

  // Same as above, but places the setting in the passed group.
  Setting(const SettingGroup* group, const std::string& name, const T& value,
          const std::string& doc_string)
      : SettingBase(group->GetGroupName() + '/' + name, doc_string),
        value_(value) {
    SetTypeDescriptorForType(this);
  }

  // Convenience constructor that does not require a documentation string.
  Setting(const std::string& name, const T& value)
      : SettingBase(name, std::string()), value_(value) {
    SetTypeDescriptorForType(this);
  }

  // Same as above, but places the setting in the passed group.
  Setting(const SettingGroup* group, const std::string& name, const T& value)
      : SettingBase(group->GetGroupName()  + '/' + name, std::string()),
        value_(value) {
    SetTypeDescriptorForType(this);
  }

  ~Setting() override {}

  const std::string ToString() const override {
    return ValueToString(value_.load());
  }

  bool FromString(const std::string& str) override {
    T value = T();
    std::istringstream in(str);
    if (StringToValue(in, &value)) {
      SetValue(value);
      return true;
    }
    return false;
  }

  // Direct value mutators.
  std::atomic<T>* GetMutableValue() { return &value_; }
  const std::atomic<T>& GetValue() const { return value_; }
  void SetValue(const T& value) {
    value_ = value;
    NotifyListeners();
  }
  operator T() const { return value_.load(); }
  void operator=(const T& value) {
    value_ = value;
    NotifyListeners();
  }

  // Equality testers.
  bool operator==(const T& value) const { return value_ == value; }
  friend bool operator==(const T& value,
                         const Setting<std::atomic<T> >& setting) {
    return setting.value_ == value;
  }

 private:
  std::atomic<T> value_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Setting<std::atomic<T> >);
};

// An EnvironmentSetting is a Setting can take its initial value from the named
// system environment variable passed to its constructor. Note that the value of
// the EnvironmentSetting does not change if the environment variable changes
// after construction.
template <typename T>
class EnvironmentSetting : public Setting<T> {
 public:
  // Creates a new setting with the passed setting_name and attempts to set its
  // initial value from the environment variable env_var_name. If the variable
  // does not exist or cannot be converted to type T, then the setting takes on
  // the passed default value.
  EnvironmentSetting(const std::string& setting_name,
                     const std::string& env_var_name,
                     const T& default_value,
                     const std::string& doc_string)
     : Setting<T>(setting_name, default_value, doc_string) {
    const std::string env_value =
        port::GetEnvironmentVariableValue(env_var_name);
    this->FromString(env_value);
  }

  // Equality testers.
  bool operator==(const T& value) const {
    return Setting<T>::operator==(value);
  }
  friend bool operator==(const T& value, const EnvironmentSetting<T>& setting) {
    return setting == value;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(EnvironmentSetting<T>);
};

// Sets a Setting<T> to a new value.  The original value will be
// restored when the ScopedSettingValue is deleted.  The Setting<T>
// must outlive the ScopedSettingValue object.
template <typename T>
class ScopedSettingValue {
 public:
  // Save the original setting value, and change to the new value.
  ScopedSettingValue(Setting<T>* setting, const T& value)
    : setting_(setting) {
    if (setting_) {
      original_value_ = setting_->GetValue();
      setting_->SetValue(value);
    }
  }

  // Restore the original value to the Setting<T>.
  ~ScopedSettingValue() {
    if (setting_) {
      setting_->SetValue(original_value_);
    }
  }

 private:
  // Setting whose value is pushed/popped.
  Setting<T>* setting_;

  // This variable maintains the original value of the Setting<T>.
  // The Setting's value is restored from original_value_ when the
  // ScopedSettingValue is destroyed.
  T original_value_;
};

// Sets the type descriptor string of a setting based on its type. The
// unspecialized version of this function leaves the string untouched (empty).
template <typename SettingType>
inline void SetTypeDescriptorForType(SettingType* setting) {
}

// Boolean settings set the descriptor to "bool".
template <>
inline void SetTypeDescriptorForType(Setting<bool>* setting) {
  setting->SetTypeDescriptor("bool");
}

template <>
inline void SetTypeDescriptorForType(Setting<std::atomic<bool> >* setting) {
  setting->SetTypeDescriptor("bool");
}

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SETTING_H_
