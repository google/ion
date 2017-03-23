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
    '../common.gypi',
  ],

  'target_defaults': {
    'includes' : [
      '../dev/target_visibility.gypi',
    ],
  },

  'targets' : [
    {
      'target_name' : 'ionprofile',
      'type': 'static_library',
      'dependencies': [
        '<(ion_dir)/analytics/analytics_nogfx.gyp:ionanalytics_nogfx',
        '<(ion_dir)/base/base.gyp:ionbase',
        '<(ion_dir)/external/external.gyp:ionjsoncpp',
        '<(ion_dir)/port/port.gyp:ionport',
      ],
      'sources' : [
        'calltracemanager.cc',
        'calltracemanager.h',
        'profiling.cc',
        'profiling.h',
        'timeline.cc',
        'timeline.h',
        'timelineevent.cc',
        'timelineevent.h',
        'timelineframe.h',
        'timelinemetric.h',
        'timelinenode.cc',
        'timelinenode.h',
        'timelinerange.h',
        'timelinescope.h',
        'timelinesearch.h',
        'timelinethread.h',
        'tracerecorder.cc',
        'tracerecorder.h',
        'vsyncprofiler.cc',
        'vsyncprofiler.h',
      ],
    },
  ],  # targets
}
