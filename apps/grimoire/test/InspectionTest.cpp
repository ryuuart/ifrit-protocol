/** @file
 * The endpoint the window and a sweep mount: it listens on loopback with
 * its address under the run's state root, answers `host` as Grimoire
 * with the sessions the run holds and `registry` over the catalog, and
 * refuses the session and clock domains as not mounted, since the run's
 * frames are its own.
 */

#include <gtest/gtest.h>
#include <sigilprotocol/dispatch/InProcess.h>
#include <sigilprotocol/host/HostClient.h>
#include <sigilprotocol/registry/RegistryClient.h>
#include <sigilsketch/core/State.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "../src/Inspection.h"
#include "Fixtures.h"

namespace {

namespace protocol = sigil::protocol;
using protocol::Answer;

template <class Result>
protocol::Reply<Result> into(std::optional<Answer<Result>>& slot) {
  return [&slot](Answer<Result> answer) { slot.emplace(std::move(answer)); };
}

/** The answer an envelope's text gets from @p client, as text. */
std::string asked(const protocol::InProcess& client, std::string_view text) {
  std::string heard;
  client.send(text,
              [&heard](std::string answer) { heard = std::move(answer); });
  return heard;
}

TEST(GrimoireInspection, AnswersHostAndRegistryAndMountsNoSessionOrClock) {
  const grimoire::test::ScratchDir state("grimoire-inspection");
  sigil::sketch::setStateDirectory(state.path);
  Inspection inspection(0, sigil::sketch::CatalogSources{}, [] {
    return std::vector<OpenSession>{
        {.sketch = "cascade", .kind = "canvas", .width = 640, .height = 360}};
  });
  ASSERT_TRUE(inspection.listening());
  EXPECT_TRUE(std::filesystem::exists(state.path / "protocol-address"));

  const protocol::InProcess client(inspection.dispatcher());
  std::optional<Answer<protocol::host::values::DescribeResult>> described;
  protocol::host::HostClient(client.caller()).describe(into(described));
  ASSERT_TRUE(described && *described);
  EXPECT_EQ(described->result().version.program, "Grimoire");
  EXPECT_EQ(described->result().domains,
            (std::vector<std::string>{"host", "registry"}));
  EXPECT_EQ(described->result().state_root, state.path.string());
  ASSERT_EQ(described->result().sessions.size(), 1u);
  EXPECT_EQ(described->result().sessions[0].sketch, "cascade");

  std::optional<Answer<protocol::registry::values::ListResult>> listed;
  protocol::registry::RegistryClient(client.caller()).list({}, into(listed));
  ASSERT_TRUE(listed && *listed);

  for (const char* method : {"session.open", "clock.current"}) {
    const std::string answer = asked(
        client, std::string(R"({"id": 1, "method": ")") + method + R"("})");
    EXPECT_NE(answer.find("notMounted"), std::string::npos)
        << method << ": " << answer;
  }
}

}  // namespace
