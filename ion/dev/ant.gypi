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
  # Note that ../common.gypi should already be in the included context to have
  # paths set correctly.
  'variables': {
    'ant': '/path/to/ant.bin',


    # this in Gyp.  This mess is essentially 'arch_tag_arm' = <(android_abi)
    'arch_tag_arm': 'armeabi-v7a',
    'arch_tag_arm64': 'arm64-v8a',
    'arch_tag_mips': 'mips',
    'arch_tag_mips64': 'mips64',
    'arch_tag_x86': 'x86',
    'arch_tag_x86_64': 'x86_64',

    'conditions': [
      ['flavor in ["arm",""]', {
        'arch_tag': '<(arch_tag_arm)',
        'ant_args': '-Darch=<(arch_tag_arm)',
        # Explicitly set flavor so build.xml doesn't have to fake one.
        'flavor': 'arm',
      }],
      ['flavor == "arm64"', {
        'arch_tag': '<(arch_tag_arm64)',
        'ant_args': '-Darch=<(arch_tag_arm64)',
      }],
      ['flavor == "mips"', {
        'arch_tag': '<(arch_tag_mips)',
        'ant_args': '-Darch=<(arch_tag_mips)',
      }],
      ['flavor == "mips64"', {
        'arch_tag': '<(arch_tag_mips64)',
        'ant_args': '-Darch=<(arch_tag_mips64)',
      }],
      ['flavor == "x86"', {
        'arch_tag': '<(arch_tag_x86)',
        'ant_args': '-Darch=<(arch_tag_x86)',
      }],
      ['flavor == "x86_64"', {
        'arch_tag': '<(arch_tag_x86_64)',
        'ant_args': '-Darch=<(arch_tag_x86_64)',
      }],
     ],
  },
}
