/** @file
 * compute_variant — the three pass verbs a scene3d sketch has that draw
 * nothing by themselves: cooking points, re-drawing a selection, and asking for
 * a resource back.
 *
 * `computePass(name).chain(chain, runtime)` COOKS POINTS and writes no
 * pixels. What it cooks lands in the resource it writes, and a geometry
 * pass that READS that resource stands its `stamp` at every point of it —
 * so the cloud on the rail below was never authored as bodies and is not
 * in the scene tree at all.
 *
 * `only(selector).variant(surface)` DRAWS THE SELECTION AGAIN in another
 * surface. The pass paints every body first; the swapped collection's are then
 * repainted in the variant, which is why the near row reads hot and the
 * far row keeps the material it declared. The ordering READS THAT OFF
 * the declaration — a pass carrying `variant` is realised as
 * `Selection::Variant` — and `realise(Selection::Variant)` is the door a
 * pass that knows better than the rule says so through; spelled here so
 * the value has a name on the page.
 *
 * `readback(name).then(fn)` ASKS FOR A RESOURCE BACK, and the callback
 * runs THE FRAME AFTER the one that made it, because reading a device's
 * memory costs a wait. The tell-tale posts along the front
 * edge are that answer made visible: one post per hundred points the
 * readback counted, so a frame in which nothing came back has none. The
 * count and the frame number are printed to stderr as well, because a scene3d
 * sketch has no text of its own.
 *
 * The passes execute on `diligent::runtime`, with the chain cooked by the
 * device's own point executor.
 *
 * EDIT THESE FIRST
 *   kMotes   — how many points the chain cooks.
 *   kSwapCollection — the word the variant pass selects on.
 *   the captured moment, which is where the rail's window stands.
 */

// TAGS: Geometry/Points

#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <cmath>
#include <cstdio>
#include <glm/vec3.hpp>
#include <ranges>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace material = sigil::material;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr int kBlocks = 5;      // blocks in each row
constexpr int kMotes = 700;     // points the chain cooks
constexpr float kSpacing = 96;  // between blocks in a row
const char* const kSwapCollection = "swap";

constexpr glm::vec3 kEye{0.0f, 210.0f, 620.0f};

/** The rail the cooked points ride: a closed loop that rises and falls,
 *  so the cloud reads as a curve in space rather than a ring seen at an
 *  angle. */
gm::curve::Spline3 rail() {
  return world::kit::wave(
      {.radius = 250.0f, .inner = 150.0f, .high = 90.0f, .low = -20.0f});
}

/** The points the compute pass cooks. A description only — nothing here
 *  runs until a pass names an executor for it. */
gm::pop::Chain motes() {
  return gm::pop::on(rail().sampleArcLength(96))
      .count(kMotes)
      .spread(9.0f)
      .vary(0.5f, 1.0f)
      .fade({0.55f, 0.85f, 1.0f, 1.0f}, {0.25f, 0.45f, 0.95f, 1.0f})
      .lookAt(kEye);
}

/** One row of blocks at @p z, every one of them in the collection
 *  @p joined. */
world::Group row(const char* key, float z, const char* joined,
                 material::Material surface) {
  return world::group(
             {std::views::iota(0, kBlocks) | std::views::transform([&](int i) {
                const float x =
                    ((float)i - (float)(kBlocks - 1) * 0.5f) * kSpacing;
                return world::mesh(gm::superellipsoid({30.0f, 44.0f, 30.0f},
                                                      5.0f, 12, 8))
                    .key(std::string(key) + std::to_string(i))
                    .at({x, -60.0f, z})
                    .rotateY((float)i * 9.0f)
                    .fill(surface)
                    .collection(joined);
              })})
      .key(key);
}

}  // namespace

namespace {

struct ComputeVariant {
  /** What the readback last handed over. It arrives the frame after the
   *  one that made it, so the first frame has none and every frame after
   *  it stands the posts the count asks for. */
  int cookedPoints = 0;

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(
        ctx, {.size = {880, 560},
              .captureAt = 1.4,
              .background = material::Color{0.028f, 0.032f, 0.05f, 1.0f}});
    gm::camera::Camera lens;
    lens.eye = kEye;
    lens.target = {0.0f, -30.0f, 0.0f};
    lens.fovYDegrees = 40.0f;
    ctx.camera(lens);
  }

  /** The bodies. The cooked cloud is NOT among them — it arrives as a
   *  resource a pass stamps. */
  world::Group scene() const {
    const material::Material slate = material::surface::program(
        {.baseColor = {0.30f, 0.33f, 0.40f, 1.0f}, .roughness = 0.6f});
    // THE READBACK, STOOD UP: one post per hundred points the callback
    // counted — none at all in a frame nothing came back in.
    const int posts = cookedPoints / 100;
    return world::group(
               {world::directionalLight(
                    {-0.42f, -0.82f, -0.38f},
                    {.color = material::Color{0.96f, 0.97f, 1.0f, 1.0f},
                     .intensity = 2.5f})
                    .key("sun"),
                world::ambientLight({.intensity = 0.07f}).key("fill"),
                world::mesh(gm::quad(900, 900))
                    .key("plate")
                    .at({0, -104, 0})
                    .rotateX(-90.0f)
                    .fill(material::Color{0.07f, 0.08f, 0.11f, 1.0f}),
                row("far", -300.0f, "keep", slate),
                row("near", 110.0f, kSwapCollection, slate),
                world::group(
                    {std::views::iota(0, posts) |
                     std::views::transform([posts](int i) {
                       return world::mesh(gm::superellipsoid(
                                              {9.0f, 18.0f, 9.0f}, 2.0f, 8, 5))
                           .key("tally" + std::to_string(i))
                           .at({((float)i - (float)(posts - 1) * 0.5f) * 34.0f,
                                -90.0f, 200.0f})
                           .fill(material::surface::unlit(
                               {.baseColor = {0.95f, 0.78f, 0.35f, 1.0f}}));
                     })})
                    .key("tally")})
        .key("set");
  }

  world::Frame describe() {
    // What the swapped row is REPAINTED in. What the two executors take
    // from it differs — the host reads its base colour and stands it
    // under the pass's own lights, the device draws the surface it
    // names — so the row reads hot on both and identical on neither.
    const material::Material hot =
        material::surface::unlit({.baseColor = {1.0f, 0.42f, 0.22f, 1.0f}});

    world::Frame frame(scene());
    frame
        // Points, cooked. It writes no pixels and orders itself ahead of
        // whatever reads what it wrote.
        .pass(world::computePass("cook").chain(motes()).writes("motes"))
        // Every body, a flake at every cooked point, and the swapped row
        // drawn again in `hot`. ONE geometry pass does all three: a
        // second one writing the same target would clear it and paint
        // the bodies over again, since a pass that narrows nothing
        // paints everything.
        //
        // The ordering infers Selection::Variant from `variant`; the
        // call below is the door a pass that knows better than the rule
        // overrides it through.
        .pass(world::geometryPass("main")
                  .reads("motes")
                  .writes("colour")
                  .clear({0.028f, 0.032f, 0.05f, 1.0f})
                  .stamp(gm::quad(7.0f, 7.0f))
                  .only(kSwapCollection)
                  .variant(hot)
                  .realise(world::Selection::Variant))
        .readback(world::readback("motes").then(
            [this](const world::Readback::Result& result) {
              const int count =
                  result.points ? (int)result.points->positions.size() : 0;
              cookedPoints = count;
              std::fprintf(stderr,
                           "[compute_variant] readback \"%s\" from frame %llu: "
                           "%d points\n",
                           result.resource.c_str(),
                           (unsigned long long)result.frame, count);
            }));
    return frame;
  }
};

}  // namespace

SIGIL_SKETCH(ComputeVariant, "Kit · API",
             "computePass cooks the cloud, a variant pass repaints the "
             "swapped row, and a readback counts the points it got back")
