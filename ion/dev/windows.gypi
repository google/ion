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
# This files gets included only for windows targets.

{
  'variables' : {
    'variables': {
      'windows_sdk_version': '10.0.10586.0',
      'windows_path_dirs_x86': [
        '<(msvc_dir)/vc/bin',
        '<(msvc_dir)/common7/IDE',
        '<(msvc_dir)/redist/x86/Microsoft.VC<(msvc_version).CRT',
        '<(msvc_redist_dir)/Debug_NonRedist/x86/Microsoft.VC<(msvc_version).DebugCRT',
        '<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT',
        '<(platformsdk_dir)/bin/x86',
        '<(platformsdk_dir)/bin/x86/ucrt',
      ],
      'windows_path_dirs_x64': [
        '<(msvc_dir)/vc/bin/amd64',
        '<(msvc_dir)/common7/IDE',
        '<(msvc_dir)/redist/x64/Microsoft.VC<(msvc_version).CRT',
        # Yes, the x64 paths include the x86 paths. This is because some tools
        # might be built with the default configuration (x86) and still need to
        # run under an x64 build.
        '<(msvc_redist_dir)/Debug_NonRedist/x86/Microsoft.VC<(msvc_version).DebugCRT',
        '<(msvc_redist_dir)/x86/Microsoft.VC<(msvc_version).CRT',
        '<(msvc_redist_dir)/Debug_NonRedist/x64/Microsoft.VC<(msvc_version).DebugCRT',
        '<(msvc_redist_dir)/x64/Microsoft.VC<(msvc_version).CRT',
        '<(platformsdk_dir)/bin/x64',
        '<(platformsdk_dir)/bin/x64/ucrt',
      ],
    },
    # The include dirs are the same for both architectures.
    'include_dirs_common': [
      '<(msvc_dir)/vc/include',
      '<(platformsdk_dir)/include/<(windows_sdk_version)/shared',
      '<(platformsdk_dir)/include/<(windows_sdk_version)/ucrt',
      '<(platformsdk_dir)/include/<(windows_sdk_version)/um',
      '<(platformsdk_dir)/include/<(windows_sdk_version)/winrt',
    ],

    'library_dirs_x86': [
      '<(msvc_dir)/vc/lib',
      '<(platformsdk_dir)/lib/<(windows_sdk_version)/ucrt/x86',
      '<(platformsdk_dir)/lib/<(windows_sdk_version)/um/x86',
    ],
    'library_dirs_x64': [
      '<(msvc_dir)/vc/lib/amd64',
      '<(platformsdk_dir)/lib/<(windows_sdk_version)/ucrt/x64',
      '<(platformsdk_dir)/lib/<(windows_sdk_version)/um/x64',
    ],

    'windows_path_dirs_x86%': '<(windows_path_dirs_x86)',
    'windows_path_dirs_x64%': '<(windows_path_dirs_x64)',
    'msvc_dir': '<!(<(python) -c "import os; print os.getenv(\'VSINSTALLDIR\') or os.path.realpath(\'<(third_party_dir)/msvc/files\')")',
    'platformsdk_dir': '<!(<(python) -c "import os; print os.getenv(\'WindowsSdkDir\') or os.path.realpath(\'<(third_party_dir)/windows_sdk_10/files\')")',
    'msvc_redist_dir': '<(msvc_dir)/vc/redist',
    'msvc_version': '<!(<(python) -c "import os; print (os.getenv(\'VisualStudioVersion\') or \'14.0\').replace(\'.\', \'\')")',
  },
  'target_defaults': {
    'conditions': [
      # Ninja on windows uses environment files, which must exist before
      # anything else is built. See documentation in
      # generate_ninja_environment.gyp
      ['GENERATOR == "ninja"', {
        'dependencies': [
          '<(DEPTH)/ion/dev/generate_ninja_environment.gyp:generate_ninja_environment',
        ],
      }],
    ],  # conditions
    'defines' : [
      'ION_PLATFORM_WINDOWS=1',
      'NOGDI',                # Don't pollute with GDI macros in windows.h.
      'NOMINMAX',             # Don't define min/max macros in windows.h.
      'OS_WINDOWS=OS_WINDOWS',
      'PRAGMA_SUPPORTED',
      'WIN32',
      'WINVER=0x0601',  # Windows 7.
      '_CRT_SECURE_NO_DEPRECATE',
      '_WIN32',
      '_WIN32_WINNT=0x0601',  # Windows 7.
      '_WINDOWS',

      # Defaults for other headers (along with OS_WINDOWS).
      'COMPILER_MSVC',

      # Use math constants (M_PI, etc.) from the math library
      '_USE_MATH_DEFINES',

      # Allows 'Foo&&' (e.g., move constructors).
      'COMPILER_HAS_RVALUEREF',

      # Unsuffixed Windows API functions resolve to Unicode variants.
      'UNICODE=1',

      # Doublespeak for "don't bloat namespace with incompatible winsock
      # versions that I didn't include".
      # http://msdn.microsoft.com/en-us/library/windows/desktop/ms737629.aspx
      'WIN32_LEAN_AND_MEAN=1',
    ],
  },  # target_defaults
}
