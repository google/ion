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

#include "ion/remote/calltracehandler.h"

#include "ion/base/invalid.h"
#include "ion/base/stringutils.h"
#include "ion/base/zipassetmanager.h"
#include "ion/profile/calltracemanager.h"
#include "ion/profile/profiling.h"
#include "ion/profile/tracerecorder.h"
#include "ion/remote/tests/httpservertest.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace remote {

namespace {

// Dummy function for testing Profiler functions.
static void DummyFunc() {
  profile::ScopedTracer scope_tracer_(
      profile::GetCallTraceManager()->GetTraceRecorder(), "DummyFunc");
}

}  // anonymous namespace

class CallTraceHandlerTest : public RemoteServerTest {
 protected:
  void SetUp() override {
    RemoteServerTest::SetUp();
    server_->SetHeaderHtml("");
    server_->SetFooterHtml("");

    // Create a new CallTraceHandler that uses the global CallTraceManager.
    CallTraceHandler* cth = new CallTraceHandler();
    server_->RegisterHandler(HttpServer::RequestHandlerPtr(cth));
  }
};

#if (!defined(ION_PLATFORM_WINDOWS) || defined(ION_GOOGLE_INTERNAL))
// 
TEST_F(CallTraceHandlerTest, ServeProfile) {
  GetUri("/ion/calltrace/does/not/exist");
  Verify404(__LINE__);

  GetUri("/ion/calltrace/index.html");
  const std::string& md =
      base::ZipAssetManager::GetFileData("ion/calltrace/index.html");
  EXPECT_FALSE(base::IsInvalidReference(md));
  EXPECT_EQ(200, response_.status);
  EXPECT_EQ(md, response_.data);

  GetUri("/ion/calltrace/");
  EXPECT_EQ(200, response_.status);

  // Add a little history to the call trace.
  DummyFunc();

  // Try to fetch call trace data.
  GetUri("/ion/calltrace/call.wtf-trace");
  EXPECT_EQ(200, response_.status);
  EXPECT_LT(0u, response_.data.size());

  // The very first 4 bytes should be "0xDEADBEEF".
  const std::string expected_header = "\xEF\xBE\xAD\xDE";
  EXPECT_EQ(expected_header, response_.data.substr(0, 4));
}
#endif

}  // namespace remote
}  // namespace ion

#endif
