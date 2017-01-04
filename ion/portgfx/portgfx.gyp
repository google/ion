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
      'target_name': 'ionportgfx',
      'type': 'static_library',
      'sources': [
        'glheaders.h',
        'isextensionsupported.cc',
        'isextensionsupported.h',
        'setswapinterval.cc',
        'setswapinterval.h',
        'visual.cc',
        'visual.h',
      ],
      'conditions': [
        ['OS in ["win"]', {
          'sources' : [
            'visual_wgl.cc',
            'window_win32.cc',
            'window_win32.h',
          ],
        }],
        ['OS in ["ios"]', {
          'sources' : [
            'visual_eagl.mm',
          ],
        }],
        ['OS in ["mac"]', {
          'sources' : [
            'visual_nsgl.mm',
          ],
        }],
        ['OS in ["linux"]', {
          'sources' : [
            'visual_glx.cc',
          ],
        }],
        ['OS in ["android"]', {
          'sources' : [
            'visual_egl.cc',
            'visual_egl_base.cc',
            'visual_egl_base.h',
          ],
        }],
        ['OS in ["nacl"]', {
          'sources' : [
            'visual_nacl.cc',
          ],
        }],
        ['OS in ["asmjs"]', {
          'sources' : [
            'visual_asmjs.cc',
            'visual_egl_base.cc',
            'visual_egl_base.h',
          ],
        }],
        # OpenGL ES is required for these platforms.
        ['OS in ["android", "ios", "nacl", "asmjs", "qnx"]', {
          'defines': [
            'ION_GFX_OGLES20=1',
          ],
          'direct_dependent_settings': {
            'defines': [
              'ION_GFX_OGLES20=1',
            ],
          },
        }],
      ],
      'dependencies': [
        '../base/base.gyp:ionbase',
        '<(ion_dir)/external/external.gyp:graphics',
      ],
    },

    {
      'target_name': 'ionportgfx_for_tests',
      'type': 'static_library',

      'dependencies': [
        ':ionportgfx',
        '../base/base.gyp:ionbase_for_tests',
      ],

      'conditions': [
        ['OS in ["ios", "mac"]', {
          # A .cc file is necessary to actually create a .a for xcode.
          'sources': ['../external/empty.cc'],
        }],
        ['OS == "ios"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        }],
        ['OS == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AppKit.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
        ['OS == "linux"', {
          'link_settings': {
            'libraries': [
              '-lX11',
              '-lGL',
            ],
          },
        }],
      ],

    },
  ],
}
