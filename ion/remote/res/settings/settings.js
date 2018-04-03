/** window.Ion holds all global JavaScript objects made available by Ion. */
window.Ion = window.Ion || {};

/**
  *  window.Ion.Settings retrieves a list of ion::base::Settings from a running
  *  Ion program.  It also lists groups of settings, and allows setting values
  *  to be set.
  *  @constructor
  */
window.Ion.Settings = function() {
  // Set in success-callback of getAllSettingsAndGroups().
  var s_settings = null;
  var s_groups = null;
  // List of callback functions(keys, settings, groups) which are invoked when
  // the list of settings/groups changes.
  var s_listeners = [];

  /** Simple wrapper for accessing Setting data, including the ability to
      set its value in the Ion application.
      @param {string} name Name of the setting.
      @param {string} type_desc Description of the data-type of the setting.
      @param {string} doc_string Description of the setting's purpose.
      @param {string} value Value of the setting, in type-specific encoding.
      @constructor
  */
  var Setting = function(name, type_desc, doc_string, value) {
    this.name = name;
    this.type_desc = type_desc;
    this.doc_string = doc_string;

    this.setValue = function setValue(newValue, valueListener) {
      // An error indicates that the value could not be set.
      getUri({
        url: '/ion/settings/set_setting_value',
        data: {
          name: name,
          value: newValue
        },
        success: function(text) {
          // Update the value with the data from the server.
          value = text;
          valueListener(value);
        },
        error: function(jqXHR, textStatus, errorThrown) {
          // Revert the value text since it seems to be erroneous.
          valueListener(value);
        },
        dataType: 'text'
      });
    };

    this.getValue = function() { return value; };
  };

  // Retrieves all settings from the server and parses their names, values, and
  // the groups they belong to.
  var getAllSettingsAndGroups = function getAllSettingsAndGroups() {
    getUri({url: '/ion/settings/get_all_settings', success: function(text) {
      var array = text.split('|');

      s_groups = [];
      s_settings = {};

      // Update dict of settings and sort it.
      var keys = [];
      for (var i = 0; i < array.length - 1; ++i) {
        var tokens = array[i].split('/');
        // All values are URL-encoded.
        var name = unescape(tokens[0]);
        var type_desc = unescape(tokens[1]);
        var doc_string = unescape(tokens[2]);
        var value = unescape(tokens[3]);
        keys.push(name);
        s_settings[name] = new Setting(name, type_desc, doc_string, value);

        // Extract the groups from the name.
        var group_names = name.split('/');
        if (group_names.length > 1)
          group_names.pop();
        for (var j = 0; j < group_names.length; ++j) {
          if (j)
            group_names[j] = group_names[j - 1] + '/' + group_names[j];
          if (s_groups.indexOf(group_names[j]) == -1)
            s_groups.push(group_names[j]);
        }
      }
      // Sort all the arrays.
      keys.sort();
      s_groups.sort();

      for (var i = 0; i < s_listeners.length; i++) {
        s_listeners[i](keys, s_settings, s_groups);
      }
    }});
  };

  this.addListener = function addListener(listener) {
    s_listeners.push(listener);
  };

  this.update = getAllSettingsAndGroups;
};
