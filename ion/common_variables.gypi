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
  # User-settable variables that are orthogonal to targets, toolchains,
  # platforms, configurations, etc.

  'variables': {
    # Nested 'variables' required for 'conditions' in including .gyp's to apply.
    'variables': {
      # Use ICU library for RTL analysis, complex text layout (e.g. Arabic &
      # Devanagari shaping), etc.  Disabling this means that many non-Latin
      # scripts will be rendered incorrectly (but also that binary size will see
      # a significant reduction).
      'use_icu%': 0,
    },  # nested variables

    # Use angle.
    'angle%': 0,

    # Use ogles.
    'ogles20%': 0,

    # Use curl in the net library (see net/net.gyp).
    'use_curl_net%': 0,

    # Disable GL error checking by default.
    'check_gl_errors%': 0,

    # Disable GL profiling by default.
    'ion_analytics_enabled%': 0,

    # Disable tracking of shareable references by default.
    'ion_track_shareable_references%': 0,

    'conditions': [
      ['OS in ["android", "mac", "win"]', {
        # Platforms where ICU is known to work well.  Trickiest bit to adding
        # new platforms to this list should be finding the ICU data files and
        # carrying out initialization (so, not rocket-science).
        'use_icu%': 1,
      }, {
        'use_icu%': '<(use_icu)',
      }],

      ['OS in ["qnx", "linux"]', {
        # QNX and Linux use CurlNetworkManager.
        # Mac/iOS use DarwinNetworkManager.
        # Android uses AndroidNetworkManager.
        # Windows uses WindowsNetworkManager.
        # NaCl uses NaClNetworkManager.
        'use_curl_net%': 1,
      }],
    ],
  },  # variables
}
