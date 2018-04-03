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

#include <mutex>  // NOLINT(build/c++11)
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <vector>

#include "ion/base/allocatable.h"
#include "ion/base/invalid.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/stlalloc/allocmap.h"
#include "ion/base/stlalloc/allocset.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"

namespace ion {
namespace gfxutils {

namespace {

static const char* kUnknownShaderSentinel = "#error";

static bool SetAndSaveZipAssetData(const std::string& filename,
                                   const std::string& source) {
  return base::ZipAssetManager::SetFileData(filename, source) &&
      base::ZipAssetManager::SaveFileData(filename);
}

static const std::string GetZipAssetFileData(const std::string& filename) {
  const std::string& data = base::ZipAssetManager::GetFileData(filename);
  return base::IsInvalidReference(data) ? kUnknownShaderSentinel : data;
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// Helper class for loading file-like resources that may contain $input
// directives.
//
//-----------------------------------------------------------------------------
class ShaderSourceComposer::IncludeDirectiveHelper : public base::Allocatable {
 public:
  IncludeDirectiveHelper(const base::Allocatable& owner,
                         const std::string& filename,
                         const SourceLoader& source_loader,
                         const SourceSaver& source_saver,
                         const SourceModificationTime& source_time,
                         bool insert_line_directives)
      : file_to_id_(owner),
        id_to_file_(owner),
        filename_(filename),
        source_loader_(source_loader),
        source_saver_(source_saver),
        source_time_(source_time),
        insert_line_directives_(insert_line_directives),
        used_files_(owner) {
    const size_t pos = filename_.rfind('/');
    if (pos != std::string::npos) {
      search_path_ = filename_.substr(0, pos);
      filename_ = filename_.substr(pos + 1, std::string::npos);
    }
  }

  void SetBasePath(const std::string& path) {
    base_path_ = path;
  }

  const std::vector<std::string> GetChangedDependencies() {
    std::vector<std::string> changed;
    for (FileInfoMap::iterator it = used_files_.begin();
        it != used_files_.end(); ++it) {
      std::chrono::system_clock::time_point timestamp;
      const bool found = source_time_(it->first, &timestamp);
      if (found && timestamp > it->second.timestamp) {
        it->second.timestamp = timestamp;
        changed.push_back(it->first);
      }
    }
    return changed;
  }

  const std::string GetSource() {
    // Stack of inputs to process to avoid recursion and keep all of the
    // intermediate data in one place.
    std::stack<InputInfo> stack;
    // Set of files that have been loaded in the current input stack frame.
    std::set<std::string> file_names;
    // The actual lines of shader source code.
    std::vector<std::string> output_source;

    // The id of the next input file to be processed.
    unsigned int input_id = 1;

    // Clear any old data.
    file_to_id_.clear();
    id_to_file_.clear();
    used_files_.clear();

    // Push top-level filename so we will have something to do.
    stack.push(InputInfo(BuildFilename(filename_)));
    while (!stack.empty()) {
      // Process the next input.
      InputInfo info = stack.top();
      stack.pop();
      // If the info has no lines then the file has not been loaded.
      if (info.lines.empty()) {
        // Check for a recursive $input.
        if (file_names.count(info.name) != 0) {
          LOG(WARNING) << stack.top().name << ":" << (stack.top().line - 1U)
                       << ": Recursive $input ignored while trying to $input"
                       << " \"" << info.name << "\".\n";
          continue;
        }

        // Load the source of the shader.
        const std::string source = source_loader_(info.name);

        // If the source does not exist or is empty then there is nothing to do.
        if (source.empty())
          continue;

        // If the identifier wasn't found, include a helpful error message.
        if (source == kUnknownShaderSentinel) {
          output_source.push_back(
              "#error Invalid shader source identifier: " + info.name);
          continue;
        }

        // Lines are counted from 1, so prepend an empty line.
        info.lines.push_back("");
        const std::vector<std::string> lines =
            base::SplitStringWithoutSkipping(source, "\n");
        info.lines.insert(info.lines.end(), lines.begin(), lines.end());

        // Mark this file as used so it will not be recursively included.
        file_names.insert(info.name);
        std::chrono::system_clock::time_point timestamp;
        source_time_(info.name, &timestamp);
        used_files_[info.name] = FileInfo(timestamp);

        // Add the file name to the list of inputs if it isn't there.
        if (file_to_id_.count(info.name) == 0) {
          file_to_id_[info.name] = input_id;
          id_to_file_[input_id] = info.name;
        }
        input_id++;

        // Reset info.
        info.id = file_to_id_[info.name];
        info.line = 1U;

        if (insert_line_directives_ && !stack.empty()) {
          // If this is not the top-level source file, inject a #line directive
          // indicating the first line of the new file.
          output_source.push_back(GetLineDirectiveString(1, info.id));
        }
      } else if (insert_line_directives_) {
        // We are returning to a file after an $input, so we need another #line
        // directive.
        output_source.push_back(GetLineDirectiveString(info.line - 1, info.id));
      }
      if (ParseInputLines(&stack, &info, &output_source)) {
        // Remove the file name from the name set so that it can be $input
        // again.
        file_names.erase(info.name);
      }
    }

    // Cat lines together with newlines.
    return base::JoinStrings(output_source, "\n");
  }

  // Returns the source of the passed filename, if is a dependency of the
  // source.
  const std::string GetDependencySource(const std::string& dependency) {
    if (DependsOn(dependency)) {
      source_time_(dependency, &used_files_[dependency].timestamp);
      return source_loader_(dependency);
    } else {
      return std::string();
    }
  }

  // Sets the source of dependency if this depends on it.
  bool SetDependencySource(const std::string& dependency,
                           const std::string& source) {
    return DependsOn(dependency) ? source_saver_(dependency, source) : false;
  }

  // Returns whether this composer depends on the passed filename.
  bool DependsOn(const std::string& dependency) const {
    return file_to_id_.find(dependency) != file_to_id_.end();
  }

  // Returns the name of the filename identified by id, if it was used,
  // otherwise returns an empty string.
  const std::string GetDependencyName(unsigned int id) const {
    std::string name;
    IdToFileMap::const_iterator it = id_to_file_.find(id);
    if (it != id_to_file_.end())
      name = it->second;
    return name;
  }

  // Returns all of the filenames used by this composer.
  const std::vector<std::string> GetDependencyNames() const {
    std::vector<std::string> names(file_to_id_.size());
    size_t i = 0;
    for (FileToIdMap::const_iterator it = file_to_id_.begin();
         it != file_to_id_.end(); ++it, ++i)
      names[i] = it->first;
    return names;
  }

 private:
  // Helper struct that contains $input information found during file parsing.
  struct InputInfo {
    explicit InputInfo(const std::string& file_name)
        : name(file_name), lines(), id(-1), line(-1) {}
    std::string name;
    std::vector<std::string> lines;
    int id;
    size_t line;
  };

  // Helper struct that contains the last modification time of a file.
  struct FileInfo {
    FileInfo() {}
    explicit FileInfo(std::chrono::system_clock::time_point timestamp)
        : timestamp(timestamp) {}
    std::chrono::system_clock::time_point timestamp;
  };

  // Mappings to and from file names and ids.
  typedef base::AllocMap<std::string, unsigned int> FileToIdMap;
  typedef base::AllocMap<unsigned int, std::string> IdToFileMap;
  typedef base::AllocMap<std::string, FileInfo> FileInfoMap;

  // Uses the base and search paths to construct a filename.
  const std::string BuildFilename(const std::string& filename) {
    const std::string base_path = base_path_.empty() ?
        "" : base_path_ + '/';
    const std::string search_path = search_path_.empty() ?
        "" : search_path_ + '/';
    return base_path + search_path + filename;
  }

  // Returns a #line directive given a line number and file id.
  std::string GetLineDirectiveString(size_t line, unsigned int file_id) {
    std::stringstream str;
    str << "#line " << line << " " << file_id;
    return str.str();
  }

  // Parses the lines of input from a file and looks for $input directives. If
  // a new $input is found, returns false to tell the caller the current file
  // is not done yet. If no $inputs are found, then this file is complete, and
  // this returns true.
  bool ParseInputLines(std::stack<InputInfo>* stack, InputInfo* info,
                       std::vector<std::string>* output_lines) {
    // Parse all lines of the $input.
    for (; info->line < info->lines.size(); ++info->line) {
      const std::string trimmed =
          base::TrimStartAndEndWhitespace(info->lines[info->line]);
      if (base::StartsWith(trimmed, "$input")) {
        // Get the file name to include and try to get its source. The file must
        // be contained within double quotes, e.g. a standard C-like include
        // statement of an ASCII filename.
        const size_t start_pos = trimmed.find("\"") + 1;
        const size_t end_pos = trimmed.find_last_of("\"");
        const std::string input_file =
            trimmed.substr(start_pos, end_pos - start_pos);
        const InputInfo new_info(BuildFilename(input_file));
        if (new_info.name.empty()) {
          // We could not get the $input name, perhaps the file is missing a
          // closing ".
          LOG(WARNING) << info->name << ":" << info->line
                       << ": Invalid $input directive, perhaps missing a '\"'?";
          // Just go to the next line of source.
          continue;
        } else {
          // Push current input for resumption later.
          ++info->line;  // Mark $input line as parsed.
          stack->push(*info);
          // Start parsing $input file.
          stack->push(new_info);
          return false;
        }
      } else {
        // No $input, so simply add this line to the source.
        output_lines->push_back(info->lines[info->line]);
        if (insert_line_directives_) {
          // Add an extra #line directive in case an $input was wrapped in a
          // #define. GLSL compilers typically ignore #line directives enclosed
          // in an unfollowed preprocessor path. This is simpler than actually
          // trying to preprocess the source.
          if (trimmed.find("#if") != std::string::npos ||
              trimmed.find("#el") != std::string::npos ||
              trimmed.find("#endif") != std::string::npos)
            output_lines->push_back(
                GetLineDirectiveString(info->line, info->id));
        }
      }
    }
    return true;
  }

  // Map filenames to ids and vice versa.
  FileToIdMap file_to_id_;
  IdToFileMap id_to_file_;

  // The top-level filename that contains the shader's source.
  std::string filename_;
  // The base path of the top-level filename.
  std::string search_path_;
  // The base path to be prepended to all filenames.
  std::string base_path_;
  // A function to get a source string given a filename.
  SourceLoader source_loader_;
  // A function to update a source string given a filename.
  SourceSaver source_saver_;
  // A function that returns the last time a dependency was modified.
  SourceModificationTime source_time_;
  // Whether to insert #line directives when an $input directive is found.
  bool insert_line_directives_;
  // The set of filenames that this shader depends on, and information about
  // them.
  FileInfoMap used_files_;
};

// Helper for StringComposer that plays a role similar to base::ZipAssetManager
// because it manages a globally-available set of id-string pairs.  This enables
// proper handling of $input directives.
class StringComposerRegistry {
 public:
  static const std::string GetStringContent(const std::string& label) {
    StringComposerRegistry* reg = GetRegistry();
    std::lock_guard<std::mutex> guard(reg->mutex_);
    auto it = reg->strings_.find(label);
    return it == reg->strings_.end() ? kUnknownShaderSentinel
                                     : it->second.content;
  }

  static bool SetStringContent(const std::string& label,
                               const std::string& source) {
    StringComposerRegistry* reg = GetRegistry();
    std::lock_guard<std::mutex> guard(reg->mutex_);
    reg->strings_[label].content = source;
    reg->strings_[label].last_modified = std::chrono::system_clock::now();
    return true;
  }

  static bool GetModificationTime(
      const std::string& label,
      std::chrono::system_clock::time_point* timestamp) {
    StringComposerRegistry* reg = GetRegistry();
    std::lock_guard<std::mutex> guard(reg->mutex_);
    auto it = reg->strings_.find(label);
    if (it == reg->strings_.end()) {
      return false;
    }
    *timestamp = it->second.last_modified;
    return true;
  }

 private:
  struct StringInfo {
    std::string content;
    std::chrono::system_clock::time_point last_modified;
  };

  typedef std::map<std::string, StringInfo> InfoMap;

  // The constructor is private since this is a singleton class.
  StringComposerRegistry() {}

  // This ensures that the registry will be safely destroyed when the program
  // exits.
  static StringComposerRegistry* GetRegistry() {
    ION_DECLARE_SAFE_STATIC_POINTER(StringComposerRegistry, s_registry);
    return s_registry;
  }

  // Mutex to guard access to the singleton.
  std::mutex mutex_;

  InfoMap strings_;

  DISALLOW_COPY_AND_ASSIGN(StringComposerRegistry);
};

//-----------------------------------------------------------------------------
//
// ShaderSourceComposer definition.
//
//-----------------------------------------------------------------------------
ShaderSourceComposer::ShaderSourceComposer() {}
ShaderSourceComposer::ShaderSourceComposer(
    const std::string& filename, const SourceLoader& source_loader,
    const SourceSaver& source_saver, const SourceModificationTime& source_time,
    bool insert_line_directives)
    : helper_(new (GetAllocator()) IncludeDirectiveHelper(
          *this, filename, source_loader, source_saver, source_time,
          insert_line_directives)) {}
ShaderSourceComposer::~ShaderSourceComposer() {}

void ShaderSourceComposer::SetBasePath(const std::string& path) {
  helper_->SetBasePath(path);
}

const std::vector<std::string> ShaderSourceComposer::GetChangedDependencies() {
  return helper_->GetChangedDependencies();
}

const std::string ShaderSourceComposer::GetSource() {
  return helper_->GetSource();
}

const std::string ShaderSourceComposer::GetDependencySource(
    const std::string& dependency) const {
  return helper_->GetDependencySource(dependency);
}

bool ShaderSourceComposer::SetDependencySource(const std::string& dependency,
                                               const std::string& source) {
  return helper_->SetDependencySource(dependency, source);
}

bool ShaderSourceComposer::DependsOn(const std::string& resource) const {
  return helper_->DependsOn(resource);
}

const std::string ShaderSourceComposer::GetDependencyName(
    unsigned int id) const {
  return helper_->GetDependencyName(id);
}

const std::vector<std::string> ShaderSourceComposer::GetDependencyNames()
    const {
  return helper_->GetDependencyNames();
}

//-----------------------------------------------------------------------------
//
// StringComposer definition.
//
//-----------------------------------------------------------------------------
StringComposer::StringComposer(const std::string& label,
                               const std::string& source)
    : ShaderSourceComposer(label, StringComposerRegistry::GetStringContent,
                           StringComposerRegistry::SetStringContent,
                           StringComposerRegistry::GetModificationTime, true) {
  StringComposerRegistry::SetStringContent(label, source);
}

StringComposer::~StringComposer() {}

//-----------------------------------------------------------------------------
//
// ZipAssetComposer definition.
//
//-----------------------------------------------------------------------------
ZipAssetComposer::ZipAssetComposer(const std::string& filename,
                                   bool insert_line_directives)
    : ShaderSourceComposer(
          filename, GetZipAssetFileData, SetAndSaveZipAssetData,
          base::ZipAssetManager::UpdateFileIfChanged, insert_line_directives) {}

ZipAssetComposer::~ZipAssetComposer() {}

}  // namespace gfxutils
}  // namespace ion
