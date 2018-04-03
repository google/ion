jQuery(document).ready(function() {
  // Resource tabs.
  var buffers;
  var framebuffers;
  var programs;
  var samplers;
  var shaders;
  var textures;
  var vertex_arrays;
  var tab_index_map = {
    'buffers' : 1,
    'framebuffers' : 2,
    'programs' : 3,
    'samplers' : 4,
    'shaders' : 5,
    'textures' : 6,
    'vertex_arrays' : 7
  };
  var tab_map = [];

  // Converts a stringn from underscore to camel case.
  function toCamelCase(str) {
    var string = str.replace(/[\-_\s]+(.)?/g, function(match, chr) {
      return chr ? ' ' + chr.toUpperCase() : '';
    });
    // Ensure 1st char is always uppercase.
    string = string.replace(/^([a-z])/, function(match, chr) {
      return chr ? chr.toUpperCase() : '';
    });
    return string;
  }

  // Returns whether a string ends with end.
  function endsWith(str, end) {
    return str.match(end + '$') == end;
  }

  // Sets the passed div's HTML to be a texture's image.
  function getTexture(div) {
    var id = div.attr('id');
    var image = getUri({url: '/ion/resources/texture_data',
      data: {id: id}, success: function(data) {
        div.html('<img src="data:image/png;base64,' + data + '">');
      }
    });
  }

  function ResourceTab(id) {
    this.id = id;
    this.tab = $('#' + this.id);
    this.loaded = false;
    this.selected = 0;
    this.first = true;
  }

  // Writes the HTML for a single struct as an unordered list.
  function writeStruct(obj, data_callback) {
    var ul = $('<ul></ul>\n');
    // Write each field.
    $.each(obj, function(key, value) {
      // Recursively write structs and arrays.
      if (typeof value == 'object') {
        var li = $('<li>' + toCamelCase(String(key)) + '</li>\n');
        li.append(writeStruct(value));
        ul.append(li);
      } else {
        // Field values that are resource references end in '_glid'. We will
        // add a link to that resource for easy navigation.
        var add_link = false;
        var li = null;
        if (typeof key == "number") {
          // This is an array.
          li = $('<li>' + value + '</li>\n');
        } else {
          if (key == 'source') {
            // Shader sources are mime64 encoded.
            value = atob(value);
          }
          if (endsWith(key, '_glid') && value != 0) {
            key = key.substring(0, key.length - 5);
            add_link = true;
          }
          li = $('<li>' + toCamelCase(key) + ': ' + value + '</li>\n');
          if (add_link) {
            // Add a pseudo-link to the referenced resource.
            var span = $('<span class="gl_link">(Go to resource)</span>\n');
            span.click(function() {
              if (endsWith(key, '_shader'))
                key = 'shader';
              // Get the correct main tab and activate it.
              var tab_name = key + 's';
              $('#tabs').tabs('option', 'active', tab_index_map[tab_name]);
              // Select the option with the right object id.
              $('#' + tab_name + '_select').val(value);
              $('#' + tab_name + '_select').change();
            });
            li.append(span);
          }
        }
        ul.append(li);
      }
    });
    return ul;
  }

  // Writes the HTML for a class of resources.
  ResourceTab.prototype.writeResources = function(objs, data_callback) {
    if (objs.length == 0) {
      this.tab.append('There are no resources to display.');
      return;
    }
    // Create a select box for the resources.
    var select_td = $('<td style="vertical-align: top;"></td>\n');
    var select = $('<select id="' + this.id + '_select"></select>\n');
    select.attr('size', Math.max(2, Math.min(objs.length, 40)));

    var data_span_id = this.id + '_data';
    var data_span =
        $('<td style="vertical-align: top; width: 100%"><span id="' +
          data_span_id + '" class="resource_data"></span></td>\n');

    // Write an option for each resource.
    var name_lower = this.id.substring(0, this.id.length - 1);
    var name_upper = toCamelCase(name_lower);
    for (var i = 0; i < objs.length; ++i) {
      var obj = objs[i];
      var label = obj.label == '' ? '' : ' (' + obj.label + ')';
      var option = $('<option value="' + obj.object_id + '">' + name_upper +
                     ' ' + obj.object_id + label + '</option>\n');
      select.append(option);
    }
    select_td.append(select);

    // Save the tab so that the following callback can access it.
    var tab = this;

    // Write the resource's data when the selection changes.
    select.change(function() {
      var i = $(this).prop('selectedIndex');
      // Save the selected index.
      var span = $('<span></span>\n');
      tab.selected = i;
      if (data_callback) {
        var display = $('<span class="display_object" width=300 id="' +
            objs[i].object_id + '"></span>\n');
        display.append('Click to retrieve ' + name_lower + ' data');
        span.append(display);
        display.click(function() {
          data_callback($(this));
        });
      }
      span.append(writeStruct(objs[i], data_callback));
      $('#' + data_span_id).html(span);
    });

    var main_span = $('<span></span>\n');
    var row = $('<tr></tr>');
    row.append(select_td);
    row.append(data_span);
    var table = $('<table></table>\n');
    table.append(row);
    main_span.append(table);
    main_span.append($('<div style="clear: both;"></div>'));
    this.tab.html(main_span);

    // Select and and show the first resource by default the first time the
    // panel is shown.
    select.prop('selectedIndex', this.selected);
    select.change();
  };

  // Make all tabs.
  $('#tabs').tabs({
    beforeActivate: function(event, ui) {
      // Get the ResourceTab corresponding to ui.newTab.
      var tab = tab_map[ui.newTab.index()];
      // Make sure the tab content is loaded.
      if (tab && !tab.loaded) {
        // Write in the page that resources are loading?
        tab.loaded = true;
        tab.tab.append('Fetching resources from application.... ');
        getJSON('/ion/resources/resources_by_type?types=' + tab.id,
          function(resources) {
            // Update resource tab.
            var callback = tab.id == 'textures' ? getTexture : null;
            tab.writeResources(resources[tab.id], callback);
          }
        );
      }
    }
  });
  buffers = new ResourceTab('buffers');
  framebuffers = new ResourceTab('framebuffers');
  programs = new ResourceTab('programs');
  samplers = new ResourceTab('samplers');
  shaders = new ResourceTab('shaders');
  textures = new ResourceTab('textures');
  vertex_arrays = new ResourceTab('vertex_arrays');
  // The first entry is null since platform info is first.
  tab_map = [
    null,
    buffers,
    framebuffers,
    programs,
    samplers,
    shaders,
    textures,
    vertex_arrays
  ];

  // Retrieves resources from the server and constructs the DOM.
  function initResourceTabs(type) {
    getJSON('/ion/resources/resources_by_type?types=platform',
            function(resources) {
      // Writes platform info.
      function writePlatformInfo(id, obj) {
        var div = $('<div id="platform_info"></div>\n');
        div.append(writeStruct(obj));
        $('#' + id).html(div);
      }

      // Create all resource tabs.
      writePlatformInfo('platform', resources.platform);
    });
  }

  // When the user click's the refresh button mark all tabs as unloaded.
  $('#refresh').click(function() {
    for (var i = 1; i < tab_map.length; i++) {
      tab_map[i].loaded = false;
    }
    // Switch back to platform info and then to the active tab to force a
    // refresh.
    var active = $('#tabs').tabs('option', 'active');
    // There is nothing to do if we are on the platform tab.
    if (active > 0) {
      $('#tabs').tabs('option', 'active', 0);
      $('#tabs').tabs('option', 'active', active);
    }
  });


  initResourceTabs();
});
