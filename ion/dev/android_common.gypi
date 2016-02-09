#
# Copyright 2016 Google Inc. All Rights Reserved.
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
  'make_global_settings': [
    ['CC', '<(android_ndk_bin_dir)/<(android_tool_prefix)-gcc'],
    ['CXX', '<(android_ndk_bin_dir)/<(android_tool_prefix)-g++'],
    ['LINK', '<(android_ndk_bin_dir)/<(android_tool_prefix)-g++'],
    ['AR', '<(android_ndk_bin_dir)/<(android_tool_prefix)-ar'],
    ['NM', '<(android_ndk_bin_dir)/<(android_tool_prefix)-nm'],
    ['READELF', '<(android_ndk_bin_dir)/<(android_tool_prefix)-readelf'],

    ['CC.host', '<!(which gcc)'],
    ['CXX.host', '<!(which g++)'],
    ['LINK.host', '<!(which g++)'],
    ['AR.host', '<!(which ar)'],
  ],
  'variables': {
    'variables': {
      'android_toolchain': 'android-ndk-r10c/standalone/4.9',
      'conditions': [
        ['host_os == "mac"', {
          'sdk_host_label': 'darwin',  # Can't use OS because it's "mac".
        }, { # else
          'conditions': [
            ['host_os == "linux"', {
              'sdk_host_label': 'linux',
            }],
          ],  # conditions
        }],
      ],  # conditions
    },

    # Traits of our toolchain.
    'android_ndk_dir': '<!(P=${PWD}; echo $P)/third_party/android_ndk',

    'android_ndk_arch_dir': '<(android_ndk_dir)/files/<(android_toolchain)/<(android_arch)/<(sdk_host_label)',
    'android_ndk_sysroot': '<(android_ndk_arch_dir)/sysroot',
    'android_ndk_bin_dir': '<(android_ndk_arch_dir)/bin',
    'android_strip': '<(android_ndk_bin_dir)/<(android_tool_prefix)-strip',
    'android_sdk_dir': '<!(P=${PWD}; echo $P)/third_party/android_sdk/files/android-sdk-<(host_os)_x86',

    # Traits of our target build.
    'android_common_defines' : [
      'ANDROID',
      'OS_ANDROID',
      'OS_EMBEDDED_ANDROID',
      '__NEW__',
      '__SGI_STL_INTERNAL_PAIR_H',
    ],
    'android_common_cflags': [
      '-fno-exceptions',
      '-Wall',
      '-Wno-unused-local-typedefs', # Boost 1.49, in particular, is too noisy.
      '-mandroid',
      '-fPIC',
      '-ffunction-sections',
      '-fno-short-enums',
      '-fomit-frame-pointer',
      '-fno-strict-aliasing',
      '-finline-limit=64',
      '-fvisibility=hidden',
      '--sysroot=<(android_ndk_sysroot)',
      '-funwind-tables',
    ],
    'android_common_cflags_cc': [
      '-fvisibility-inlines-hidden',
    ],
    'android_common_ldflags' : [
      '--sysroot=<(android_ndk_sysroot)',
      '-Wl,--gc-sections',
      '-Wl,--no-undefined,-z,nocopyreloc',
      '-Wl,-dynamic-linker,/system/bin/linker',
    ],
    'android_common_libraries': [
      '-lstdc++',
      '-lc',
      '-lm',
      '-ldl',
      '-llog',
      '-lgcc',
    ]
  },
}
