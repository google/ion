#
# Copyright 2016 Google Inc. All Rights Reserved.
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
# ALWAYS refer to the targets in this file with the same path type (absolute vs
# relative).

{
  'includes' : [
    '../common.gypi',
    'external_common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      ['OS in ["linux", "android"]', {
        # Various third-party libraries result in warnings.
        'cflags_cc': [
          '-Wno-unused-variable',
        ],
        'cflags_cc!': [
          '-Wconversion',
        ],
      }],
      ['OS in ["mac", "ios"]', {
        'xcode_settings': {
          'OTHER_CFLAGS': [
            '-Wno-conversion',
            '-Wno-sign-compare',
           ],
        },
      }],  # mac or ios

      [ 'OS == "nacl"', {
        'cflags!': [
          '-include ion/port/nacl/override/aligned_malloc.h',
        ],
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'ionb64',
      'type': 'static_library',
      'sources': [
        '../../third_party/omaha/omaha/base/security/b64.c',
        '../../third_party/omaha/omaha/base/security/b64.h',
      ],
    }, # target: ionb64

    {
      'target_name': 'iontess',
      'type': 'static_library',
      'defines': [
        'STDC',
      ],
      'include_dirs': [
        'tess',
      ],
      'conditions': [
        ['OS == "windows"', {
          'msvs_disabled_warnings': [
            '4244',   # Conversion from __int64 to int [64-bit builds].
          ],
        }],
      ],
      'dependencies': [
        'graphics',
      ],
      'sources': [
        '../../third_party/GL/glu/src/libtess/dict.c',
        '../../third_party/GL/glu/src/libtess/dict.h',
        '../../third_party/GL/glu/src/libtess/geom.c',
        '../../third_party/GL/glu/src/libtess/geom.h',
        '../../third_party/GL/glu/src/libtess/memalloc.c',
        '../../third_party/GL/glu/src/libtess/memalloc.h',
        '../../third_party/GL/glu/src/libtess/mesh.c',
        '../../third_party/GL/glu/src/libtess/mesh.h',
        '../../third_party/GL/glu/src/libtess/normal.c',
        '../../third_party/GL/glu/src/libtess/normal.h',
        '../../third_party/GL/glu/src/libtess/priorityq.c',
        '../../third_party/GL/glu/src/libtess/priorityq.h',
        '../../third_party/GL/glu/src/libtess/render.c',
        '../../third_party/GL/glu/src/libtess/render.h',
        '../../third_party/GL/glu/src/libtess/sweep.c',
        '../../third_party/GL/glu/src/libtess/sweep.h',
        '../../third_party/GL/glu/src/libtess/tess.c',
        '../../third_party/GL/glu/src/libtess/tess.h',
        '../../third_party/GL/glu/src/libtess/tessmono.c',
        '../../third_party/GL/glu/src/libtess/tessmono.h',
      ],
    },  # target: iontess

    {
      'target_name' : 'ionmongoose',
      'type': 'static_library',
      'sources': [
        '../../third_party/mongoose/mongoose.c',
        '../../third_party/mongoose/mongoose.h',
      ],
      'defines': [
        'USE_WEBSOCKET',
        'NO_SSL',
        'DEBUG_TRACE',
      ],
      'conditions': [
        ['OS == "linux"', {
          'cflags': [
            '-Wno-deprecated-declarations',
          ],
        }],

        ['OS=="asmjs"', {
          # sys/ioctl.h defines SO_RCVTIMEO and SO_SNDTIMEO. This directive
          # instructs emcc to include its own internal version of sys/ioctl.h,
          # not the local system version.
          'cflags': [
            '-include sys/ioctl.h',
          ],
          # emscripten does not define SHUT_WR, which is defined to 1 on Linux.
          'defines': [
            'SHUT_WR=1'
          ],
        }],

        ['OS=="windows"', {
          'msvs_disabled_warnings': [
            # Conversion from int64 to int.
            '4244',
            # Conversion from size_t to int.
            '4267',
          ]
        }]
      ],
    },  # target: ionmongoose

    {
      'target_name' : 'ioneasywsclient',
      'type': 'static_library',
      'sources': [
        '../../third_party/easywsclient/easywsclient.cpp',
        '../../third_party/easywsclient/easywsclient.hpp',
      ],
      'conditions': [
        ['OS=="windows"', {
          'msvs_disabled_warnings': [
            # Conversion from int64 to int.
            '4244',
            # Conversion from size_t to int.
            '4267',
          ]
        }]
      ],
    },  # target: ioneasywsclient

    {
      'target_name': 'ionopenctm',
      'type': 'static_library',
      'defines': [
        'OPENCTM_NO_CPP',  # Disable exceptions in headers.
      ],
      'include_dirs': [
        '../../third_party/openctm/files/lib',
        '../../third_party/openctm/files/tools',
        '../../third_party/openctm/files/tools/rply',
        '../../third_party/tinyxml',
      ],
      'all_dependent_settings': {
        'defines': [
          'OPENCTM_NO_CPP',  # Disable exceptions in headers.
        ],
        'include_dirs': [
          '../../third_party/openctm/files/lib',
        ],
      },
      'conditions': [
        ['OS == "windows"', {
          'defines': [
            '_CRT_SECURE_NO_WARNINGS',
          ],
          'msvs_disabled_warnings': [
            '4244',  # Conversion from __int64 to int [64-bit builds].
            '4267',  # Conversion from size_t to long [64-bit builds].
          ],
        }, { # else
          'includes': [
            '../dev/exceptions.gypi',
          ],
        }],
      ],  # conditions
      'sources': [
        '../../third_party/openctm/files/tools/3ds.cpp',
        '../../third_party/openctm/files/tools/3ds.h',
        '../../third_party/openctm/files/tools/common.cpp',
        '../../third_party/openctm/files/tools/common.h',
        '../../third_party/openctm/files/tools/dae.cpp',
        '../../third_party/openctm/files/tools/dae.h',
        '../../third_party/openctm/files/tools/lwo.cpp',
        '../../third_party/openctm/files/tools/lwo.h',
        '../../third_party/openctm/files/tools/mesh.cpp',
        '../../third_party/openctm/files/tools/mesh.h',
        '../../third_party/openctm/files/tools/obj.cpp',
        '../../third_party/openctm/files/tools/obj.h',
        '../../third_party/openctm/files/tools/off.cpp',
        '../../third_party/openctm/files/tools/off.h',
        '../../third_party/openctm/files/tools/ply.cpp',
        '../../third_party/openctm/files/tools/ply.h',
        '../../third_party/openctm/files/tools/rply/rply.c',
        '../../third_party/openctm/files/tools/rply/rply.h',
        '../../third_party/openctm/files/tools/stl.cpp',
        '../../third_party/openctm/files/tools/stl.h',
        '../../third_party/openctm/files/tools/wrl.h',
        '../../third_party/tinyxml/tinystr.cpp',
        '../../third_party/tinyxml/tinystr.h',
        '../../third_party/tinyxml/tinyxml.cpp',
        '../../third_party/tinyxml/tinyxml.h',
        '../../third_party/tinyxml/tinyxmlerror.cpp',
        '../../third_party/tinyxml/tinyxmlparser.cpp',
      ],
    },  # target: ionopenctm

    {
      'target_name': 'ionlodepnglib',
      'type': 'static_library',
      'sources': [
        '../../third_party/lodepng/lodepng.cpp',
        '../../third_party/lodepng/lodepng.h',
      ],
      'defines': [
        # Ion only needs the in-memory decoder.  STB handles encode duties.
        'LODEPNG_NO_COMPILE_ENCODER',
        'LODEPNG_NO_COMPILE_DISK',
        'LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS',
        'LODEPNG_NO_COMPILE_ERROR_TEXT',
        'LODEPNG_NO_COMPILE_CPP',
      ],
      'conditions': [
        ['OS == "android"', {
          'cflags': [
             '-Wno-unused-but-set-variable',
           ],
        }],
        ['OS == "windows"', {
          'msvs_disabled_warnings': [
            '4244',   # Conversion from __int64 to int [64-bit builds].
            '4267',   # Conversion from size_t to int [64-bit builds].
          ],
        }],
      ],
    },  # target: ionlodepnglib

    {
      'target_name': 'ionstblib',
      'type': 'static_library',
      'sources': [
        '../../util/stb_image.c',
        '../../third_party/stblib/stb_image.h',
        '../../util/stb_image_write.c',
        '../../third_party/stblib/stb_image_write.h',
      ],
    },  # target: ionstblib

    {
      'target_name' : 'ionzlib',
      'type': 'static_library',
      'sources' : [
        '../../third_party/zlib/src/contrib/minizip/ioapi.c',
        '../../third_party/zlib/src/contrib/minizip/ioapi.h',
        '../../third_party/unzip/unzip.c',
        '../../third_party/unzip/unzip.h',
        '../../third_party/zlib/src/contrib/minizip/zip.c',
        '../../third_party/zlib/src/contrib/minizip/zip.h',
        '../../third_party/zlib/src/adler32.c',
        '../../third_party/zlib/src/compress.c',
        '../../third_party/zlib/src/crc32.c',
        '../../third_party/zlib/src/crc32.h',
        '../../third_party/zlib/src/deflate.c',
        '../../third_party/zlib/src/deflate.h',
        '../../third_party/zlib/src/inffast.c',
        '../../third_party/zlib/src/inffast.h',
        '../../third_party/zlib/src/inflate.c',
        '../../third_party/zlib/src/inflate.h',
        '../../third_party/zlib/src/inftrees.c',
        '../../third_party/zlib/src/inftrees.h',
        '../../third_party/zlib/src/trees.c',
        '../../third_party/zlib/src/trees.h',
        '../../third_party/zlib/src/uncompr.c',
        '../../third_party/zlib/src/zlib.h',
        '../../third_party/zlib/src/zconf.h',
        '../../third_party/zlib/src/zutil.c',
        '../../third_party/zlib/src/zutil.h',
      ],
      'defines': [
        'NOCRYPT=1',
        'NOUNCRYPT=1',
        'STDC',
        'NOCRYPT=1'
        'NOUNCRYPT=1'
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '../../third_party/zlib/src/',
        ],
      },
      'conditions' : [
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-parentheses-equality',
              '-Wno-incompatible-pointer-types',
              '-Wno-dangling-else',
             ],
          },
        }],
        ['OS == "ios"', {
          'xcode_settings': {
            'GCC_WARN_INHIBIT_ALL_WARNINGS' : 'YES',
          },
        }],
        ['OS in ["android", "mac", "linux"]', {
          'cflags': [
             '-Wno-parentheses',      # Ambiguous-looking if-else statements.
             '-Wno-unused-function',  # Func declared static but never defined.
             '-w',  # Turn on other warnings.
          ],
        }],
        ['OS == "android"', {
          'defines': [
            'USE_FILE32API=1',
          ],
        }],
        ['OS == "windows"', {
          'msvs_disabled_warnings': [
            '4018',  # '>' : signed/unsigned mismatch.
            '4267',  # Conversion from size_t to long [64-bit builds].
          ],
        }],
      ],
    },  # target: ionzlib

    {
      'target_name': 'graphics',
      'type': 'none',
      'link_settings': {
        'conditions': [
          ['OS == "windows"', {
            'libraries': [
              '-lgdi32',
              '-luser32',
            ],
            'conditions': [
              ['angle', {
                'libraries': [
                  '-llibEGL',
                  '-llibGLESv2',
                ],
              }, { # else
                'libraries': [
                  '-lopengl32',
                ],
              }],
            ],  # conditions
          }],
          ['OS == "mac"', {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AGL.framework',
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          }],
          ['OS == "ios"', {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/OpenGLES.framework',
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
              '$(SDKROOT)/System/Library/Frameworks/UIKit.framework',
            ],
          }],
          ['OS in ["android", "qnx"]', {
            'libraries': [
              '-lEGL',
              '-lGLESv2',
            ],
          }],
          ['OS == "nacl"', {
            'libraries': [
              '-lppapi_gles2',
              '-lppapi',
              '-lppapi_cpp',
              '-lppapi_cpp_private',
            ],
          }],
          ['OS == "linux"', {
            'libraries': [
              '-lGL',
              '-lX11',
            ],
          }],
        ],
      },  # link_settings

      'all_dependent_settings': {
        'conditions': [
          ['OS == "windows"', {
            'VCCLCompilerTool': {
              'ExceptionHandling': '1',
            },
            'msvs_disabled_warnings': [
              '4099',   # Type struct reused as class.
            ],
            'all_dependent_settings': {
              'msvs_disabled_warnings': [
                '4099',   # Type struct reused as class.
              ],
            },
            'conditions': [
              ['angle or ogles20', {
                'include_dirs': [
                  # TODO(user): Is this set anyhwere?
                  #'<(ANGLE_SOURCE)/include',
                ],
              }],
            ],  # conditions
          }],
          ['OS in ["linux", "windows"] and not angle', {
            'include_dirs': [
              '../../third_party/GL/gl/include',
            ],
          }],
        ],
      },  # all_dependent_settings
    },  # target: graphics

    {
      'target_name': 'ionjsoncpp',
      'type': 'static_library',
      'sources': [
        '../../third_party/jsoncpp/src/lib_json/json_reader.cpp',
        '../../third_party/jsoncpp/src/lib_json/json_value.cpp',
        '../../third_party/jsoncpp/src/lib_json/json_writer.cpp',
      ],
      'include_dirs': [
        '../../third_party/jsoncpp/include',
      ],
      'defines': [
        'JSON_USE_EXCEPTION=0',
      ],
    },  # target: ionjsoncpp
  ],

  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'includes' : [
            '../dev/os.gypi',
          ],
          'cflags': [
             '-Wno-unused-function',
             '-Wno-unused-variable',
          ],
          'target_name': 'ionandroid_cpufeatures',
          'type': 'static_library',
          'sources': [
              '<(android_ndk_sources_dir)/android/cpufeatures/cpu-features.c',
              '<(android_ndk_sources_dir)/android/cpufeatures/cpu-features.h',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              '<(android_ndk_sources_dir)/android/cpufeatures',
            ],
          },
        },  # target: ionandroid_cpufeatures
      ],
    }],
  ],
}
