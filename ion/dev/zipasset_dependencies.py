#!/usr/bin/python
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

"""Similar to zipasset_generator.py, but just prints the IAD's required files.

This is useful in generating a list of dependencies for adding to the build
system. This means that if anything inside the IAD changes (any of its assets),
then the IAD is rebuilt.

Run like:

  python $0 --iads "iad_file1.iad iad_file2.iad ..." --search_path <search path>

This will print out the RELATIVE path to the asset files in ALL of the iad
files, relative to the .iad file. THIS IS IMPORTANT! Returning absolute paths to
source files is no bueno for certain gyp generators (xcode).
"""

__author__ = 'dimator@google.com (Dimi Shahbaz)'

import argparse
import os
import sys
import zipasset_generator


def main():
  parser = argparse.ArgumentParser(
      usage='Usage: %prog --iads "<asset definition files>" '
      '--search_path <search path> '
      '\nFile arguments are relative to the directory in which this is run.')
  parser.add_argument('--iads',
                      default=None,
                      help='quoted list of iad files to parse.')
  parser.add_argument('--search_path',
                      help='search path to find files in the iad.')
  options = parser.parse_args(sys.argv[1:])

  # iads can be empty, but it can't be completely unset.
  if options.iads is None:
    print 'Need --iads'
    return 1

  if not options.search_path:
    print 'Need --search_path'
    return 1

  # Set up additional search paths.
  path_to_script = os.path.abspath(os.path.dirname(__file__))
  # Assume that the 'ion' directory is top-level.
  root_path = os.path.join(os.path.dirname(__file__), '..', '..')
  root_path = os.path.abspath(root_path)
  default_search_paths = [root_path, os.path.abspath(options.search_path)]

  # Note that --iads is given as one big space-separated string.
  for iad_file in options.iads.split():
    # Add the absolute path to the IAD file as the primary search path for the
    # manifest.
    search_paths = default_search_paths[:]
    search_paths.insert(0, os.path.dirname(os.path.abspath(iad_file)))

    _, _, manifest = zipasset_generator.BuildManifest(iad_file, search_paths)
    for abs_path_to_asset, _ in manifest:
      rel_path_to_asset = os.path.relpath(
          abs_path_to_asset, options.search_path)
      # Windows requires replacing backslashes (given by the os module
      # implicitly) with forward slashes.
      print rel_path_to_asset.replace('\\', '/')


if __name__ == '__main__':
  sys.exit(main())
