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

#include "ion/portgfx/isextensionsupported.h"

#include <cctype>
#include <cstring>

#include "ion/base/logging.h"
#include "ion/portgfx/glcontext.h"
#include "ion/portgfx/glheaders.h"

// ion/port/logging.h fixes conflict between the Windows global define of ERROR
// and the logging.h ERROR. However, glheaders.h can redefine it, so remove it
// again.
#ifdef ERROR
#undef ERROR
#endif

namespace ion {
namespace portgfx {

ION_API bool IsExtensionSupported(const std::string& unprefixed_extension,
                                  const std::string& extensions_string) {
  if (unprefixed_extension.empty())
    return false;

  // An extension is supported if it is in the list of GL extensions.
  size_t found = 0;
  while ((found = extensions_string.find(unprefixed_extension, found + 1)) !=
         std::string::npos) {
    // Check that the unmatched prefix of |unprefixed_extensions| is of the form
    //   API_FOO_BAR_unprefixed_extension
    size_t found_begin = extensions_string.rfind(' ', found);
    if (found_begin != std::string::npos) {
      ++found_begin;
    } else {
      found_begin = 0;
    }

    // Check that the API_FOO_BAR prefix is all uppercase or "_".
    for (size_t c = found_begin; c < found; ++c) {
      if (!isupper(extensions_string[c]) && extensions_string[c] != '_')
        goto next;
    }

    // Check that there is no part of the extension name after
    // |unprefixed_extension|.
    if (extensions_string.size() > found + unprefixed_extension.size() &&
        extensions_string[found + unprefixed_extension.size()] != ' ')
      goto next;

    // Matched!  Return true.
    return true;

  next:
    continue;
  }

  return false;
}

ION_API bool IsExtensionSupported(const char* unprefixed_extension) {
  const GlContextPtr gl_context = GlContext::GetCurrent();
  if (!gl_context) {
    // If there is no OpenGL context, we have no extensions. However, this
    // is probably a bug, so warn about it.
    LOG(WARNING) << "IsExtensionSupported(" << unprefixed_extension
                 << ") returning false because there is no OpenGL context.";
    return false;
  }

  using GetStringFn = const GLubyte* (*)(GLenum);
  const auto get_string_fn =
      reinterpret_cast<GetStringFn>(gl_context->GetProcAddress(
          "glGetString",
          GlContext::kProcAddressCore | GlContext::kProcAddressPure));
  if (!get_string_fn) {
    LOG(ERROR) << "IsExtensionSupported(" << unprefixed_extension
               << ") failed to retrieve extensions string.";
    return false;
  }

  const char* extensions =
      reinterpret_cast<const char*>(get_string_fn(GL_EXTENSIONS));
  if (!extensions) return false;
  const std::string unprefixed(unprefixed_extension);
  return IsExtensionSupported(std::string(unprefixed_extension),
                              std::string(extensions));
}

}  // namespace portgfx
}  // namespace ion
