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
        'glenums.h',
        'glheaders.h',
        'isextensionsupported.cc',
        'isextensionsupported.h',
        'setswapinterval.cc',
        'setswapinterval.h',
        'glcontext.cc',
        'glcontext.h',
      ],
      'conditions': [
        ['OS in ["win"] and ogles20==0', {
          'sources' : [
            'wglcontext.cc',
            'window_win32.cc',
            'window_win32.h',
          ],
        }],
        ['OS in ["win"] and ogles20==1', {
          'sources' : [
            'eglcontext.cc',
            'eglcontextbase.cc',
            'eglcontextbase.h',
          ],
        }],
        ['OS in ["ios"]', {
          'sources' : [
            'eaglcontext.mm',
          ],
        }],
        ['OS in ["mac"]', {
          'sources' : [
            'nsglcontext.mm',
          ],
        }],
        ['OS in ["linux"]', {
          'sources' : [
            'glxcontext.cc',
          ],
        }],
        ['OS in ["android"]', {
          'sources' : [
            'eglcontext.cc',
            'eglcontextbase.cc',
            'eglcontextbase.h',
          ],
        }],
        ['OS in ["nacl"]', {
          'sources' : [
            'naclcontext.cc',
          ],
        }],
        ['OS in ["asmjs"]', {
          'sources' : [
            'asmjscontext.cc',
            'eglcontextbase.cc',
            'eglcontextbase.h',
          ],
        }],
        # OpenGL ES is required for these platforms or when OGLES20 is
        # explicitly enabled.
        ['OS in ["android", "ios", "nacl", "asmjs", "qnx"] or ogles20==1', {
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
