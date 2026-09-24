/** @file
 * The registry domain's self-check, in process: mounted on a dispatcher,
 * the agent answers registry.list with the registry's rows — an entry
 * this machine cannot draw among them, with its reason — kept to one
 * runtime when one is named, and registry.catalog with the registry's
 * rows followed by the files a client names; and describe then lists the
 * domain as mounted.
 */

#include <gtest/gtest.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/host/HostClient.h>
#include <sigilprotocol/registry/RegistryClient.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Session.h>
#include <sigilsketch/core/agent/RegistryAgent.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "ScratchDir.h"

namespace {

namespace protocol = sigil::protocol;
namespace values = protocol::registry::values;
using sigil::sketch::RegistryAgent;
using sigil::test::ScratchDir;

/** A kind that draws nothing: the registry never opens one. */
struct AgentProbeKind final : sigil::sketch::KindOperations {
  [[nodiscard]] std::string_view runtime() const override {
    return "agentprobe";
  }
  [[nodiscard]] std::unique_ptr<sigil::sketch::Session> open(
      sigil::weave::FontContext&, sigil::sketch::Assets&, bool,
      std::string_view) const override {
    return nullptr;
  }
  bool operator==(const AgentProbeKind&) const { return true; }
};

sigil::sketch::Kind agentProbeKind() { return AgentProbeKind{}; }

bool missingEverything(std::string* why) {
  if (why) *why = "needs a device this test does not have";
  return false;
}

const bool kRegistered =
    sigil::sketch::add("registry_agent_probe", "registry_agent_probe",
                       "Agent \xc2\xb7 Probe", "one entry", &agentProbeKind) &&
    sigil::sketch::add("registry_agent_absent", "registry_agent_absent",
                       "Agent \xc2\xb7 Probe", "unavailable", &agentProbeKind,
                       &missingEverything);

/** An answer handed back in process, which is at once. */
template <class Result>
struct Heard {
  std::optional<protocol::Answer<Result>> answer;
  protocol::Reply<Result> reply() {
    return [this](protocol::Answer<Result> given) {
      answer.emplace(std::move(given));
    };
  }
};

const values::Entry* named(const values::ListResult& list,
                           std::string_view name) {
  const auto found =
      std::find_if(list.sketches.begin(), list.sketches.end(),
                   [&](const values::Entry& entry) { return entry.name == name; });
  return found == list.sketches.end() ? nullptr : &*found;
}

TEST(SketchRegistryAgent, ListsTheRegistryUnavailableEntriesIncluded) {
  ASSERT_TRUE(kRegistered);
  protocol::Dispatcher dispatcher;
  RegistryAgent agent;
  protocol::registry::wire(dispatcher, agent);
  const protocol::InProcess client(dispatcher);

  Heard<values::ListResult> heard;
  protocol::registry::RegistryClient(client.caller())
      .list(values::ListParameters{}, heard.reply());
  ASSERT_TRUE(heard.answer && *heard.answer) << heard.answer->error().message;
  const values::ListResult& list = heard.answer->result();
  EXPECT_EQ(list.sketches.size(), sigil::sketch::registry().size());
  const values::Entry* probe = named(list, "registry_agent_probe");
  ASSERT_TRUE(probe);
  EXPECT_EQ(probe->stem, "registry_agent_probe");
  EXPECT_EQ(probe->kind, "agentprobe");
  EXPECT_TRUE(probe->available);
  const values::Entry* absent = named(list, "registry_agent_absent");
  ASSERT_TRUE(absent);
  EXPECT_FALSE(absent->available);
  EXPECT_EQ(absent->reason, "needs a device this test does not have");
}

TEST(SketchRegistryAgent, KeepsTheListToTheRuntimeNamed) {
  protocol::Dispatcher dispatcher;
  RegistryAgent agent;
  protocol::registry::wire(dispatcher, agent);
  const protocol::InProcess client(dispatcher);

  Heard<values::ListResult> heard;
  values::ListParameters parameters;
  parameters.kind = "agentprobe";
  protocol::registry::RegistryClient(client.caller())
      .list(parameters, heard.reply());
  ASSERT_TRUE(heard.answer && *heard.answer);
  ASSERT_EQ(heard.answer->result().sketches.size(), 2u);
  for (const values::Entry& entry : heard.answer->result().sketches)
    EXPECT_EQ(entry.kind, "agentprobe");
}

TEST(SketchRegistryAgent, CatalogsTheRegistryThenTheFilesAClientNames) {
  const ScratchDir scratch("sketch-registry-agent");
  scratch.write("drafted.cpp", "// A draft.\n//\n// Its subject.\nint x;\n");
  const std::filesystem::path file = scratch.path / "drafted.cpp";

  protocol::Dispatcher dispatcher;
  RegistryAgent agent;
  protocol::registry::wire(dispatcher, agent);
  const protocol::InProcess client(dispatcher);

  Heard<values::CatalogResult> heard;
  values::CatalogParameters parameters;
  parameters.paths = {file.string()};
  protocol::registry::RegistryClient(client.caller())
      .catalog(parameters, heard.reply());
  ASSERT_TRUE(heard.answer && *heard.answer) << heard.answer->error().message;
  const std::vector<values::CatalogRow>& rows = heard.answer->result().rows;
  ASSERT_EQ(rows.size(), sigil::sketch::registry().size() + 1);
  const values::CatalogRow& drafted = rows.back();
  EXPECT_EQ(drafted.name, "drafted");
  EXPECT_TRUE(drafted.external);
  EXPECT_FALSE(drafted.video_exportable);
  // A C++ file names no runtime until it has been built.
  EXPECT_EQ(drafted.kind, "");
  EXPECT_EQ(drafted.path, file.string());
  EXPECT_GT(drafted.lines, 0u);
}

TEST(SketchRegistryAgent, IsListedAmongTheDomainsDescribeNames) {
  protocol::Dispatcher dispatcher;
  RegistryAgent agent;
  protocol::registry::wire(dispatcher, agent);
  const protocol::InProcess client(dispatcher);

  Heard<protocol::host::values::DescribeResult> heard;
  protocol::host::HostClient(client.caller()).describe(heard.reply());
  ASSERT_TRUE(heard.answer && *heard.answer);
  EXPECT_EQ(heard.answer->result().domains,
            (std::vector<std::string>{"host", "registry"}));
}

}  // namespace
