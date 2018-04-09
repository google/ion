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
    '../../common.gypi',
  ],

  'targets': [
    {
      'target_name': 'ionremote_test_assets',
      'type': 'static_library',
      'includes': [
        '../../dev/zipasset_generator.gypi',
      ],
      'dependencies': [
        '<(ion_dir)/port/port.gyp:ionport',
      ],
      'sources': [
        'data/remote_tests.iad',
      ],
    },  # target: ionremote_test_assets

    {
      'target_name': 'ionremote_test',
      'includes': [
        '../../dev/test_target.gypi',
      ],
      'conditions': [
        ['OS not in  ["asmjs", "nacl"]', {
          # We don't use HttpClient or websockets in asmjs and nacl since they
          # don't support networking; instead these tests directly serve URIs.
          'sources' : [
            'httpclient_test.cc',
            'websocket_test.cc',
          ],
        }],
      ],  # conditions

      'sources' : [
        'calltracehandler_test.cc',
        'httpserver_test.cc',
        'nodegraphhandler_test.cc',
        'portutils_test.cc',
        'remoteserver_test.cc',
        'resourcehandler_test.cc',
        'settinghandler_test.cc',
        'shaderhandler_test.cc',
        'tracinghandler_test.cc',
      ],

      'dependencies' : [
        ':ionremote_test_assets',
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/external/external.gyp:ioneasywsclient',
        '<(ion_dir)/external/gtest.gyp:iongtest_safeallocs',
        '<(ion_dir)/gfx/gfx.gyp:iongfx_for_tests',
        '<(ion_dir)/gfxutils/gfxutils.gyp:iongfxutils_for_tests',
        '<(ion_dir)/image/image.gyp:ionimage',
        '<(ion_dir)/port/port.gyp:ionport',
        '<(ion_dir)/remote/remote.gyp:ionremote_for_tests',
        '<(ion_dir)/remote/remote.gyp:portutils',
        '<(ion_dir)/analytics/analytics.gyp:ionanalytics',
      ],
    },  # target: remote_test

  ],
}

