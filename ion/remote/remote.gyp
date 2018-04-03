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
    'type': 'static_library',
    'includes' : [
      '../dev/target_visibility.gypi',
    ],

    'conditions': [
      ['OS == "nacl"', {
        'link_settings': {
          'libraries': [
            '-lnacl_io',
          ],
        },
      }],
    ],  # conditions

  },

  'targets' : [
    {
      'target_name' : 'remote_assets',

      'type': 'static_library',
      'includes' : [
        '../dev/zipasset_generator.gypi',
      ],

      'sources': [
        'res/calltrace.iad',
        'res/nodegraph.iad',
        'res/resources.iad',
        'res/root.iad',
        'res/settings.iad',
        'res/shader_editor.iad',
        'res/tracing.iad',
        # Excluded conditionally below.
        'res/geturi_asmjs.iad',
        'res/geturi_cc.iad',
      ],

      'conditions': [
        ['OS in ["asmjs", "nacl"]', {
          'sources!': [
            'res/geturi_cc.iad',
          ],
        }, { # else
          'sources!': [
            'res/geturi_asmjs.iad',
          ],
        }],
      ],  # conditions

      'dependencies' : [
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },

    {
      'target_name' : 'ionremote',
      'sources': [
        'calltracehandler.cc',
        'calltracehandler.h',
        'httpserver.cc',
        'httpserver.h',
        'nodegraphhandler.cc',
        'nodegraphhandler.h',
        'remoteserver.cc',
        'remoteserver.h',
        'resourcehandler.cc',
        'resourcehandler.h',
        'settinghandler.cc',
        'settinghandler.h',
        'shaderhandler.cc',
        'shaderhandler.h',
        'tracinghandler.cc',
        'tracinghandler.h',
      ],
      'dependencies': [
        ':remote_assets',
        '<(ion_dir)/analytics/analytics.gyp:ionanalytics',
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/external/external.gyp:ionmongoose',
        '<(ion_dir)/external/external.gyp:ionstblib',
        '<(ion_dir)/external/external.gyp:ionzlib',
        '<(ion_dir)/external/imagecompression.gyp:ionimagecompression',
        '<(ion_dir)/gfx/gfx.gyp:iongfx',
        '<(ion_dir)/gfxprofile/gfxprofile.gyp:iongfxprofile',
        '<(ion_dir)/gfxutils/gfxutils.gyp:iongfxutils',
        '<(ion_dir)/image/image.gyp:ionimage',
        '<(ion_dir)/port/port.gyp:ionport',
        '<(ion_dir)/portgfx/portgfx.gyp:ionportgfx',
        '<(ion_dir)/profile/profile.gyp:ionprofile',
      ],
    },

    {
      'target_name': 'ionremote_test_utils',
      'type': 'static_library',
      'sources': [
        'tests/getunusedport.cc',
        'tests/getunusedport.h',
      ],
      'dependencies' : [
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
      ],
    },  # target: ionremote_test_utils

    {
      'target_name': 'ionremote_for_tests',
      'type': 'static_library',
      'sources': [
        'tests/getunusedport.cc',
        'tests/getunusedport.h',
        'tests/httpservertest.h',
      ],
      'dependencies' : [
        ':httpclient',
        ':ionremote',
        ':ionremote_test_utils',
        ':portutils',
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
        '<(ion_dir)/gfx/gfx.gyp:iongfx_for_tests',
        '<(ion_dir)/gfxutils/gfxutils.gyp:iongfxutils_for_tests',
        '<(ion_dir)/image/image.gyp:ionimage_for_tests',
        '<(ion_dir)/portgfx/portgfx.gyp:ionportgfx_for_tests',
      ],
    },  # target: ionremote_for_tests

    {
      'target_name': 'httpclient',
      'type': 'static_library',
      'sources': [
        'httpclient.cc',
        'httpclient.h',
      ],
      'dependencies': [
        ':remote_assets',
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/port/port.gyp:ionport',
        '<(ion_dir)/external/external.gyp:ionmongoose',
      ],
    },  # target: httpclient
    {
      'target_name': 'portutils',
      'type': 'static_library',
      'sources': [
        'portutils.cc',
        'portutils.h',
      ],
      'dependencies': [
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },  # target: portutils
],
}
