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

#include "ion/base/setting.h"

#include "ion/base/settingmanager.h"

namespace ion {
namespace base {

SettingBase::SettingBase(const std::string& name, const std::string& doc_string)
    : name_(name), doc_string_(doc_string) {
  // Register the setting with the manager.
  SettingManager::RegisterSetting(this);
}

SettingBase::~SettingBase() {
  SettingManager::UnregisterSetting(this);
}

void SettingBase::RegisterListener(const std::string& key,
                                   const Listener& listener) {
  listeners_[key] = ListenerInfo(listener, true);
}

void SettingBase::EnableListener(const std::string& key, bool enable) {
  ListenerMap::iterator it = listeners_.find(key);
  if (it != listeners_.end())
    it->second.enabled = enable;
}

void SettingBase::UnregisterListener(const std::string& key) {
  listeners_.erase(key);
}

void SettingBase::NotifyListeners() {
  for (ListenerMap::const_iterator it = listeners_.begin();
       it != listeners_.end(); ++it) {
    if (it->second.enabled)
      it->second.listener(this);
  }
}

}  // namespace base
}  // namespace ion
