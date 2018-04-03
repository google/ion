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
"""Ion build setup file.

This module contains functions that are used for building Ion and any
projects that use Ion.
"""


#------------------------------------------------------------------------------
def ComputeLibraryDependencies(direct_lib_dict):
  """Computes transitive closure of library dependencies.

  Args:
    direct_lib_dict: a dictionary containing direct library dependencies.

  Returns:
    A dictionary in which each entry key is a library name and the value is a
    list of all libraries on which that library depends on, in the proper link
    order.

  Raises:
    Exception: if a cycle is found.
  """
  ret_dict = {}
  for cur_lib in direct_lib_dict.keys():
    # Gather all libraries cur_lib depends on into all_libs.
    libs_to_process = [cur_lib]
    all_libs = []
    while libs_to_process:
      lib = libs_to_process.pop(0)
      if lib != cur_lib:
        all_libs.append(lib)
      if lib in direct_lib_dict:
        # Add all dependencies to the list to process if not already done.
        dep_libs = direct_lib_dict[lib]
        if lib in dep_libs:
          raise Exception('ComputeLibraryDependencies found a cycle at lib "' +
                          lib + '"')
        libs_to_process += [dep_lib for dep_lib in dep_libs
                            if dep_lib not in all_libs]
    ret_dict[cur_lib] = all_libs
  return ret_dict


#------------------------------------------------------------------------------
def TopologicalSort(d):
  """Returns a topologically-sorted list of nodes.

  Args:
    d: a dictionary representing a directed acyclic graph in which each
       key represents a node and its value is a list of nodes it is connected
       to.

  Returns:
    A topologically-sorted list of all nodes.

  Raises:
    Exception: if a cycle is found.
  """
  visited = []
  sorted_nodes = []
  visiting = []  # For detecting cycles.

  def _Search(key):
    if key not in visited:
      if key in visiting:
        raise Exception('TopologicalSort found a cycle at key "' + key + '"')
      visiting.append(key)
      for edge in d.get(key, []):
        _Search(edge)
      visited.append(key)
      sorted_nodes.append(key)
      visiting.remove(key)

  for key in d.keys():
    _Search(key)

  # The DFS puts the leaves in the list first, so reverse it.
  sorted_nodes.reverse()
  return sorted_nodes


#------------------------------------------------------------------------------
def LoadDependencyDictionary(filename):
  """Loads dependencies from the named file, returning a dictionary.

  Args:
    filename: name of the file to load the dictionary from.

  Returns:
    A dictionary in which each entry key is a library name and the value is a
    list of all libraries on which that library depends on.
  """
  return eval(open(filename, 'r').read())
