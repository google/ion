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
# Make a .pexe from a pnacl executable, by calling pnacl-finalize.
#
# Used from 'make_into_app.gypi', which knows how to use this. Basically,
# this file needs a variable called 'make_this_target_into_an_app_param' which
# is the target being finalized.
{
  'includes': [ 'pnacl.gypi' ],
  'target_name': '<(make_this_target_into_an_app_param)_pexe',
  'type': 'none',

  'dependencies': [ '<(make_this_target_into_an_app_param)', ],

  'sources=': [
    # The unfinalized pnacl output. Note that the file name on disk must match
    # exactly the target name.
    '<(target_app_location_param)/<(make_this_target_into_an_app_param)',
  ],

  'rules': [
    {
      'rule_name': 'pnacl-finalize',
      'extension': '',
      'inputs': [
        '<(pnacl_toolchain_path)/pnacl-finalize',
      ],
      'outputs': [
        '<(target_app_location_param)/<(RULE_INPUT_ROOT).pexe',
      ],
      'action': [
        '<(pnacl_toolchain_path)/pnacl-finalize',
        '-o',
        '<(target_app_location_param)/<(RULE_INPUT_ROOT).pexe',
        '<(RULE_INPUT_PATH)',
      ],
    },
  ], # rules
}
