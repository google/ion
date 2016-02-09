#
# Copyright 2016 Google Inc. All Rights Reserved.
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
      'target_name' : 'iongfx_test',
      'includes': [ '../../dev/test_target.gypi' ],
      'sources' : [
        'attribute_test.cc',
        'attributearray_test.cc',
        'bufferobject_test.cc',
        'cubemaptexture_test.cc',
        'framebufferobject_test.cc',
        'glplatformcaps.inc',
        'graphicsmanager_test.cc',
        'image_test.cc',
        'indexbuffer_test.cc',
        'mockgraphicsmanager_test.cc',
        'mockresource_test.cc',
        'node_test.cc',
        'renderer_test.cc',
        'resourcemanager_test.cc',
        'sampler_test.cc',
        'shader_test.cc',
        'shaderinputregistry_test.cc',
        'shaderprogram_test.cc',
        'shape_test.cc',
        'statetable_test.cc',
        'texture_test.cc',
        'texturemanager_test.cc',
        'tracecallextractor_test.cc',
        'uniform_test.cc',
        'uniformblock_test.cc',
        'uniformholder_test.cc',
        'updatestatetable_test.cc',
      ],
      'conditions': [
        ['OS == "windows" and not angle', {
          'sources!': [
            'graphicsmanager_test.cc',
            'resourcemanager_test.cc',
          ],
        }],
        ['OS == "asmjs"', {
          'sources!': [
            # TODO(user): Re-enable this test by creating a stubbed out
            # canvas.
            'graphicsmanager_test.cc',
          ],
        }],
      ],
      'dependencies' : [
        '<(ion_dir)/base/base.gyp:ionbase_for_tests',
        '<(ion_dir)/external/external.gyp:ionzlib',
        '<(ion_dir)/external/gtest.gyp:iongtest_safeallocs',
        '<(ion_dir)/gfx/gfx.gyp:iongfx_for_tests',
        '<(ion_dir)/port/port.gyp:ionport',
        '<(ion_dir)/portgfx/portgfx.gyp:ionportgfx_for_tests',
      ],
    },
  ],
}
