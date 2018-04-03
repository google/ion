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
  ],

  'targets': [
    {
      'target_name': 'ionanalytics_nogfx',
      'type': 'static_library',
      'includes' : [
        '../dev/target_visibility.gypi',
      ],
      'sources': [
        'benchmark.cc',
        'benchmark.h',
        'benchmarkutils.cc',
        'benchmarkutils.h',
        'discrepancy.h',
      ],
      'dependencies': [
        '../base/base.gyp:ionbase',
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },
  ],
}
