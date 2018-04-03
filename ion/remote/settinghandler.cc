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
#include "ion/base/settingmanager.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"

ION_REGISTER_ASSETS(IonRemoteSettingsRoot);

namespace ion {
namespace remote {

using base::SettingBase;
using base::SettingManager;

namespace {

// Returns a string representation of all settings where the name, type
// descriptor, and value of each setting is URL-encoded. Names and type
// descriptor strings are separated from docstrings and values by a
// (non-URL-encoded) '/' and settings from other settings by a
// (non-URL-encoded) '|'.
static const std::string GetAllSettings() {
  std::string str;

  const SettingManager::SettingMap settings = SettingManager::GetAllSettings();
  for (SettingManager::SettingMap::const_iterator it = settings.begin();
       it != settings.end(); ++it) {
    const std::string type_desc_string =
        it->second->GetTypeDescriptor().empty() ? " " :
        it->second->GetTypeDescriptor();
    const std::string doc_string =
        it->second->GetDocString().empty() ? " " : it->second->GetDocString();
    str += base::UrlEncodeString(it->first) + "/" +
           base::UrlEncodeString(type_desc_string) + "/" +
           base::UrlEncodeString(doc_string) + "/" +
           base::UrlEncodeString(it->second->ToString()) + "|";
  }

  return str;
}

// Attempts to set the value of a setting and returns the string representation
// of the setting if successful or an empty string otherwise. The args must
// contain "name" and "value" entries.
static const std::string SetSettingValue(const HttpServer::QueryMap& args) {
  HttpServer::QueryMap::const_iterator name_it = args.find("name");
  HttpServer::QueryMap::const_iterator value_it = args.find("value");

  std::string response;
  if (name_it != args.end() && value_it != args.end()) {
    SettingBase* setting = SettingManager::GetSetting(name_it->second);
    // If the setting can set its value from the string then return the
    // properly parsed value. If an error occurred then the client will get a
    // 404 error.
    if (setting && setting->FromString(value_it->second))
      response = setting->ToString();
  }

  return response;
}

}  // anonymous namespace

SettingHandler::SettingHandler()
    : HttpServer::RequestHandler("/ion/settings") {
  IonRemoteSettingsRoot::RegisterAssetsOnce();
}

SettingHandler::~SettingHandler() {}

const std::string SettingHandler::HandleRequest(
    const std::string& path_in, const HttpServer::QueryMap& args,
    std::string* content_type) {
  const std::string path = path_in.empty() ? "index.html" : path_in;

  if (path == "get_all_settings") {
    return GetAllSettings();
  } else  if (path == "set_setting_value") {
    return SetSettingValue(args);
  } else {
    const std::string& data = base::ZipAssetManager::GetFileData(
        "ion/settings/" + path);
    if (base::IsInvalidReference(data)) {
      return std::string();
    } else {
      // Ensure the content type is set if the editor HTML is requested.
      if (base::EndsWith(path, "html"))
        *content_type = "text/html";
      return data;
    }
  }
}

}  // namespace remote
}  // namespace ion

#endif
