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
"""Replace strings in a input file to produce an output file.

Use as:

  replace_strings.py --input file.in [--output path/to/file.out]  \
      --replacement_mapping file_containing_a_replacement_mapping

where file_containing_a_replacement_mapping is a file that looks like:

  {'FROM SOME STRING': 'TO SOME STRING',
   'remove_this_entirely': '',
   'foo': 'bar }

This file is essentially a python dict format, and is insensitive to whitespace.

Use this form if the strings your replacing contain spaces, or are otherwise
cumbersome to represent in the command line form, which looks like:

  replace_strings.py --input file.in [--output path/to/file.out]  \
      --from FROM_STRING --to TO_STRING --from REMOVE_ENTIRELY --to=

Note that the intermediate directories to --output will be created if needed.
If --output is not specified, results are written to standard output.

From gyp:

  'actions': [
    {
      'action_name': 'replace_strings',
      'inputs': [
        '<(google3_dir)/path/to/file.in',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/put/file/here/file.out',
      ],
      'action': [
        '<(python)',
        '<(ion_dir)/dev/replace_strings.py',

        '--replacement_mapping', 'file_containing_a_replacement_mapping',

        '--output',
        '<@(_outputs)',
        '--input',
        '<@(_inputs)',
      ],
    },
  ],

"""


import optparse
import os
import re
import sys


def main(argv):
  """Entry point.

  Args:
    argv: use sys.argv[1:]. See ArgumentParser below.
  """

  parser = optparse.OptionParser()
  parser.add_option('--input')
  parser.add_option('--output')
  parser.add_option('--replacement_mapping', default=None)
  parser.add_option('--from', action='append', default=[])
  parser.add_option('--to', action='append', default=[])

  options, _ = parser.parse_args(argv)

  replacement_mapping = {}
  if options.replacement_mapping is not None:
    with open(options.replacement_mapping, 'r') as m:
      replacement_mapping = eval(m.read())

  if options.output and not os.path.isdir(os.path.dirname(options.output)):
    os.makedirs(os.path.dirname(options.output))

  # We can't use options.input here, because 'input' is a python keyword.
  with open(getattr(options, 'input'), 'r') as input_:
    text = input_.read()

  for from_pattern, to_text in replacement_mapping.items():
    # Treat from_pattern as a regex, with re.DOTALL (meaning dot captures
    # newlines). To prevent . from being greedy, use a "?". E.g.:
    #
    # 'remove: {.*?}' will correctly handle:
    #
    # 'remove: { things we want removed }  { things we want to keep }'
    #
    # because the . stops at the first '}'. See:
    # https://docs.python.org/2/library/re.html#regular-expression-syntax
    text = re.sub(re.compile(from_pattern, re.DOTALL), to_text, text)

  for from_text, to_text in zip(getattr(options, 'from'), options.to):
    text = text.replace(from_text, to_text)

  if options.output:
    with open(options.output, 'w') as output:
      output.write(text)
  else:
    sys.stdout.write(text)


if __name__ == '__main__':
  main(sys.argv[1:])
