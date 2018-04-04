// This function is a wrapper around jQuery.ajax():
// http://api.jquery.com/jQuery.ajax/. The passed settings object is a set of
// key/value pairs that configure the URI request
function getUri(settings) {
  jQuery.ajax(settings);
}

// This function gets a JSON struct from the passed url, and calls success if
// the request succeeds. It uses the above getUri() function to perform the
// request.
function getJSON(url, success) {
  getUri({url: url, success: function(text) {
    if (success) {
      success(text);
    }
  }});
}
