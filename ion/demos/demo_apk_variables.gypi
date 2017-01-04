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
{
  'includes': [
    '../dev/make_into_app.gypi',
  ],

  # Making an APK for android requires a little more attention.
  'variables': {
    # This is the package under which the generated file below
    # (IonDemo.java.in) will be put.
    'apk_package_name_param': 'com.google.ion.<(apk_class_name_param)',
  },

  'conditions': [
    ['OS == "android"', {
      # These files will be treated correctly by make_apk.
      'sources': [
        '<(ion_dir)/dev/android/AndroidManifest.xml.in',
        '<(ion_dir)/dev/android/build.xml.in',
        '<(ion_dir)/dev/android/local.properties.in',
        '<(ion_dir)/dev/android/project.properties',
        '<(ion_dir)/dev/android/proguard-project.txt',
        '<(ion_dir)/dev/android/strings.xml.in',
        '<(ion_dir)/dev/android/main.xml.in',
        '<(ion_dir)/demos/android/IonDemo.java.in',
      ],
    }],
  ],
}
