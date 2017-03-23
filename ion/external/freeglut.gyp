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
    'freeglut_rel_dir' : '../../third_party/freeglut/freeglut',
  },
  'targets' : [
    {
      'target_name' : 'freeglut',
      'type': 'static_library',
      'dependencies': [
        'external.gyp:graphics',
      ],
      'sources' : [
        '<(freeglut_rel_dir)/include/GL/freeglut.h',
        '<(freeglut_rel_dir)/include/GL/freeglut_ext.h',
        '<(freeglut_rel_dir)/include/GL/freeglut_std.h',
        '<(freeglut_rel_dir)/src/freeglut_callbacks.c',
        '<(freeglut_rel_dir)/src/freeglut_cursor.c',
        '<(freeglut_rel_dir)/src/freeglut_display.c',
        '<(freeglut_rel_dir)/src/freeglut_ext.c',
        '<(freeglut_rel_dir)/src/freeglut_font.c',
        '<(freeglut_rel_dir)/src/freeglut_font_data.c',
        '<(freeglut_rel_dir)/src/freeglut_gamemode.c',
        '<(freeglut_rel_dir)/src/freeglut_geometry.c',
        '<(freeglut_rel_dir)/src/freeglut_glutfont_definitions.c',
        '<(freeglut_rel_dir)/src/freeglut_init.c',
        '<(freeglut_rel_dir)/src/freeglut_input_devices.c',
        '<(freeglut_rel_dir)/src/freeglut_joystick.c',
        '<(freeglut_rel_dir)/src/freeglut_main.c',
        '<(freeglut_rel_dir)/src/freeglut_menu.c',
        '<(freeglut_rel_dir)/src/freeglut_misc.c',
        '<(freeglut_rel_dir)/src/freeglut_overlay.c',
        '<(freeglut_rel_dir)/src/freeglut_spaceball.c',
        '<(freeglut_rel_dir)/src/freeglut_state.c',
        '<(freeglut_rel_dir)/src/freeglut_stroke_mono_roman.c',
        '<(freeglut_rel_dir)/src/freeglut_stroke_roman.c',
        '<(freeglut_rel_dir)/src/freeglut_structure.c',
        '<(freeglut_rel_dir)/src/freeglut_teapot.c',
        '<(freeglut_rel_dir)/src/freeglut_videoresize.c',
        '<(freeglut_rel_dir)/src/freeglut_window.c',
        '<(freeglut_rel_dir)/src/freeglut_xinput.c',
      ],
      'include_dirs' : [
        'freeglut',
        '<(freeglut_rel_dir)/include',
        '<(freeglut_rel_dir)/src',
      ],
      'all_dependent_settings' : {
        'include_dirs' : [
          'freeglut',
          '<(freeglut_rel_dir)/include',
          '<(freeglut_rel_dir)/src',
        ],
        'defines': [
          # This is necessary so that symbol visibility is correctly handled.
          'FREEGLUT_STATIC',
        ],
      },  # all_dependent_settings
      'defines': [
        'FREEGLUT_STATIC',
      ],
      'defines!': [
        # Freeglut copiously prints out every event when _DEBUG is defined, so
        # undefine it.
        '_DEBUG',
        # Freeglut seems to be incompatible with UNICODE.
        'UNICODE=1'
      ],
      'conditions': [
        ['OS in ["linux", "mac"]', {
          'cflags': [
            '-Wno-int-to-pointer-cast',
            '-Wno-pointer-to-int-cast',
          ],
          'all_dependent_settings': {
            'cflags_cc': [
              '-Wno-mismatched-tags',
            ],
          },
        }],
        ['OS in ["mac", "ios"]', {
          'xcode_settings': {
            'OTHER_CFLAGS': [
              '-Wno-conversion',
             ],
          },
        }],  # mac or ios
        ['OS=="mac"', {
          'include_dirs': [
            '/usr/X11/include',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/AGL.framework',
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
              '/usr/X11/lib/libX11.dylib',
              '/usr/X11/lib/libGL.dylib',
              '/usr/X11/lib/libXext.dylib',
              '/usr/X11/lib/libXxf86vm.dylib',
              '/usr/X11/lib/libXrandr.dylib',
            ],
          },
          'defines': [
            # This prevents freeglut from using its hacky font definitions on OSX.
            '__CYGWIN__',
            'HAVE_DLFCN_H',
            'HAVE_ERRNO_H',
            'HAVE_FCNTL_H',
            'HAVE_GETTIMEOFDAY',
            'HAVE_GL_GLU_H',
            'HAVE_GL_GLX_H',
            'HAVE_GL_GL_H',
            'HAVE_INTTYPES_H',
            'HAVE_LIMITS_H',
            'HAVE_MEMORY_H',
            'HAVE_STDINT_H',
            'HAVE_STDLIB_H',
            'HAVE_STRINGS_H',
            'HAVE_STRING_H',
            'HAVE_SYS_IOCTL_H',
            'HAVE_SYS_PARAM_H',
            'HAVE_SYS_STAT_H',
            'HAVE_SYS_TIME_H',
            'HAVE_SYS_TYPES_H',
            'HAVE_UNISTD_H',
            'HAVE_VFPRINTF',
            'HAVE_VPRINTF',
            'HAVE_X11_EXTENSIONS_XF86VMODE_H',
            'HAVE_X11_EXTENSIONS_XINPUT_H',
            'HAVE_X11_EXTENSIONS_XI_H',
            'HAVE_X11_EXTENSIONS_XRANDR_H',
            'STDC_HEADERS',
            'TIME_WITH_SYS_TIME',
          ],
        }],
        ['OS=="linux"', {
          'defines' : [
#            '_DEBUG',
#            '_GNU_SOURCE',
            'HAVE_DLFCN_H',
            'HAVE_ERRNO_H',
            'HAVE_FCNTL_H',
            'HAVE_GETTIMEOFDAY',
            'HAVE_GL_GLU_H',
            'HAVE_GL_GLX_H',
            'HAVE_GL_GL_H',
            'HAVE_INTTYPES_H',
            'HAVE_LIBXI',
            'HAVE_LIMITS_H'
            'HAVE_MEMORY_H',
            'HAVE_STDINT_H',
            'HAVE_STDINT_H',
            'HAVE_STDLIB_H',
            'HAVE_STRINGS_H',
            'HAVE_STRING_H',
            'HAVE_SYS_IOCTL_H',
            'HAVE_SYS_PARAM_H',
            'HAVE_SYS_STAT_H',
            'HAVE_SYS_TIME_H',
            'HAVE_SYS_TYPES_H',
            'HAVE_UNISTD_H',
            'HAVE_VFPRINTF',
            'HAVE_VPRINTF',
            'STDC_HEADERS',
          ],
          'link_settings': {
            'libraries': [
              '-lrt',  # For clock_gettime.
            ],
          },  # link_settings
        }],

        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lWinmm',  # For time stuff.
              '-lAdvapi32',  # For registry stuff.
            ],
          },  # link_settings
          'defines': [
            'FREEGLUT_LIB_PRAGMAS=0',
          ],
          'all_dependent_settings': {
            'defines': [
              'FREEGLUT_LIB_PRAGMAS=0',
            ],
          },  # all_dependent_settings
          'defines!': [
            'NOGDI',
          ],
          'msvs_disabled_warnings': [
            '4311', # pointer truncation from 'void *' to 'int'
            '4312', # conversion from 'int' to 'void *' of greater size
          ],
        }],
      ],
    },
  ],
}
