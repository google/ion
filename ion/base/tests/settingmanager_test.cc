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

#include <memory>

#include "ion/base/logchecker.h"
#include "ion/base/logging.h"
#include "ion/base/staticsafedeclare.h"

#include "absl/memory/memory.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace base {

namespace {

class Listener {
 public:
  Listener() : was_called_(false) {}

  void Callback(SettingBase* setting) {
    was_called_ = true;
  }

  // Returns if the callback was called and resets the called flag.
  bool WasCalled() {
    const bool called = was_called_;
    was_called_ = false;
    return called;
  }

 private:
  bool was_called_;
};

}  // anonymous namespace

TEST(SettingManager, GetRegisterUnregisterSettings) {
  const SettingManager::SettingMap& settings = SettingManager::GetAllSettings();

  EXPECT_EQ(0U, SettingManager::GetAllSettings().size());
  {
    Setting<int> int_setting("int", 12, "");
    EXPECT_EQ(12, int_setting);
    EXPECT_EQ(1U, settings.size());
    EXPECT_EQ(&int_setting, settings.find("int")->second);
    EXPECT_EQ(&int_setting, SettingManager::GetSetting("int"));

    Setting<std::string> string_setting("string", "\"string\"", "");
    EXPECT_EQ("\"string\"", string_setting);

    EXPECT_EQ(2U, settings.size());
    EXPECT_EQ(&int_setting, settings.find("int")->second);
    EXPECT_EQ(&string_setting, settings.find("string")->second);
    EXPECT_EQ(&int_setting, SettingManager::GetSetting("int"));
    EXPECT_EQ(&string_setting, SettingManager::GetSetting("string"));
  }
  EXPECT_EQ(0U, SettingManager::GetAllSettings().size());

  {
    LogChecker log_checker;
    Setting<double> setting1("setting", 1., "");
    EXPECT_EQ(1U, settings.size());
    EXPECT_EQ(&setting1, settings.find("setting")->second);
    EXPECT_EQ(&setting1, SettingManager::GetSetting("setting"));

    EXPECT_FALSE(log_checker.HasAnyMessages());
    Setting<float> setting2("setting", 1.f, "");
    EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                       "Duplicate setting named 'setting'"));
    EXPECT_EQ(1U, settings.size());
    EXPECT_EQ(&setting2, settings.find("setting")->second);
    EXPECT_EQ(&setting2, SettingManager::GetSetting("setting"));
  }

  std::unique_ptr<Setting<std::string> > string_setting;
  {
    string_setting = absl::make_unique<Setting<std::string>>("string setting",
                                                             "\"string\"", "");
    EXPECT_EQ(1U, settings.size());
  }
  // The setting is still there.
  EXPECT_EQ(1U, settings.size());
  EXPECT_TRUE(SettingManager::GetSetting("string setting") != nullptr);
}

TEST(SettingManager, RegisterSameBeforeUnregisterSettings) {
  LogChecker log_checker;
  const SettingManager::SettingMap& settings = SettingManager::GetAllSettings();
  std::unique_ptr<Setting<int>> int_setting(new Setting<int>("int", 12, ""));
  std::unique_ptr<Setting<std::string>> string_setting(
      new Setting<std::string>("string setting", "\"string\"", ""));
  EXPECT_EQ(2U, settings.size());
  EXPECT_TRUE(SettingManager::GetSetting("int") != nullptr);
  EXPECT_TRUE(SettingManager::GetSetting("string setting") != nullptr);
  EXPECT_FALSE(log_checker.HasAnyMessages());
  int_setting = absl::make_unique<Setting<int>>("int", 12, "");
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "Duplicate setting named 'int"));
  string_setting = absl::make_unique<Setting<std::string>>("string setting",
                                                           "\"string\"", "");
  EXPECT_TRUE(
      log_checker.HasMessage("WARNING", "Duplicate setting named 'string"));
  *(int_setting.get()) = 14;
  EXPECT_EQ(2U, settings.size());
  EXPECT_TRUE(SettingManager::GetSetting("int") != nullptr);
  EXPECT_TRUE(SettingManager::GetSetting("string setting") != nullptr);
  EXPECT_FALSE(log_checker.HasAnyMessages());
}

TEST(SettingManager, GroupListeners) {
  Setting<int> setting1("group1/group2/int1", 123, "");
  Listener listener1, listener2, listener3;
  SettingManager::RegisterGroupListener(
      "group1", "listener1",
      std::bind(&Listener::Callback, &listener1, std::placeholders::_1));
  SettingManager::RegisterGroupListener(
      "group1/group2", "listener2",
      std::bind(&Listener::Callback, &listener2, std::placeholders::_1));

  setting1 = 0;
  EXPECT_TRUE(listener1.WasCalled());
  EXPECT_TRUE(listener2.WasCalled());

  // Disable/enable listeners.
  SettingManager::EnableGroupListener("group1/group2", "listener2", false);
  setting1 = 1;
  EXPECT_TRUE(listener1.WasCalled());
  EXPECT_FALSE(listener2.WasCalled());
  SettingManager::EnableGroupListener("group1", "listener1", false);
  setting1 = 2;
  EXPECT_FALSE(listener1.WasCalled());
  EXPECT_FALSE(listener2.WasCalled());
  SettingManager::EnableGroupListener("group1/group2", "listener2", true);
  SettingManager::EnableGroupListener("group1", "listener1", true);
  setting1 = 0;
  EXPECT_TRUE(listener1.WasCalled());
  EXPECT_TRUE(listener2.WasCalled());

  Setting<int> setting2("group2/int2", 456, "");
  SettingManager::RegisterGroupListener(
      "group2", "listener3",
      std::bind(&Listener::Callback, &listener3, std::placeholders::_1));
  setting2 = 0;
  EXPECT_FALSE(listener1.WasCalled());
  EXPECT_FALSE(listener2.WasCalled());
  EXPECT_TRUE(listener3.WasCalled());

  // Remove a listener.
  SettingManager::UnregisterGroupListener("group1", "listener1");
  setting1 = 1;
  EXPECT_FALSE(listener1.WasCalled());
  EXPECT_TRUE(listener2.WasCalled());
  EXPECT_FALSE(listener3.WasCalled());

  // Does nothing.
  SettingManager::UnregisterGroupListener("group1", "listener3");
  setting1 = 2;
  EXPECT_FALSE(listener1.WasCalled());
  EXPECT_TRUE(listener2.WasCalled());
  EXPECT_FALSE(listener3.WasCalled());

  // Listener2 does not listen to gorup1, it listens to group1/group2.
  SettingManager::UnregisterGroupListener("group1", "listener2");
  setting1 = 2;
  EXPECT_FALSE(listener1.WasCalled());
  EXPECT_TRUE(listener2.WasCalled());
  EXPECT_FALSE(listener3.WasCalled());
  SettingManager::UnregisterGroupListener("group1/group2", "listener2");
  setting1 = 2;
  EXPECT_FALSE(listener1.WasCalled());
  EXPECT_FALSE(listener2.WasCalled());
  EXPECT_FALSE(listener3.WasCalled());

  setting2 = 1;
  EXPECT_FALSE(listener1.WasCalled());
  EXPECT_FALSE(listener2.WasCalled());
  EXPECT_TRUE(listener3.WasCalled());

  SettingManager::UnregisterGroupListener("group1", "listener3");
  EXPECT_FALSE(listener1.WasCalled());
  EXPECT_FALSE(listener2.WasCalled());
  EXPECT_FALSE(listener3.WasCalled());
}

TEST(SettingManager, RegisterSameBeforeUnregisterSettingsAndGroupListeners) {
  base::LogChecker log_checker;
  const SettingManager::SettingMap& settings = SettingManager::GetAllSettings();
  std::unique_ptr<Setting<int>> int_setting(
      new Setting<int>("group1/group2/int", 12, ""));
  EXPECT_TRUE(SettingManager::GetSetting("group1/group2/int") != nullptr);
  Listener listener1, listener2;
  SettingManager::RegisterGroupListener(
      "group1", "listener1",
      std::bind(&Listener::Callback, &listener1, std::placeholders::_1));
  SettingManager::RegisterGroupListener(
      "group1/group2", "listener2",
      std::bind(&Listener::Callback, &listener2, std::placeholders::_1));

  *int_setting = 21;
  EXPECT_TRUE(listener1.WasCalled());
  EXPECT_TRUE(listener2.WasCalled());
  int_setting = absl::make_unique<Setting<int>>("group1/group2/int", 12, "");
  *int_setting = 14;
  EXPECT_EQ(1U, settings.size());
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING", "Duplicate setting named 'group1/group2/int"));

  EXPECT_TRUE(listener1.WasCalled());
  EXPECT_TRUE(listener2.WasCalled());
}

}  // namespace base
}  // namespace ion
