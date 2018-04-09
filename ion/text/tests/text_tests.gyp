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
      'target_name': 'iontext_test',
      'includes': [
        '../../dev/test_target.gypi',
        '../../dev/zipasset_generator.gypi',
      ],
      'sources' : [
        'basicbuilder_test.cc',
        'binpacker_test.cc',
        'font_test.cc',
        'fontimage_test.cc',
        'fontmacros_test.cc',
        'fontmanager_test.cc',
        'freetypefont_test.cc',
        'layout_test.cc',
        'outlinebuilder_test.cc',
        'platformfont_test.cc',
        'sdfutils_test.cc',
      ],
      'conditions': [
        ['OS in ["ios", "mac"]', {
          'sources': [
            'coretextfont_test.cc',
          ],
        }],  # OS in ["ios", "mac"]
      ],  # conditions
      'dependencies' : [
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/external/gtest.gyp:iongtest_safeallocs',
        '<(ion_dir)/text/text.gyp:iontext_for_tests',
        '<(ion_dir)/text/fonts/fonts.gyp:roboto_regular',
        '<(ion_dir)/gfx/gfx.gyp:iongfx_for_tests',
        '<(ion_dir)/gfxutils/gfxutils.gyp:iongfxutils_for_tests',
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },
  ],
}
