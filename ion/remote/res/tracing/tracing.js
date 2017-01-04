/**
Copyright 2017 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS-IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

jQuery(document).ready(function() {
  $('#trace_next_frame').click(function() {
    getUri({
      url: '/ion/tracing/trace_next_frame',
      data: {
        resources_to_delete:
          ($('#delete_resources').is(':checked') == true) ?
              $('#resources_to_delete option:selected').map(
                  function() { return this.value }).get().join(',') : ''
      },
      success: function(text) {
        var trace = $('#trace');
        trace.html(text);
      }});
  });
  $('#clear').click(function() {
    getUri({url: '/ion/tracing/clear',
      success: function(text) {
        var trace = $('#trace');
        trace.html('');
      }});
  });
});
