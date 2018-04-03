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
      'target_name': 'ionmath',
      'type': 'static_library',
      'sources' : [
        'angle.h',
        'angleutils.h',
        'fieldofview.h',
        'matrix.h',
        'matrixutils.cc',
        'matrixutils.h',
        'range.h',
        'rangeutils.h',
        'rotation.cc',
        'rotation.h',
        'transformutils.cc',
        'transformutils.h',
        'utils.h',
        'vector.h',
        'vectorutils.h',
      ],
      'dependencies': [
        '../base/base.gyp:ionbase',
      ],
    },  # target: ionmath

    {
      'target_name': 'ionmath_for_tests',
      'type': 'static_library',

      'sources' : [
        # A .cc file is necessary to actually create a .a.
        '../external/empty.cc',
        'tests/testutils.h',
      ],
      'dependencies': [
        ':ionmath',
        '../base/base.gyp:ionbase_for_tests',
        '<(ion_dir)/external/gtest.gyp:iongmock',
      ],
    },  # target: ionmath_for_tests
  ],
}
