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

#include "ion/remote/nodegraphhandler.h"


#include <algorithm>
#include <sstream>

#include "ion/base/invalid.h"
#include "ion/base/serialize.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/base/zipassetmanagermacros.h"
#include "ion/gfxutils/printer.h"

ION_REGISTER_ASSETS(IonRemoteNodeGraphRoot);

namespace ion {
namespace remote {

namespace {

// Search |root|'s hierarchy for a Node labeled |label|.
gfx::Node* SearchTrackedNodeHierarchy(gfx::Node* root,
                                      const std::string& label) {
  if (root->GetLabel() == label) {
    return root;
  }
  for (const auto& child : root->GetChildren()) {
    if (gfx::Node* child_search_result =
        SearchTrackedNodeHierarchy(child.Get(), label)) {
      return child_search_result;
    }
  }
  return nullptr;
}

// Search the hierarchies contained in the set of |nodes| for a Node labeled
// with |label|.
gfx::Node* SearchTrackedNodeHierarchy(const std::vector<gfx::NodePtr>& nodes,
                                      const std::string& label) {
  for (const auto& root : nodes) {
    if (gfx::Node* labeled_node =
        SearchTrackedNodeHierarchy(root.Get(), label)) {
      return labeled_node;
    }
  }
  return nullptr;
}

}  // namespace

NodeGraphHandler::NodeGraphHandler()
    : HttpServer::RequestHandler("/ion/nodegraph") {
  IonRemoteNodeGraphRoot::RegisterAssetsOnce();
}

NodeGraphHandler::~NodeGraphHandler() {}

void NodeGraphHandler::AddNode(const gfx::NodePtr& node) {
  if (node.Get() && !IsNodeTracked(node))
    nodes_.push_back(node);
}

bool NodeGraphHandler::RemoveNode(const gfx::NodePtr& node) {
  if (node.Get()) {
    std::vector<gfx::NodePtr>::iterator it =
        std::find(nodes_.begin(), nodes_.end(), node);
    if (it != nodes_.end()) {
      nodes_.erase(it);
      return true;
    }
  }
  return false;
}

bool NodeGraphHandler::IsNodeTracked(const gfx::NodePtr& node) const {
  return std::find(nodes_.begin(), nodes_.end(), node) != nodes_.end();
}

const std::string NodeGraphHandler::HandleRequest(
    const std::string& path_in, const HttpServer::QueryMap& args,
    std::string* content_type) {
  const std::string path = path_in.empty() ? "index.html" : path_in;

  if (path == "set_node_enable") {
    HttpServer::QueryMap::const_iterator it = args.find("node_label");
    std::string server_response;
    if (it != args.end()) {
      const auto& node_label = it->second;
      gfx::Node* named_node = SearchTrackedNodeHierarchy(nodes_, node_label);
      if (named_node) {
        named_node->Enable(!named_node->IsEnabled());
        server_response = "Success";
      } else {
        server_response = "Node not found.";
      }
    } else {
      server_response = "Malformed request; node_label argument expected but "
          "not found.";
    }
    return server_response;
  } else if (path == "update") {
    gfxutils::Printer printer;
    SetUpPrinter(args, &printer);
    return GetPrintString(&printer);
  } else {
    const std::string& data =
        base::ZipAssetManager::GetFileData("ion/nodegraph/" + path);
    if (!base::IsInvalidReference(data)) {
      // Ensure the content type is set if the editor HTML is requested.
      if (base::EndsWith(path, "html"))
        *content_type = "text/html";
      return data;
    }
  }
  return std::string();
}

void NodeGraphHandler::SetUpPrinter(const HttpServer::QueryMap& args,
                                    gfxutils::Printer* printer) {
  // Turn address printing off by default.
  printer->EnableAddressPrinting(false);

  HttpServer::QueryMap::const_iterator it = args.find("format");
  if (it != args.end()) {
    const std::string& format_string = it->second;
    if (format_string == "HTML")
      printer->SetFormat(gfxutils::Printer::kHtml);
  }

  it = args.find("enable_address_printing");
  if (it != args.end())
    printer->EnableAddressPrinting(it->second == "true");

  it = args.find("enable_full_shape_printing");
  if (it != args.end())
    printer->EnableFullShapePrinting(it->second == "true");
}

const std::string NodeGraphHandler::GetPrintString(gfxutils::Printer* printer) {
  std::ostringstream s;

  s << "<span class=\"nodes_header\">Tracked Nodes";
  if (frame_.Get())
    s << " at frame " << frame_->GetCounter();
  s << "</span><br><br>\n";

  if (printer->GetFormat() == gfxutils::Printer::kText)
    s << "<pre>\n";
  else if (printer->GetFormat() == gfxutils::Printer::kHtml)
    s << "<div class=\"tree\">\n";

  for (size_t i = 0; i < nodes_.size(); ++i)
    printer->PrintScene(nodes_[i], s);

  if (printer->GetFormat() == gfxutils::Printer::kText)
    s << "</pre>\n";
  else if (printer->GetFormat() == gfxutils::Printer::kHtml)
    s << "</div>\n";

  return s.str();
}

}  // namespace remote
}  // namespace ion

#endif
