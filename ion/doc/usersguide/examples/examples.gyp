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
    '../../../common.gypi',
  ],

  'variables': {
    'example_deps': [
      '<(ion_dir)/analytics/analytics.gyp:ionanalytics',
      '<(ion_dir)/base/base.gyp:ionbase',
      '<(ion_dir)/external/external.gyp:*',
      '<(ion_dir)/external/freeglut.gyp:freeglut',
      '<(ion_dir)/external/freetype2.gyp:ionfreetype2',
      '<(ion_dir)/external/imagecompression.gyp:ionimagecompression',
      '<(ion_dir)/gfx/gfx.gyp:iongfx',
      '<(ion_dir)/gfxutils/gfxutils.gyp:iongfxutils',
      '<(ion_dir)/image/image.gyp:ionimage',
      '<(ion_dir)/math/math.gyp:ionmath',
      '<(ion_dir)/portgfx/portgfx.gyp:ionportgfx',
      '<(ion_dir)/port/port.gyp:ionport',
      '<(ion_dir)/profile/profile.gyp:ionprofile',
      '<(ion_dir)/remote/remote.gyp:ionremote',
      '<(ion_dir)/text/text.gyp:iontext',
    ]
  },

  'conditions' : [
    ['OS == "win"', {
      'libraries': [
        '-ladvapi32',
        '-lwinmm',
      ],
    }],
  ],

  'targets': [
    {
      'target_name': 'hierarchy',
      'type': 'executable',
      'sources': ['hierarchy.cc'],
      'dependencies': ['<@(example_deps)'],
    },
    {
      'target_name': 'rectangle',
      'type': 'executable',
      'sources': ['rectangle.cc'],
      'dependencies': ['<@(example_deps)'],
    },
    {
      'target_name': 'shaders',
      'type': 'executable',
      'sources': ['shaders.cc'],
      'dependencies': ['<@(example_deps)'],
    },
    {
      'target_name': 'shape',
      'type': 'executable',
      'sources': ['shape.cc'],
      'dependencies': ['<@(example_deps)'],
    },
    {
      'target_name': 'text',
      'type': 'executable',
      'sources': ['text.cc'],
      'dependencies': ['<@(example_deps)'],
    },
    {
      'target_name': 'texture',
      'type': 'executable',
      'sources': ['texture.cc'],
      'dependencies': ['<@(example_deps)'],
    },
  ],
}
