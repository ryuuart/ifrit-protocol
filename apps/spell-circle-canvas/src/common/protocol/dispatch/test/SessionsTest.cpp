/** @file
 * One session per client: what a client alone set goes when it detaches
 * and the next command is answered as if it never had; an answer owed to
 * a client that has gone reaches nobody; and a client outliving its
 * dispatcher, one that detached itself and one moved from are each
 * refused in its own words, saying which.
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
  protocol::Dispatcher dispatcher;
  EXPECT_TRUE(dispatcher.sessions().empty());
  const protocol::InProcess first(dispatcher);
  const protocol::InProcess second(dispatcher);
  EXPECT_NE(first.session(), second.session());
  EXPECT_EQ(dispatcher.sessions(),
            (std::vector<std::string>{first.session(), second.session()}));
  // Outside a handler no session is being answered.
  EXPECT_TRUE(dispatcher.asking().empty());
}

TEST(ProtocolSessions, WhatADetachingClientSetGoesWithIt) {
  protocol::Dispatcher dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  protocol::InProcess setting(dispatcher);
  const protocol::InProcess reading(dispatcher);
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
  protocol::Dispatcher dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  protocol::InProcess leaving(dispatcher);

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
  auto dispatcher = std::make_unique<protocol::Dispatcher>();
  const protocol::InProcess client(*dispatcher);
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
  EXPECT_NE(refused->message.find("the host is closing"), std::string::npos)
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

TEST(ProtocolSessions, AClientThatDetachedItselfSaysSo) {
  protocol::Dispatcher dispatcher;
  protocol::InProcess client(dispatcher);
  client.detach();
  EXPECT_TRUE(dispatcher.sessions().empty());

  std::optional<sigil::data::Json> answered;
  client.send(R"({"id": 6, "method": "host.describe"})", [&](std::string text) {
    answered = sigil::data::decodeJson(text);
  });
  ASSERT_TRUE(answered);
  EXPECT_EQ((*answered)["error"]["code"].text(), "notSent");
  // Its dispatcher still stands; it is the client that went.
  EXPECT_NE((*answered)["error"]["message"].text().find("has detached"),
            std::string::npos)
      << (*answered)["error"]["message"].text();
}

TEST(ProtocolSessions, AClientMovedFromIsAnsweredAndSaysSo) {
  protocol::Dispatcher dispatcher;
  protocol::InProcess moved(dispatcher);
  const protocol::InProcess kept = std::move(moved);
  EXPECT_EQ(dispatcher.sessions(), std::vector<std::string>{kept.session()});

  // What was moved from answers every envelope, rather than leaving the
  // one listening for it waiting.
  int heard = 0;
  std::optional<sigil::data::Json> answered;
  // The case is what a client moved from does, so it is used after the
  // move on purpose.
  // NOLINTNEXTLINE(bugprone-use-after-move)
  moved.send(R"({"id": 7, "method": "host.describe"})",
             [&](std::string text) {
               ++heard;
               answered = sigil::data::decodeJson(text);
             });
  EXPECT_EQ(heard, 1);
  ASSERT_TRUE(answered);
  EXPECT_EQ((*answered)["id"].number(), 7);
  EXPECT_EQ((*answered)["error"]["code"].text(), "notSent");
  EXPECT_NE((*answered)["error"]["message"].text().find("moved"),
            std::string::npos)
      << (*answered)["error"]["message"].text();
}

}  // namespace
