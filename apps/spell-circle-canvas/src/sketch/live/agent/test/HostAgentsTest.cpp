/** @file
 * The session and clock domains' self-check, in process: every command
 * each agent answers, asked through the generated clients over one
 * dispatcher, with the refusals each owes — and the clock's claims, each
 * a case: a session is opened for its clock, a step moves it only under
 * Advance, two stills a wall-second apart under Pause are one picture,
 * a budget runs out once and says so to a client that enabled the
 * clock, and what a client set goes when it detaches.
 */

#include <gtest/gtest.h>
#include <sigilcompose/core/Core.h>
#include <sigilprotocol/clock/ClockClient.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/host/HostClient.h>
#include <sigilprotocol/session/SessionClient.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/live/agent/HostAgents.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "ScratchDir.h"
#include "support/Fixtures.h"

namespace {

namespace protocol = sigil::protocol;
namespace session = protocol::session;
namespace clock = protocol::clock;
using protocol::Answer;
using sigil::test::ScratchDir;
using namespace std::chrono_literals;

/** A box marching right a canvas unit every sixtieth of a second, so
 *  any two moments of it are two pictures. */
struct MarchingBox {
  void setup(sigil::sketch::SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.5);
  }
  void update(double elapsed, sigil::sketch::SketchContext& ctx) {
    using namespace sigil::compose;
    ctx.composer.render(box()
                            .width(10)
                            .height(10)
                            .inset(0, 0, 0, (float)elapsed * 60.0f)
                            .fill(Fill::color({1, 0, 0, 1})));
  }
};

[[maybe_unused]] const bool kRegistered = sigil::sketch::add(
    "agents_marching_box", nullptr, "Test", "a box that moves every frame",
    &sigil::sketch::kindOf<MarchingBox>);

std::string bytesOf(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/** How many hosts this process has made, so each keeps a state root of
 *  its own. */
int hostsMade = 0;

/** A host of the three agents over one dispatcher, a client attached in
 *  process, and every answer waited for by turning the host's loop. */
struct AgentHost {
  ScratchDir scratch{"sketch-host-agents-" + std::to_string(++hostsMade)};
  protocol::Dispatcher dispatcher{[this] {
    protocol::Program program;
    program.name = "HostAgentsTest";
    program.stateRoot = scratch.path;
    return program;
  }()};
  sigil::sketch::HostAgents agents{dispatcher,
                                   sigil::sketch::SessionAgentOptions{
                                       .fonts = &sigil::sketch::test::fonts()}};
  protocol::InProcess client{dispatcher};

  /** Turns the loop until @p answered holds, or gives up. */
  template <class Result>
  Answer<Result> wait(std::optional<Answer<Result>>& answered) {
    for (int turn = 0; !answered && turn < 2000; ++turn) {
      agents.frame();
      if (!answered) std::this_thread::sleep_for(1ms);
    }
    if (!answered)
      return protocol::refusal(protocol::ErrorCode_failed, "never answered");
    return std::move(*answered);
  }

  template <class Result>
  static protocol::Reply<Result> into(std::optional<Answer<Result>>& slot) {
    return [&slot](Answer<Result> answer) { slot.emplace(std::move(answer)); };
  }

  Answer<session::values::Summary> open(const std::string& sketch) {
    std::optional<Answer<session::values::Summary>> answered;
    session::values::OpenParameters parameters;
    parameters.sketch = sketch;
    session::SessionClient(client.caller()).open(parameters, into(answered));
    return wait(answered);
  }

  Answer<protocol::values::Empty> policy(clock::Policy policy,
                                         std::optional<double> budget = {}) {
    std::optional<Answer<protocol::values::Empty>> answered;
    clock::values::SetPolicyParameters parameters;
    parameters.policy = policy;
    parameters.budget_seconds = budget;
    clock::ClockClient(client.caller()).setPolicy(parameters, into(answered));
    return wait(answered);
  }

  Answer<clock::values::StepResult> step(double seconds) {
    std::optional<Answer<clock::values::StepResult>> answered;
    clock::values::StepParameters parameters;
    parameters.seconds = seconds;
    clock::ClockClient(client.caller()).step(parameters, into(answered));
    return wait(answered);
  }

  Answer<clock::values::CurrentResult> current() {
    std::optional<Answer<clock::values::CurrentResult>> answered;
    clock::ClockClient(client.caller()).current(into(answered));
    return wait(answered);
  }

  Answer<session::values::StillResult> still(double density = 1,
                                             std::string path = {}) {
    std::optional<Answer<session::values::StillResult>> answered;
    session::values::StillParameters parameters;
    parameters.density = density;
    parameters.path = std::move(path);
    session::SessionClient(client.caller()).still(parameters, into(answered));
    return wait(answered);
  }
};

TEST(SketchSessionAgent, OpensARegistrySketchAndDescribeListsItsSession) {
  AgentHost host;
  const Answer<session::values::Summary> opened =
      host.open("agents_marching_box");
  ASSERT_TRUE(opened) << opened.error().message;
  EXPECT_EQ(opened.result().kind, "canvas");
  EXPECT_EQ(opened.result().width, 64.0f);
  EXPECT_EQ(opened.result().height, 48.0f);
  EXPECT_EQ(opened.result().moment, 0.5);

  std::optional<Answer<protocol::host::values::DescribeResult>> described;
  protocol::host::HostClient(host.client.caller())
      .describe(AgentHost::into(described));
  ASSERT_TRUE(described && *described);
  EXPECT_EQ(described->result().domains,
            (std::vector<std::string>{"clock", "host", "registry", "session"}));
  ASSERT_EQ(described->result().sessions.size(), 1u);
  EXPECT_EQ(described->result().sessions[0].sketch, "agents_marching_box");
  EXPECT_EQ(described->result().clock, clock::Policy_Wall);
  EXPECT_EQ(described->result().state_root, host.scratch.path.string());
}

TEST(SketchSessionAgent, RefusesASketchNothingAnswersTo) {
  AgentHost host;
  const Answer<session::values::Summary> opened =
      host.open("no_such_sketch_anywhere");
  ASSERT_FALSE(opened);
  EXPECT_EQ(opened.error().code, protocol::ErrorCode_failed);
  EXPECT_NE(opened.error().message.find("no_such_sketch_anywhere"),
            std::string::npos);
}

TEST(SketchSessionAgent, OpensUnderTheWallWithAFrameAndUnderAdvanceWithNone) {
  AgentHost wall;
  ASSERT_TRUE(wall.open("agents_marching_box"));
  EXPECT_EQ(wall.current().result().frame, 1u);

  AgentHost advance;
  ASSERT_TRUE(advance.policy(clock::Policy_Advance));
  ASSERT_TRUE(advance.open("agents_marching_box"));
  EXPECT_EQ(advance.current().result().frame, 0u);
}

TEST(SketchClockAgent, RefusesAStepUnlessThePolicyIsAdvance) {
  AgentHost host;
  ASSERT_TRUE(host.open("agents_marching_box"));
  const Answer<clock::values::StepResult> stepped = host.step(1.0);
  ASSERT_FALSE(stepped);
  EXPECT_NE(stepped.error().message.find("Advance"), std::string::npos);
}

TEST(SketchClockAgent, StepsSecondsInWholeFramesOfTheRate) {
  AgentHost host;
  ASSERT_TRUE(host.open("agents_marching_box"));
  // Leaving the wall opens the session again, at its own zero.
  ASSERT_TRUE(host.policy(clock::Policy_Advance));
  const Answer<clock::values::StepResult> stepped = host.step(1.0);
  ASSERT_TRUE(stepped) << stepped.error().message;
  EXPECT_EQ(stepped.result().frame, 60u);
  EXPECT_NEAR(stepped.result().seconds, 1.0, 1e-9);
}

TEST(SketchClockAgent, TwoStillsAWallSecondApartUnderPauseAreOnePicture) {
  AgentHost host;
  ASSERT_TRUE(host.policy(clock::Policy_Advance));
  ASSERT_TRUE(host.open("agents_marching_box"));
  ASSERT_TRUE(host.step(0.25));
  ASSERT_TRUE(host.policy(clock::Policy_Pause));
  const Answer<session::values::StillResult> first = host.still(1, "first.png");
  ASSERT_TRUE(first) << first.error().message;
  const auto until = std::chrono::steady_clock::now() + 1s;
  while (std::chrono::steady_clock::now() < until) {
    host.agents.frame();
    std::this_thread::sleep_for(5ms);
  }
  const Answer<session::values::StillResult> second =
      host.still(1, "second.png");
  ASSERT_TRUE(second) << second.error().message;
  EXPECT_EQ(first.result().seconds, second.result().seconds);
  EXPECT_EQ(bytesOf(first.result().path), bytesOf(second.result().path));
  EXPECT_FALSE(bytesOf(first.result().path).empty());
}

TEST(SketchClockAgent, TwoStillsUnderAdvanceAreTwoMoments) {
  // The same pair under a moving clock is two pictures, which is what
  // gives the case above its power.
  AgentHost host;
  ASSERT_TRUE(host.policy(clock::Policy_Advance));
  ASSERT_TRUE(host.open("agents_marching_box"));
  const Answer<session::values::StillResult> first = host.still(1, "a.png");
  ASSERT_TRUE(host.step(0.25));
  const Answer<session::values::StillResult> second = host.still(1, "b.png");
  ASSERT_TRUE(first && second);
  EXPECT_NE(bytesOf(first.result().path), bytesOf(second.result().path));
  // The canvas runtime draws its still one sixtieth on, and the clock
  // counts that frame.
  EXPECT_NEAR(first.result().seconds, 1.0 / 60.0, 1e-9);
}

TEST(SketchClockAgent, ABudgetRunsOutOnceAndTellsAClientThatEnabledTheClock) {
  AgentHost host;
  std::vector<double> expired;
  const clock::ClockClient clockClient(host.client.caller());
  clockClient.onBudgetExpired(
      [&](const clock::values::BudgetExpiredEvent& event) {
        expired.push_back(event.seconds);
      });
  std::optional<Answer<protocol::values::Empty>> enabled;
  clockClient.enable(AgentHost::into(enabled));
  ASSERT_TRUE(enabled && *enabled);
  ASSERT_TRUE(host.policy(clock::Policy_Advance, 0.5));
  ASSERT_TRUE(host.open("agents_marching_box"));
  ASSERT_TRUE(host.step(1.0));
  ASSERT_EQ(expired.size(), 1u);
  EXPECT_NEAR(expired[0], 0.5, 1e-9);
  EXPECT_FALSE(host.current().result().budget_remaining);
}

TEST(SketchClockAgent, WhatADetachingClientSetGoesWithIt) {
  AgentHost host;
  ASSERT_TRUE(host.open("agents_marching_box"));
  {
    protocol::InProcess setter(host.dispatcher);
    std::optional<Answer<protocol::values::Empty>> answered;
    clock::values::SetPolicyParameters parameters;
    parameters.policy = clock::Policy_Advance;
    clock::ClockClient(setter.caller())
        .setPolicy(parameters, AgentHost::into(answered));
    ASSERT_TRUE(answered && *answered);
    EXPECT_EQ(host.current().result().policy, clock::Policy_Advance);
  }
  // The setter has gone: the next frame is the wall's, and the session
  // opened again for it drew its first.
  const Answer<clock::values::CurrentResult> after = host.current();
  EXPECT_EQ(after.result().policy, clock::Policy_Wall);
  EXPECT_EQ(after.result().frame, 1u);
}

/** Whether the last session this probe opened was opened for a
 *  repeatable run. */
bool g_openedRepeatable = false;

/** A sketch that says what it was opened for. */
struct OpenedFor {
  void setup(sigil::sketch::SketchContext& ctx) {
    ctx.canvas(16, 16);
    g_openedRepeatable = ctx.deterministic;
  }
};

[[maybe_unused]] const bool kOpenedForRegistered =
    sigil::sketch::add("agents_opened_for", nullptr, "Test",
                       "a sketch that says what it was opened for",
                       &sigil::sketch::kindOf<OpenedFor>);

TEST(SketchClockAgent,
     PauseWhileLoadingIsARepeatableRunHeldWhileAnythingArrives) {
  const ScratchDir scratch("sketch-host-agents-loading");
  protocol::Program program;
  program.stateRoot = scratch.path;
  protocol::Dispatcher dispatcher(std::move(program));
  bool arriving = true;
  sigil::sketch::HostAgents agents(
      dispatcher, sigil::sketch::SessionAgentOptions{
                      .fonts = &sigil::sketch::test::fonts(),
                      .arriving = [&arriving] { return arriving; }});
  const protocol::InProcess client(dispatcher);
  const auto answer = [](auto& slot) { return AgentHost::into(slot); };

  std::optional<Answer<protocol::values::Empty>> set;
  clock::values::SetPolicyParameters parameters;
  parameters.policy = clock::Policy_PauseWhileLoading;
  clock::ClockClient(client.caller()).setPolicy(parameters, answer(set));
  ASSERT_TRUE(set && *set);
  std::optional<Answer<session::values::Summary>> opened;
  session::values::OpenParameters open;
  open.sketch = "agents_opened_for";
  g_openedRepeatable = false;
  session::SessionClient(client.caller()).open(open, answer(opened));
  ASSERT_TRUE(opened && *opened);
  // A sketch reads only that the clock is not the wall's.
  EXPECT_TRUE(g_openedRepeatable);

  const auto seconds = [&] {
    std::optional<Answer<clock::values::CurrentResult>> current;
    clock::ClockClient(client.caller()).current(answer(current));
    return current->result().seconds;
  };
  for (int turn = 0; turn < 5; ++turn) {
    agents.frame();
    std::this_thread::sleep_for(5ms);
  }
  EXPECT_EQ(seconds(), 0.0);
  arriving = false;
  for (int turn = 0; turn < 5; ++turn) {
    agents.frame();
    std::this_thread::sleep_for(5ms);
  }
  EXPECT_GT(seconds(), 0.0);
}

TEST(SketchSessionAgent, WritesAStillUnderTheStateRootAtItsDensity) {
  AgentHost host;
  ASSERT_TRUE(host.policy(clock::Policy_Advance));
  ASSERT_TRUE(host.open("agents_marching_box"));
  const Answer<session::values::StillResult> still = host.still(2);
  ASSERT_TRUE(still) << still.error().message;
  EXPECT_EQ(still.result().width, 128u);
  EXPECT_EQ(still.result().height, 96u);
  EXPECT_EQ(std::filesystem::path(still.result().path).parent_path(),
            host.scratch.path / "stills");
  EXPECT_EQ(host.agents.session().lastStill(),
            std::filesystem::path(still.result().path));

  const Answer<session::values::StillResult> outside =
      host.still(1, "/tmp/outside-the-root.png");
  ASSERT_FALSE(outside);
  EXPECT_NE(outside.error().message.find("outside the state root"),
            std::string::npos);
}

TEST(SketchSessionAgent, ReadsBackTimingProfileAndCompositeCounts) {
  AgentHost host;
  ASSERT_TRUE(host.policy(clock::Policy_Advance));
  ASSERT_TRUE(host.open("agents_marching_box"));
  const session::SessionClient client(host.client.caller());

  std::optional<Answer<session::values::ProfileResult>> profiled;
  session::values::ProfileParameters limit;
  limit.limit = 4;
  client.profile(limit, AgentHost::into(profiled));
  EXPECT_FALSE(profiled);  // owed by the next frame
  ASSERT_TRUE(host.step(1.0 / 60.0));
  ASSERT_TRUE(profiled && *profiled) << profiled->error().message;
  EXPECT_LE(profiled->result().rows.size(), 4u);

  std::optional<Answer<session::values::TimingResult>> timing;
  client.timing(AgentHost::into(timing));
  ASSERT_TRUE(timing && *timing);
  EXPECT_GE(timing->result().total_milliseconds, 0.0);
  EXPECT_FALSE(timing->result().lanes.empty());

  std::optional<Answer<session::values::CompositeCountsResult>> counts;
  client.compositeCounts(AgentHost::into(counts));
  EXPECT_FALSE(counts);  // owed by the next still
  ASSERT_TRUE(host.still());
  ASSERT_TRUE(counts);
  if (*counts) EXPECT_TRUE(std::filesystem::exists(counts->result().path));
}

TEST(SketchSessionAgent, RefusesWhatThisHostCannotDoNamingWhy) {
  AgentHost host;
  ASSERT_TRUE(host.open("agents_marching_box"));
  const session::SessionClient client(host.client.caller());
  std::optional<Answer<session::values::MeasuredResult>> measured;
  client.measured(AgentHost::into(measured));
  ASSERT_TRUE(measured && !*measured);
  EXPECT_EQ(measured->error().code, protocol::ErrorCode_failed);

  std::optional<Answer<protocol::values::Empty>> device;
  session::values::DeviceParameters gpu;
  gpu.device = session::Device_Gpu;
  client.pinDevice(gpu, AgentHost::into(device));
  ASSERT_TRUE(device && !*device);
  EXPECT_NE(device->error().message.find("CPU"), std::string::npos);
}

}  // namespace
