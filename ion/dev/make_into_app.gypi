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
# This gypi is responsible for turning a target into a runnable bundle,
# application, app, launchable, or whatever else, if a platform needs it.
#
# Examples are turning a pnacl build result into a .pexe, turning a target
# into an .apk (android), etc. This is not needed too often, but certain
# platforms require it.
#
# To use, from the 'targets' section of your .gyp file:
#    {
#      # This will do the right things to get a runnable "thing", depending on
#      # platform.
#      'variables': {
#         'make_this_target_into_an_app_param': 'your_target', },
#         'target_app_location_param': '<(where you want the thing to end up)', },
#      'includes': [ '../../../ion/dev/make_into_app.gypi' ],
#    },
#
# Where 'your_target' is defined as a target *in the same 'targets' section*
# where you are using this. The app bundle will be dependent on 'your_target'.
#
# Some platforms might require some extra sources/configuration. Define those
# as appropriate (see e.g. make_apk.gypi).
{
  'sources': [ ],
  'conditions': [
    ['OS == "nacl" and flavor == "pnacl"', {
      'includes': [ 'pnacl_finalize.gypi' ],

    }, { # else
      'conditions': [
        ['OS == "android"', {
          'includes': [ 'make_apk.gypi' ],
        }, {  # else
          # This OS/flavor does not need to make this an app, launchable,
          # bundle, whatever. Add a dummy target_name here just to complete the
          # including target rule.
          #
          # This means a dummy target will exist, but won't do anything.
          'target_name': '__<(make_this_target_into_an_app_param)_ignore',
          'type': 'none',
          'sources=': [ ],
        }],
      ],  # conditions
    }],
  ],  # conditions
}
