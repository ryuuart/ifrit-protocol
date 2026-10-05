/** @file
 * THE LIGHTING MODEL, held to what it means on the device: a light that
 * stands somewhere reaches each pixel at the falloff its distance and its
 * cone give there; colour is decoded into light, lit, curved and encoded
 * once; a clear coat is a second, sharper reflection; ambient and
 * hemisphere lights fill from everywhere; and occlusion darkens only what
 * arrives from everywhere.
 *
 * Every case reads the effect off the pixels of one picture, or of two
 * pictures that differ in one statement, so what is asserted is what a
 * term does and never a number a rasteriser happens to produce.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/World.h>

#include <cmath>
#include <functional>
#include <glm/geometric.hpp>
#include <string>
#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;
using ::sigil::world::diligent::at;

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr glm::ivec2 kExtent{160, 160};

/** A texture of @p width x @p height, drawn by @p paint. */
material::Texture drawnTexture(const std::string& key, int width, int height,
                               const std::function<void(SkCanvas&)>& paint) {
  return material::Texture(
      sigil::media::PixelSource::produce(key, [width, height, paint] {
        sk_sp<SkSurface> surface =
            SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
        surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        paint(*surface->getCanvas());
        return surface->makeImageSnapshot();
      }));
}

/** One flat colour, one texel. */
material::Texture flat(const std::string& key, SkColor4f colour) {
  return drawnTexture(key, 1, 1,
                      [colour](SkCanvas& canvas) { canvas.clear(colour); });
}

/** How the camera reads the picture, for the cases that need a curve
 *  other than the stock one. */
struct Reading {
  float exposure = 1.0f;
  ToneCurve curve = ToneCurve::Linear;
};

/** @p content seen square on from @p eye through a camera that reads it
 *  as @p reading says, photographed. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime,
                      Reading reading = {},
                      gm::camera::Camera eye = diligent::levelEye()) {
  content.push_back(
      camera(eye, {.exposure = reading.exposure, .toneCurve = reading.curve})
          .key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return diligent::photograph(frame, runtime, kExtent);
}

/** A card facing the camera, wearing @p surface. */
Node card(const material::Material& surface, float size = 120) {
  return mesh(gm::quad(size, size)).key("card").fill(surface);
}

float luma(SkColor4f c) { return c.fR * 0.3f + c.fG * 0.59f + c.fB * 0.11f; }

/** The display-encoded value @p code stands for, in linear light. */
float decoded(float code) {
  return code <= 0.04045f ? code / 12.92f
                          : std::pow((code + 0.055f) / 1.055f, 2.4f);
}

/** A grey matte surface: no metal, rough enough that no highlight
 *  stands out of the diffuse. */
material::Material matte(float grey) {
  return material::surface::program(
      {.baseColor = {grey, grey, grey, 1}, .roughness = 1.0f});
}

}  // namespace

TEST(Lighting, APointLightIsBrighterNearItThanFarFromIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A lamp standing just in front of the card's left edge: per pixel,
  // the left of the card is near it and the right far from it.
  const SkBitmap plate =
      photographed({pointLight({.intensity = intensityAt(3.0f, 40.0f)})
                        .key("lamp")
                        .at({-50, 0, 30}),
                    card(matte(0.8f))},
                   on.runtime);
  const float near = luma(at(plate, 0.22f, 0.5f));
  const float far = luma(at(plate, 0.78f, 0.5f));
  EXPECT_GT(near, 0.2f);
  EXPECT_GT(near, far * 2.0f) << "the falloff is read at each pixel";
}

TEST(Lighting, ASpotLightsInsideItsConeAndNotOutsideIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap plate = photographed(
      {spotLight({0, 0, -1}, {.intensity = intensityAt(3.0f, 100.0f),
                              .innerAngle = 5,
                              .outerAngle = 10})
           .key("spot")
           .at({0, 0, 100}),
       card(matte(0.8f))},
      on.runtime);
  // The cone meets the card in a disc under a fifth of its width across.
  EXPECT_GT(luma(at(plate, 0.5f, 0.5f)), 0.3f);
  EXPECT_LT(luma(at(plate, 0.75f, 0.5f)), 1.5f / 255.0f);
  EXPECT_LT(luma(at(plate, 0.5f, 0.25f)), 1.5f / 255.0f);
}

namespace {

/** A grey card whose normal map leans its left half one way and its right
 *  half the other by @p lean in x — positive leans both halves toward the
 *  middle — lit by a point light grazing the card from just in front of
 *  its middle. */
SkBitmap raked(float lean, const Runtime& runtime) {
  material::Material surface = matte(0.8f);
  if (lean != 0.0f) {
    const glm::vec3 left = glm::normalize(glm::vec3(lean, 0, 1));
    const glm::vec3 right = glm::normalize(glm::vec3(-lean, 0, 1));
    surface.slot(
        material::surface::kNormalSlot,
        drawnTexture("world.test.rake." + std::to_string(lean), 2, 1,
                     [left, right](SkCanvas& canvas) {
                       SkPaint paint;
                       paint.setColor4f(
                           {left.x * .5f + .5f, .5f, left.z * .5f + .5f, 1});
                       canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
                       paint.setColor4f(
                           {right.x * .5f + .5f, .5f, right.z * .5f + .5f, 1});
                       canvas.drawRect(SkRect::MakeXYWH(1, 0, 1, 1), paint);
                     }));
  }
  return photographed({pointLight({.intensity = intensityAt(2.0f, 50.0f)})
                           .key("rake")
                           .at({0, 0, 6}),
                       card(surface)},
                      runtime);
}

}  // namespace

TEST(Lighting, ANormalMappedPlateUnderARakingPointLightShowsItsRelief) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap smooth = raked(0.0f, on.runtime);
  const SkBitmap toward = raked(1.0f, on.runtime);
  const SkBitmap away = raked(-1.0f, on.runtime);
  // Half way out, the light rakes the flat card at a grazing angle; a
  // face leaning toward it catches far more and a face leaning away far
  // less — which is the relief, read per pixel.
  for (const float x : {0.3f, 0.7f}) {
    const float flatLuma = luma(at(smooth, x, 0.5f));
    EXPECT_GT(luma(at(toward, x, 0.5f)), flatLuma * 1.5f + 0.02f) << x;
    EXPECT_LT(luma(at(away, x, 0.5f)), flatLuma * 0.7f) << x;
  }
}

TEST(Lighting, AMidGreyComesBackTheMidGreyItWentInAs) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // Emission is the one term no light reaches: a mid-grey texel over a
  // white emission factor, with nothing lighting the card and no curve,
  // is decoded into light, carried as light and encoded again — and comes
  // out the code it went in as.
  material::surface::SurfaceParameters parameters;
  parameters.baseColor = {0, 0, 0, 1};
  parameters.emissive = {1, 1, 1, 1};
  parameters.emissiveStrength = 1;
  material::Material grey = material::surface::program(parameters);
  grey.slot(material::surface::kEmissiveSlot,
            flat("world.test.mid-grey", {0.5f, 0.5f, 0.5f, 1}));
  const SkColor4f centre =
      at(photographed({card(grey)}, on.runtime), 0.5f, 0.5f);
  EXPECT_NEAR(centre.fR, 0.5f, 1.5f / 255.0f);
  EXPECT_NEAR(centre.fG, 0.5f, 1.5f / 255.0f);
  EXPECT_NEAR(centre.fB, 0.5f, 1.5f / 255.0f);
}

TEST(Lighting, DoublingALightMovesAMidGreyByOneStopAndNotByDoublingItsCode) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto lit = [&](float intensity) {
    return at(photographed({directionalLight({-0.3f, -0.2f, -1.0f},
                                             {.intensity = intensity})
                                .key("sun"),
                            card(matte(0.5f))},
                           on.runtime),
              0.5f, 0.5f)
        .fG;
  };
  const float once = lit(1.2f);
  const float twice = lit(2.4f);
  ASSERT_GT(once, 0.1f);
  ASSERT_LT(twice, 0.95f);
  // Twice the light is twice the LIGHT the code stands for…
  EXPECT_NEAR(decoded(twice) / decoded(once), 2.0f, 0.12f);
  // …which on the display is far less than twice the code.
  EXPECT_LT(twice / once, 1.55f);
}

TEST(Lighting, EmissionAboveOneSurvivesToTheToneCurve) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const auto glowing = [&](float strength) {
    material::surface::SurfaceParameters parameters;
    parameters.baseColor = {0, 0, 0, 1};
    parameters.emissive = {1, 1, 1, 1};
    parameters.emissiveStrength = strength;
    return at(photographed({card(material::surface::program(parameters))},
                           on.runtime, {.curve = ToneCurve::Reinhard}),
              0.5f, 0.5f)
        .fG;
  };
  const float one = glowing(1.0f);
  const float four = glowing(4.0f);
  const float sixteen = glowing(16.0f);
  // Each is brighter than the last and none is cut off at white: the
  // curve, not the target, holds what lies above one.
  EXPECT_GT(four, one + 0.05f);
  EXPECT_GT(sixteen, four + 0.02f);
  EXPECT_LT(sixteen, 1.0f);
}

namespace {

/** A dark, fully rough card under a lamp standing between it and the eye,
 *  coated at @p weight with a coat of roughness @p coatRoughness. */
SkBitmap coated(float weight, float coatRoughness, const Runtime& runtime) {
  material::surface::SurfaceParameters parameters;
  parameters.baseColor = {0.25f, 0.3f, 0.45f, 1};
  parameters.roughness = 1.0f;
  parameters.clearcoat = weight;
  parameters.clearcoatRoughness = coatRoughness;
  return photographed({pointLight({.intensity = intensityAt(3.0f, 150.0f)})
                           .key("lamp")
                           .at({0, 0, 150}),
                       card(material::surface::program(parameters))},
                      runtime);
}

}  // namespace

TEST(Lighting, AClearcoatAddsASecondSharperHighlightOverARoughBase) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap bare = coated(0.0f, 0.1f, on.runtime);
  const SkBitmap coat = coated(1.0f, 0.1f, on.runtime);
  // The rough base spreads the lamp evenly; the coat mirrors it into a
  // tight spot where the reflection meets the eye, and almost nothing a
  // little way off it.
  const float bareCentre = luma(at(bare, 0.5f, 0.5f));
  const float coatCentre = luma(at(coat, 0.5f, 0.5f));
  EXPECT_GT(coatCentre, bareCentre + 0.15f);
  const float bareOff = luma(at(bare, 0.75f, 0.5f));
  const float coatOff = luma(at(coat, 0.75f, 0.5f));
  EXPECT_LT(std::abs(coatOff - bareOff), 0.06f)
      << "the coat's highlight is sharp, not a second wash";
  // A coat of no weight is no coat at all, whatever its roughness says.
  EXPECT_EQ(diligent::worstChannel(bare, coated(0.0f, 0.6f, on.runtime)), 0);
  EXPECT_EQ(diligent::worstChannel(bare, coated(0.0f, 0.1f, on.runtime)), 0);
}

namespace {

/** A sphere in front of the camera under @p light alone. */
SkBitmap sphereUnder(Node light, const Runtime& runtime) {
  return photographed(
      {std::move(light), mesh(gm::superellipsoid({60, 60, 60}, 2.0f, 64, 48))
                             .key("ball")
                             .fill(matte(0.8f))},
      runtime);
}

}  // namespace

TEST(Lighting, AnAmbientLightAloneLightsEveryFacingEqually) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap ball =
      sphereUnder(ambientLight({.intensity = 0.6f}).key("fill"), on.runtime);
  // Four points the same angle off the eye: facing up, down, left and
  // right, and all four alike.
  const float up = luma(at(ball, 0.5f, 0.38f));
  const float down = luma(at(ball, 0.5f, 0.62f));
  const float left = luma(at(ball, 0.38f, 0.5f));
  const float right = luma(at(ball, 0.62f, 0.5f));
  EXPECT_GT(up, 0.2f);
  EXPECT_NEAR(up, down, 2.0f / 255.0f);
  EXPECT_NEAR(up, left, 2.0f / 255.0f);
  EXPECT_NEAR(up, right, 2.0f / 255.0f);
  // With nothing but the ambient light, the background stays black.
  EXPECT_LT(luma(at(ball, 0.05f, 0.05f)), 1.0f / 255.0f);
}

TEST(Lighting, AHemisphereLightLightsUpAndDownFacingsDifferently) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  const SkBitmap ball =
      sphereUnder(hemisphereLight({.color = material::Color{1, 1, 1, 1},
                                   .groundColor = material::Color{0, 0, 0, 1},
                                   .intensity = 0.8f})
                      .key("sky"),
                  on.runtime);
  // The top of the ball sees the sky, the bottom the ground, and the two
  // sides alike.
  EXPECT_GT(luma(at(ball, 0.5f, 0.3f)), luma(at(ball, 0.5f, 0.7f)) + 0.1f);
  EXPECT_NEAR(luma(at(ball, 0.35f, 0.5f)), luma(at(ball, 0.65f, 0.5f)),
              2.0f / 255.0f);
  // Turned upside down by its node, the sky is below.
  const SkBitmap turned =
      sphereUnder(hemisphereLight({.color = material::Color{1, 1, 1, 1},
                                   .groundColor = material::Color{0, 0, 0, 1},
                                   .intensity = 0.8f})
                      .key("sky")
                      .rotateX(180.0f),
                  on.runtime);
  EXPECT_LT(luma(at(turned, 0.5f, 0.3f)), luma(at(turned, 0.5f, 0.7f)) - 0.1f);
}

namespace {

/** A card whose occlusion map is black on its left half and white on its
 *  right, under @p light alone. */
SkBitmap occludedUnder(Node light, const Runtime& runtime) {
  material::Material surface = matte(0.9f);
  surface.slot(
      material::surface::kOcclusionSlot,
      drawnTexture("world.test.lighting.occlusion", 2, 1, [](SkCanvas& canvas) {
        canvas.clear(SK_ColorWHITE);
        SkPaint paint;
        paint.setColor(SK_ColorBLACK);
        canvas.drawRect(SkRect::MakeXYWH(0, 0, 1, 1), paint);
      }));
  return photographed({std::move(light), card(surface)}, runtime);
}

}  // namespace

TEST(Lighting, OcclusionDarkensLightFromEverywhereAndNotADirectLight) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The points sit at the card's outer edges, where a two-texel map read
  // linearly is still all one texel.
  const SkBitmap filled =
      occludedUnder(ambientLight({.intensity = 0.6f}).key("fill"), on.runtime);
  EXPECT_LT(luma(at(filled, 0.2f, 0.5f)), luma(at(filled, 0.8f, 0.5f)) * 0.4f);
  const SkBitmap direct = occludedUnder(
      directionalLight({0, 0, -1}, {.intensity = 1.5f}).key("sun"), on.runtime);
  EXPECT_NEAR(luma(at(direct, 0.2f, 0.5f)), luma(at(direct, 0.8f, 0.5f)),
              2.0f / 255.0f);
  EXPECT_GT(luma(at(direct, 0.2f, 0.5f)), 0.2f);
}

TEST(Lighting, ASetWithNoAmbientLightAndNoEnvironmentHasBlackShadows) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A sun from behind the card: nothing it does not reach is lit, because
  // nothing the set did not state shines.
  const SkBitmap plate =
      photographed({directionalLight({0, 0, 1}, {.intensity = 2.0f}).key("sun"),
                    mesh(gm::quad(120, 120))
                        .key("card")
                        .fill(matte(0.9f))
                        .backface(material::Backface::Hidden)},
                   on.runtime);
  EXPECT_LT(luma(at(plate, 0.5f, 0.5f)), 1.0f / 255.0f);
}

// ---- a light aimed at a node ------------------------------------------

TEST(Lighting, ALightLookingAtANullLightsTheSideFacingIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // The light is made travelling +x — which would light the ball's left —
  // and stands off to its right, looking at a null where the ball is.
  const auto ball = [&](bool aimed) {
    Light sun = directionalLight({1, 0, 0}, {.intensity = 2.0f})
                    .key("sun")
                    .at({300, 0, 0});
    if (aimed) sun.lookAt("pivot");
    return photographed({std::move(sun), null().key("pivot"),
                         mesh(gm::superellipsoid({60, 60, 60}, 2.0f, 64, 48))
                             .key("ball")
                             .fill(matte(0.8f))},
                        on.runtime);
  };
  const SkBitmap unaimed = ball(false);
  EXPECT_GT(luma(at(unaimed, 0.38f, 0.5f)), luma(at(unaimed, 0.62f, 0.5f)))
      << "as made, the light reaches the left";
  const SkBitmap aimed = ball(true);
  EXPECT_GT(luma(at(aimed, 0.62f, 0.5f)), 0.2f)
      << "aimed at the null, it lights the side facing it";
  EXPECT_LT(luma(at(aimed, 0.38f, 0.5f)), 1.0f / 255.0f)
      << "and the side away from it is dark";
}
