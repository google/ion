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
  },  # target_defaults

  'targets' : [
    {
      'type': 'static_library',
      'target_name' : 'iontext',
      'sources': [
        'basicbuilder.cc',
        'basicbuilder.h',
        'binpacker.cc',
        'binpacker.h',
        'builder.cc',
        'builder.h',
        'font.cc',
        'font.h',
        'fontimage.cc',
        'fontimage.h',
        'fontmacros.h',
        'fontmanager.cc',
        'fontmanager.h',
        'icuutils.cc',
        'icuutils.h',
        'layout.cc',
        'layout.h',
        'outlinebuilder.cc',
        'outlinebuilder.h',
        'sdfutils.cc',
        'sdfutils.h',
      ],
      'dependencies': [
        '../gfx/gfx.gyp:iongfx',
        '../gfxutils/gfxutils.gyp:iongfxutils',
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/port/port.gyp:ionport',
      ],
      'conditions': [
        ['OS in ["ios", "mac"]', {
          'sources': [
            'coretextfont.h',
            'coretextfont.mm',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreGraphics.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreText.framework',
            ],
          },
        }, {  # else
          'conditions': [
            ['use_icu', {
              'include_dirs': [
                '<(root_dir)/third_party/icu/icu4c/source/common', # For cmemory.h
              ],
              'dependencies': [
                '<(ion_dir)/external/iculx_hb.gyp:ioniculx_hb',
              ],
              'defines': [
                'U_STATIC_IMPLEMENTATION',
                'U_IMPORT=',
                'U_EXPORT=',
              ],
            }],  # use_icu
          ],
          'sources': [
            'freetypefont.cc',
            'freetypefont.h',
            'freetypefontutils.cc',
            'freetypefontutils.h',
          ],
          'dependencies': [
            '../external/freetype2.gyp:ionfreetype2',
          ],
        }],  # OS in ["ios", "mac"]
      ],  # conditions
    },

    {
      'target_name': 'iontext_assets',
      'type': 'static_library',
      'includes': [
        '../dev/zipasset_generator.gypi',
      ],
      'dependencies': [
        '<(ion_dir)/port/port.gyp:ionport',
      ],
      'sources': [
        'tests/data/text_tests.iad',
      ],
    },  # target: iontext_assets

    {
      'target_name': 'iontext_for_tests',
      'includes': [
      ],
      'type': 'static_library',
      'sources': [
        'tests/buildertestbase.h',
        'tests/mockfont.h',
        'tests/mockfontimage.h',
        'tests/mockfontmanager.h',
        'tests/testfont.cc',
        'tests/testfont.h',
      ],
      # Though FreeType isn't available in the text library on iOS / Mac, it is
      # still used instead of a mock font for some tests.
      'conditions': [
        ['OS in ["ios", "mac"]', {
          'sources': [
            'freetypefont.cc',
            'freetypefont.h',
            'freetypefontutils.cc',
            'freetypefontutils.h',
          ],
          'dependencies': [
            '../external/freetype2.gyp:ionfreetype2',
          ],
        }],  # OS in ["ios", "mac"]
      ],
      'dependencies': [
        ':iontext',
        ':iontext_assets',
        '../base/base.gyp:ionbase_for_tests',
        '../gfx/gfx.gyp:iongfx',
        '../gfxutils/gfxutils.gyp:iongfxutils_for_tests',
      ],
    },  # target: iontext_for_tests
  ],  # targets
}
