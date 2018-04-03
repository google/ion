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

  'targets': [
    {
      'target_name' : 'ionport',
      'type': 'static_library',
      'includes' : [
        '../dev/target_visibility.gypi',
    ],
      'dependencies': [
        'header_overrides',
      ],
      'sources' : [
        'align.h',
        'atomic.h',
        'barrier.cc',
        'barrier.h',
        'break.cc',
        'break.h',
        'environment.cc',
        'environment.h',
        'fileutils.cc',
        'fileutils.h',
        'logging.cc',
        'logging.h',
        'macros.h',
        'memory.cc',
        'memory.h',
        'memorymappedfile.cc',
        'memorymappedfile.h',
        'nullptr.h',
        'semaphore.cc',
        'semaphore.h',
        'stacktrace.cc',
        'stacktrace.h',
        'static_assert.h',
        'string.cc',
        'string.h',
        'threadutils.cc',
        'threadutils.h',
        'timer.cc',
        'timer.h',
        'trace.h',
        'useresult.h',
      ],

      'conditions': [
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            # Prevent xcode confusion due to name collision of port/semaphore.h
            # with the system header semaphore.h
            'USE_HEADERMAP': 'NO'
          },
        }],
        ['OS == "ios"', {
          'sources': [
            'logging_ios.mm',
          ],
          'xcode_settings': {
            # Force all files to compile as Objective-C++ because fileutils.cc
            # has some objective-c++ in it. Easier than calling it out
            # specifically, or splitting the code out into (e.g.) fileutils.mm.
            'OTHER_CFLAGS': [ '-x objective-c++' ],
          },
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        }],
        ['OS == "android"', {
          'sources': [
            'logging_android.cc',
            'android/jniutil.cc',
            'android/jniutil.h',
            'android/trace.cc',
          ],
          # Mips can only load and store words.  It needs a library for
          # atomic subword ops.
          'conditions': [
            ['flavor == "mips"', {
              'link_settings': {
                'libraries': [
                  '-latomic',
                ],
              }
            }],
          ]
        }],
        ['OS == "linux"', {
          'link_settings': {
            'libraries': [
              '-ldl',
              '-lrt',
              '-latomic',
            ],
          }
        }],
        ['OS == "nacl"', {
          'sources': [
            'logging_nacl.cc',
          ],
          'link_settings': {
            'libraries': [
              '-lppapi',
              '-lppapi_cpp',
              '-lppapi_cpp_private',
            ],
          }
        }],
        ['OS not in ["android", "nacl", "ios"]', {
          'sources': [ 'logging_cerr.cc' ],
        }],
        ['OS == "asmjs"', {
          'sources': [ 'asmjsfixes.cc' ],
        }],
        ['OS == "nacl"', {
          'sources': [ 'naclfixes.cc' ],
          'link_settings': {
            'libraries': [
              '-lpthread',
            ],
          },
        }],
      ]
    },  # target: ionport

    {
      'target_name': 'header_overrides',
      'type': 'none',
      'dependencies': [
        # This target depends on ABSL headers.
        '../external/external.gyp:ionabsl',
      ],
      'all_dependent_settings': {
        'include_dirs+': [
          '<(ion_dir)/port/override',
          '<(ion_dir)/port/override/third_party',
        ],
      },
    },  # target: header_overrides
  ],
}
