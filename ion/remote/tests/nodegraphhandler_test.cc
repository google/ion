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

#include <iomanip>

#include "ion/base/invalid.h"
#include "ion/base/tests/multilinestringsequal.h"
#include "ion/base/zipassetmanager.h"
#include "ion/gfx/attributearray.h"
#include "ion/gfx/bufferobject.h"
#include "ion/gfx/node.h"
#include "ion/gfx/shaderinputregistry.h"
#include "ion/gfx/shape.h"
#include "ion/gfxutils/buffertoattributebinder.h"
#include "ion/gfxutils/frame.h"
#include "ion/math/vector.h"
#include "ion/remote/tests/httpservertest.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace remote {

class NodeGraphHandlerTest : public HttpServerTest {
 protected:
  void SetUp() override {
    HttpServerTest::SetUp();
    // Create and register a NodeGraphHandler.
    ngh_ = new NodeGraphHandler();
    server_->RegisterHandler(ngh_);
  }

  NodeGraphHandlerPtr ngh_;
};

//-----------------------------------------------------------------------------
//
// These tests do not require the server, so they do not use the harness.
//
//-----------------------------------------------------------------------------

TEST(NodeGraphHandler, Frame) {
  NodeGraphHandlerPtr ngh(new NodeGraphHandler);
  EXPECT_FALSE(ngh->GetFrame());
  gfxutils::FramePtr frame(new gfxutils::Frame);
  ngh->SetFrame(frame);
  EXPECT_EQ(frame, ngh->GetFrame());
  frame = nullptr;
  ngh->SetFrame(frame);
  EXPECT_FALSE(ngh->GetFrame());
}

TEST(NodeGraphHandler, Nodes) {
  NodeGraphHandlerPtr ngh(new NodeGraphHandler);
  gfx::NodePtr node1, node2, node3;
  EXPECT_EQ(0U, ngh->GetTrackedNodeCount());
  EXPECT_FALSE(ngh->IsNodeTracked(node1));
  node1 = new gfx::Node;
  EXPECT_FALSE(ngh->IsNodeTracked(node1));

  ngh->AddNode(node1);
  EXPECT_TRUE(ngh->IsNodeTracked(node1));
  EXPECT_FALSE(ngh->IsNodeTracked(node2));
  EXPECT_EQ(1U, ngh->GetTrackedNodeCount());

  ngh->AddNode(node2);  // No effect - NULL node.
  EXPECT_TRUE(ngh->IsNodeTracked(node1));
  EXPECT_FALSE(ngh->IsNodeTracked(node2));
  EXPECT_EQ(1U, ngh->GetTrackedNodeCount());

  ngh->AddNode(node1);  // No effect - already tracked.
  EXPECT_TRUE(ngh->IsNodeTracked(node1));
  EXPECT_FALSE(ngh->IsNodeTracked(node2));
  EXPECT_EQ(1U, ngh->GetTrackedNodeCount());

  node2 = new gfx::Node;
  ngh->AddNode(node2);
  EXPECT_TRUE(ngh->IsNodeTracked(node1));
  EXPECT_TRUE(ngh->IsNodeTracked(node2));
  EXPECT_EQ(2U, ngh->GetTrackedNodeCount());

  EXPECT_FALSE(ngh->RemoveNode(node3));  // No effect - NULL node.
  EXPECT_TRUE(ngh->IsNodeTracked(node1));
  EXPECT_TRUE(ngh->IsNodeTracked(node2));
  EXPECT_EQ(2U, ngh->GetTrackedNodeCount());

  EXPECT_TRUE(ngh->RemoveNode(node1));
  EXPECT_FALSE(ngh->IsNodeTracked(node1));
  EXPECT_TRUE(ngh->IsNodeTracked(node2));
  EXPECT_EQ(1U, ngh->GetTrackedNodeCount());

  EXPECT_FALSE(ngh->RemoveNode(node1));  // No effect - already removed.
  EXPECT_FALSE(ngh->IsNodeTracked(node1));
  EXPECT_TRUE(ngh->IsNodeTracked(node2));
  EXPECT_EQ(1U, ngh->GetTrackedNodeCount());

  EXPECT_TRUE(ngh->RemoveNode(node2));
  EXPECT_FALSE(ngh->IsNodeTracked(node1));
  EXPECT_FALSE(ngh->IsNodeTracked(node2));
  EXPECT_EQ(0U, ngh->GetTrackedNodeCount());
}

//-----------------------------------------------------------------------------
//
// Tests using the harness.
//
//-----------------------------------------------------------------------------

// A very simple vertex for testing.
struct Vertex {
  Vertex() {}
  Vertex(float x, float y) : v(x, y) {}
  math::Vector2f v;
};

// Creates and returns a Node containing a Shape. The ShaderInputRegistry is
// also returned because it has to persist through the test.
static const gfx::NodePtr BuildNodeWithShape(
    gfx::ShaderInputRegistryPtr* reg_out) {
  gfx::NodePtr root(new gfx::Node);
  gfx::ShapePtr shape(new gfx::Shape);
  shape->SetPrimitiveType(gfx::Shape::kPoints);
  gfx::AttributeArrayPtr aa(new gfx::AttributeArray);
  const Vertex vertex(1.0f, 2.0f);
  gfx::BufferObjectPtr bo(new gfx::BufferObject);
  base::DataContainerPtr container = base::DataContainer::CreateAndCopy<Vertex>(
      &vertex, 1, false, bo->GetAllocator());
  bo->SetData(container, sizeof(vertex), 1U, gfx::BufferObject::kStaticDraw);
  gfx::ShaderInputRegistryPtr reg(new gfx::ShaderInputRegistry);
  reg->Add(gfx::ShaderInputRegistry::AttributeSpec(
      "aBOE", gfx::kBufferObjectElementAttribute, "."));
  gfxutils::BufferToAttributeBinder<Vertex>(vertex)
      .Bind(vertex.v, "aBOE").Apply(reg, aa, bo);
  shape->SetAttributeArray(aa);
  root->AddShape(shape);
  *reg_out = reg;
  return root;
}

TEST_F(NodeGraphHandlerTest, ServeNodeGraph) {
  GetUri("/ion/nodegraph/does/not/exist");
  Verify404(__LINE__);

  GetUri("/ion/nodegraph/index.html");
  const std::string& index =
      base::ZipAssetManager::GetFileData("ion/nodegraph/index.html");
  EXPECT_FALSE(base::IsInvalidReference(index));
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  GetUri("/ion/nodegraph/");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  GetUri("/ion/nodegraph");
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(index, response_.data);

  // Strings used to build expected results.
  const std::string pre_frame("<span class=\"nodes_header\">Tracked Nodes");
  const std::string post_frame("</span><br><br>\n");
  const std::string text_start("<pre>\n");
  const std::string node_start_pre_address("ION Node");
  const std::string node_start_post_address(" {\n  Enabled: true\n");
  const std::string node_end("}\n");
  const std::string text_end("</pre>\n");

  // These are for the Node with a Shape.
  const std::string shape_start(
      "  ION Shape {\n"
      "    Primitive Type: Points\n"
      "    ION AttributeArray {\n");
  const std::string buffer_values(
      "      Buffer Values: {\n"
      "        v 0: [1, 2]\n"
      "      }\n");
  const std::string shape_end(
      "      ION Attribute (Buffer) {\n"
      "        Name: \"aBOE\"\n"
      "        Enabled: true\n"
      "        Normalized: false\n"
      "      }\n"
      "    }\n"
      "  }\n");

  // Update with no Nodes being tracked.
  GetUri("/ion/nodegraph/update");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      pre_frame + post_frame + text_start + text_end, response_.data));

  // Set a Frame and make sure the header changes.
  gfxutils::FramePtr frame(new gfxutils::Frame);
  frame->Begin();
  frame->End();
  frame->Begin();
  frame->End();  // Counter should now be 2.
  ngh_->SetFrame(frame);
  GetUri("/ion/nodegraph/update");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      pre_frame + " at frame 2" + post_frame + text_start + text_end,
      response_.data));
  ngh_->SetFrame(gfxutils::FramePtr());

  // Try the HTML version.
  GetUri("/ion/nodegraph/update?format=HTML");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      pre_frame + post_frame + "<div class=\"tree\">\n</div>\n",
      response_.data));

  // Add an empty Node to track.
  gfx::NodePtr empty_node(new gfx::Node);
  ngh_->AddNode(empty_node);
  GetUri("/ion/nodegraph/update");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      (pre_frame + post_frame + text_start + node_start_pre_address +
       node_start_post_address + node_end + text_end),
      response_.data));

  // Update again with address printing enabled.
  GetUri("/ion/nodegraph/update?enable_address_printing=true");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      (pre_frame + post_frame + text_start + node_start_pre_address +
       " [" + base::ValueToString(empty_node.Get()) + "]" +
       node_start_post_address + node_end + text_end),
      response_.data));

  // Track a Node with a Shape to test full shape printing.
  gfx::ShaderInputRegistryPtr reg;
  gfx::NodePtr node_with_shape(BuildNodeWithShape(&reg));
  ngh_->RemoveNode(empty_node);
  ngh_->AddNode(node_with_shape);
  GetUri("/ion/nodegraph/update");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      (pre_frame + post_frame + text_start + node_start_pre_address +
       node_start_post_address + shape_start + shape_end + node_end + text_end),
      response_.data));

  // Update again with full shape printing enabled.
  GetUri("/ion/nodegraph/update?enable_full_shape_printing=true");
  EXPECT_EQ(200, response_.status);
  EXPECT_TRUE(base::testing::MultiLineStringsEqual(
      (pre_frame + post_frame + text_start + node_start_pre_address +
       node_start_post_address + shape_start + buffer_values + shape_end +
       node_end + text_end),
      response_.data));
}

}  // namespace remote
}  // namespace ion

#endif
