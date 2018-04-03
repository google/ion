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

#include "ion/base/settingmanager.h"

#include <mutex>  // NOLINT(build/c++11)
#include <set>
#include <vector>

#include "ion/base/allocator.h"
#include "ion/base/logging.h"
#include "ion/base/shareable.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/stringutils.h"

namespace ion {
namespace base {

namespace {

static const char kListenerKey[] = "SettingManager";

}  // anonymous namespace

class SettingManager::SettingData : public Shareable {
 public:
  // Listener function that notifies all listeners for a setting's groups that
  // the setting has changed.
  void SettingListener(SettingBase* setting);

  // The below functions implement SettingManager.
  SettingBase* GetSetting(const std::string& name);
  void RegisterSetting(SettingBase* setting);
  void UnregisterSetting(SettingBase* setting);
  void RegisterGroupListener(const std::string& group,
                             const std::string& key,
                             const SettingBase::Listener& listener);
  void EnableGroupListener(const std::string& group, const std::string& key,
                           bool enable);
  void UnregisterGroupListener(const std::string& group,
                               const std::string& key);

  const SettingMap& GetAllSettings() { return settings_; }

  // The body of UnregisterSetting that must be called while mutex_ is locked.
  // This is to allow unregistration of old settings when a duplicate one is
  // registered.
  void UnregisterSettingLocked(SettingBase* setting);

 private:
  // Maps a setting name to the groups it is contained within.
  typedef std::map<std::string, std::vector<std::string> > GroupMap;

  // Container for all settings that belong to a particular group, and the
  // listeners of that group.
  struct SettingGroupInfo {
    typedef std::map<std::string, SettingBase::ListenerInfo> ListenerMap;
    std::set<SettingBase*> settings;
    ListenerMap listeners;
  };
  // Maps a group name to a set of settings.
  typedef std::map<std::string, SettingGroupInfo> SettingGroupMap;

  typedef base::SharedPtr<SettingData> SettingDataPtr;

  // The destructor is private since this is derived from base::Referent.
  ~SettingData() override {}

  std::mutex mutex_;
  SettingMap settings_;
  GroupMap setting_groups_;
  SettingGroupMap groups_;
};

//-----------------------------------------------------------------------------
//
// SettingManager::SettingData
//
//-----------------------------------------------------------------------------

void SettingManager::SettingData::SettingListener(SettingBase* setting) {
  SettingMap::iterator it = settings_.find(setting->GetName());
  DCHECK(it != settings_.end());
  // Call the listeners of each group the setting is in.
  const std::vector<std::string>& group_names =
      setting_groups_[setting->GetName()];
  const size_t count = group_names.size();
  for (size_t i = 0; i < count; ++i) {
    const SettingGroupInfo& group_info = groups_[group_names[i]];
    for (SettingGroupInfo::ListenerMap::const_iterator it =
             group_info.listeners.begin(); it != group_info.listeners.end();
         ++it)
      if (it->second.enabled)
        it->second.listener(setting);
  }
}

SettingBase* SettingManager::SettingData::GetSetting(const std::string& name) {
  SettingMap::const_iterator it = settings_.find(name);
  return it == settings_.end() ? nullptr : it->second;
}

void SettingManager::SettingData::RegisterSetting(SettingBase* setting) {
  DCHECK(setting);
  const std::string& name = setting->GetName();

  // The mutex is only locked during setting creation and destruction.
  {
    std::lock_guard<std::mutex> lock(mutex_);

    SettingMap::const_iterator it = settings_.find(name);
    if (it != settings_.end()) {
      LOG(WARNING) << "Duplicate setting named '" << name
                   << "' registered in SettingManager";
      // Unregister the old setting.
      UnregisterSettingLocked(setting);
    }
    std::vector<std::string> group_names = SplitString(name, "/");
    // If the setting has more than one group, the last one is actually the
    // name of the setting, so remove it from the set of groups.
    if (group_names.size() > 1)
      group_names.pop_back();

    // Associate this setting with all of its groups.
    const size_t count = group_names.size();
    for (size_t i = 0; i < count; ++i) {
      if (i)
        group_names[i] = group_names[i - 1U] + "/" + group_names[i];
      groups_[group_names[i]].settings.insert(setting);
    }

    // Store the setting's info.
    settings_[name] = setting;
    setting_groups_[name] = group_names;

    // Register a listener so that we know when the setting changes and can
    // notify any group listeners.
    setting->RegisterListener(
        kListenerKey,
        std::bind(&SettingData::SettingListener, this, std::placeholders::_1));

    // Add a ref to this.
    setting->data_ref_ = this;
  }  // unlock mutex
}

void SettingManager::SettingData::UnregisterSetting(SettingBase* setting) {
  DCHECK(setting);

  std::lock_guard<std::mutex> lock(mutex_);
  UnregisterSettingLocked(setting);
}

void SettingManager::SettingData::UnregisterSettingLocked(
    SettingBase* setting) {
  DCHECK(!mutex_.try_lock());
  SettingMap::iterator it = settings_.find(setting->GetName());
  if (it != settings_.end() && it->second == setting) {
    const std::vector<std::string>& group_names =
        setting_groups_[setting->GetName()];

    // Remove the setting from its groups.
    const size_t count = group_names.size();
    for (size_t i = 0; i < count; ++i)
      groups_[group_names[i]].settings.erase(it->second);

    settings_.erase(it);
  }
  setting->UnregisterListener(kListenerKey);
}

void SettingManager::SettingData::RegisterGroupListener(
    const std::string& group,
    const std::string& key,
    const SettingBase::Listener& listener) {
  SettingGroupMap::iterator it = groups_.find(group);
  if (it != groups_.end())
    it->second.listeners[key] = SettingBase::ListenerInfo(listener, true);
}

void SettingManager::SettingData::EnableGroupListener(const std::string& group,
                                                      const std::string& key,
                                                      bool enable) {
  SettingGroupMap::iterator it = groups_.find(group);
  if (it != groups_.end())
    it->second.listeners[key].enabled = enable;
}

void SettingManager::SettingData::UnregisterGroupListener(
    const std::string& group,
    const std::string& key) {
  SettingGroupMap::iterator it = groups_.find(group);
  if (it != groups_.end())
    it->second.listeners.erase(key);
}

//-----------------------------------------------------------------------------
//
// SettingManager
//
//-----------------------------------------------------------------------------

SettingManager::SettingManager() : data_(new SettingData) {
}

SettingManager::~SettingManager() {
}

SettingBase* SettingManager::GetSetting(const std::string& name) {
  return GetInstance()->data_->GetSetting(name);
}

const SettingManager::SettingMap& SettingManager::GetAllSettings() {
  return GetInstance()->data_->GetAllSettings();
}

void SettingManager::RegisterSetting(SettingBase* setting) {
  GetInstance()->data_->RegisterSetting(setting);
}

void SettingManager::UnregisterSetting(SettingBase* setting) {
  // We can't rely on the SettingManager instance still existing at this time.
  // However, we know the SettingBase object has a reference to the SettingData,
  // so just use that instead.
  SettingData* data = static_cast<SettingData*>(setting->data_ref_.Get());
  data->UnregisterSetting(setting);
}

void SettingManager::RegisterGroupListener(
    const std::string& group,
    const std::string& key,
    const SettingBase::Listener& listener) {
  GetInstance()->data_->RegisterGroupListener(group, key, listener);
}

void SettingManager::EnableGroupListener(const std::string& group,
                                         const std::string& key,
                                         bool enable) {
  GetInstance()->data_->EnableGroupListener(group, key, enable);
}

void SettingManager::UnregisterGroupListener(const std::string& group,
                                             const std::string& key) {
  GetInstance()->data_->UnregisterGroupListener(group, key);
}

SettingManager* SettingManager::GetInstance() {
  ION_DECLARE_SAFE_STATIC_POINTER(SettingManager, manager);
  return manager;
}

}  // namespace base
}  // namespace ion
