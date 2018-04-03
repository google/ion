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
    '../../common.gypi',
  ],

  'targets': [
    {
      'target_name': 'ionbase_test',
      'includes': [
        '../../dev/test_target.gypi',
      ],
      'sources' : [
        'allocatable_test.cc',
        'allocationmanager_test.cc',
        'allocator_test.cc',
        'array2_test.cc',
        'calllist_test.cc',
        'circularbuffer_test.cc',
        'datacontainer_test.cc',
        'datetime_test.cc',
        'enumhelper_test.cc',
        'fullallocationtracker_test.cc',
        'functioncall_test.cc',
        'incompletetype.cc',
        'incompletetype.h',
        'indexmap_test.cc',
        'invalid_test.cc',
        'logchecker_test.cc',
        'logging_test.cc',
        'memoryzipstream_test.cc',
        'notifier_test.cc',
        'nulllogentrywriter_test.cc',
        'once_test.cc',
        'readwritelock_test.cc',
        'scalarsequence_test.cc',
        'scopedallocation_test.cc',
        'serialize_test.cc',
        'setting_test.cc',
        'settingmanager_test.cc',
        'sharedptr_test.cc',
        'signal_test.cc',
        'spinmutex_test.cc',
        'staticsafedeclare_test.cc',
        'stlallocator_test.cc',
        'stringutils_test.cc',
        'threadlocalobject_test.cc',
        'threadspawner_test.cc',
        'type_structs_test.cc',
        'utf8iterator_test.cc',
        'variant_test.cc',
        'varianttyperesolver_test.cc',
        'vectordatacontainer_test.cc',
        'weakreferent_test.cc',
        'workerpool_test.cc',
        'zipassetmanager_test.cc',
      ],
      'conditions': [
        # Threads don't exist in asmjs, so remove those tests.
        ['OS == "asmjs"', {
          'sources!': [
            'readwritelock_test.cc',
            'threadlocalobject_test.cc',
            'threadspawner_test.cc',
            'workerpool_test.cc',
          ],
        }],
      ],
      'dependencies' : [
        'base_tests_assets',
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
        '<(ion_dir)/external/gtest.gyp:iongtest_safeallocs',
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },

    {
      'target_name': 'base_tests_assets',
      'type': 'static_library',
      'includes': [
        '../../dev/zipasset_generator.gypi',
      ],
      'sources' : [
        'data/zipasset.iad',
      ],
      'dependencies' : [
        '<(ion_dir)/port/port.gyp:ionport',
      ],
    },
  ],
}
