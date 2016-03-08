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

#ifndef ION_GFX_GRAPHICSMANAGERMACRODEFS_H_
#define ION_GFX_GRAPHICSMANAGERMACRODEFS_H_

// This file defines some magic preprocessor macros for wrapping
// OpenGL functions easily in the GraphicsManager class.

#if defined(ION_ANALYTICS_ENABLED)
#  define ION_PROFILE_GL_FUNC(name) \
  ION_PROFILE_FUNCTION("ion::gfx::GraphicsManager::" #name)
#else
#  define ION_PROFILE_GL_FUNC(name)
#endif

#define ION_WRAP_NON_PROD_GL_FUNC(name, return_type, typed_args, args, trace) \
 public:                                                                      \
  /* Invokes the wrapped function. */                                         \
  return_type name typed_args {                                               \
    ION_PROFILE_GL_FUNC(name);                                                \
    DCHECK(name##_wrapper_.Get());                                            \
    /* Don't trace calls to glGetError(). */                                  \
    static const bool do_trace = strcmp(#name, "GetError") &&                 \
                                 strcmp(#name, "PushGroupMarker") &&          \
                                 strcmp(#name, "PopGroupMarker");             \
    if (tracing_ostream_ && do_trace) {                                       \
      *tracing_ostream_ << tracing_prefix_ << name##_wrapper_.GetFuncName()   \
                        << "(" << trace << ")\n";                             \
    }                                                                         \
    if (is_error_checking_enabled_) {                                         \
      /* See ErrorChecker class doc for why it is needed here. */             \
      std::ostringstream call;                                                \
      call << name##_wrapper_.GetFuncName() << "(" << trace << ")";           \
      ErrorChecker error_checker(this, call.str());                           \
      return (*name##_wrapper_.Get())args;                                    \
    } else {                                                                  \
      return (*name##_wrapper_.Get())args;                                    \
    }                                                                         \
  }

// In production builds, just invoke the function directly. Since tracing is
// disabled in production builds, however, any tests that rely on tracing must
// be disabled or they will fail.
#define ION_WRAP_PROD_GL_FUNC(name, return_type, typed_args, args, trace) \
 public:                                                                  \
  /* Invokes the wrapped function. */                                     \
  return_type name typed_args {                                           \
    ION_PROFILE_GL_FUNC(name);                                            \
    return (*name ## _wrapper_.Get())args;                                \
  }

#define ION_DECLARE_GL_WRAPPER(group, name, return_type, typed_args, args) \
 private:                                                                  \
  /* Typedef for a pointer to the function. */                             \
  typedef return_type(ION_APIENTRY* name ## _Type) typed_args;             \
                                                                           \
  /* Wrapper class that initializes the pointer with a lookup. */          \
  class name ## _Wrapper : public WrapperBase {                            \
   public:                                                                 \
    name ## _Wrapper() : WrapperBase(#name, k ## group) {}                 \
    name ## _Type Get() { return reinterpret_cast<name ## _Type>(ptr_); }  \
  };                                                                       \
                                                                           \
  /* Instance of the wrapper class. */                                     \
  name ## _Wrapper name ## _wrapper_;

#if ION_PRODUCTION
#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  ION_WRAP_PROD_GL_FUNC(name, return_type, typed_args, args, trace)         \
  ION_DECLARE_GL_WRAPPER(group, name, return_type, typed_args, args)
#else
#define ION_WRAP_GL_FUNC(group, name, return_type, typed_args, args, trace) \
  ION_WRAP_NON_PROD_GL_FUNC(name, return_type, typed_args, args, trace)     \
  ION_DECLARE_GL_WRAPPER(group, name, return_type, typed_args, args)
#endif

// Logs an argument to the TracingHelper.
#define ION_TRACE_ARG(name, type, arg) \
  #arg << " = " << tracing_helper_.ToString(#type, arg)

#endif  // ION_GFX_GRAPHICSMANAGERMACRODEFS_H_
