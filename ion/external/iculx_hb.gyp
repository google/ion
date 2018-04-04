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
    'iculxhb_src_dir': '<(root_dir)/third_party/iculx_hb',
  },
  'targets': [
    {
      'target_name': 'ioniculx_hb',
      'type': 'static_library',
      'include_dirs': [
        '<(root_dir)/',
        '<(root_dir)/third_party/icu/icu4c/source/common', # For cmemory.h
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
        '4005', # Macro redefinition. NO_GOOGLE_STRING_PIECE_IN_ICU is defined
                # in all_dependent_settings in icu.gyp, but also gets explicitly
                # defined in icu/include/unicode/stringpiece.h.
      ],
      'sources': [
        # To generate this list cd into third_party/iculx_hb and run:
        # find . -type f \( -name '*.cpp' -o -name '*.h' \)|sed -e  "sI\(.*\)I        '<(iculxhb_src_dir)/\1',I" -e 'sI/./I/I' |LC_ALL=C sort
        '<(iculxhb_src_dir)/include/layout/ParagraphLayout.h',
        '<(iculxhb_src_dir)/include/layout/RunArrays.h',
        '<(iculxhb_src_dir)/include/layout/playout.h',
        '<(iculxhb_src_dir)/include/layout/plruns.h',
        '<(iculxhb_src_dir)/source/layoutex/LXUtilities.cpp',
        '<(iculxhb_src_dir)/source/layoutex/LXUtilities.h',
        '<(iculxhb_src_dir)/source/layoutex/ParagraphLayout.cpp',
        '<(iculxhb_src_dir)/source/layoutex/RunArrays.cpp',
        '<(iculxhb_src_dir)/source/layoutex/playout.cpp',
        '<(iculxhb_src_dir)/source/layoutex/plruns.cpp',
      ],  # sources
      'dependencies' : [
        'iculehb.gyp:ioniculehb',
        'icu.gyp:ionicu',
      ],
    },  # target: ioniculx_hb
  ],  # targets
}
