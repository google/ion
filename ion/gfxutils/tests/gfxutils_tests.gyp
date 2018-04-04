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
      'target_name': 'iongfxutils_test',
      'includes': [ '../../dev/test_target.gypi' ],
      'sources' : [
        'buffertoattributebinder_test.cc',
        'frame_test.cc',
        'printer_test.cc',
        'shadermanager_test.cc',
        'shadersourcecomposer_test.cc',
        'shapeutils_test.cc',
      ],
      'dependencies' : [
        '<(ion_dir)/external/gtest.gyp:iongtest_safeallocs',
        '<(ion_dir)/gfxutils/gfxutils.gyp:iongfxutils_for_tests',
        '<(ion_dir)/gfx/gfx.gyp:iongfx_for_tests',  # for mockgraphics
      ],
    },
  ],
}

