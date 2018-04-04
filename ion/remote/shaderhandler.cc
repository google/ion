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

#if !ION_PRODUCTION

#include "ion/remote/shaderhandler.h"

#include <regex>  // NOLINT(build/c++11)
#include <sstream>

#include "base/integral_types.h"
#include "ion/base/invalid.h"
#include "ion/base/staticsafedeclare.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/gfx/shaderprogram.h"
#include "ion/gfxutils/resourcecallback.h"
#include "ion/gfxutils/shadermanager.h"

ION_REGISTER_ASSETS(IonRemoteShadersRoot);

namespace ion {
namespace remote {

using gfxutils::ShaderManagerPtr;
using gfxutils::ShaderSourceComposerPtr;

namespace {

// Handy typedef to save some typing.
typedef std::vector<std::string> StringVector;

static const char kInfoLogString[] = "|info log|";

// Makes an HTML page that contains the passed title, description and an
// unordered HTML list of the passed elements. Each element links to its name
// within the passed dir.
static std::string SimpleHtmlList(const std::string& title,
                                  const std::string& description,
                                  const std::string& dir,
                                  const StringVector& elements) {
  std::stringstream str;
  str << "<!DOCTYPE html>\n<html><head><title>" << title << "</title></head>"
      << "<body><link rel=\"stylesheet\" href=\"/ion/css/style.css\" />\n"
      << description << "\n<ul>\n";

  const size_t count = elements.size();
  for (size_t i = 0; i < count; ++i) {
    str << "  <li><a href=\"" << dir << "/" << elements[i] << "\">"
        << elements[i] << "</a></li>\n";
  }
  str << "</body>\n</html>\n";
  return str.str();
}

//-----------------------------------------------------------------------------
//
// The following functions return raw text if serve_raw is true, otherwise they
// set the content type to HTML and return HTML-formatted text.
//
//-----------------------------------------------------------------------------

// Returns a string that lists the names of all programs registered through the
// ShaderManager.
static const std::string GetProgramNamesString(const ShaderManagerPtr& sm,
                                               bool serve_raw,
                                               std::string* content_type) {
  StringVector shaders = sm->GetShaderProgramNames();
  if (serve_raw) {
    // Return a a list of all strings joined by newlines.
    if (shaders.empty())
      return "\n";
    else
      return base::JoinStrings(shaders, "\n");
  } else {
    // Return an HTML formatted page.
    *content_type = "text/html";
    // Add a link to the shader editor.
    shaders.push_back("shader_editor");
    return SimpleHtmlList("Registered shader programs",
                          "<h3>List of registered shader programs. Click on a"
                          " name to see shader stages.</h3>", "/ion/shaders",
                          shaders);
  }
}

// Returns a string that lists the shader stages of the named program.
static const std::string GetShaderStagesString(const std::string& program_name,
                                               bool serve_raw,
                                               std::string* content_type) {
  StringVector stages(3);
  stages[0] = kInfoLogString;
  stages[1] = "vertex";
  stages[2] = "fragment";
  if (serve_raw) {
    return base::JoinStrings(stages, "\n");
  } else {
    *content_type = "text/html";
    return SimpleHtmlList(
        std::string("Info log and shader stages for ") + program_name,
        std::string("<h3>Info log and shader stages for program '") +
            program_name + "'. Click on a stage to see shader sources.</h3>",
        program_name, stages);
  }
}

// Returns a string that lists the dependencies for the named program's stage.
static const std::string GetDependenciesString(
    const std::string& program_name, const std::string& stage,
    const StringVector& dependencies, bool serve_raw,
    std::string* content_type) {
  if (serve_raw) {
    // Return a a list of all strings joined by newlines.
    return base::JoinStrings(dependencies, "\n");
  } else {
    // Return an HTML formatted page.
    *content_type = "text/html";
    return SimpleHtmlList(
        std::string("List of dependencies for the ") + stage +
            std::string(" stage of ") + program_name,
        std::string("<h3>List of dependencies for the ") + stage +
            std::string(" stage of program '") + program_name +
            "'. Click on a stage to see shader sources.</h3>",
        stage, dependencies);
  }
}

// Uses the passed composer to replace input ids with dependency names in the
// passed log. Returns the formatted log.
static const std::string FormatInfoLog(
    const std::string& log,
    const ShaderSourceComposerPtr& composer) {
  enum class InfoLogStyle { kNvidia = 0, kMacIOS, kCount };
  constexpr size_t kInfoLogStyleCount =
      static_cast<size_t>(InfoLogStyle::kCount);

  enum class InfoLogField { kInputID = 0, kLine, kMessage, kCount };
  constexpr size_t kInfoLogFieldCount =
      static_cast<size_t>(InfoLogField::kCount);

  // Known info log regex patterns.  OpenGL information log messages are not
  // stable and may vary between both implementors and versions.
  // When adding a new pattern to infolog_patterns, be sure to add the indices
  // to field_indices;  These direct the matcher to use these specific match
  // indices to extract data.
  ION_DECLARE_SAFE_STATIC_ARRAY_WITH_INITIALIZERS(
      std::regex, infolog_patterns, kInfoLogStyleCount,
      std::regex(R"pattern(\s*(\d+)\((\d+)\).*:(.*))pattern"),  // kNvidia
      std::regex(R"pattern(^.*:\s*(\d+):(\d+):(.*))pattern"));  // kMacIOS
  static constexpr size_t
      field_indices[kInfoLogStyleCount][kInfoLogFieldCount] = {
          {1 /* input id */, 2 /* line number */, 3 /* message */},  // KNvidia
          {1 /* input id */, 2 /* line number */, 3 /* message */},  // kMacIOS
      };

  const StringVector lines = base::SplitString(log, "\n");
  std::stringstream str;
  for (const auto& line : lines) {
    std::smatch match;
    bool found = false;
    for (size_t match_index = 0; match_index < kInfoLogStyleCount;
         ++match_index) {
      if (std::regex_match(line, match, infolog_patterns[match_index])) {
        const size_t input_id_index =
            field_indices[match_index]
                         [static_cast<size_t>(InfoLogField::kInputID)];
        const size_t line_number_index =
            field_indices[match_index]
                         [static_cast<size_t>(InfoLogField::kLine)];
        const size_t message_index =
            field_indices[match_index]
                         [static_cast<size_t>(InfoLogField::kMessage)];

        const uint32 input_id = base::StringToInt32(match[input_id_index]);

        str << composer->GetDependencyName(input_id) << ":"
            << match[line_number_index] << ":" << match[message_index];
        found = true;
        break;
      }
    }

    // Unknown error string format; Use verbatim.
    if (!found) {
      str << line;
    }
    str << "<br>\n";
  }
  return str.str();
}

// Returns the info log of the passed program for the passed stage, which must
// be one of "vertex", "fragment" or "link". The passed composer is used to
// format the info log if there are any warnings or errors.
static const std::string GetShaderProgramInfoLog(
    const gfx::ShaderProgram* program,
    const ShaderSourceComposerPtr& composer,
    const std::string& stage) {
  DCHECK(program);
  std::string log;
  if (stage == "vertex" && program->GetVertexShader().Get())
    log = FormatInfoLog(program->GetVertexShader()->GetInfoLog(), composer);
  else if (stage == "fragment" && program->GetFragmentShader().Get())
    log = FormatInfoLog(program->GetFragmentShader()->GetInfoLog(), composer);
  else
    log = program->GetInfoLog();

  // An empty info log means success.
  if (log.empty())
    log = "OK";
  return log;
}

// The passed path is analyzed to figure out which of page to serve. See the
// header comment for ShaderHandler for more details.
static const std::string GetShadersRootString(const ShaderManagerPtr& sm,
                                              const gfx::RendererPtr& renderer,
                                              const std::string& path,
                                              const HttpServer::QueryMap& args,
                                              std::string* content_type) {
  const bool serve_raw = args.find("raw") != args.end();
  if (path.empty()) {
    return GetProgramNamesString(sm, serve_raw, content_type);
  } else {
    const StringVector names = base::SplitString(path, "/");
    if (const gfx::ShaderProgram* program =
        sm->GetShaderProgram(names[0]).Get()) {
      // The next level is the type of shader, vertex or fragment.
      if (names.size() == 1) {
        return GetShaderStagesString(names[0], serve_raw, content_type);
      } else if (names.size() >= 2) {
        // Get the composer for the requested stage.
        ShaderSourceComposerPtr composer;
        if (names[1] == "vertex")
          sm->GetShaderProgramComposers(names[0], &composer, nullptr);
        else if (names[1] == "fragment")
          sm->GetShaderProgramComposers(names[0], nullptr, &composer);
        else if (names[1] == kInfoLogString)
          return GetShaderProgramInfoLog(program, composer, "link");

        if (composer.Get()) {
          StringVector dependencies;
          // Add links to the info log and unified shader source.
          dependencies.push_back(kInfoLogString);
          const StringVector deps = composer->GetDependencyNames();
          dependencies.insert(dependencies.end(), deps.begin(), deps.end());

          // Check if the source is being set.
          HttpServer::QueryMap::const_iterator it = args.find("set_source");
          if (it == args.end()) {
            // If the path is not to a specific dependency then serve a list of
            // all of them, otherwise serve the requested one.
            if (names.size() == 2) {
              return GetDependenciesString(
                  names[0], names[1], dependencies, serve_raw, content_type);
            } else {
              // Serve either the info log or the dependency, restoring any '/'
              // in its name. Note that GetShaderProgramInfoLog() cannot be
              // called with an invalid stage since that would mean composer
              // is nullptr.
              if (names[2] == kInfoLogString)
                return GetShaderProgramInfoLog(program, composer, names[1]);
              else
                return composer->GetDependencySource(base::JoinStrings(
                    StringVector(names.begin() + 2, names.end()), "/"));
            }
          } else {
            // Set the dependency source. If the composer has no dependencies
            // then it will set its internal source.
            const std::string dep_name = base::JoinStrings(
                StringVector(names.begin() + 2, names.end()), "/");
            composer->SetDependencySource(dep_name, it->second);
            sm->RecreateShaderProgramsThatDependOn(dep_name);
            // Now we have to wait for the programs to be recreated.
            if (renderer.Get()) {
              gfxutils::ProgramCallback::RefPtr callback(
                  new (renderer->GetAllocatorForLifetime(base::kShortTerm))
                      gfxutils::ProgramCallback());
              gfx::ResourceManager* rm = renderer->GetResourceManager();
              rm->RequestAllResourceInfos<gfx::ShaderProgram,
                  gfx::ResourceManager::ProgramInfo>(
                      std::bind(&gfxutils::ProgramCallback::Callback,
                          callback.Get(), std::placeholders::_1));
              callback->WaitForCompletion(nullptr);
            }
            return "Shader source changed.";
          }
        }
      }
    }
  }
  return std::string();
}

// Serves the names and status of all shaders registered through the
// ShaderManager.
static const std::string ServeShaderStatus(const ShaderManagerPtr& sm) {
  StringVector shaders = sm->GetShaderProgramNames();
  const size_t count = shaders.size();
  StringVector logs(3);
  for (size_t i = 0; i < count; ++i) {
    const gfx::ShaderProgramPtr program =
        sm->GetShaderProgram(shaders[i]);
    ShaderSourceComposerPtr vertex_composer;
    ShaderSourceComposerPtr fragment_composer;
    sm->GetShaderProgramComposers(shaders[i], &vertex_composer,
                                  &fragment_composer);

    if (program.Get()) {
      static const char kOk[] = "OK";
      static const char kError[] = "Error";
      logs[0] = (!program->GetVertexShader().Get() ||
                 program->GetVertexShader()->GetInfoLog().empty())
                    ? kOk
                    : kError;
      logs[1] = (!program->GetFragmentShader().Get() ||
                 program->GetFragmentShader()->GetInfoLog().empty())
                    ? kOk
                    : kError;
      logs[2] = program->GetInfoLog().empty() ? kOk : kError;
      shaders[i] += "," + base::JoinStrings(logs, ",");
    }
  }
  // Return a list of all strings joined by newlines. If there are no shaders,
  // just return a newline since an empty string means the path cannot be
  // served.
  if (shaders.empty())
    return "\n";
  else
    return base::JoinStrings(shaders, "\n");
}

// Updates all shaders in the passed ShaderManager that need to be updated, and
// returns a semicolon-delimited string containing their names.
static const std::string UpdateAndServeChangedDependencies(
    const ShaderManagerPtr& sm) {
  StringVector shaders = sm->GetShaderProgramNames();
  const size_t count = shaders.size();
  std::set<std::string> changed_set;
  for (size_t i = 0; i < count; ++i) {
    const gfx::ShaderProgramPtr program =
        sm->GetShaderProgram(shaders[i]);
    ShaderSourceComposerPtr vertex_composer;
    ShaderSourceComposerPtr fragment_composer;
    sm->GetShaderProgramComposers(shaders[i], &vertex_composer,
                                  &fragment_composer);
    if (vertex_composer.Get()) {
      const std::vector<std::string> changed =
          vertex_composer->GetChangedDependencies();
      changed_set.insert(changed.begin(), changed.end());
    }
    if (fragment_composer.Get()) {
      const std::vector<std::string> changed =
          fragment_composer->GetChangedDependencies();
      changed_set.insert(changed.begin(), changed.end());
    }
  }

  // Create a semicolon-delimited list of changed dependencies.
  if (const size_t count = changed_set.size()) {
    const std::vector<std::string> changed(changed_set.begin(),
                                           changed_set.end());
    for (size_t i = 0; i < count; ++i)
      sm->RecreateShaderProgramsThatDependOn(changed[i]);
    return base::JoinStrings(changed, ";");
  } else {
    return ";";
  }
}

}  // anonymous namespace

ShaderHandler::ShaderHandler(const gfxutils::ShaderManagerPtr& shader_manager,
                             const gfx::RendererPtr& renderer)
    : HttpServer::RequestHandler("/ion/shaders"),
      sm_(shader_manager),
      renderer_(renderer) {
  // Register assets.
  IonRemoteShadersRoot::RegisterAssetsOnce();
}

ShaderHandler::~ShaderHandler() {}

const std::string ShaderHandler::HandleRequest(const std::string& path_in,
                                               const HttpServer::QueryMap& args,
                                               std::string* content_type) {
  const std::string path =
      path_in == "shader_editor" ? "shader_editor/index.html" : path_in;

  if (path == "shader_status") {
    return ServeShaderStatus(sm_);
  } else if (path == "update_changed_dependencies") {
    return UpdateAndServeChangedDependencies(sm_);
  } else if (base::StartsWith(path, "shader_editor")) {
    const std::string& data = base::ZipAssetManager::GetFileData(
        "ion/shaders/" + path);
    if (base::IsInvalidReference(data)) {
      return std::string();
    } else {
      // Ensure the content type is set if the editor HTML is requested.
      if (base::EndsWith(path, "html"))
        *content_type = "text/html";
      return data;
    }
  } else {
    return GetShadersRootString(sm_, renderer_, path, args, content_type);
  }
}

}  // namespace remote
}  // namespace ion

#endif
