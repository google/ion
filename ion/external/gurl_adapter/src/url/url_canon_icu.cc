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

#include <stdlib.h>
#include <string.h>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "url/url_canon_icu.h"

namespace url {

bool IDNToASCII(const base::char16* src, int src_len,
                url::CanonOutputW* output) {
  DCHECK(false) << "IDNToASCII not implemented";
  return true;
}

}  // namespace url
