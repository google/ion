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
  $('#address_printing').click(function() {
    var button = $('#address_printing');
    button.toggleClass('button button_toggled');
    if (button.hasClass('button_toggled')) {
      button.html('Disable Address Printing');
    } else {
      button.html('Enable Address Printing');
    }
  });

  $('#full_shape_printing').click(function() {
    var button = $('#full_shape_printing');
    button.toggleClass('button button_toggled');
    if (button.hasClass('button_toggled')) {
      button.html('Disable Full Shape Printing');
    } else {
      button.html('Enable Full Shape Printing');
    }
  });

  $('#update').click(function() {
    getUri({
      url: '/ion/nodegraph/update',
      data: {
        format: $('#format').val(),
        enable_address_printing:
            $('#address_printing').hasClass('button_toggled'),
        enable_full_shape_printing:
            $('#full_shape_printing').hasClass('button_toggled')
      },
      success: function(text) {
        var nodes = $('#nodes');
        nodes.html(text);

        // Query all the node toggle checkboxes (those under root, of checkbox
        // type, but not the tree_expandbox class) and give them a click
        // behavior to send the toggle query to the embedded server.
        var checkboxes = $("#nodes :checkbox:not(.tree_expandbox)");
        for (var i = 0; i < checkboxes.length; i++) {
          var checkbox_i = checkboxes[i];

          // This IIFE closure exists to capture `checkbox_i` in a new variable, to
          // generate a family of click functions with different queries.
          (function (checkbox) {
            $(checkbox).click(function() {
              getUri({
                url: '/ion/nodegraph/set_node_enable',
                data: {
                  node_label: checkbox.id,
                },
                success: function(text) {
                  // Display the error on any non-successful query.
                  if (text != "Success") {
                    window.alert(text);
                  }
                }});
            });
          })(checkbox_i);
        }
      }});
  });
});
