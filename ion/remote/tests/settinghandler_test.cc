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

#if !ION_PRODUCTION

#include "ion/remote/settinghandler.h"

#include "ion/base/invalid.h"
#include "ion/base/setting.h"
#include "ion/base/zipassetmanager.h"
#include "ion/remote/tests/httpservertest.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace remote {

using base::Setting;

namespace {

class SettingHandlerTest : public RemoteServerTest {
 protected:
  void SetUp() override {
    RemoteServerTest::SetUp();
    server_->SetHeaderHtml("");
    server_->SetFooterHtml("");
    server_->RegisterHandler(
        HttpServer::RequestHandlerPtr(new SettingHandler()));
  }

  void SetSetting(const std::string& name, const std::string& value) {
    GetUri("/ion/settings/set_setting_value?name=" + name + "&value=" + value);
  }
};

}  // anonymous namespace

TEST_F(SettingHandlerTest, ServeSettings) {
  std::vector<std::string> elements;

  GetUri("/ion/settings/does/not/exist");
  Verify404(__LINE__);

  GetUri("/ion/settings/index.html");
  const std::string& index = base::ZipAssetManager::GetFileData(
      "ion/settings/index.html");
  EXPECT_FALSE(base::IsInvalidReference(index));
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  GetUri("/ion/settings/");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  GetUri("/ion/settings");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  // If there are already settings in ion, we will get a 200.
  // Otherwise we get a 404. Either way is OK.
  GetUri("/ion/settings/get_all_settings");
  EXPECT_TRUE(response_.status == 404 || response_.status == 200);

  // Create two settings.
  Setting<int> int_setting("group1/int", 42, "int");
  Setting<double> double_setting("group1/group2/double", 3.14, "");
  Setting<bool> bool_setting("group3/bool", false, "my bool");
  Setting<int> enum_setting("group3/enum", 2, "my enum");
  enum_setting.SetTypeDescriptor("enum:Left|Center|Right");

  EXPECT_EQ(42, int_setting);
  EXPECT_EQ(3.14, static_cast<double>(double_setting));
  EXPECT_FALSE(bool_setting);
  EXPECT_EQ(2, enum_setting);

  GetUri("/ion/settings/get_all_settings");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(response_.data.find("group1%2fint/%20/int/42|")
              != std::string::npos);
  EXPECT_TRUE(response_.data.find("group1%2fgroup2%2fdouble/%20/%20/3.14")
              != std::string::npos);
  EXPECT_TRUE(response_.data.find("group3%2fbool/bool/my%20bool/false|")
              != std::string::npos);
  EXPECT_TRUE(response_.data.find(
     "group3%2fenum/enum%3aLeft%7cCenter%7cRight/my%20enum/2|")
     != std::string::npos);

  // Set values.
  SetSetting("group1%2fint", "15");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("15", response_.data);
  EXPECT_EQ(15, int_setting);
  EXPECT_EQ(3.14, static_cast<double>(double_setting));
  EXPECT_FALSE(bool_setting);
  EXPECT_EQ(2, enum_setting);

  SetSetting("group1%2fgroup2%2fdouble", "2.818");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("2.818", response_.data);
  EXPECT_EQ(15, int_setting);
  EXPECT_EQ(2.818, static_cast<double>(double_setting));
  EXPECT_FALSE(bool_setting);
  EXPECT_EQ(2, enum_setting);

  SetSetting("group3%2fbool", "true");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("true", response_.data);
  EXPECT_EQ(15, int_setting);
  EXPECT_EQ(2.818, static_cast<double>(double_setting));
  EXPECT_TRUE(bool_setting);
  EXPECT_EQ(2, enum_setting);

  SetSetting("group3%2fenum", "1");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ("1", response_.data);
  EXPECT_EQ(15, int_setting);
  EXPECT_EQ(2.818, static_cast<double>(double_setting));
  EXPECT_TRUE(bool_setting);
  EXPECT_EQ(1, enum_setting);

  // A failed set returns a 404.
  SetSetting("group1%2fint", "abc");
  Verify404(__LINE__);
  SetSetting("group3%2fbool", "TRUE");
  Verify404(__LINE__);
  // Nonexistent setting.
  SetSetting("notasetting", "1");
  Verify404(__LINE__);
}

}  // namespace remote
}  // namespace ion

#endif
