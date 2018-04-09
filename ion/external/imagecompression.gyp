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
    'external_common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['OS in ["linux"]', {
        # Avoid conversion warnings.
        'cflags_cc!': [
          '-Wconversion',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'ionimagecompression',
      'type': 'static_library',
      'sources': [
        '../../third_party/image_compression/image_compression/internal/bit_util.h',
        '../../third_party/image_compression/image_compression/internal/color_types.h',
        '../../third_party/image_compression/image_compression/internal/color_util.h',
        '../../third_party/image_compression/image_compression/internal/compressor4x4_helper.cc',
        '../../third_party/image_compression/image_compression/internal/compressor4x4_helper.h',
        '../../third_party/image_compression/image_compression/internal/dxtc_compressor.cc',
        '../../third_party/image_compression/image_compression/internal/dxtc_const_color_table.cc',
        '../../third_party/image_compression/image_compression/internal/dxtc_const_color_table.h',
        '../../third_party/image_compression/image_compression/internal/dxtc_to_etc_transcoder.cc',
        '../../third_party/image_compression/image_compression/internal/etc_compressor.cc',
        '../../third_party/image_compression/image_compression/internal/pixel4x4.cc',
        '../../third_party/image_compression/image_compression/internal/pixel4x4.h',
        '../../third_party/image_compression/image_compression/internal/pvrtc_compressor.cc',
        '../../third_party/image_compression/image_compression/public/compressed_image.h',
        '../../third_party/image_compression/image_compression/public/compressor.h',
        '../../third_party/image_compression/image_compression/public/dxtc_compressor.h',
        '../../third_party/image_compression/image_compression/public/dxtc_to_etc_transcoder.h',
        '../../third_party/image_compression/image_compression/public/etc_compressor.h',
        '../../third_party/image_compression/image_compression/public/pvrtc_compressor.h',
      ],
      'dependencies': [
        '<(ion_dir)/port/port.gyp:ionport',   # because of override
      ],
      'conditions': [
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
             ],
          },
        }],
      ],
      'defines': [
        'IS_LITTLE_ENDIAN=1',
      ],
      'include_dirs': [
        '../../third_party/image_compression/image_compression/..',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '../../third_party/image_compression/image_compression/..',
        ],
      },
    },
  ],
}
