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

#ifndef ION_GFX_SHADER_H_
#define ION_GFX_SHADER_H_

#include <string>

#include "ion/base/referent.h"
#include "ion/gfx/resourceholder.h"
#include "ion/gfx/shaderinputregistry.h"

namespace ion {
namespace gfx {

// Base class for Shader and ProgramBase objects.
class ION_API ShaderBase : public ResourceHolder {
 public:
  // Sets/returns a string documenting the shader program for help.
  void SetDocString(const std::string& s) { doc_string_ = s; }
  const std::string& GetDocString() const { return doc_string_; }

  // Sets/returns the latest info log.
  void SetInfoLog(const std::string& info_log) const {
    info_log_ = info_log;
  }
  const std::string GetInfoLog() const { return info_log_; }

 protected:
  // The constructor is protected since this is a base class.
  ShaderBase();

  // The destructor is protected because all base::Referent classes must have
  // protected or private destructors.
  ~ShaderBase() override;

 private:
  std::string doc_string_;

  // The info log is mutable so that it be set on const references in Renderer.
  mutable std::string info_log_;
};

// A Shader represents an OpenGL shader stage. It contains the source code of
// its shader as a string.
class ION_API Shader : public ShaderBase {
 public:
  // Changes that affect the resource.
  enum Changes {
    kSourceChanged = kNumBaseChanges,
    kNumChanges
  };

  Shader();
  explicit Shader(const std::string& source);

  // Sets/returns the source of the shader.
  void SetSource(const std::string& source) {
    source_.Set(source);
  }
  const std::string& GetSource() const {
    return source_.Get();
  }

 private:
  // The destructor is private because all base::Referent classes must have
  // protected or private destructors.
  ~Shader() override;

  Field<std::string> source_;
};

// Convenience typedef for shared pointers to Shaders.
using ShaderPtr = base::SharedPtr<Shader>;

}  // namespace gfx
}  // namespace ion

#endif  // ION_GFX_SHADER_H_
