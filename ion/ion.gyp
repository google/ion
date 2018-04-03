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
# Declares demos, all_public_libraries and all_tests aggregate targets.
#
# The targets in this file are here just to serve as groupings, so that "all of
# Ion" can be built by pointing gyp to this file. Do NOT depend on the targets
# in this file to build your Ion-dependent thing, point to the individual Ion
# library targets you need.
{
  'includes': [
    'common.gypi',
  ],

  'targets': [
    {
      'target_name': 'demos',
      'type': 'none',
      'conditions': [
        ['OS != "qnx"', {
          'dependencies' : [
            'demos/demos.gyp:*',
          ],
        }],
      ],  # conditions
    },
    {
      'target_name': 'examples',
      'type': 'none',
      'conditions': [
        # Examples build only on systems with FreeGLUT
        ['OS in ["linux", "win"]', {
          'dependencies' : [
            'doc/usersguide/examples/examples.gyp:*',
          ],
        }],
      ],  # conditions
    },
    {
      # NOTE: Do not depend on this target! See note above.
      'target_name': 'all_public_libraries',
      'type': 'none',
      'dependencies' : [
        'analytics/analytics.gyp:ionanalytics',
        'base/base.gyp:ionbase',
        'external/external.gyp:*',
        'external/freetype2.gyp:ionfreetype2',
        'external/imagecompression.gyp:ionimagecompression',
        'gfx/gfx.gyp:iongfx',
        'gfxprofile/gfxprofile.gyp:iongfxprofile',
        'gfxutils/gfxutils.gyp:iongfxutils',
        'image/image.gyp:ionimage',
        'math/math.gyp:ionmath',
        'portgfx/portgfx.gyp:ionportgfx',
        'port/port.gyp:ionport',
        'profile/profile.gyp:ionprofile',
        'remote/remote.gyp:ionremote',
        'text/text.gyp:iontext',
      ],
    },
    {
      'target_name': 'all_tests',
      'type': 'none',
      'dependencies' : [
        'analytics/tests/analytics_tests.gyp:ionanalytics_test',
        'base/tests/base_tests.gyp:ionbase_test',
        'gfx/tests/gfx_tests.gyp:iongfx_test',
        'gfxprofile/tests/gfxprofile_tests.gyp:iongfxprofile_test',
        'gfxutils/tests/gfxutils_tests.gyp:iongfxutils_test',
        'image/tests/image_tests.gyp:ionimage_test',
        'math/tests/math_tests.gyp:ionmath_test',
        'port/tests/port_tests.gyp:ionport_test',
        'portgfx/tests/portgfx_tests.gyp:ionportgfx_test',
        'profile/tests/profile_tests.gyp:ionprofile_test',
        'remote/tests/remote_tests.gyp:ionremote_test',
        'text/tests/text_tests.gyp:iontext_test',
      ],
    },
  ],
}
