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
# ALWAYS refer to the targets in this file with the same path type (absolute vs
# relative).

{
  'includes' : [
    '../common.gypi',
    'external_common.gypi',
  ],
  'target_defaults': {
    'conditions': [
      # Attempting to insert mac/ios OTHER_CFLAGS changes here does not work due
      # to arcane rules concerning merging:
      #   https://gyp.gsrc.io/docs/InputFormatReference.md#Merge-Basics
      # and order of evaluation (specifically for included files):
      #   https://gyp.gsrc.io/docs/InputFormatReference.md#Processing-Order
      ['OS in ["linux", "android"]', {
        # Various third-party libraries result in warnings.
        'cflags_cc': [
          '-Wno-unused-variable',
        ],
        'cflags_cc!': [
          '-Wconversion',
        ],
      }],
      [ 'OS == "nacl"', {
        'cflags!': [
          '-include ion/port/nacl/override/aligned_malloc.h',
        ],
      }],
    ],
  },

  'targets': [
    {
      'target_name': 'ionabsl',
      'type': 'static_library',
      'sources': [
        '../../../../absl/base/casts.h',
        '../../../../absl/base/config.h',
        '../../../../absl/base/macros.h',
        '../../../../absl/base/policy_checks.h',
        '../../../../absl/memory/memory.h',
        '../../../../absl/meta/type_traits.h',
        '../../third_party/google/absl/base/intergral_types.h',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(third_party_dir)/google',
          '<(third_party_dir)/absl',
        ],
      },
    }, # target: ionabsl

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
        ['OS == "win"', {
          'msvs_disabled_warnings': [
            '4244',   # Conversion from __int64 to int [64-bit builds].
          ],
        }],
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
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

        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
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

        ['OS=="win"', {
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
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
        }],
        ['OS=="win"', {
          'msvs_disabled_warnings': [
            # Conversion from int64 to int.
            '4244',
            # Conversion from size_t to int.
            '4267',
          ],
          # This file must be built without UNICODE, since defining that
          # breaks the POSIX function gai_strerror - it returns const wchar_t*
          # instead of const char*
          'defines!': [
            'UNICODE=1'
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
        '../../third_party/openctm/lib',
        '../../third_party/openctm/tools',
        '../../third_party/openctm/tools/rply',
        '../../third_party/tinyxml2',
      ],
      'all_dependent_settings': {
        'defines': [
          'OPENCTM_NO_CPP',  # Disable exceptions in headers.
        ],
        'include_dirs': [
          '../../third_party/openctm/lib',
        ],
      },
      'conditions': [
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
        }],
        ['OS == "win"', {
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
        '../../third_party/openctm/tools/3ds.cpp',
        '../../third_party/openctm/tools/3ds.h',
        '../../third_party/openctm/tools/common.cpp',
        '../../third_party/openctm/tools/common.h',
        '../../third_party/openctm/tools/dae.cpp',
        '../../third_party/openctm/tools/dae.h',
        '../../third_party/openctm/tools/lwo.cpp',
        '../../third_party/openctm/tools/lwo.h',
        '../../third_party/openctm/tools/mesh.cpp',
        '../../third_party/openctm/tools/mesh.h',
        '../../third_party/openctm/tools/obj.cpp',
        '../../third_party/openctm/tools/obj.h',
        '../../third_party/openctm/tools/off.cpp',
        '../../third_party/openctm/tools/off.h',
        '../../third_party/openctm/tools/ply.cpp',
        '../../third_party/openctm/tools/ply.h',
        '../../third_party/openctm/tools/rply/rply.c',
        '../../third_party/openctm/tools/rply/rply.h',
        '../../third_party/openctm/tools/stl.cpp',
        '../../third_party/openctm/tools/stl.h',
        '../../third_party/openctm/tools/wrl.h',
        '../../third_party/tinyxml2/tinyxml2.cpp',
        '../../third_party/tinyxml2/tinyxml2.h',
      ],
    },  # target: ionopenctm

    {
      'target_name': 'ionlodepnglib',
      'type': 'static_library',
      'sources': [
        '../../third_party/lodepng/lodepng.cpp',
        '../../third_party/lodepng/lodepng.h',
      ],
      'conditions': [
        ['OS == "android"', {
          'cflags': [
             '-Wno-unused-but-set-variable',
           ],
        }],
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
        }],
        ['OS == "win"', {
          'msvs_disabled_warnings': [
            '4244',   # Conversion from __int64 to int [64-bit builds].
            '4267',   # Conversion from size_t to int [64-bit builds].
            '4334',   # Result of 32-bit shift implicitly converted to 64 bits.
          ],
        }],
      ],
    },  # target: ionlodepnglib

    {
      'variables': {
        'ljt_src_dir': '<(root_dir)/third_party/libjpeg_turbo',
        'ljt_config_dir': '<(root_dir)/third_party/google/libjpeg_turbo',
      },
      'target_name': 'ionjpeg',
      'type': 'static_library',
      'sources': [
        '<(ljt_config_dir)/src/jconfig.h',
        '<(ljt_config_dir)/src/jconfigint.h',
        '../../third_party/libjpeg_turbo//config.h',
        '../../third_party/libjpeg_turbo//jaricom.c',
        '../../third_party/libjpeg_turbo//jcapimin.c',
        '../../third_party/libjpeg_turbo//jcapistd.c',
        '../../third_party/libjpeg_turbo//jcarith.c',
        '../../third_party/libjpeg_turbo//jccoefct.c',
        '../../third_party/libjpeg_turbo//jccolor.c',
        '../../third_party/libjpeg_turbo//jcdctmgr.c',
        '../../third_party/libjpeg_turbo//jchuff.c',
        '../../third_party/libjpeg_turbo//jchuff.h',
        '../../third_party/libjpeg_turbo//jcinit.c',
        '../../third_party/libjpeg_turbo//jcmainct.c',
        '../../third_party/libjpeg_turbo//jcmarker.c',
        '../../third_party/libjpeg_turbo//jcmaster.c',
        '../../third_party/libjpeg_turbo//jcomapi.c',
        '../../third_party/libjpeg_turbo//jcparam.c',
        '../../third_party/libjpeg_turbo//jcphuff.c',
        '../../third_party/libjpeg_turbo//jcprepct.c',
        '../../third_party/libjpeg_turbo//jcsample.c',
        '../../third_party/libjpeg_turbo//jcstest.c',
        '../../third_party/libjpeg_turbo//jctrans.c',
        '../../third_party/libjpeg_turbo//jdapimin.c',
        '../../third_party/libjpeg_turbo//jdapistd.c',
        '../../third_party/libjpeg_turbo//jdarith.c',
        '../../third_party/libjpeg_turbo//jdatadst.c',
        '../../third_party/libjpeg_turbo//jdatadst-tj.c',
        '../../third_party/libjpeg_turbo//jdatasrc.c',
        '../../third_party/libjpeg_turbo//jdatasrc-tj.c',
        '../../third_party/libjpeg_turbo//jdcoefct.c',
        '../../third_party/libjpeg_turbo//jdcoefct.h',
        '../../third_party/libjpeg_turbo//jdcolor.c',
        '../../third_party/libjpeg_turbo//jdct.h',
        '../../third_party/libjpeg_turbo//jddctmgr.c',
        '../../third_party/libjpeg_turbo//jdhuff.c',
        '../../third_party/libjpeg_turbo//jdhuff.h',
        '../../third_party/libjpeg_turbo//jdinput.c',
        '../../third_party/libjpeg_turbo//jdmainct.c',
        '../../third_party/libjpeg_turbo//jdmainct.h',
        '../../third_party/libjpeg_turbo//jdmarker.c',
        '../../third_party/libjpeg_turbo//jdmaster.c',
        '../../third_party/libjpeg_turbo//jdmaster.h',
        '../../third_party/libjpeg_turbo//jdmerge.c',
        '../../third_party/libjpeg_turbo//jdphuff.c',
        '../../third_party/libjpeg_turbo//jdpostct.c',
        '../../third_party/libjpeg_turbo//jdsample.c',
        '../../third_party/libjpeg_turbo//jdsample.h',
        '../../third_party/libjpeg_turbo//jdtrans.c',
        '../../third_party/libjpeg_turbo//jerror.c',
        '../../third_party/libjpeg_turbo//jerror.h',
        '../../third_party/libjpeg_turbo//jfdctflt.c',
        '../../third_party/libjpeg_turbo//jfdctfst.c',
        '../../third_party/libjpeg_turbo//jfdctint.c',
        '../../third_party/libjpeg_turbo//jidctflt.c',
        '../../third_party/libjpeg_turbo//jidctfst.c',
        '../../third_party/libjpeg_turbo//jidctint.c',
        '../../third_party/libjpeg_turbo//jidctred.c',
        '../../third_party/libjpeg_turbo//jinclude.h',
        '../../third_party/libjpeg_turbo//jmemmgr.c',
        '../../third_party/libjpeg_turbo//jmemnobs.c',
        '../../third_party/libjpeg_turbo//jmemsys.h',
        '../../third_party/libjpeg_turbo//jmorecfg.h',
        '../../third_party/libjpeg_turbo//jpegcomp.h',
        '../../third_party/libjpeg_turbo//jpegint.h',
        '../../third_party/libjpeg_turbo//jpeglib.h',
        '../../third_party/libjpeg_turbo//jpeg_nbits_table.h',
        '../../third_party/libjpeg_turbo//jquant1.c',
        '../../third_party/libjpeg_turbo//jquant2.c',
        '../../third_party/libjpeg_turbo//jsimddct.h',
        '../../third_party/libjpeg_turbo//jsimd.h',
        '../../third_party/libjpeg_turbo//jsimd_none.c',
        '../../third_party/libjpeg_turbo//jutils.c',
        '../../third_party/libjpeg_turbo//jversion.h',
      ],
      'include_dirs': [
        '<(ljt_src_dir)',
        '<(ljt_config_dir)',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '<(ljt_src_dir)',
          '<(ljt_config_dir)',
        ]
      },
      # copybara:oss-replace-end
      'conditions': [
        ['OS == "android"', {
          'cflags': [
             '-Wno-unused-but-set-variable',
           ],
        }],
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
        }],
        ['OS == "win"', {
          'msvs_disabled_warnings': [
            '4146',   # Unary minux operator applied to unsigned type.
            '4244',   # Conversion from __int64 to int [64-bit builds].
            '4267',   # Conversion from size_t to int [64-bit builds].
            '4334',   # Result of 32-bit shift implicitly converted to 64 bits.
          ],
        }],
      ],
    },  # target: ionjpeg

    {
      'target_name': 'ionstblib',
      'type': 'static_library',
      'sources': [
        '../../util/stb_image.c',
        '../../third_party/stblib/stb_image.h',
        '../../util/stb_image_write.c',
        '../../third_party/stblib/stb_image_write.h',
      ],
      'conditions': [
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
        }],
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
        'NO_THIRD_PARTY_ZLIB',
      ],
      'include_dirs': [
        '../../third_party/zlib/src/',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '../../third_party/zlib/src/',
        ],
        'defines': [
          'NO_THIRD_PARTY_ZLIB',
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
             '-Wno-implicit-function-declaration',  # Same as above.
             '-w',  # Turn on other warnings.
          ],
        }],
        ['OS == "android"', {
          'defines': [
            'USE_FILE32API=1',
          ],
        }],
        ['OS == "win"', {
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
          ['OS == "win"', {
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
                'conditions': [
                  # Only include opengl32 if we're not explicitly using GLES.
                  # There is no default GLES implementation to link in provided
                  # by the system; instead clients must link in their own
                  # implementation if they wish to use GLES on Windows without
                  # linking in ANGLE.
                  ['ogles20==0', {
                    'libraries': [
                      '-lopengl32',
                    ],
                  }]
                ]
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
          ['OS == "win"', {
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
          }],
          ['OS in ["linux", "win"] and not angle', {
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
      'conditions': [
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
        }],
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
