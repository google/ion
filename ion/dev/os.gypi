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
# NOTE: This file is used OUTSIDE of gyp as well. It is parsed by build.py,
# which does some cursory conditional expansion as well. As such, this file
# should ONLY make use of the following variables *inside of conditionals*:
#
#   - OS
#   - flavor
#   - GENERATOR
{
  'variables': {
    'variables': {
      'conditions': [
        ['GENERATOR in ["ninja"]', {
          'out_dir': '<(gyp_out_os_dir)/$|CONFIGURATION_NAME',
        }],

        ['OS == "win"', {
          # The windows toolchain really *really* wants backslashes.
          'out_dir': '<(gyp_out_os_dir)\$|CONFIGURATION_NAME',
          'path_sep': '\\',
        }],

        ['GENERATOR in ["gypd", "dump_dependency_json"]', {
          # Doesn't really matter.
          'out_dir': '<(gyp_out_os_dir)/$|CONFIGURATION_NAME',
        }],

        ['GENERATOR in ["xcode", "msvs"]', {
          'out_dir': '<(gyp_out_os_dir)/$(CONFIGURATION)',
        }],

        ['GENERATOR in ["make"]', {
          'out_dir': '<(gyp_out_os_dir)/$(BUILDTYPE)',
        }],
      ],  # conditions
    },
    'gyp_out': '<!(<(python) -c "import os;print os.path.abspath(\'<(DEPTH)/gyp-out/\')")',
    'out_dir': '<(out_dir)',

    # Specify MSVC runtime version. See more details see:
    # https://msdn.microsoft.com/en-us/library/abx4dbyh.aspx
    # 0 => multithreaded static library (/MT)
    # 1 => multithreaded static debug library (/MTd)
    # 2 => multithreaded DLL (/MD)
    # 3 => multithreaded debug DLL (/MDd)
    # Default is multithreaded debug DLL (/MDd).
    'msvc_runtime_library_debug%': 3,
    # Default is multithreaded DLL (/MD).
    'msvc_runtime_library_release%': 2,
    # To override these values, you need to pass e.g.
    # -D=msvc_runtime_library_release=2 -D=msvc_runtime_library_debug=1 on the
    # gyp command line.
  },

  'target_defaults': {
    'target_conditions': [
      ['_type in ["static_library", "shared_library"]', {
        'includes': [
          'target_type_library.gypi',
        ],
      }],

      ['_type == "executable"', {
        'includes': [
          'target_type_executable.gypi',
        ],
      }],
    ],  # target_conditions

    # This target_defaults is just an empty holder for the base level
    # abstract configurations.
    'configurations': {
      'dbg_base': {
        'abstract': 1,
        'defines': [
          '_DEBUG',
          'DEBUG=1',
          'ION_DEBUG=1',
        ],
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': '0',
          'RETAIN_RAW_BINARIES': 'YES',
        },
        'conditions': [
          ['GENERATOR != "ninja"', {
            # The ninja generator doesn't know how to deal with
            # GCC_DEBUGGING_SYMBOLS.
            'xcode_settings': {
              'GCC_DEBUGGING_SYMBOLS': 'full',
            },
          }],
        ],  # conditions
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',  # Disable optimization.
            'DebugInformationFormat': '3',  # /Zi
            'BasicRuntimeChecks': '1',  # '/RTC1' enable fast checks
            'RuntimeLibrary': '<(msvc_runtime_library_debug)',
            'AdditionalOptions': ['/FS'],  # Force synchronous pdb writing.
          },

          'VCLinkerTool': {
            'GenerateDebugInformation': 'true',
            'LinkIncremental': '2',     # Enabled.
          },
        },
      },
      'opt_base': {
        'abstract': 1,
        'cflags': [
          '-fdata-sections',
          '-ffunction-sections',
          '-fvisibility=hidden',
        ],
        'ldflags': [
          '-Wl,--strip-debug',
          '-Wl,--gc-sections',
          #'-Wl,--icf=all',  # What's the clang equivalent?
        ],
        'defines': [
          'NDEBUG',
        ],
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': 's',
          'RETAIN_RAW_BINARIES': 'NO',
          'DEAD_CODE_STRIPPING': 'YES',
          # No debugging symbols, prevents '-g' flag:
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
          # Keep whatever debug info might be left out of the binary:
          'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
        },
        'conditions': [
          ['GENERATOR != "ninja"', {
            # The ninja generator doesn't know how to deal with
            # GCC_DEBUGGING_SYMBOLS.
            'xcode_settings': {
              'GCC_DEBUGGING_SYMBOLS': 'used',
            },
          }],
        ],  # conditions
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',         # Maximize speed (/O2).
            'StringPooling': 'true',
            'RuntimeLibrary': '<(msvc_runtime_library_release)',
            'BufferSecurityCheck': 'true',
            'DebugInformationFormat': '3',  # /Zi
            'AdditionalOptions': ['/FS'],  # Force synchronous pdb writing.
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',      # Disabled.
            'OptimizeReferences': '2',   # Enabled.
            'EnableCOMDATFolding': '2',  # Folding.
            'OptimizeForWindows98': '1', # Disabled.
            'GenerateDebugInformation': 'true',
          },
        },
      },
      'prod_base': {
        'abstract': 1,
        # Everything from opt_base.
        'inherit_from': ['opt_base'],

        'defines': [
          'ION_PRODUCTION=1',
        ],
        'xcode_settings': {
          'DEPLOYMENT_POSTPROCESSING': 'YES',
          'STRIPFLAGS': '-x',
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            # /GL disabled since it can take more than an hour even for
            # incremental linking and we haven't been able to show any
            # significant performance improvements.
            'WholeProgramOptimization': 'false',
          },
          'VCLinkerTool': {
            # /LTCG
            'LinkTimeCodeGeneration': 'LinkTimeCodeGenerationOptionUse',
          },
          'VCLibrarianTool': {
            # /LTCG
            'LinkTimeCodeGeneration': 'LinkTimeCodeGenerationOptionUse',
          },
        },
      },
    },
  },

  'conditions': [
    ['OS=="mac"', {
      'variables': {
        'library': 'static_library',
      },
      'target_defaults' : {
        'default_configuration': 'dbg',
        'configurations': {
          'dbg': {
            'inherit_from': ['dbg_base'],
          },
          'opt': {
            'inherit_from': ['opt_base'],
          },
          'prod': {
            'inherit_from': ['prod_base'],
          },
          # Also define 'Debug' and 'Release' to coincide with naming
          # conventions for most OSX apps.
          'Debug': {
            'inherit_from': ['dbg'],
          },
          'Release': {
            'inherit_from': ['prod'],
          },
        },
        'xcode_settings': {
          'SDKROOT': 'macosx',
          'ARCHS': ['x86_64'],
          # Target Mac OSX 10.8+.
          'MACOSX_DEPLOYMENT_TARGET': '10.8',
          'CLANG_CXX_LANGUAGE_STANDARD' : 'c++11',
          'CLANG_CXX_LIBRARY': 'libc++',
        },
        'conditions': [
          ['GENERATOR in ["ninja"]', {
            'link_settings': {
              'libraries': [
                '-lc++',
              ],
            },
          }],
        ],
      },
    }],  # mac

    ['OS=="android"', {
      'includes': [
        'ant.gypi',
      ],
      # It would be sweet if something like
      # 'includes : [ android_arm<(flavor) ' were supported by gyp, but
      # neither honza@ nor I could make that parse.
      'conditions': [
        ['flavor in [ "arm", ""]', {
          'includes': [
            'android_arm.gypi',
          ],
        }],
        ['flavor == "arm64"', {
          'includes': [
            'android_arm64.gypi',
          ],
        }],
        ['flavor == "mips"', {
          'includes': [
            'android_mips.gypi',
          ],
        }],
        ['flavor == "mips64"', {
          'includes': [
            'android_mips64.gypi',
          ],
        }],
        ['flavor == "x86"', {
          'includes': [
            'android_x86.gypi',
          ],
        }],
        ['flavor == "x86_64"', {
          'includes': [
            'android_x86_64.gypi',
          ],
        }],
      ],  # conditions
      'variables': {
        'library': 'static_library',
        'ogles20': '1',
      },
      'target_defaults' : {
        'link_settings': {
          'libraries': [
            '-lgnustl_static',
          ],
        },
        'default_configuration': 'dbg',
        'configurations': {
          'dbg': {
            'inherit_from': ['dbg_base'],
            'cflags': [
              '-g',
            ],
            'ldflags': [
              '-g',
            ],
          },
          'opt': {
            'inherit_from': ['opt_base'],
            'cflags': [
              '-O2',
              '-fno-partial-inlining',
              '-fno-move-loop-invariants',
              '-fno-tree-loop-optimize',
            ],
          },
          'prod': {
            'inherit_from': ['prod_base'],
            'cflags': [
              '-O2',
              '-fno-partial-inlining',
              '-fno-move-loop-invariants',
              '-fno-tree-loop-optimize',
            ],
          },
        },
      },  # target_defaults
    }],  # android

    ['OS=="ios"', {
      'includes': [
        'ios.gypi',
      ],
      'variables': {
        'library': 'static_library',
      },
      'target_defaults' : {
        'default_configuration': 'dbg',
        'configurations': {
          'dbg': {
            'inherit_from': ['dbg_base'],
          },
          'opt': {
            'inherit_from': ['opt_base'],
          },
          'prod': {
            'inherit_from': ['prod_base'],
          },
          # Also define 'Debug' and 'Release' to coincide with naming
          # conventions for most iOS apps.
          'Debug': {
            'inherit_from': ['dbg'],
          },
          'Release': {
            'inherit_from': ['prod'],
          },
        },
        'target_conditions': [
          ['_toolset == "target"', {
            'variables': {
              'ios_deployment_target%': '6.0',
            },
            'xcode_settings': {
              'IPHONEOS_DEPLOYMENT_TARGET': '<(ios_deployment_target)',
              'CLANG_CXX_LANGUAGE_STANDARD' : 'c++11',
              'CLANG_CXX_LIBRARY': 'libc++',
            },  # xcode_settings
            'conditions': [
              ['flavor == "x86"', {
                'xcode_settings': {
                  'SDKROOT': 'iphonesimulator',
                },
              }, {
                'xcode_settings': {
                  'SDKROOT': 'iphoneos',
                  'GOOGLE_CODE_SIGN_IDENTITY_DEFAULT[sdk=iphoneos*]': 'iPhone Developer',
                  'GOOGLE_CODE_SIGN_IDENTITY': '$(GOOGLE_CODE_SIGN_IDENTITY_DEFAULT)',
                  'CODE_SIGN_IDENTITY': '$(GOOGLE_CODE_SIGN_IDENTITY)',
                },
              }],
            ],
          }],
        ],  # target_conditions
      },  # target_defaults
    }],  # ios

    ['OS=="nacl"', {
      'conditions': [
        ['flavor == "pnacl"', {
          'includes': [
            'pnacl.gypi',
          ],
          'target_defaults': {
            'default_configuration': 'dbg',
            'defines': [
            ],
            'configurations': {
              'dbg': {
                'inherit_from': ['dbg_base'],
                'defines': [
                  'DEBUG=1',
                ],
                'cflags': [
                  '-O0',
                ],
                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/pnacl/Debug',
                ],
              },
              'opt': {
                'inherit_from': ['opt_base'],
                'cflags': [
                  '-O2',
                ],

                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/pnacl/Release',
                ],
              },
              'prod': {
                'inherit_from': ['prod_base'],
                'cflags': [
                  '-O2',
                ],

                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/pnacl/Release',
                ],
              },
            },  # configurations
          },
        }, { # else
          'includes': [
            'nacl.gypi',
          ],
          'target_defaults': {
            'default_configuration': 'dbg-32',
            'configurations': {
              'nacl_32_base': {
                'abstract': 1,
                'cflags': [
                  '-m32',
                ],
                'ldflags': [
                  '-m32',
                ],
              },

              'nacl_64_base': {
                'abstract': 1,
                'cflags': [
                  '-m64',
                ],
                'ldflags': [
                  '-m64',
                ],
                'defines': [
                  'OS_NACL',
                  'NACL_X86',
                  'ION_ARCH_X86_64=1',
                ],
              },

              'dbg-32': {
                'inherit_from': ['dbg_base', 'nacl_32_base'],
                'cflags': [
                  '-g',
                  '-O0',
                ],
                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/clang-newlib_x86_32/Debug',
                ],
              },
              'dbg-64': {
                'inherit_from': ['dbg_base', 'nacl_64_base'],
                'cflags': [
                  '-g',
                  '-O0',
                ],
                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/clang-newlib_x86_64/Debug',
                ],
              },
              'opt-32': {
                'inherit_from': ['opt_base', 'nacl_32_base'],
                'cflags': [
                  '-O2',
                ],
                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/clang-newlib_x86_32/Release',
                ],
              },
              'opt-64': {
                'inherit_from': ['opt_base', 'nacl_64_base'],
                'cflags': [
                  '-O2',
                ],
                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/clang-newlib_x86_64/Release',
                ],
              },
              'prod-32': {
                'inherit_from': ['prod_base', 'nacl_32_base'],
                'cflags': [
                  '-O2',
                ],
                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/clang-newlib_x86_32/Release',
                ],
              },
              'prod-64': {
                'inherit_from': ['prod_base', 'nacl_64_base'],
                'cflags': [
                  '-O2',
                ],
                'library_dirs': [
                  '<(nacl_sdk_root_path)/lib/clang-newlib_x86_64/Release',
                ],
              },
            },
          },
        }],
      ],  # conditions

      'variables': {
        'library': 'static_library',
      },
    }],  # nacl

    ['OS=="asmjs"', {
      'includes': [
        'asmjs.gypi'
      ],
      'variables': {
        'library': 'static_library',
      },
      'target_defaults': {
        'default_configuration': 'dbg',
        'configurations': {
          'asmjs_opt_base': {
            'abstract': 1,
            'target_conditions' : [
              ['_toolset=="target"', {
                'cflags': [
                  '-O2',
                ],
                'ldflags': [
                  '-O2',
                  '--closure 0',
                  '-s ASM_JS=1',
                  '-s OUTLINING_LIMIT=50000',
                ],
              }],
            ],  # target_conditions
          },

          'dbg': {
            'inherit_from': ['dbg_base'],
            'target_conditions' : [
              ['_toolset=="target"', {
                'cflags': [
                  # This should be -O0, but we started to hit "too many variables"
                  # from nodejs (see b/17253141). -O1 removes enough code to get us
                  # below this limit (for now!) while maintaining somewhat readable
                  # code and asserts.
                  '-O1',
                ],
              }],
            ],  # target_conditions
          },
          'opt': {
            'inherit_from': ['opt_base', 'asmjs_opt_base'],
          },
          'prod': {
            'inherit_from': ['prod_base', 'asmjs_opt_base'],
          },
        },
        'cflags!': [
          '-g',  # Useless with asm.js, just makes binaries bigger
        ],
      },
    }],  # asmjs

    ['OS=="linux"', {
      'includes': [
        'linux.gypi',
      ],
      'variables': {
        'library': 'static_library',
      },
      'target_defaults': {
        'default_configuration': 'dbg',
        'configurations': {
          'dbg': {
            'inherit_from': ['dbg_base'],
            'cflags': [
              '-g',
              '-O0',
              # No -fvisibility=hidden, which interferes with backtrace_symbols.
            ],
          },
          'opt': {
            'inherit_from': ['opt_base'],
            'cflags': [
              '-O2',
              '-fvisibility=hidden',
              '-fvisibility-inlines-hidden',
            ],
          },
          'prod': {
            'inherit_from': ['prod_base'],
            'cflags': [
              '-O2',
              '-fvisibility=hidden',
              '-fvisibility-inlines-hidden',
            ],
          },
        },
        'ldflags': [
          '-lc++',
          '-lc++abi',
          '-rdynamic', # Support backtrace_symbols.
          # Uncomment for verbose linker debugging.
          # '-Wl,--verbose',
        ],
      }
    }],  # linux

    ['OS=="win"', {
      'includes': [
        'windows.gypi',
      ],
      'variables': {
        'library': 'static_library',
        'ogles20%': '0',
      },
      'target_defaults': {
        'configurations': {
          'x86_base': {
            'abstract': 1,
            'msvs_settings': {
              'VCLinkerTool': {
                'TargetMachine' : 1 # /MACHINE:X64
              },
            },
            'include_dirs': [
              '<@(include_dirs_common)',
            ],
            'library_dirs': [
              '<@(library_dirs_x86)',
            ],
            'defines' : [
              'ION_ARCH_X86=1'
            ],
          },
          'x64_base': {
            'abstract': 1,
            'msvs_configuration_platform': 'x64',
            'msvs_target_platform': 'x64',
            'msvs_settings': {
              'VCLinkerTool': {
                'TargetMachine': '17', # x86 - 64
              },
            },
            'include_dirs': [
              '<@(include_dirs_common)',
            ],
            'library_dirs': [
              '<@(library_dirs_x64)',
            ],
            'defines' : [
              'ION_ARCH_X86_64=1'
            ],
          },
        },  # configurations

        'conditions': [
          ['GENERATOR == "msvs"', {
            'default_configuration': 'Debug',
            'configurations': {
              # The visual studio standards are "Debug" and "Release" so use
              # those here. Also, we're only targeting x64 out of the box. It's
              # possible to add additional platforms in the Configuration
              # manager inside visual studio.
              'Debug': {
                'inherit_from': ['dbg_base', 'x64_base'],
              },
              'Release': {
                'inherit_from': ['dbg_base', 'x64_base'],
              },
            },
          }, {
            'default_configuration': 'dbg_x86',
            'configurations': {
              # A note on naming: the convention on windows is to append 'x86' or
              # 'x64' to the configuration name, so we follow that here.
              'dbg_x86': {
                'inherit_from': ['dbg_base', 'x86_base'],
              },
              'opt_x86': {
                'inherit_from': ['opt_base', 'x86_base'],
              },
              'prod_x86': {
                'inherit_from': ['prod_base', 'x86_base'],
              },
              'dbg_x64': {
                'inherit_from': ['dbg_base', 'x64_base'],
              },
              'opt_x64': {
                'inherit_from': ['opt_base', 'x64_base'],
              },
              'prod_x64': {
                'inherit_from': ['prod_base', 'x64_base'],
              },
              # Windows needs an additional "Default" configuration because
              # that's configuration used in protoc_build.gyp. The name
              # "Default" is special because that's what gyp uses as the
              # default configuration name if no other configurations are
              # specified.  On windows, though, the configuration names have to
              # be enumerated because of environment.x86/x64 file generation.
              # See generate_ninja_environment.gyp for how the environment files
              # are created.
              'Default': {
                'inherit_from': ['opt_base', 'x86_base'],
              },
            },
          }],
        ],  # conditions

        # Do NOT run any scripts as a cygwin script (which does all sorts of
        # cygpath conversion that we don't need).
        'msvs_cygwin_shell': 0,
        'msvs_quote_cmd': 0,

        'msvs_settings': {
          'VCCLCompilerTool': {
            'WarnAsError': 'true',  #  /WX
            'DefaultCharIsUnsigned': 'true',  # /J
            'EnableFunctionLevelLinking': 'true',  # /Gy
            'StringPooling': 'true',  # /GF
            'SuppressStartupBanner': 'true',  # /nologo

            'WarningLevel': '3',  # /W3
            'AdditionalOptions': ['/bigobj', '/Zm500'],
            'ExceptionHandling': '1',
          },
          'VCResourceCompilerTool': {
            'AdditionalIncludeDirectories': ['<@(include_dirs_common)'],
          },
        },
        'msvs_configuration_attributes': {
          'OutputDirectory': '<(out_dir)',
          # Make the intermediate directory include the target name. This is
          # because visual studio tries to build multiple targets at once, but
          # steps on its own feet when outputting to the same 'obj' directory.
          'IntermediateDirectory': '<(out_dir)\\obj\\>(_target_name)',
        },
      },
    }],  # win

    ['OS == "win"', {
      'target_defaults': {
        'defines': [
          'ION_APIENTRY=APIENTRY',
        ],  # defines
      },
    }, {
      # Doing it this way is easier than sprinkling this define in each of the
      # other platform sections (above).
      'target_defaults': {
        'defines': [
          'ION_APIENTRY=',
        ],
      },
    }],
  ],

  # Note that this is applied *outside* of target_defaults. This has the effect
  # of being applied to ALL targets (even those that don't import or use our
  # target_defaults) such as third party gyp files.
  'xcode_settings': {
    'SYMROOT': '<(out_dir)/obj',
    'OBJROOT': '<(out_dir)/obj',
  },
}
