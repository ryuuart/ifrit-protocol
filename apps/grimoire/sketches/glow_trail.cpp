/** @file
 * glow_trail — the first thing this library's PASSES made.
 *
 * The scene is drawn once. What is tagged "glow" is then reached three
 * ways, one per realisation the ordering knows: a post pass narrowed to
 * that tag brightens the beads in place through the coverage the
 * geometry pass before it was made to write; a geometry pass narrowed to
 * the same tag draws them alone into a target of their own, which is
 * softened; and the softened result is laid over ITS OWN OUTPUT FROM THE
 * FRAME BEFORE, so the comet drags a decaying tail behind it that no
 * single frame contains.
 *
 * Nothing here states an order. The six passes declare what they read
 * and write, and the graph derives the rest — including the surfaces
 * three of them take turns on.
 */

// TAGS: Motion/Particles, Materials/Compositing

#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/time/Duration.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <cmath>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace arrange = sigil::geometry::arrange;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr float kTwoPi = 6.283185307179586f;
constexpr int kPosts = 16;
constexpr float kRing = 190.0f;
/** Where the scene is watched from. The comet's flakes are turned onto it,
 *  so the same point is both the lens's station and their gaze. */
constexpr glm::vec3 kEye{0.0f, 250.0f, 560.0f};

/** The loop the comet rides: the world kit's wave, a ring standing high
 *  and low by turns, so the tail reads as a curve in space rather than
 *  as an arc on a plane. */
gm::curve::Spline3 loop() {
  return world::kit::wave(
      {.radius = 200.0f, .inner = 120.0f, .high = 70.0f, .low = -50.0f});
}

/** The scene: a ring of posts, a plate under them, and a comet of beads
 *  riding a moving window of the loop. Only the comet is in the "glow"
 *  collection. */
world::Group set(motion::Duration now) {
  const std::vector<glm::vec3> path = loop().sampleArcLength(96);
  const float head = motion::phase(now, motion::Duration(1.0 / 0.42));
  // What stands at every point is a FLAKE, and a flat body reads as the
  // bead it draws only while it faces the viewer — so the direction lane
  // the loop seeded with its tangent is replaced by the gaze, and the
  // square covers the area the bead's disc did. The window rides the
  // loop, so the comet is instanced afresh as it moves.
  const gm::Instances comet =
      gm::pop::on(path)
          .count(600)
          .window(head, 0.22f)
          .spread(7.0f)
          .vary(0.6f, 1.0f)
          .fade({1.0f, 0.78f, 0.38f, 1.0f}, {1.0f, 0.42f, 0.16f, 1.0f})
          .lookAt(kEye)
          .instances(gm::quad(8.9f, 8.9f));

  const world::Group posts =
      world::group(world::each(kPosts, [](std::size_t i) {
        // The angle is wanted too, to turn each post onto its own spoke, so
        // the ring is taken as the two halves rather than as onRing.
        const float angle =
            arrange::along(0.0f, kTwoPi, i, kPosts, arrange::Turn::Closed);
        const SkPoint on = sigil::geometry::path::toSk(
            arrange::onEllipse({0, 0}, {kRing, kRing}, angle));
        return world::mesh(gm::superellipsoid({8.0f, 40.0f, 8.0f}, 6.0f, 10, 6))
            .key("post" + std::to_string(i))
            .at({on.fX, -84.0f, on.fY})
            .rotateY(angle * 57.2957795f)
            .fill(material::Color{0.34f, 0.37f, 0.46f, 1.0f})
            .collection("lit");
      })).key("posts");

  return world::group({world::directionalLight(
                           {-0.4f, -0.85f, -0.35f},
                           {.color = material::Color{0.95f, 0.96f, 1.0f, 1.0f},
                            .intensity = 2.3f})
                           .key("sun"),
                       world::ambientLight({.intensity = 0.07f}).key("fill"),
                       world::mesh(gm::quad(760, 760))
                           .key("plate")
                           .at({0, -120, 0})
                           .rotateX(-90.0f)
                           .fill(material::Color{0.09f, 0.10f, 0.13f, 1.0f})
                           .collection("ground"),
                       posts,
                       world::mesh(comet)
                           .key("comet")
                           .fill(material::Color{1.0f, 0.72f, 0.34f, 1.0f})
                           .collection("glow")})
      .key("set");
}

}  // namespace

namespace {

struct GlowTrail {
  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = {640, 440},
              .captureAt = 1.3,
              .background = material::Color{0.03f, 0.035f, 0.05f, 1.0f}});
    ctx.camera({.eye = kEye, .target = {0.0f, -30.0f, 0.0f}, .fovYDeg = 42.0f});
  }

  world::Frame describe(motion::Duration now) {
    world::Frame frame(set(now));
    frame
        // The whole set, and — because a narrowed post pass follows it —
        // the coverage of what that pass addresses.
        .pass(world::geometryPass("main").writes("colour"))
        // MASK: the beads are lifted where they stand, and nowhere else.
        .pass(world::postPass("hot")
                  .reads("colour")
                  .writes("hot")
                  .only("glow")
                  .levels({.gain = 1.9f,
                           .lift = 0.06f,
                           .tint = {1.0f, 0.86f, 0.66f, 1.0f}}))
        // CULL: the same beads, alone, so what is softened next is the
        // beads rather than the picture.
        .pass(world::geometryPass("beads").only("glow").writes("spark"))
        .pass(
            world::postPass("bloom").reads("spark").writes("bloom").blur(11.0f))
        // …at a strength a tail can be built out of: what accumulates
        // over many frames must be dim in any one of them.
        .pass(world::postPass("ember").reads("bloom").writes("ember").levels(
            {.gain = 0.42f, .tint = {1.0f, 0.80f, 0.52f, 1.0f}}))
        // PREVIOUS: this frame's embers over the tail the frames before
        // it left, at a weight that lets the tail fade out.
        .pass(world::postPass("trail")
                  .reads("ember")
                  .previous("trail")
                  .writes("trail")
                  .composite({.mode = material::BlendMode::PlusLighter,
                              .opacity = 0.88f}))
        .pass(world::postPass("picture")
                  .reads("hot", "trail")
                  .writes("picture")
                  .composite({.mode = material::BlendMode::PlusLighter}));
    return frame;
  }
};

}  // namespace

SIGIL_SKETCH(GlowTrail, "Scene3D",
             "The passes, made visible — one scene reached three ways: a "
             "masked grade where the beads stand, the same beads culled "
             "into a target of their own and softened, and that laid over "
             "its own output from the frame before")
