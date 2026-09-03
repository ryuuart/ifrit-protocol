/** @file
 * set_stagger — the entrances of a set's children, cascaded, and the two
 * selectors that address a subtree afterwards.
 *
 * `staggerChildren(Spread)` cascades the entrances of a node's children
 * AS THEY MOUNT, on the schedule SigilMotion speaks — an even ladder, a
 * fixed total divided across however many children there are, a cue
 * table, an origin and a distribution curve. The delay COMPOUNDS down the
 * subtree, so a grandchild enters after its parent did, and only children
 * that actually mount are delayed.
 *
 * An entrance is `animate(from(a).to(b))` and nothing else: the path
 * plays once, when the node first appears, and afterwards the property
 * behaves exactly like `to(b)`. That is the whole grammar — `to(v)` alone
 * is ramp-on-change and says nothing about mounting.
 *
 * The two rows here differ only in their spread's ORIGIN. The near row
 * runs from Start and the far row from Edges, which begins at both ends
 * and meets in the middle, so at one moment the two rows hold different
 * shapes of the same cascade.
 *
 * A SELECTOR is the other half: a comparable value naming a set of nodes
 * rather than enumerating one. `sel::under(key)` answers for everything
 * BELOW that node and not for the node itself; `sel::material(m)`
 * compares the node's material by value. A pass narrowed by one addresses
 * those bodies and no others — here the far row is drawn again into a
 * target of its own and composited back over the frame, which is how a
 * selection is made visible where a pass paints everything.
 *
 * EDIT THESE FIRST
 *   kEach, kDuration — the ladder's spacing and one child's own motion.
 *   kCount — how many children each row mounts.
 *   The captured moment, which is what decides how far the cascade got.
 */

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/element/Selector.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/kit/Kit.h>

#include <chrono>
#include <string>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr int kCount = 7;         // children in each row
constexpr float kEach = 150;      // the ladder's spacing, ms
constexpr float kDuration = 620;  // one child's own motion, ms
constexpr float kPitch = 62;      // between two children, world units
constexpr float kRise = 120;      // how far a child enters from, world units

/** The material the far row wears, held as a value so a pass can name it
 *  with `sel::material` — a selector compares it by value. */
material::Material litSlab() {
  return material::kit::surface(
      {.baseColor = {0.86f, 0.62f, 0.34f, 1}, .roughness = 0.38f});
}

material::Material coolSlab() {
  return material::kit::surface(
      {.baseColor = {0.62f, 0.68f, 0.78f, 1}, .roughness = 0.45f});
}

/** One row of children under its own key, cascaded by its own spread. */
world::Element row(const std::string& key, float z, motion::Spread spread,
                   const material::Material& skin) {
  world::Element parent;
  parent.key(key).staggerChildren(spread);
  for (int i = 0; i < kCount; ++i) {
    const float x = ((float)i - (float)(kCount - 1) * 0.5f) * kPitch;
    parent.child(
        world::Element()
            .key(key + std::to_string(i))
            .at({x, 30, z})
            .mesh(gm::superellipsoid({22, 30, 22}, 3.0f, 18, 12))
            .fill(skin)
            .tag("body")
            // THE ENTRANCE, and the only thing the cascade delays: the
            // path plays once, when the node first appears.
            .translateY(motion::animate(
                motion::from(kRise).to(0.0f),
                {std::chrono::milliseconds((int)kDuration)}))
            .scale(motion::animate(
                motion::from(0.35f).to(1.0f),
                {std::chrono::milliseconds((int)kDuration)})));
  }
  return parent;
}

}  // namespace

struct SetStagger final : sketch::Set {
  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(900, 460);
    ctx.background({0.045f, 0.05f, 0.062f, 1});
    // MID-CASCADE: far enough in that the head of each row has landed and
    // its tail is still on the way, which is the whole of what a ladder
    // looks like.
    ctx.captureAt(0.62);
    gm::camera::Camera lens;
    lens.eye = {0, 190, 430};
    lens.target = {0, 26, -10};
    lens.up = {0, 1, 0};
    lens.fovYDeg = 40;
    lens.zNear = 4;
    lens.zFar = 4000;
    ctx.camera(lens);
  }

  world::Frame describe(float seconds) override {
    world::Element subject;
    subject.key("rows")
        .child(row("near", 60,
                   {.eachMs = kEach,
                    .durationMs = kDuration,
                    .from = motion::Spread::From::Start},
                   coolSlab()))
        .child(row("far", -80,
                   {.eachMs = kEach,
                    .durationMs = kDuration,
                    .from = motion::Spread::From::Edges},
                   litSlab()));

    // The tree declares NO camera, so the viewpoint the setup wrote is
    // the one the frame is seen from — a still eye, because what moves
    // here is the cascade and not the lens.
    world::kit::Rig rig;
    rig.extent = 190.0f;
    rig.bearing = -34.0f;
    rig.elevation = 34.0f;

    world::Element root;
    root.key("set")
        .child(world::Element()
                   .key("ground")
                   .at({0, -12, 0})
                   .mesh(gm::superellipsoid({420, 10, 300}, 8.0f, 12, 8))
                   .fill(material::kit::surface(
                       {.baseColor = {0.20f, 0.21f, 0.24f, 1},
                        .roughness = 0.7f})))
        .child(world::kit::threePoint(rig))
        .child(std::move(subject));

    world::Frame frame(std::move(root));
    // THE SELECTION, made visible where the frame paints everything: the
    // whole picture first, then the far row ALONE culled into a target of
    // its own, softened, and laid back over it.
    frame.pass(world::geometryPass("main").writes("colour"))
        .pass(world::geometryPass("far-only")
                  .only(world::sel::under("far") &
                        world::sel::material(litSlab()))
                  .writes("halo"))
        .pass(world::postPass("soft").reads("halo").writes("soft").blur(9.0f))
        .pass(world::postPass("picture")
                  .reads("colour", "soft")
                  .writes("picture")
                  .composite(SkBlendMode::kPlus, 0.75f));
    return frame;
  }
};

SIGIL_SKETCH(SetStagger, "Set",
             "two rows of children entering on their own cascades \xe2\x80\x94 "
             "one from the start, one from both edges \xe2\x80\x94 with the "
             "far row alone selected by key and by material, softened and "
             "laid back over the frame")
