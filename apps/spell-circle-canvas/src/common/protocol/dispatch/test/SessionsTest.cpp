/** @file
 * One session per client: what a client alone set goes when it detaches
 * and the next command is answered as if it never had; an answer owed to
 * a client that has gone reaches nobody; and a client outliving its
 * dispatcher is refused in its own words.
 */

#include <gtest/gtest.h>
#include <sigildata/decode/Json.h>
#include <sigilprotocol/clock/ClockClient.h>
#include <sigilprotocol/dispatch/InProcess.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ClockUnderTest.h"

namespace {

namespace protocol = sigil::protocol;
using protocol::Answer;

/** The policy @p client reads off the clock now. */
std::optional<protocol::clock::Policy> policyOf(
    const protocol::clock::ClockClient& client) {
  std::optional<protocol::clock::Policy> read;
  client.current([&](Answer<protocol::clock::values::CurrentResult> answer) {
    if (answer) read = answer.result().policy;
  });
  return read;
}

TEST(ProtocolSessions, EachClientIsASessionOfItsOwn) {
  protocol::InProcess dispatcher;
  EXPECT_TRUE(dispatcher.sessions().empty());
  const protocol::InProcess::Client first = dispatcher.connect();
  const protocol::InProcess::Client second = dispatcher.connect();
  EXPECT_NE(first.session(), second.session());
  EXPECT_EQ(dispatcher.sessions(),
            (std::vector<std::string>{first.session(), second.session()}));
  // Outside a handler no session is being answered.
  EXPECT_TRUE(dispatcher.asking().empty());
}

TEST(ProtocolSessions, WhatADetachingClientSetGoesWithIt) {
  protocol::InProcess dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  protocol::InProcess::Client setting = dispatcher.connect();
  const protocol::InProcess::Client reading = dispatcher.connect();
  const protocol::clock::ClockClient setter(setting.caller());
  const protocol::clock::ClockClient reader(reading.caller());

  protocol::clock::values::SetPolicyParameters advance;
  advance.policy = protocol::clock::Policy_Advance;
  setter.setPolicy(advance, [](Answer<protocol::values::Empty> answer) {
    ASSERT_TRUE(answer) << answer.error().message;
  });
  setter.enable([](Answer<protocol::values::Empty> answer) {
    ASSERT_TRUE(answer) << answer.error().message;
  });
  EXPECT_EQ(policyOf(reader), protocol::clock::Policy_Advance);

  const std::string gone = setting.session();
  setting.detach();
  // The next command is answered as if that client had never set it,
  // and it neither stands among the sessions nor has anything enabled.
  EXPECT_EQ(policyOf(reader), protocol::clock::Policy_Wall);
  EXPECT_EQ(dispatcher.sessions(), std::vector<std::string>{reading.session()});
  EXPECT_FALSE(dispatcher.enabled(gone, "clock"));
}

TEST(ProtocolSessions, AnAnswerOwedToAClientThatLeftReachesNobody) {
  protocol::InProcess dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  protocol::InProcess::Client leaving = dispatcher.connect();

  int answered = 0;
  protocol::clock::ClockClient(leaving.caller())
      .step({},
            [&](Answer<protocol::clock::values::StepResult>) { ++answered; });
  ASSERT_TRUE(clock.heldStep);
  leaving.detach();
  (*clock.heldStep)(protocol::clock::values::StepResult{});
  EXPECT_EQ(answered, 0);
}

TEST(ProtocolSessions, AClientOutlivingItsDispatcherSendsNowhere) {
  auto dispatcher = std::make_unique<protocol::InProcess>();
  const protocol::InProcess::Client client = dispatcher->connect();
  const protocol::clock::ClockClient clock(client.caller());
  dispatcher.reset();

  std::optional<protocol::values::Error> refused;
  clock.current([&](Answer<protocol::clock::values::CurrentResult> answer) {
    if (!answer) refused = answer.error();
  });
  ASSERT_TRUE(refused);
  EXPECT_EQ(refused->code, protocol::ErrorCode_notSent);
  EXPECT_TRUE(refused->message.starts_with("client: clock.current: "))
      << refused->message;

  // The envelope is refused the same way, under the request's own id.
  std::optional<sigil::data::Json> answered;
  client.send(R"({"id": 5, "method": "host.describe"})", [&](std::string text) {
    answered = sigil::data::decodeJson(text);
  });
  ASSERT_TRUE(answered);
  EXPECT_EQ((*answered)["id"].number(), 5);
  EXPECT_EQ((*answered)["error"]["code"].text(), "notSent");
  EXPECT_TRUE((*answered)["error"]["message"].text().starts_with(
      "client: host.describe: "))
      << (*answered)["error"]["message"].text();
}

}  // namespace
