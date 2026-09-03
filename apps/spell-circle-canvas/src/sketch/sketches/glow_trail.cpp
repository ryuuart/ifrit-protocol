/** @file
 * glow_trail — the first thing this library's PASSES made.
 *
 * The set is drawn once. What is tagged "glow" is then reached three
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

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <glm/vec3.hpp>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr float kTwoPi = 6.283185307179586f;
constexpr int kPosts = 16;
constexpr float kRing = 190.0f;
/** Where the set is watched from. The comet's flakes are turned onto it,
 *  so the same point is both the lens's station and their gaze. */
constexpr glm::vec3 kEye{0.0f, 250.0f, 560.0f};

/** The loop the comet rides: the world kit's wave, a ring standing high
 *  and low by turns, so the tail reads as a curve in space rather than
 *  as an arc on a plane. */
gm::curve::Spline3 loop() {
  return world::kit::wave(
      {.radius = 200.0f, .inner = 120.0f, .high = 70.0f, .low = -50.0f});
}

/** The set: a ring of posts, a plate under them, and a comet of beads
 *  riding a moving window of the loop. Only the comet answers to
 *  "glow". */
world::Element set(float seconds) {
  const gm::curve::Spline3 rail = loop();
  const std::vector<glm::vec3> path = rail.sampleArcLength(96);
  const float head = motion::phase(seconds, 1.0 / 0.42);
  // What stands at every point is a FLAKE, and a flat body reads as the
  // bead it draws only while it faces the viewer — so the direction lane
  // the loop seeded with its tangent is replaced by the gaze, and the
  // square covers the area the bead's disc did.
  const gm::pop::Chain comet =
      gm::pop::on(path)
          .count(600)
          .spread(7.0f)
          .vary(0.6f, 1.0f)
          .fade({1.0f, 0.78f, 0.38f, 1.0f}, {1.0f, 0.42f, 0.16f, 1.0f})
          .lookAt(kEye);

  world::Element root;
  root.key("set")
      .child(world::Element().key("sun").light(world::sun(
          {-0.4f, -0.85f, -0.35f}, {0.95f, 0.96f, 1.0f, 1.0f}, 0.9f)))
      .child(world::Element()
                 .key("plate")
                 .at({0, -120, 0})
                 .rotateX(-90.0f)
                 .mesh(gm::quad(760, 760))
                 .fill(material::kit::surface(
                     {.baseColor = {0.09f, 0.10f, 0.13f, 1.0f}}))
                 .tag("ground"));

  world::Element posts;
  posts.key("posts");
  for (int i = 0; i < kPosts; ++i) {
    const float angle = (float)i * kTwoPi / (float)kPosts;
    posts.child(
        world::Element()
            .key("post" + std::to_string(i))
            .at({kRing * std::cos(angle), -84.0f, kRing * std::sin(angle)})
            .rotateY(angle * 57.2957795f)
            .mesh(gm::superellipsoid({8.0f, 40.0f, 8.0f}, 6.0f, 10, 6))
            .fill(material::kit::surface(
                {.baseColor = {0.34f, 0.37f, 0.46f, 1.0f}}))
            .tag("lit"));
  }
  root.child(std::move(posts));

  root.child(world::Element()
                 .key("comet")
                 .chain(comet)
                 .stamp(gm::quad(8.9f, 8.9f))
                 .window(head, 0.22f)
                 .fill(material::kit::surface(
                     {.baseColor = {1.0f, 0.72f, 0.34f, 1.0f}}))
                 .tag("glow"));
  return root;
}

}  // namespace

namespace {

struct GlowTrail final : sketch::Set {
  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(640, 440);
    ctx.background({0.03f, 0.035f, 0.05f, 1.0f});
    ctx.captureAt(1.3);
    gm::camera::Camera lens;
    lens.eye = kEye;
    lens.target = {0.0f, -30.0f, 0.0f};
    lens.fovYDeg = 42.0f;
    ctx.camera(lens);
  }

  world::Frame describe(float seconds) override {
    world::Frame frame(set(seconds));
    frame
        // The whole set, and — because a narrowed post pass follows it —
        // the coverage of what that pass addresses.
        .pass(world::geometryPass("main").writes("colour"))
        // MASK: the beads are lifted where they stand, and nowhere else.
        .pass(world::postPass("hot")
                  .reads("colour")
                  .writes("hot")
                  .only(world::sel::tag("glow"))
                  .levels(1.9f, 0.06f, {1.0f, 0.86f, 0.66f, 1.0f}))
        // CULL: the same beads, alone, so what is softened next is the
        // emitters rather than the picture.
        .pass(world::geometryPass("emitters")
                  .only(world::sel::tag("glow"))
                  .writes("spark"))
        .pass(world::postPass("bloom")
                  .reads("spark")
                  .writes("bloom")
                  .blur(11.0f))
        // …at a strength a tail can be built out of: what accumulates
        // over many frames must be dim in any one of them.
        .pass(world::postPass("ember").reads("bloom").writes("ember").levels(
            0.42f, 0.0f, {1.0f, 0.80f, 0.52f, 1.0f}))
        // PREVIOUS: this frame's embers over the tail the frames before
        // it left, at a weight that lets the tail fade out.
        .pass(world::postPass("trail")
                  .reads("ember")
                  .previous("trail")
                  .writes("trail")
                  .composite(SkBlendMode::kPlus, 0.88f))
        .pass(world::postPass("picture")
                  .reads("hot", "trail")
                  .writes("picture")
                  .composite(SkBlendMode::kPlus, 1.0f));
    return frame;
  }
};

}  // namespace

SIGIL_SKETCH(
    GlowTrail, "Set",
    "The passes, made visible \xe2\x80\x94 one set reached three ways: a "
    "masked grade where the beads stand, the same beads culled "
    "into a target of their own and softened, and that laid over "
    "its own output from the frame before")
