/** @file
 * deformed_cloud — a point chain, cooked and stood up in a room.
 *
 * A body's surface is scattered into points, a BAND across its middle is
 * selected, and two deformers are taken as far as that one selection
 * says — one inside it and one outside. Inside, every point is pushed
 * out along its own normal, which raises a waist. Outside, the body is
 * turned about the up axis by an amount that grows with height, so the
 * top shears against the bottom while the waist between them stands
 * still. The whole cloud is coloured by height, off the same axis. A
 * small facet stands at every cooked point, so what a plate shows is the
 * chain's answer and not a picture of it.
 *
 * THE CHAIN IS THE DESCRIPTION, NOT THE POINTS. Nothing here evaluates
 * anything: a `Chain` is a value the node carries, and the runtime the
 * frame is performed on cooks it — the host executor on a machine with
 * no device, and the device's own kernels where every operator in the
 * chain has one, from the same description and to the same points. The
 * count is the one number that decides how much work that is.
 *
 * A MASK IS A LANE, and that is what makes this readable: `select`
 * writes one from a region, `masked` says the operator just added takes
 * it, and INVERTING the same region is how the second deformer addresses
 * everything the first one did not. Two operators, one region, and the
 * only thing separating them is one flag.
 */

#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <cmath>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;

using namespace sigil::world;

namespace {

namespace gm = ::sigil::geometry::mesh;
/** The point-operator language is a SCOPE rather than a namespace, so
 *  it is named by a type alias and spelled the same way either is. */
using pop = gm::pop;

/** How far across the body stands, how many points are scattered over
 *  it, and how thick the selected band is as a fraction of its height. */
constexpr float kExtent = 150.0f;
constexpr int kMotes = 24000;
constexpr float kBandFraction = 0.22f;

/** The body the points are taken from: a rounded SLAB, whose corners and
 *  flats are what a twist about the up axis can be read by. A shape
 *  symmetric about that axis would be turned into itself and the whole
 *  deformer would be invisible, which is the one property this study
 *  cannot afford in its subject. */
gm::Mesh body() {
  return gm::superellipsoid({kExtent, kExtent * 0.95f, kExtent * 0.62f}, 5.0f,
                            96, 56);
}

/** The colours height is read by: cool at the bottom, warm at the top,
 *  with one bright stop between so the band's own height reads. */
const std::vector<glm::vec4>& heights() {
  static const std::vector<glm::vec4> stops = {{0.18f, 0.30f, 0.72f, 1.0f},
                                               {0.94f, 0.86f, 0.62f, 1.0f},
                                               {0.95f, 0.36f, 0.22f, 1.0f}};
  return stops;
}

}  // namespace

namespace {

struct DeformedCloud final : sketch::Set {
  gm::Cloud seed;
  float low = 0.0f;
  float high = 0.0f;

  void setup(sketch::SetContext& ctx) override {
    ctx.canvas(880, 580);
    ctx.background({0.026f, 0.029f, 0.040f, 1.0f});
    ctx.captureAt(1.45);
    // Seeded, and taken once: the scatter is a property of the body and
    // not of the moment, so every frame describes the same seed and only
    // what the chain does to it changes.
    seed = gm::points::onMesh(body(), kMotes, 5);
    low = high = seed.positions.empty() ? 0.0f : seed.positions[0].y;
    for (const glm::vec3& point : seed.positions) {
      low = std::min(low, point.y);
      high = std::max(high, point.y);
    }
  }

  world::Frame describe(float seconds) override {
    const float middle = (low + high) * 0.5f;
    const glm::vec3 centre{0.0f, middle, 0.0f};
    const glm::vec3 slab{kExtent * 6.0f, (high - low) * kBandFraction,
                         kExtent * 6.0f};
    // The band is a slab across the middle, feathered so its edge is a
    // graded fraction rather than a cut. Every `masked` below names it.
    const float turn = 18.0f + 86.0f * (0.5f + 0.5f * std::sin(seconds * 0.7f));
    const float push = 16.0f + 30.0f * (0.5f + 0.5f * std::sin(seconds * 0.5f));

    const Chain forged =
        pop::on(seed)
            .rampBy(pop::Lane::P, 1, heights(), low, high)
            .select("band", pop::Select::Shape::Box, centre, slab, 0.45f)
            // Inside the band: pushed out along each point's own normal,
            // which raises the waist the shear is read against.
            .peak(push)
            .masked("band")
            .select("band", pop::Select::Shape::Box, centre, slab, 0.45f,
                    pop::Select::Combine::Replace, /*invert=*/true)
            // …and everything outside it: turned about the up axis, more
            // the higher it stands, so the top shears against the bottom
            // and the band between them does not move at all.
            .twist(turn, {0, 1, 0}, low, high, {0, 0, 0})
            .masked("band")
            .vary(0.45f, 1.0f);

    kit::Set set;
    set.rig.extent = kExtent * 1.4f;
    set.rig.bearing = -38.0f;
    set.rig.elevation = 30.0f;
    set.ground = 4.0f;
    set.drop = 1.0f;
    set.table.radius = 640.0f;
    set.table.height = 230.0f;
    set.table.period = 20.0f;
    set.table.fovYDeg = 42.0f;

    return Frame(
        kit::litSet(Element()
                        .key("forged")
                        .chain(forged)
                        .stamp(gm::quad(3.1f, 3.1f))
                        .fill(material::kit::surface(
                            {.baseColor = {1, 1, 1, 1}, .roughness = 0.6f}))
                        .tag("cloud"),
                    set, seconds));
  }
};

}  // namespace

SIGIL_SKETCH(DeformedCloud, "Set",
             "A point chain cooked in a room \xe2\x80\x94 a band selected "
             "across a scattered body, twisted inside it and pushed out "
             "beyond it, with one facet stood at every cooked point")
