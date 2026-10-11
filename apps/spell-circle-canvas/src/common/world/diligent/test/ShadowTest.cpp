/** @file
 * SHADOWS, held to what they mean on the device: a light that casts
 * darkens what stands behind a body from it and nothing beside; a light
 * that does not, a body that opts out of casting, a body that opts out of
 * receiving and a body its light's shadow link leaves out each take their
 * shadow away; a spot light's shadow stays in its cone and follows it; a
 * shadow takes only its own light's share; a flat plate grazed by the
 * light is not striped by its own map; every instance of instanced
 * geometry casts; and a frame with no light that casts draws no map.
 *
 * The scene is a ground plate seen from above and in front, a card held
 * level over it, and lights from above. Every case reads the effect off
 * the pixels where the projection puts a point of the ground, so what is
 * asserted is where a shadow falls and never a number a rasteriser
 * happens to produce.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilworld/World.h>
#include <sigilworld/scene/Scene.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr glm::ivec2 kExtent{200, 200};

float luma(SkColor4f c) { return c.fR * 0.3f + c.fG * 0.59f + c.fB * 0.11f; }

material::Material matte(float grey) {
  return material::surface::program(
      {.baseColor = {grey, grey, grey, 1}, .roughness = 1.0f});
}

/** A ground plate three hundred units across, facing up. */
Mesh ground() {
  return mesh(gm::quad(300, 300)).key("ground").rotateX(-90).fill(matte(0.8f));
}

/** A card forty units across, held level forty units over the ground's
 *  middle, in the collection "props". */
Mesh card() {
  return mesh(gm::quad(40, 40))
      .key("card")
      .collection("props")
      .rotateX(-90)
      .at({0, 40, 0})
      .fill(matte(0.8f));
}

/** A sun straight overhead. */
DirectionalLightOptions sun(bool casts) {
  return {.intensity = 3.0f, .castShadow = casts};
}

/** @p content photographed from above and in front, read without a curve
 *  so a sum of lights is a sum of codes before the encoding. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime,
                      gm::camera::Camera eye = diligent::raisedEye()) {
  content.push_back(
      camera(eye, {.exposure = 1.0f, .toneCurve = ToneCurve::Linear})
          .key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return diligent::photograph(frame, runtime, kExtent);
}

/** The pixel the ground point @p x, @p z is seen at. */
SkColor4f groundAt(const SkBitmap& plate, float x, float z) {
  const std::optional<glm::vec2> seen = diligent::raisedEye().project(
      {x, 0, z}, glm::vec2((float)kExtent.x, (float)kExtent.y));
  if (!seen) return {0, 0, 0, 0};
  const int px = std::clamp((int)seen->x, 0, plate.width() - 1);
  const int py = std::clamp((int)seen->y, 0, plate.height() - 1);
  return plate.getColor4f(px, py);
}

float groundLuma(const SkBitmap& plate, float x, float z) {
  return luma(groundAt(plate, x, z));
}

}  // namespace

TEST(Shadow, ACardDarkensThePlateBeneathItAndNotBesideIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate = photographed(
      {directionalLight({0, -1, 0}, sun(true)).key("sun"), ground(), card()},
      on.runtime);
  const float beside = groundLuma(plate, 80, 0);
  EXPECT_GT(beside, 0.3f);
  EXPECT_LT(groundLuma(plate, 0, 0), beside * 0.1f) << "beneath the card";
  // Its edges stand where the card's do: just outside them is lit.
  EXPECT_GT(groundLuma(plate, 30, 0), beside * 0.9f);
  EXPECT_GT(groundLuma(plate, 0, 30), beside * 0.9f);
}

TEST(Shadow, ALightThatDoesNotCastLeavesNoShadow) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate = photographed(
      {directionalLight({0, -1, 0}, sun(false)).key("sun"), ground(), card()},
      on.runtime);
  EXPECT_NEAR(groundLuma(plate, 0, 0), groundLuma(plate, 80, 0), 1.5f / 255.0f);
  // …and a light that casts over bodies none of which cast is the very
  // picture of one that does not: the shadow's share of an unshadowed
  // light is exactly all of it.
  const SkBitmap nothingCasts =
      photographed({directionalLight({0, -1, 0}, sun(true)).key("sun"),
                    ground().castShadow(false), card().castShadow(false)},
                   on.runtime);
  EXPECT_EQ(diligent::worstChannel(plate, nothingCasts), 0);
}

TEST(Shadow, ABodyThatDoesNotCastLeavesNoShadow) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate =
      photographed({directionalLight({0, -1, 0}, sun(true)).key("sun"),
                    ground(), card().castShadow(false)},
                   on.runtime);
  EXPECT_NEAR(groundLuma(plate, 0, 0), groundLuma(plate, 80, 0), 1.5f / 255.0f);
}

TEST(Shadow, ABodyThatDoesNotReceiveIsNotDarkened) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate =
      photographed({directionalLight({0, -1, 0}, sun(true)).key("sun"),
                    ground().receiveShadow(false), card()},
                   on.runtime);
  EXPECT_NEAR(groundLuma(plate, 0, 0), groundLuma(plate, 80, 0), 1.5f / 255.0f);
}

TEST(Shadow, AShadowLinkLeavingTheCardOutTakesOnlyThatLightsShadowAway) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // One sun overhead, whose shadow falls beneath the card; a second slanted
  // so its shadow falls sixty units to the left.
  const auto set = [](Light overhead) {
    return std::vector<Node>{
        std::move(overhead),
        directionalLight({-1.5f, -1, 0}, sun(true)).key("slant"), ground(),
        card()};
  };
  const SkBitmap linked = photographed(
      set(directionalLight({0, -1, 0}, sun(true)).key("sun")), on.runtime);
  const SkBitmap unlinked =
      photographed(set(directionalLight({0, -1, 0}, sun(true))
                           .key("sun")
                           .shadowLink(Collection{.exclude = {"props"}})),
                   on.runtime);
  const float bothLit = groundLuma(linked, 80, 0);
  // Linked, the overhead sun's shadow stands beneath the card…
  EXPECT_LT(groundLuma(linked, 0, 0), bothLit - 0.1f);
  // …and left out of its link, the card casts none from it…
  EXPECT_NEAR(groundLuma(unlinked, 0, 0), bothLit, 2.0f / 255.0f);
  // …while the slanted light still shadows where it did.
  EXPECT_LT(groundLuma(unlinked, -60, 0), bothLit - 0.1f);
  EXPECT_NEAR(groundLuma(unlinked, -60, 0), groundLuma(linked, -60, 0),
              2.0f / 255.0f);
}

TEST(Shadow, ASpotLightsShadowStaysInItsConeAndFollowsIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto lit = [&](glm::vec3 from) {
    return photographed(
        {spotLight({0, -1, 0}, {.intensity = intensityAt(3.0f, 120.0f),
                                .innerAngle = 25,
                                .outerAngle = 35,
                                .castShadow = true})
             .key("spot")
             .at(from),
         ground(), card()},
        on.runtime);
  };
  const SkBitmap above = lit({0, 120, 0});
  // The card's shadow, half as wide again as the card, beneath it; the
  // ground beside it inside the cone lit; outside the cone, dark anyway.
  EXPECT_LT(groundLuma(above, 0, 0), groundLuma(above, 45, 0) * 0.1f);
  EXPECT_GT(groundLuma(above, 45, 0), 0.2f);
  EXPECT_GT(groundLuma(above, -40, 0), 0.2f);
  EXPECT_LT(groundLuma(above, 140, 0), 1.5f / 255.0f);
  // Moved thirty to the right, the light throws the shadow left: where
  // the ground was lit it is shadowed now.
  const SkBitmap moved = lit({30, 120, 0});
  EXPECT_LT(groundLuma(moved, -40, 0), groundLuma(above, -40, 0) * 0.2f);
}

TEST(Shadow, APointLightCastsEveryWayFromWhereItStands) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A lamp low over the ground's middle and a wall standing to its right:
  // the wall's shadow falls on the ground beyond it, read through the
  // lamp's face looking that way.
  const SkBitmap plate = photographed(
      {pointLight({.intensity = intensityAt(3.0f, 40.0f), .castShadow = true})
           .key("lamp")
           .at({0, 20, 0}),
       ground(),
       mesh(gm::quad(60, 60))
           .key("wall")
           .rotateY(-90)
           .at({40, 30, 0})
           .fill(matte(0.8f))},
      on.runtime);
  // The same distance either side of the lamp: lit on the open side, in
  // the wall's shadow beyond it.
  const float open = groundLuma(plate, -70, 0);
  EXPECT_GT(open, 0.1f);
  EXPECT_LT(groundLuma(plate, 70, 0), open * 0.1f);
  // Below the lamp, toward the camera, nothing stands between.
  EXPECT_GT(groundLuma(plate, 0, 30), open);
}

TEST(Shadow, AShadowTakesOnlyItsOwnLightsShare) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto fill = [] {
    return directionalLight({1, -1, 0.5f}, {.intensity = 1.0f}).key("fill");
  };
  const SkBitmap both =
      photographed({directionalLight({0, -1, 0}, sun(true)).key("sun"), fill(),
                    ground(), card()},
                   on.runtime);
  const SkBitmap fillAlone =
      photographed({fill(), ground(), card()}, on.runtime);
  // Beneath the card the sun's share is gone and the fill's is whole.
  const float shadowed = groundLuma(both, 0, 0);
  EXPECT_GT(shadowed, 0.05f);
  EXPECT_NEAR(shadowed, groundLuma(fillAlone, 0, 0), 2.0f / 255.0f);
  EXPECT_GT(groundLuma(both, 80, 0), shadowed * 1.5f);
}

TEST(Shadow, APlateGrazedByTheLightIsNotStripedByItsOwnMap) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // Fifteen degrees above the plate, the plate the only body: it casts on
  // itself, and must not shadow itself.
  const SkBitmap plate =
      photographed({directionalLight({1, -0.27f, 0.2f},
                                     {.intensity = 6.0f, .castShadow = true})
                        .key("sun"),
                    ground()},
                   on.runtime);
  float low = 1.0f;
  float high = 0.0f;
  for (float x = -100; x <= 100; x += 7)
    for (float z = -80; z <= 80; z += 7) {
      const float value = groundLuma(plate, x, z);
      low = std::min(low, value);
      high = std::max(high, value);
    }
  EXPECT_GT(low, 0.1f);
  EXPECT_LT(high - low, 3.0f / 255.0f) << "a flat plate is lit evenly";
}

TEST(Shadow, EveryInstanceOfInstancedGeometryCasts) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  gm::Instances row;
  row.stamp = gm::quad(24, 24);
  for (const float x : {-60.0f, 0.0f, 60.0f})
    row.positions.emplace_back(x, 0, 0);
  // The stamp faces +z; the node lays the whole row level, forty up.
  const SkBitmap plate = photographed(
      {directionalLight({0, -1, 0}, sun(true)).key("sun"), ground(),
       mesh(row).key("row").rotateX(-90).at({0, 40, 0}).fill(matte(0.8f))},
      on.runtime);
  const float lit = groundLuma(plate, 100, 30);
  for (const float x : {-60.0f, 0.0f, 60.0f})
    EXPECT_LT(groundLuma(plate, x, 0), lit * 0.1f) << x;
  for (const float x : {-30.0f, 30.0f})
    EXPECT_GT(groundLuma(plate, x, 0), lit * 0.9f) << x;
}

TEST(Shadow, CascadesSharpenAShadowNearTheCameraOnALongGround) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A low eye at one end of a ground three thousand units long, and a
  // block standing close in front of it whose shadow runs toward the eye: one
  // map spreads its texels over the whole ground, three put the nearest of them
  // where the block is.
  gm::camera::Camera low;
  low.eye = {0, 40, 1400};
  low.target = {0, 0, 1000};
  const auto lit = [&](int cascades) {
    return photographed(
        {directionalLight({0, -1, 0.6f},
                          {.intensity = 3.0f,
                           .castShadow = true,
                           .shadow = {.mapSize = 512, .cascades = cascades}})
             .key("sun"),
         mesh(gm::quad(400, 3000)).key("ground").rotateX(-90).fill(matte(0.8f)),
         mesh(gm::box({-15, 0, -4}, {15, 60, 4}))
             .key("block")
             .at({0, 0, 1250})
             .fill(matte(0.8f))},
        on.runtime, low);
  };
  // The row the ground half way along the shadow is seen on, and the
  // pixels along it that stand part way between the shadowed and the lit
  // level: the width of the shadow's two edges.
  const std::optional<glm::vec2> seen =
      low.project({0, 0, 1272}, glm::vec2((float)kExtent.x, (float)kExtent.y));
  ASSERT_TRUE(seen);
  const int row = (int)seen->y;
  const auto edge = [row](const SkBitmap& plate) {
    float darkest = 1.0f;
    float lightest = 0.0f;
    for (int x = 0; x < plate.width(); ++x) {
      const float value = luma(plate.getColor4f(x, row));
      darkest = std::min(darkest, value);
      lightest = std::max(lightest, value);
    }
    int between = 0;
    for (int x = 0; x < plate.width(); ++x) {
      const float value = luma(plate.getColor4f(x, row));
      const float share =
          (value - darkest) / std::max(lightest - darkest, 1e-3f);
      if (share > 0.1f && share < 0.9f) ++between;
    }
    return std::pair{between, lightest - darkest};
  };
  const auto [single, singleDepth] = edge(lit(1));
  const auto [three, threeDepth] = edge(lit(3));
  EXPECT_GT(singleDepth, 0.1f) << "the block's shadow crosses the row";
  EXPECT_GT(threeDepth, 0.2f);
  EXPECT_LT(three * 2, single)
      << "three cascades: " << three << " pixels of edge, one map: " << single;
}

TEST(Shadow, AFrameWithNoLightThatCastsDrawsNoShadowMap) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto maps = [&](bool casts) {
    motion::Engine engine;
    Scene scene(engine);
    Frame frame(
        group({directionalLight({0, -1, 0}, sun(casts)).key("sun"),
               pointLight({.castShadow = casts}).key("lamp"), ground(), card()})
            .key("set"));
    frame.extent(kExtent)
        .defaultCamera(diligent::raisedEye())
        .runtime(on.runtime)
        .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
    scene.render(frame);
    return scene.stats().shadowMaps;
  };
  EXPECT_EQ(maps(false), 0);
  // The sun's map and the lamp's six faces: two lights cast.
  EXPECT_EQ(maps(true), 2);
}

namespace {

/** Two spot lights with a stated reach, so neither map's fit follows the
 *  scene's bounds: one casting only the card's shadow, one casting every
 *  shadow but the card's. The card stands at @p cardX. */
Frame twoSpots(float cardX, const Runtime& runtime) {
  const auto spot = [](Collection link) {
    return spotLight({0, -1, 0}, {.intensity = intensityAt(3.0f, 120.0f),
                                  .range = 400.0f,
                                  .innerAngle = 25,
                                  .outerAngle = 35,
                                  .castShadow = true})
        .at({0, 120, 0})
        .shadowLink(std::move(link));
  };
  Frame frame(
      group({spot(Collection{.include = {"props"}}).key("card spot"),
             spot(Collection{.exclude = {"props"}}).key("rest spot"), ground(),
             mesh(gm::superellipsoid({20, 20, 20}, 2.0f, 16, 8))
                 .key("block")
                 .at({50, 20, 0})
                 .fill(matte(0.6f)),
             card().at({cardX, 40, 0})})
          .key("set"));
  frame.extent(kExtent)
      .defaultCamera(diligent::raisedEye())
      .runtime(runtime)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

}  // namespace

TEST(Shadow, AStillSetDrawsNoShadowMapOnItsSecondFrame) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  motion::Engine engine;
  Scene scene(engine);
  const auto step = [&] {
    engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
    scene.render(twoSpots(0.0f, on.runtime));
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
    SkCanvas canvas(bitmap);
    draw(scene, canvas);
    return bitmap;
  };
  const SkBitmap first = step();
  EXPECT_EQ(scene.stats().shadowMapsDrawn, 2);
  const SkBitmap second = step();
  // Nothing moved, so both maps stand as the first frame drew them…
  EXPECT_EQ(scene.stats().shadowMapsDrawn, 0);
  // …and the picture is the same picture.
  EXPECT_EQ(diligent::worstChannel(first, second), 0);
}

TEST(Shadow, MovingOneCasterRedrawsOnlyTheMapsItCastsInto) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  motion::Engine engine;
  Scene scene(engine);
  const auto step = [&](float cardX) {
    engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
    scene.render(twoSpots(cardX, on.runtime));
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
    SkCanvas canvas(bitmap);
    draw(scene, canvas);
    return bitmap;
  };
  (void)step(0.0f);
  const SkBitmap moved = step(-30.0f);
  // The card casts only for the card's spot: that map is drawn again and
  // the other is kept.
  EXPECT_EQ(scene.stats().shadowMapsDrawn, 1);
  // A fresh scene drawing the moved scene from nothing is the same picture.
  EXPECT_EQ(diligent::worstChannel(
                moved, diligent::photograph(twoSpots(-30.0f, on.runtime),
                                            on.runtime, kExtent)),
            0);
}

namespace {

/** How wide, in scene units along the ground's x axis, the far edge of
 *  the shadow a card held @p height over the ground's middle throws under
 *  a sun of @p size degrees shining down at forty-five degrees: the run
 *  between a tenth and nine tenths of the way from the shadow's core to
 *  the lit ground beyond it. */
float edgeWidth(const Runtime& runtime, float height, float size) {
  // The edge stands the card's half width past the height along x.
  const float edge = height + 20;
  // Seen from straight above the edge, so a unit of the ground is the
  // same number of pixels everywhere across it.
  gm::camera::Camera above;
  above.eye = {edge, 120, 0};
  above.target = {edge, 0, 0};
  above.up = {0, 0, -1};
  const SkBitmap plate = photographed(
      {directionalLight({1, -1, 0}, {.intensity = 3.0f,
                                     .castShadow = true,
                                     .shadow = {.radius = 0.5f, .size = size}})
           .key("sun"),
       mesh(gm::quad(600, 600)).key("ground").rotateX(-90).fill(matte(0.8f)),
       mesh(gm::quad(40, 40))
           .key("card")
           .rotateX(-90)
           .at({0, height, 0})
           .fill(matte(0.8f))},
      runtime, above);
  const auto at = [&](float x) {
    const std::optional<glm::vec2> seen =
        above.project({x, 0, 0}, glm::vec2((float)kExtent.x, (float)kExtent.y));
    return seen ? luma(plate.getColor4f(
                      std::clamp((int)seen->x, 0, kExtent.x - 1),
                      std::clamp((int)seen->y, 0, kExtent.y - 1)))
                : 0.0f;
  };
  const float dark = at(edge - 10);
  const float lit = at(edge + 25);
  float width = 0;
  for (float x = edge - 10; x < edge + 25; x += 0.25f) {
    const float share = (at(x) - dark) / std::max(lit - dark, 1e-3f);
    if (share > 0.1f && share < 0.9f) width += 0.25f;
  }
  return width;
}

}  // namespace

TEST(Shadow, ASunWithAWidthCastsAWiderEdgeFromAFartherCaster) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const float near = edgeWidth(on.runtime, 10, 10);
  const float far = edgeWidth(on.runtime, 100, 10);
  EXPECT_GT(far, near + 6.0f) << near << " " << far;
  // A sun of no width keeps the filter's own edge at any height.
  EXPECT_NEAR(edgeWidth(on.runtime, 100, 0), edgeWidth(on.runtime, 10, 0),
              2.0f);
}

TEST(Shadow, ALargePanelsShadowIsSharpAtTheContactAndSoftAwayFromIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A tall post standing on the ground under a wide panel off to one
  // side: its shadow runs across the ground away from its foot.
  gm::camera::Camera above;
  above.eye = {0, 260, 0};
  above.target = {0, 0, 0};
  above.up = {0, 0, -1};
  const SkBitmap plate = photographed(
      {rectLight(
           {0.6f, -1, 0},
           {.width = 60, .height = 60, .intensity = 6, .castShadow = true})
           .key("panel")
           .at({-120, 200, 0}),
       ground(),
       mesh(gm::box({-4, 0, -4}, {4, 120, 4})).key("post").fill(matte(0.8f))},
      on.runtime, above);
  const auto at = [&](float x, float z) {
    const std::optional<glm::vec2> seen =
        above.project({x, 0, z}, glm::vec2((float)kExtent.x, (float)kExtent.y));
    return seen ? luma(plate.getColor4f(
                      std::clamp((int)seen->x, 0, kExtent.x - 1),
                      std::clamp((int)seen->y, 0, kExtent.y - 1)))
                : 0.0f;
  };
  // Across the shadow near the post's foot and far along it: how many
  // units of z stand between a tenth and nine tenths of the way from its
  // core to the lit ground beside it.
  const auto across = [&](float x) {
    const float dark = at(x, 0);
    const float lit = at(x, 40);
    float width = 0;
    for (float z = 0; z < 40; z += 0.25f) {
      const float share = (at(x, z) - dark) / std::max(lit - dark, 1e-3f);
      if (share > 0.1f && share < 0.9f) width += 0.25f;
    }
    return width;
  };
  const float foot = across(10);
  const float away = across(60);
  EXPECT_GT(away, foot + 3.0f) << foot << " " << away;
}

TEST(Shadow, ACascadeHoldsStillAsTheCameraTurns) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A coarse map over a reach shorter than the ground, so the fit follows
  // what the camera sees and a texel is wide enough to see move.
  const auto edgeSeenBy = [&](glm::vec3 target) {
    gm::camera::Camera eye;
    eye.eye = {0, 150, 250};
    eye.target = target;
    const SkBitmap plate = photographed(
        {directionalLight(
             {1, -1, 0},
             {.intensity = 3.0f,
              .castShadow = true,
              .shadow = {.radius = 1.0f, .mapSize = 128, .distance = 400}})
             .key("sun"),
         mesh(gm::quad(2000, 2000))
             .key("ground")
             .rotateX(-90)
             .fill(matte(0.8f)),
         card()},
        on.runtime, eye);
    // The ground's light at a point, read between the four pixels round
    // where it is seen.
    const auto at = [&](float x) {
      const std::optional<glm::vec2> seen =
          eye.project({x, 0, 0}, glm::vec2((float)kExtent.x, (float)kExtent.y));
      if (!seen) return 0.0f;
      const glm::vec2 p = *seen - glm::vec2(0.5f);
      const int x0 = std::clamp((int)std::floor(p.x), 0, kExtent.x - 2);
      const int y0 = std::clamp((int)std::floor(p.y), 0, kExtent.y - 2);
      const glm::vec2 f = glm::clamp(p - glm::vec2(x0, y0), 0.0f, 1.0f);
      const float top = luma(plate.getColor4f(x0, y0)) * (1 - f.x) +
                        luma(plate.getColor4f(x0 + 1, y0)) * f.x;
      const float bottom = luma(plate.getColor4f(x0, y0 + 1)) * (1 - f.x) +
                           luma(plate.getColor4f(x0 + 1, y0 + 1)) * f.x;
      return top * (1 - f.y) + bottom * f.y;
    };
    // Where along x, past the card's far edge, the shadow rises through
    // half of the lit ground beyond it.
    const float dark = at(30);
    const float lit = at(70);
    float before = at(30);
    for (float x = 30.1f; x < 70; x += 0.1f) {
      const float here = at(x);
      if ((before - dark) < 0.5f * (lit - dark) &&
          (here - dark) >= 0.5f * (lit - dark))
        return x;
      before = here;
    }
    return 0.0f;
  };
  const float ahead = edgeSeenBy({0, 0, 0});
  const float turned = edgeSeenBy({14, 0, 0});
  EXPECT_GT(ahead, 35.0f);
  EXPECT_NEAR(turned, ahead, 0.3f) << "the edge stands where it stood";
}

TEST(Shadow, AGreenGlassCastsAGreenShadow) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The sun shines down at forty-five degrees through a green pane held
  // over the ground, so its shadow falls beside it, where the eye sees the
  // ground directly.
  const auto litGround = [&](bool pane) {
    std::vector<Node> content{
        directionalLight({1, -1, 0}, sun(true)).key("sun"), ground()};
    if (pane)
      content.push_back(
          mesh(gm::quad(40, 40))
              .key("pane")
              .rotateX(-90)
              .at({0, 40, 0})
              .fill(material::from(material::Color{0.2f, 0.9f, 0.2f, 1})
                        .surface({.roughness = 0.1f,
                                  .transmission = 1.0f,
                                  .ior = 1.5f})));
    return photographed(content, on.runtime);
  };
  const SkBitmap through = litGround(true);
  const SkColor4f behind = groundAt(through, 40, 0);
  const SkColor4f beside = groundAt(through, 40, 60);
  EXPECT_LT(luma(behind), luma(beside) * 0.95f) << "darker behind the pane";
  EXPECT_GT(behind.fG, behind.fR + 0.1f) << "and greener than red";
  EXPECT_GT(luma(behind), luma(beside) * 0.2f) << "but lit through it";
}

TEST(Shadow, APanelCastsAcrossTheWholeOfItsFrontHalf) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A small panel facing straight down, and a post far off to its side —
  // more than sixty degrees from where the panel faces — whose shadow
  // falls further out still.
  gm::camera::Camera above;
  above.eye = {200, 300, 0};
  above.target = {200, 0, 0};
  above.up = {0, 0, -1};
  const SkBitmap plate = photographed(
      {rectLight(
           {0, -1, 0},
           {.width = 4, .height = 4, .intensity = 6000, .castShadow = true})
           .key("panel")
           .at({0, 100, 0}),
       mesh(gm::quad(800, 800)).key("ground").rotateX(-90).fill(matte(0.8f)),
       mesh(gm::box({196, 0, -4}, {204, 40, 4})).key("post").fill(matte(0.8f))},
      on.runtime, above);
  const auto at = [&](float x, float z) {
    const std::optional<glm::vec2> seen =
        above.project({x, 0, z}, glm::vec2((float)kExtent.x, (float)kExtent.y));
    return seen ? luma(plate.getColor4f(
                      std::clamp((int)seen->x, 0, kExtent.x - 1),
                      std::clamp((int)seen->y, 0, kExtent.y - 1)))
                : 0.0f;
  };
  const float beside = at(270, 40);
  EXPECT_GT(beside, 0.02f) << "the ground out there is lit";
  EXPECT_LT(at(270, 0), beside * 0.5f) << "and the post shadows it";
}
