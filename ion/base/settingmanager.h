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

#ifndef ION_BASE_SETTINGMANAGER_H_
#define ION_BASE_SETTINGMANAGER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "ion/base/setting.h"
#include "ion/base/sharedptr.h"

namespace ion {
namespace base {

// SettingManager tracks all existing SettingBase instances, and allows callers
// to obtain a map of all settings, get a specific setting or listen for when
// any setting in a group changes. See SettingBase for a discussion of groups.
class ION_API SettingManager {
 public:
  // Maps a setting's name to setting itself and its groups.
  typedef std::map<std::string, SettingBase*> SettingMap;

  ~SettingManager();

  // Returns the setting with the passed name.
  static SettingBase* GetSetting(const std::string& name);
  // Returns all settings keyed by their names.
  static const SettingMap& GetAllSettings();

  // Adds the setting to the manager and its groups.
  static void RegisterSetting(SettingBase* setting);

  // Removes the setting from the manager and its groups.
  static void UnregisterSetting(SettingBase* setting);

  // Adds a function that will be called when any setting in the passed group
  // changes. The listening function is identified by the passed key, which must
  // be passed to UnregisterListener() to remove the listener. The particular
  // setting that changed will be passed to the listener (see
  // SettingBase::Listener).
  static void RegisterGroupListener(const std::string& group,
                                    const std::string& key,
                                    const SettingBase::Listener& listener);

  // Enables or disables the group listener identified by key.
  static void EnableGroupListener(const std::string& group,
                                  const std::string& key,
                                  bool enable);

  // Removes the group listener identified by key.
  static void UnregisterGroupListener(const std::string& group,
                                      const std::string& key);

 private:
  class SettingData;
  SharedPtr<SettingData> data_;

  // The constructor is private since this is a singleton class.
  SettingManager();

  // Returns the singleton instance.
  static SettingManager* GetInstance();

  DISALLOW_COPY_AND_ASSIGN(SettingManager);
};

}  // namespace base
}  // namespace ion

#endif  // ION_BASE_SETTINGMANAGER_H_
