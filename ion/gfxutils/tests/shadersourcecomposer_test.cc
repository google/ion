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

#include "ion/gfxutils/shadersourcecomposer.h"

#include <chrono>  // NOLINT

#include "ion/base/logchecker.h"
#include "ion/base/stringutils.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/port/timer.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

ION_REGISTER_ASSETS(ZipAssetComposerTest);

namespace ion {
namespace gfxutils {

using std::bind;
using std::placeholders::_1;
using std::placeholders::_2;

namespace {

class SourceHolder {
 public:
  SourceHolder() {
    strings_["source1"] = StringInfo("Source\nstring 1\n$input \"source2\"");
    strings_["source2"] = StringInfo("Source string 2\n");
    strings_["source3"] = StringInfo("Source string 3\n$input \"source4\"\n");
    strings_["source4"] = StringInfo("Source string 4\n$input \"source3\"\n");
    strings_["source5"] =
        StringInfo("Source string 5\n$input \"doesnotexist\"\n");
    strings_["source6"] = StringInfo("Source string 6\n$input badinput\"\n");
    strings_["source7"] =
        StringInfo("Source string 7\n#if 0\n$input \"source1\"\n#endif\n");
    strings_["path/depth/source8"] =
        StringInfo("Source string 8\n$input \"to/source9\"\n");
    strings_["path/depth/to/source9"] = StringInfo("Source string 9\n");
  }

  ~SourceHolder() {}

  const std::string GetSource(const std::string& name) const {
    StringInfoMap::const_iterator it = strings_.find(name);
    return it == strings_.end() ? std::string() : it->second.source;
  }
  // A function that saves a string source given a filename. Returns whether
  // the file was successfully saved.
  bool SetSource(const std::string& name, const std::string& source) {
    StringInfoMap::iterator it = strings_.find(name);
    if (it == strings_.end()) {
      return false;
    } else {
      it->second.source = source;
      it->second.timestamp = std::chrono::system_clock::now();
      return true;
    }
  }

  const bool GetTime(const std::string& name,
                     std::chrono::system_clock::time_point* timestamp) const {
    StringInfoMap::const_iterator it = strings_.find(name);
    if (it == strings_.end()) {
      return false;
    } else {
      *timestamp = it->second.timestamp;
      return true;
    }
  }

 private:
  struct StringInfo {
    StringInfo() {}
    explicit StringInfo(const std::string& source) : source(source) {}
    std::string source;
    std::chrono::system_clock::time_point timestamp;
  };
  typedef std::map<std::string, StringInfo> StringInfoMap;

  std::map<std::string, StringInfo> strings_;
};

const char kShaderBeforeRewrite[] =
    "void main() { gl_MagicVariable = 1; }";
const char kShaderAfterRewrite[] =
    "#version 300 es\n"
    "void main() { gl_MagicVariableEXT = 1; }";
std::string RewriteShader(const std::string& source) {
  return "#version 300 es\n" +
      base::ReplaceString(source, "gl_MagicVariable", "gl_MagicVariableEXT");
}

}  // anonymous namespace

class ShaderSourceComposerTest : public ::testing::Test {
 public:
  const std::string StringSourceLoader(const std::string& name) const {
    return holder_.GetSource(name);
  }

  bool StringSourceSaver(const std::string& name, const std::string& source) {
    return holder_.SetSource(name, source);
  }

  const bool StringSourceTime(
      const std::string& name,
      std::chrono::system_clock::time_point* timestamp) {
    return holder_.GetTime(name, timestamp);
  }

 protected:
  SourceHolder holder_;
};

TEST_F(ShaderSourceComposerTest, StringComposer) {
  static const char kSource[] = "Some source code.";
  static const char kSource2[] = "Some other source code.";
  ShaderSourceComposerPtr composer(new StringComposer("dependency", kSource));
  EXPECT_EQ(kSource, composer->GetSource());
  EXPECT_TRUE(composer->DependsOn("dependency"));
  EXPECT_EQ(kSource, composer->GetDependencySource("dependency"));
  EXPECT_FALSE(composer->SetDependencySource("", kSource2));
  EXPECT_FALSE(composer->SetDependencySource("not a dependency", kSource2));
  EXPECT_TRUE(composer->SetDependencySource("dependency", kSource2));
  EXPECT_EQ(kSource2, composer->GetDependencySource("dependency"));
  EXPECT_EQ("", composer->GetDependencySource("anything"));
  EXPECT_EQ("", composer->GetDependencySource(""));
  EXPECT_FALSE(composer->DependsOn(""));
  EXPECT_FALSE(composer->DependsOn("anything"));
  EXPECT_EQ(1U, composer->GetDependencyNames().size());
  // IncludeDirectiveHelper sets the first id to 1.
  EXPECT_EQ("dependency", composer->GetDependencyName(1));
  EXPECT_EQ(std::vector<std::string>(), composer->GetChangedDependencies());
}

TEST_F(ShaderSourceComposerTest, StringComposerDirectives) {
  // Test simple non-recursive usage of $input with StringComposer.
  static const char kBody[] = R"(
    $input "header"
    void main{})";
  static const char kHeader[] = "attribute vec3 Position;";
  static const char kExpanded[] = R"(
#line 1 2
attribute vec3 Position;
#line 2 1
    void main{})";
  ShaderSourceComposerPtr composer1(new StringComposer("main", kBody));
  ShaderSourceComposerPtr composer2(new StringComposer("header", kHeader));
  EXPECT_EQ(kExpanded, composer1->GetSource());

  // Including an unknown label results in #error.
  static const char kBodyWithUnknownInclude[] = R"(
    $input "unknown"
    void main{})";
  static const char kExpandedWithUnknownInclude[] = R"(
#error Invalid shader source identifier: unknown
#line 2 1
    void main{})";
  ShaderSourceComposerPtr composer3(
      new StringComposer("main2", kBodyWithUnknownInclude));
  EXPECT_EQ(kExpandedWithUnknownInclude, composer3->GetSource());

  // Modify the "header" composer and check that the "main" composer knows
  // that it has been dirtied.  Dirtiness is detected using a timestamp
  // rather than a file hash, so the test needs to sleep briefly before the
  // mutating the header source.
  EXPECT_TRUE(composer1->DependsOn("header"));
  EXPECT_EQ(0U, composer1->GetChangedDependencies().size());
  port::Timer::SleepNMilliseconds(1);
  EXPECT_TRUE(composer1->SetDependencySource("header", "foo"));
  EXPECT_EQ(1U, composer1->GetChangedDependencies().size());
}

TEST_F(ShaderSourceComposerTest, FilterComposer) {
  static const char kSource[] = "unicorn vec3 rainbow;";
  ShaderSourceComposerPtr base(new StringComposer("dependency",
      kShaderBeforeRewrite));
  ShaderSourceComposerPtr filter(new FilterComposer(base,
      RewriteShader));
  EXPECT_EQ(kShaderAfterRewrite, filter->GetSource());
  EXPECT_EQ(base->DependsOn("wombat"), filter->DependsOn("wombat"));
  EXPECT_EQ(base->DependsOn("dependency"), filter->DependsOn("dependency"));
  EXPECT_EQ(kShaderBeforeRewrite,
            filter->GetDependencySource("dependency"));
  EXPECT_EQ(base->GetDependencySource("dependency"),
            filter->GetDependencySource("dependency"));
  EXPECT_FALSE(filter->SetDependencySource("", kSource));
  EXPECT_FALSE(filter->SetDependencySource("platypus", kSource));
  EXPECT_TRUE(filter->SetDependencySource("dependency", kSource));
  EXPECT_EQ(kSource, filter->GetDependencySource("dependency"));
  EXPECT_EQ(1U, filter->GetDependencyNames().size());
  // IncludeDirectiveHelper sets the first id to 1.
  EXPECT_EQ("dependency", filter->GetDependencyName(1));
  EXPECT_EQ(std::vector<std::string>(), filter->GetChangedDependencies());
}

TEST_F(ShaderSourceComposerTest, ShaderSourceComposerSimpleInput) {
  // Test a simple $input.
  ShaderSourceComposerPtr composer(new ShaderSourceComposer(
      "source1", bind(&ShaderSourceComposerTest::StringSourceLoader, this, _1),
      bind(&ShaderSourceComposerTest::StringSourceSaver, this, _1, _2),
      bind(&ShaderSourceComposerTest::StringSourceTime, this, _1, _2), false));
  EXPECT_EQ("Source\nstring 1\nSource string 2", composer->GetSource());
  EXPECT_FALSE(composer->DependsOn(""));
  EXPECT_FALSE(composer->DependsOn("anything"));
  EXPECT_TRUE(composer->DependsOn("source1"));
  EXPECT_TRUE(composer->DependsOn("source2"));
  EXPECT_FALSE(composer->DependsOn("source3"));
  EXPECT_FALSE(composer->DependsOn("source4"));
  EXPECT_FALSE(composer->DependsOn("source5"));
  EXPECT_FALSE(composer->DependsOn("source6"));
  EXPECT_FALSE(composer->DependsOn("source7"));
  EXPECT_EQ("source1", composer->GetDependencyName(1));
  EXPECT_EQ("source2", composer->GetDependencyName(2));
  EXPECT_EQ("", composer->GetDependencyName(3));

  std::vector<std::string> names;
  names.push_back("source1");
  names.push_back("source2");
  EXPECT_EQ(names, composer->GetDependencyNames());

  EXPECT_EQ(StringSourceLoader("source1"),
            composer->GetDependencySource("source1"));
  EXPECT_EQ(StringSourceLoader("source2"),
              composer->GetDependencySource("source2"));
  EXPECT_TRUE(composer->GetDependencySource("source3").empty());
  EXPECT_FALSE(composer->SetDependencySource("", ""));
  EXPECT_FALSE(composer->SetDependencySource("not a dependency", ""));
  EXPECT_TRUE(composer->SetDependencySource("source1", "new source"));
  EXPECT_EQ("new source", composer->GetDependencySource("source1"));
  EXPECT_EQ(StringSourceLoader("source1"),
            composer->GetDependencySource("source1"));
  EXPECT_EQ(StringSourceLoader("source2"),
              composer->GetDependencySource("source2"));
}

TEST_F(ShaderSourceComposerTest, ShaderSourceComposerRecursiveInput) {
  // Test a recursive $input.
  base::LogChecker log_checker;
  ShaderSourceComposerPtr composer(new ShaderSourceComposer(
      "source3", bind(&ShaderSourceComposerTest::StringSourceLoader, this, _1),
      bind(&ShaderSourceComposerTest::StringSourceSaver, this, _1, _2),
      bind(&ShaderSourceComposerTest::StringSourceTime, this, _1, _2), false));
  EXPECT_EQ("Source string 3\nSource string 4", composer->GetSource());
  EXPECT_TRUE(log_checker.HasMessage("WARNING",
                                     "source4:2: Recursive $input ignored "
                                     "while trying to $input \"source3\""));
  EXPECT_FALSE(composer->DependsOn(""));
  EXPECT_FALSE(composer->DependsOn("anything"));
  EXPECT_FALSE(composer->DependsOn("source1"));
  EXPECT_FALSE(composer->DependsOn("source2"));
  EXPECT_TRUE(composer->DependsOn("source3"));
  EXPECT_TRUE(composer->DependsOn("source4"));
  EXPECT_FALSE(composer->DependsOn("source5"));
  EXPECT_FALSE(composer->DependsOn("source6"));
  EXPECT_FALSE(composer->DependsOn("source7"));
  EXPECT_EQ("source3", composer->GetDependencyName(1));
  EXPECT_EQ("source4", composer->GetDependencyName(2));
  EXPECT_EQ("", composer->GetDependencyName(3));

  std::vector<std::string> names;
  names.push_back("source3");
  names.push_back("source4");
  EXPECT_EQ(names, composer->GetDependencyNames());

  EXPECT_EQ(StringSourceLoader("source3"),
            composer->GetDependencySource("source3"));
  EXPECT_EQ(StringSourceLoader("source4"),
              composer->GetDependencySource("source4"));
  EXPECT_TRUE(composer->GetDependencySource("source1").empty());
  EXPECT_TRUE(composer->GetDependencySource("source2").empty());
  EXPECT_FALSE(composer->SetDependencySource("", ""));
  EXPECT_FALSE(composer->SetDependencySource("not a dependency", ""));
  EXPECT_TRUE(composer->SetDependencySource("source3", "new source"));
  EXPECT_EQ("new source", composer->GetDependencySource("source3"));
  EXPECT_EQ(StringSourceLoader("source3"),
            composer->GetDependencySource("source3"));
  EXPECT_EQ(StringSourceLoader("source4"),
              composer->GetDependencySource("source4"));
}

TEST_F(ShaderSourceComposerTest, ShaderSourceComposerNonexistantInput) {
  // Test $input of a resource that does not exist.
  ShaderSourceComposerPtr composer(new ShaderSourceComposer(
      "source5", bind(&ShaderSourceComposerTest::StringSourceLoader, this, _1),
      bind(&ShaderSourceComposerTest::StringSourceSaver, this, _1, _2),
      bind(&ShaderSourceComposerTest::StringSourceTime, this, _1, _2), false));
  EXPECT_EQ("Source string 5", composer->GetSource());
  EXPECT_FALSE(composer->DependsOn(""));
  EXPECT_FALSE(composer->DependsOn("anything"));
  EXPECT_FALSE(composer->DependsOn("source1"));
  EXPECT_FALSE(composer->DependsOn("source2"));
  EXPECT_FALSE(composer->DependsOn("source3"));
  EXPECT_FALSE(composer->DependsOn("source4"));
  EXPECT_TRUE(composer->DependsOn("source5"));
  EXPECT_FALSE(composer->DependsOn("source6"));
  EXPECT_FALSE(composer->DependsOn("source7"));
  EXPECT_EQ("source5", composer->GetDependencyName(1));
  EXPECT_EQ("", composer->GetDependencyName(2));

  std::vector<std::string> names;
  names.push_back("source5");
  EXPECT_EQ(names, composer->GetDependencyNames());

  EXPECT_EQ(StringSourceLoader("source5"),
              composer->GetDependencySource("source5"));
  EXPECT_TRUE(composer->GetDependencySource("source1").empty());
}

TEST_F(ShaderSourceComposerTest, ShaderSourceComposerBadInput) {
  // Test a malformed $input.
  base::LogChecker log_checker;
  ShaderSourceComposerPtr composer(new ShaderSourceComposer(
      "source6", bind(&ShaderSourceComposerTest::StringSourceLoader, this, _1),
      bind(&ShaderSourceComposerTest::StringSourceSaver, this, _1, _2),
      bind(&ShaderSourceComposerTest::StringSourceTime, this, _1, _2), false));
  EXPECT_EQ("Source string 6", composer->GetSource());
  EXPECT_TRUE(log_checker.HasMessage(
      "WARNING",
      "source6:2: Invalid $input directive, perhaps missing a '\"'"));
  EXPECT_FALSE(composer->DependsOn(""));
  EXPECT_FALSE(composer->DependsOn("anything"));
  EXPECT_FALSE(composer->DependsOn("source1"));
  EXPECT_FALSE(composer->DependsOn("source2"));
  EXPECT_FALSE(composer->DependsOn("source3"));
  EXPECT_FALSE(composer->DependsOn("source4"));
  EXPECT_FALSE(composer->DependsOn("source5"));
  EXPECT_TRUE(composer->DependsOn("source6"));
  EXPECT_FALSE(composer->DependsOn("source7"));
  EXPECT_EQ("source6", composer->GetDependencyName(1));
  EXPECT_EQ("", composer->GetDependencyName(2));

  std::vector<std::string> names;
  names.push_back("source6");
  EXPECT_EQ(names, composer->GetDependencyNames());

  EXPECT_EQ(StringSourceLoader("source6"),
              composer->GetDependencySource("source6"));
  EXPECT_TRUE(composer->GetDependencySource("source1").empty());
}

TEST_F(ShaderSourceComposerTest, ShaderSourceComposerLineDirectives) {
  // Test #line directives.
  ShaderSourceComposerPtr composer(new ShaderSourceComposer(
      "source1", bind(&ShaderSourceComposerTest::StringSourceLoader, this, _1),
      bind(&ShaderSourceComposerTest::StringSourceSaver, this, _1, _2),
      bind(&ShaderSourceComposerTest::StringSourceTime, this, _1, _2), true));
  EXPECT_EQ("Source\nstring 1\n#line 1 2\nSource string 2\n#line 3 1",
            composer->GetSource());
  EXPECT_FALSE(composer->DependsOn(""));
  EXPECT_FALSE(composer->DependsOn("anything"));
  EXPECT_TRUE(composer->DependsOn("source1"));
  EXPECT_TRUE(composer->DependsOn("source2"));
  EXPECT_FALSE(composer->DependsOn("source3"));
  EXPECT_FALSE(composer->DependsOn("source4"));
  EXPECT_FALSE(composer->DependsOn("source5"));
  EXPECT_FALSE(composer->DependsOn("source6"));
  EXPECT_FALSE(composer->DependsOn("source7"));
  EXPECT_EQ("source1", composer->GetDependencyName(1));
  EXPECT_EQ("source2", composer->GetDependencyName(2));
  EXPECT_EQ("", composer->GetDependencyName(3));

  std::vector<std::string> names;
  names.push_back("source1");
  names.push_back("source2");
  EXPECT_EQ(names, composer->GetDependencyNames());

  EXPECT_EQ(StringSourceLoader("source1"),
            composer->GetDependencySource("source1"));
  EXPECT_EQ(StringSourceLoader("source2"),
              composer->GetDependencySource("source2"));
  EXPECT_TRUE(composer->GetDependencySource("source3").empty());
}

TEST_F(ShaderSourceComposerTest, ShaderSourceComposerWithPaths) {
  // Test #line directives.
  ShaderSourceComposer* ic = new ShaderSourceComposer(
      "depth/source8",
      bind(&ShaderSourceComposerTest::StringSourceLoader, this, _1),
      bind(&ShaderSourceComposerTest::StringSourceSaver, this, _1, _2),
      bind(&ShaderSourceComposerTest::StringSourceTime, this, _1, _2), true);
  ic->SetBasePath("path");
  ShaderSourceComposerPtr composer(ic);
  EXPECT_EQ("Source string 8\n#line 1 2\nSource string 9\n#line 2 1",
            composer->GetSource());
  EXPECT_FALSE(composer->DependsOn(""));
  EXPECT_FALSE(composer->DependsOn("anything"));
  EXPECT_FALSE(composer->DependsOn("source1"));
  EXPECT_FALSE(composer->DependsOn("source2"));
  EXPECT_FALSE(composer->DependsOn("source3"));
  EXPECT_FALSE(composer->DependsOn("source4"));
  EXPECT_FALSE(composer->DependsOn("source5"));
  EXPECT_FALSE(composer->DependsOn("source6"));
  EXPECT_FALSE(composer->DependsOn("source7"));
  EXPECT_TRUE(composer->DependsOn("path/depth/source8"));
  EXPECT_TRUE(composer->DependsOn("path/depth/to/source9"));
  EXPECT_EQ("path/depth/source8", composer->GetDependencyName(1));
  EXPECT_EQ("path/depth/to/source9", composer->GetDependencyName(2));

  std::vector<std::string> names;
  names.push_back("path/depth/source8");
  names.push_back("path/depth/to/source9");
  EXPECT_EQ(names, composer->GetDependencyNames());

  EXPECT_EQ(StringSourceLoader("path/depth/source8"),
            composer->GetDependencySource("path/depth/source8"));
  EXPECT_EQ(StringSourceLoader("path/depth/to/source9"),
              composer->GetDependencySource("path/depth/to/source9"));
  EXPECT_TRUE(composer->GetDependencySource("source3").empty());
}

TEST_F(ShaderSourceComposerTest, ShaderSourceComposerLineDirectivesAndIfdefs) {
  // Test #line directives.
  ShaderSourceComposerPtr composer(new ShaderSourceComposer(
      "source7", bind(&ShaderSourceComposerTest::StringSourceLoader, this, _1),
      bind(&ShaderSourceComposerTest::StringSourceSaver, this, _1, _2),
      bind(&ShaderSourceComposerTest::StringSourceTime, this, _1, _2), true));
  // Each preprocessor directive should trigger a new #line directive.
  EXPECT_EQ(
      "Source string 7\n#if 0\n#line 2 1\n#line 1 2\nSource\nstring 1\n#line 1 "
      "3\nSource string 2\n#line 3 2\n#line 3 1\n#endif\n#line 4 1",
      composer->GetSource());
  EXPECT_FALSE(composer->DependsOn(""));
  EXPECT_FALSE(composer->DependsOn("anything"));
  EXPECT_TRUE(composer->DependsOn("source1"));
  EXPECT_TRUE(composer->DependsOn("source2"));
  EXPECT_FALSE(composer->DependsOn("source3"));
  EXPECT_FALSE(composer->DependsOn("source4"));
  EXPECT_FALSE(composer->DependsOn("source5"));
  EXPECT_FALSE(composer->DependsOn("source6"));
  EXPECT_TRUE(composer->DependsOn("source7"));
  EXPECT_EQ("source7", composer->GetDependencyName(1));
  EXPECT_EQ("source1", composer->GetDependencyName(2));
  EXPECT_EQ("source2", composer->GetDependencyName(3));
  EXPECT_EQ("", composer->GetDependencyName(4));

  std::vector<std::string> names;
  names.push_back("source1");
  names.push_back("source2");
  names.push_back("source7");
  EXPECT_EQ(names, composer->GetDependencyNames());

  EXPECT_EQ(StringSourceLoader("source1"),
            composer->GetDependencySource("source1"));
  EXPECT_EQ(StringSourceLoader("source2"),
              composer->GetDependencySource("source2"));
  EXPECT_EQ(StringSourceLoader("source7"),
              composer->GetDependencySource("source7"));
  EXPECT_TRUE(composer->GetDependencySource("source3").empty());
}

TEST_F(ShaderSourceComposerTest, ShaderSourceComposerGetChangedDependencies) {
  // Test #line directives.
  ShaderSourceComposerPtr composer(new ShaderSourceComposer(
      "source1", bind(&ShaderSourceComposerTest::StringSourceLoader, this, _1),
      bind(&ShaderSourceComposerTest::StringSourceSaver, this, _1, _2),
      bind(&ShaderSourceComposerTest::StringSourceTime, this, _1, _2), false));
  // Each preprocessor directive should trigger a new #line directive.
  EXPECT_EQ("Source\nstring 1\nSource string 2", composer->GetSource());
  EXPECT_EQ("source1", composer->GetDependencyName(1));
  EXPECT_EQ("source2", composer->GetDependencyName(2));

  // Check that there are no changed dependencies yet.
  EXPECT_TRUE(composer->GetChangedDependencies().empty());

  // Make a source change directly, without the composer knowing about it.
  const std::string new_source1("New source 1");
  holder_.SetSource("source1", new_source1);
  std::vector<std::string> changed = composer->GetChangedDependencies();
  EXPECT_EQ(1U, changed.size());
  EXPECT_EQ("source1", changed[0]);

  EXPECT_EQ(new_source1, composer->GetSource());
  EXPECT_EQ(new_source1, composer->GetDependencySource("source1"));
  EXPECT_EQ("source1", composer->GetDependencyName(1));
  // The composer no longer depends on source2 since the include is gone.
  EXPECT_EQ("", composer->GetDependencyName(2));

  // Change multiple sources.
  const std::string new_source2("Source 1\n$input \"path/depth/source8\"\n");
  holder_.SetSource("source1", new_source2);
  // This will update the list of dependencies.
  composer->GetSource();
  // Sleep so that there will be a timestamp difference when source1 is set
  // below.
  port::Timer::SleepNSeconds(1);
  holder_.SetSource("source1", new_source2);
  holder_.SetSource("path/depth/source8", "Source 8\n$input \"source2\"\n");

  // Both should have changed since the last calls to get their sources.
  changed = composer->GetChangedDependencies();
  EXPECT_EQ(2U, changed.size());
  EXPECT_EQ("path/depth/source8", changed[0]);
  EXPECT_EQ("source1", changed[1]);

  EXPECT_EQ("Source 1\nSource 8\nSource string 2", composer->GetSource());
  EXPECT_EQ("source1", composer->GetDependencyName(1));
  EXPECT_EQ("path/depth/source8", composer->GetDependencyName(2));
  EXPECT_EQ("source2", composer->GetDependencyName(3));
}

TEST_F(ShaderSourceComposerTest, ZipAssetComposer) {
  ZipAssetComposerTest::RegisterAssets();
  std::vector<std::string> names;
  names.push_back("included_shader_source.glsl");
  names.push_back("shader_source.glsl");
  {
    ShaderSourceComposerPtr composer(
        new ZipAssetComposer("shader_source.glsl", false));
    EXPECT_EQ_ML("Shader source in a zip asset.\nIncluded source.\nLast line.",
                 composer->GetSource());
    EXPECT_FALSE(composer->DependsOn(""));
    EXPECT_FALSE(composer->DependsOn("anything"));
    EXPECT_TRUE(composer->DependsOn("shader_source.glsl"));
    EXPECT_TRUE(composer->DependsOn("included_shader_source.glsl"));
    EXPECT_EQ("shader_source.glsl", composer->GetDependencyName(1));
    EXPECT_EQ("included_shader_source.glsl", composer->GetDependencyName(2));
    EXPECT_EQ("", composer->GetDependencyName(3));
    EXPECT_EQ(names, composer->GetDependencyNames());
    EXPECT_EQ(base::ZipAssetManager::GetFileData("shader_source.glsl"),
              composer->GetDependencySource("shader_source.glsl"));
    EXPECT_EQ(base::ZipAssetManager::GetFileData("included_shader_source.glsl"),
              composer->GetDependencySource("included_shader_source.glsl"));
    EXPECT_TRUE(composer->GetDependencySource("not a dependency").empty());
    // Set a dependency. This will return false if the file is read-only.
    composer->SetDependencySource("included_shader_source.glsl",
                                  "New included source.\n");
    EXPECT_EQ_ML(
        "Shader source in a zip asset.\nNew included source.\n"
        "Last line.",
        composer->GetSource());

    // Set the dependency back.
    composer->SetDependencySource("included_shader_source.glsl",
                                  "Included source.\n");
    EXPECT_EQ_ML("Shader source in a zip asset.\nIncluded source.\nLast line.",
                 composer->GetSource());
  }
  {
    ShaderSourceComposerPtr composer(
        new ZipAssetComposer("shader_source.glsl", true));
    EXPECT_EQ_ML(
        "Shader source in a zip asset.\n#line 1 "
        "2\nIncluded source.\n#line 2 1\nLast line.",
        composer->GetSource());
    EXPECT_FALSE(composer->DependsOn(""));
    EXPECT_FALSE(composer->DependsOn("anything"));
    EXPECT_TRUE(composer->DependsOn("shader_source.glsl"));
    EXPECT_TRUE(composer->DependsOn("included_shader_source.glsl"));
    EXPECT_EQ("shader_source.glsl", composer->GetDependencyName(1));
    EXPECT_EQ("included_shader_source.glsl", composer->GetDependencyName(2));
    EXPECT_EQ("", composer->GetDependencyName(3));
    EXPECT_EQ(names, composer->GetDependencyNames());
    EXPECT_EQ(base::ZipAssetManager::GetFileData("shader_source.glsl"),
              composer->GetDependencySource("shader_source.glsl"));
    EXPECT_EQ(base::ZipAssetManager::GetFileData("included_shader_source.glsl"),
              composer->GetDependencySource("included_shader_source.glsl"));
    EXPECT_TRUE(composer->GetDependencySource("not a dependency").empty());
  }
}

}  // namespace gfxutils
}  // namespace ion
