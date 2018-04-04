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
# Copy files that depend on the configuration.
#
# This really has to be done in a action script like this, because there is no
# no common support among generators for configuration-specific 'copies'
# sections.
#
# This works (crudely) as follows:
#
# python $0 --configuration <config_name> --destination <destination> \
#     [list of files]
#
# where each item in the list of files is prefixed by the configuration in which
# it should be copied. For example:
#
# python $0 --configuration dbg --destination wherever/tests \
#      dbg:path/to/file1  opt:path/to/file2  prod:path/to/file3
#
# This would result in ONLY file1 being copied.

import argparse
import filecmp
import os
import shutil
import stat
import sys


def main(argv):
  parser = argparse.ArgumentParser(
      usage='Usage: %(prog)s --configuration config --destination dest/ '
      'prefix:file1 prefix:file2')
  parser.add_argument('--configuration',
                      required=True,
                      help='configuration name.')
  parser.add_argument('--destination',
                      required=True,
                      help='path to copy to.')
  parser.add_argument(
      'files',
      nargs='+',
      help='files with prefixes (e.g. "opt:path/to/file1")')
  options = parser.parse_args(argv[1:])

  for f in options.files:
    prefix, source_path = f.split(':', 1)

    if prefix != options.configuration:
      continue

    dest_path = os.path.join(options.destination, os.path.basename(source_path))

    # Determine if we need to copy the file at all. This is a little
    # optimization to avoid copying a existing files that have not changed.
    if os.path.isfile(dest_path) and filecmp.cmp(dest_path, source_path):
      continue

    if not os.path.isdir(os.path.dirname(dest_path)):
      os.makedirs(os.path.dirname(dest_path))

    if os.path.isfile(dest_path):
      # On windows, we have issues copying files if they exist and are
      # read-only, so chmod first.
      if not os.access(dest_path, os.W_OK):
        os.chmod(dest_path, stat.S_IWRITE)

    shutil.copy2(source_path, dest_path)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
