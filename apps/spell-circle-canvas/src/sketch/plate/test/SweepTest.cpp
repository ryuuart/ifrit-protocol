/** @file
 * The sweep: that a plate lands where it is named, and that the same
 * declaration renders the same bytes twice.
 */

// Registered the way a sketch file is, so the sweep walks a real registry
// rather than a fixture it would never otherwise see. It has to stand
// before the prelude: the macro chooses its form at include time.
#define SIGIL_SKETCH_STATIC "sweep_probe"

#include <gtest/gtest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

Assets& assets() {
  static auto* store = new Assets("");
  return *store;
}

/** A moving scene with a declared moment, so what the sweep captures
 *  depends on the declaration and could differ if it did not. */
struct Probe : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.5);
  }
  void update(double elapsed, SketchContext& ctx) override {
    ctx.composer.render(box()
                            .width(10)
                            .height(10)
                            .inset((float)elapsed * 20.0f, 0, 0, 0)
                            .fill(Fill::color({1, 0, 0, 1})));
  }
};

std::vector<char> read(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

SweepOptions ledgerRun(const std::filesystem::path& outDir) {
  SweepOptions options;
  options.outDir = outDir.string();
  options.ledger = true;
  options.noPromotion = true;
  options.only = find("sweep_probe");
  return options;
}

TEST(Sweep, WritesThePlateUnderTheNameTheSketchIsFiledAs) {
  const std::filesystem::path out =
      std::filesystem::temp_directory_path() / "sigil_sweep_test_a";
  std::filesystem::remove_all(out);
  ASSERT_GE(find("sweep_probe"), 0);
  ASSERT_EQ(0, sweep(ledgerRun(out), fonts(), assets()));
  EXPECT_TRUE(std::filesystem::exists(out / "plate_sweep_probe.png"));
}

TEST(Sweep, TheSameDeclarationRendersTheSameBytes) {
  // The whole point of the ledger tier: a plate is a function of the
  // declared moment, so nothing about how fast this machine ran can
  // reach the image.
  const std::filesystem::path first =
      std::filesystem::temp_directory_path() / "sigil_sweep_test_b";
  const std::filesystem::path second =
      std::filesystem::temp_directory_path() / "sigil_sweep_test_c";
  std::filesystem::remove_all(first);
  std::filesystem::remove_all(second);
  ASSERT_EQ(0, sweep(ledgerRun(first), fonts(), assets()));
  ASSERT_EQ(0, sweep(ledgerRun(second), fonts(), assets()));
  EXPECT_EQ(read(first / "plate_sweep_probe.png"),
            read(second / "plate_sweep_probe.png"));
}

TEST(Sweep, RefusesToReportTimingItDidNotMeasure) {
  // A ledger run performs no benchmark, so a timing file would be zeros.
  const std::filesystem::path out =
      std::filesystem::temp_directory_path() / "sigil_sweep_test_d";
  SweepOptions options = ledgerRun(out);
  options.timingJson = (out / "timing.json").string();
  EXPECT_EQ(1, sweep(options, fonts(), assets()));
}

TEST(Sweep, SelectsByRuntime) {
  const std::filesystem::path out =
      std::filesystem::temp_directory_path() / "sigil_sweep_test_e";
  std::filesystem::remove_all(out);
  SweepOptions options = ledgerRun(out);
  options.only = -1;
  options.kind = "set";  // nothing in this binary draws through one
  ASSERT_EQ(0, sweep(options, fonts(), assets()));
  EXPECT_FALSE(std::filesystem::exists(out / "plate_sweep_probe.png"));
}

}  // namespace

SIGIL_SKETCH(Probe, "Test", "the sweep's own fixture")
