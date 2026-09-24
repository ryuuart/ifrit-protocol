/** The generated seam, carried end to end with no socket: a wire()
 *  mounts every command an agent answers and nothing the dispatcher
 *  answers, a handler refuses parameters that do not fit before the
 *  agent is asked, an asynchronous command answers when its reply is
 *  called, an event goes out as its table's text, and a generated client
 *  reads a result back as its table, passes an error through, and fails
 *  an answer that is no result.
 */

#include <gtest/gtest.h>
#include <sigilprotocol/clock/ClockAgent.h>
#include <sigilprotocol/clock/ClockClient.h>
#include <sigilprotocol/host/HostAgent.h>
#include <sigilprotocol/host/HostClient.h>
#include <sigilprotocol/registry/RegistryAgent.h>
#include <sigilprotocol/registry/RegistryClient.h>
#include <sigilprotocol/session/SessionAgent.h>
#include <sigilprotocol/session/SessionClient.h>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace protocol = sigil::protocol;
using protocol::Answer;
using protocol::Reply;
using Empty = protocol::values::Empty;

/** A dispatcher at its smallest: handlers filed by method, asked with
 *  text, the answer kept. */
struct Board {
  std::map<std::string, protocol::Handler> handlers;
  void mount(std::string method, protocol::Handler handler) {
    handlers[std::move(method)] = std::move(handler);
  }
  std::set<std::string> methods() const {
    std::set<std::string> out;
    for (const auto& [method, handler] : handlers) out.insert(method);
    return out;
  }
  /** The answer @p method gives @p parameters, or nothing while it has
   *  not answered. */
  std::optional<Answer<std::string>> ask(const std::string& method,
                                         std::string_view parameters) {
    std::optional<Answer<std::string>> answered;
    handlers.at(method)(parameters, [&](Answer<std::string> answer) {
      answered.emplace(std::move(answer));
    });
    return answered;
  }
  /** A caller that speaks to this board as a client would to a host. */
  protocol::Caller caller() {
    protocol::Caller out;
    out.call = [this](std::string method, std::string parameters,
                      protocol::Respond respond) {
      const auto found = handlers.find(method);
      if (found == handlers.end()) {
        respond(protocol::refusal(protocol::ErrorCode_methodNotFound, method));
        return;
      }
      found->second(parameters, std::move(respond));
    };
    return out;
  }
};
static_assert(protocol::Mounts<Board>);

protocol::values::Error declined(const std::string& method) {
  return protocol::refusal(protocol::ErrorCode_failed, method + " declined");
}

/** A clock that records what it was asked and holds a step's reply. */
struct RecordingClock : protocol::clock::ClockAgent {
  std::vector<protocol::clock::values::SetPolicyParameters> policies;
  std::optional<Reply<protocol::clock::values::StepResult>> heldStep;

  Answer<Empty> setPolicy(
      const protocol::clock::values::SetPolicyParameters& parameters) override {
    policies.push_back(parameters);
    return Empty{};
  }
  void step(const protocol::clock::values::StepParameters&,
            Reply<protocol::clock::values::StepResult> reply) override {
    heldStep = std::move(reply);
  }
  Answer<protocol::clock::values::CurrentResult> current() override {
    protocol::clock::values::CurrentResult now;
    now.policy = protocol::clock::Policy_Advance;
    now.seconds = 2.0;
    now.frame = 120;
    return now;
  }
  Answer<Empty> pause(const protocol::clock::values::PauseParameters&) override {
    return declined("clock.pause");
  }
  Answer<Empty> setTimeScale(
      const protocol::clock::values::TimeScaleParameters&) override {
    return declined("clock.setTimeScale");
  }
};

/** Every other domain's agent, declining everything: what a wire()
 *  mounts is the agent's command list, whatever the agent answers. */
struct DecliningHost : protocol::host::HostAgent {
  Answer<protocol::host::values::DescribeResult> describe() override {
    return declined("host.describe");
  }
  Answer<protocol::host::values::StateRootResult> stateRoot() override {
    return declined("host.stateRoot");
  }
  Answer<protocol::host::values::VersionResult> version() override {
    return declined("host.version");
  }
};

struct DecliningSession : protocol::session::SessionAgent {
  void open(const protocol::session::values::OpenParameters&,
            Reply<protocol::session::values::Summary> reply) override {
    reply(declined("session.open"));
  }
  Answer<Empty> pinDevice(
      const protocol::session::values::DeviceParameters&) override {
    return declined("session.pinDevice");
  }
  Answer<Empty> pinPromotion(
      const protocol::session::values::PromotionParameters&) override {
    return declined("session.pinPromotion");
  }
  void still(const protocol::session::values::StillParameters&,
             Reply<protocol::session::values::StillResult> reply) override {
    reply(declined("session.still"));
  }
  void sequence(const protocol::session::values::SequenceParameters&,
                Reply<protocol::session::values::SequenceResult> reply) override {
    reply(declined("session.sequence"));
  }
  Answer<protocol::session::values::TimingResult> timing() override {
    return declined("session.timing");
  }
  Answer<protocol::session::values::MeasuredResult> measured() override {
    return declined("session.measured");
  }
  void compositeCounts(
      Reply<protocol::session::values::CompositeCountsResult> reply) override {
    reply(declined("session.compositeCounts"));
  }
};

struct DecliningRegistry : protocol::registry::RegistryAgent {
  Answer<protocol::registry::values::ListResult> list(
      const protocol::registry::values::ListParameters&) override {
    return declined("registry.list");
  }
  Answer<protocol::registry::values::CatalogResult> catalog(
      const protocol::registry::values::CatalogParameters&) override {
    return declined("registry.catalog");
  }
};

TEST(ProtocolWiring, EachDomainMountsTheCommandsItsAgentAnswers) {
  Board board;
  RecordingClock clock;
  DecliningHost host;
  DecliningSession session;
  DecliningRegistry registry;
  protocol::clock::wire(board, clock);
  protocol::host::wire(board, host);
  protocol::session::wire(board, session);
  protocol::registry::wire(board, registry);
  // enable and disable are the dispatcher's, so no wire() mounts them.
  EXPECT_EQ((std::set<std::string>{
                "clock.current", "clock.pause", "clock.setPolicy",
                "clock.setTimeScale", "clock.step", "host.describe",
                "host.stateRoot", "host.version", "registry.catalog",
                "registry.list", "session.compositeCounts", "session.measured",
                "session.open", "session.pinDevice", "session.pinPromotion",
                "session.sequence", "session.still", "session.timing"}),
            board.methods());
}

TEST(ProtocolWiring, AHandlerReadsTheParametersAndAnswersTheResultAsText) {
  Board board;
  RecordingClock clock;
  protocol::clock::wire(board, clock);

  const std::optional<Answer<std::string>> answer = board.ask(
      "clock.setPolicy", R"({"policy": "Advance", "budget_seconds": 2})");
  ASSERT_TRUE(answer);
  ASSERT_TRUE(*answer) << answer->error().message;
  EXPECT_EQ("{}", answer->result());
  ASSERT_EQ(1u, clock.policies.size());
  EXPECT_EQ(protocol::clock::Policy_Advance, clock.policies[0].policy);
  EXPECT_EQ(2.0, clock.policies[0].budget_seconds);

  // A command that takes nothing takes no text at all as well as "{}".
  const std::optional<Answer<std::string>> now = board.ask("clock.current", "");
  ASSERT_TRUE(now && *now);
  EXPECT_NE(std::string::npos, now->result().find(R"("frame": 120)"));
}

TEST(ProtocolWiring, ParametersThatDoNotFitNeverReachTheAgent) {
  Board board;
  RecordingClock clock;
  protocol::clock::wire(board, clock);

  const std::optional<Answer<std::string>> answer =
      board.ask("clock.setPolicy", R"({"policy": "Sideways"})");
  ASSERT_TRUE(answer);
  ASSERT_FALSE(*answer);
  EXPECT_EQ(protocol::ErrorCode_invalidParameters, answer->error().code);
  EXPECT_NE(std::string::npos, answer->error().message.find("clock.setPolicy"));
  EXPECT_TRUE(clock.policies.empty());

  // An agent's own refusal comes back as it was made.
  const std::optional<Answer<std::string>> declinedPause =
      board.ask("clock.pause", "{}");
  ASSERT_TRUE(declinedPause && !*declinedPause);
  EXPECT_EQ(protocol::ErrorCode_failed, declinedPause->error().code);
}

TEST(ProtocolWiring, AnAsynchronousCommandAnswersWhenItsReplyIsCalled) {
  Board board;
  RecordingClock clock;
  protocol::clock::wire(board, clock);

  std::optional<Answer<std::string>> answered;
  board.handlers.at("clock.step")(R"({"frames": 3})",
                                  [&](Answer<std::string> answer) {
                                    answered.emplace(std::move(answer));
                                  });
  EXPECT_FALSE(answered);
  ASSERT_TRUE(clock.heldStep);

  protocol::clock::values::StepResult done;
  done.seconds = 0.05;
  done.frame = 3;
  (*clock.heldStep)(done);
  ASSERT_TRUE(answered && *answered);
  EXPECT_NE(std::string::npos, answered->result().find(R"("frame": 3)"));
}

TEST(ProtocolWiring, AnEventGoesOutAsItsTablesText) {
  std::vector<std::pair<std::string, std::string>> sent;
  const protocol::clock::ClockEvents events(
      [&](std::string_view method, std::string parameters) {
        sent.emplace_back(std::string(method), std::move(parameters));
      });
  protocol::clock::values::BudgetExpiredEvent expired;
  expired.seconds = 4.0;
  EXPECT_TRUE(events.budgetExpired(expired));
  ASSERT_EQ(1u, sent.size());
  EXPECT_EQ("clock.budgetExpired", sent[0].first);
  EXPECT_EQ(R"({"seconds": 4.0})", sent[0].second);

  // With nowhere to send it, nothing is sent and the emitter says so.
  EXPECT_FALSE(protocol::clock::ClockEvents(nullptr).budgetExpired(expired));
}

TEST(ProtocolWiring, AClientReadsTheResultBackAsItsTable) {
  Board board;
  RecordingClock clock;
  protocol::clock::wire(board, clock);
  const protocol::clock::ClockClient client(board.caller());

  std::optional<Answer<protocol::clock::values::CurrentResult>> now;
  client.current([&](auto answer) { now.emplace(std::move(answer)); });
  ASSERT_TRUE(now && *now);
  EXPECT_EQ(120u, now->result().frame);
  EXPECT_EQ(protocol::clock::Policy_Advance, now->result().policy);

  protocol::clock::values::SetPolicyParameters policy;
  policy.policy = protocol::clock::Policy_Pause;
  std::optional<Answer<Empty>> set;
  client.setPolicy(policy, [&](auto answer) { set.emplace(std::move(answer)); });
  ASSERT_TRUE(set && *set);
  ASSERT_EQ(1u, clock.policies.size());
  EXPECT_EQ(protocol::clock::Policy_Pause, clock.policies[0].policy);

  // A command the host did not mount answers its seam's refusal, named.
  std::optional<Answer<Empty>> enabled;
  client.enable([&](auto answer) { enabled.emplace(std::move(answer)); });
  ASSERT_TRUE(enabled && !*enabled);
  EXPECT_EQ(protocol::ErrorCode_methodNotFound, enabled->error().code);
  EXPECT_EQ("clock.enable", enabled->error().message);
}

TEST(ProtocolWiring, AnAnswerThatIsNoResultIsFailedNamingTheMethod) {
  protocol::Caller caller;
  caller.call = [](std::string, std::string, protocol::Respond respond) {
    respond(std::string(R"({"no_such_field": 1})"));
  };
  const protocol::host::HostClient client(caller);
  std::optional<Answer<protocol::host::values::VersionResult>> version;
  client.version([&](auto answer) { version.emplace(std::move(answer)); });
  ASSERT_TRUE(version && !*version);
  EXPECT_EQ(protocol::ErrorCode_failed, version->error().code);
  EXPECT_NE(std::string::npos, version->error().message.find("host.version"));
}

TEST(ProtocolWiring, AClientHearsAnEventAsItsTable) {
  std::map<std::string, std::function<void(std::string_view)>> listeners;
  protocol::Caller caller;
  caller.listen = [&](std::string method,
                      std::function<void(std::string_view)> hear) {
    listeners[std::move(method)] = std::move(hear);
  };
  const protocol::registry::RegistryClient client(caller);
  std::vector<std::string> heard;
  client.onChanged([&](const protocol::registry::values::ChangedEvent& event) {
    heard = event.names;
  });
  ASSERT_EQ(1u, listeners.count("registry.changed"));
  listeners["registry.changed"](R"({"names": ["hello", "cascade"]})");
  EXPECT_EQ((std::vector<std::string>{"hello", "cascade"}), heard);
  // Text that is no ChangedEvent is no event.
  heard.clear();
  listeners["registry.changed"](R"({"names": 3})");
  EXPECT_TRUE(heard.empty());
}

}  // namespace
