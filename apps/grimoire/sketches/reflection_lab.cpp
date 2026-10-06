/** @file
 * reflection_lab — what a body sees when it looks past the lights.
 *
 * Four spheres stand in a row under a sky. Each is the same shape and a
 * different SURFACE, and each is chosen because the environment map is
 * what makes it legible at all: a polished chrome mirror, a rough metal,
 * a dielectric that picks the sky up only at its rim, and glass, whose
 * transmission, index and thickness finally have something on the other
 * side of them.
 *
 * THE SKY IS THE FRAME'S ENVIRONMENT, and its `rotation` orients it: one
 * dial is the whole of turning the sky, and the reflections turn with it
 * while the lights and the bodies stand still.
 * That is what makes the study readable — a still picture of a mirror
 * says nothing about which way the mirror is facing.
 *
 * TWO SKIES, CROSSFADED. The row stands under a mix of the neutral
 * studio bake and the sunset one, so what every surface reflects is
 * something neither panorama holds. A crossfade samples both maps and
 * mixes; it does not rebuild one, which is what lets it move while the
 * frame is running. It is HELD at one value here rather than ramping,
 * because a dial that moves with the time is photographed at whatever
 * value each lane's own route to the capture reached, and two plates of
 * one description have to be pictures of the same moment. The turn of
 * the sky is what moves.
 *
 * WHAT THE DEVICE SAYS. Under a sky the device evaluates the terms per
 * pixel — the cosine convolution for what falls on a surface, the split
 * sum for what it mirrors — so a sphere, whose normal turns continuously,
 * reads as a curve, and glass is the sky bent through it.
 *
 * GLASS SEES THE SET BEHIND IT, one layer deep: the sky and the ball
 * beside it, bent across the ball's own depth, and the sky alone where
 * the bent ray leaves the picture.
 */

// TAGS: Materials/Lighting

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <glm/vec3.hpp>
#include <string_view>

#include "shapeworks_lab/Environments.h"

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr float kRadius = 52.0f;
constexpr float kGap = 132.0f;

/** A polished mirror: metal, and rough enough to be a real object. Steel
 *  is not a mirror and not white: a slight cool bias. */
material::Material chrome() {
  return material::from(material::Color{0.92f, 0.95f, 1.0f, 1})
      .surface({.metallic = 1.0f, .roughness = 0.04f});
}

/** A metal at @p roughness — the study between a mirror and a matte
 *  casting. */
material::Material metal(material::Color tint, float roughness) {
  return material::from(tint).surface(
      {.metallic = 1.0f, .roughness = roughness});
}

/** A dielectric: not a metal, so it reflects a few per cent head on and
 *  much more at the rim, and keeps its colour in the diffuse. */
material::Material dielectric(material::Color base, float roughness) {
  return material::from(base).surface(
      {.metallic = 0.0f, .roughness = roughness});
}

/** Clear glass: what is behind it, refracted across the ball's depth,
 *  with a reflection over the top, and a faint green cast where the light
 *  has crossed the most of it, which is what soda-lime glass does and
 *  what tells an eye it is glass rather than a hole. */
material::Material glass() {
  return material::from(material::Color{1, 1, 1, 1})
      .surface({.metallic = 0.0f,
                .roughness = 0.02f,
                .transmission = 1.0f,
                .ior = 1.5f,
                .thickness = 2.0f * kRadius,
                .attenuationColor = {0.90f, 0.98f, 0.94f, 1},
                .attenuationDistance = 2.0f * kRadius});
}

/** How finely the spheres are tessellated: enough that the silhouette of
 *  a mirror reads as a ball rather than a polygon. */
constexpr int kMeridians = 64;
constexpr int kParallels = 40;

/** One sphere at @p x, wearing @p surface. */
world::Mesh ball(std::string_view key, float x, material::Material surface) {
  return world::mesh(gm::superellipsoid({kRadius, kRadius, kRadius}, 2.0f,
                                        kMeridians, kParallels))
      .key(key)
      .at({x, 0.0f, 0.0f})
      .fill(std::move(surface))
      .collection("ball");
}

/** The four surfaces, left to right: a mirror, a rough metal, a
 *  dielectric and glass. Each is a composition the kit ships, so what
 *  the study varies is the surface and not the way it is spelled. */
world::Group balls() {
  const float left = -1.5f * kGap;
  return world::group({ball("chrome", left, chrome()),
                       ball("rough", left + kGap,
                            metal({0.85f, 0.86f, 0.88f, 1}, 0.35f)),
                       ball("dielectric", left + 2.0f * kGap,
                            dielectric({0.14f, 0.30f, 0.42f, 1}, 0.15f)),
                       ball("glass", left + 3.0f * kGap, glass())})
      .key("balls")
      .at({0.0f, 26.0f, 0.0f})
      // The row runs across the turntable's parked station rather than
      // along it, so all four are seen face on.
      .rotateY(90.0f);
}

}  // namespace

namespace {

struct ReflectionLab {
  /** The row and the two panoramas, made once. A panorama is baked
   *  texel by texel and prefiltered into nine levels the first time it
   *  is asked for; building one per frame would bake the same sky sixty
   *  times a second to describe a picture that never changed. */
  world::Group row;
  world::EnvironmentMap studio;
  world::EnvironmentMap sunset;

  void setup(sketch::Scene3DContext& ctx) {
    // Far enough into the turn that the sky has moved off its start and
    // the crossfade is under way, so the plate is a picture of the
    // study rather than of its first frame.
    sketch::kit::stage(
        ctx, {.size = {880, 520},
              .captureAt = 1.6,
              .background = material::Color{0.02f, 0.024f, 0.035f, 1.0f}});
    row = balls();
    // The lower half of a bake is a floor, and a sphere reflects it
    // straight down where nothing interesting is; a flat ground colour
    // there keeps the reflections about the sky.
    studio = shapeworks_lab::studioEnvironment(512).withGround(
        {0.06f, 0.065f, 0.08f, 1});
    sunset = shapeworks_lab::sunsetEnvironment(512).withGround(
        {0.05f, 0.03f, 0.05f, 1});
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    // The rig is dimmer than a studio's, because the sky is now most of
    // the light in the scene: a key at full strength would wash the
    // reflections out and the study would be about the key. The
    // turntable is PARKED, like every lab: the row faces the eye and
    // stays there, so what moves in the picture is the sky and nothing
    // else.
    const world::kit::Set set{.rig = {.extent = 140.0f,
                                      .bearing = 62.0f,
                                      .elevation = 34.0f,
                                      .intensity = 1.1f,
                                      .fillIntensity = 0.35f,
                                      .backIntensity = 0.45f},
                              .table = {.radius = 700.0f,
                                        .height = 250.0f,
                                        .period = sigil::motion::Duration(0.0),
                                        .fieldOfView = 32.0f},
                              .ground = 0.0f};
    // THE SKY. Its turn is one dial, so it turns every reflection in the
    // set at once; the crossfade runs beside it, from the studio bake to
    // the sunset one — HELD, not ramping, because both maps are sampled
    // and mixed at this value and the sky's TURN is what moves here. The
    // sky is shown too, softly, so the row is read against the sky it is
    // reflecting rather than against a flat ground.
    return world::Frame(
               world::kit::litSet(world::group({row}).key("study"), set, now))
        .environment(studio, {.intensity = 0.4f,
                              .next = sunset,
                              .crossfade = 0.45f,
                              .rotation = seconds * 26.0f,
                              .shown = 0.85f,
                              .blur = 0.35f});
  }
};

}  // namespace

SIGIL_SKETCH(ReflectionLab, "Scene3D",
             "What a body sees past the lights — chrome, rough metal, a "
             "dielectric and glass under a turning sky that crossfades from a "
             "studio to a sunset")
