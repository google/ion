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

  'targets' : [
    {
      'target_name' : 'ionimage',
      'type': 'static_library',
      'sources' : [
        'conversionutils.cc',
        'conversionutils.h',
        'ninepatch.cc',
        'ninepatch.h',
        'renderutils.cc',
        'renderutils.h',
      ],
      'dependencies': [
        '../external/imagecompression.gyp:ionimagecompression',
        '../gfx/gfx.gyp:iongfx',
        '../gfxutils/gfxutils.gyp:iongfxutils',
        '../portgfx/portgfx.gyp:ionportgfx',
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/external/external.gyp:ionlodepnglib',
        '<(ion_dir)/external/external.gyp:ionstblib',
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },

    {
      'target_name' : 'ionimagejpeg',
      'type': 'static_library',
      'sources' : [
        'exportjpeg.cc',
        'exportjpeg.h',
      ],
      'dependencies': [
        '../gfx/gfx.gyp:iongfx',
        '<(ion_dir)/external/external.gyp:ionjpeg',
      ],
    },

    {
      'target_name': 'ionimage_for_tests',
      'type': 'static_library',
      'sources': [
        # A .cc file is necessary to actually create a .a.
        '../external/empty.cc',
        'tests/image_bytes.h',
      ],
      'dependencies': [
        ':ionimage',
        '../gfx/gfx.gyp:iongfx_for_tests',
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
        '../gfxutils/gfxutils.gyp:iongfxutils_for_tests',
        '../portgfx/portgfx.gyp:ionportgfx_for_tests',
      ],
    },  # target: ionimage_for_tests
],
}
