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
    '../common.gypi',
  ],  # includes
  'variables': {
    'gurl_src_dir': '<(third_party_dir)/googleurl/src',
  },

  'targets': [
    {
      'target_name': 'iongurl',
      'type': 'static_library',
      'include_dirs+': [
        # Needed for base/string16.h, etc.
        '<(ion_dir)/external/gurl_adapter/src',
        '<(third_party_dir)/googleurl/src',
      ],
      'dependencies': [
        # For the logging stuff below.
        '<(ion_dir)/base/base.gyp:ionbase',
      ],
      'conditions': [
        ['OS == "linux"', {
          'cflags_cc': [
            '-Wno-unused-function',
          ],
          # Avoid conversion warnings in googleurl files we've pulled in.
          'cflags_cc!': [
            '-Wconversion',
          ],
        }],
        ['OS == "win"', {
          'defines': [
            'GURL_OS_WINDOWS',
          ],
          'msvs_disabled_warnings': [
            # Conversion from size_t to int.
            '4267',
          ],
        }],
        ['OS in ["ios", "mac"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-shorten-64-to-32',
            ],
          },
          'defines': [
            'GURL_OS_POSIX',
          ],
        }, { # else
          'conditions': [
            ['OS == "android"', {
              'defines': [
                'GURL_OS_ANDROID',
              ],
            }, { # else
              'defines': [
                'GURL_OS_POSIX',
              ],
            }],
          ],  # conditions
        }],
      ],  # conditions
      'sources': [
        '<(gurl_src_dir)/base/strings/string_piece.cc',
        '<(gurl_src_dir)/base/strings/string_util.cc',
        '<(gurl_src_dir)/base/strings/utf_string_conversion_utils.cc',
        '<(gurl_src_dir)/base/third_party/icu/icu_utf.cc',
        '<(gurl_src_dir)/url/gurl.cc',
        '<(gurl_src_dir)/url/origin.cc',
        '<(gurl_src_dir)/url/scheme_host_port.cc',
        '<(gurl_src_dir)/url/third_party/mozilla/url_parse.cc',
        '<(gurl_src_dir)/url/url_canon_etc.cc',
        '<(gurl_src_dir)/url/url_canon_filesystemurl.cc',
        '<(gurl_src_dir)/url/url_canon_fileurl.cc',
        '<(gurl_src_dir)/url/url_canon_host.cc',
        # Replaced with adapter code, we don't use ICU.
        # '<(gurl_src_dir)/url/url_canon_icu.cc',
        '<(gurl_src_dir)/url/url_canon_internal.cc',
        '<(gurl_src_dir)/url/url_canon_ip.cc',
        '<(gurl_src_dir)/url/url_canon_mailtourl.cc',
        '<(gurl_src_dir)/url/url_canon_path.cc',
        '<(gurl_src_dir)/url/url_canon_pathurl.cc',
        '<(gurl_src_dir)/url/url_canon_query.cc',
        '<(gurl_src_dir)/url/url_canon_relative.cc',
        '<(gurl_src_dir)/url/url_canon_stdstring.cc',
        '<(gurl_src_dir)/url/url_canon_stdurl.cc',
        '<(gurl_src_dir)/url/url_constants.cc',
        '<(gurl_src_dir)/url/url_parse_file.cc',
        '<(gurl_src_dir)/url/url_util.cc',
      ],
      'all_dependent_settings': {
        'include_dirs+': [
          # Needed for base/string16.h, etc.
          '<(ion_dir)/external/gurl_adapter/src',
          '<(third_party_dir)/googleurl/src',
        ],
        'conditions': [
          ['OS == "win"', {
            'defines': [
              'GURL_OS_WINDOWS',
            ],
          }, { # else
            'conditions': [
              ['OS == "andorid"', {
                'defines': [
                  'GURL_OS_ANDROID',
                ],
              }, { # else
                'defines': [
                  'GURL_OS_POSIX',
                ],
              }],
            ],  # conditions
          }],
        ],  # conditions
      },  # all_dependent_settings
    },  # target: iongurl

    {
      'target_name': 'iongurl_adapter',
      'dependencies': [
        'iongurl',
      ],
      'type': 'static_library',
      'sources': [
        'gurl_adapter/src/base/strings/string16.cc',
        'gurl_adapter/src/base/strings/string16.h',
        'gurl_adapter/src/build/build_config.h',
        'gurl_adapter/src/url/url_canon_icu.cc',
      ],
    },  # target: iongurl_adapter
  ],  # targets
}
