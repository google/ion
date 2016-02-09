/**
Copyright 2016 Google Inc. All Rights Reserved.

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
