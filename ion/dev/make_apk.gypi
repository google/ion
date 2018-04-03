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
# Make an APK target from an android target (shared_library).
#
# Used from 'make_into_app.gypi', which knows how to use this. Basically,
# this file needs a variable called 'make_this_target_into_an_app_param' which
# is the target being finalized.
#
# Making an APK also requires an additional variable to be defined for the
# target:
#
#   apk_package_name_param: the package name, e.g. com.google.ion.demo
#   apk_class_name_param: the main java class name (CamelCase). This class must
#                         be defined somewhere either in your template .java.in
#                         files or your static .java.
#
# This gypi knows how to handle APK-specific sources files defined in the
# target in the 'sources' list of the target:
#
#   AndroidManifest.xml.in   - (required) template for the AndroidManifest file.
#   build.xml.in             - (required) template for the ant build.xml file.
#   local.properties.in      - (required) template for local.properties.
#   project.properties       - (required) copy of project.properties.
#   proguard-project.txt     - (required) copy of proguard-project.txt.
#   strings.xml.in           - (required) template for res/values/strings.xml.
#   main.xml.in              - (required) template for res/layout/main.xml.
#   files ending in .java.in - template java files, copied to src/...
#   files ending in .java    - vanilla java files, copied to src/...
#
# Files ending with '.in' are templates, and are run through
# java_string_replacement_rules.gypi before ending up in the right place. All
# other files are copied directly to INTERMEDIATE_DIR.
#
# The .java files themselves are copied to
# src/<(apk_java_source_path)/<(class_name_lower)/
#
# A complete example:
#
#   {
#     'includes': [ '../../dev/make_apk.gypi' ],
#
#     'variables': {
#       'make_this_target_into_an_app_param': 'MyDemo',
#       'target_app_location_param': '<(PRODUCT_DIR)/demos',
#
#       'apk_package_name_param': 'com.mycompany.product',
#       'apk_class_name_param': 'MyDemo',
#     },
#
#     'conditions': [
#       ['OS == "android"', {
#         'sources': [
#           # All of these will be copied to the appropriate ant locations.
#           'android/AndroidManifest.xml.in',
#           'android/build.xml.in',
#           'android/local.properties.in',
#           'android/project.properties',
#           'android/proguard-project.txt',
#           'android/res/values/strings.xml.in',
#           'android/res/layout/main.xml.in',
#
#           # Will become: src/com/mycompany/product/SomeTemplate.java
#           'android/src/SomeTemplate.java.in',
#
#           # Will become: src/com/mycompany/product/Vanilla.java
#           'android/src/Vanilla.java',
#         ],
#       }],
#     ],  # conditions
#   },


{
  'target_name': '<(make_this_target_into_an_app_param)_apk',
  'dependencies': [ '<(make_this_target_into_an_app_param)', ],
  'type': 'none',
  'includes': [
    'ant.gypi',
  ],
  'variables': {
    'class_name_lower': '<!(<(python) -c "print \'<(apk_class_name_param)\'.lower()")',

    # All generated/copied java files will end up under this directory, e.g.,
    # src/com/google/ion/demo
    'apk_java_source_path': '<!(<(python) -c "import os; print \'<(apk_package_name_param)\'.replace(\'.\', os.sep)")',
  },

  'conditions': [
    ['[s for s in _sources if s.endswith(".java.in")]', {
      # If there are .java.in files that need to be built into the apk,
      # run them through java_string_replacement_rules and put them under
      # src/apk_java_source_path/class_name_lower.
      'actions': [
        {
          'action_name': 'gen_java_template',
          'includes': [ 'java_string_replacement_rules.gypi' ],

          'inputs': [
            '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'.java.in\')])" <@(_sources))',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/src/<(apk_java_source_path)/<(class_name_lower)/<(apk_class_name_param).java'
          ],
        },
      ],
    }],

    ['[s for s in _sources if s.endswith(".java")]', {
      # If there are vanilla .java files that need to be built into the apk,
      # copy them to src/apk_java_source_path/class_name_lower.
      'copies': [
        {
          'destination': '<(INTERMEDIATE_DIR)/src/<(apk_java_source_path)/<(class_name_lower)/',
          'files': [
            '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'.java\')])" <@(_sources))',
          ],
        },
      ],
    }],
  ],  # conditions

  'copies+': [
    {
      'destination': '<(INTERMEDIATE_DIR)',
      'files': [
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'project.properties\')])" <@(_sources))',
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'proguard-project.txt\')])" <@(_sources))',
      ],
    },
  ],

  'actions': [
    {
      'action_name': 'gen_manifest',
      'message': 'Generating AndroidManifest.xml',
      'includes': [ 'java_string_replacement_rules.gypi' ],

      'inputs': [
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if \'AndroidManifest.xml.in\' in s])" <@(_sources))',
      ],
      'outputs': [ '<(INTERMEDIATE_DIR)/AndroidManifest.xml'],
    },
    {
      'action_name': 'gen_build_xml',
      'message': 'Generating build.xml',
      'includes': [ 'java_string_replacement_rules.gypi' ],

      'inputs': [
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if \'build.xml.in\' in s])" <@(_sources))',
      ],
      'outputs': [ '<(INTERMEDIATE_DIR)/build.xml'],
    },
    {
      'action_name': 'gen_local_properties',
      'message': 'Generating local.properties',
      'includes': [ 'java_string_replacement_rules.gypi' ],

      'inputs': [
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'local.properties.in\')])" <@(_sources))',
      ],
      'outputs': [ '<(INTERMEDIATE_DIR)/local.properties'],
    },
    {
      'action_name': 'gen_strings_xml',
      'message': 'Generating res/values/strings.xml',
      'includes': [ 'java_string_replacement_rules.gypi' ],

      'inputs': [
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'strings.xml.in\')])" <@(_sources))',
      ],
      'outputs': [ '<(INTERMEDIATE_DIR)/res/values/strings.xml'],
    },
    {
      'action_name': 'gen_main_xml',
      'message': 'Generating res/layout/main.xml',
      'includes': [ 'java_string_replacement_rules.gypi' ],

      'inputs': [
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'main.xml.in\')])" <@(_sources))',
      ],
      'outputs': [ '<(INTERMEDIATE_DIR)/res/layout/main.xml'],
    },
    {
      'action_name': 'copy_apklib_so',
      'message': 'Copying <(SHARED_INTERMEDIATE_DIR)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)<(SHARED_LIB_SUFFIX) to <(SHARED_INTERMEDIATE_DIR)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib-unstripped<(SHARED_LIB_SUFFIX)',
      # This is a copy and a rename, so we can't put it in the 'copies' section.
      'action': [
        'cp', '-fv',
        '<(SHARED_INTERMEDIATE_DIR)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)<(SHARED_LIB_SUFFIX)',
        '<(SHARED_INTERMEDIATE_DIR)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib-unstripped<(SHARED_LIB_SUFFIX)',
      ],

      'inputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)<(SHARED_LIB_SUFFIX)',
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib-unstripped<(SHARED_LIB_SUFFIX)',
      ],
    },
    {
      # Strip the shared lib before building it into the apk.
      'action_name': 'strip_apklib_so',
      'message': 'Stripping .so to <(INTERMEDIATE_DIR)/libs/<(arch_tag)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib<(SHARED_LIB_SUFFIX)',

      'action': [
        '<(android_strip)',
        '<(SHARED_INTERMEDIATE_DIR)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib-unstripped<(SHARED_LIB_SUFFIX)',
        '-o',
        '<(INTERMEDIATE_DIR)/libs/<(arch_tag)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib<(SHARED_LIB_SUFFIX)'
      ],

      'inputs': [
        '<(SHARED_INTERMEDIATE_DIR)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib-unstripped<(SHARED_LIB_SUFFIX)',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)/libs/<(arch_tag)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib<(SHARED_LIB_SUFFIX)',
      ],
    },
    {
      'action_name': 'ant_clean_debug',
      'message': 'Running "ant clean debug"',

      'inputs': [
        '<(ant)',
        '<(INTERMEDIATE_DIR)/libs/<(arch_tag)/<(SHARED_LIB_PREFIX)<(make_this_target_into_an_app_param)_apklib<(SHARED_LIB_SUFFIX)',

        # In addition to the .so, depend on the java files so that the apk is
        # rebuilt if they change.
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'.java\')])" <@(_sources))',
        '<!@(<(python) -c "import sys; print \' \'.join([s for s in sys.argv if s.endswith(\'.java.in\')])" <@(_sources))',

        '<(INTERMEDIATE_DIR)/project.properties',
        '<(INTERMEDIATE_DIR)/proguard-project.txt',
      ],
      'outputs': [
        '<(INTERMEDIATE_DIR)/bin/<(apk_class_name_param)-debug.apk',
      ],
      'action': [
        '<(ant)',
        '<(ant_args)',
        '-f',
        '<(INTERMEDIATE_DIR)/build.xml',
        'clean',
        'debug',
      ],
    },
    {
      # Copy the built apk into <(target_app_location_param), with the lower
      # case filename.
      'action_name': 'copy_apk',
      'message': 'Copying .apk to <(target_app_location_param)/<(make_this_target_into_an_app_param)-debug.apk',

      'inputs': [
        '<(INTERMEDIATE_DIR)/bin/<(apk_class_name_param)-debug.apk',
      ],
      'outputs': [
        '<(target_app_location_param)/<(make_this_target_into_an_app_param)-debug.apk',
      ],
      'action': [
        'cp', '-v',
        '<(INTERMEDIATE_DIR)/bin/<(apk_class_name_param)-debug.apk',
        '<(target_app_location_param)/<(make_this_target_into_an_app_param)-debug.apk',
      ],
    },
  ]
}
