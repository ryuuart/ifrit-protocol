/** @file
 * The sweep: that a plate lands where it is named, that the same
 * declaration renders the same bytes twice — including after another
 * sketch has run in the same process — that a plate is the size the
 * sketch asked for, and that a sketch this machine cannot draw is passed
 * over rather than failed.
 */

// Registered the way a sketch file is, so the sweep walks a real registry
// rather than a fixture it would never otherwise see. It has to stand
// before the prelude: the macro chooses its form at include time.
#define SIGIL_SKETCH_STATIC "sweep_probe"

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/image/Decode.h>
#include <sigilmedia/video/Video.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/plate/Story.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "ScratchDir.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::test::ScratchDir;

/** A moving scene with a declared moment, so what the sweep captures
 *  depends on the declaration and could differ if it did not. */
struct Probe {
  void setup(SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.5);
  }
  void update(double elapsed, SketchContext& ctx) {
    ctx.composer.render(box()
                            .width(10)
                            .height(10)
                            .inset(0, 0, 0, (float)elapsed * 20.0f)
                            .fill(Fill::color({1, 0, 0, 1})));
  }
};

/** Red until a deliberately late capture moment, then green and still. The
 *  Story tests can distinguish an honest pre-roll from an early loading cut,
 *  and can measure whether editorial motion shifted its frame. */
struct StoryMomentProbe {
  static inline int updates = 0, paints = 0;
  static inline double firstPaintSeconds = -1, lastUpdateSeconds = -1;
  double seconds = 0;

  void setup(SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(2.0);
    ctx.paintDiscardedFrames(false);
    ctx.composer.render(
        pen(
            "story_moment_probe.paint",
            [this](sigil::draw::Pen& pen) {
              if (paints == 0) firstPaintSeconds = seconds;
              ++paints;
              pen.noStroke();
              pen.fill(seconds > 1.9 ? sigil::material::Color{0, 1, 0, 1}
                                     : sigil::material::Color{1, 0, 0, 1});
              pen.rect(0, 0, 64, 48);
            },
            Cache::None)
            .width(64)
            .height(48));
  }
  void update(double elapsed) {
    ++updates;
    seconds = lastUpdateSeconds = elapsed;
  }
};

/** A retained set whose initial scene is green and every later scene is
 *  red. Repainting its held frame must keep the initial scene time. */
struct InitialSetProbe {
  static inline float describedSeconds = -1;

  void setup(SetContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.5);
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
  }

  sigil::world::Frame describe(float seconds) {
    describedSeconds = seconds;
    const sigil::material::Color color =
        seconds == 0 ? sigil::material::Color{0, 1, 0, 1}
                     : sigil::material::Color{1, 0, 0, 1};
    return sigil::world::Element()
        .key("initial_set")
        .mesh(sigil::geometry::mesh::quad(120, 90))
        .fill(sigil::material::surface::unlit({.baseColor = color}));
  }
};

/** A PEN THAT NEVER CLEARS: one green square per frame, marching right,
 *  on the canvas a `graphics` node keeps. What it is for is the
 *  montage's pre-roll: the picture IS the frames already drawn, and the
 *  canvas belongs to the node rather than to the surface the host steps
 *  on, so the march has to survive a pre-roll taken somewhere else. */
struct DrawnTrail {
  int drawn = 0;
  void setup(SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.5);
    ctx.composer.render(graphics("drawn_trail.loop",
                                 [this](sigil::draw::Pen& pen) {
                                   pen.noStroke();
                                   pen.fill(0, 255, 0);
                                   pen.rect((float)((drawn++ * 2) % 60), 20, 3,
                                            8);
                                 })
                            .absolute()
                            .inset(0));
  }
};

/** A canvas wider than half the host's plate-width budget, so the scale
 *  the host would pick for it is a FRACTION. Both probes below declare
 *  this same canvas: what separates them is whether the sketch names its
 *  own oversample, which is the whole of what the pair asks about. */
constexpr float kWideW = 1600;
constexpr float kWideH = 100;

struct WidePlate {
  void setup(SketchContext& ctx) {
    ctx.canvas(kWideW, kWideH);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.1);
    ctx.composer.render(
        box().width(40).height(40).fill(Fill::color({1, 1, 1, 1})));
  }
};

struct WidePlateAtTwo {
  void setup(SketchContext& ctx) {
    ctx.canvas(kWideW, kWideH);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.1);
    ctx.oversample(2);
    ctx.composer.render(
        box().width(40).height(40).fill(Fill::color({1, 1, 1, 1})));
  }
};

/** Each opened session records its first frame's bake density. */
struct DensityPlate {
  static inline std::vector<float> firstDensities;
  bool firstFrame = true;

  void setup(SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.oversample(3);
    ctx.composer.render(box().width(64).height(48).children(
        {box().absolute().left(8).top(8).width(16).height(16).fill(
            Fill::color({0, 1, 0, 1}))}));
  }

  void update(double, SketchContext& ctx) {
    if (!firstFrame) return;
    firstDensities.push_back(ctx.composer.bakeDensity());
    firstFrame = false;
  }
};

/** A sketch over something this machine does not have. The probe is a
 *  static member, which is how a sketch states its own requirement. */
struct Ungrounded {
  static bool available(std::string* why) {
    if (why) *why = "the thing it draws is not installed";
    return false;
  }
  void setup(SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.1);
  }
};

struct MissingShader {
  void setup(SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.captureAt(0.05);
    ctx.composer.render(box().width(64).height(48).fill(sigil::material::shader(
        ctx.assets.hub(), "res://missing_sweep_program.sksl")));
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

/** A plate, decoded. The byte-identity cases read the FILE, because the
 *  question there is the encode; a case about drift has to read pixels. */
sk_sp<SkImage> plateImage(const std::filesystem::path& path) {
  const std::vector<char> encoded = bytesOf(path);
  if (encoded.empty()) return nullptr;
  const auto asset = sigil::media::decode<sigil::media::Image>(
      std::as_bytes(std::span(encoded)), {}, path);
  return asset && !asset->frames().empty() ? asset->frames().front().image
                                           : nullptr;
}

/** The worst per-channel disagreement between two plates of one size. */
int worstChannelDrift(const SkImage& a, const SkImage& b) {
  if (a.width() != b.width() || a.height() != b.height()) return 256;
  SkBitmap left, right;
  left.allocN32Pixels(a.width(), a.height());
  right.allocN32Pixels(b.width(), b.height());
  if (!a.readPixels(left.pixmap(), 0, 0) || !b.readPixels(right.pixmap(), 0, 0))
    return 256;
  int worst = 0;
  for (int y = 0; y < a.height(); ++y)
    for (int x = 0; x < a.width(); ++x) {
      const SkColor p = left.getColor(x, y), q = right.getColor(x, y);
      if (p == q) continue;
      for (int shift : {0, 8, 16, 24})
        worst = std::max(worst, std::abs((int)((p >> shift) & 0xffu) -
                                         (int)((q >> shift) & 0xffu)));
    }
  return worst;
}

struct PixelBounds {
  int left = 0;
  int top = 0;
  int right = -1;
  int bottom = -1;

  bool operator==(const PixelBounds&) const = default;
};

PixelBounds greenBounds(const SkImage& image) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(image.width(), image.height());
  if (!image.readPixels(bitmap.pixmap(), 0, 0)) return {};
  PixelBounds bounds{image.width(), image.height(), -1, -1};
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const SkColor color = bitmap.getColor(x, y);
      const int red = SkColorGetR(color);
      const int green = SkColorGetG(color);
      const int blue = SkColorGetB(color);
      if (green < 150 || green < red + 70 || green < blue + 70) continue;
      bounds.left = std::min(bounds.left, x);
      bounds.top = std::min(bounds.top, y);
      bounds.right = std::max(bounds.right, x);
      bounds.bottom = std::max(bounds.bottom, y);
    }
  }
  return bounds;
}

SweepOptions ledgerRun(const std::filesystem::path& outputDirectory) {
  SweepOptions options;
  options.outputDirectory = outputDirectory.string();
  options.ledger = true;
  options.noPromotion = true;
  options.only = find("sweep_probe");
  return options;
}

TEST(Sweep, PromotionAndNoPromotionAskForOppositeRunsAndAreRefusedTogether) {
  // The two flags name the same switch in opposite positions, and a run
  // that took one silently would report a picture nobody asked for.
  const ScratchDir out("sigil_sweep_both_promotions");
  SweepOptions options = ledgerRun(out.path);
  options.promotion = true;  // …beside the noPromotion ledgerRun sets
  EXPECT_EQ(1, sweep(options, fonts(), assets()));
  EXPECT_FALSE(std::filesystem::exists(out.path / "plate_sweep_probe.png"));
}

TEST(Sweep, APromotedRunKeepsEveryOtherPinAndStaysWithinOneCodeValue) {
  // The lane's contract on one sketch: a promoted run still renders, and
  // what it renders stands within the rule the whole tier is judged by.
  // A bake is taken under the live matrix post-translated by an integer,
  // so a shaded pixel can land one code value from the live paint and
  // nothing may land further. This probe is too cheap for the promoter to
  // fire on, so the drift it measures is zero — what fails here is a
  // promoted run that stops rendering, or one that moves a picture the
  // promoter never touched.
  const ScratchDir off("sigil_sweep_promo_off");
  const ScratchDir on("sigil_sweep_promo_on");
  ASSERT_EQ(0, sweep(ledgerRun(off.path), fonts(), assets()));
  SweepOptions promoted = ledgerRun(on.path);
  promoted.noPromotion = false;
  promoted.promotion = true;
  ASSERT_EQ(0, sweep(promoted, fonts(), assets()));
  const sk_sp<SkImage> a = plateImage(off.path / "plate_sweep_probe.png");
  const sk_sp<SkImage> b = plateImage(on.path / "plate_sweep_probe.png");
  ASSERT_TRUE(a && b);
  EXPECT_LE(worstChannelDrift(*a, *b), 1);
}

TEST(Sweep, WritesThePlateUnderTheNameTheSketchIsFiledAs) {
  const ScratchDir out("sigil_sweep_named");
  ASSERT_GE(find("sweep_probe"), 0);
  ASSERT_EQ(0, sweep(ledgerRun(out.path), fonts(), assets()));
  EXPECT_TRUE(std::filesystem::exists(out.path / "plate_sweep_probe.png"));
}

TEST(Sweep, AShaderFallbackFailsTheRunAndDoesNotTaintTheNextSketch) {
  const ScratchDir out("sigil_sweep_missing_shader");
  SweepOptions broken = ledgerRun(out.path);
  broken.only = find("missing_shader");
  ASSERT_GE(broken.only, 0);
  ::testing::internal::CaptureStderr();
  EXPECT_EQ(1, sweep(broken, fonts(), assets()));
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("missing_sweep_program.sksl"), std::string::npos);
  EXPECT_NE(log.find("resource error"), std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(out.path / "plate_missing_shader.png"));
  EXPECT_EQ(0, sweep(ledgerRun(out.path), fonts(), assets()));
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

TEST(Sweep, ASketchIsTheSamePlateAfterAnotherOneHasRunInThisProcess) {
  // A SWEEP OPENS EVERY SKETCH IN ONE PROCESS, so whatever one leaves
  // behind reaches the next: a warmed shaping cache, a shared context's
  // counters, a static that only fills. A plate that came out different
  // for any of those reasons would be reported as moved by a change that
  // moved nothing, which is the entire value of a byte-identity sweep.
  const ScratchDir alone("sigil_sweep_carry_alone");
  const ScratchDir after("sigil_sweep_carry_after");
  ASSERT_EQ(0, sweep(ledgerRun(alone.path), fonts(), assets()));

  SweepOptions between = ledgerRun(after.path);
  between.only = find("story_moment_probe");
  ASSERT_GE(between.only, 0);
  ASSERT_EQ(0, sweep(between, fonts(), assets()));

  ASSERT_EQ(0, sweep(ledgerRun(after.path), fonts(), assets()));
  EXPECT_EQ(bytesOf(alone.path / "plate_sweep_probe.png"),
            bytesOf(after.path / "plate_sweep_probe.png"));
}

TEST(Sweep, AMomentTheRunNamesOutranksTheOneTheSketchDeclared) {
  // The probe moves, and declares half a second: a run that names that
  // same moment writes the same bytes, and one that names the first
  // frame — zero is a moment like any other — writes a different picture.
  const ScratchDir declared("sigil_sweep_at_declared");
  const ScratchDir same("sigil_sweep_at_same");
  const ScratchDir first("sigil_sweep_at_first");
  ASSERT_EQ(0, sweep(ledgerRun(declared.path), fonts(), assets()));
  SweepOptions atDeclared = ledgerRun(same.path);
  atDeclared.at = 0.5;
  ASSERT_EQ(0, sweep(atDeclared, fonts(), assets()));
  SweepOptions atFirst = ledgerRun(first.path);
  atFirst.at = 0.0;
  ASSERT_EQ(0, sweep(atFirst, fonts(), assets()));
  EXPECT_EQ(bytesOf(declared.path / "plate_sweep_probe.png"),
            bytesOf(same.path / "plate_sweep_probe.png"));
  EXPECT_NE(bytesOf(declared.path / "plate_sweep_probe.png"),
            bytesOf(first.path / "plate_sweep_probe.png"));
}

TEST(Sweep, AZeroMomentDescribesTheInitialSetAndKeepsItsTime) {
  const ScratchDir out("sigil_sweep_initial_set");
  SweepOptions options = ledgerRun(out.path);
  options.only = find("initial_set_probe");
  options.at = 0.0;
  ASSERT_GE(options.only, 0);
  InitialSetProbe::describedSeconds = -1;
  ASSERT_EQ(0, sweep(options, fonts(), assets()));
  EXPECT_EQ(InitialSetProbe::describedSeconds, 0.0f);

  const sk_sp<SkImage> image =
      plateImage(out.path / "plate_initial_set_probe.png");
  ASSERT_TRUE(image);
  const PixelBounds bounds = greenBounds(*image);
  EXPECT_GT(bounds.right, bounds.left);
  EXPECT_GT(bounds.bottom, bounds.top);
  EXPECT_GT(bounds.left, 0);
  EXPECT_GT(bounds.top, 0);
  EXPECT_LT(bounds.right, image->width() - 1);
  EXPECT_LT(bounds.bottom, image->height() - 1);
}

TEST(Sweep, SelectsByRuntime) {
  const ScratchDir out("sigil_sweep_runtime");
  SweepOptions options = ledgerRun(out.path);
  options.only = -1;
  options.kind = "set";
  ASSERT_EQ(0, sweep(options, fonts(), assets()));
  EXPECT_FALSE(std::filesystem::exists(out.path / "plate_sweep_probe.png"));
  EXPECT_TRUE(
      std::filesystem::exists(out.path / "plate_initial_set_probe.png"));
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
  EXPECT_EQ(plate.width, (unsigned)kPlateWidthCeiling);
}

TEST(Sweep, AnExplicitDensitySizesThePlateAndPinsEverySessionsFirstFrame) {
  for (const float density : {0.5f, 2.0f}) {
    SCOPED_TRACE(density);
    const ScratchDir out("sigil_sweep_density");
    SweepOptions options = ledgerRun(out.path);
    options.only = find("density_plate");
    options.density = density;
    options.at = 0;
    // Exercise the measured session and the fresh capture session.
    options.ledger = false;
    ASSERT_GE(options.only, 0);
    DensityPlate::firstDensities.clear();
    ASSERT_EQ(0, sweep(options, fonts(), assets()));
    ASSERT_EQ(DensityPlate::firstDensities.size(), 2u);
    for (const float observed : DensityPlate::firstDensities)
      EXPECT_EQ(observed, density);

    const sk_sp<SkImage> image =
        plateImage(out.path / "plate_density_plate.png");
    ASSERT_TRUE(image);
    EXPECT_EQ(image->width(), (int)(64 * density));
    EXPECT_EQ(image->height(), (int)(48 * density));
    const PixelBounds bounds = greenBounds(*image);
    EXPECT_EQ(bounds.left, (int)(8 * density));
    EXPECT_EQ(bounds.top, (int)(8 * density));
    EXPECT_EQ(bounds.right, (int)(24 * density) - 1);
    EXPECT_EQ(bounds.bottom, (int)(24 * density) - 1);
  }
}

TEST(Sweep, InvalidOrOverflowingDensityIsRefusedBeforeAllocatingAPlate) {
  const ScratchDir out("sigil_sweep_invalid_density");
  for (const float density : {-1.0f, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN(), 1e20f}) {
    SCOPED_TRACE(density);
    SweepOptions options = ledgerRun(out.path);
    options.density = density;
    EXPECT_EQ(1, sweep(options, fonts(), assets()));
  }
  EXPECT_FALSE(std::filesystem::exists(out.path / "plate_sweep_probe.png"));
}

TEST(Story, EncodesASelectedSketchAsVerticalMp4) {
  const ScratchDir out("sigil_story_selected");
  StoryOptions options;
  options.outputPath = (out.path / "story.mp4").string();
  options.only = find("story_moment_probe");
  options.width = 360;
  options.height = 640;
  options.framesPerSecond = 10;
  options.framesPerSketch = 3;
  options.introFrames = 0;
  options.outroFrames = 0;
  options.bitRate = 500'000;
  options.hardware = sigil::media::HardwarePreference::Disabled;
  ASSERT_GE(options.only, 0);
  StoryMomentProbe::updates = StoryMomentProbe::paints = 0;
  StoryMomentProbe::firstPaintSeconds = StoryMomentProbe::lastUpdateSeconds =
      -1;
  ASSERT_EQ(0, story(options, fonts(), assets()));
  EXPECT_GE(StoryMomentProbe::updates, 120 + options.framesPerSketch);
  EXPECT_EQ(StoryMomentProbe::paints, options.framesPerSketch);
  EXPECT_NEAR(StoryMomentProbe::firstPaintSeconds, 2.0, 1e-9);
  EXPECT_NEAR(StoryMomentProbe::lastUpdateSeconds, 2.2, 1e-9);

  const std::vector<char> encoded = bytesOf(options.outputPath);
  ASSERT_FALSE(encoded.empty());
  const std::span<const std::byte> bytes = std::as_bytes(std::span(encoded));
  const std::optional<sigil::media::Metadata> probe = probeDocument(
      std::type_identity<sigil::media::Video>{}, bytes, options.outputPath);
  ASSERT_TRUE(probe);
  EXPECT_EQ(probe->width, 360);
  EXPECT_EQ(probe->height, 640);
  EXPECT_NEAR(probe->frameRate, 10.0, 0.1);
  EXPECT_GE(probe->duration.count(), 0.29);

  const auto clip = sigil::media::decode<sigil::media::Video>(
      bytes, {.hardware = sigil::media::HardwarePreference::Disabled},
      options.outputPath);
  ASSERT_TRUE(clip);
  const sigil::media::Frame first = clip->decodeAt({});
  const sigil::media::Frame last =
      clip->decodeAt(std::chrono::duration<double>(0.2));
  ASSERT_TRUE(first.image);
  ASSERT_TRUE(last.image);
  const PixelBounds firstBounds = greenBounds(*sigil::media::toSk(first.image));
  const PixelBounds lastBounds = greenBounds(*sigil::media::toSk(last.image));
  EXPECT_GE(firstBounds.right, firstBounds.left)
      << "the first video frame was captured before the declared moment";
  EXPECT_EQ(firstBounds, lastBounds)
      << "the fitted sketch moved inside its story card";
}

/** THE MONTAGE'S PRE-ROLL KEEPS A PEN'S PIXELS. The picture a pen
 *  program builds up is the frames it has already drawn, and they stand
 *  on the node's own canvas — so reaching the declared moment on a
 *  scratch surface leaves the march intact. */
TEST(Story, AKeptCanvasSurvivesThePreRoll) {
  const ScratchDir out("sigil_story_drawn");
  StoryOptions options;
  options.outputPath = (out.path / "drawn.mp4").string();
  options.only = find("drawn_trail");
  options.width = 360;
  options.height = 640;
  options.framesPerSecond = 10;
  options.framesPerSketch = 2;
  options.introFrames = 0;
  options.outroFrames = 0;
  options.bitRate = 500'000;
  options.hardware = sigil::media::HardwarePreference::Disabled;
  ASSERT_GE(options.only, 0);
  ASSERT_EQ(0, story(options, fonts(), assets()));

  const std::vector<char> encoded = bytesOf(options.outputPath);
  ASSERT_FALSE(encoded.empty());
  const std::span<const std::byte> bytes = std::as_bytes(std::span(encoded));
  const auto clip = sigil::media::decode<sigil::media::Video>(
      bytes, {.hardware = sigil::media::HardwarePreference::Disabled},
      options.outputPath);
  ASSERT_TRUE(clip);
  const sigil::media::Frame first = clip->decodeAt({});
  ASSERT_TRUE(first.image);
  const PixelBounds trail = greenBounds(*sigil::media::toSk(first.image));
  ASSERT_GE(trail.right, trail.left);
  // Several squares' worth of march, not the one square the last frame
  // drew: the trail spans most of the fitted canvas's width.
  EXPECT_GT(trail.right - trail.left, (trail.bottom - trail.top) * 2)
      << "the pre-roll's pixels were thrown away";
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
[[maybe_unused]] const bool missingShaderRegistered =
    add("missing_shader", nullptr, "Test", "a shader fallback is not a pass",
        &kindOf<MissingShader>);
[[maybe_unused]] const bool drawnTrailRegistered =
    add("drawn_trail", nullptr, "Test", "a pen's trail, kept between frames",
        &kindOf<DrawnTrail>);
[[maybe_unused]] const bool storyMomentRegistered =
    add("story_moment_probe", nullptr, "Test",
        "a late capture moment held in a fixed video frame",
        &kindOf<StoryMomentProbe>);
[[maybe_unused]] const bool initialSetRegistered =
    add("initial_set_probe", nullptr, "Test", "a retained scene at time zero",
        &kindOf<InitialSetProbe>);
[[maybe_unused]] const bool densityPlateRegistered =
    add("density_plate", nullptr, "Test", "the first frame's capture density",
        &kindOf<DensityPlate>);

}  // namespace

SIGIL_SKETCH(Probe, "Test", "the sweep's own fixture")
