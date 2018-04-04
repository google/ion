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
  'conditions': [
    ['OS in ["android", "linux", "asmjs", "nacl"]', {
      'cflags!': [
        '-fno-exceptions',
      ],
      'cflags_cc!': [
        '-fno-exceptions',
      ],
      'cflags': [
        '-fexceptions',
      ],
      'cflags_cc': [
        '-fexceptions',
      ],
    }],

    ['GENERATOR != "ninja"', {
      # The ninja generator doesn't know how to deal with
      # GCC_ENABLE_OBJC_EXCEPTIONS.
      'xcode_settings': {
        'GCC_ENABLE_OBJC_EXCEPTIONS': 'YES',
      },
    }],

    ['OS == "win"', {
      'defines!': [
        '_HAS_EXCEPTIONS=0',
      ],
    }],
  ],  # conditions

  'xcode_settings': {
    'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
  },

  'msvs_settings': {
    'VCCLCompilerTool': {
      'WholeProgramOptimization': 'false',
      'ExceptionHandling': '1',  # /EHsc
    },
    'VCLibrarianTool': {
      'LinkTimeCodeGeneration': 'false',
    },
    'VCLinkerTool': {
      # 1 = 'LinkTimeCodeGenerationOptionUse'
      'LinkTimeCodeGeneration': '0',
    },
  },
}
