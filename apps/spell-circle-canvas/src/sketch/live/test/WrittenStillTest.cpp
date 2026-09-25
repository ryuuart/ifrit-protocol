/** @file
 * A written still at a plate's density is that plate: the host steps a
 * scene to its moment with every raster it bakes pinned to the density
 * the still is taken at, as the sweep pins its plate's, so a pen's canvas
 * formed on the way is drawn on the still's grid rather than magnified
 * into it; and it photographs through the runtime's own still, as the
 * sweep does, so a scene that moves is the plate of the same moment —
 * and so is the still a protocol session takes of it in process, and a
 * still at a fractional density is the plate's own pixels.
 */

#include <gtest/gtest.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigildraw/Pen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilsketch/testing/Comparison.h>
#include <sigilsketch/testing/InProcessHost.h>

#include <cmath>
#include <filesystem>
#include <limits>
#include <stdexcept>

#include "Fixture.h"
#include "support/Fixtures.h"

namespace {

namespace sketch = sigil::sketch;
namespace protocol = sigil::protocol;

/** A PEN THAT DRAWS ONCE and keeps it: a disc and a thin diagonal on its
 *  first run, nothing after, on the canvas the node holds. Every pixel of
 *  the still is therefore what the first frame formed, at whatever
 *  density that canvas was formed at — a canvas formed at one pixel per
 *  unit and photographed at two is visibly softer than one formed at
 *  two. */
void drawOnce(sigil::draw::Pen& pen) {
  if (pen.frameCount > 1) return;
  pen.noStroke();
  pen.fill(255, 200, 40);
  pen.circle(24, 24, 17);
  pen.stroke(80, 220, 255);
  pen.strokeWeight(0.75f);
  pen.line(4, 44, 60, 4);
}

struct PenDrawnOnce {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(1.0);
    ctx.composer.render(
        sigil::compose::graphics("pen_drawn_once.loop", &drawOnce)
            .absolute()
            .inset(0));
  }
};

sketch::Kind penDrawnOnceKind() { return sketch::kindOf<PenDrawnOnce>(); }

[[maybe_unused]] const bool kRegistered =
    sketch::add("pen_drawn_once", nullptr, "Test",
                "a pen that draws its picture on its first run and keeps it",
                &penDrawnOnceKind);

const sketch::Entry kPenDrawnOnce{"pen_drawn_once", "pen_drawn_once", "Test",
                                  "", &penDrawnOnceKind};

sketch::Host::Options optionsFor(const std::filesystem::path& path) {
  sketch::Host::Options options;
  options.sketchPath = path;
  options.assetsDirectory = std::filesystem::temp_directory_path();
  options.flagsFile = std::filesystem::temp_directory_path() / "no_such.rsp";
  options.compiledIn = &kPenDrawnOnce;
  options.clock = sigil::motion::ClockPolicy::Advance;
  return options;
}

TEST(SketchWrittenStill, AtAPlatesDensityIsThePlateOfThatMoment) {
  const sketch::test::Watched file("sigil_sketch_written_still");
  sketch::SweepOptions sweep;
  sweep.outputDirectory = (file.dir.path / "sweep").string();
  sweep.only = sketch::find("pen_drawn_once");
  sweep.at = 1.0;
  sweep.ledger = true;
  ASSERT_GE(sweep.only, 0);
  ASSERT_EQ(sketch::sweep(sweep, sketch::test::fonts(), sketch::test::assets()),
            0);
  const std::filesystem::path plate =
      file.dir.path / "sweep" / "plate_pen_drawn_once.png";

  sketch::Host host(optionsFor(file.path), sketch::test::fonts());
  ASSERT_TRUE(host.live());
  const float density = sketch::plateDensity(*host.session());
  ASSERT_EQ(density, 2.0f);
  EXPECT_EQ(host.prepareCapture(1.0, 60.0, density), 1.0);
  const std::filesystem::path written = file.dir.path / "written.png";
  ASSERT_TRUE(host.writePhotograph(written, density));

  const sketch::testing::Comparison same =
      sketch::testing::compare(written, plate);
  EXPECT_TRUE(same.identical())
      << same.problem << " differing pixels " << same.pixels.differingPixels
      << ", worst " << same.pixels.worst;

  // What gives the case its power: the same scene stepped at one pixel
  // per unit and photographed at two is another picture, the one a
  // written still was before its frames were pinned.
  ASSERT_TRUE(host.restartSession());
  EXPECT_EQ(host.prepareCapture(1.0, 60.0, 1.0f), 1.0);
  const std::filesystem::path magnified = file.dir.path / "magnified.png";
  ASSERT_TRUE(host.writePhotograph(magnified, density));
  EXPECT_FALSE(sketch::testing::compare(magnified, plate).identical());
}

/** A BOX MARCHING RIGHT a canvas unit every sixtieth of a second, so a
 *  still one frame later than another is another picture. */
struct MarchingBox {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(64, 48);
    ctx.background({0.1f, 0.1f, 0.2f, 1});
    ctx.captureAt(1.0);
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

sketch::Kind marchingBoxKind() { return sketch::kindOf<MarchingBox>(); }

[[maybe_unused]] const bool kMarchingRegistered = sketch::add(
    "written_still_marching_box", nullptr, "Test",
    "a box that moves every frame, photographed as a plate", &marchingBoxKind);

const sketch::Entry kMarchingBox{"written_still_marching_box",
                                 "written_still_marching_box", "Test", "",
                                 &marchingBoxKind};

TEST(SketchWrittenStill, OfAMovingSceneIsThePlateOfThatMoment) {
  const sketch::test::Watched file("sigil_sketch_written_still_moving");
  sketch::SweepOptions sweep;
  sweep.outputDirectory = (file.dir.path / "sweep").string();
  sweep.only = sketch::find("written_still_marching_box");
  sweep.at = 1.0;
  sweep.ledger = true;
  ASSERT_GE(sweep.only, 0);
  ASSERT_EQ(sketch::sweep(sweep, sketch::test::fonts(), sketch::test::assets()),
            0);
  const std::filesystem::path plate =
      file.dir.path / "sweep" / "plate_written_still_marching_box.png";

  sketch::Host::Options options = optionsFor(file.path);
  options.compiledIn = &kMarchingBox;
  sketch::Host host(options, sketch::test::fonts());
  ASSERT_TRUE(host.live());
  const float density = sketch::plateDensity(*host.session());
  EXPECT_EQ(host.prepareCapture(1.0, 60.0, density), 1.0);
  const std::filesystem::path written = file.dir.path / "written.png";
  ASSERT_TRUE(host.writePhotograph(written, density));
  const sketch::testing::Comparison same =
      sketch::testing::compare(written, plate);
  EXPECT_TRUE(same.identical())
      << same.problem << " differing pixels " << same.pixels.differingPixels
      << ", worst " << same.pixels.worst;

  // The protocol's still of the same moment, in process: the density
  // pinned from the first frame, the clock set to Advance, one second
  // stepped and a still taken at two pixels a unit under that moving
  // clock. It is the plate and the written still both.
  std::filesystem::create_directories(file.dir.path / "protocol");
  sketch::testing::InProcessHost session(
      {.stateDirectory = file.dir.path / "protocol"});
  ASSERT_TRUE(session.pinDensity(2.0));
  const auto opened = session.open("written_still_marching_box");
  ASSERT_TRUE(opened) << opened.error().message;
  ASSERT_TRUE(session.clock(protocol::clock::Policy_Advance));
  const auto stepped = session.step(1.0);
  ASSERT_TRUE(stepped) << stepped.error().message;
  const auto taken = session.still(2.0);
  ASSERT_TRUE(taken) << taken.error().message;
  const sketch::testing::Comparison protocolPlate =
      sketch::testing::compare(taken.result().path, plate);
  EXPECT_TRUE(protocolPlate.identical())
      << protocolPlate.problem << " differing pixels "
      << protocolPlate.pixels.differingPixels;
  EXPECT_TRUE(
      sketch::testing::compare(taken.result().path, written).identical());

  // What gives the case its power: the state the last step left, drawn
  // without the runtime's own still, is the scene one frame earlier and
  // another picture.
  ASSERT_TRUE(host.restartSession());
  EXPECT_EQ(host.prepareCapture(1.0, 60.0, density), 1.0);
  const std::filesystem::path held = file.dir.path / "held.png";
  ASSERT_TRUE(host.capture(held, density));
  EXPECT_FALSE(sketch::testing::compare(held, plate).identical());
}

/** A STRIP WIDER THAN HALF THE PLATE CEILING, so the density its plate
 *  is photographed at is the ceiling over its width rather than a whole
 *  number, and its height times that density falls between two pixels. */
struct WideStrip {
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1250, 333);
    ctx.background({0.1f, 0.1f, 0.2f, 1});
    ctx.captureAt(0.5);
  }
  void update(double elapsed, sketch::SketchContext& ctx) {
    using namespace sigil::compose;
    ctx.composer.render(box()
                            .width(40)
                            .height(333)
                            .inset(0, 0, 0, (float)elapsed * 300.0f)
                            .fill(Fill::color({1, 0.5f, 0, 1})));
  }
};

sketch::Kind wideStripKind() { return sketch::kindOf<WideStrip>(); }

[[maybe_unused]] const bool kWideRegistered =
    sketch::add("written_still_wide_strip", nullptr, "Test",
                "a strip whose plate density is fractional", &wideStripKind);

const sketch::Entry kWideStrip{"written_still_wide_strip",
                               "written_still_wide_strip", "Test", "",
                               &wideStripKind};

TEST(SketchWrittenStill, AtAFractionalDensityIsThePlatesOwnPixels) {
  const sketch::test::Watched file("sigil_sketch_written_still_fractional");
  sketch::SweepOptions sweep;
  sweep.outputDirectory = (file.dir.path / "sweep").string();
  sweep.only = sketch::find("written_still_wide_strip");
  sweep.ledger = true;
  ASSERT_GE(sweep.only, 0);
  ASSERT_EQ(sketch::sweep(sweep, sketch::test::fonts(), sketch::test::assets()),
            0);
  const std::filesystem::path plate =
      file.dir.path / "sweep" / "plate_written_still_wide_strip.png";

  sketch::Host::Options options = optionsFor(file.path);
  options.compiledIn = &kWideStrip;
  sketch::Host host(options, sketch::test::fonts());
  ASSERT_TRUE(host.live());
  const float density = sketch::plateDensity(*host.session());
  // What gives the case its power: the density is not whole, and the
  // canvas's height at it is not a whole number of pixels.
  ASSERT_NE(density, std::floor(density));
  ASSERT_NE(333.0f * density, std::floor(333.0f * density));
  const sketch::PlateExtent extent = sketch::plateExtent(1250, 333, density);
  EXPECT_EQ(extent.height, (int)std::floor(333.0f * density));

  EXPECT_EQ(host.prepareCapture(0.5, 60.0, density), 0.5);
  const std::filesystem::path written = file.dir.path / "written.png";
  ASSERT_TRUE(host.writePhotograph(written, density));
  const sketch::testing::Comparison same =
      sketch::testing::compare(written, plate);
  EXPECT_TRUE(same.identical())
      << same.problem << " differing pixels " << same.pixels.differingPixels;

  std::filesystem::create_directories(file.dir.path / "protocol");
  sketch::testing::InProcessHost session(
      {.stateDirectory = file.dir.path / "protocol"});
  ASSERT_TRUE(session.pinDensity());
  ASSERT_TRUE(session.open("written_still_wide_strip"));
  ASSERT_TRUE(session.clock(protocol::clock::Policy_Advance));
  ASSERT_TRUE(session.step(0.5));
  const auto taken = session.still(density);
  ASSERT_TRUE(taken) << taken.error().message;
  EXPECT_EQ(taken.result().height, (unsigned)extent.height);
  EXPECT_TRUE(sketch::testing::compare(taken.result().path, plate).identical());
}

TEST(SketchWrittenStill, RefusesADensityWithNoPixels) {
  const sketch::test::Watched file("sigil_sketch_written_still_density");
  sketch::Host host(optionsFor(file.path), sketch::test::fonts());
  for (float density : {0.0f, -1.0f, std::numeric_limits<float>::infinity()})
    EXPECT_THROW(host.prepareCapture(0, 60.0, density), std::invalid_argument);
}

}  // namespace
