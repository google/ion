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
# This file only gets included on Windows and Linux.

{
  'includes' : [
    '../common.gypi',
    'external_common.gypi',
  ],
  'variables' : {
    'freetype2_dir' : '../../third_party/freetype2',
    'freetype2_absdir' : '<(third_party_dir)/freetype2',
  },
  'targets' : [
    {
      'target_name' : 'ionfreetype2',
      'type': 'static_library',
      'sources' : [
        '<(freetype2_dir)/include/freetype.h',
        '<(freetype2_dir)/src/autofit/autofit.c',
        '<(freetype2_dir)/src/base/ftbase.c',
        '<(freetype2_dir)/src/base/ftbbox.c',
        '<(freetype2_dir)/src/base/ftbdf.c',
        '<(freetype2_dir)/src/base/ftbitmap.c',
        '<(freetype2_dir)/src/base/ftcid.c',
        '<(freetype2_dir)/src/base/ftdebug.c',
        '<(freetype2_dir)/src/base/ftfntfmt.c',
        '<(freetype2_dir)/src/base/ftfstype.c',
        '<(freetype2_dir)/src/base/ftgasp.c',
        '<(freetype2_dir)/src/base/ftglyph.c',
        '<(freetype2_dir)/src/base/ftgxval.c',
        '<(freetype2_dir)/src/base/ftinit.c',
        '<(freetype2_dir)/src/base/ftlcdfil.c',
        '<(freetype2_dir)/src/base/ftmm.c',
        '<(freetype2_dir)/src/base/ftotval.c',
        '<(freetype2_dir)/src/base/ftpatent.c',
        '<(freetype2_dir)/src/base/ftpfr.c',
        '<(freetype2_dir)/src/base/ftstroke.c',
        '<(freetype2_dir)/src/base/ftsynth.c',
        '<(freetype2_dir)/src/base/fttype1.c',
        '<(freetype2_dir)/src/base/ftwinfnt.c',
        '<(freetype2_dir)/src/bdf/bdf.c',
        # omit ftbzip2.c
        '<(freetype2_dir)/src/cache/ftcache.c',
        '<(freetype2_dir)/src/cff/cff.c',
        '<(freetype2_dir)/src/cid/type1cid.c',
        '<(freetype2_dir)/src/gxvalid/gxvalid.c',
        '<(freetype2_dir)/src/gzip/ftgzip.c',
        '<(freetype2_dir)/src/lzw/ftlzw.c',
        '<(freetype2_dir)/src/otvalid/otvalid.c',
        '<(freetype2_dir)/src/pcf/pcf.c',
        '<(freetype2_dir)/src/pfr/pfr.c',
        '<(freetype2_dir)/src/psaux/psaux.c',
        '<(freetype2_dir)/src/pshinter/pshinter.c',
        '<(freetype2_dir)/src/psnames/psnames.c',
        '<(freetype2_dir)/src/raster/raster.c',
        '<(freetype2_dir)/src/sfnt/sfnt.c',
        '<(freetype2_dir)/src/smooth/smooth.c',
        '<(freetype2_dir)/src/truetype/truetype.c',
        '<(freetype2_dir)/src/type1/type1.c',
        '<(freetype2_dir)/src/type42/type42.c',
        '<(freetype2_dir)/src/winfonts/winfnt.c',
      ],
      'include_dirs' : [
        '<(freetype2_absdir)/include',
        '<(freetype2_absdir)/include/freetype',
        '<(freetype2_absdir)/include/freetype/config',
      ],
      'all_dependent_settings' : {
        'include_dirs' : [
          '<(freetype2_dir)/include',
          '<(freetype2_dir)/include/freetype',
          '<(freetype2_dir)/include/freetype/config',
        ],
      },
      'defines': [
        'FT2_BUILD_LIBRARY',
        'FT_CONFIG_OPTIONS_H="ion/external/freetype2/ftoption.h"',
        'FT_CONFIG_CONFIG_H=<ftconfig.h>',
        'FT_CONFIG_MODULES_H=<ftmodule.h>',
        'HAVE_FSREF=0',
        'HAVE_UNISTD_H=1',
        'HAVE_FCNTL_H=1',
      ],
      'conditions': [
        ['OS in ["qnx", "nacl", "android", "linux", "asmjs"]', {
          'include_dirs': [
            '<(freetype2_dir)/builds/unix'
          ],
          'defines': [
            'HAVE_FCNTL_H=1',
          ],
          'sources': [
            '<(freetype2_dir)/builds/unix/ftsystem.c',
          ],
        }],
        ['OS in ["mac", "ios"]', {
          'sources': [
            '<(freetype2_dir)/src/base/ftsystem.c',
          ],
          'defines': [
            'DARWIN_NO_CARBON'
          ],
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
              '-Wno-sign-compare',
             ],
          },
        }],
        ['OS == "win"', {
          'sources': [
            '<(freetype2_dir)/src/base/ftsystem.c',
          ],
          'msvs_disabled_warnings': [
            '4018',  # Signed/unsigned mismatch for '>'.
            '4146',  # Unary minus applied to unsigned type.
            '4244',  # Conversion loses precision.
            '4267',  # Conversion loses data.
            '4312',  # Conversion from A to B of greater size.
          ],
        }],
        ['OS == "android"', {
          'cflags': [
            # Probably true for any modern GCC...Just turn off hyperactive
            # warnings in thirdparty code.
            '-Wno-unused-but-set-variable',
          ],
        }],
        ['OS == "nacl"', {
          'cflags!': [
            # freetype being C, the function defined in aligned_malloc.h gets
            # replicated across all objects. Remove it here since it is not
            # really needed for this library.
            '-include ion/port/nacl/override/aligned_malloc.h',
          ],
          'sources': [
            'freetype2/fcntl.c',
          ]
        }],
      ],
    },
  ],
}
