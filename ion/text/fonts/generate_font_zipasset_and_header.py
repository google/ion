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

"""Generates the .cc file and .h file for using a font with the ION_FONT macro.

The .cc file contains the zip data given a font (obtained using the
zipasset_generator module).

The .h file contains all the necessary declarations for the ION_FONT macro
to work.
"""

import os
import sys
sys.path.append('../../dev')
# pylint: disable=g-import-not-at-top
from zipasset_generator import GenerateZipAsset


def main():
  # The font name, e.g. 'roboto_regular'.
  font_name = sys.argv[1]

  # The full path to the ttf file, e.g.:
  # '../../../third_party/webfonts/apache/roboto/Roboto-Regular.ttf'
  font_path = sys.argv[2]

  # Intermediate directory to store the generated header and cc file.
  output_dir = sys.argv[3]
  if not os.path.exists(output_dir):
    os.makedirs(output_dir)

  # Generate iad file that wraps the font file.
  with open('template.iad.in') as iad_template_file:
    iad_text = iad_template_file.read()
  iad_text = iad_text.replace('__font_name__', font_name)
  iad_text = iad_text.replace('__file_name__', font_path)
  iad_output_path = os.path.join(output_dir, font_name + '.iad')
  with open(iad_output_path, 'w') as iad_out_file:
    iad_out_file.write(iad_text)

  # Based on the just generated iad file, generate the zipasset cc file.
  cc_output_path = os.path.join(output_dir, font_name + '.cc')
  GenerateZipAsset(iad_output_path, [], cc_output_path)

  # Generate the header file containing the necessary declarations for the
  # ION_FONT macro to work.
  with open('template.h.in') as h_template_file:
    iad_text = h_template_file.read()
  iad_text = iad_text.replace('__font_name__', font_name)
  h_output_path = os.path.join(output_dir, font_name + '.h')
  with open(h_output_path, 'w') as h_out_file:
    h_out_file.write(iad_text)

if __name__ == '__main__':
  main()
