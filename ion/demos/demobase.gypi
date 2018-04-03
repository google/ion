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
  'type': 'executable',
  'mac_bundle' : 1,
  'dependencies' : [
    '<(ion_dir)/analytics/analytics.gyp:ionanalytics',
    '<(ion_dir)/base/base.gyp:ionbase',
    '<(ion_dir)/demos/demolib.gyp:iondemo',
    '<(ion_dir)/external/external.gyp:ionstblib',
    '<(ion_dir)/external/external.gyp:ionzlib',
    '<(ion_dir)/external/imagecompression.gyp:ionimagecompression',
    '<(ion_dir)/gfx/gfx.gyp:iongfx',
    '<(ion_dir)/gfxutils/gfxutils.gyp:iongfxutils',
    '<(ion_dir)/image/image.gyp:ionimage',
    '<(ion_dir)/math/math.gyp:ionmath',
    '<(ion_dir)/port/port.gyp:ionport',
    '<(ion_dir)/portgfx/portgfx.gyp:ionportgfx',
    '<(ion_dir)/profile/profile.gyp:ionprofile',
    '<(ion_dir)/remote/remote.gyp:ionremote',
    '<(ion_dir)/text/text.gyp:iontext',
  ],
  'defines': [
    '__class_name__=>(demo_class_name)',
  ],
  'conditions' : [

    # /demos, but Xcode will then not find the app, looking for it in
    # PRODUCT_DIR itself. Investigate in Xcode generator.
    ['OS not in ["ios", "mac"]', {
      'product_dir': '<(PRODUCT_DIR)/demos',
    }],

    ['OS not in ["nacl", "android", "ios", "mac"]', {
      'sources': [
        'demobase_glut.cc',
      ],
    }],

    [ 'OS == "nacl"', {
      'sources': [
        'demobase_nacl.cc',
      ],
    }],

    [ 'OS == "asmjs"', {
      'product_extension': 'js',
      'ldflags': [
        '-s EXPORTED_FUNCTIONS="[\'_main\', \'_malloc\', \'_IonRemoteGet\']"',
      ],
      'actions': [
        {
          'action_name': 'replace_demobase_strings',
          'inputs': [
            '<(ion_dir)/demos/demobase.html',
          ],
          'outputs': [
            '<(target_app_location_param)/<(_target_name).html',
          ],
          'action': [
            '<(python)',
            '<(ion_dir)/dev/replace_strings.py',

            '--from=SCRIPT_NAME',
            '--to=<(_target_name)',

            '--output',
            '<@(_outputs)',
            '--input',
            '<@(_inputs)',
          ],
        },
      ],
    }],

    ['OS not in ["android", "asmjs", "ios", "mac", "nacl"]', {
      # Asmjs has its own implementation of GLUT; android, iOS and Mac don't
      # need it.
      'dependencies' : [
        '<(ion_dir)/external/freeglut.gyp:freeglut',
      ],
    }],

    ['OS == "mac"', {
      'sources': [
        'mac/demoglview.h',
        'mac/demoglview.mm',
        'mac/appdelegate.h',
        'mac/appdelegate.mm',
        'mac/main.mm',
      ],
      'xcode_settings' : {
        'INFOPLIST_FILE' : '<(ion_dir)/demos/mac/info.plist',
      },
      'libraries': [
        '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
      ],
    }],

    ['OS == "ios"', {
      'sources' : [
        'demobase_ios.mm',
        'ios/src/main.mm',
        'ios/src/IonDemoAppDelegate.mm',
        'ios/src/IonGL2View.mm',
        'ios/src/IonViewController.mm'
      ],
      'xcode_settings': {
        'INFOPLIST_FILE': 'ios/Info-gyp.plist',
        'CLANG_ENABLE_OBJC_ARC': 'YES',
        'TARGETED_DEVICE_FAMILY': '1,2',
        'GOOGLE_VERSION_MAJOR': '1',
        'GOOGLE_VERSION_MINOR': '0',
        'GOOGLE_VERSION_FIXLEVEL': '0',
        'BUNDLE_IDENTIFIER': 'com.google.geo.>(_target_name)',
      },
    }],

    ['OS == "android"', {
      # On android, this target is destined for a shared library, which will be
      # included (see make_this_target_into_an_app_param.gypi) in an APK.
      'type': 'shared_library',

      # For string replacements in demobase_android.cc.
      'variables' : {
        'make_this_target_into_an_app_param': '', # unused
        'apk_package_name_param': 'com.google.ion.<(demo_class_name)',
        'apk_class_name_param': '<(demo_class_name)',
      },

      'product_dir=': '<(SHARED_INTERMEDIATE_DIR)',

      'sources' : [
        '<(INTERMEDIATE_DIR)/demobase_android.cc',
      ],

      'actions' : [
        {
          'action_name': 'gen_demobase',
          'message': 'Generating native stub',
          'includes': [ '../dev/java_string_replacement_rules.gypi' ],

          'inputs': [
            'demobase_android.cc',
          ],
          'outputs': [ '<(INTERMEDIATE_DIR)/demobase_android.cc'],
        },
      ],
    }],

    ['OS == "win"', {
      'libraries': [
        '-ladvapi32',
        '-lwinmm',
      ],
    }],
  ],
}
