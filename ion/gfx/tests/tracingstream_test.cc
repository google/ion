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

#include "ion/gfx/tracingstream.h"
#include "ion/base/logchecker.h"

#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace ion {
namespace gfx {

TEST(TracingStreamTest, Smoke) {
#if ION_PRODUCTION

  // In production builds, GL tracing should be non-functional.
  TracingStream stream;
  stream.StartTracing();
  EXPECT_FALSE(stream.IsTracing());
  EXPECT_FALSE(stream.IsLogging());
  stream << 42U << "foo" << 2.4f;

#else

  ion::base::LogChecker log_checker;
  TracingStream stream;

  // Check that TracingStream is turned off by default.
  stream.Append(1, "Constellation of Kasterborous");
  EXPECT_EQ("", stream.String(1));
  EXPECT_EQ(false, stream.IsTracing());

  // Capture two visuals, one of which has a nested scope.
  stream.StartTracing();
  stream << "Milky Way\n";
  stream.EnterScope(0, "Solar System");
  stream << "Mercury\n";
  stream << "Venus\n";
  stream.EnableLogging();
  EXPECT_TRUE(stream.IsLogging());
  stream.EnterScope(0, "Earth");
  stream << "Moon" << std::hex << 42;
  stream.ExitScope(0);
  stream.DisableLogging();
  EXPECT_FALSE(stream.IsLogging());
  stream.ExitScope(0);
  stream.Append(42, "Andromeda\n");
  stream.StopTracing();

  // Check that the captured strings are what we expect.
  EXPECT_EQ(stream.Keys()[0], 0);
  EXPECT_EQ(stream.Keys()[1], 42);
  EXPECT_EQ(stream.String(0), stream.String());
  EXPECT_EQ(stream.String(0),
            "Milky Way\n"
            ">Solar System:\n"
            "  Mercury\n"
            "  Venus\n"
            "-->Earth:\n"
            "    Moon2a");
  EXPECT_EQ(stream.String(42), "Andromeda\n");

  // Check that the stream is properly cleared.
  stream.Clear();
  EXPECT_EQ(stream.Keys().size(), 0ul);

  // Make sure that the logging was enabled for Earth but not for Andromeda.
  // Logging is independent of string streams, which were cleared above.
  EXPECT_FALSE(log_checker.HasMessage("INFO", "Andromeda"));
  EXPECT_TRUE(log_checker.HasMessage("INFO", "Earth"));
#endif
}

}  // namespace gfx
}  // namespace ion
