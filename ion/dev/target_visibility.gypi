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
# Here is a summary of the visibility rules:
#
# when building this library static:
#   defines <- empty macro
#   all deps <- empty macro
# when building this library dynamic
#   defines <- linux: visible, windows: dllexport
#   all deps <- linux: visible, windows: dlimport

{
  'variables': {
    # api_macro is overridable.
    'api_macro': 'ION_API',
    'gcc_default_visibility': '<(api_macro)=__attribute__ ((visibility (\"default\")))',
    'windows_export_visibility': '<(api_macro)=__declspec(dllexport)',
    'windows_import_visibility': '<(api_macro)=__declspec(dllimport)',
    'empty_visibility': '<(api_macro)=',
  },
  'conditions': [
    ['OS == "win"', {
      'target_conditions': [
        ['_type == "shared_library"', {
          'defines': [
            '<(windows_export_visibility)',
          ],
        }, { # else
          'defines': [
            '<(empty_visibility)',
          ],
        }],
      ],  # target_conditions

    }, { # else
      'target_conditions': [
        ['_type == "shared_library"', {
          'defines': [
            '<(gcc_default_visibility)',
          ],
        }, { # else
          'defines': [
            '<(empty_visibility)',
          ],
        }],
      ],  # target_conditions

    }],
  ],  # conditions

  'all_dependent_settings': {
    'conditions': [
      ['OS=="win"', {
        'target_conditions': [
          ['_type == "shared_library"', {
            'defines': [
              '<(windows_import_visibility)',
            ],
          }, {
            'defines': [
              '<(empty_visibility)',
            ],
          }],
        ],
      }, {  # else
        'target_conditions': [
          ['_type == "shared_library"', {
            'defines': [
              '<(gcc_default_visibility)',
            ],
          }, {
            'defines': [
              '<(empty_visibility)',
            ],
          }],
        ],
      }],
    ],  # conditions
  },  # all_dependent_settings
}
