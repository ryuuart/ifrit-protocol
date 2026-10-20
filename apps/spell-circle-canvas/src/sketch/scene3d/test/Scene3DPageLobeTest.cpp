/** @file
 * The optional lobes a scene3d page states reach the shader through its
 * surface: a clear coat and its normal, a specular, and a brushing whose
 * highlight runs along the strip it is painted on.
 *
 * Every case that opens a scene3d sketch draws it on the device and SKIPS where
 * the machine has no Vulkan runtime; its suite carries the `gpu` label.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilcompose/Compose.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

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

/** A page whose one panel states a clear coat of weight @p Coat over a
 *  rough grey base, worn on a card under a lamp standing between the card
 *  and the eye — where a coat mirrors the lamp into a spot and a rough
 *  base spreads it evenly. */
template <int Coat>
struct CoatedPage {
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
    return world::Frame(
               world::group({world::camera(
                                 lens, {.toneCurve = world::ToneCurve::Neutral})
                                 .key("camera"),
                             world::pointLight({.intensity = world::intensityAt(
                                                    3.0f, 150.0f)})
                                 .key("lamp")
                                 .at({0, 0, 150}),
                             world::mesh(gm::quad(120, 90))
                                 .key("card")
                                 .fill(page.surface())})
                   .key("set"))
        .pass(page.render(box().width(64).height(64).fill(
            material::from(material::Color{.3f, .3f, .35f, 1})
                .surface({.roughness = 1.0f,
                          .clearcoat = (float)Coat,
                          .clearcoatRoughness = .08f}))));
  }
};

/** A page whose one panel states a glossy clear coat over a rough grey
 *  base, worn on a card under a lamp standing between the card and the eye
 *  as `CoatedPage` is — the coat's own normal facing the viewer across the
 *  page's left half and, where @p Turned, leaning hard across its right
 *  half. */
template <bool Turned>
struct CoatNormalPage {
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
    const material::Color facing{.5f, .5f, 1, 1};
    const material::Color leaning =
        Turned ? material::Color{.9f, .5f, .6f, 1} : facing;
    const material::Material coatNormal = material::linearGradient(
        {0, 0}, {1, 0},
        {{0.0f, facing}, {0.5f, facing}, {0.5f, leaning}, {1.0f, leaning}});
    return world::Frame(
               world::group({world::camera(
                                 lens, {.toneCurve = world::ToneCurve::Neutral})
                                 .key("camera"),
                             world::pointLight({.intensity = world::intensityAt(
                                                    3.0f, 150.0f)})
                                 .key("lamp")
                                 .at({0, 0, 150}),
                             world::mesh(gm::quad(120, 90))
                                 .key("card")
                                 .fill(page.surface())})
                   .key("set"))
        .pass(page.render(box().width(64).height(64).fill(
            material::from(material::Color{.3f, .3f, .35f, 1})
                .surface({.roughness = 1.0f,
                          .clearcoat = 1.0f,
                          .clearcoatRoughness = .08f,
                          .clearcoatNormal = coatNormal}))));
  }
};

/** A page whose middle is a wide metal strip turned @p Turn degrees
 *  clockwise on the page and brushed along itself, worn on a card under a
 *  lamp standing between the card and the eye. */
template <int Turn>
struct BrushedStripPage {
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
    return world::Frame(
               world::group(
                   {world::camera(lens, {.toneCurve = world::ToneCurve::Linear})
                        .key("camera"),
                    world::pointLight(
                        {.intensity = world::intensityAt(0.5f, 150.0f)})
                        .key("lamp")
                        .at({0, 0, 150}),
                    world::mesh(gm::quad(120, 90))
                        .key("card")
                        .fill(page.surface())})
                   .key("set"))
        .pass(page.render(
            box()
                .width(64)
                .height(64)
                .fill(material::from(material::Color{.05f, .05f, .05f, 1})
                          .surface({.roughness = 1.0f}))
                .children(
                    {box()
                         .absolute()
                         .rect(-28, 12, 120, 40)
                         .rotate((float)Turn)
                         .fill(material::from(material::Color{.9f, .9f, .9f, 1})
                                   .surface(
                                       {.metallic = 1.0f, .roughness = 0.35f})
                                   .brushed({.strength = .9f}))})));
  }
};

/** A PAGE WHOSE CONTENT STATES AN OPTIONAL LOBE, on a card under a lamp
 *  standing between it and the eye: a dielectric whose specular strength is
 *  @p Specular, brushed along the page's width at @p Brushed tenths. */
template <int Specular, int Brushed>
struct LobedPage {
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
    const bool metal = Brushed > 0;
    return world::Frame(
               world::group(
                   {world::camera(lens, {.toneCurve = world::ToneCurve::Linear})
                        .key("camera"),
                    world::pointLight({.intensity = world::intensityAt(
                                           metal ? 0.5f : 1.0f, 150.0f)})
                        .key("lamp")
                        .at({0, 0, 150}),
                    world::mesh(gm::quad(120, 90))
                        .key("card")
                        .fill(page.surface())})
                   .key("set"))
        .pass(page.render(box().width(64).height(64).fill(
            material::from(metal ? material::Color{.9f, .9f, .9f, 1}
                                 : material::Color{.3f, .3f, .35f, 1})
                .surface({.metallic = metal ? 1.0f : 0.0f,
                          .roughness = 0.35f,
                          .anisotropy = Brushed / 10.0f,
                          .specular = (float)Specular}))));
  }
};

TEST_F(Scene3DDoors, APagesClearcoatReachesTheShaderThroughItsSurface) {
  std::unique_ptr<Session> coated =
      kindOf<CoatedPage<1>>()->open(fonts(), assets());
  std::unique_ptr<Session> bare =
      kindOf<CoatedPage<0>>()->open(fonts(), assets());
  const SkBitmap withCoat = shadedFrame(*coated);
  const SkBitmap without = shadedFrame(*bare);
  // The coat the page painted mirrors the lamp where the reflection meets
  // the eye; the bare page shows only its rough base there.
  const SkColor4f centreCoated = withCoat.getColor4f(80, 60);
  const SkColor4f centreBare = without.getColor4f(80, 60);
  EXPECT_GT(centreCoated.fG, centreBare.fG + 0.15f);
  // A little way off the spot the two pages are nearly alike.
  EXPECT_NEAR(withCoat.getColor4f(40, 60).fG, without.getColor4f(40, 60).fG,
              0.08f);
}

TEST_F(Scene3DDoors, APagesCoatNormalReachesTheShaderThroughItsSurface) {
  const SkBitmap level =
      shadedFrame(*kindOf<CoatNormalPage<false>>()->open(fonts(), assets()));
  const SkBitmap turned =
      shadedFrame(*kindOf<CoatNormalPage<true>>()->open(fonts(), assets()));
  // A coat facing the viewer everywhere mirrors the lamp into one small
  // round spot at the card's middle, as bright a step to either side.
  EXPECT_NEAR(level.getColor4f(80 - 2, 60).fG, level.getColor4f(80 + 2, 60).fG,
              0.06f);
  // Where the coat's normal turns, the reflection breaks: the left half of
  // the spot stands as it was and the right half has left the eye.
  EXPECT_NEAR(turned.getColor4f(80 - 2, 60).fG, level.getColor4f(80 - 2, 60).fG,
              0.03f);
  EXPECT_LT(turned.getColor4f(80 + 2, 60).fG,
            level.getColor4f(80 + 2, 60).fG - 0.5f);
}

TEST_F(Scene3DDoors, APagesSpecularReachesTheShaderThroughItsSurface) {
  // The page learns its content states a specular while it paints, after
  // the frame was described; the first frame already shades with it.
  const SkBitmap stock =
      shadedFrame(*kindOf<LobedPage<1, 0>>()->open(fonts(), assets()));
  const SkBitmap none =
      shadedFrame(*kindOf<LobedPage<0, 0>>()->open(fonts(), assets()));
  EXPECT_GT(stock.getColor4f(80, 60).fG, none.getColor4f(80, 60).fG + 0.1f)
      << "with no specular the page's dielectric shows no highlight";
}

TEST_F(Scene3DDoors, APagesBrushingReachesTheShaderThroughItsSurface) {
  const SkBitmap round =
      shadedFrame(*kindOf<LobedPage<1, 1>>()->open(fonts(), assets()));
  const SkBitmap brushed =
      shadedFrame(*kindOf<LobedPage<1, 9>>()->open(fonts(), assets()));
  // Barely brushed, the highlight is nearly round; brushed hard, it runs
  // out along the page's width and not up it.
  EXPECT_NEAR(round.getColor4f(80 + 18, 60).fG,
              round.getColor4f(80, 60 + 18).fG, 0.03f);
  const float along = brushed.getColor4f(80 + 18, 60).fG;
  const float across = brushed.getColor4f(80, 60 + 18).fG;
  EXPECT_GT(along, across + 0.05f);
}

TEST_F(Scene3DDoors, ABrushedStripStretchesItsHighlightAlongTheStrip) {
  const SkBitmap down =
      shadedFrame(*kindOf<BrushedStripPage<45>>()->open(fonts(), assets()));
  const SkBitmap up =
      shadedFrame(*kindOf<BrushedStripPage<-45>>()->open(fonts(), assets()));
  // The page's y runs down the picture, so a strip turned clockwise on
  // the page runs down to the right in it. Its brushing runs along it: a
  // step down that diagonal stays in the highlight and the same step
  // across it, still on the strip, falls out of it. Turned the other way,
  // the strip and its highlight run up to the right. This holds only
  // where the direction map's green is read up the page end to end.
  for (const int step : {8, 12}) {
    EXPECT_GT(down.getColor4f(80 + step, 60 + step).fG,
              down.getColor4f(80 + step, 60 - step).fG + 0.12f)
        << "step " << step;
    EXPECT_GT(up.getColor4f(80 + step, 60 - step).fG,
              up.getColor4f(80 + step, 60 + step).fG + 0.12f)
        << "step " << step;
  }
}

}  // namespace
