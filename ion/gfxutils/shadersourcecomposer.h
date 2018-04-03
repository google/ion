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

// ShaderSourceComposer provides basic functionality for constructing a shader
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
// Each Composer can also load shaders from a resource that includes other
// resources using the special directive '$input "name"'. The function passed to
// the constructor loads data given a resource name. Optionally injects #line
// directives in the shader source if it contains any $input directives. The
// filenames in $input directives should have UNIX style path separators
// ('/'). Filenames passed to the loader and saver also contain UNIX style path
// separators ('/'), and can be converted to the local platform style using
// port::GetCanonicalFilePath().
//
//-----------------------------------------------------------------------------
//
// Base class for all composers.
//
//-----------------------------------------------------------------------------
class ION_API ShaderSourceComposer : public base::Referent {
 public:
  // A function that returns a string source given a filename.
  typedef std::function<const std::string(const std::string& name)>
      SourceLoader;
  // A function that saves a string source given a filename. Returns whether
  // the file was successfully saved.
  typedef std::function<bool(const std::string& name,  // NOLINT
                             const std::string& source)>
      SourceSaver;
  // A function that returns the last time the source in filename was modified.
  typedef std::function<bool(const std::string& filename,
                             std::chrono::system_clock::time_point* timestamp)>
      SourceModificationTime;
  ShaderSourceComposer();
  // This constructor takes a base identifier that represents the top-level
  // name, functions for loading, saving, and seeing if sources have changed,
  // and whether #line directives should be injected in the source when $input
  // directives are processed.  If this seems complex, consider using one of
  // the derived classes that have simpler constructors.
  ShaderSourceComposer(const std::string& filename,
                       const SourceLoader& source_loader,
                       const SourceSaver& source_saver,
                       const SourceModificationTime& source_time,
                       bool insert_line_directives);
  // Sets a string that will be prepended to all dependency names loaded by this
  // composer.  This is especially useful for file paths.
  void SetBasePath(const std::string& path);
  // Returns the source string of a shader.
  virtual const std::string GetSource();
  // Returns whether this composer depends on the named dependency, which might
  // be a filename or some other identifier that this recognizes.
  virtual bool DependsOn(const std::string& resource) const;
  // Returns the source of the passed dependency.
  virtual const std::string GetDependencySource(
      const std::string& dependency) const;
  // Requests that the composer set the source of the dependency. Returns
  // whether the composer actually changes the source.
  virtual bool SetDependencySource(const std::string& dependency,
                                   const std::string& source);
  // Returns the name of a dependency identified by the passed id. The id is
  // an integral value used by OpenGL to identify a shader file. Returns an
  // empty string if the id is unknown or if there are no dependencies.
  virtual const std::string GetDependencyName(unsigned int id) const;
  // Returns a vector containing all names that this composer depends on, or an
  // empty vector if there are no dependencies.
  virtual const std::vector<std::string> GetDependencyNames() const;
  // Determines if any dependencies have changed (e.g., if a file has changed on
  // disk since the last call to Get(Source|DependencySource)()) and updates
  // them. Returns a vector containing the names of the dependencies that have
  // changed.
  virtual const std::vector<std::string> GetChangedDependencies();

 protected:
  // The destructor is protected since this is derived from base::Referent.
  ~ShaderSourceComposer() override;

 private:
  class IncludeDirectiveHelper;
  std::unique_ptr<IncludeDirectiveHelper> helper_;
};
using ShaderSourceComposerPtr = base::SharedPtr<ShaderSourceComposer>;

//-----------------------------------------------------------------------------
//
// Simple composer that returns the source string passed to its constructor,
// expanding all $input directives if they are present.  The label is used to
// resolve $input directives.
//
//-----------------------------------------------------------------------------
class ION_API StringComposer : public ShaderSourceComposer {
 public:
  StringComposer(const std::string& label, const std::string& source);

 protected:
  // The destructor is protected since this is derived from base::Referent.
  ~StringComposer() override;
};
using StringComposerPtr = base::SharedPtr<StringComposer>;

//-----------------------------------------------------------------------------
//
// Applies a fixed transformation to the output of another composer.
// The transformation is specified as a C++11 functor.
//
//-----------------------------------------------------------------------------
class ION_API FilterComposer : public ShaderSourceComposer {
 public:
  typedef std::function<std::string(const std::string&)> StringFilter;
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
using FilterComposerPtr = base::SharedPtr<FilterComposer>;

//-----------------------------------------------------------------------------
//
// Loads a shader source from zip asset resources that may $input other zip
// assets.
//
//-----------------------------------------------------------------------------
class ION_API ZipAssetComposer : public ShaderSourceComposer {
 public:
  ZipAssetComposer(const std::string& filename, bool insert_line_directives);

 protected:
  // The destructor is protected since this is derived from base::Referent.
  ~ZipAssetComposer() override;
};
using ZipAssetComposerPtr = base::SharedPtr<ZipAssetComposer>;

}  // namespace gfxutils
}  // namespace ion

#endif  // ION_GFXUTILS_SHADERSOURCECOMPOSER_H_
