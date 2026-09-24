/** The generator's refusals, one case each: a definition of a few lines
 *  that breaks exactly one rule of the definition's shape, parsed here
 *  into its reflected form with the documentation and the builtin marks
 *  kept, as the build's flatc run keeps them, and read by the model the
 *  generator reads. A definition that keeps every rule reads, so each
 *  refusal is the rule's and not the fixture's.
 */

#include <flatbuffers/idl.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ProtocolModel.h"

namespace {

namespace generator = sigil::protocol::generator;

/** The attributes, the shared namespace and its Empty table every
 *  fixture opens with. */
const std::string kPrelude = R"(
attribute "experimental";
attribute "asynchronous";
namespace sigil.protocol;
/// Nothing.
table Empty {}
)";

/** One domain that keeps every rule: a command, an event, and the
 *  enable and disable its events need. */
const std::string kClock = R"(
namespace sigil.protocol.clock;
/// How far to step.
table StepParameters {
  /// Frames.
  frames: uint = 1;
}
/// The budget ran out.
table BudgetExpiredEvent {
  /// When.
  seconds: double;
}
/// The clock.
rpc_service Clock {
  /// Steps.
  step(StepParameters): sigil.protocol.Empty (asynchronous);
  /// Starts the events.
  enable(sigil.protocol.Empty): sigil.protocol.Empty;
  /// Stops them.
  disable(sigil.protocol.Empty): sigil.protocol.Empty;
}
/// The clock's events.
rpc_service ClockEvents {
  /// The budget ran out.
  budgetExpired(sigil.protocol.Empty): BudgetExpiredEvent (streaming: "server");
}
)";

/** What the model reads out of @p definition's reflected form; @p why
 *  holds the refusal where it reads nothing. */
std::optional<generator::Model> read(const std::string& definition,
                                     std::string* why) {
  flatbuffers::IDLOptions options;
  options.binary_schema_comments = true;
  options.binary_schema_builtins = true;
  flatbuffers::Parser parser(options);
  if (!parser.Parse(definition.c_str())) {
    *why = "the fixture does not parse: " + parser.error_;
    return std::nullopt;
  }
  parser.Serialize();
  return generator::readModel(
      std::span<const uint8_t>(parser.builder_.GetBufferPointer(),
                               parser.builder_.GetSize()),
      why);
}

/** The refusal @p definition earns; empty where it reads. */
std::string refusalOf(const std::string& definition) {
  std::string why;
  if (read(definition, &why)) return std::string();
  return why;
}

/** Whether @p text says @p words. */
testing::AssertionResult says(const std::string& text,
                              const std::string& words) {
  if (text.find(words) != std::string::npos) return testing::AssertionSuccess();
  return testing::AssertionFailure()
         << "the refusal \"" << text << "\" does not say \"" << words << "\"";
}

TEST(ProtocolModel, ADefinitionThatKeepsEveryRuleReads) {
  std::string why;
  const std::optional<generator::Model> model = read(kPrelude + kClock, &why);
  ASSERT_TRUE(model) << why;
  ASSERT_EQ(1u, model->domains.size());
  const generator::Domain& clock = model->domains[0];
  EXPECT_EQ("clock", clock.name);
  EXPECT_EQ("Clock", clock.service);
  ASSERT_EQ(3u, clock.commands.size());
  EXPECT_TRUE(clock.commands[0].asynchronous);
  ASSERT_EQ(1u, clock.eventList.size());
  EXPECT_EQ("clock.budgetExpired", clock.eventList[0].method);
  EXPECT_EQ("sigil.protocol.Empty", model->empty);
  EXPECT_TRUE(generator::domainsAre(*model, {"clock"}, &why)) << why;
}

TEST(ProtocolModel, APartWithNoDocumentationIsRefused) {
  std::string clock = kClock;
  clock.replace(clock.find("  /// Frames.\n"), 14, "");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the field sigil.protocol.clock.StepParameters.frames has"
                   " no documentation"));

  clock = kClock;
  clock.replace(clock.find("  /// Steps.\n"), 13, "");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the command clock.step has no documentation"));

  clock = kClock;
  clock.replace(clock.find("/// The clock.\n"), 15, "");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the domain sigil.protocol.clock.Clock has no"
                   " documentation"));
}

TEST(ProtocolModel, AUnionIsRefused) {
  EXPECT_TRUE(says(refusalOf(kPrelude + R"(
/// Either.
union Either { Empty }
)" + kClock),
                   "the union sigil.protocol.Either has no place"));
}

TEST(ProtocolModel, AStructIsRefused) {
  EXPECT_TRUE(says(refusalOf(kPrelude + R"(
/// A point.
struct Point {
  /// Across.
  x: float;
}
)" + kClock),
                   "the struct sigil.protocol.Point has no place"));
}

TEST(ProtocolModel, ADefaultThatIsNoFiniteNumberIsRefused) {
  std::string clock = kClock;
  clock.replace(clock.find("frames: uint = 1;"), 17, "frames: double = inf;");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the field frames declares a default that is no finite"
                   " number"));
}

TEST(ProtocolModel, ADefinitionWithNoEmptyTableIsRefused) {
  std::string prelude = kPrelude;
  prelude.replace(prelude.find("table Empty {}"), 14, "table Nothing {}");
  std::string clock = kClock;
  for (size_t at;
       (at = clock.find("sigil.protocol.Empty")) != std::string::npos;)
    clock.replace(at, 20, "sigil.protocol.Nothing");
  EXPECT_TRUE(says(refusalOf(prelude + clock),
                   "declares no table Empty with no fields"));
}

TEST(ProtocolModel, ADefinitionWithNoServicesIsRefused) {
  EXPECT_TRUE(says(refusalOf(kPrelude), "carries no services"));
}

TEST(ProtocolModel, ADomainServiceNotNamedForItsNamespaceIsRefused) {
  std::string clock = kClock;
  clock.replace(clock.find("rpc_service Clock {"), 19, "rpc_service Timer {");
  clock.replace(clock.find("rpc_service ClockEvents"), 23,
                "rpc_service TimerEvents");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the domain service sigil.protocol.clock.Timer is named"
                   " Timer, and a domain's service is named for its"
                   " namespace's last word raised, Clock"));
}

TEST(ProtocolModel, TwoNamespacesEndingInOneWordAreRefused) {
  EXPECT_TRUE(says(refusalOf(kPrelude + kClock + R"(
namespace sigil.elsewhere.clock;
/// A second clock.
rpc_service Clock {
  /// Asks.
  ask(sigil.protocol.Empty): sigil.protocol.Empty;
}
)"),
                   "both declare the domain clock"));
}

TEST(ProtocolModel, AnEventsServiceWithNoDomainIsRefused) {
  EXPECT_TRUE(says(refusalOf(kPrelude + kClock + R"(
namespace sigil.protocol.timer;
/// Events of no domain.
rpc_service TimerEvents {
  /// Ticks.
  tick(sigil.protocol.Empty): sigil.protocol.Empty (streaming: "server");
}
)"),
                   "the events service sigil.protocol.timer.TimerEvents has"
                   " no domain sigil.protocol.timer.Timer beside it"));
}

TEST(ProtocolModel, AnEventNotMarkedStreamingIsRefused) {
  std::string clock = kClock;
  clock.replace(clock.find(R"( (streaming: "server"))"), 22, "");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the event clock.budgetExpired is not marked"));
}

TEST(ProtocolModel, ACommandMarkedStreamingIsRefused) {
  std::string clock = kClock;
  clock.replace(clock.find("(asynchronous)"), 14, R"((streaming: "server"))");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the command clock.step is marked streaming"));
}

TEST(ProtocolModel, AnEventThatTakesATableIsRefused) {
  std::string clock = kClock;
  clock.replace(clock.find("budgetExpired(sigil.protocol.Empty)"), 35,
                "budgetExpired(StepParameters)");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the event clock.budgetExpired takes"
                   " sigil.protocol.clock.StepParameters"));
}

TEST(ProtocolModel, EventsWithoutEnableAndDisableAreRefused) {
  std::string clock = kClock;
  const size_t from = clock.find("  /// Starts the events.");
  clock.erase(from, clock.find("}\n/// The clock's events.") - from);
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the domain clock has events and no enable and disable"));
}

TEST(ProtocolModel, EnableAndDisableWithoutEventsAreRefused) {
  std::string clock = kClock;
  clock.erase(clock.find("/// The clock's events."));
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the domain clock has enable or disable and no events"));
}

TEST(ProtocolModel, AnEnableThatTakesATableIsRefused) {
  std::string clock = kClock;
  clock.replace(clock.find("enable(sigil.protocol.Empty)"), 28,
                "enable(StepParameters)");
  EXPECT_TRUE(says(refusalOf(kPrelude + clock),
                   "the command clock.enable takes"
                   " sigil.protocol.clock.StepParameters"));
}

TEST(ProtocolModel, DomainsTheBuildDoesNotNameAreRefused) {
  std::string why;
  const std::optional<generator::Model> model = read(kPrelude + kClock, &why);
  ASSERT_TRUE(model) << why;
  EXPECT_FALSE(generator::domainsAre(*model, {"clock", "host"}, &why));
  EXPECT_TRUE(says(why,
                   "the definition declares the domains clock, and the"
                   " build expects others"));
}

}  // namespace
