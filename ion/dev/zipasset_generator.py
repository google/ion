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

"""Generates a compilable .cc file with zip data given an asset definition file.

An asset file is an XML file that lists group of files. See below for an
example:

<?xml version="1.0"?>
<IAD name="ZipAssetTest" disable_in_prod="true">
  <assets>
    <file>zipasset_file1.txt</file>
    <file>zipasset_file2.txt</file>
    <file name="dir/file1.txt">zipasset_file1.txt</file>
    <file name="dir/file2.txt">zipasset_file2.txt</file>
  </assets>
  <assets prefix="path">
    <file name="file1.txt">zipasset_file1.txt</file>
    <file name="file2.txt">zipasset_file2.txt</file>
  </assets>
</IAD>

IAD stands for Ion Asset Definition and is the root tag that must be present.
It must specify the name of the created zip asset. It can optionally specify
whether the assets in this file should be disabled in production, using the
disable_in_prod attribute. If this is "true", then the generated assets will be
guarded with "#if !ION_PRODUCTION".

Below that are any number of <assets> blocks which define groups of files with
similar paths. The filename to be added is the text of each <file> block, and
path to the file relative to the definition file. If an <asset> has a "prefix"
tag then that prefix is prepended to the filename. The "name" tag in a <file>
gives the file that name in the zipfile (with the current prefix prepended). The
above example uses only two actual files on the filesystem, but renames them and
puts them in different paths.

This script also embeds a special file named __asset_manifest__.txt. This file
contains one line of the form zip_name|local_name for each file in the zip. Each
of these is a mapping from the name of the file in the zip to the absolute path
of the file that it was sourced from on the local disk of the machine that runs
this script. This is useful for editing files inside of the zip in an external
source tree.

The files are zipped up using python's zip compression and saved in a memory
array; they are not written to a zip file. Instead, the zip data is written to
a .cc file that contains a single, namespace-wrapped static function called
RegisterAssets(). This function registers the zip data with Ion's
ZipAssetManager. The namespace containing the function is the name passed to
the IAD's "name" tag. For example, a definition file with name "MyAssets"
produces the function

MyAssets::RegisterAssets()

Prototype and call this function in your code to make the assets available from
ZipAssetManager.
"""


import array
import os
import StringIO
import sys
import textwrap
import xml.etree.ElementTree as ET
import zipfile


class Error(Exception):
  """Base class for errors."""


class InvalidAssetSyntaxError(Error):
  """Error representing an asset file with invalid syntax."""


#------------------------------------------------------------------------------
def GetPathToAsset(asset, search_paths):
  """Builds a list of (filename, asset) pairs from an asset file.

  Args:
    asset: string - a file to search for.
    search_paths: list of strings - paths to search for asset files

  Returns:
    The full path to the found file, or None if it does not exist.
  """
  if os.path.exists(asset):
    return asset
  else:
    for path in search_paths:
      asset_filename = os.path.join(path, asset)
      if os.path.exists(asset_filename):
        return asset_filename
  return None


#------------------------------------------------------------------------------
def GetIdentifyingAssetPath(abs_path):
  """Returns a path suitable for identifying an asset.

  Args:
    abs_path: string - an absolute path to a an asset file.

  Returns:
    A string that is either the absolute path or a source-tree relative path,
    depending on the contents of the path. Some distributed build systems use
    auto-generated paths which make the absolute path non-deterministic across
    build machines. If this case is detected, a source-tree relative path is
    returned instead.
  """
  return abs_path


#------------------------------------------------------------------------------
def BuildManifest(asset_file, search_paths):
  """Builds a list of (filename, asset) pairs from an asset file.

  Args:
    asset_file: string - a file containing asset definitions.
    search_paths: list of strings - paths to search for asset files

  Returns:
    A tuple containing:
      - an asset name
      - boolean, representing disable_in_prod
      - an array of (filename, asset) pairs, where filename is the absolute path
        to the file on local disk, and asset is the name the stored asset should
        be given.

  Raises:
    InvalidAssetSyntaxError: if asset_file has syntax errors.
    IOError: if asset_file or any files it references are not found.
  """
  if not os.path.exists(asset_file):
    raise IOError('Could not find asset file "%s"' % asset_file)

  # Load the xml.
  root = ET.parse(asset_file).getroot()
  if root.tag != 'IAD':
    raise InvalidAssetSyntaxError('Root tag should be "IAD" not "%s"' %
                                  root.tag)
  if 'name' not in root.attrib:
    raise InvalidAssetSyntaxError('Root tag requires a "name" attribute')

  disable_in_prod = root.attrib.get('disable_in_prod', 'false') == 'true'

  manifest = []
  for group in root.findall('assets'):
    # Get the path prefix if there is one.
    prefix = group.attrib['prefix'] if 'prefix' in group.attrib else ''
    for asset in group.findall('file'):
      # See if the file exists.
      asset_filename = GetPathToAsset(os.path.normcase(asset.text),
                                      search_paths)
      if not asset_filename:
        raise IOError('Asset "%s" does not exist, searched in %s' %
                      (asset.text, str(search_paths)))

      # The filename is either the asset text appended to the prefix, or a
      # requested name appended to the prefix.
      filename = os.path.join(
          prefix,
          asset.attrib['name'] if 'name' in asset.attrib else asset.text)
      # Add the full path to the file to the manifest.
      manifest += [(os.path.abspath(asset_filename), filename)]

  return root.attrib['name'], disable_in_prod, manifest


#------------------------------------------------------------------------------
def BuildAssetZip(asset_file, search_paths):
  """Reads an asset definition file and builds a zip file in memory.

  Args:
    asset_file: string - a file containing asset definitions.
    search_paths: list of strings - paths to search for asset files

  Returns:
    A tuple containing:
      - an asset name
      - boolean, representing disable_in_prod
      - a StringIO object that holds a memory zip file containing the assets and
        a manifest file.

  Raises:
    InvalidAssetSyntaxError: if asset_file has syntax errors.
  """
  name, disable_in_prod, manifest = BuildManifest(asset_file, search_paths)
  if not manifest:
    raise InvalidAssetSyntaxError(
        '"%s" does not contain any asset definitions' % asset_file)

  in_memory_zip = StringIO.StringIO()
  zip_file = zipfile.ZipFile(in_memory_zip, 'w',
                             compression=zipfile.ZIP_DEFLATED)

  # Use a dummy time so that the generated zip data is deterministic even if
  # built across distributed machines.
  dummy_date = (2017, 1, 1, 1, 0, 0)
  # Keep track of what the absolute path is for each file in the zip.
  manifest_file = StringIO.StringIO()
  # Write each file to the zip using the manifest name.
  original_size = 0
  for (local_name, zip_name) in manifest:
    original_size += os.path.getsize(local_name)
    internal_name = zip_name.lstrip('/')
    info = zipfile.ZipInfo(internal_name, dummy_date)
    with open(local_name, 'rb') as f:
      data = f.read()
      zip_file.writestr(info, data, zipfile.ZIP_DEFLATED)
    manifest_file.write('%s|%s\n' % (zip_name,
                                     GetIdentifyingAssetPath(local_name)))

  # Write the special manifest file.
  info = zipfile.ZipInfo('__asset_manifest__.txt', dummy_date)
  zip_file.writestr(info, manifest_file.getvalue(), zipfile.ZIP_DEFLATED)
  zip_file.close()

  # Pad the zip if it is not 4-byte aligned since we will encode it as 32-bit
  # integers.
  remainder = len(in_memory_zip.getvalue()) % 4
  if remainder != 0:
    in_memory_zip.write('\0' * (4 - remainder))

  # Print out useful statistics prettily if running interactively.
  if sys.stdout.isatty():
    suffix_index = 0
    levels = ['', 'k', 'M', 'G', 'T']
    compressed_size = in_memory_zip.len
    min_size = min(compressed_size, original_size)
    while min_size > 1024:
      min_size /= 1024
      original_size /= 1024
      compressed_size /= 1024
      suffix_index += 1
      suffix = levels[suffix_index]

    print ('Created ZipAsset %s: compressed %d%sB -> %d%sB (%.0f%%)' %
           (name, original_size, suffix, compressed_size, suffix,
            (100 * compressed_size) / original_size))

  return name, disable_in_prod, in_memory_zip


#------------------------------------------------------------------------------
def GenerateZipAsset(asset_file, search_paths, source_name):
  """Reads an asset definition file and produces asset files.

  The cc file contains a local static array defines a zip file of the files
  described in asset_file. It registers itself with the ZipAssetManager.

  Args:
    asset_file: string - a file containing asset definitions.
    search_paths: list of strings - paths to search for asset files
    source_name: string - the name of the output source file.
  """
  asset_name, disable_in_prod, zipdata = BuildAssetZip(asset_file, search_paths)

  # Write the source file that contains the array.
  source = open(source_name, 'w')

  if disable_in_prod:
    source.write('#if !ION_PRODUCTION\n\n')

  source.write('#include "ion/base/once.h"\n')
  source.write('#include "ion/base/zipassetmanager.h"\n\n')

  # Write namespace. Use the name of the IAD.
  source.write('namespace %s {\n\n' % asset_name)

  # Write function.
  source.write('bool RegisterAssets() {\n')

  # Write the zip data.
  wrapper = textwrap.TextWrapper(initial_indent='  ', width=80,
                                 subsequent_indent=' ' * 6)

  # Encode the zip file as an array of unsigned ints to keep the file smaller
  # and speed compilation time. Note that this works regardless of endian.
  ints = array.array('I', zipdata.getvalue())
  data_string = ['static const unsigned int kData[] = { 0x%x' % ints[0]]
  for value in ints[1:]:
    data_string.append(', 0x%x' % value)
  data_string.append(' };')
  source.write(wrapper.fill(''.join(data_string)))
  source.write('\n  return ::ion::base::ZipAssetManager::')
  source.write('RegisterAssetData(\n')
  source.write('      reinterpret_cast<const char*>(kData), sizeof(kData));\n')
  source.write('}\n\n')

  # Write function that will only register assets once.
  source.write('void RegisterAssetsOnce() {\n')
  source.write('  ION_STATIC_ONCE_CHECKED(RegisterAssets);\n')
  source.write('}\n\n')

  # Close namespace.
  source.write('}  // namespace %s\n' % asset_name)

  if disable_in_prod:
    source.write('#endif\n\n')

  source.close()


#------------------------------------------------------------------------------
def main():
  if len(sys.argv) < 2:
    print(('Usage: %s <asset definition file> <search path> '
           '[[<generated source file>] <constituent file ...>]' % sys.argv[0]) +
          '  All files are relative to the directory in which this is run.')
  else:
    # Save the full absolute path to the input and output files.
    cur_dir = os.getcwd()

    # Set up additional search paths.
    path_to_script = os.path.abspath(os.path.dirname(__file__))
    # Assume that the 'ion' directory is top-level.
    root_path = os.path.join(os.path.dirname(__file__), '..', '..')
    root_path = os.path.abspath(root_path)
    source_basedirs = []
    if len(sys.argv) > 4:
      source_basedirs = [
          os.path.dirname(os.path.abspath(source)) for source in sys.argv[4:]
      ]
    search_paths = [
        os.path.dirname(os.path.abspath(sys.argv[1])), root_path,
        os.path.abspath(sys.argv[2])
    ] + list(set(source_basedirs))

    asset_file_path = os.path.join(cur_dir, sys.argv[1])
    output_file_path = (
        os.path.join(cur_dir, sys.argv[3])
        if len(sys.argv) >= 4 else asset_file_path + '.cc')
    # Change to the directory containing the asset file so that relative paths
    # are correct.
    os.chdir(os.path.dirname(asset_file_path))
    GenerateZipAsset(asset_file_path, search_paths, output_file_path)


if __name__ == '__main__':
  main()
