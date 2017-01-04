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
# Implements the rule to create zipasset source from an .iad file.
#
# How to use:
#  - include this .gypi in the includes section of your target.
#  - list the .iad file in your sources, with the extension, e.g.:
#      'sources' : [
#        'data/zipasset1.iad',
#        'data/zipasset2.iad',
#      ],
#  - for increased CPU utilization and decreased build time, it's best to keep
#    .iad files in separate distinct targets than your other .cc files. You can
#    put a bunch of .iad files in a single static_library and link that from
#    your "real" target library.
#
# Note that this file knows how to compute the dependencies of an iad file, and
# makes those files dependencies on the generated IAD .cc file. This means that
# any of the constituent files inside an IAD can change, and the change will be
# recognized, causing the generated IAD file to be rebuilt.

{

  'variables': {
    # Whittle down the list of sources for this target, which might include
    # other non-IAD files (although that's not a good idea; see How to use
    # point above).
    #
    # First we have to serialize the list of source files to a file, because
    # the list itself could be very long and not fit on a command line (i.e., on
    # Windows).
    'all_source_files': '<|(_all_source_files.txt <@(_sources))',
    # We add a fake dependency on _sources here to ensure that the variable
    # always gets regenerated on each inclusion.
    'iad_files_in_sources': [
      '<!@(<(python) -c "import sys, os; \
        dummy=\'<@(_sources)\'; \
        print \' \'.join([s for s in open(sys.argv[1]) if os.path.splitext(s.strip())[1] == \'.iad\'])" <(all_source_files))'
    ],

    # Run *all* IAD files in the target against zipasset_dependencies. That
    # tool parses out the constituent files and prints them out.
    'all_iad_asset_files': [
      '<!@(<(python) <(ion_dir)/dev/zipasset_dependencies.py --iads "<@(iad_files_in_sources)" --search_path . )'
    ],
  },

  'rules': [
    {
      'rule_name': 'generate_zip_assets',
      'extension': 'iad',
      'message': 'Generating from <(RULE_INPUT_PATH)',
      'outputs': [
        '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).cc'
      ],
      'inputs': [
        '<(ion_dir)/base/zipassetmanager.h',
        '<(ion_dir)/base/zipassetmanagermacros.h',
        '<(ion_dir)/dev/zipasset_generator.py',
        '<@(all_iad_asset_files)',
      ],
      'action': [
        '<(python)',
        '<(ion_dir)/dev/zipasset_generator.py',
        '<(RULE_INPUT_PATH)',
        '.',
        '<@(_outputs)',
      ],
      'process_outputs_as_sources': 1,
    }
  ],
}
