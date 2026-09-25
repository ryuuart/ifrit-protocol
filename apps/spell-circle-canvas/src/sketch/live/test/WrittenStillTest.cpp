/** @file
 * A written still at a plate's density is that plate: the host steps a
 * scene to its moment with every raster it bakes pinned to the density
 * the still is taken at, as the sweep pins its plate's, so a pen's canvas
 * formed on the way is drawn on the still's grid rather than magnified
 * into it; and it photographs through the runtime's own still, as the
 * sweep does, so a scene that moves is the plate of the same moment.
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

#include <filesystem>
#include <limits>
#include <stdexcept>

#include "Fixture.h"
#include "support/Fixtures.h"

namespace {

namespace sketch = sigil::sketch;

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

[[maybe_unused]] const bool kMarchingRegistered =
    sketch::add("written_still_marching_box", nullptr, "Test",
                "a box that moves every frame, photographed as a plate",
                &marchingBoxKind);

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

  // What gives the case its power: the state the last step left, drawn
  // without the runtime's own still, is the scene one frame earlier and
  // another picture.
  ASSERT_TRUE(host.restartSession());
  EXPECT_EQ(host.prepareCapture(1.0, 60.0, density), 1.0);
  const std::filesystem::path held = file.dir.path / "held.png";
  ASSERT_TRUE(host.capture(held, density));
  EXPECT_FALSE(sketch::testing::compare(held, plate).identical());
}

TEST(SketchWrittenStill, RefusesADensityWithNoPixels) {
  const sketch::test::Watched file("sigil_sketch_written_still_density");
  sketch::Host host(optionsFor(file.path), sketch::test::fonts());
  for (float density : {0.0f, -1.0f, std::numeric_limits<float>::infinity()})
    EXPECT_THROW(host.prepareCapture(0, 60.0, density), std::invalid_argument);
}

}  // namespace
