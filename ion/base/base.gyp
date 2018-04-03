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

  'target_defaults': {
    'includes' : [
      '../dev/target_visibility.gypi',
    ],
  },

  'targets': [
    {
      'target_name': 'ionbase',
      'type': 'static_library',
      'sources': [
        'allocatable.cc',
        'allocatable.h',
        'allocationmanager.cc',
        'allocationmanager.h',
        'allocationsizetracker.h',
        'allocationtracker.h',
        'allocator.cc',
        'allocator.h',
        'argcount.h',
        'array2.h',
        'bufferbuilder.cc',
        'bufferbuilder.h',
        'circularbuffer.h',
        'calllist.cc',
        'calllist.h',
        'datacontainer.cc',
        'datacontainer.h',
        'datetime.cc',
        'datetime.h',
        'enumhelper.h',
        'fullallocationtracker.cc',
        'fullallocationtracker.h',
        'functioncall.h',
        'indexmap.h',
        'invalid.cc',
        'invalid.h',
        'lockguards.h',
        'logchecker.cc',
        'logchecker.h',
        'memoryzipstream.cc',
        'memoryzipstream.h',
        'notifier.cc',
        'notifier.h',
        'nulllogentrywriter.h',
        'once.h',
        'readwritelock.cc',
        'readwritelock.h',
        'referent.h',
        'scalarsequence.h',
        'scopedallocation.h',
        'serialize.h',
        'setting.cc',
        'setting.h',
        'settingmanager.cc',
        'settingmanager.h',
        'shareable.h',
        'sharedptr.h',
        'signal.h',
        'spinmutex.cc',
        'spinmutex.h',
        'static_assert.h',
        'stringtable.cc',
        'stringtable.h',
        'stringutils.cc',
        'stringutils.h',
        'threadspawner.cc',
        'threadspawner.h',
        'type_structs.h',
        'utf8iterator.cc',
        'utf8iterator.h',
        'variant.h',
        'varianttyperesolver.h',
        'vectordatacontainer.h',
        'weakreferent.h',
        'workerpool.cc',
        'workerpool.h',
        'zipassetmanager.cc',
        'zipassetmanager.h',
        'zipassetmanagermacros.h',
      ],
      'dependencies': [
        ':ionlogging',
        '<(ion_dir)/port/port.gyp:ionport',
        '<(ion_dir)/external/external.gyp:ionb64',
        '<(ion_dir)/external/external.gyp:ionzlib',
      ],
    },  # target: ionbase

    {
      'target_name': 'ionbase_for_tests',
      'type': 'static_library',
      'sources': [
        'tests/badwritecheckingallocator.h',
        'tests/logging_test_util.h',
        'tests/multilinestringsequal.h',
        'tests/testallocator.cc',
        'tests/testallocator.h',
      ],
      'dependencies': [
        ':ionbase',
      ],
    },  # target: ionbase_for_tests

    {
      'target_name': 'ionlogging',
      'type': 'static_library',
      'sources': [
        'logging.cc',
        'logging.h',
        'staticsafedeclare.cc',
        'staticsafedeclare.h',
      ],
      'dependencies': [
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },  # target: ionlogging
  ],
}
