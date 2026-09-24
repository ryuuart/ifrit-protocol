/** @file
 * Events reach a client only between its enable of their domain and its
 * disable — and a client that never enabled them hears none, however it
 * listens.
 */

#include <gtest/gtest.h>
#include <sigilprotocol/clock/ClockClient.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/host/HostClient.h>

#include <memory>
#include <string>
#include <vector>

#include "ClockUnderTest.h"

namespace {

namespace protocol = sigil::protocol;
using protocol::Answer;
using protocol::values::Empty;

/** Sends enable or disable through @p client and says whether it was
 *  answered with nothing wrong. */
bool toggle(const protocol::clock::ClockClient& client, bool enable) {
  bool done = false;
  const auto answered = [&](Answer<Empty> answer) { done = bool(answer); };
  if (enable)
    client.enable(answered);
  else
    client.disable(answered);
  return done;
}

protocol::clock::values::BudgetExpiredEvent at(double seconds) {
  protocol::clock::values::BudgetExpiredEvent event;
  event.seconds = seconds;
  return event;
}

TEST(ProtocolEvents, ReachAClientOnlyBetweenItsEnableAndItsDisable) {
  protocol::Dispatcher dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  const protocol::clock::ClockEvents events(dispatcher.events());
  const protocol::InProcess watching(dispatcher);
  const protocol::InProcess listening(dispatcher);
  const protocol::clock::ClockClient watcher(watching.caller());
  const protocol::clock::ClockClient listener(listening.caller());

  std::vector<double> watched;
  std::vector<double> listened;
  watcher.onBudgetExpired(
      [&](const auto& event) { watched.push_back(event.seconds); });
  listener.onBudgetExpired(
      [&](const auto& event) { listened.push_back(event.seconds); });

  // Before any enable: nobody hears it.
  EXPECT_TRUE(events.budgetExpired(at(1)));
  EXPECT_TRUE(watched.empty());

  ASSERT_TRUE(toggle(watcher, true));
  EXPECT_TRUE(dispatcher.enabled(watching.session(), "clock"));
  EXPECT_TRUE(events.budgetExpired(at(2)));
  // The one that enabled hears it, and the one that only listens does
  // not.
  EXPECT_EQ(watched, std::vector<double>{2});
  EXPECT_TRUE(listened.empty());

  ASSERT_TRUE(toggle(watcher, false));
  EXPECT_TRUE(events.budgetExpired(at(3)));
  EXPECT_EQ(watched, std::vector<double>{2});
  EXPECT_TRUE(listened.empty());
}

TEST(ProtocolEvents, EnablingOneDomainOpensNoOther) {
  protocol::Dispatcher dispatcher;
  protocol::test::ClockUnderTest clock(dispatcher);
  const protocol::InProcess client(dispatcher);
  const protocol::host::HostClient host(client.caller());
  host.enable([](Answer<Empty> answer) { ASSERT_TRUE(answer); });

  EXPECT_TRUE(dispatcher.enabled(client.session(), "host"));
  EXPECT_FALSE(dispatcher.enabled(client.session(), "clock"));
  std::vector<double> heard;
  protocol::clock::ClockClient(client.caller())
      .onBudgetExpired(
          [&](const auto& event) { heard.push_back(event.seconds); });
  EXPECT_TRUE(
      protocol::clock::ClockEvents(dispatcher.events()).budgetExpired(at(1)));
  EXPECT_TRUE(heard.empty());
}

TEST(ProtocolEvents, TheHostLettingAClientGoTellsItWhyIfItAsked) {
  auto dispatcher = std::make_unique<protocol::Dispatcher>();
  const protocol::InProcess told(*dispatcher);
  const protocol::InProcess untold(*dispatcher);
  std::vector<std::string> reasons;
  const protocol::host::HostClient host(told.caller());
  host.onDetached([&](const auto& event) { reasons.push_back(event.reason); });
  host.enable([](Answer<Empty> answer) { ASSERT_TRUE(answer); });
  std::vector<std::string> unheard;
  protocol::host::HostClient(untold.caller())
      .onDetached([&](const auto& event) { unheard.push_back(event.reason); });

  dispatcher.reset();
  EXPECT_EQ(reasons, std::vector<std::string>{"the host is closing"});
  EXPECT_TRUE(unheard.empty());
}

}  // namespace
