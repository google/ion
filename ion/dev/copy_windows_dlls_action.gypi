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
  # On windows, console binaries depend on the CRT dlls. However, there are
  # multiple versions of these depending on the configuration you're
  # building. At gyp-time, there is no way to determine the configuration
  # being built. There is also no way of supplying configuration-specific
  # 'copies' sections for each configuration. So, we have to resort to this
  # hack: this is an action (that runs at build time) that takes the
  # configuration name on the command line, and copies a specific subset of
  # DLLs into the right spot.
  #
  # Make sure to specify a <(windows_dlls_destination_param), e.g.:
  #
  #    'variables': {
  #      'windows_dlls_destination_param': '<(PRODUCT_DIR)/tests',
  #    },
  #    'includes': [
  #      'copy_windows_dlls_action.gypi',
  #    ],

  'actions': [
    {
      'action_name': 'copying_msvc_dlls',
      'message': 'Copying MSVC runtime dlls',

      'inputs': [
        'copy_configuration_specific_files.py',
      ],

      'conditions': [
        ['msvc_version < 140', {
          'includes': [
            'config_variable_holder.gypi',
          ],
          'action': [
            '<(python)',
            '<(ion_dir)/dev/copy_configuration_specific_files.py',
            '--configuration=<(config_variable_holder)',  # Passed at build time.
            '--destination=<(windows_dlls_destination_param)',

            'dbg_x86:<(msvc_redist_dir)/Debug_NonRedist/x86/Microsoft.VC<(msvc_version).DebugCRT/msvcr<(msvc_version)d.dll',
            'dbg_x86:<(msvc_redist_dir)/Debug_NonRedist/x86/Microsoft.VC<(msvc_version).DebugCRT/msvcp<(msvc_version)d.dll',

            'opt_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/msvcr<(msvc_version).dll',
            'opt_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/msvcp<(msvc_version).dll',

            'prod_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/msvcr<(msvc_version).dll',
            'prod_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/msvcp<(msvc_version).dll',

            'dbg_x64:<(msvc_redist_dir)/Debug_NonRedist/x64/Microsoft.VC<(msvc_version).DebugCRT/msvcr<(msvc_version)d.dll',
            'dbg_x64:<(msvc_redist_dir)/Debug_NonRedist/x64/Microsoft.VC<(msvc_version).DebugCRT/msvcp<(msvc_version)d.dll',

            'opt_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/msvcr<(msvc_version).dll',
            'opt_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/msvcp<(msvc_version).dll',

            'prod_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/msvcr<(msvc_version).dll',
            'prod_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/msvcp<(msvc_version).dll',
          ],
        }, { # else
          'includes': [
            'config_variable_holder.gypi',
          ],
          'action': [
            '<(python)',
            '<(ion_dir)/dev/copy_configuration_specific_files.py',
            '--configuration=<(config_variable_holder)',  # Passed at build time.
            '--destination=<(windows_dlls_destination_param)',

            'dbg_x86:<(msvc_redist_dir)/Debug_NonRedist/x86/Microsoft.VC<(msvc_version).DebugCRT/concrt<(msvc_version)d.dll',
            'dbg_x86:<(msvc_redist_dir)/Debug_NonRedist/x86/Microsoft.VC<(msvc_version).DebugCRT/msvcp<(msvc_version)d.dll',
            'dbg_x86:<(msvc_redist_dir)/Debug_NonRedist/x86/Microsoft.VC<(msvc_version).DebugCRT/vcruntime<(msvc_version)d.dll',

            'opt_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/concrt<(msvc_version).dll',
            'opt_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/msvcp<(msvc_version).dll',
            'opt_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/vcruntime<(msvc_version).dll',

            'prod_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/concrt<(msvc_version).dll',
            'prod_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/msvcp<(msvc_version).dll',
            'prod_x86:<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT/vcruntime<(msvc_version).dll',

            'dbg_x64:<(msvc_redist_dir)/Debug_NonRedist/x64/Microsoft.VC<(msvc_version).DebugCRT/concrt<(msvc_version)d.dll',
            'dbg_x64:<(msvc_redist_dir)/Debug_NonRedist/x64/Microsoft.VC<(msvc_version).DebugCRT/msvcp<(msvc_version)d.dll',
            'dbg_x64:<(msvc_redist_dir)/Debug_NonRedist/x64/Microsoft.VC<(msvc_version).DebugCRT/vcruntime<(msvc_version)d.dll',

            'opt_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/concrt<(msvc_version).dll',
            'opt_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/msvcp<(msvc_version).dll',
            'opt_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/vcruntime<(msvc_version).dll',

            'prod_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/concrt<(msvc_version).dll',
            'prod_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/msvcp<(msvc_version).dll',
            'prod_x64:<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT/vcruntime<(msvc_version).dll',
          ],
        }],
      ],  # conditions


      'outputs': [
        # We can't know the outputs yet, so we can't list a concrete output
        # here. However, the action requires an output to be listed. This is
        # a non-existant file, which means this action will always run. Yes
        # I know this sucks.
        'nonexistant_file.nevercreated_<(_target_name)',
      ],
    },
  ],  # actions
}
