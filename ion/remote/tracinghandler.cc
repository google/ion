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

#if !ION_PRODUCTION

#include "ion/remote/tracinghandler.h"

#include <vector>

#include "ion/base/invalid.h"
#include "ion/base/logging.h"
#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"

ION_REGISTER_ASSETS(IonRemoteTracingRoot);

namespace ion {
namespace remote {

namespace {

//-----------------------------------------------------------------------------
//
// Helper class that parses OpenGL tracing output and categorizes each line,
// then constructs HTML that represents the structured output.
//
//-----------------------------------------------------------------------------

class TracingHtmlHelper {
 public:
  TracingHtmlHelper();
  ~TracingHtmlHelper();

  // This takes the frame counter for the OpenGL trace and the string
  // containing the tracing output and appends to a string containing the HTML
  // for the structured output.
  void AddHtml(uint64 frame_counter, const std::string& trace_string,
               std::string* html_string);

 private:
  // This struct represents a parsed line in the OpenGL trace.
  struct ParsedLine {
    // Type of line.
    enum Type {
      kLabel,  // A label for some Ion object.
      kCall,   // A call to an OpenGL function.
      kError,  // An OpenGL error message.
    };

    ParsedLine() : type(kCall), level(0) {}

    Type type;         // Type of line.
    int level;         // Indentation level of line.
    std::string text;  // Tracing text with indentation stripped out.
  };

  // Adds a header string with an optional horizontal rule to the stream.
  void AddHeader(uint64 frame_counter, bool add_horizontal_rule,
                 std::ostringstream& s);  // NOLINT

  // Parses a tracing string, returning a vector of ParsedLine instances.
  const std::vector<ParsedLine> ParseLines(const std::string& trace_string);

  // Parses a single line of a tracing string, returning a ParsedLine instance.
  const ParsedLine ParseLine(const std::string& line);

  // Adds HTML for parsed lines to the stream.
  void AddHtmlForLines(const std::vector<ParsedLine>& parsed_lines,
                       std::ostringstream& s);  // NOLINT

  // Adds HTML for an OpenGL call to the stream, adding syntax coloring.
  void AddHtmlForCall(const std::string& line,
                      std::ostringstream& s);  // NOLINT
};

TracingHtmlHelper::TracingHtmlHelper() {}
TracingHtmlHelper::~TracingHtmlHelper() {}

void TracingHtmlHelper::AddHtml(uint64 frame_counter,
                                const std::string& trace_string,
                                std::string* html_string) {
  // This ostringstream is used to construct the new HTML string.
  std::ostringstream s;

  AddHeader(frame_counter, !html_string->empty(), s);
  const std::vector<ParsedLine> parsed_lines = ParseLines(trace_string);

  AddHtmlForLines(parsed_lines, s);

  html_string->append(s.str());
}

void TracingHtmlHelper::AddHeader(uint64 frame_counter,
                                  bool add_horizontal_rule,
                                  std::ostringstream& s) {  // NOLINT
  if (add_horizontal_rule)
    s << "<hr>\n";

  s << "<span class=\"trace_header\">OpenGL trace at frame "
    << frame_counter << "</span><br><br>\n";
}

const std::vector<TracingHtmlHelper::ParsedLine> TracingHtmlHelper::ParseLines(
    const std::string& trace_string) {
  const std::vector<std::string> lines = base::SplitString(trace_string, "\n");
  const size_t num_lines = lines.size();

  std::vector<ParsedLine> parsed_lines;
  parsed_lines.reserve(num_lines);
  for (size_t i = 0; i < num_lines; ++i)
    parsed_lines.push_back(ParseLine(lines[i]));

  return parsed_lines;
}

const TracingHtmlHelper::ParsedLine TracingHtmlHelper::ParseLine(
    const std::string& line) {
  DCHECK(!line.empty());
  const size_t length = line.size();
  ParsedLine parsed_line;

  // Errors contain the string "GL error"
  if (line.find("GL error") != std::string::npos) {
    static const char kErrorHeader[] = "*** GL error after call to";
    parsed_line.type = ParsedLine::kError;
    parsed_line.level = 0;
    // Remove "GL error after call to ".
    parsed_line.text = line.substr(sizeof(kErrorHeader));
  } else if (line[0] == '>' || line[0] == '-') {
    // Labels start with ">" or "---->" with some even number of dashes.
    const size_t num_dashes = line.find_first_not_of('-');
    // The number of dashes must be even and followed by a '>'.
    DCHECK_EQ(num_dashes % 1, 0U);
    DCHECK_LT(num_dashes + 1, length);
    DCHECK_EQ(line[num_dashes], '>');

    parsed_line.type = ParsedLine::kLabel;
    parsed_line.level = static_cast<int>(num_dashes / 2);
    parsed_line.text = line.substr(num_dashes + 1);

    // Remove any trailing colon.
    if (base::EndsWith(parsed_line.text, ":"))
      parsed_line.text.resize(parsed_line.text.size() - 1);
  } else {
    // Calls start with an even number of spaces.
    const size_t num_spaces = line.find_first_not_of(' ');
    // The number of spaces must be even.
    DCHECK_EQ(num_spaces % 1, 0U);

    parsed_line.type = ParsedLine::kCall;
    parsed_line.level = static_cast<int>(num_spaces / 2);
    parsed_line.text = line.substr(num_spaces);
  }
  return parsed_line;
}

void TracingHtmlHelper::AddHtmlForLines(
    const std::vector<ParsedLine>& parsed_lines,
    std::ostringstream& s) {  // NOLINT
  s << "<div class=\"tree\">\n<ul>\n";

  const size_t num_lines = parsed_lines.size();
  int cur_list = 0;
  int list_level = -1;

  for (size_t i = 0; i < num_lines; ++i) {
    const ParsedLine& line = parsed_lines[i];

    // Errors are handled specially.
    if (line.type == ParsedLine::kError) {
      s << "<br><span class=\"trace_error\">" << "***OpenGL Error: "
        << line.text << "</span><br><br>\n";
      continue;
    }

    // Close lists at higher levels.
    while (list_level >= line.level) {
      s << "</ul>\n</li>\n";
      --list_level;
    }

    if (line.type == ParsedLine::kLabel) {
      // A label starts a new unnumbered list.
      const size_t list_id = cur_list++;
      s << "<li><input type =\"checkbox\" checked=\"checked\" id=\"list-"
        << list_id << "\"/><label for=\"list-" << list_id << "\">" << line.text
        << "</label>\n<ul>\n";
      list_level = line.level;
    } else {
      // Parse OpenGL calls to add coloring by syntax.
      s << "<li>";
      AddHtmlForCall(line.text, s);
      s << "</li>\n";
    }
  }

  // Close open lists.
  while (list_level >= 0) {
    s << "</ul>\n</li>\n";
    --list_level;
  }

  s << "</ul>\n</div>\n";
}

void
TracingHtmlHelper::AddHtmlForCall(const std::string& line,
                                  std::ostringstream& s) {  // NOLINT
  std::vector<std::string> args = base::SplitString(line, "(),");
  const size_t arg_count = args.size();

  // Remove whitespace around args to make processing easier.
  for (size_t i = 0; i < arg_count; ++i)
    args[i] = base::TrimStartAndEndWhitespace(args[i]);

  // The first argument is the function name.
  s << "<span class=\"trace_function\">" << args[0] << "</span>(";

  for (size_t i = 1; i < arg_count; ++i) {
    std::string arg = args[i];

    // For each function argument, look for "name = value".
    const size_t pos = arg.find(" = ");
    if (pos != std::string::npos) {
      if (i > 1)
        s << "</span>, ";
      s << "<span class=\"trace_arg_name\">"
        << arg.substr(0, pos) << "</span> = <span class=\"trace_arg_value\">"
        << arg.substr(pos + 3, std::string::npos);
    } else {
      // If there is no equal sign, this is part of the previous argument.
      s << ", " << arg;
    }
  }
  if (arg_count > 1)
    s << "</span>";
  s << ")";
}

//-----------------------------------------------------------------------------
//
// Helper functions.
//
//-----------------------------------------------------------------------------

// Returns the ResourceType associated with the given string name.
static gfx::Renderer::ResourceType GetResourceTypeFromName(
    const std::string& name) {
  static const char* kResourceNames[gfx::Renderer::kNumResourceTypes] = {
    "Attribute Arrays",         // Renderer::kAttributeArray,
    "Buffer Objects",           // Renderer::kBufferObject,
    "Framebuffer Objects",      // Renderer::kFramebufferObject,
    "Samplers",                 // Renderer::kSampler,
    "Shader Input Registries",  // Renderer::kShaderInputRegistry,
    "Shader Programs",          // Renderer::kShaderProgram,
    "Shaders",                  // Renderer::kShader,
    "Textures",                 // Renderer::kTexture,
  };
  int index;
  for (index = 0; index < gfx::Renderer::kNumResourceTypes; ++index) {
    if (name == kResourceNames[index])
      break;
  }
  DCHECK_LT(index, gfx::Renderer::kNumResourceTypes);
  return static_cast<gfx::Renderer::ResourceType>(index);
}

}  // anonymous namespace

//-----------------------------------------------------------------------------
//
// TracingHandler functions.
//
//-----------------------------------------------------------------------------

TracingHandler::TracingHandler(const gfxutils::FramePtr& frame,
                               const gfx::RendererPtr& renderer)
    : HttpServer::RequestHandler("/ion/tracing"),
      frame_(frame),
      renderer_(renderer),
      prev_stream_(renderer_->GetGraphicsManager()->GetTracingStream()),
      state_(kInactive),
      frame_counter_(0) {
  using std::bind;
  using std::placeholders::_1;

  IonRemoteTracingRoot::RegisterAssetsOnce();

  // Install frame callbacks to do the work.
  if (frame_.Get()) {
    frame_->AddPreFrameCallback("TracingHandler",
                                bind(&TracingHandler::BeginFrame, this, _1));
    frame_->AddPostFrameCallback("TracingHandler",
                                 bind(&TracingHandler::EndFrame, this, _1));
  }
}

TracingHandler::~TracingHandler() {
  // Uninstall frame callbacks.
  if (frame_.Get()) {
    frame_->RemovePreFrameCallback("TracingHandler");
    frame_->RemovePostFrameCallback("TracingHandler");
  }

  // Restore the previous stream.
  renderer_->GetGraphicsManager()->SetTracingStream(prev_stream_);
}

const std::string TracingHandler::HandleRequest(
    const std::string& path_in, const HttpServer::QueryMap& args,
    std::string* content_type) {
  const std::string path = path_in.empty() ? "index.html" : path_in;

  if (path == "trace_next_frame") {
    // Store the list of resources to delete, if any.
    const HttpServer::QueryMap::const_iterator it =
        args.find("resources_to_delete");
    resources_to_delete_ = it != args.end() ? it->second : "";
    // Tests use the "nonblocking" flag to avoid blocking until a frame is
    // rendered.
    TraceNextFrame(args.find("nonblocking") == args.end());
    return html_string_;
  } else if (path == "clear") {
    html_string_.clear();
    return "clear";
  } else {
    const std::string& data =
        base::ZipAssetManager::GetFileData("ion/tracing/" + path);
    if (!base::IsInvalidReference(data)) {
      // Ensure the content type is set if the editor HTML is requested.
      if (base::EndsWith(path, "html"))
        *content_type = "text/html";
      return data;
    }
  }
  return std::string();
}

void TracingHandler::TraceNextFrame(bool block_until_frame_rendered) {
  if (frame_.Get()) {
    // Set the state so that tracing occurs during the next frame.
    state_ = kWaitingForBeginFrame;
    // If not blocking, just call Begin() and End() explicitly.
    if (!block_until_frame_rendered) {
      frame_->Begin();
      frame_->End();
    }
    semaphore_.Wait();
    DCHECK_EQ(state_, kInactive);
    // Add HTML to the string and clear the tracing stream.
    TracingHtmlHelper helper;
    helper.AddHtml(frame_counter_, tracing_stream_.str(), &html_string_);
    tracing_stream_.str("");
  }
}

void TracingHandler::BeginFrame(const gfxutils::Frame& frame) {
  if (state_ == kWaitingForBeginFrame) {
    // Delete named resources if requested.
    if (!resources_to_delete_.empty()) {
      // Resource names are separated by commas.
      const std::vector<std::string> resources =
          base::SplitString(resources_to_delete_, ",");
      const size_t num_resources = resources.size();
      for (size_t i = 0; i < num_resources; ++i)
        renderer_->ClearTypedResources(GetResourceTypeFromName(resources[i]));
      resources_to_delete_.clear();
    }
    frame_counter_ = frame.GetCounter();
    renderer_->GetGraphicsManager()->SetTracingStream(&tracing_stream_);
    state_ = kWaitingForEndFrame;
  }
}

void TracingHandler::EndFrame(const gfxutils::Frame& frame) {
  if (state_ == kWaitingForEndFrame) {
    renderer_->GetGraphicsManager()->SetTracingStream(prev_stream_);
    state_ = kInactive;
    // Clear the semaphore so HandleRequest() can proceed.
    semaphore_.Post();
  }
}

}  // namespace remote
}  // namespace ion

#endif
