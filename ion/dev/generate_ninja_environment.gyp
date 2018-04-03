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
      'target_name': 'generate_ninja_environment',
      'type': 'none',

      # Must reset this targets dependencies to empty, because otherwise,
      # it depends on itself (see target_defaults in windows.gypi).
      # Nb. the '=' sign below, which means "replace" existing dependencies.
      'dependencies=': [
      ],

      'conditions': [
        ['GENERATOR == "ninja" ', {
          'toolsets': ['host', 'target'],
        }],
      ],  # conditions

      'actions': [
        {
          # This action is responsible for creating environment.x86 for ninja
          # to use when it's building.
          'action_name': 'generate_ninja_environment_files',
          'inputs': [
            'gen_ninja_environment.py',
          ],
          'outputs': [
            # Note: environment.x64 is not included. If it is necessary, modify
            # gen_ninja_environment.py as appropriate to add this, and add more
            # paths to windows.gypi.
            '<(out_dir)/environment.x86',
            '<(out_dir)/environment.x64',
          ],
          'action': [
            # Note that the path to gen_ninja_environment.py has to be in the
            # python interpreter's path (build.py does this).
            '<!@pymod_do_main(gen_ninja_environment '
            '--environment_file_dir "<(gyp_out_os_dir)" '
            # See os.gypi for where this variable comes from.
            '--possible_configurations "<(windows_possible_configurations)" '
            # See windows.gypi.
            '--windows_path_dirs_x86 "<(windows_path_dirs_x86)" '
            '--windows_path_dirs_x64 "<(windows_path_dirs_x64)" '
            '--windows_include_dirs "<(include_dirs_common)" '
            ')',
          ],
        },
      ],
    },  # target: generate_ninja_environment
  ],  # targets
}
