/** @file
 * The sweep: that a plate lands where it is named, that the same
 * declaration renders the same bytes twice, that a plate is the size the
 * sketch asked for, and that a sketch this machine cannot draw is passed
 * over rather than failed.
 */

// Registered the way a sketch file is, so the sweep walks a real registry
// rather than a fixture it would never otherwise see. It has to stand
// before the prelude: the macro chooses its form at include time.
#define SIGIL_SKETCH_STATIC "sweep_probe"

#include <gtest/gtest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/plate/Sweep.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "ScratchDir.h"
#include "Support.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::test::ScratchDir;

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

/** A sketch over something this machine does not have. The probe is a
 *  static member, which is how a sketch states its own requirement. */
struct Ungrounded : Sketch {
  static bool available(std::string* why) {
    if (why) *why = "the thing it draws is not installed";
    return false;
  }
  void setup(SketchContext& ctx) override {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.1);
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

std::vector<char> bytesOf(const std::filesystem::path& path) {
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
  const ScratchDir out("sigil_sweep_named");
  ASSERT_GE(find("sweep_probe"), 0);
  ASSERT_EQ(0, sweep(ledgerRun(out.path), fonts(), assets()));
  EXPECT_TRUE(std::filesystem::exists(out.path / "plate_sweep_probe.png"));
}

TEST(Sweep, TheSameDeclarationRendersTheSameBytes) {
  // The whole point of the ledger tier: a plate is a function of the
  // declared moment, so nothing about how fast this machine ran can
  // reach the image.
  const ScratchDir first("sigil_sweep_twice_first");
  const ScratchDir second("sigil_sweep_twice_second");
  ASSERT_EQ(0, sweep(ledgerRun(first.path), fonts(), assets()));
  ASSERT_EQ(0, sweep(ledgerRun(second.path), fonts(), assets()));
  EXPECT_EQ(bytesOf(first.path / "plate_sweep_probe.png"),
            bytesOf(second.path / "plate_sweep_probe.png"));
}

TEST(Sweep, RefusesToReportTimingItDidNotMeasure) {
  // A ledger run performs no benchmark, so a timing file would be zeros.
  const ScratchDir out("sigil_sweep_timing");
  SweepOptions options = ledgerRun(out.path);
  options.timingJson = (out.path / "timing.json").string();
  EXPECT_EQ(1, sweep(options, fonts(), assets()));
}

TEST(Sweep, SelectsByRuntime) {
  const ScratchDir out("sigil_sweep_runtime");
  SweepOptions options = ledgerRun(out.path);
  options.only = -1;
  options.kind = "set";  // nothing in this binary draws through one
  ASSERT_EQ(0, sweep(options, fonts(), assets()));
  EXPECT_FALSE(std::filesystem::exists(out.path / "plate_sweep_probe.png"));
}

TEST(Sweep, PassesOverASketchWhoseProbeSaysThisMachineCannotDrawIt) {
  // A skip is not a failure and not a mover: the run still succeeds, and
  // no plate is written. A card naming what is missing is not the
  // picture the sketch's name stands for, and a baseline that adopted
  // one would hold a promise about this machine's install rather than
  // about the drawing code.
  const ScratchDir out("sigil_sweep_unavailable");
  SweepOptions options = ledgerRun(out.path);
  options.only = find("ungrounded");
  ASSERT_GE(options.only, 0);
  EXPECT_EQ(0, sweep(options, fonts(), assets()));
  EXPECT_FALSE(std::filesystem::exists(out.path / "plate_ungrounded.png"));
}

TEST(Sweep, RendersAtExactlyTheOversampleTheSketchDeclared) {
  // A whole number is the point: the sketch that declares one draws one
  // pixel of what it reconstructs as a whole number of canvas pixels,
  // and only a whole scale keeps that count the same in every column.
  const ScratchDir out("sigil_sweep_oversample");
  SweepOptions options = ledgerRun(out.path);
  options.only = find("wide_plate_at_two");
  ASSERT_GE(options.only, 0);
  ASSERT_EQ(0, sweep(options, fonts(), assets()));
  const Extent plate = extentOf(out.path / "plate_wide_plate_at_two.png");
  EXPECT_EQ(plate.width, (unsigned)(kWideW * 2));
  EXPECT_EQ(plate.height, (unsigned)(kWideH * 2));
}

TEST(Sweep, KeepsTheWidthCeilingForASketchThatDeclaresNoOversample) {
  // The same canvas without the declaration: the host fits the plate to
  // its own width budget, which for this canvas is a fraction — so the
  // ceiling is what the plate's width reports, not twice the canvas.
  const ScratchDir out("sigil_sweep_ceiling");
  SweepOptions options = ledgerRun(out.path);
  options.only = find("wide_plate");
  ASSERT_GE(options.only, 0);
  ASSERT_EQ(0, sweep(options, fonts(), assets()));
  const Extent plate = extentOf(out.path / "plate_wide_plate.png");
  EXPECT_LT(plate.width, (unsigned)(kWideW * 2));
  EXPECT_EQ(plate.width, 2400u);
}

/** The extra fixtures, recorded by hand: the registration macro files
 *  ONE sketch per translation unit, and the two wide ones are a pair
 *  that only means anything read together. */
[[maybe_unused]] const bool wideRegistered =
    add("wide_plate", nullptr, "Test", "a plate the width budget fits",
        &kindOf<WidePlate>);
[[maybe_unused]] const bool wideAtTwoRegistered =
    add("wide_plate_at_two", nullptr, "Test",
        "the same canvas, at a declared 2", &kindOf<WidePlateAtTwo>);
[[maybe_unused]] const bool ungroundedRegistered =
    add("ungrounded", nullptr, "Test", "a sketch this machine cannot draw",
        &kindOf<Ungrounded>, &probeOf<Ungrounded>);

}  // namespace

SIGIL_SKETCH(Probe, "Test", "the sweep's own fixture")
