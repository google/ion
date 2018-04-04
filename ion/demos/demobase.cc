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

#include "ion/demos/demobase.h"

#if !ION_PRODUCTION
#include "ion/remote/remoteserver.h"

std::unique_ptr<ion::remote::RemoteServer> DemoBase::remote_;
#endif


#if defined(ION_PLATFORM_ASMJS)
extern "C" {
char* IonRemoteGet(const char* page) {
#if !ION_PRODUCTION
  const std::string content = DemoBase::GetRemoteServer() ?
      DemoBase::GetRemoteServer()->GetUriData(page) : "";
  // Emscripten should clean up the memory.
  char* content_cstr = reinterpret_cast<char*>(malloc(content.length() + 1));
  memcpy(content_cstr, content.c_str(), content.length());
  content_cstr[content.length()] = 0;
  return content_cstr;
#else
  return "";
#endif
}
}
#endif

DemoBase::~DemoBase() {
#if !ION_PRODUCTION
  remote_.reset(nullptr);
#endif
}

// Rewrite the shader to be compatible with the given GL version.
// 
const std::string RewriteShader(const std::string& source,
                                ion::gfx::GraphicsManager::GlFlavor gl_flavor,
                                unsigned int version, bool is_fragment_shader) {
  std::string body = source;
  std::string preamble;
  const std::string es_fragment_boilerplate =
      "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
      "precision highp float;\n"
      "#else\n"
      "precision mediump float;\n"
      "#endif\n";
  bool modernize = false;
  if (gl_flavor == ion::gfx::GraphicsManager::kDesktop) {
    preamble = "#version 140\n";
    modernize = true;
  } else {
    if (gl_flavor == ion::gfx::GraphicsManager::kEs && version >= 30) {
      preamble = "#version 300 es\n";
      modernize = true;
    } else {
      preamble =
          "#version 100 es\n"
          "#extension EXT_draw_instanced : enable\n";
      if (is_fragment_shader) preamble += es_fragment_boilerplate;
      body =
          ion::base::ReplaceString(body, "gl_InstanceID", "gl_InstanceIDEXT");
      modernize = false;
    }
  }
  if (modernize) {
    if (is_fragment_shader) {
      preamble += es_fragment_boilerplate + "out vec4 FragColor;\n";
      body = ion::base::ReplaceString(body, "gl_FragColor", "FragColor");
    }
    // Replace deprecated storage qualifiers with modern equivalents.
    body = ion::base::ReplaceString(body, "attribute", "in");
    body = ion::base::ReplaceString(body, "varying",
                                    is_fragment_shader ? "in" : "out");
  }
  return preamble + body;
}
