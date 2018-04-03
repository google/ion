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

#ifndef ION_BASE_ZIPASSETMANAGERMACROS_H_
#define ION_BASE_ZIPASSETMANAGERMACROS_H_

// Use the ION_REGISTER_ASSETS macro with the namespace from which to register
// assets. For example:
//   ION_REGISTER_ASSETS(ZipAssetTest);
// will expand to:
// namespace ZipAssetTest { bool RegisterAssets(); bool RegisterAssetsOnce(); }
// The namespace name is the name of the IAD (not the file name, rather the
// defined name inside of the IAD file).
#define ION_REGISTER_ASSETS(n) \
  namespace n {                 \
  bool RegisterAssets();        \
  void RegisterAssetsOnce();        \
  }

#endif  // ION_BASE_ZIPASSETMANAGERMACROS_H_
