/** @file
 * How the light meets a scene3d page: a flat colour takes the lamp while only
 * emission glows, a page painted past white reads above it and blooms, and a
 * glass region shows the plate behind it bent.
 *
 * Every case that opens a scene3d sketch draws it on the device and SKIPS where
 * the machine has no Vulkan runtime; its suite carries the `gpu` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilcompose/Compose.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <cmath>
#include <cstdlib>
#include <memory>

#include "Scene3DTestSupport.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;
using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::sketch::test::Scene3DDoors;
using sigil::sketch::test::shadedFrame;

/** A page of one colour worn on a card under a single lamp standing off
 *  its left edge, so the card's near side takes more light than its far
 *  side: the colour stated flat when @p Glowing is zero, as the emission
 *  of a black surface when it is one, and stated unlit when it is two. */
template <int Glowing>
struct SideLitPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    const material::Color grey{0.7f, 0.7f, 0.7f, 1};
    const material::Material colour =
        Glowing == 2   ? material::from(grey).surface({.unlit = true})
        : Glowing == 1 ? material::from(material::Color{0, 0, 0, 1})
                             .surface({.roughness = 1.0f,
                                       .emission = grey,
                                       .emissionStrength = 1.0f})
                       : material::Material(grey);
    return world::Frame(
               world::group({world::pointLight({.intensity = world::intensityAt(
                                                    3.0f, 100.0f)})
                                 .key("lamp")
                                 .at({-90, 0, 40}),
                             world::mesh(gm::quad(120, 90))
                                 .key("card")
                                 .fill(page.surface())})
                   .key("set"))
        .pass(page.render(box().width(64).height(64).fill(colour)));
  }
};

/** A PAGE GLOWING PAST WHITE: a card smaller than the picture wearing a
 *  page whose whole face states an emission of four over black, seen
 *  through a camera exposed at a fifth with no curve, so light up to five
 *  reads below white on the plate — under a bloom over light brighter
 *  than one where @p Bloomed. */
template <bool Bloomed>
struct BrightPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    world::Frame frame(
        world::group(
            {world::camera(lens, {.exposure = 0.2f,
                                  .toneCurve = world::ToneCurve::Linear})
                 .key("camera"),
             world::mesh(gm::quad(60, 45)).key("card").fill(page.surface())})
            .key("set"));
    frame.pass(page.render(box().width(64).height(64).fill(
        material::from(material::Color{0, 0, 0, 1})
            .surface({.emission = {1, 1, 1, 1}, .emissionStrength = 4}))));
    if (Bloomed)
      frame.pass(world::bloom({.threshold = 1, .intensity = 1, .radius = 24}));
    return frame;
  }
};

/** A PAGE WITH A GLASS REGION, on a card turned forty degrees before a
 *  plate red on its left and blue on its right, the edge between them
 *  straight behind the card's middle: the page's left quarter is green and
 *  its own colour, and the rest is clear glass of index @p IorHundredths
 *  hundredths, twenty scene units deep. */
template <int IorHundredths>
struct GlassPage {
  Page page;
  void setup(Scene3DContext& ctx) {
    ctx.canvas(160, 120);
    ctx.background({0, 0, 0, 1});
    sigil::geometry::mesh::camera::Camera lens;
    lens.eye = {0, 0, 200};
    lens.target = {0, 0, 0};
    ctx.camera(lens);
    page = ctx.page({64, 64});
  }
  world::Frame describe(sigil::motion::Duration) {
    using namespace sigil::compose;
    namespace material = sigil::material;
    const auto flat = [](material::Color colour) {
      return material::from(colour).surface({.unlit = true});
    };
    const material::Material glass =
        material::from(material::Color{1, 1, 1, 1})
            .surface({.roughness = 0.05f,
                      .transmission = 1.0f,
                      .ior = IorHundredths / 100.0f,
                      .thickness = 20.0f});
    return world::Frame(world::group({world::mesh(gm::quad(100, 150))
                                          .key("red")
                                          .at({-50, 0, -60})
                                          .fill(flat({1, 0, 0, 1})),
                                      world::mesh(gm::quad(100, 150))
                                          .key("blue")
                                          .at({50, 0, -60})
                                          .fill(flat({0, 0, 1, 1})),
                                      world::mesh(gm::quad(120, 90))
                                          .key("card")
                                          .rotateY(40)
                                          .fill(page.surface())})
                            .key("set"))
        .pass(page.render(box().width(64).height(64).children({
            box().absolute().rect(0, 0, 16, 64).fill(flat({0, 1, 0, 1})),
            box().absolute().rect(16, 0, 48, 64).fill(glass),
        })));
  }
};

TEST_F(Scene3DDoors, AFlatColourOnAPageTakesTheLightAndOnlyEmissionGlows) {
  const auto light = [](SkColor colour) {
    return (int)SkColorGetR(colour) + (int)SkColorGetG(colour) +
           (int)SkColorGetB(colour);
  };
  // A colour that states no surface is the page's base colour: the side of
  // the card nearer the lamp reads lighter than the far side.
  std::unique_ptr<Session> flat =
      kindOf<SideLitPage<0>>()->open(fonts(), assets());
  const SkBitmap lit = shadedFrame(*flat);
  EXPECT_GT(light(lit.getColor(40, 60)), light(lit.getColor(120, 60)) + 30);
  // The same colour stated as emission glows alike on both sides.
  std::unique_ptr<Session> glowing =
      kindOf<SideLitPage<1>>()->open(fonts(), assets());
  const SkBitmap glow = shadedFrame(*glowing);
  EXPECT_GT(light(glow.getColor(120, 60)), 90);
  EXPECT_NEAR(light(glow.getColor(40, 60)), light(glow.getColor(120, 60)), 12);
  // Stated unlit, it is its own colour, alike on both sides.
  std::unique_ptr<Session> own =
      kindOf<SideLitPage<2>>()->open(fonts(), assets());
  const SkBitmap unlit = shadedFrame(*own);
  EXPECT_GT(light(unlit.getColor(120, 60)), 90);
  EXPECT_NEAR(light(unlit.getColor(40, 60)), light(unlit.getColor(120, 60)),
              12);
}

TEST_F(Scene3DDoors, APageGlowingPastWhiteReadsAboveWhiteAndBlooms) {
  // The light a plate's pixel stands for, at the camera's fifth.
  const auto light = [](const SkBitmap& plate, int x, int y) {
    const float code = plate.getColor4f(x, y).fG;
    const float decoded = code <= 0.04045f
                              ? code / 12.92f
                              : std::pow((code + 0.055f) / 1.055f, 2.4f);
    return decoded / 0.2f;
  };
  const SkBitmap plain =
      shadedFrame(*kindOf<BrightPage<false>>()->open(fonts(), assets()));
  const SkBitmap bloomed =
      shadedFrame(*kindOf<BrightPage<true>>()->open(fonts(), assets()));
  // The card's face is the four its page states, not the one an eight-bit
  // map would clip it to.
  EXPECT_GT(light(plain, 80, 60), 3.0f);
  EXPECT_LT(light(plain, 80, 60), 5.0f);
  // Above the card the picture is dark until the bloom spreads the light
  // past one out over it.
  EXPECT_LT(light(plain, 80, 30), 0.01f);
  EXPECT_GT(light(bloomed, 80, 30), 0.05f);
}

TEST_F(Scene3DDoors, APagesGlassRegionShowsThePlateBehindItBent) {
  const auto green = [](SkColor c) {
    return SkColorGetG(c) > SkColorGetR(c) + 60 &&
           SkColorGetG(c) > SkColorGetB(c) + 60;
  };
  const auto blue = [](SkColor c) {
    return SkColorGetB(c) > SkColorGetR(c) + 60 &&
           SkColorGetB(c) > SkColorGetG(c) + 60;
  };
  const auto red = [](SkColor c) {
    return SkColorGetR(c) > SkColorGetG(c) + 60 &&
           SkColorGetR(c) > SkColorGetB(c) + 60;
  };
  /** Where along the middle row the plate first reads blue, and how many
   *  pixels of it read green. */
  struct Row {
    int firstBlue = -1;
    int greens = 0;
    int reds = 0;
  };
  const auto across = [&](const SkBitmap& picture) {
    Row row;
    for (int x = 0; x < picture.width(); ++x) {
      const SkColor c = picture.getColor(x, 60);
      if (green(c)) ++row.greens;
      if (red(c)) ++row.reds;
      if (blue(c) && row.firstBlue < 0) row.firstBlue = x;
    }
    return row;
  };
  const SkBitmap bent =
      shadedFrame(*kindOf<GlassPage<133>>()->open(fonts(), assets()));
  const SkBitmap straight =
      shadedFrame(*kindOf<GlassPage<100>>()->open(fonts(), assets()));
  const Row through = across(bent);
  const Row level = across(straight);
  // Outside the glass the card is its own green; inside it the plate shows
  // through, red and then blue, where an opaque page would hide it.
  EXPECT_GT(through.greens, 3);
  ASSERT_GT(through.firstBlue, 0);
  ASSERT_GT(level.firstBlue, 0);
  EXPECT_TRUE(blue(bent.getColor(through.firstBlue + 4, 60)));
  // An index of one sees the plate's edge straight behind the card's
  // middle; the water's index bends it aside.
  EXPECT_NEAR(level.firstBlue, 80, 3);
  EXPECT_GE(std::abs(through.firstBlue - level.firstBlue), 2)
      << "bent at " << through.firstBlue << ", straight at " << level.firstBlue;
}

}  // namespace
