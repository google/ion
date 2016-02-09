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
  'includes': [
    '../common.gypi',
  ],
  'variables': {
    'hb_src_dir': '<(root_dir)/third_party/harfbuzz',
    'hb_config_dir': '<(root_dir)/third_party/google/harfbuzz',
  },
  'targets': [
    {
      'target_name': 'ionharfbuzz',
      'type': 'static_library',
      'include_dirs': [
        '<(root_dir)',
        '<(hb_src_dir)/src',
        '<(hb_config_dir)',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(root_dir)',
          '<(hb_src_dir)/src',
          '<(hb_config_dir)',
        ],
      },  # all_dependent_settings
      'cflags_cc': [
        '-Wno-conversion',
      ],
      'defines': [
        'HAVE_CONFIG_H',
        '__ion__',
      ],
      'sources': [
        '<(hb_config_dir)/config.h',
        # To generate this list cd into third_party/harfbuzz and run:
        # ls -1 src/*.{h,hh,cc}|egrep -v 'main|test|hb-(coretext|glib|gobject|graphite2|ucdn|uniscribe)'|sed "sI\(.*\)I        '<(hb_src_dir)/\1',I"|LC_ALL=C sort
        '<(hb_src_dir)/src/hb-atomic-private.hh',
        '<(hb_src_dir)/src/hb-blob.cc',
        '<(hb_src_dir)/src/hb-blob.h',
        '<(hb_src_dir)/src/hb-buffer-deserialize-json.hh',
        '<(hb_src_dir)/src/hb-buffer-deserialize-text.hh',
        '<(hb_src_dir)/src/hb-buffer-private.hh',
        '<(hb_src_dir)/src/hb-buffer-serialize.cc',
        '<(hb_src_dir)/src/hb-buffer.cc',
        '<(hb_src_dir)/src/hb-buffer.h',
        '<(hb_src_dir)/src/hb-cache-private.hh',
        '<(hb_src_dir)/src/hb-common.cc',
        '<(hb_src_dir)/src/hb-common.h',
        '<(hb_src_dir)/src/hb-deprecated.h',
        '<(hb_src_dir)/src/hb-face-private.hh',
        '<(hb_src_dir)/src/hb-face.cc',
        '<(hb_src_dir)/src/hb-face.h',
        '<(hb_src_dir)/src/hb-fallback-shape.cc',
        '<(hb_src_dir)/src/hb-font-private.hh',
        '<(hb_src_dir)/src/hb-font.cc',
        '<(hb_src_dir)/src/hb-font.h',
        '<(hb_src_dir)/src/hb-ft.cc',
        '<(hb_src_dir)/src/hb-ft.h',
        '<(hb_src_dir)/src/hb-icu.cc',
        '<(hb_src_dir)/src/hb-icu.h',
        '<(hb_src_dir)/src/hb-mutex-private.hh',
        '<(hb_src_dir)/src/hb-object-private.hh',
        '<(hb_src_dir)/src/hb-open-file-private.hh',
        '<(hb_src_dir)/src/hb-open-type-private.hh',
        '<(hb_src_dir)/src/hb-ot-cmap-table.hh',
        '<(hb_src_dir)/src/hb-ot-font.cc',
        '<(hb_src_dir)/src/hb-ot-font.h',
        '<(hb_src_dir)/src/hb-ot-head-table.hh',
        '<(hb_src_dir)/src/hb-ot-hhea-table.hh',
        '<(hb_src_dir)/src/hb-ot-hmtx-table.hh',
        '<(hb_src_dir)/src/hb-ot-layout-common-private.hh',
        '<(hb_src_dir)/src/hb-ot-layout-gdef-table.hh',
        '<(hb_src_dir)/src/hb-ot-layout-gpos-table.hh',
        '<(hb_src_dir)/src/hb-ot-layout-gsub-table.hh',
        '<(hb_src_dir)/src/hb-ot-layout-gsubgpos-private.hh',
        '<(hb_src_dir)/src/hb-ot-layout-jstf-table.hh',
        '<(hb_src_dir)/src/hb-ot-layout-private.hh',
        '<(hb_src_dir)/src/hb-ot-layout.cc',
        '<(hb_src_dir)/src/hb-ot-layout.h',
        '<(hb_src_dir)/src/hb-ot-map-private.hh',
        '<(hb_src_dir)/src/hb-ot-map.cc',
        '<(hb_src_dir)/src/hb-ot-maxp-table.hh',
        '<(hb_src_dir)/src/hb-ot-name-table.hh',
        '<(hb_src_dir)/src/hb-ot-shape-complex-arabic-fallback.hh',
        '<(hb_src_dir)/src/hb-ot-shape-complex-arabic-table.hh',
        '<(hb_src_dir)/src/hb-ot-shape-complex-arabic.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-default.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-hangul.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-hebrew.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-indic-machine.hh',
        '<(hb_src_dir)/src/hb-ot-shape-complex-indic-private.hh',
        '<(hb_src_dir)/src/hb-ot-shape-complex-indic-table.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-indic.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-myanmar-machine.hh',
        '<(hb_src_dir)/src/hb-ot-shape-complex-myanmar.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-private.hh',
        '<(hb_src_dir)/src/hb-ot-shape-complex-sea-machine.hh',
        '<(hb_src_dir)/src/hb-ot-shape-complex-sea.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-thai.cc',
        '<(hb_src_dir)/src/hb-ot-shape-complex-tibetan.cc',
        '<(hb_src_dir)/src/hb-ot-shape-fallback-private.hh',
        '<(hb_src_dir)/src/hb-ot-shape-fallback.cc',
        '<(hb_src_dir)/src/hb-ot-shape-normalize-private.hh',
        '<(hb_src_dir)/src/hb-ot-shape-normalize.cc',
        '<(hb_src_dir)/src/hb-ot-shape-private.hh',
        '<(hb_src_dir)/src/hb-ot-shape.cc',
        '<(hb_src_dir)/src/hb-ot-shape.h',
        '<(hb_src_dir)/src/hb-ot-tag.cc',
        '<(hb_src_dir)/src/hb-ot-tag.h',
        '<(hb_src_dir)/src/hb-ot.h',
        '<(hb_src_dir)/src/hb-private.hh',
        '<(hb_src_dir)/src/hb-set-private.hh',
        '<(hb_src_dir)/src/hb-set.cc',
        '<(hb_src_dir)/src/hb-set.h',
        '<(hb_src_dir)/src/hb-shape-plan-private.hh',
        '<(hb_src_dir)/src/hb-shape-plan.cc',
        '<(hb_src_dir)/src/hb-shape-plan.h',
        '<(hb_src_dir)/src/hb-shape.cc',
        '<(hb_src_dir)/src/hb-shape.h',
        '<(hb_src_dir)/src/hb-shaper-impl-private.hh',
        '<(hb_src_dir)/src/hb-shaper-list.hh',
        '<(hb_src_dir)/src/hb-shaper-private.hh',
        '<(hb_src_dir)/src/hb-shaper.cc',
        '<(hb_src_dir)/src/hb-unicode-private.hh',
        '<(hb_src_dir)/src/hb-unicode.cc',
        '<(hb_src_dir)/src/hb-unicode.h',
        '<(hb_src_dir)/src/hb-utf-private.hh',
        '<(hb_src_dir)/src/hb-version.h',
        '<(hb_src_dir)/src/hb-warning.cc',
        '<(hb_src_dir)/src/hb.h',
      ],  # sources
      'dependencies': [
        'freetype2.gyp:ionfreetype2',
        'icu.gyp:ionicu',
      ],
    },  # target: ionicu
  ],  # targets
}
