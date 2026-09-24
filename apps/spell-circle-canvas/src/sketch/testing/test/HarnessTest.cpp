/** @file
 * The harness: its hooks run in their order over a state directory of
 * the case's own; a failing case prints the host's own account before
 * the failure; and, end to end, a registry sketch opened, set to
 * Advance, stepped one second and photographed is the very plate the
 * sweep writes of that scene at that moment and density.
 */

#include <gtest/gtest.h>
#include <mach-o/dyld.h>
#include <sigilcompose/core/Core.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilsketch/testing/Harness.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "support/Fixtures.h"

namespace {

namespace protocol = sigil::protocol;
namespace sketch = sigil::sketch;
using sketch::testing::Harness;

/** A box marching right a canvas unit every sixtieth of a second, with a
 *  moment declared, so a still at any other time is another picture. */
struct HarnessMarchingBox {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0.1f, 0.1f, 0.2f, 1});
    ctx.captureAt(0.5);
  }
  void update(double elapsed, sketch::SketchContext& ctx) {
    using namespace sigil::compose;
    ctx.composer.render(box()
                            .width(10)
                            .height(10)
                            .inset(0, 0, 0, (float)elapsed * 30.0f)
                            .fill(Fill::color({1, 0.5f, 0, 1})));
  }
};

[[maybe_unused]] const bool kRegistered = sketch::add(
    "harness_marching_box", nullptr, "Test", "a box the harness photographs",
    &sketch::kindOf<HarnessMarchingBox>);

TEST_F(Harness, AStillAfterOneSecondIsTheSweepsPlateOfThatMoment) {
  // The sweep's plate of the scene at one second, at the density the
  // sweep chooses for a 64-unit canvas: its runtime's oversample of two.
  sketch::SweepOptions sweep;
  sweep.outputDirectory = (stateDirectory() / "sweep").string();
  sweep.only = sketch::find("harness_marching_box");
  sweep.at = 1.0;
  sweep.ledger = true;
  ASSERT_GE(sweep.only, 0);
  ASSERT_EQ(sketch::sweep(sweep, sketch::test::fonts(), sketch::test::assets()),
            0);
  const std::filesystem::path plate =
      stateDirectory() / "sweep" / "plate_harness_marching_box.png";

  ASSERT_TRUE(host().pinDensity());
  const auto opened = open("harness_marching_box");
  ASSERT_TRUE(opened) << opened.error().message;
  ASSERT_TRUE(clock(protocol::clock::Policy_Advance));
  const auto stepped = step(1.0);
  ASSERT_TRUE(stepped) << stepped.error().message;
  EXPECT_EQ(stepped.result().frame, 60u);
  const auto picture = still(2.0);
  ASSERT_TRUE(picture) << picture.error().message;
  EXPECT_EQ(picture.result().width, 128u);

  const sketch::testing::Comparison same = compare(picture.result(), plate);
  EXPECT_TRUE(same.identical())
      << same.problem << " differing pixels " << same.pixels.differingPixels
      << ", worst " << same.pixels.worst;
}

TEST_F(Harness, AStillAtAnotherMomentIsAnotherPicture) {
  // What gives the case above its power: the scene moves, so a still one
  // second in and one half a second in are two pictures.
  ASSERT_TRUE(open("harness_marching_box"));
  ASSERT_TRUE(clock(protocol::clock::Policy_Advance));
  ASSERT_TRUE(step(0.5));
  const auto early = still(2.0, "early.png");
  ASSERT_TRUE(step(0.5));
  const auto late = still(2.0, "late.png");
  ASSERT_TRUE(early && late);
  EXPECT_FALSE(compare(late.result(), early.result().path).identical());
}

/** Records the hooks in the order they ran, and what stood at each. */
class HarnessHooks : public Harness {
 protected:
  void SetUpStateDirectory(const std::filesystem::path& directory) override {
    order.push_back("directory");
    directoryWasEmpty = std::filesystem::is_directory(directory) &&
                        std::filesystem::is_empty(directory);
    std::FILE* seed = std::fopen((directory / "seed.txt").c_str(), "w");
    if (seed) std::fclose(seed);
  }
  void SetUpHostOptions(
      sketch::testing::InProcessHostOptions& options) override {
    order.push_back("options");
    options.program = "HookedHarness";
  }
  void SetUpOnHost() override { order.push_back("host"); }
  void TearDownOnHost() override { order.push_back("teardown"); }

  std::vector<std::string> order;
  bool directoryWasEmpty = false;
};

TEST_F(HarnessHooks, RunInTheirOrderOverADirectoryOfTheCasesOwn) {
  EXPECT_EQ(order, (std::vector<std::string>{"directory", "options", "host"}));
  EXPECT_TRUE(directoryWasEmpty);
  EXPECT_TRUE(std::filesystem::exists(stateDirectory() / "seed.txt"));
  EXPECT_NE(stateDirectory().string().find(
                "HarnessHooks.RunInTheirOrderOverADirectoryOfTheCasesOwn"),
            std::string::npos);
  const auto described = host().describe();
  ASSERT_TRUE(described);
  EXPECT_EQ(described.result().version.program, "HookedHarness");
  EXPECT_EQ(described.result().state_root, stateDirectory().string());
}

TEST_F(Harness, TheReadoutIsDescribeTheClockAndTheLastStill) {
  ASSERT_TRUE(clock(protocol::clock::Policy_Advance));
  ASSERT_TRUE(open("harness_marching_box"));
  const auto picture = still();
  ASSERT_TRUE(picture);
  const std::string account = readout();
  EXPECT_NE(account.find("host.describe: "), std::string::npos);
  EXPECT_NE(account.find("\"harness_marching_box\""), std::string::npos);
  EXPECT_NE(account.find("clock.current: "), std::string::npos);
  EXPECT_NE(account.find("\"Advance\""), std::string::npos);
  EXPECT_NE(account.find("last still: " + picture.result().path),
            std::string::npos);
}

/** A case that fails on purpose, run only by the case below in a
 *  process of its own. */
TEST_F(Harness, DISABLED_FailsAfterAStill) {
  ASSERT_TRUE(clock(protocol::clock::Policy_Advance));
  ASSERT_TRUE(open("harness_marching_box"));
  ASSERT_TRUE(still());
  ADD_FAILURE() << "the failure itself";
}

std::string thisBinary() {
  std::array<char, 4096> path{};
  uint32_t size = path.size();
  return _NSGetExecutablePath(path.data(), &size) == 0
             ? std::string(path.data())
             : std::string();
}

TEST(HarnessFailure, PrintsTheHostsAccountBeforeTheFailure) {
  const std::string binary = thisBinary();
  ASSERT_FALSE(binary.empty());
  const std::string command =
      "'" + binary +
      "' --gtest_also_run_disabled_tests "
      "--gtest_filter=Harness.DISABLED_FailsAfterAStill 2>&1";
  std::FILE* run = ::popen(command.c_str(), "r");
  ASSERT_NE(run, nullptr);
  std::string output;
  std::array<char, 4096> chunk{};
  while (std::fgets(chunk.data(), chunk.size(), run)) output += chunk.data();
  EXPECT_NE(::pclose(run), 0);  // the case failed, as it was written to
  const size_t account = output.find("[ HARNESS  ] what the host says");
  const size_t failure = output.find("the failure itself");
  ASSERT_NE(account, std::string::npos) << output;
  ASSERT_NE(failure, std::string::npos) << output;
  EXPECT_LT(account, failure) << output;
  EXPECT_NE(output.find("last still: "), std::string::npos);
  EXPECT_EQ(output.find("last still: none taken"), std::string::npos);
  // The directory a failed case keeps is this case's to take back.
  const std::string kept = "[ HARNESS  ] state directory kept: ";
  const size_t at = output.find(kept);
  ASSERT_NE(at, std::string::npos) << output;
  const size_t start = at + kept.size();
  const std::filesystem::path directory =
      output.substr(start, output.find('\n', start) - start);
  EXPECT_TRUE(std::filesystem::is_directory(directory));
  std::error_code error;
  std::filesystem::remove_all(directory, error);
}

}  // namespace
