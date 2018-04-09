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

#ifndef ION_REMOTE_SETTINGHANDLER_H_
#define ION_REMOTE_SETTINGHANDLER_H_

#include <string>

#include "ion/remote/httpserver.h"

namespace ion {
namespace remote {

// SettingHandler serves files related to Settings, including an interface for
// viewing and modifying them.
//
// /   or /index.html        - Settings editor interface.
// /get_all_settings         - Gets a string representing all Settings and their
//                             type descriptors and values (see below).
// /set_setting_value?name=name&value=value
//                           - Sets the value of a Setting and returns its
//                             string representation.
//
// The get_all_settings string contains the names, type descriptors, and values
// of all settings. The type descriptor is used to customize the interface for
// a Setting. The handler recognizes the following types of type descriptor
// strings:
//    "bool"                      Displayed as a checkbox.
//    "enum:choice1|choice2|..."  Displayed as a drop-down list.
class ION_API SettingHandler : public HttpServer::RequestHandler {
 public:
  SettingHandler();
  ~SettingHandler() override;

  const std::string HandleRequest(const std::string& path_in,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override;
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_SETTINGHANDLER_H_
