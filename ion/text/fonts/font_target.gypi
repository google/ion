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
# Generates a zipasset from a ttf file and a header file containing the
# necessary declarations for the ION_FONT macro to work.
#
# Every target in fonts.gyp should include this file.
{
  'hard_dependency': '1',
  'rules': [
    {
      'rule_name': 'generate_font_zipasset_and_header',
      'extension': 'ttf',
      'message': 'Generating zipasset and header for <(_target_name)',
      'action': [
        '<(python)', 'generate_font_zipasset_and_header.py',

        # The font name, e.g. 'roboto_regular'.
        '<(_target_name)',

        # The full path to the ttf file, e.g.:
        # '../../../third_party/webfonts/apache/roboto/Roboto-Regular.ttf'
        '<(RULE_INPUT_PATH)',

        # Intermediate directory to store the generated header and cc file.
        '<(genfiles_dir)',
      ],
      'outputs': [
        # A header containing the necessary declaration for the ION_FONT macro
        # to work with this font.
        '<(genfiles_dir)/<(_target_name).h',

        # The processed zipasset of the ttf file, ready for compilation.
        '<(genfiles_dir)/<(_target_name).cc',
      ],
      'process_outputs_as_sources': 1,
    },
  ]
}
