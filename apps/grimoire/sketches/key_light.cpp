/** @file
 * key_light — a light bound to live values, and what it reaches.
 *
 * One set under the kit's three-point rig, with the KEY LIGHT'S strength
 * and colour bound to live values while the tree that declares them
 * stands still. Nothing about the description changes from frame to
 * frame: what moves is what the light's values are bound to, which is the
 * second of this library's two write paths.
 *
 * The subject is a ring of posts round a body, so the falloff has
 * something to fall across and the rig's fill and back lights have
 * somewhere to be seen. A body is shaded by the lights the frame
 * gathered, and the gather happens on the walk that visits every node —
 * so a light that ramps stales no cache, which is why the set can be as
 * still as it looks and still change colour.
 */

// TAGS: Materials/Lighting

#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/World.h>

#include <cmath>
#include <string>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace arrange = sigil::geometry::arrange;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr float kTwoPi = 6.283185307179586f;
constexpr int kPosts = 9;
constexpr float kRing = 150.0f;

/** A ring of posts round one body — the same tree at every moment. */
world::Group subject() {
  return world::group(
             {world::mesh(gm::superellipsoid({46, 46, 46}, 3.0f, 28, 18))
                  .key("body")
                  .at({0.0f, 34.0f, 0.0f})
                  .fill(material::surface::program(
                      {.baseColor = {0.72f, 0.70f, 0.66f, 1.0f},
                       .roughness = 0.45f}))
                  .collection("lit"),
              world::each(
                  kPosts,
                  [](std::size_t i) {
                    // The angle is wanted too, to turn each post onto
                    // its own spoke, so the ring is taken as the two
                    // halves rather than as onRing.
                    const float angle = arrange::along(0.0f, kTwoPi, i, kPosts,
                                                       arrange::Turn::Closed);
                    const SkPoint on = sigil::geometry::path::toSk(
                        arrange::onEllipse({0, 0}, {kRing, kRing}, angle));
                    return world::mesh(
                               gm::superellipsoid({11, 52, 11}, 5.0f, 12, 8))
                        .key("post" + std::to_string(i))
                        .at({on.fX, 0.0f, on.fY})
                        .rotateY(angle * 57.2957795f)
                        .fill(material::Color{0.36f, 0.38f, 0.44f, 1.0f})
                        .collection("lit");
                  })})
      .key("subject");
}

struct KeyLight {
  /** WHAT THE KEY LIGHT IS BOUND TO. Live values the study drives from
   *  the scene time, held for as long as the study is, because a light
   *  reads the value rather than copying it. */
  sigil::motion::Animatable<float> strength = sigil::motion::animatable(1.0f);
  sigil::motion::Animatable<material::Color> tint =
      sigil::motion::animatable(material::Color{1, 1, 1, 1});

  void setup(sketch::SetContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = {760, 500},
              .captureAt = 1.6,
              .background = material::Color{0.028f, 0.032f, 0.042f, 1.0f}});
  }

  world::Frame describe(sigil::motion::Duration now) {
    // The values the key reads, written from the scene time — so the
    // plate is a function of the declared moment and of nothing else.
    const float swing = 0.5f + 0.5f * std::sin((float)now.count() * 2.1f);
    strength = 0.9f + 2.9f * swing;
    tint = material::Color{1.0f, 0.62f + 0.38f * swing,
                           0.35f + 0.65f * (1.0f - swing), 1.0f};
    return world::kit::litSet(
        subject(),
        {.rig = {.extent = 150.0f,
                 .bearing = -50.0f,
                 .elevation = 26.0f,
                 .intensity = strength,
                 .color = tint,
                 .fillIntensity = 0.75f,
                 .backIntensity = 1.3f,
                 .ambientIntensity = 0.05f},
         .table = {.radius = 560.0f,
                   .height = 250.0f,
                   .period = sigil::motion::Duration(16.0),
                   .fieldOfView = 44.0f},
         .ground = 5.0f,
         .drop = 0.36f},
        now);
  }
};

}  // namespace

SIGIL_SKETCH(KeyLight, "Set",
             "A light bound to live values — one still set whose key light's "
             "strength and colour follow the scene time, so what moves is "
             "the value and never the description")
