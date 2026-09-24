/** @file
 * The endpoint Seer's window mounts: it listens on loopback with its
 * address under the state root it was given, answers `host` as Seer, and
 * refuses every other domain as not mounted, since Seer holds no sketch
 * session, no clock and no registry.
 */

#include <gtest/gtest.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/host/HostClient.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Inspection.h"
#include "ScratchDir.h"

namespace {

namespace protocol = sigil::protocol;
using protocol::Answer;

TEST(SeerInspection, AnswersHostAsSeerAndMountsNothingElse) {
  const sigil::test::ScratchDir state("seer-inspection");
  seer::Inspection inspection(0, state.path);
  ASSERT_TRUE(inspection.listening());
  EXPECT_TRUE(std::filesystem::exists(state.path / "protocol-address"));

  const protocol::InProcess client(inspection.dispatcher());
  std::optional<Answer<protocol::host::values::DescribeResult>> described;
  protocol::host::HostClient(client.caller())
      .describe(
          [&described](Answer<protocol::host::values::DescribeResult> answer) {
            described.emplace(std::move(answer));
          });
  ASSERT_TRUE(described && *described);
  EXPECT_EQ(described->result().version.program, "Seer");
  EXPECT_EQ(described->result().domains, (std::vector<std::string>{"host"}));
  EXPECT_EQ(described->result().state_root, state.path.string());
  EXPECT_TRUE(described->result().sessions.empty());

  for (const char* method :
       {"session.open", "clock.current", "registry.list"}) {
    std::string heard;
    client.send(std::string(R"({"id": 1, "method": ")") + method + R"("})",
                [&heard](std::string answer) { heard = std::move(answer); });
    EXPECT_NE(heard.find("notMounted"), std::string::npos)
        << method << ": " << heard;
  }
}

}  // namespace
