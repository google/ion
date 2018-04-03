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
  # Cannot include '../common.gypi' here, because this is a .gypi file.
  # However, it should already be included, so we can use its variables.
  'variables': {
    # Since the paths in make_global_settings must be identical for ALL
    # targets, we can't rely on relative paths. Instead, making the paths to
    # build toolchains absolute.
    'abspath_emscripten': '<!(echo ${PWD}/third_party/emscripten',
  },
  'make_global_settings': [
    ['CC', '<(abspath_emscripten)/emcc'],
    ['CXX', '<(abspath_emscripten)/em++'],
    ['LINK', '<(abspath_emscripten)/emcc'],
    ['AR', '<(abspath_emscripten)/emar'],

    ['CC.host', '<!(which gcc-4.6)'],
    ['CXX.host', '<!(which g++-4.6)'],
    ['LINK.host', '<!(which g++-4.6)'],
    ['AR.host', '<!(which ar)'],
  ],
  'target_defaults': {
    'target_conditions': [
      ['_toolset == "target"', {
        'include_dirs': [
          '<(abspath_emscripten)/system/lib/libcxxabi/include/',
        ],
      }],
    ],  # target_conditions
  },
}
