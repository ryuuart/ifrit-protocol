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

#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Pop.h>

#include <cmath>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Studies.h"

namespace sigil::world::testing {

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr float kTwoPi = 6.283185307179586f;
constexpr int kPosts = 16;
constexpr float kRing = 190.0f;

struct Paint {
  glm::vec4 baseColor{1, 1, 1, 1};
};

material::Material paint(glm::vec4 colour) {
  static const std::shared_ptr<const material::Recipe> recipe =
      std::make_shared<const material::Recipe>(
          material::Recipe::of<Paint>("world.study.paint"));
  return material::Material(recipe, Paint{colour});
}

/** The loop the comet rides: a ring that rises and falls, so the tail
 *  reads as a curve in space rather than as an arc on a plane. */
Spline3 loop() {
  Spline3 spline;
  for (int i = 0; i < 6; ++i) {
    const float angle = (float)i * kTwoPi / 6.0f;
    const float radius = (i % 2 == 0) ? 200.0f : 120.0f;
    const float height = (i % 2 == 0) ? 70.0f : -50.0f;
    spline.points.emplace_back(radius * std::cos(angle), height,
                               radius * std::sin(angle));
  }
  spline.closed = true;
  return spline;
}

/** The set: a ring of posts, a plate under them, and a comet of beads
 *  riding a moving window of the loop. Only the comet answers to
 *  "glow". */
Element set(float seconds) {
  const Spline3 rail = loop();
  const std::vector<glm::vec3> path = rail.sampleArcLength(96);
  const float head = std::fmod(seconds * 0.42f, 1.0f);
  const Chain comet =
      gm::pop::on(path)
          .count(600)
          .spread(7.0f)
          .vary(0.6f, 1.0f)
          .fade({1.0f, 0.78f, 0.38f, 1.0f}, {1.0f, 0.42f, 0.16f, 1.0f});

  Element root;
  root.key("set")
      .child(Element().key("sun").light(
          sun({-0.4f, -0.85f, -0.35f}, {0.95f, 0.96f, 1.0f, 1.0f}, 0.9f)))
      .child(Element()
                 .key("plate")
                 .at({0, -120, 0})
                 .rotateX(-90.0f)
                 .mesh(gm::quad(760, 760))
                 .fill(paint({0.09f, 0.10f, 0.13f, 1.0f}))
                 .tag("ground"));

  Element posts;
  posts.key("posts");
  for (int i = 0; i < kPosts; ++i) {
    const float angle = (float)i * kTwoPi / (float)kPosts;
    posts.child(
        Element()
            .key("post" + std::to_string(i))
            .at({kRing * std::cos(angle), -84.0f, kRing * std::sin(angle)})
            .rotateY(angle * 57.2957795f)
            .mesh(gm::superellipsoid({8.0f, 40.0f, 8.0f}, 6.0f, 10, 6))
            .fill(paint({0.34f, 0.37f, 0.46f, 1.0f}))
            .tag("lit"));
  }
  root.child(std::move(posts));

  root.child(Element()
                 .key("comet")
                 .chain(comet)
                 .stamp(gm::superellipsoid({5.0f, 5.0f, 5.0f}, 2.0f, 8, 6))
                 .window(head, 0.22f)
                 .fill(paint({1.0f, 0.72f, 0.34f, 1.0f}))
                 .tag("glow"));
  return root;
}

}  // namespace

Study glowTrail() {
  Study study;
  study.name = "glow_trail";
  study.canvas = {640, 440};
  study.captureSeconds = 1.3f;
  study.background = {0.03f, 0.035f, 0.05f, 1.0f};
  study.camera.eye = {0.0f, 250.0f, 560.0f};
  study.camera.target = {0.0f, -30.0f, 0.0f};
  study.camera.fovYDeg = 42.0f;

  study.describe = [](float seconds) {
    Frame frame(set(seconds));
    frame
        // The whole set, and — because a narrowed post pass follows it —
        // the coverage of what that pass addresses.
        .pass(geometryPass("main").writes("colour"))
        // MASK: the beads are lifted where they stand, and nowhere else.
        .pass(postPass("hot")
                  .reads("colour")
                  .writes("hot")
                  .only(sel::tag("glow"))
                  .levels(1.9f, 0.06f, {1.0f, 0.86f, 0.66f, 1.0f}))
        // CULL: the same beads, alone, so what is softened next is the
        // emitters rather than the picture.
        .pass(geometryPass("emitters").only(sel::tag("glow")).writes("spark"))
        .pass(postPass("bloom").reads("spark").writes("bloom").blur(11.0f))
        // …at a strength a tail can be built out of: what accumulates
        // over many frames must be dim in any one of them.
        .pass(postPass("ember").reads("bloom").writes("ember").levels(
            0.42f, 0.0f, {1.0f, 0.80f, 0.52f, 1.0f}))
        // PREVIOUS: this frame's embers over the tail the frames before
        // it left, at a weight that lets the tail fade out.
        .pass(postPass("trail")
                  .reads("ember")
                  .previous("trail")
                  .writes("trail")
                  .composite(SkBlendMode::kPlus, 0.88f))
        .pass(postPass("picture")
                  .reads("hot", "trail")
                  .writes("picture")
                  .composite(SkBlendMode::kPlus, 1.0f));
    return frame;
  };
  return study;
}

}  // namespace sigil::world::testing
