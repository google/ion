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

#ifndef ION_REMOTE_SHADERHANDLER_H_
#define ION_REMOTE_SHADERHANDLER_H_

#include "ion/remote/httpserver.h"

#include "ion/gfx/renderer.h"
#include "ion/gfxutils/shadermanager.h"

namespace ion {
namespace remote {

// ShaderHandler serves files related to shaders, their dependencies, and the
// shader editor.
//
// Shader-related pages have a proc-like structure. For example, if there
// are shaders shader1 and shader2 (with respective dependencies
// shader1_v_source, shader1_f_source, and shader2_v_source, shader2_f_source),
// are registered with the ShaderManager, then the following paths are valid
// (note that the paths are relative to the handler's root):
// /                                      - List of shaders
// /shader1                           - Lists info log and shader stages
// /shader1/|info log|                - shader1's link info log
// /shader1/vertex                    - Lists dependencies and info log
// /shader1/vertex/|info log|         - shader1's vertex shader info log
// /shader1/vertex/shader1_v_source   - Text of shader1_v_source
// /shader1/fragment/|info log|       - shader1's fragment shader info log
// /shader1/fragment/shader1_f_source - Text of shader1_f_source
// /shader2                           - Lists info log and shader stages
// /shader2/|info log|                - shader2's link info log
// /shader2/vertex                    - Lists dependencies and info log
// /shader2/vertex/|info log|         - shader2's vertex shader info log
// /shader2/vertex/shader2_v_source   - Text of shader2_v_source
// /shader2/fragment/|info log|       - shader2's fragment shader info log
// /shader2/fragment/shader2_f_source - Text of shader2_f_source
//
// /shader_editor serves the shader editor. It allows run-time editing
// of shaders by modifiying their dependencies directly. See the online help
// on the served page for more information.
class ION_API ShaderHandler : public HttpServer::RequestHandler {
 public:
  // A ShaderHandler requires a valid ShaderManager as well as a Renderer to
  // notify of changes made to shaders in the shader manager.
  ShaderHandler(const gfxutils::ShaderManagerPtr& shader_manager,
                const gfx::RendererPtr& renderer);
  ~ShaderHandler() override;

  const std::string HandleRequest(const std::string& path_in,
                                  const HttpServer::QueryMap& args,
                                  std::string* content_type) override;

 private:
  gfxutils::ShaderManagerPtr sm_;
  gfx::RendererPtr renderer_;
};

}  // namespace remote
}  // namespace ion

#endif  // ION_REMOTE_SHADERHANDLER_H_
