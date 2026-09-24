/** @file
 * What the dispatcher refuses, over the envelope a socket carries, in
 * the order it asks: a method the definition does not declare
 * (methodNotFound) apart from a declared one no agent is mounted for
 * (notMounted), each naming the method whatever its parameters say;
 * parameters their table cannot hold (invalidParameters), naming the
 * parameter, the agent never asked; a message that is no
 * request (invalidRequest); and a reply an agent let go unanswered. And
 * what an answer carries: the request's id and the client's session.
 */

#include <gtest/gtest.h>
#include <sigildata/decode/Json.h>
#include <sigilprotocol/dispatch/InProcess.h>

#include <optional>
#include <string>
#include <string_view>

#include "ClockUnderTest.h"

namespace {

namespace protocol = sigil::protocol;
using sigil::data::Json;

/** The answer @p client is sent for @p envelope, read back as JSON;
 *  nothing where none came. */
std::optional<Json> ask(const protocol::InProcess& client,
                        std::string_view envelope) {
  std::optional<Json> answered;
  client.send(envelope, [&](std::string answer) {
    answered = sigil::data::decodeJson(answer);
  });
  return answered;
}

/** Whether @p text holds @p part. */
bool holds(std::string_view text, std::string_view part) {
  return text.find(part) != std::string_view::npos;
}

TEST(ProtocolRefusals, AnAnswerCarriesTheRequestsIdAndTheClientsSession) {
  protocol::Dispatcher dispatcher;
  const protocol::InProcess client(dispatcher);

  const std::optional<Json> answer =
      ask(client, R"({"id": "first", "method": "host.version"})");
  ASSERT_TRUE(answer);
  EXPECT_EQ((*answer)["id"].text(), "first");
  EXPECT_EQ((*answer)["session"].text(), client.session());
  EXPECT_TRUE((*answer)["error"].null());
  EXPECT_EQ((*answer)["result"].kind(), Json::Kind::Record);
}

TEST(ProtocolRefusals, AMethodTheDefinitionDoesNotDeclareIsNotFound) {
  protocol::Dispatcher dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  const protocol::InProcess client(dispatcher);

  const std::optional<Json> answer =
      ask(client, R"({"id": 1, "method": "clock.warp"})");
  ASSERT_TRUE(answer);
  EXPECT_EQ((*answer)["id"].number(), 1);
  EXPECT_EQ((*answer)["error"]["code"].text(), "methodNotFound");
  EXPECT_TRUE(holds((*answer)["error"]["message"].text(), "clock.warp"))
      << (*answer)["error"]["message"].text();
  // The seam that refused opens its own words.
  EXPECT_TRUE((*answer)["error"]["message"].text().starts_with("dispatcher: "));
}

TEST(ProtocolRefusals, ADeclaredCommandNoAgentAnswersIsNotMounted) {
  // No clock agent: the definition declares clock.current, and this host
  // mounts nothing for the domain. That is not a method it has never
  // heard of, and the two answer apart.
  protocol::Dispatcher dispatcher;
  const protocol::InProcess client(dispatcher);

  for (const std::string_view method : {"clock.current", "clock.enable"}) {
    const std::optional<Json> answer =
        ask(client, R"({"id": 2, "method": ")" + std::string(method) + "\"}");
    ASSERT_TRUE(answer) << method;
    EXPECT_EQ((*answer)["error"]["code"].text(), "notMounted") << method;
    EXPECT_TRUE(holds((*answer)["error"]["message"].text(), method))
        << (*answer)["error"]["message"].text();
  }
}

TEST(ProtocolRefusals, BadParametersToADomainNoAgentAnswersAreNotMounted) {
  // Whether an agent is there comes before what it would be asked with:
  // parameters no table holds, sent to a command this host never wired,
  // are answered notMounted, and so is an enable carrying them.
  protocol::Dispatcher dispatcher;
  const protocol::InProcess client(dispatcher);

  for (const std::string_view method : {"clock.step", "clock.enable"}) {
    const std::optional<Json> answer =
        ask(client, R"({"id": 12, "method": ")" + std::string(method) +
                        R"(", "parameters": {"frames": "many"}})");
    ASSERT_TRUE(answer) << method;
    EXPECT_EQ((*answer)["error"]["code"].text(), "notMounted") << method;
    EXPECT_TRUE(holds((*answer)["error"]["message"].text(), method))
        << (*answer)["error"]["message"].text();
  }
}

TEST(ProtocolRefusals, ParametersTheTableCannotHoldNeverReachTheAgent) {
  protocol::Dispatcher dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  const protocol::InProcess client(dispatcher);

  // A value of the wrong type, and a member the table does not declare:
  // each answer names the parameter it stopped at.
  const std::optional<Json> mistyped = ask(
      client,
      R"({"id": 3, "method": "clock.step", "parameters": {"frames": "many"}})");
  ASSERT_TRUE(mistyped);
  EXPECT_EQ((*mistyped)["error"]["code"].text(), "invalidParameters");
  EXPECT_TRUE(holds((*mistyped)["error"]["message"].text(), "frames"))
      << (*mistyped)["error"]["message"].text();
  const std::optional<Json> unknown =
      ask(client,
          R"({"id": 4, "method": "clock.step", "parameters": {"framez": 2}})");
  ASSERT_TRUE(unknown);
  EXPECT_EQ((*unknown)["error"]["code"].text(), "invalidParameters");
  EXPECT_TRUE(holds((*unknown)["error"]["message"].text(), "framez"))
      << (*unknown)["error"]["message"].text();
  // A whole number its type cannot hold, and a name no value of the
  // enumeration carries, are refused the same way.
  const std::optional<Json> negative = ask(
      client,
      R"({"id": 10, "method": "clock.step", "parameters": {"frames": -1}})");
  ASSERT_TRUE(negative);
  EXPECT_EQ((*negative)["error"]["code"].text(), "invalidParameters");
  EXPECT_TRUE(holds((*negative)["error"]["message"].text(), "frames"))
      << (*negative)["error"]["message"].text();
  EXPECT_EQ(clock.steps, 0);
  const std::optional<Json> sideways = ask(
      client,
      R"({"id": 11, "method": "clock.setPolicy", "parameters": {"policy": "Sideways"}})");
  ASSERT_TRUE(sideways);
  EXPECT_EQ((*sideways)["error"]["code"].text(), "invalidParameters");
  EXPECT_TRUE(holds((*sideways)["error"]["message"].text(), "policy"))
      << (*sideways)["error"]["message"].text();
  EXPECT_TRUE(holds((*sideways)["error"]["message"].text(), "Sideways"))
      << (*sideways)["error"]["message"].text();
  // An enumeration given by a number it does not declare is refused as a
  // name it does not carry is, though the number fits its type.
  const std::optional<Json> seventh = ask(
      client,
      R"({"id": 13, "method": "clock.setPolicy", "parameters": {"policy": 7}})");
  ASSERT_TRUE(seventh);
  EXPECT_EQ((*seventh)["error"]["code"].text(), "invalidParameters");
  EXPECT_TRUE(holds((*seventh)["error"]["message"].text(), "policy"))
      << (*seventh)["error"]["message"].text();

  // The same command, with parameters that fit, does reach it.
  EXPECT_FALSE(
      ask(client,
          R"({"id": 5, "method": "clock.step", "parameters": {"frames": 2}})"));
  EXPECT_EQ(clock.steps, 1);
}

TEST(ProtocolRefusals, AMessageThatIsNoRequestIsAnInvalidRequest) {
  protocol::Dispatcher dispatcher;
  const protocol::InProcess client(dispatcher);

  const std::optional<Json> notJson = ask(client, "a scene arrives");
  ASSERT_TRUE(notJson);
  EXPECT_EQ((*notJson)["error"]["code"].text(), "invalidRequest");
  EXPECT_TRUE((*notJson)["id"].null());

  const std::optional<Json> noId =
      ask(client, R"({"method": "host.describe"})");
  ASSERT_TRUE(noId);
  EXPECT_EQ((*noId)["error"]["code"].text(), "invalidRequest");
  // It names the method the message did carry.
  EXPECT_TRUE(holds((*noId)["error"]["message"].text(), "host.describe"))
      << (*noId)["error"]["message"].text();

  const std::optional<Json> noMethod = ask(client, R"({"id": 6})");
  ASSERT_TRUE(noMethod);
  EXPECT_EQ((*noMethod)["id"].number(), 6);
  EXPECT_EQ((*noMethod)["error"]["code"].text(), "invalidRequest");

  const std::optional<Json> another =
      ask(client,
          R"({"id": 7, "session": "session-99", "method": "host.describe"})");
  ASSERT_TRUE(another);
  EXPECT_EQ((*another)["error"]["code"].text(), "invalidRequest");
  EXPECT_TRUE(holds((*another)["error"]["message"].text(), "session-99"))
      << (*another)["error"]["message"].text();
}

TEST(ProtocolRefusals, AReplyLetGoWithoutAnsweringIsAnsweredFailed) {
  protocol::Dispatcher dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  clock.dropSteps = true;
  const protocol::InProcess client(dispatcher);

  const std::optional<Json> answer =
      ask(client, R"({"id": 8, "method": "clock.step"})");
  ASSERT_TRUE(answer);
  EXPECT_EQ((*answer)["error"]["code"].text(), "failed");
  EXPECT_TRUE(holds((*answer)["error"]["message"].text(), "clock.step"))
      << (*answer)["error"]["message"].text();
}

TEST(ProtocolRefusals, AReplyIsAnsweredOnceWhenTheAgentCallsIt) {
  protocol::Dispatcher dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  const protocol::InProcess client(dispatcher);

  int heard = 0;
  std::optional<Json> answered;
  client.send(R"({"id": 9, "method": "clock.step"})", [&](std::string text) {
    ++heard;
    answered = sigil::data::decodeJson(text);
  });
  EXPECT_EQ(heard, 0);
  ASSERT_TRUE(clock.heldStep);
  protocol::clock::values::StepResult stepped;
  stepped.frame = 3;
  (*clock.heldStep)(stepped);
  (*clock.heldStep)(stepped);
  EXPECT_EQ(heard, 1);
  ASSERT_TRUE(answered);
  EXPECT_EQ((*answered)["result"]["frame"].number(), 3);
}

}  // namespace
