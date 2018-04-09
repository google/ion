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
  'includes' : [
    'common_variables.gypi',
  ],

  'variables' : {
    # The path to the python interpreter is passed in by build.py.
    'python%': 'python',

    # Ninja doesn't normalize path separators that appear in 'product_dir'
    # values. This is a bug. Instead, we have to define and use the proper path
    # separator to use in those cases. This should ONLY be used/needed inside of
    # 'product_dir' values!
    'path_sep': '/',

    # The following variable makes it possible to use Ion's gypfiles from other
    # projects that do not rely on build.by; this disables the inclusion of
    # xcconfig files for ion.
    'no_project_config_file': '1',

    # Paths
    'root_dir': '<(DEPTH)',
    'ion_dir': '<(root_dir)/ion',
    'third_party_dir': '<(root_dir)/third_party',

    # These are the OSes that ion is known to build on.
    'ion_valid_target_oses': [
      'linux',
      'mac',
      'win',
      'ios',
      'android',
      'nacl',
      'asmjs',
      'qnx',
    ],
  },

  'target_defaults': {
    'include_dirs' : [
      '<(ion_dir)/port/override',
      '<(root_dir)',
    ],
    'defines' : [
      'ION_GYP=1',
      'ION_CHECK_GL_ERRORS=<(check_gl_errors)',
      'ION_NO_RTTI=0',
    ],
    'conditions' : [
      ['ion_analytics_enabled', {
        'defines': ['ION_ANALYTICS_ENABLED=1']
      }],
      ['ion_track_shareable_references', {
        'defines': ['ION_TRACK_SHAREABLE_REFERENCES=1']
      }],
    ],
  },

  # No effect on other platforms:
  'xcode_settings': {
    # DO NOT set SYMROOT and OBJROOT here. This is included by all targets, and
    # should be build-output-directory agnostic.
    'USE_HEADERMAP': 'NO',
    'CLANG_ENABLE_OBJC_ARC': 'YES',
    'GCC_WARN_SIGN_COMPARE': 'YES',
    'GCC_WARN_NON_VIRTUAL_DESTRUCTOR': 'YES',
    'GCC_WARN_ABOUT_RETURN_TYPE': 'YES',
    'GCC_SYMBOLS_PRIVATE_EXTERN': 'YES',
    'GCC_ENABLE_CPP_EXCEPTIONS': 'NO',
    'OTHER_CFLAGS': [
      '-Wall',
      '-Wconversion',
      '-Wno-sign-conversion',
    ],
  },

  'conditions': [
    # Check that the OS is one of the supported platforms.
    ['OS not in ion_valid_target_oses', {
      'variables': {
        # There is no good way to cause gyp to say "hey, assertion failed,
        # stop". This is something that fails in a somewhat obvious way:
        'library': '<!(<(python) -c "import sys;print >>sys.stderr, \'Error: OS <(OS) is not supported\';sys.exit(1)" )',
      },
    }],

    ['GENERATOR != "ninja"', {
      # The ninja generator doesn't know how to deal with
      # GCC_ENABLE_OBJC_EXCEPTIONS.
      'xcode_settings': {
        'GCC_ENABLE_OBJC_EXCEPTIONS': 'NO',
      },
    }],

    ['use_icu and OS not in ["ios", "mac"]', {
      'target_defaults': {
        'defines': [
          'ION_USE_ICU=1',
        ],  # defines
      },  # target_defaults
    }],

    ['OS=="mac"', {
      'target_defaults' : {
        'defines' : [
          'ION_PLATFORM_MAC=1',
          'ION_ARCH_X86_64=1',
          'OS_MACOSX=OS_MACOSX',
        ],
        'all_dependent_settings': {
          'defines' : [
            'ION_PLATFORM_MAC=1',
          ],
        },  # all_dependent_settings
      },  # target_defaults
    }],  # mac

    ['OS=="android"', {
      'target_defaults' : {
        'defines' : [
          'ION_PLATFORM_ANDROID=1',
        ],
        'all_dependent_settings': {
          'defines' : [
            'ION_PLATFORM_ANDROID=1',
          ],
        },  # all_dependent_settings
        'conditions': [
          ['flavor == "arm"', {
            'defines': [
              'ION_ARCH_ARM=1',
              'ION_ARCH_ARM32=1',
            ],
          }],
        ],  # conditions
        'cflags_cc': [
          # Enable C++11 support.
          '-std=gnu++11'
        ],
      },  # target_defaults
    }],  # android

    ['OS=="ios"', {
      'xcode_settings': {
        'CLANG_ENABLE_OBJC_ARC': 'YES',
      },
      'target_defaults' : {
        'defines' : [
          'ION_PLATFORM_IOS=1',
          'ION_ARCH_X86=1',
          # This is needed on iOS as well.
          'OS_MACOSX=OS_MACOSX',
        ],
        'all_dependent_settings': {
          'defines' : [
            'ION_PLATFORM_IOS=1',
          ],
        },  # all_dependent_settings
      },  # target_defaults

      'configurations': {
        'Debug': {
          'xcode_settings': {
            # This fixes the warning about "enable build active architecture
            # only when debugging".
            'ONLY_ACTIVE_ARCH' : 'YES',
          },
        },
      },  # configurations
    }],  # ios

    ['OS=="nacl"', {
      'conditions': [
        ['flavor == "pnacl"', {
          'target_defaults': {
            'defines': [
              'PNACL',
              'ION_PLATFORM_PNACL=1',
            ],

            'cflags_cc': [
              # Enable C++11 support.
              '-std=gnu++11',
            ],
          },
        }, { # else
          'target_defaults': {

            'cflags_cc': [
              # Enable C++11 support.
              '-std=gnu++11',
            ],
          },
        }],
      ],  # conditions

      'variables': {
        'library': 'static_library',
      },

      'target_defaults': {
        # The below settings are for nacl OR pnacl.
        'cflags+': [
          '-include ion/port/nacl/override/aligned_malloc.h',
        ],
        'include_dirs+' : [
          '<(ion_dir)/port/override',
          '<(ion_dir)/port/nacl/override',
        ],
        'cflags_cc': [
          '-Wno-non-virtual-dtor',
          '-fno-exceptions',
          '-fno-strict-aliasing',
          '-frtti',
          '-Wno-parentheses',
          '-Wno-write-strings',
        ],
        'defines': [
          'NACL',
          'ACE_LACKS_PRAGMA_ONCE',
          'ION_PLATFORM_NACL=1',
          'OS_NACL',
          'NACL_X86',
          'ION_ARCH_X86=1',
        ],
        'defines!': [
          'COMPILER_GCC3',  # For base headers like port.h.
        ],
        'all_dependent_settings': {
          'defines': [
            'ION_PLATFORM_NACL=1',
          ],
          'conditions': [
            ['flavor == "pnacl"', {
              'defines': [
                'ION_PLATFORM_PNACL=1',
              ],
            }],
          ],  # conditions
        },  # all_dependent_settings
      },
    }],  # nacl

    ['OS=="asmjs"', {

      'variables': {
        'library': 'static_library',
      },

      'target_defaults': {
        'target_conditions' : [
          [ '_type == "executable"', {
            'ldflags': [
              '-s PRECISE_I64_MATH=1',
              '-s TOTAL_MEMORY=268435456',
            ]
          }],
          [ '_type == "static_library"', {
            'ldflags': [
              '--ignore-dynamic-linking',
            ],
          }],
        ],
        'cflags!': [
          '-g',  # Useless with asm.js, just makes binaries bigger
        ],
        'cflags': [
          '-Wno-warn-absolute-paths',
        ],
        'cflags_cc': [
          '-fPIC',
          '-fno-exceptions',
          '-std=c++11',
          '-Wall',
          '-Wno-null-conversion',
          '-fvisibility=hidden',
          '-fvisibility-inlines-hidden',
        ],
        'defines': [
          'OS_LINUX=OS_LINUX',
          'ION_PLATFORM_ASMJS=1',
          # Fixes an assert in regex compile in some unit tests.
          'BOOST_XPRESSIVE_BUGGY_CTYPE_FACET=1',
        ],
        'all_dependent_settings': {
          'defines': [
            'ION_PLATFORM_ASMJS=1',
          ],
        },  # all_dependent_settings
        'ldflags': [
          '-s ERROR_ON_UNDEFINED_SYMBOLS=1',
        ]
      },
    }],  # asmjs

    ['OS=="linux"', {
      'target_defaults': {
        'defines' : [
          'ION_PLATFORM_LINUX=1',
          'OS_LINUX=OS_LINUX',
          'ARCH_K8',
          'ION_ARCH_X86_64=1',
        ],
        'all_dependent_settings': {
          'defines': [
            'ION_PLATFORM_LINUX=1',
          ],
        },  # all_dependent_settings
        'ldflags': [
          '-m64',
          # Uncomment for verbose linker debugging.
          # '-Wl,--verbose',
          '-fno-stack-protector',
          '-Wl,--hash-style=sysv',
          '-Wl,-nostdlib',
          '-Wl,--no-as-needed',
        ],
        'link_settings': {
          'libraries': [
            '-lpthread',
            '-lpthread_nonshared',
            '-lm',
            '-lc',
            '-lc_nonshared',
            '-ldl',
            '-lgcc',
          ],
        },
        'cflags': [
          '-m64',
          '-fPIC',
        ],
        'cflags_cc': [
          '-Wall',
          '-Werror',  # Treat warnings as errors...
          '-Wno-error=unknown-warning-option', # Except the ones we don't know about.
          '-Wno-deprecated',
          '-Wno-unknown-pragmas',
          '-fno-exceptions',

          # There are several bugs in g++ since version 4.4 regarding strict
          # aliasing.
          '-Wno-strict-aliasing',

          # Enable warnings that could break on Windows.
          '-Wconversion',
          '-Wno-sign-conversion',
          '-Wsign-compare',

          # Enable C++11 support.
          '-std=c++11',
        ],
      }
    }],  # linux

    ['OS=="qnx"', {
      'target_defaults': {
        'defines' : [
          'OS_EMBEDDED_QNX',
          'OS_EMBEDDED_QNX_ARM',
          'KDWIN',
          'ION_ARCH_ARM=1',
          'ION_ARCH_ARM32=1',
          'ION_PLATFORM_QNX=1',
        ],
        'all_dependent_settings': {
          'defines': [
            'ION_PLATFORM_QNX=1',
          ],
        },  # all_dependent_settings
        'include_dirs+' : [
          '<(ion_dir)/port/override',
        ],
        'link_settings': {
          'libraries': [
            '-lm',
            '-lsocket',
            '-lstdc++',
          ],
        },
        'cflags': [
          '-Wall',
          '-fPIC',
          '-fno-strict-aliasing',
          '-fvisibility=hidden',
          '-fno-omit-frame-pointer',
          '-fno-inline',
        ],
        'cflags_cc': [
          '-fno-default-inline',
          '-Wc,-std=c++0x',
          '-fpermissive',
          '-fvisibility-inlines-hidden',
          '-fPIC',
        ],
      }
    }],  # qnx

    ['OS=="win"', {
      'target_defaults': {
        'all_dependent_settings': {
          'defines': [
            'ION_PLATFORM_WINDOWS=1',
          ],
          'msvs_disabled_warnings': [
            # Generated by Windows header dbghelp.h in VS2015.
            '4091',
            # No matching operator delete found; memory will not be freed if
            # initialization throws an exception.
            # Placement new operators in Ion headers are not exception-safe
            # and we have to enable exceptions in Windows builds (because the
            # VS2013 headers are incompatible with disabling exceptions), so
            # any dependent project has to suppress this level 1 warning which
            # is pretty bad but necessary.
            '4291',
            # VS2015 performance warning about converting int to bool.
            '4800',
          ],
        },  # all_dependent_settings

        'msvs_disabled_warnings': [
          # '*/' found outside of comment
          # Some google code does this, in violation of the style guide.

          '4138',

          # Type1 needs to have dll-interface to be used by clients of class
          #     Type2
          # Exported class derived from a class that was not exported.
          # Deriving a class from an STL class causes these.  If the STL class
          # isn't completely inlined in the instantiation of the derived class,
          # this can lead unresolved symbols at link time.
          '4251',
          '4275',

          # New behavior: elements of array will be default initialized.
          # That's exactly what we want to happen.  This warning exists because
          # old versions of MSVC++ didn't do this and thus it might otherwise
          # appear to be a performance regression.
          '4351',
        ],
        'msvs_settings': {
          # The object files are stored in a directory structure that mirrors the
          # corresponding source tree. This prevents clobbering when two files in
          # different directories have the same name.
          'VCCLCompilerTool': {
            'ObjectFile': '$(IntDir)%(Directory)',
          },
          # Visual Studio 2015 Update 2 has an undocumented feature where it
          # automatically inserts telemetry calls that sound suspicious.
          # This linker option removes them (verified via disassembly).
          # For more details, see:
          # https://www.infoq.com/news/2016/06/visual-cpp-telemetry#anch137071
          'VCLinkerTool': {
            'AdditionalOptions': [
              'notelemetry.obj',
            ],
          },
        },
      },
    }],  # windows
  ],
}
