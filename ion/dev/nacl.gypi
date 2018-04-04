#
# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
{
  'variables': {
    # Since the paths in make_global_settings must be identical for ALL
    # targets, we can't rely on relative paths. Instead, make the paths to build
    # toolchains absolute.
    'nacl_sdk_root_path': 'third_party/native_client_sdk',
    'nacl_toolchain_path': '<(nacl_sdk_root_path)/toolchain/linux_pnacl',
  },
  'make_global_settings': [
    # The x86_64 toolchain can build both 32 and 64 bit binaries.
    ['CC', '<(nacl_toolchain_path)/bin/x86_64-nacl-clang'],
    ['CXX', '<(nacl_toolchain_path)/bin/x86_64-nacl-clang++'],
    ['LINK', '<(nacl_toolchain_path)/bin/x86_64-nacl-clang++'],
    ['AR', '<(nacl_toolchain_path)/bin/x86_64-nacl-ar'],

    ['CC.host', '<!(which gcc-4.6)'],
    ['CXX.host', '<!(which g++-4.6)'],
    ['LINK.host', '<!(which g++-4.6)'],
    ['AR.host', '<!(which ar)'],
  ],
  'target_defaults': {
    'target_conditions': [
      ['_toolset == "target"', {
        'include_dirs': [
          '<(nacl_sdk_root_path)/include/',
          '<(nacl_sdk_root_path)/include/newlib/',
        ],
      }],
    ],  # target_conditions
  },
}
