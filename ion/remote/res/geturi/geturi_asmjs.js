// This function is similar to jQuery.ajax(): http://api.jquery.com/jQuery.ajax/
// except that it is not an asynchronous function. Instead, it takes the same
// arguments as ajax() but instead directly requests a url through a C-function
// wrapped by Emscripten (IonRemoteGet()). The passed settings object is a set
// of key/value pairs that configure the URI request, in particular url must be
// defined.
function getUri(settings) {
  var url = settings['url'];
  // Combine the data into a query string. Emscripten has no locking, but
  // operates in a single thread, so we use non-blocking callbacks.
  url += '?nonblocking';
  if (settings['data']) {
    var i = 0;
    for (var key in settings['data']) {
      url += '&' + key + '=' + encodeURIComponent(settings['data'][key]);
      i++;
    }
  }
  IonRemoteGet(url, settings['success'], settings['error']);
}

// This function gets a JSON struct from the passed url, and calls success if
// the request succeeds. It uses the above getUri() function to perform the
// request, and jQuery's parseJSON() to decode the returned JSON string.
function getJSON(url, success) {
  getUri({url: url, success: function(text) {
    if (success) {
      success(jQuery.parseJSON(text));
    }
  }});
}
