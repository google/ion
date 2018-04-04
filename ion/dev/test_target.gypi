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
# Intended to be included within test targets.
{
  'type': 'executable',
  'conditions' : [

    # but Xcode will then not find the app, looking for it in PRODUCT_DIR
    # itself. Investigate in Xcode generator.
    ['GENERATOR != "xcode" and OS != "ios"', {
      'product_dir': '<(PRODUCT_DIR)/tests',
    }],

    ['OS == "mac"', {
      'xcode_settings' : {
        'INFOPLIST_FILE' : '<(ion_dir)/demos/mac/info.plist',
      },
    }],

    ['OS == "asmjs"', {
      'product_extension': 'js',
      'ldflags': [
        # Disable memory init file (.js.mem alongside .js file) for tests,
        # since nodejs can't load the file if it's not in the cwd. This is the
        # default for -O0/1 but sometimes it's useful to run tests with higher
        # opt levels (-c opt, -c prod).
        '--memory-init-file 0',
      ],
    }],

    ['OS == "ios"', {
      'mac_bundle' : 1,
      'variables': {
        # Tests can have an invalid character in their name ("_"), so replace
        # that here. I have no qualms about using tr, since this will always
        # run on a mac.
        'fixed_name': '<!(echo <(_target_name) | tr _ - )',
        'ios_deployment_target%': '6.0',
      },
      'xcode_settings': {
        'INFOPLIST_FILE': '<(ion_dir)/demos/ios/Info-gyp.plist',
        'TARGETED_DEVICE_FAMILY': '1,2',
        'IPHONEOS_DEPLOYMENT_TARGET': '<(ios_deployment_target)',
        'BUNDLE_IDENTIFIER': 'com.google.test.<(fixed_name)',
      },
    }],

    ['OS == "win"', {
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1',  # console, since gtest binaries are console.
        },
      },

      'variables': {
        'windows_dlls_destination_param': '<(PRODUCT_DIR)/tests',
      },
      'includes': [
        'copy_windows_dlls_action.gypi',
      ],
    }],  # conditions
  ],
}
