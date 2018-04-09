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
    '../../common.gypi',
  ],

  'variables': {
    # The directory that gets added to the include dirs of every target that
    # transitively depends on a font target. Dependent targets use a font
    # by including a header file in ion/text/fonts, e.g.:
    #
    #     #include "ion/text/fonts/roboto_regular.h"
    #
    'genfiles_root_dir': '<(SHARED_INTERMEDIATE_DIR)/ionfonts-genfiles',

    # The place inside genfiles_root_dir where the header and cc files for this
    # font get placed.
    'genfiles_dir': '<(genfiles_root_dir)/ion/text/fonts'
  },

  'target_defaults': {
    'type': 'static_library',
    'dependencies': [
      '<(ion_dir)/port/port.gyp:ionport'
    ],
    'all_dependent_settings': {
      'include_dirs+': [
        '<(genfiles_root_dir)'
      ]
    },
  },

  'targets' : [
    {
      'target_name': 'roboto_regular',
      'sources': [
        '<(root_dir)/third_party/webfonts/apache/roboto/Roboto-Regular.ttf',
      ],
      'includes': [
        'font_target.gypi',
      ],
    },

    {
      'target_name': 'roboto_bold',
      'sources': [
        '<(root_dir)/third_party/webfonts/apache/roboto/Roboto-Bold.ttf',
      ],
      'includes': [
        'font_target.gypi',
      ],
    },

    {
      'target_name': 'roboto_italic',
      'sources': [
        '<(root_dir)/third_party/webfonts/apache/roboto/Roboto-Italic.ttf',
      ],
      'includes': [
        'font_target.gypi',
      ],
    },

    {
      'target_name': 'roboto_thin',
      'sources': [
        '<(root_dir)/third_party/webfonts/apache/roboto/Roboto-Thin.ttf',
      ],
      'includes': [
        'font_target.gypi',
      ],
    },

    {
      'target_name': 'droid_sans_mono_regular',
      'sources': [
        '<(root_dir)/third_party/webfonts/apache/droidsans/DroidSansMono-Regular.ttf',
      ],
      'includes': [
        'font_target.gypi',
      ],
    },

    {
      'target_name': 'droid_serif_regular',
      'sources': [
        '<(root_dir)/third_party/webfonts/apache/droidserif/DroidSerif-Regular.ttf',
      ],
      'includes': [
        'font_target.gypi',
      ],
    },

    {
      'target_name': 'droid_serif_bold',
      'sources': [
        '<(root_dir)/third_party/webfonts/apache/droidserif/DroidSerif-Bold.ttf',
      ],
      'includes': [
        'font_target.gypi',
      ],
    },

    {
      'target_name': 'droid_serif_italic',
      'sources': [
        '<(root_dir)/third_party/webfonts/apache/droidserif/DroidSerif-Italic.ttf',
      ],
      'includes': [
        'font_target.gypi',
      ],
    },
  ],  # targets
}
