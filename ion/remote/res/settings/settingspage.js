/** window.Ion holds all global JavaScript objects made available by Ion. */
window.Ion = window.Ion || {};

/** window.Ion.SettingsPage provides the logic for an interactive web page that
    allows listing, filtering, setting, etc. of ion::base::Settings
    @constructor
    @param {Ion.Settings} s_settings_provider
*/
window.Ion.SettingsPage = function(s_settings_provider) {
  var s_settings = {};
  var s_groups = [];

  // Setup the autocomplete widget.
  $('#search').autocomplete({
    // Don't delay before updating results.
    delay: 0,
    // Override messages since we will do our own thing to display settings.
    messages: {
      noResults: '',
      results: function(amount) {
        return '';
      }
    },
    // Trigger callbacks even when there is no text.
    minLength: 0,
    response: function(event, ui) {
      if ($('#search').val() == '' || ui.content.length == 0) {
        $('#results').html('No settings match your search');
      } else {
        var table = $('<table></table>').attr('class', 'settings');
        for (var i = 0; i < ui.content.length; ++i) {
          var label = ui.content[i].label;
          var setting = s_settings[label];
          var row = $('<tr></tr>').attr('class', 'setting');
          row.append($('<td></td>')
                         .attr('class', 'setting_name')
                         .attr('title', setting.doc_string)
                         .text(label));

          var val;
          var slider = null;
          if (setting.type_desc.slice(0, 5) == 'enum:') {
            // Create a drop-down list for enums.
            val = $('<select></select>');
            var tokens = setting.type_desc.slice(5).split('|');
            for (var j = 0; j < tokens.length; ++j) {
              val.append(
                  $('<option></option>').attr('value', j).text(tokens[j]));
            }
            // Select the correct starting value.
            val.val(setting.getValue());
          } else if (setting.type_desc.slice(0, 6) == 'range:') {
            var tokens = setting.type_desc.slice(6).split('|');
            // Set the value the latest, as it will be otherwise rounded during
            // the subsequent attr() calls.
            slider = $('<input></input>')
                         .attr('type', 'range')
                         .attr('min', tokens[0])
                         .attr('max', tokens[1])
                         .attr('step', tokens[2])
                         .attr('value', setting.getValue());
            val = $('<input></input>')
                      .attr('size', 20)
                      .attr('value', setting.getValue())
                      .attr('type', 'text')
                      .attr('is_range', true);
          } else {
            val = $('<input></input>')
                .attr('size', 80)
                .attr('value', setting.getValue());
            if (setting.type_desc == 'bool') {
              val.attr('type', 'checkbox');
              if (setting.getValue() == 'true')
                val.attr('checked', true);
            } else {
              val.attr('type', 'text');
            }
          }
          val.attr('id', 'value_' + i).attr('setting_name', label);
          td = $('<td></td>').attr('class', 'setting_value').append(val);
          if (slider) {
            td.append(slider.attr('id', 'value_' + i + '_slider')
                          .attr('setting_name', label));
          }
          row.append(td);
          table.append(row);
        }
        $('#results').html(table);
      }
      location.hash = $('#search').val();

      // Set up handlers for each input.
      for (var i = 0; i < ui.content.length; ++i) {
        $('#value_' + i).change(function(event) {
          var name = $(this).attr('setting_name');
          var id = '#' + $(this).attr('id');
          if ($(this).tagName == 'select') {
            setSettingValue([id], name, $(id).val());
          } else if ($(this).attr('type') == 'checkbox') {
            setSettingValue(
                [id], name, $(id).prop('checked') ? 'true' : 'false');
          } else if ($(this).attr('is_range') == 'true') {
            setSettingValue([id, id + '_slider'], name, $(id).val());
          } else {
            setSettingValue([id], name, $(id).val());
          }
        });
        $('#value_' + i + '_slider').on('input', function() {
          var name = $(this).attr('setting_name');
          var slider_id = '#' + $(this).attr('id');
          var id = slider_id.substr(0, slider_id.lastIndexOf('_'));
          setSettingValue([id, slider_id], name, $(slider_id).val());
        });
      }

      // Clear the content so that the context menu does not appear.
      // Do this in-place, without creating a new array.
      ui.content.splice(0, ui.content.length);

      return false;
    },
    source: ['<None>']
  });

  // Updates the left panel list of groups.
  function updateGroupLinks() {
    var html = '';
    for (var i = 0; i < s_groups.length; ++i) {
      html += '<div class="group" id="group_' + i + '" group_name="' +
          s_groups[i] + '">' + s_groups[i] + '</div>\n';
    }
    $('#grouplist').html(html);
    for (var i = 0; i < s_groups.length; ++i) {
      $('#group_' + i).click(function(event) {
        $('#search').val('^' + $(this).attr('group_name'));
        $('#search').autocomplete('search');
      });
    }
  }

  // Sets the value of a setting and updates the input value if necessary.
  function setSettingValue(ids, name, valueText) {
    s_settings[name].setValue(valueText, function(updatedValueText) {
      for (var i = 0; i < ids.length; i++) {
        $(ids[i]).val(updatedValueText);
      }
    });
  }

  s_settings_provider.addListener(function(keys, settings, groups) {
    s_settings = settings;
    s_groups = groups;
    updateGroupLinks();

    // Set the values to use for autocomplete, and allow regex searches.
    $('#search').autocomplete('option', 'source', function(req, responder) {
      var re = $.ui.autocomplete.escapeRegex(req.term);
      var matcher = new RegExp(req.term, 'i');
      responder($.grep(keys, function(item, index) {
        return matcher.test(item);
      }));
    });

    // Now that the autocomplete is populated, set the initial search as the
    // URL hash.
    $('#search').val(location.hash.substring(1, location.hash.length));
    $('#search').autocomplete('search');
  });
};
