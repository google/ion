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
