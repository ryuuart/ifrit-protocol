/** @file
 * LIGHTS WITH A SIZE, held to what they mean on the device: a rect light
 * lights the plate beneath it and nothing behind its face; a mirror shows
 * its outline, wide for a wide panel and turning with its node; a larger
 * sphere at the same intensity spreads a wider, dimmer highlight; a light
 * link narrows a sized light as any other; and the sized path leaves a
 * picture lit only by a point light exactly as it was.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/World.h>

#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;
using ::sigil::world::diligent::at;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr glm::ivec2 kExtent{160, 160};

/** @p content seen square on from the level eye, photographed. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime) {
  content.push_back(camera(diligent::levelEye(),
                           {.exposure = 1.0f, .toneCurve = ToneCurve::Linear})
                        .key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return diligent::photograph(frame, runtime, kExtent);
}

float luma(SkColor4f c) { return c.fR * 0.3f + c.fG * 0.59f + c.fB * 0.11f; }

material::Material matte() {
  return material::surface::program(
      {.baseColor = {0.8f, 0.8f, 0.8f, 1}, .roughness = 1.0f});
}

/** A white metal smooth enough to mirror. */
material::Material mirror(float roughness) {
  material::surface::SurfaceParameters parameters;
  parameters.baseColor = {1, 1, 1, 1};
  parameters.metallic = 1.0f;
  parameters.roughness = roughness;
  return material::surface::program(parameters);
}

/** A card facing the camera, wearing @p surface. */
Node card(const material::Material& surface, float size = 120) {
  return mesh(gm::quad(size, size)).key("card").fill(surface);
}

/** How many pixels along the row (or column) through the middle are
 *  brighter than half the middle's own brightness. */
int brightAcross(const SkBitmap& plate, bool row) {
  const float middle = luma(at(plate, 0.5f, 0.5f));
  int count = 0;
  for (int i = 0; i < kExtent.x; ++i) {
    const float t = (i + 0.5f) / (float)kExtent.x;
    const SkColor4f c = row ? at(plate, t, 0.5f) : at(plate, 0.5f, t);
    if (luma(c) > middle * 0.5f) ++count;
  }
  return count;
}

}  // namespace

TEST(AreaLight, ARectLightsThePlateBeneathItMostAndFallsOff) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate = photographed(
      {rectLight({0, 0, -1}, {.width = 40, .height = 40, .intensity = 1.0f})
           .key("panel")
           .at({0, 0, 20}),
       card(matte())},
      on.runtime);
  const float beneath = luma(at(plate, 0.5f, 0.5f));
  const float aside = luma(at(plate, 0.8f, 0.5f));
  const float corner = luma(at(plate, 0.85f, 0.85f));
  EXPECT_GT(beneath, 0.1f);
  EXPECT_GT(beneath, aside * 1.5f);
  EXPECT_GT(aside, corner);
}

TEST(AreaLight, NothingBehindAPanelsFaceIsLit) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // Each panel stands in front of the card with its face turned away.
  for (Light panel :
       {rectLight({0, 0, 1}, {.width = 40, .height = 40, .intensity = 4.0f}),
        diskLight({0, 0, 1}, {.radius = 20, .intensity = 4000.0f})}) {
    const SkBitmap plate = photographed(
        {panel.key("panel").at({0, 0, 20}), card(matte())}, on.runtime);
    EXPECT_LT(luma(at(plate, 0.5f, 0.5f)), 1.5f / 255.0f);
    EXPECT_LT(luma(at(plate, 0.2f, 0.3f)), 1.5f / 255.0f);
  }
  // Turned toward it, the disk lights it.
  const SkBitmap lit =
      photographed({diskLight({0, 0, -1}, {.radius = 20, .intensity = 400.0f})
                        .key("panel")
                        .at({0, 0, 20}),
                    card(matte())},
                   on.runtime);
  EXPECT_GT(luma(at(lit, 0.5f, 0.5f)), 0.1f);
}

TEST(AreaLight, AMirrorShowsTheRectsOutlineAndItTurnsWithItsNode) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto under = [&](float turn) {
    return photographed(
        {rectLight({0, 0, -1}, {.width = 60, .height = 12, .intensity = 0.8f})
             .key("panel")
             .at({0, 0, 100})
             .rotateZ(turn),
         card(mirror(0.05f))},
        on.runtime);
  };
  const SkBitmap level = under(0.0f);
  const SkBitmap turned = under(90.0f);
  EXPECT_GT(luma(at(level, 0.5f, 0.5f)), 0.3f);
  const int wide = brightAcross(level, true);
  const int tall = brightAcross(level, false);
  EXPECT_GT(wide, tall * 3) << wide << " across, " << tall << " down";
  const int turnedWide = brightAcross(turned, true);
  const int turnedTall = brightAcross(turned, false);
  EXPECT_GT(turnedTall, turnedWide * 3)
      << turnedWide << " across, " << turnedTall << " down";
}

TEST(AreaLight, ALargerSphereSpreadsAWiderDimmerHighlight) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The same intensity — the same light — from a small ball and a large
  // one.
  const auto under = [&](float radius) {
    return photographed({sphereLight({.radius = radius,
                                      .intensity = intensityAt(0.002f, 100.0f)})
                             .key("ball")
                             .at({0, 0, 100}),
                         card(mirror(0.15f))},
                        on.runtime);
  };
  const SkBitmap small = under(3.0f);
  const SkBitmap large = under(20.0f);
  EXPECT_GT(luma(at(small, 0.5f, 0.5f)), luma(at(large, 0.5f, 0.5f)) * 1.5f);
  const int smallSpread = brightAcross(small, true);
  const int largeSpread = brightAcross(large, true);
  EXPECT_GT(largeSpread, smallSpread * 2)
      << largeSpread << " pixels against " << smallSpread;
}

TEST(AreaLight, ALightLinkNarrowsASizedLight) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate = photographed(
      {rectLight({0, 0, -1}, {.width = 120, .height = 40, .intensity = 1.0f})
           .key("panel")
           .at({0, 0, 30})
           .lightLink(Collection{.include = {"/left"}}),
       mesh(gm::quad(55, 100)).key("left").at({-32, 0, 0}).fill(matte()),
       mesh(gm::quad(55, 100)).key("right").at({32, 0, 0}).fill(matte())},
      on.runtime);
  EXPECT_GT(luma(at(plate, 0.35f, 0.5f)), 0.1f);
  EXPECT_LT(luma(at(plate, 0.65f, 0.5f)), 1.5f / 255.0f);
}

TEST(AreaLight, APointLitPictureIsUnchangedBesideAPanelThatDoesNotReachIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto lamp = [] {
    return pointLight({.intensity = intensityAt(3.0f, 60.0f)})
        .key("lamp")
        .at({-30, 10, 50});
  };
  const SkBitmap alone = photographed({lamp(), card(mirror(0.4f))}, on.runtime);
  const SkBitmap beside = photographed(
      {lamp(),
       rectLight({0, 0, 1}, {.width = 40, .height = 40, .intensity = 4.0f})
           .key("panel")
           .at({0, 0, 20}),
       card(mirror(0.4f))},
      on.runtime);
  EXPECT_EQ(diligent::worstChannel(alone, beside), 0);
}

TEST(AreaLight, ANodesScaleSizesThePanelItHolds) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A panel forty units a side under a group scaled twice over lights the
  // plate as a panel eighty a side does.
  const SkBitmap scaled = photographed(
      {group({rectLight({0, 0, -1},
                        {.width = 40, .height = 40, .intensity = 1.0f})
                  .key("panel")})
           .key("rig")
           .at({0, 0, 30})
           .scale(2),
       card(matte())},
      on.runtime);
  const SkBitmap stated = photographed(
      {rectLight({0, 0, -1}, {.width = 80, .height = 80, .intensity = 1.0f})
           .key("panel")
           .at({0, 0, 30}),
       card(matte())},
      on.runtime);
  EXPECT_LE(diligent::worstChannel(scaled, stated), 1);
  const SkBitmap unscaled = photographed(
      {rectLight({0, 0, -1}, {.width = 40, .height = 40, .intensity = 1.0f})
           .key("panel")
           .at({0, 0, 30}),
       card(matte())},
      on.runtime);
  EXPECT_GT(diligent::worstChannel(scaled, unscaled), 8)
      << "a larger panel lights the plate otherwise";
}
