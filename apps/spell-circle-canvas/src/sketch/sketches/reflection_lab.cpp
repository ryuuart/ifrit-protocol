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
 * THE SKY IS A NODE, and the node's transform orients it: `rotateY` on
 * the environment node is the whole of turning the sky, and the
 * reflections turn with it while the lights and the bodies stand still.
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
 * the sky is what moves, and a transform is the same transform on
 * either tier.
 *
 * WHAT EACH TIER SAYS. Both tiers compute the same terms — the cosine
 * convolution for what falls on a surface, the split sum for what it
 * mirrors — and they compute them at different RATES: the device per
 * pixel, the host tier once per vertex with Skia interpolating between.
 * A sphere is the shape that shows the difference honestly, because its
 * normal turns continuously and a per-vertex reflection therefore reads
 * as facets exactly where the mesh is coarse. Glass is where the two
 * part company most: refraction is a per-pixel question, so the host
 * tier's glass sphere is its transmitted tint and its rim rather than a
 * picture of the sky bent through it.
 *
 * NO SCREEN-SPACE REFRACTION. Glass here refracts the WORLD — the
 * environment map behind it — and not the other bodies in the row. What
 * lies behind a body on screen is a backdrop pass, and that is a
 * frame-graph subject rather than a shading one.
 */

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Environments.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <glm/vec3.hpp>
#include <string_view>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr float kRadius = 52.0f;
constexpr float kGap = 132.0f;
/** How finely the spheres are tessellated. A reflection is a function of
 *  the normal, and on the host tier the normal is only sampled where a
 *  vertex is — so this count is the resolution of the reflection there,
 *  and it is set high enough that a mirror reads as a mirror rather than
 *  as a faceted ball. */
constexpr int kMeridians = 64;
constexpr int kParallels = 40;

/** One sphere at @p x, wearing @p surface. */
world::Element ball(std::string_view key, float x,
                    material::Material surface) {
  return world::Element()
      .key(key)
      .at({x, 0.0f, 0.0f})
      .mesh(gm::superellipsoid({kRadius, kRadius, kRadius}, 2.0f, kMeridians,
                               kParallels))
      .fill(std::move(surface))
      .tag("ball");
}

/** The four surfaces, left to right: a mirror, a rough metal, a
 *  dielectric and glass. Each is a composition the kit ships, so what
 *  the study varies is the surface and not the way it is spelled. */
world::Element balls() {
  const float left = -1.5f * kGap;
  return world::Element()
      .key("balls")
      .at({0.0f, 26.0f, 0.0f})
      // The row runs across the turntable's parked station rather than
      // along it, so all four are seen face on.
      .rotateY(90.0f)
      .child(ball("chrome", left,
                  material::kit::surface(material::kit::SurfaceParams::chrome())))
      .child(ball("rough", left + kGap,
                  material::kit::surface(material::kit::SurfaceParams::metal(
                      {0.85f, 0.86f, 0.88f, 1}, 0.35f))))
      .child(ball(
          "dielectric", left + 2.0f * kGap,
          material::kit::surface(material::kit::SurfaceParams::dielectric(
              {0.14f, 0.30f, 0.42f, 1}, 0.15f))))
      .child(ball("glass", left + 3.0f * kGap,
                  material::kit::surface(
                      material::kit::SurfaceParams::glass())));
}

}  // namespace

namespace {

struct ReflectionLab final : sketch::Set {
  /** The row and the two panoramas, made once. A panorama is baked
   *  texel by texel and prefiltered into nine levels the first time it
   *  is asked for; building one per frame would bake the same sky sixty
   *  times a second to describe a picture that never changed. */
  world::Element row;
  material::EnvironmentMap studio;
  material::EnvironmentMap sunset;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(880, 520);
    ctx.background({0.02f, 0.024f, 0.035f, 1.0f});
    // Far enough into the turn that the sky has moved off its start and
    // the crossfade is under way, so the plate is a picture of the
    // study rather than of its first frame.
    ctx.captureAt(1.6);
    row = balls();
    // The lower half of a bake is a floor, and a sphere reflects it
    // straight down where nothing interesting is; a flat ground colour
    // there keeps the reflections about the sky.
    studio = material::kit::studioEnvironment(512).withGround(
        {0.06f, 0.065f, 0.08f, 1});
    sunset = material::kit::sunsetEnvironment(512).withGround(
        {0.05f, 0.03f, 0.05f, 1});
  }

  world::Frame describe(float seconds) override {
    // THE SKY, AS A NODE. Its transform is its orientation, so one
    // rotate lane turns every reflection in the set at once; the
    // crossfade runs beside it, from the studio bake to the sunset one.
    world::Environment sky;
    sky.map = studio;
    sky.next = sunset;
    world::Element dome = world::Element()
                       .key("sky")
                       .environmentMap(sky)
                       .rotateY(seconds * 26.0f)
                       // HELD, not ramping. Both maps are sampled and
                       // mixed at this value, which is what the dial
                       // does; moving it with the time would photograph
                       // at two different values on two lanes that
                       // reach their capture by different routes, and
                       // the tiers would stop being comparable pictures
                       // of one description. The SKY'S TURN is what
                       // moves here, and it is a transform.
                       .crossfade(0.45f)
                       // The backdrop is shown, softly, so the row is
                       // read against the sky it is reflecting rather
                       // than against a flat ground.
                       .backdrop(0.85f)
                       .backdropBlur(0.35f);

    world::kit::Set set;
    set.rig.extent = 140.0f;
    // The rig is dimmer than a studio's, because the sky is now most of
    // the light in the set: a key at full strength would wash the
    // reflections out and the study would be about the key.
    set.rig.intensity = 0.45f;
    set.rig.bearing = 62.0f;
    set.rig.elevation = 34.0f;
    set.rig.fill = 0.3f;
    set.rig.back = 0.4f;
    set.ground = 0.0f;
    set.table.radius = 700.0f;
    set.table.height = 250.0f;
    // PARKED, like every lab: the row faces the eye and stays there, so
    // what moves in the picture is the sky and nothing else.
    set.table.period = 0.0f;
    set.table.fovYDeg = 46.0f;
    return world::Frame(world::kit::litSet(
        world::Element().key("study").child(dome).child(row), set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(
    ReflectionLab, "Set",
    "What a body sees past the lights \xe2\x80\x94 chrome, rough metal, a "
    "dielectric and glass under a turning sky that crossfades from a "
    "studio to a sunset")
