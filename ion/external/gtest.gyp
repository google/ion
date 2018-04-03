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
    # Avoid conversion warnings.
    'cflags_cc!': [
      '-Wconversion',
    ],
    'defines': [
      # Needed for libc++ support.
      'GTEST_HAS_TR1_TUPLE=0',
      'GTEST_LANG_CXX11=1',    ],
    'xcode_settings': {
      'OTHER_CFLAGS': [
        '-Wno-conversion',
       ],
    },
    'include_dirs': [
       '../../third_party/googletest/googletest/include',
       '../../third_party/googletest/googletest',
       '../../third_party/googletest/googlemock/include',
       '../../third_party/googletest/googlemock/',
    ],

    'direct_dependent_settings': {
      'defines': [
        # Needed for libc++ support.
        'GTEST_HAS_TR1_TUPLE=0',
        'GTEST_LANG_CXX11=1',
      ],
      'include_dirs': [
         '../../third_party/googletest/googletest/include',
         '../../third_party/googletest/googletest',
         '../../third_party/googletest/googlemock/include',
         '../../third_party/googletest/googlemock/',
      ],
    },  # direct_dependent_settings
  },

  'targets': [
    {
      # A version of gtest that has safe allocs.
      'target_name': 'iongtest_safeallocs',
      'includes': [
        # This is for the API macros.
        '../dev/target_visibility.gypi',
      ],
      'include_dirs': [ '<(root_dir)' ],
      'dependencies': [
        ':iongtest_safeallocs_no_main',
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
      ],
      'sources': [
        # This is the gtest_main.cc that is has a call to
        # StaticDeleterDeleter::DestroyInstance();
        'gtest/gtest_main_safeallocs.cc',
      ],
    },  # target: iongtest
    {
      # A version of gtest that has safe allocs.  No main function is included.
      'target_name': 'iongtest_safeallocs_no_main',
      'includes': [
        # This is for the API macros.
        '../dev/target_visibility.gypi',
      ],
      'include_dirs': [ '<(root_dir)' ],
      'dependencies': [
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
      ],
      'sources': [
        '../../third_party/googletest/googletest/src/gtest-all.cc',
      ],
    },  # target: iongtest

    {
      'target_name' : 'iongtest_vanilla',
      'include_dirs': [ '<(root_dir)' ],
      'sources' : [
        '../../third_party/googletest/googletest/src/gtest-all.cc',
        'gtest/gtest_port_main.cc',
      ],
    },  # target: iongtest_vanilla

    {
      'target_name': 'iongmock',
      'type': 'static_library',
      'dependencies': [
        ':iongtest_safeallocs_no_main',
      ],
      'sources': [
        '../../third_party/googletest/googlemock/src/gmock-all.cc',
      ],
    },  # target: iongmock
  ],
}
