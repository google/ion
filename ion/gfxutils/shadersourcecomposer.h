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

#ifndef ION_GFXUTILS_SHADERSOURCECOMPOSER_H_
#define ION_GFXUTILS_SHADERSOURCECOMPOSER_H_

#include <chrono>  // NOLINT
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ion/base/referent.h"

namespace ion {
namespace gfxutils {

// A ShaderSourceComposer is a generic interface for constructing a shader
// source string. Subclasses implement the details of how the source is created.
// For example, a subclass may read shader code from a file, or construct a
// string programatically based on arguments passed to its constructor. A
// composer may inject strings into the source code of shaders, such as global
// #defines or forced includes.
//
// Composers may be chained together. For example, ComposerA (which loads files)
// could hold an instance of ComposerB (which injects #defines). A trivial
// implementation of ComposerA's GetSource() might be:
//
// const std::string ComposerA::GetSource() {
//   const std::string defines = composer_b_->GetSource();
//   return defines + this->LoadFile();
// }
//
// Each Composer must support queries on the set of named dependencies (e.g.,
// filenames) that the shader source depends on. OpenGL uses integers to
// identify files or other resources in shaders; Composers must support
// returning a meaningful name given an identifier.
//
//-----------------------------------------------------------------------------
//
// Base class for all composers.
//
//-----------------------------------------------------------------------------
class ION_API ShaderSourceComposer : public base::Referent {
 public:
  // Returns the source string of a shader.
  virtual const std::string GetSource() = 0;
  // Returns whether this composer depends on the named dependency, which might
  // be a filename or some other identifier that this recognizes.
  virtual bool DependsOn(const std::string& dependency) const = 0;
  // Returns the source of the passed dependency.
  virtual const std::string GetDependencySource(
      const std::string& dependency) const = 0;
  // Requests that the composer set the source of the dependency. Returns
  // whether the composer actually changes the source.
  virtual bool SetDependencySource(const std::string& dependency,
                                   const std::string& source) = 0;
  // Returns the name of a dependency identified by the passed id. The id is
  // an integral value used by OpenGL to identify a shader file. Returns an
  // empty string if the id is unknown or if there are no dependencies.
  virtual const std::string GetDependencyName(unsigned int id) const = 0;
  // Returns a vector containing all names that this composer depends on, or an
  // empty vector if there are no dependencies.
  virtual const std::vector<std::string> GetDependencyNames() const = 0;
  // Determines if any dependencies have changed (e.g., if a file has changed on
  // disk since the last call to Get(Source|DependencySource)()) and updates
  // them. Returns a vector containing the names of the dependencies that have
  // changed.
  virtual const std::vector<std::string> GetChangedDependencies() = 0;

 protected:
  // The constructor is protected since this is an abstract base class.
  ShaderSourceComposer();
  // The destructor is protected since this is derived from base::Referent.
  ~ShaderSourceComposer() override;
};
typedef base::ReferentPtr<ShaderSourceComposer>::Type ShaderSourceComposerPtr;

//-----------------------------------------------------------------------------
//
// Simple composer that just returns the string passed to its constructor.
//
//-----------------------------------------------------------------------------
class ION_API StringComposer : public ShaderSourceComposer {
 public:
  explicit StringComposer(const std::string& dependency_name,
                          const std::string& source)
      : dependency_(dependency_name),
        source_(source) {}

  const std::string GetSource() override { return source_; }
  bool DependsOn(const std::string& dependency) const override {
    return dependency == dependency_;
  }
  const std::string GetDependencySource(
      const std::string& dependency) const override {
    return dependency == dependency_ ? source_ : std::string();
  }
  bool SetDependencySource(const std::string& dependency,
                           const std::string& source) override {
    if (dependency == dependency_) {
      source_ = source;
      return true;
    } else {
      return false;
    }
  }
  const std::string GetDependencyName(unsigned int id) const override {
    return dependency_;
  }
  const std::vector<std::string> GetDependencyNames() const override {
    std::vector<std::string> names;
    names.push_back(dependency_);
    return names;
  }
  const std::vector<std::string> GetChangedDependencies() override {
    return std::vector<std::string>();
  }

 protected:
  // The destructor is protected since this is derived from base::Referent.
  ~StringComposer() override {}

 private:
  std::string dependency_;
  std::string source_;
};
typedef base::ReferentPtr<StringComposer>::Type StringComposerPtr;

//-----------------------------------------------------------------------------
//
// Applies a fixed transformation to the output of another composer.
// The transformation is specified as a C++11 functor.
//
//-----------------------------------------------------------------------------
class ION_API FilterComposer : public ShaderSourceComposer {
 public:
  typedef std::function<std::string(const std::string)> StringFilter;
  FilterComposer(const ShaderSourceComposerPtr& base,
                    const StringFilter& transformer)
      : base_(base),
        transformer_(transformer) {}
  const std::string GetSource() override {
    return transformer_(base_->GetSource());
  }
  bool DependsOn(const std::string& dependency) const override {
    return base_->DependsOn(dependency);
  }
  const std::string GetDependencySource(
      const std::string& dependency) const override {
    return base_->GetDependencySource(dependency);
  }
  bool SetDependencySource(const std::string& dependency,
                           const std::string& source) override {
    return base_->SetDependencySource(dependency, source);
  }
  const std::string GetDependencyName(unsigned int id) const override {
    return base_->GetDependencyName(id);
  }
  const std::vector<std::string> GetDependencyNames() const override {
    return base_->GetDependencyNames();
  }
  const std::vector<std::string> GetChangedDependencies() override {
    return base_->GetChangedDependencies();
  }

 private:
  ShaderSourceComposerPtr base_;
  StringFilter transformer_;
};
typedef base::ReferentPtr<FilterComposer>::Type FilterComposerPtr;

//-----------------------------------------------------------------------------
//
// Loads a shader source from a resource that may include other resources using
// the special directive '$input "name"'. The function passed to the constructor
// loads data given a resource name. Optionally injects #line directives in the
// shader source if it contains any $input directives. The filenames in $input
// directives should have UNIX style path separators ('/'). Filenames passed to
// the loader and saver also contain UNIX style path separators ('/'), and can
// be converted to the local platform style using port::GetCanonicalFilePath().
//
//-----------------------------------------------------------------------------
class ION_API IncludeComposer : public ShaderSourceComposer {
 public:
  // A function that returns a string source given a filename.
  typedef std::function<const std::string(const std::string& name)>
      SourceLoader;
  // A function that saves a string source given a filename. Returns whether
  // the file was successfully saved.
  typedef std::function<bool(const std::string& name,  // NOLINT
                             const std::string& source)> SourceSaver;
  // A function that returns the last time the source in filename was modified.
  typedef std::function<bool(const std::string& filename,
                             std::chrono::system_clock::time_point* timestamp)>
      SourceModificationTime;

  // The constructor requires a base filename that represents the top-level
  // file, functions for loading, saving, and seeing if sources have changed,
  // and whether #line directives should be injected in the source when $input
  // directives are processed.
  IncludeComposer(const std::string& filename,
                  const SourceLoader& source_loader,
                  const SourceSaver& source_saver,
                  const SourceModificationTime& source_time,
                  bool insert_line_directives);
  // Sets a path that will be prepended to all files (including the top-level
  // filename) loaded by this composer.
  void SetBasePath(const std::string& path);

  const std::string GetSource() override;
  bool DependsOn(const std::string& dependency) const override;
  const std::string GetDependencySource(
      const std::string& dependency) const override;
  bool SetDependencySource(const std::string& dependency,
                           const std::string& source) override;
  const std::string GetDependencyName(unsigned int id) const override;
  const std::vector<std::string> GetDependencyNames() const override;
  const std::vector<std::string> GetChangedDependencies() override;

 protected:
  // The destructor is protected since this is derived from base::Referent.
  ~IncludeComposer() override;

 private:
  class IncludeComposerHelper;
  std::unique_ptr<IncludeComposerHelper> helper_;
};
typedef base::ReferentPtr<IncludeComposer>::Type IncludeComposerPtr;

//-----------------------------------------------------------------------------
//
// Loads a shader source from zip asset resources that may $input other zip
// assets.
//
//-----------------------------------------------------------------------------
class ION_API ZipAssetComposer : public IncludeComposer {
 public:
  ZipAssetComposer(const std::string& filename, bool insert_line_directives);

 protected:
  // The destructor is protected since this is derived from base::Referent.
  ~ZipAssetComposer() override;
};
typedef base::ReferentPtr<ZipAssetComposer>::Type ZipAssetComposerPtr;

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_SHADERSOURCECOMPOSER_H_
