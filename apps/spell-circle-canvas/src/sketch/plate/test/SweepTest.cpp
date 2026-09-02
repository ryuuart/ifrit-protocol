/** @file
 * The sweep: that a plate lands where it is named, that the same
 * declaration renders the same bytes twice, and that a plate is the size
 * the sketch asked for.
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

/** A canvas wider than half the host's plate-width budget, so the scale
 *  the host would pick for it is a FRACTION. Both probes below declare
 *  this same canvas: what separates them is whether the sketch names its
 *  own oversample, which is the whole of what the pair asks about. */
constexpr float kWideW = 1600;
constexpr float kWideH = 100;

struct WidePlate : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(kWideW, kWideH);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.1);
    ctx.composer.render(
        box().width(40).height(40).fill(Fill::color({1, 1, 1, 1})));
  }
};

struct WidePlateAtTwo : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(kWideW, kWideH);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.1);
    ctx.oversample(2);
    ctx.composer.render(
        box().width(40).height(40).fill(Fill::color({1, 1, 1, 1})));
  }
};

/** A PNG's width and height, read off IHDR — the first chunk of the
 *  file, its two big-endian dimensions at a fixed offset. Read rather
 *  than decoded because the question here is the size the sweep asked
 *  the encoder for, and nothing about the pixels. */
struct Extent {
  unsigned width = 0;
  unsigned height = 0;
};

Extent extentOf(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  unsigned char header[24] = {};
  in.read(reinterpret_cast<char*>(header), sizeof header);
  const auto be32 = [](const unsigned char* at) {
    return ((unsigned)at[0] << 24u) | ((unsigned)at[1] << 16u) |
           ((unsigned)at[2] << 8u) | (unsigned)at[3];
  };
  return {be32(header + 16), be32(header + 20)};
}

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

TEST(Sweep, RendersAtExactlyTheOversampleTheSketchDeclared) {
  // A whole number is the point: the sketch that declares one draws one
  // pixel of what it reconstructs as a whole number of canvas pixels,
  // and only a whole scale keeps that count the same in every column.
  const std::filesystem::path out =
      std::filesystem::temp_directory_path() / "sigil_sweep_test_f";
  std::filesystem::remove_all(out);
  SweepOptions options = ledgerRun(out);
  options.only = find("wide_plate_at_two");
  ASSERT_GE(options.only, 0);
  ASSERT_EQ(0, sweep(options, fonts(), assets()));
  const Extent plate = extentOf(out / "plate_wide_plate_at_two.png");
  EXPECT_EQ(plate.width, (unsigned)(kWideW * 2));
  EXPECT_EQ(plate.height, (unsigned)(kWideH * 2));
}

TEST(Sweep, KeepsTheWidthCeilingForASketchThatDeclaresNoOversample) {
  // The same canvas without the declaration: the host fits the plate to
  // its own width budget, which for this canvas is a fraction — so the
  // ceiling is what the plate's width reports, not twice the canvas.
  const std::filesystem::path out =
      std::filesystem::temp_directory_path() / "sigil_sweep_test_g";
  std::filesystem::remove_all(out);
  SweepOptions options = ledgerRun(out);
  options.only = find("wide_plate");
  ASSERT_GE(options.only, 0);
  ASSERT_EQ(0, sweep(options, fonts(), assets()));
  const Extent plate = extentOf(out / "plate_wide_plate.png");
  EXPECT_LT(plate.width, (unsigned)(kWideW * 2));
  EXPECT_EQ(plate.width, 2400u);
}

/** The two extra fixtures, recorded by hand: the registration macro
 *  files ONE sketch per translation unit, and these two are a pair that
 *  only means anything read together. */
[[maybe_unused]] const bool wideRegistered =
    add("wide_plate", nullptr, "Test", "a plate the width budget fits",
        &kindOf<WidePlate>);
[[maybe_unused]] const bool wideAtTwoRegistered =
    add("wide_plate_at_two", nullptr, "Test",
        "the same canvas, at a declared 2", &kindOf<WidePlateAtTwo>);

}  // namespace

SIGIL_SKETCH(Probe, "Test", "the sweep's own fixture")
