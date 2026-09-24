/** @file
 * An endpoint mounted with no client costs a run nothing: its hub
 * dispatched between every frame runs no handler and attaches no client,
 * and the stills the run takes are byte for byte those of the same run
 * with no endpoint at all.
 */

#include <gtest/gtest.h>
#include <sigilcompose/core/Core.h>
#include <sigilio/hub/Hub.h>
#include <sigilprotocol/endpoint/Endpoint.h>
#include <sigilprotocol/registry/RegistryAgent.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/testing/InProcessHost.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#include "ScratchDir.h"

namespace {

namespace protocol = sigil::protocol;
namespace sketch = sigil::sketch;
using sigil::test::ScratchDir;

/** A box sliding across the canvas, so every frame is another picture. */
struct QuietBox {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
  }
  void update(double elapsed, sketch::SketchContext& ctx) {
    using namespace sigil::compose;
    ctx.composer.render(box()
                            .width(6)
                            .height(6)
                            .inset(0, 0, 0, (float)elapsed * 40.0f)
                            .fill(Fill::color({0, 1, 0, 1})));
  }
};

[[maybe_unused]] const bool kRegistered =
    sketch::add("quiet_endpoint_box", nullptr, "Test",
                "a box a quiet endpoint's run photographs",
                &sketch::kindOf<QuietBox>);

/** The registry's domain, counting every command it is asked: an
 *  endpoint with no client must never reach it. */
struct CountingRegistry final : protocol::registry::RegistryAgent {
  int asked = 0;
  protocol::Answer<protocol::registry::values::ListResult> list(
      const protocol::registry::values::ListParameters&) override {
    ++asked;
    return protocol::registry::values::ListResult{};
  }
  protocol::Answer<protocol::registry::values::CatalogResult> catalog(
      const protocol::registry::values::CatalogParameters&) override {
    ++asked;
    return protocol::registry::values::CatalogResult{};
  }
};

std::string bytesOf(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

/** Six stills a sixtieth of a second apart, with an endpoint mounted and
 *  dispatched before every step where @p mounted says, and nothing else
 *  different. */
std::string run(const std::filesystem::path& state, bool mounted,
                int* asked, size_t* clients) {
  sketch::testing::InProcessHostOptions options;
  options.stateDirectory = state;
  sketch::testing::InProcessHost host(std::move(options));
  CountingRegistry counting;
  protocol::registry::wire(host.dispatcher(), counting);
  sigil::io::Hub hub;
  std::optional<protocol::Endpoint> endpoint;
  if (mounted) endpoint.emplace(hub, host.dispatcher());
  EXPECT_TRUE(!mounted || endpoint->listening());
  EXPECT_TRUE(host.clock(protocol::clock::Policy_Advance));
  EXPECT_TRUE(host.open("quiet_endpoint_box"));
  std::string stills;
  for (int frame = 0; frame < 6; ++frame) {
    if (mounted) hub.dispatch();
    EXPECT_TRUE(host.step(1.0 / 60.0));
    const auto still = host.still(1.0, "still-" + std::to_string(frame) + ".png");
    EXPECT_TRUE(still);
    if (still) stills += bytesOf(still.result().path);
  }
  *asked = counting.asked;
  // The one client is the host's own, attached in process.
  *clients = host.dispatcher().sessions().size();
  endpoint.reset();
  return stills;
}

TEST(SketchQuietEndpoint, AFrameWithNoClientRunsNoHandlerAndDrawsWhatItWould) {
  const ScratchDir mountedState("sketch-quiet-endpoint-mounted");
  const ScratchDir bareState("sketch-quiet-endpoint-bare");
  int askedMounted = -1, askedBare = -1;
  size_t clientsMounted = 0, clientsBare = 0;
  const std::string mounted =
      run(mountedState.path, true, &askedMounted, &clientsMounted);
  const std::string bare = run(bareState.path, false, &askedBare, &clientsBare);
  EXPECT_EQ(askedMounted, 0);
  EXPECT_EQ(clientsMounted, 1u);
  EXPECT_EQ(clientsBare, 1u);
  // The endpoint took its address back as it went.
  EXPECT_FALSE(std::filesystem::exists(mountedState.path / "protocol-address"));
  ASSERT_FALSE(mounted.empty());
  EXPECT_EQ(mounted, bare);
}

}  // namespace
