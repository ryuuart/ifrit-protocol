/** @file
 * key_light — the emitter's dials, and what they reach.
 *
 * One set under the kit's three-point rig, with the KEY LIGHT'S strength
 * and colour bound to live values while the tree that declares them
 * stands still. Nothing about the description changes from frame to
 * frame: what moves is what the lanes are bound to, which is the second
 * of this library's two write paths and the whole point of a lane.
 *
 * The subject is a ring of posts round a body, so the falloff has
 * something to fall across and the rig's fill and back lights have
 * somewhere to be seen. A body is shaded by the emitters the frame
 * gathered, and the gather happens on the walk that visits every node —
 * so a lamp that ramps stales no cache, which is why the set can be as
 * still as it looks and still change colour.
 */

#include <choreograph/Choreograph.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilworld/element/Node.h>
#include <sigilworld/kit/Kit.h>

#include <cmath>
#include <memory>
#include <string>

#include "Studies.h"

namespace sigil::world::testing {

namespace {

namespace gm = ::sigil::geometry::mesh;

constexpr float kTwoPi = 6.283185307179586f;
constexpr int kPosts = 9;
constexpr float kRing = 150.0f;

/** WHAT THE LANES ARE BOUND TO. Live values the study drives from the
 *  scene time, held for as long as the study is, because a lane
 *  addresses the output rather than copying it. */
struct Dials {
  choreograph::Output<float> intensity = 1.0f;
  choreograph::Output<float> red = 1.0f;
  choreograph::Output<float> green = 1.0f;
  choreograph::Output<float> blue = 1.0f;
};

/** A ring of posts round one body — the same tree at every moment. */
Element subject() {
  Element set;
  set.key("subject").child(
      Element()
          .key("body")
          .at({0.0f, 34.0f, 0.0f})
          .mesh(gm::superellipsoid({46, 46, 46}, 3.0f, 28, 18))
          .fill(material::kit::surface(
              {.baseColor = {0.72f, 0.70f, 0.66f, 1.0f}, .roughness = 0.45f}))
          .tag("lit"));
  for (int i = 0; i < kPosts; ++i) {
    const float angle = (float)i * kTwoPi / (float)kPosts;
    set.child(Element()
                  .key("post" + std::to_string(i))
                  .at({kRing * std::cos(angle), 0.0f, kRing * std::sin(angle)})
                  .rotateY(angle * 57.2957795f)
                  .mesh(gm::superellipsoid({11, 52, 11}, 5.0f, 12, 8))
                  .fill(material::kit::surface(
                      {.baseColor = {0.36f, 0.38f, 0.44f, 1.0f}}))
                  .tag("lit"));
  }
  return set;
}

/** THE RIG WITH THE DIALS ON ITS KEY. A preset returns an ordinary
 *  tree, so dressing one of its children is rebuilding the tree with
 *  that child replaced — which is what a copy-on-write value is for, and
 *  why a preset needs no hook for this. */
Element rigWithDials(const kit::Rig& spec, Dials& dials) {
  const Element rig = kit::threePoint(spec);
  Element out;
  out.key(rig.node()->key);
  for (const Element& lamp : rig.node()->children) {
    Element copy = lamp;
    if (lamp.node()->key == "key")
      copy.intensity(&dials.intensity)
          .emission(&dials.red, &dials.green, &dials.blue);
    out.child(std::move(copy));
  }
  return out;
}

}  // namespace

Study keyLight() {
  Study study;
  study.name = "key_light";
  study.canvas = {760, 500};
  study.captureSeconds = 1.6f;
  study.background = {0.028f, 0.032f, 0.042f, 1.0f};

  const std::shared_ptr<Dials> dials = std::make_shared<Dials>();
  study.describe = [dials](float seconds) {
    // The values the lanes read, written from the scene time — so the
    // plate is a function of the declared moment and of nothing else.
    const float swing = 0.5f + 0.5f * std::sin(seconds * 2.1f);
    dials->intensity = 0.35f + 1.15f * swing;
    dials->red = 1.0f;
    dials->green = 0.62f + 0.38f * swing;
    dials->blue = 0.35f + 0.65f * (1.0f - swing);

    kit::Set set;
    set.rig.extent = 150.0f;
    set.rig.bearing = -50.0f;
    set.rig.elevation = 26.0f;
    set.rig.fill = 0.28f;
    set.rig.back = 0.5f;
    set.ground = 5.0f;
    set.drop = 0.36f;
    set.table.radius = 560.0f;
    set.table.height = 250.0f;
    set.table.period = 16.0f;
    set.table.fovYDeg = 44.0f;

    const Element root = kit::litSet(subject(), set, seconds);
    Element dressed;
    dressed.key(root.node()->key);
    for (const Element& child : root.node()->children)
      dressed.child(child.node()->key == "rig" ? rigWithDials(set.rig, *dials)
                                               : child);
    return Frame(std::move(dressed));
  };
  return study;
}

}  // namespace sigil::world::testing
