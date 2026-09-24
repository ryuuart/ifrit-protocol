// The receiver's command line and the protocol: no endpoint unless
// `--inspect` asked, the port `=PORT` names, the state root `--state`
// names, and an endpoint that listens and writes its address there,
// answers `host` as SpellCircle and mounts no other domain.

#include <gtest/gtest.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/host/HostClient.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ReceiverInspection.h"
#include "ScratchDir.h"

namespace {

using spellcircle::inspectionRequested;

TEST(ReceiverInspection, NothingIsMountedUnlessInspectIsAsked) {
  EXPECT_FALSE(inspectionRequested({"SpellCircle"}));
  EXPECT_FALSE(inspectionRequested({"SpellCircle", "--state", "/tmp/s"}));
  EXPECT_FALSE(inspectionRequested({"SpellCircle", "--inspect=port"}));
}

TEST(ReceiverInspection, InspectTakesAnOptionalPortAndTheStateRoot) {
  const auto any = inspectionRequested({"SpellCircle", "--inspect"});
  ASSERT_TRUE(any);
  EXPECT_EQ(any->port, 0);
  EXPECT_TRUE(any->stateRoot.empty());
  const auto stated = inspectionRequested(
      {"SpellCircle", "--state", "/tmp/receiver", "--inspect=9444"});
  ASSERT_TRUE(stated);
  EXPECT_EQ(stated->port, 9444);
  EXPECT_EQ(stated->stateRoot, "/tmp/receiver");
}

TEST(ReceiverInspection, AnEndpointAskedForListensAndWritesItsAddress) {
  const sigil::test::ScratchDir scratch("receiver-inspection");
  const auto request = inspectionRequested({"SpellCircle", "--inspect"});
  ASSERT_TRUE(request);
  spellcircle::ReceiverInspection inspection(*request, scratch.path);
  ASSERT_TRUE(inspection.listening()) << inspection.address();
  EXPECT_EQ(inspection.address().rfind("ws://127.0.0.1:", 0), 0u);
  EXPECT_TRUE(std::filesystem::exists(scratch.path / "protocol-address"));
  inspection.dispatch();
}

TEST(ReceiverInspection, AnswersHostAsTheReceiverAndMountsNothingElse) {
  const sigil::test::ScratchDir scratch("receiver-inspection-host");
  const auto request = inspectionRequested({"SpellCircle", "--inspect"});
  ASSERT_TRUE(request);
  spellcircle::ReceiverInspection inspection(*request, scratch.path);
  ASSERT_TRUE(inspection.listening()) << inspection.address();
  namespace protocol = sigil::protocol;
  const protocol::InProcess client(inspection.dispatcher());
  std::optional<protocol::Answer<protocol::host::values::DescribeResult>>
      described;
  protocol::host::HostClient(client.caller())
      .describe(
          [&described](
              protocol::Answer<protocol::host::values::DescribeResult> answer) {
            described.emplace(std::move(answer));
          });
  ASSERT_TRUE(described && *described);
  EXPECT_EQ(described->result().version.program, "SpellCircle");
  EXPECT_EQ(described->result().domains, (std::vector<std::string>{"host"}));
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
