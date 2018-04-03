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
      'target_name': 'ionport_test',
      'includes': [ '../../dev/test_target.gypi' ],
      'sources' : [
        'align_test.cc',
        'atomic_test.cc',
        'barrier_test.cc',
        'break_test.cc',
        'cxx11_test.cc',
        'environment_test.cc',
        'fileutils_test.cc',
        'macros_test.cc',
        'memory_test.cc',
        'memorymappedfile_test.cc',
        'semaphore_test.cc',
        'stacktrace_test.cc',
        'std_array_test.cc',
        'std_unordered_map_test.cc',
        'std_unordered_set_test.cc',
        'string_test.cc',
        'threadutils_test.cc',
        'timer_test.cc',
      ],
      'dependencies' : [
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/external/gtest.gyp:iongtest_vanilla',
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },
  ],
}

