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
# Usage: include in a 'rules' or 'actions' block, like this:
#
#  'rules': [
#    {
#      'includes': [
#        'java_string_replacement_rules.gypi'
#      ],
#      'rule_name': 'replace_android_strings',
#      'extension': 'in',
#      'outputs': [ '<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT)'],
#    },
#  ],

{
  'variables': {
    'apk_package_name_jni' : '<!(<(python) -c "print \'<(apk_package_name_param)\'.replace(\'_\', \'_1\').replace(\'.\', \'_\')")',
  },

  'inputs': [
  ],

  'action': [
    '<(python)',
    '<(ion_dir)/dev/replace_strings.py',

    '--from=__class_name__',
    '--to=<(apk_class_name_param)',

    '--from=__app_name__',
    '--to=<(make_this_target_into_an_app_param)',

    '--from=__package_name__',
    '--to=<(apk_package_name_param)',

    '--from=__jni_name__',
    '--to=<(apk_package_name_jni)',

    '--from=__sdk_dir__',
    # The path to the sdk dir, as written in the build.xml file, should be absolute.
    '--to=<!(<(python) -c "import os.path; print os.path.realpath(\'<(android_sdk_dir)\')" )',

    '--output',
    '<@(_outputs)',
    '--input',
    '<(_inputs)',
  ],
}
