/**
 * A kind of minitabs with a callback.
 * @this {jQuery}
 * @param {function} callback A calback called when a tab is clicked.
 */
jQuery.fn.minitabs = function(callback) {
  var id = '#' + this.attr('id');
  $(id + '>DIV:gt(0)').hide();
  $(id + '>UL>LI>A:first').addClass('current');
  $(id + '>UL>LI>A').click(function() {
    $(id + '>UL>LI>A').removeClass('current');
    $(this).addClass('current');
    $(this).blur();
    if (jQuery.isFunction(callback))
      callback.call($(this));
    return false;
  });
};

jQuery(document).ready(function() {
  var SHADER_ITEM_HTML =
      '<li style="display: inline-block;">' +
      ' <div id="SHADER_ID" shader_name="SHADER_NAME" style="text-decoration:' +
      ' underline; cursor: pointer; cursor: hand; padding: 0.2em;' +
      ' color: #COLOR">SHADER_NAME</div></li>\n';

  var s_activeId = 'no_active_program';
  var s_activeProgram = 'no_active_program';
  var s_showHelp = false;
  var doclist = require('shaders/doclist');
  var s_id_map = [];

  // Requests shader data from the tweak server and updates the display.
  function getShadersAndStatus() {
    getUri({url: '/ion/shaders/shader_status', success: function(text) {
      var array = text.split('\n');
      var content = '<h3>Active GLSL programs</h3>' +
        '<ul style="padding: 0; margin: 0; list-style-type: none; ' +
        'font-size: 90%;">\n';
      s_id_map = [];
      for (var i = 0; i < array.length; ++i) {
        var itemContent = SHADER_ITEM_HTML;
        // Shader name is the first element, followed by the logs for the
        // three stages (vertex, fragment, link).
        var items = array[i].split(',');
        // DOM items can't have spaces in their names.
        var name = 'shader_' + i;
        s_id_map[items[0]] = name;
        itemContent = itemContent.replace(/SHADER_ID/g, name);
        itemContent = itemContent.replace(/SHADER_NAME/g, items[0]);
        if (items[1] == 'OK' && items[2] == 'OK' && items[3] == 'OK')
          itemContent = itemContent.replace(/COLOR/g, '0a0');
        else
          itemContent = itemContent.replace(/COLOR/g, 'a00');

        content += itemContent;
      }

      content += '</ul><br>\n';
      $('#shader_list').html(content);
      $('#' + s_activeId).css('background-color', '#FFFFAA');
      for (var i = 0; i < array.length; ++i) {
        // Set up handlers for clicks on program names.
        var items = array[i].split(',');
        var name = 'shader_' + i;
        $('#' + name).click(function(event) {
          // Get the sources for the named shader.
          getShaderSources($(this).attr('shader_name'));
        });
      }
    }});
  }

  // Gets the info logs of the program.
  function getInfoLogs() {
    function processInfoLogText(stage, text) {
      $('#' + stage + '_output').html(text);
      if (text == 'OK') {
        $('#' + stage + '_output').css({color: '#3c3'});
      } else {
        $('#' + stage + '_output').css({color: '#c33'});
      }
    }

    getUri({url: '/ion/shaders/' + s_activeProgram + '/vertex/|info log|',
      data: {
        raw: ''
      },
      success: function(text) {
        processInfoLogText('vertex', text);
      }
    });
    getUri({url: '/ion/shaders/' + s_activeProgram + '/fragment/|info log|',
      data: {
        raw: ''
      },
      success: function(text) {
        processInfoLogText('fragment', text);
      }
    });
    getUri({url: '/ion/shaders/' + s_activeProgram + '/|info log|',
      data: {
        raw: ''
      },
      success: function(text) {
        processInfoLogText('link', text);
      }
    });
  }

  // *** This needs to get for all three stages.
  doclist.changed = function() {
    getShadersAndStatus();
    getInfoLogs();
  };

  function getShaderSources(name) {
    // Reset the old div.
    $('#' + s_activeId).css('background-color', '#272822');
    s_activeProgram = name;
    s_activeId = s_id_map[name];
    $('#' + s_activeId).css('background-color', '#FFFFAA');

    function getInputsAndCreateTabs(stage) {
      // Create a tab for each input.
      function createTabs(inputs, stage) {
        var ul = $('<ul id="minitabs"></ul>');
        if (inputs[0] != '') {
          for (var i = 0; i < inputs.length; ++i) {
            var li = $('<li><a  stage="' + stage + '" title="' + inputs[i] +
              '" href="#">' + inputs[i] + '</a></li>\n');
            ul.append(li);
          }
        }
        $('#' + stage + '_tabs').html(ul);
        $('#' + stage + '_tabs').minitabs(function() {
          var name = this.attr('title');
          if (stage == 'vertex')
            doclist.setLeftDoc(s_activeProgram, 'vertex', name);
          else if (stage == 'fragment')
            doclist.setRightDoc(s_activeProgram, 'fragment', name);
        });
      }

      getUri({url: '/ion/shaders/' + s_activeProgram + '/' + stage,
        data: {
          raw: ''
        },
        success: function(text) {
          var inputs = text.split('\n');
          // The first entry is the info log.
          inputs = inputs.slice(1);
          createTabs(inputs, stage, stage);
          if (stage == 'vertex')
            doclist.setLeftDoc(s_activeProgram, 'vertex', inputs[0]);
          else if (stage == 'fragment')
            doclist.setRightDoc(s_activeProgram, 'fragment', inputs[0]);
        }
      });
    }
    getInputsAndCreateTabs('vertex');
    getInputsAndCreateTabs('fragment');

    // Get the info logs of the program.
    getInfoLogs();
  }

  function displayStringizedSource(editor) {
    // Get the source.
    var lines = editor.getValue().split('\n');
    // Wrap each line in quotes with a newline at the end of each line.
    var string = 'static const char source[] = ';
    for (var i = 0; i < lines.length; ++i) {
      string += '\n    "' + lines[i] + '\\n"';
    }
    string += ';';

    // Display the string in a popup box.
    var source_window = window.open('', '', 'width=640,height=800');
    source_window.document.write('<pre>' + string + '</pre>');
    source_window.focus();
  }

  // Continuously polls the server to see if any dependencies have changed.
  function watchInputs() {
    if ($('#auto_reload_shaders').is(':checked') == true) {
      getUri({
        url: '/ion/shaders/update_changed_dependencies',
        success: function(text) {
          var array = text.split(';');
          // Reload any changed dependencies.
          for (var i = 0; i < array.length; ++i) {
            doclist.reloadFile(array[i]);
          }
          getShadersAndStatus();
        }
      });
      setTimeout(watchInputs, 750);
    }
  }

  // Bind actions to form buttons.
  $('#refresh_shaders').click(function() {
    getShadersAndStatus();
  });

  $('#reload_shaders').click(function() {
    doclist.reloadFiles();
    getShadersAndStatus();
  });

  $('#fragment_string').click(function() {
    displayStringizedSource(env.editor1);
  });

  $('#vertex_string').click(function() {
    displayStringizedSource(env.editor0);
  });

  // Bind help text actions.
  $('#help_container').click(function() {
    if (s_showHelp) {
      s_showHelp = false;
      $('#help_container_text').html('<h3>Click for help</h>');
      $('#help').hide();
    } else {
      s_showHelp = true;
      $('#help_container_text').html('<h3>Click to hide help</h');
      $('#help').show();
    }
  });

  $('#auto_reload_shaders').click(function() {
    watchInputs();
  });

  // Initialize shader editors.
  var init = require('shaders/init');
  getShadersAndStatus();
});

define('shaders/init', ['require', 'exports', 'module',
                        'ace/lib/fixoldbrowsers', 'ace/config',
                        'ace/theme/monokai', 'ace/multi_select',
                        'shaders/doclist', 'ace/split'],
                        function(require, exports, module) {
  require('ace/lib/fixoldbrowsers');
  require('ace/config').init();
  var env = {};
  var doclist = require('./doclist');
  var defaultCommands = require('ace/commands/default_commands').commands;
  var emacs = require('ace/keyboard/emacs').handler;
  var theme = require('ace/theme/monokai');
  var useragent = require('ace/lib/useragent');
  var util = require('./util');
  var vim = require('ace/keyboard/vim').handler;
  var container = document.getElementById('shader_source_editor');
  var CommandManager = require('ace/commands/command_manager').CommandManager;
  var Editor = require('ace/editor').Editor;
  var KeyBinding = require('ace/keyboard/keybinding').KeyBinding;
  var Renderer = require('ace/virtual_renderer').VirtualRenderer;
  var Split = require('ace/split').Split;
  // Override Split's create and resize functions to fit the editors within an
  // element.
  Split.prototype.$createEditor = function() {
    var el = document.createElement('div');
    el.className = this.$editorCSS;
    el.style.cssText = '';
    this.$container.appendChild(el);
    var editor = new Editor(new Renderer(el, this.$theme));

    editor.on('focus', function() {
        this._emit('focus', editor);
    }.bind(this));

    this.$editors.push(editor);
    editor.setFontSize(this.$fontSize);
    return editor;
  };
  Split.prototype.resize = function() {
    var container = document.getElementById('shader_source_editor');
    var width = container.clientWidth;
    var height = container.clientHeight;
    var editor;
    var border = .0125;
    var editorWidth = (width * (1. - border)) / this.$splits;
    for (var i = 0; i < this.$splits; i++) {
      editor = this.$editors[i];
      editor.container.style.width = editorWidth + 'px';
      editor.container.style.top = 'absolute';
      editor.container.style.left = border * width + i * editorWidth + 'px';
      editor.container.style.height = container.style.height;
      editor.resize();
    }
  };

  function setKeybindings(editor, bindings) {
    editor.commands = new CommandManager(useragent.isMac ? 'mac' : 'win',
                                         defaultCommands);
    editor.keyBinding = new KeyBinding(editor);

    if (bindings == 'default') {
      // Allow saving changes to disk.
      editor.commands.addCommand({
          name: 'save',
          bindKey: {
              win: 'Ctrl-S',
              mac: 'Command-S'
          },
          exec: function(editor) {
            var doc = editor.session.docref;
            // Get the doc name of the editor.
            var name = doc.name;
            // Send the source to the server.
            getUri({url: '/ion/shaders/' + doc.program_name + '/' + doc.stage +
                       '/' + doc.filename, data: {
                set_source: editor.getValue()
              },
              success: function(text) {
                // Have to get status of all programs again.
                doclist.changed();
              }
            });
          }
      });

      editor.commands.addCommand({
        name: 'findnext',
        bindKey: {
            win: 'Ctrl-G',
            mac: 'Command-G'
        },
        exec: function(editor) {
          editor.findNext();
        }
      });

      editor.commands.addCommand({
        name: 'findprevious',
        bindKey: {
            win: 'Ctrl-Shift-G',
            mac: 'Command-Shift-G'
        },
        exec: function(editor) {
          editor.findPrevious();
        }
      });
    } else if (bindings == 'emacs') {
      editor.setKeyboardHandler(emacs);
    } else if (bindings == 'vim') {
      editor.setKeyboardHandler(vim);
    }
  }

  var split = new Split(container, theme, 2);
  env.editor = split.getEditor(0);
  env.editor0 = split.getEditor(0);
  env.editor1 = split.getEditor(1);
  env.container = container;
  for (var i = 0; i < 2; ++i) {
    var editor = split.getEditor(i);
    editor.setSelectionStyle('line');
    editor.setHighlightActiveLine(true);
    editor.setShowInvisibles(false);
    editor.setDisplayIndentGuides(false);
    editor.renderer.setShowGutter(true);
    editor.renderer.setShowPrintMargin(true);
    editor.setHighlightSelectedWord(true);
    editor.renderer.setHScrollBarAlwaysVisible(true);
    editor.setAnimatedScroll(true);
    editor.setBehavioursEnabled(true);
    editor.setFadeFoldWidgets(false);
    editor.session.setFoldStyle('markbeginend');
    editor.setShowFoldWidgets(true);
    editor.setAnimatedScroll(true);
    editor.setTheme('ace/theme/monokai');

    editor.session.setTabSize(2);
    editor.session.setUseSoftTabs(true);
    editor.session.setUseWrapMode(true);
    editor.session.setWrapLimitRange(80, 80);
    editor.session.setMode('ace/mode/glsl');

    setKeybindings(editor, 'default');

    require('ace/multi_select').MultiSelect(editor);
  }

  split.on('focus', function(editor) {
    env.editor = editor;
  });
  split.setOrientation(split.BESIDE);
  split.setSplits(2);
  env.split = split;

  window.env = env;
  window.ace = env.editor;

  $('#keybindings').change(function() {
    //setKeybindings(env.editor0, $(this).val());
    //setKeybindings(env.editor1, $(this).val());
  });

  $('#fontsize').change(function() {
    env.split.setFontSize($(this).val());
  });

  function onResize() {
    var left = env.split.$container.offsetLeft;
    var width = document.documentElement.clientWidth - left;
    container.style.width = width + 'px';
    container.style.height = env.container.height;
    env.split.resize();
  }
  window.onresize = onResize;

  onResize();
}); // shaders/init

define('shaders/doclist', [
    'require',
    'exports',
    'module',
    'ace/edit_session',
    'ace/undomanager',
    'ace/lib/net'
], function(require, exports, module) {

  var EditSession = require('ace/edit_session').EditSession;
  var UndoManager = require('ace/undomanager').UndoManager;
  var net = require('ace/lib/net');

  var fileCache = {};

  function initDoc(text, doc) {
    if (doc.session) {
      doc.session.setValue(text);
    } else {
      var session = new EditSession(text);
      session.setTabSize(2);
      session.setUseSoftTabs(true);
      session.setUseWrapMode(true);
      session.setWrapLimitRange(80, 80);
      session.setMode('ace/mode/glsl');
      session.setUndoManager(new UndoManager());
      session.docref = doc;
      doc.session = session;
    }
  }

  if (window.require && window.require.s)
    try {
      for (var path in window.require.s.contexts._.defined) {
        if (path.indexOf('!') != -1)
          path = path.split('!').pop();
        else
          path = path + '.js';
      }
    } catch (e) {
    }

  function getDoc(doc, callback) {
    getUri({url: '/ion/shaders/' + doc.program_name + '/' + doc.stage + '/' +
            doc.filename, success: function(x) {
      initDoc(x, doc);
      if (callback)
        callback(doc.session);
    }});
  }

  function loadDoc(program_name, stage, filename, callback) {
    var docname = filename;
    var doc = fileCache[docname];
    if (!doc) {
      doc = {};
      doc.program_name = program_name;
      doc.name = docname;
      doc.stage = stage;
      doc.filename = filename;
      fileCache[docname] = doc;
    }

    // Doc is already loaded.
    if (doc.session)
      return callback(doc.session);

    // Doc not cached, so load it.
    getDoc(doc, callback);
  }

  function setDoc(editor, program_name, stage, filename) {
    loadDoc(program_name, stage, filename, function(session) {
      if (!session)
        return;
      editor.setSession(session);
      editor.session = session;
      editor.focus();
    });
  }

  function setLeftDoc(program_name, stage, filename) {
    setDoc(env.editor0, program_name, stage, filename);
  }

  function setRightDoc(program_name, stage, filename) {
    setDoc(env.editor1, program_name, stage, filename);
  }

  // Reload all the files in the cache.
  function reloadFiles() {
    for (var key in fileCache) {
      getDoc(fileCache[key], null);
    }
  }

  // Reload a specific file.
  function reloadFile(name) {
    for (var key in fileCache) {
      var doc = fileCache[key];
      if (doc.filename == name)
        getDoc(doc, null);
    }
  }

  module.exports = {
      reloadFile: reloadFile,
      reloadFiles: reloadFiles,
      setLeftDoc: setLeftDoc,
      setRightDoc: setRightDoc
  };
});
