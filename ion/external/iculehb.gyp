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
    '../common.gypi',
  ],
  'variables': {
    'iculehb_src_dir': '<(root_dir)/third_party/iculehb/src/src',

  },
  'targets': [
    {
      'target_name': 'ioniculehb',
      'type': 'static_library',
      'include_dirs': [
        '<(root_dir)/',
        '<(root_dir)/third_party/icu/icu4c/source/common',
        '<(root_dir)/third_party/icu/icu4c/source/common', # For cmemory.h
        '<(root_dir)/third_party/harfbuzz/src',
      ],
      'cflags_cc': [
        '-w',
      ],
      'defines': [
        'U_COMMON_IMPLEMENTATION',
        'U_STATIC_IMPLEMENTATION',
        'U_IMPORT=',
        'U_EXPORT=',
      ],
      'msvs_disabled_warnings': [
        '4244', # Conversion from 64-bit to 32-bit types.
        '4267', # Conversion from 64-bit to 32-bit types.
      ],
      'sources': [
        # To generate this list cd into third_party/iculehb/src/src and run:
        # ls -1 *.{h,cpp}|sed "sI\(.*\)I        '<(iculehb_src_dir)/\1',I"|LC_ALL=C sort
        '<(iculehb_src_dir)/LEFontInstance.cpp',
        '<(iculehb_src_dir)/LEFontInstance.h',
        '<(iculehb_src_dir)/LEGlyphFilter.h',
        '<(iculehb_src_dir)/LEGlyphStorage.cpp',
        '<(iculehb_src_dir)/LEGlyphStorage.h',
        '<(iculehb_src_dir)/LEInsertionList.cpp',
        '<(iculehb_src_dir)/LEInsertionList.h',
        '<(iculehb_src_dir)/LELanguages.h',
        '<(iculehb_src_dir)/LEScripts.h',
        '<(iculehb_src_dir)/LESwaps.h',
        '<(iculehb_src_dir)/LETypes.h',
        '<(iculehb_src_dir)/LayoutEngine.cpp',
        '<(iculehb_src_dir)/LayoutEngine.h',
        '<(iculehb_src_dir)/ScriptAndLanguageTags.cpp',
        '<(iculehb_src_dir)/ScriptAndLanguageTags.h',
        '<(iculehb_src_dir)/loengine.cpp',
        '<(iculehb_src_dir)/loengine.h',
      ],  # sources
      'dependencies': [
        'harfbuzz.gyp:ionharfbuzz',
      ],
      'conditions': [
        ['OS == "android" and flavor == "x86"', {
          'sources': [
            'scalblnf.c',
          ],
        }],
      ],
    },  # target: ionicu
  ],  # targets
}
