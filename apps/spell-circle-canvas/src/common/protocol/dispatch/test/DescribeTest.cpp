/** @file
 * host.describe, the first command a client sends, answered on every
 * dispatcher by the host domain this library mounts itself: the
 * definition's revision, the program's name and version, the domains
 * mounted, the clock's policy, the state root, the sessions open and the
 * clients attached.
 */

#include <gtest/gtest.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/host/HostClient.h>

#include <optional>
#include <string>
#include <vector>

#include "ClockUnderTest.h"

namespace {

namespace protocol = sigil::protocol;
using protocol::Answer;
using protocol::host::values::DescribeResult;

/** What describe answers @p client, which in process is at once. */
std::optional<Answer<DescribeResult>> describe(
    const protocol::InProcess& client) {
  std::optional<Answer<DescribeResult>> answered;
  protocol::host::HostClient(client.caller())
      .describe([&](Answer<DescribeResult> answer) {
        answered.emplace(std::move(answer));
      });
  return answered;
}

protocol::Program sketchbook() {
  protocol::Program program;
  program.name = "Sketchbook";
  program.version = "0.1";
  program.stateRoot = "/state";
  program.clockPolicy = [] { return protocol::clock::Policy_Advance; };
  program.sessions = [] {
    protocol::session::values::Summary open;
    open.sketch = "ember";
    open.kind = "canvas";
    return std::vector<protocol::session::values::Summary>{open};
  };
  return program;
}

TEST(ProtocolDescribe, IsAnsweredOnADispatcherNoAgentWasMountedOn) {
  protocol::Dispatcher dispatcher;
  const protocol::InProcess client(dispatcher);

  const std::optional<Answer<DescribeResult>> described = describe(client);
  ASSERT_TRUE(described);
  ASSERT_TRUE(*described) << described->error().message;
  const DescribeResult& result = described->result();
  // The revision a value made with nothing set carries is the
  // definition's own.
  EXPECT_EQ(result.version.revision, protocol::values::Revision{});
  EXPECT_EQ(result.domains, std::vector<std::string>{"host"});
  // A program that supplies no clock runs by the wall's, and one that
  // supplies no sessions holds none.
  EXPECT_EQ(result.clock, protocol::clock::Policy_Wall);
  EXPECT_TRUE(result.sessions.empty());
  EXPECT_EQ(result.attached, std::vector<std::string>{client.session()});
}

TEST(ProtocolDescribe, AnswersWhatTheProgramAndTheDispatcherEachKnow) {
  protocol::Dispatcher dispatcher(sketchbook());
  protocol::test::ClockUnderTest clock(dispatcher);
  const protocol::InProcess first(dispatcher);
  const protocol::InProcess second(dispatcher);

  const std::optional<Answer<DescribeResult>> described = describe(second);
  ASSERT_TRUE(described && *described);
  const DescribeResult& result = described->result();
  EXPECT_EQ(result.version.program, "Sketchbook");
  EXPECT_EQ(result.version.program_version, "0.1");
  EXPECT_EQ(result.domains, (std::vector<std::string>{"clock", "host"}));
  EXPECT_EQ(result.clock, protocol::clock::Policy_Advance);
  EXPECT_EQ(result.state_root, "/state");
  ASSERT_EQ(result.sessions.size(), 1u);
  EXPECT_EQ(result.sessions[0].sketch, "ember");
  // Every client attached, the one asking among them, in the order they
  // attached.
  EXPECT_EQ(result.attached,
            (std::vector<std::string>{first.session(), second.session()}));
}

TEST(ProtocolDescribe, VersionAndStateRootAnswerTheirPartsAlone) {
  protocol::Dispatcher dispatcher(sketchbook());
  const protocol::InProcess client(dispatcher);
  const protocol::host::HostClient host(client.caller());

  std::optional<std::string> root;
  host.stateRoot([&](Answer<protocol::host::values::StateRootResult> answer) {
    ASSERT_TRUE(answer) << answer.error().message;
    root = answer.result().path;
  });
  EXPECT_EQ(root, "/state");
  std::optional<std::string> program;
  host.version([&](Answer<protocol::host::values::VersionResult> answer) {
    ASSERT_TRUE(answer) << answer.error().message;
    program = answer.result().program;
  });
  EXPECT_EQ(program, "Sketchbook");
}

}  // namespace

namespace {

TEST(ProtocolDescribe, ReadsWhatAnAgentFilledInAfterTheDispatcherWasMade) {
  // An agent mounted after the dispatcher stands knows the clock's policy
  // and the sessions open; it fills them into the program, and describe
  // reads them at the moment it is asked.
  protocol::Dispatcher dispatcher;
  dispatcher.program().clockPolicy = [] {
    return protocol::clock::Policy_Pause;
  };
  const protocol::InProcess client(dispatcher);

  const std::optional<Answer<DescribeResult>> described = describe(client);
  ASSERT_TRUE(described && *described);
  EXPECT_EQ(described->result().clock, protocol::clock::Policy_Pause);
}

}  // namespace
