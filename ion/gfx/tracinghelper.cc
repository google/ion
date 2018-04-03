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

#include "ion/gfx/tracinghelper.h"

#include <string.h>  // For strcmp().

#include <sstream>

#include "ion/base/staticsafedeclare.h"
#include "ion/base/stringutils.h"
#include "ion/portgfx/glheaders.h"

namespace ion {
namespace gfx {

#if ION_PRODUCTION

template <typename T>
const std::string TracingHelper::ToString(const char*, T val) {
  std::ostringstream out;
  out << val;
  return out.str();
}

// Specialize for all types.
template ION_API const std::string TracingHelper::ToString(
    const char*, char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, char**);
template ION_API const std::string TracingHelper::ToString(
    const char*, const char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const char**);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned char);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned int);
template ION_API const std::string TracingHelper::ToString(
    const char*, const float*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const unsigned char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const unsigned int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const void*);
template ION_API const std::string TracingHelper::ToString(
    const char*, float);
template ION_API const std::string TracingHelper::ToString(
    const char*, float*);
template ION_API const std::string TracingHelper::ToString(
    const char*, int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, void*);
template ION_API const std::string TracingHelper::ToString(
    const char*, void**);
template ION_API const std::string TracingHelper::ToString(
    const char*, GLsync);
template ION_API const std::string TracingHelper::ToString(const char*,
                                                           GLDEBUGPROC);

#else

namespace {

// This helper function converts any type to a string.
template <typename T>
static const std::string AnyToString(const T& val) {
  std::ostringstream out;
  out << val;
  return out.str();
}

// Specialize for unsigned int to use hexadecimal.
template <> const std::string AnyToString(const unsigned int& val) {  // NOLINT
  std::ostringstream out;
  out << "0x" << std::hex << val;
  return out.str();
}

// This is used to convert a GLbitfield used for the glClear() call to a string
// indicating which buffers are being cleared. If anything is found to indicate
// it is a different type of GLbitfield, an empty string is returned.
static const std::string GetClearBitsString(GLbitfield mask) {
  std::string s;
  if (mask & GL_COLOR_BUFFER_BIT) {
    s += "GL_COLOR_BUFFER_BIT";
    mask &= ~GL_COLOR_BUFFER_BIT;
  }
  if (mask & GL_DEPTH_BUFFER_BIT) {
    if (!s.empty())
      s += " | ";
    s += "GL_DEPTH_BUFFER_BIT";
    mask &= ~GL_DEPTH_BUFFER_BIT;
  }
  if (mask & GL_STENCIL_BUFFER_BIT) {
    if (!s.empty())
      s += " | ";
    s += "GL_STENCIL_BUFFER_BIT";
    mask &= ~GL_STENCIL_BUFFER_BIT;
  }
  // If anything is left in the mask, assume it is something else.
  if (mask)
    s.clear();
  return s;
}

// This is used to convert a GLbitfield used for the glMapBufferRange() call to
// a string indicating the access mode for the buffer. If anything is found to
// indicate it is a different type of GLbitfield, an empty string is returned.
static const std::string GetMapBitsString(GLbitfield mode) {
  std::string s;
  if (mode & GL_MAP_READ_BIT) {
    s += "GL_MAP_READ_BIT";
    mode &= ~GL_MAP_READ_BIT;
  }
  if (mode & GL_MAP_WRITE_BIT) {
    if (!s.empty())
      s += " | ";
    s += "GL_MAP_WRITE_BIT";
    mode &= ~GL_MAP_WRITE_BIT;
  }
  // If anything is left in the mode, assume it is something else.
  if (mode)
    s.clear();
  return s;
}

// Helper function to print out values from an array-like type. By default this
// does nothing.
template <typename T>
static std::string ArrayToString(const std::string& type, T arg) {
  return std::string();
}

// A function that actually does the array printing.
template <typename T>
static std::string TypedArrayToString(const std::string& type, T arg) {
  std::ostringstream out;

  // Extract the number of elements from the end of the type.
  if (int count = base::StringToInt32(type.substr(type.length() - 2, 1))) {
    int rows = 1;
    // This assumes square matrices.
    if (type.find("matrix") != std::string::npos)
      rows = count;
    out << " -> [";
    for (int j = 0; j < rows; ++j) {
      for (int i = 0; i < count; ++i) {
        out << arg[j * count + i];
        if (i < count - 1)
          out << "; ";
      }
      if (j < rows - 1)
        out << " | ";
    }
    out << "]";
  }
  return out.str();
}

// Specializations for int and float pointers to allow array-like types to be
// printed.
template <>
std::string ArrayToString<const int*>(const std::string& type, const int* arg) {
  return TypedArrayToString(type, arg);
}
template <>
std::string ArrayToString<const float*>(const std::string& type,
                                        const float* arg) {
  return TypedArrayToString(type, arg);
}

}  // anonymous namespace

const TracingHelper::GlEnumMap& TracingHelper::GetGlEnumMap() {
  ION_DECLARE_SAFE_STATIC_POINTER_WITH_CONSTRUCTOR(
      GlEnumMap, s_enums, (CreateGlEnumMap()));
  return *s_enums;
}

// Unspecialized version.
template <typename T>
const std::string TracingHelper::ToString(const char* arg_type, T arg) {
  // Treat pointers specially.
  const std::string arg_type_str(arg_type);
  if (arg_type_str.find('*') != std::string::npos ||
      arg_type_str.find("PROC") != std::string::npos) {
    if (arg != static_cast<T>(0)) {
      std::ostringstream out;
      out << "0x" << std::hex << *reinterpret_cast<size_t*>(&arg);

      // If the pointer type is a known type then we can print more deeply.
      out << ArrayToString(arg_type, arg);
      return out.str();
    } else {
      return "NULL";
    }
  }

  return AnyToString(arg);
}

// Specialize to add quotes around strings.
template <> ION_API
const std::string TracingHelper::ToString(const char* arg_type,
                                          const char* arg) {
  return arg ? std::string("\"") + arg + '"' : "NULL";
}

// Specialize to add quotes around strings.
template <> ION_API
const std::string TracingHelper::ToString(const char* arg_type, char* arg) {
  return arg ? std::string("\"") + arg + '"' : "NULL";
}

// Specialize to print the first string in an array of strings.
template <> ION_API const std::string TracingHelper::ToString(
    const char* arg_type, const char** arg) {
  return arg ? "[" + ToString(arg_type, arg[0]) + ", ...]" : "NULL";
}

// Specialize to deal with GLboolean values.
template <> ION_API
const std::string TracingHelper::ToString(
    const char* arg_type, unsigned char arg) {
  // unsigned char is used only for GLboolean.
  switch (arg) {
    case 0:
      return "GL_FALSE";
    case 1:
      return "GL_TRUE";
    default: {
      return AnyToString(static_cast<int>(arg));
    }
  }
}

// Specialize to deal with GLenum values.
template <> ION_API
const std::string TracingHelper::ToString(const char* arg_type, int arg) {
  const GlEnumMap& enums = GetGlEnumMap();
  auto it = enums.find(arg);
  // int is sometimes used for parameter types and GLbitfield.
  if (!strcmp(arg_type, "GLtextureenum")) {
    // For texture parameters, only print certain valid values as enums, the
    // rest are just integers.
    if (arg >= 0 && it != enums.end()) {
      switch (arg) {
        case 0:
          return "GL_NONE";
        case GL_ALPHA:
        case GL_ALWAYS:
        case GL_BLUE:
        case GL_CLAMP_TO_EDGE:
        case GL_COMPARE_REF_TO_TEXTURE:
        case GL_EQUAL:
        case GL_GEQUAL:
        case GL_GREATER:
        case GL_GREEN:
        case GL_LESS:
        case GL_LINEAR:
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LEQUAL:
        case GL_MIRRORED_REPEAT:
        case GL_NEAREST:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_NEVER:
        case GL_NOTEQUAL:
        case GL_RED:
        case GL_REPEAT:
          return it->second;
        default:
          break;
      }
      std::string name = it->second;
      if (base::StartsWith(name, "GL_TEXTURE"))
        return name;
    }
  } else if (!strcmp(arg_type, "GLintenum")) {
    if (arg >= 0 && it != enums.end()) {
      return it->second;
    }
  }

  return AnyToString(arg);
}

// Specialize to deal with GLenum values.
template <> ION_API
const std::string TracingHelper::ToString(const char* arg_type,
                                          unsigned int arg) {
  const GlEnumMap& enums = GetGlEnumMap();
  auto it = enums.find(arg);
  // unsigned int is used for GLenum types, GLbitfield, GLmapaccess (a kind of
  // GLbitfield), and GLuint.
  if (!strcmp(arg_type, "GLblendenum")) {
    if (arg == GL_ZERO)
      return "GL_ZERO";
    else if (arg == GL_ONE)
      return "GL_ONE";
    else if (it != enums.end())
      return it->second;
  } else if (!strcmp(arg_type, "GLstencilenum")) {
    if (arg == GL_ZERO)
      return "GL_ZERO";
    else if (it != enums.end())
      return it->second;
  } else if (!strcmp(arg_type, "GLenum")) {
    if (it != enums.end())
      return it->second;
  } else if (!strcmp(arg_type, "GLbitfield")) {
    // GLbitfield is used for glClear().
    const std::string s = GetClearBitsString(arg);
    if (!s.empty())
      return s;
  } else if (!strcmp(arg_type, "GLmapaccess")) {
    // GLmapaccess is used for glMapBufferRange().
    const std::string s = GetMapBitsString(arg);
    if (!s.empty())
      return s;
  } else if (!strcmp(arg_type, "GLtextureenum")) {
    if (arg == GL_NONE)
      return "GL_NONE";
    else if (it != enums.end())
      return it->second;
  } else if (!strcmp(arg_type, "GLbufferenum")) {
    if (arg == GL_NONE)
      return "GL_NONE";
    else if (it != enums.end())
      return it->second;
  }
  return AnyToString(arg);
}

// Explicitly instantiate all the other unspecialized versions.
template ION_API const std::string TracingHelper::ToString(
    const char*, const float*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const unsigned char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const unsigned int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, const void*);
template ION_API const std::string TracingHelper::ToString(
    const char*, float);
template ION_API const std::string TracingHelper::ToString(
    const char*, float*);
template ION_API const std::string TracingHelper::ToString(
    const char*, int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, long long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned char*);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned int*);
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long long);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, unsigned long long*);  // NOLINT
template ION_API const std::string TracingHelper::ToString(
    const char*, void*);
template ION_API const std::string TracingHelper::ToString(
    const char*, void**);
template ION_API const std::string TracingHelper::ToString(
    const char*, GLsync);
template ION_API const std::string TracingHelper::ToString(const char*,
                                                           GLDEBUGPROC);

#endif  // ION_PRODUCTION

}  // namespace gfx
}  // namespace ion
