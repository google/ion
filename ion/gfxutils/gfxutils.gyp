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
  'includes' : [
    '../common.gypi',
  ],

  'target_defaults': {
    'includes' : [
      '../dev/target_visibility.gypi',
    ],

  },

  'targets': [
    {
      'target_name' : 'iongfxutils',
      'type': 'static_library',
      'sources': [
        'buffertoattributebinder.cc',
        'buffertoattributebinder.h',
        'frame.cc',
        'frame.h',
        'printer.cc',
        'printer.h',
        'resourcecallback.h',
        'shadermanager.cc',
        'shadermanager.h',
        'shadersourcecomposer.cc',
        'shadersourcecomposer.h',
        'shapeutils.cc',
        'shapeutils.h',
      ],
      'dependencies': [
        '<(ion_dir)/port/port.gyp:ionport',
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/external/external.gyp:ionopenctm',
        '../gfx/gfx.gyp:iongfx',
        '../portgfx/portgfx.gyp:ionportgfx',
      ],
    },  # target: iongfxutils

    {
      'target_name': 'iongfxutils_for_tests',
      'type': 'static_library',
      'includes': [
        '../dev/zipasset_generator.gypi',
      ],
      'sources': [
        'tests/data/shapeutils_test.iad',
        'tests/data/zipassetcomposer_test.iad',
      ],
      'dependencies': [
        ':iongfxutils',
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
        '../portgfx/portgfx.gyp:ionportgfx_for_tests',
      ],
    },  # target: iongfxutils_for_tests
  ],
}
