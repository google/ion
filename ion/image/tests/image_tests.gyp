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
  'includes': [
    '../../common.gypi',
  ],

  'targets': [
    {
      'target_name': 'ionimage_test',
      'includes': [ '../../dev/test_target.gypi' ],
      'sources' : [
        'conversionutils_test.cc',
        'ninepatch_test.cc',
        'renderutils_test.cc',
      ],
      'dependencies' : [
        'image_tests_assets',
        '<(ion_dir)/image/image.gyp:ionimage',
        '<(ion_dir)/image/image.gyp:ionimagejpeg',
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
        '<(ion_dir)/external/gtest.gyp:iongtest_safeallocs',
        '<(ion_dir)/port/port.gyp:ionport',
        '<(ion_dir)/gfx/gfx.gyp:iongfx_for_tests',
        '<(ion_dir)/gfxutils/gfxutils.gyp:iongfxutils_for_tests',
        '<(ion_dir)/portgfx/portgfx.gyp:ionportgfx_for_tests',
      ],
    },

    {
      'target_name': 'image_tests_assets',
      'type': 'static_library',
      'includes': [
        '../../dev/zipasset_generator.gypi',
      ],
      'sources' : [
        'data/images.iad',
      ],
      'dependencies' : [
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
      ],
    },
  ],
}

