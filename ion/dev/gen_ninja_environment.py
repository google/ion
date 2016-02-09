#
# Copyright 2016 Google Inc. All Rights Reserved.
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
"""Create an environment.$ARCH file, as used by ninja.

This is enabled by using -G ninja_use_custom_environment_files.

The file format is simply a set of environment variables in which to run things
in. Use this to set the PATH to e.g. VC binaries, platform_sdk binaries, etc.
"""

import optparse
import os
import StringIO

NUL = chr(0)


def DoMain(argv):
  """This is the entry point that's called when pymod_do_main is used in gyp.

  Args:
    argv: list of arguments as defined in the .gyp file.

  Returns:
    Command to run (which replaces the 'action' in the gyp target).
  """
  parser = optparse.OptionParser()
  parser.add_option('--environment_file_dir')
  parser.add_option('--possible_configurations')
  parser.add_option('--windows_path_dirs_x86')
  parser.add_option('--windows_path_dirs_x64')
  # Some tools, such as rc.exe, expect to find "windows.h". Since rc.exe is
  # launched by gyp-win-tool (a wrapper for some common windows build tools), we
  # don't have access to its command line arguments. Instead we set the INCLUDE
  # environment variable (globally) so it can find windows.h.
  parser.add_option('--windows_include_dirs')
  options, _ = parser.parse_args(argv)

  environment_file_dir = options.environment_file_dir
  possible_configurations = options.possible_configurations.split(' ')
  windows_path_dirs_x86 = options.windows_path_dirs_x86.split(' ')
  windows_path_dirs_x64 = options.windows_path_dirs_x64.split(' ')

  # Note that LIB is not included, as they are in other projects'
  # environment.x?? generators, because we use 'include_dirs' and 'library_dirs'
  # as needed. (INCLUDE is included, because of the above note about rc.exe).
  default_env = {
      'SYSTEMROOT': os.environ.get('SYSTEMROOT').split(os.pathsep),
      'TEMP': os.environ.get('TEMP').split(os.pathsep),
      'TMP': os.environ.get('TMP').split(os.pathsep),
      'PATH': os.environ.get('PATH').split(os.pathsep),
      'INCLUDE': options.windows_include_dirs.split(),

      # This value is needed by some tools, such as the multiprocessing module.
      'NUMBER_OF_PROCESSORS': os.environ.get('NUMBER_OF_PROCESSORS', '1'),
  }

  env_x86 = default_env.copy()
  env_x64 = default_env.copy()

  # Make the specified paths ahead of the default paths.
  env_x86['PATH'] = windows_path_dirs_x86 + env_x86['PATH']
  env_x64['PATH'] = windows_path_dirs_x64 + env_x64['PATH']

  for conf in possible_configurations:
    for env, env_file_name in [(env_x86, 'environment.x86'),
                               (env_x64, 'environment.x64')]:
      env_file = os.path.join(environment_file_dir, conf, env_file_name)
      # The directories might not exist yet, since this action is running as the
      # very first target in the build process.
      if not os.path.isdir(os.path.dirname(env_file)):
        os.makedirs(os.path.dirname(env_file))

      # It saves a lot of build time if we don't overwrite this file if it has
      # not been modified.
      existing_contents = None
      if os.path.isfile(env_file):
        with open(env_file, 'rb') as f:
          existing_contents = f.read()

      sio = StringIO.StringIO()
      for k, v in env.items():
        if isinstance(v, list):
          # Assume they are paths.
          sio.write('='.join([k, os.pathsep.join(v)]))
        else:
          # Assume it's just a value.
          sio.write('='.join((k, v)))
        sio.write(NUL)

      # Yes, this last NUL is necessary. See gyp's win-tool.py:_GetEnv() for
      # details.
      sio.write(NUL)

      if sio.getvalue() != existing_contents:
        with open(env_file, 'wb') as f:
          f.write(sio.getvalue())

  # This script is used in an 'action' block. The output from this command will
  # replace the contents of the action block, so we must return some dummy thing
  # to execute. The actual work is already done above, so this is just to make
  # gyp happy.
  #
  # REM is a command in windows that does nothing.
  return 'rem'
