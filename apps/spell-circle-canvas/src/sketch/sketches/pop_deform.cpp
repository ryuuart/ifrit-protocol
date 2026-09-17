/** @file
 * pop_deform — SELECTORS, MASKS AND DEFORMERS in `geometry::mesh::pop`.
 *
 * Every cloud is cooked once by the CPU reference executor in setup() and
 * splatted by `points::drawBillboards`, so this file is the Skia-painter
 * view of the operator language and nothing else.
 *
 * One column of points, and one question asked of it twice.
 *
 *   THE MASK, first. `select()` writes a mask LANE from a region — a box
 *     across the column's middle, feathered, so the edge is a graded band
 *     rather than a cut. The colour ramp then reads that lane, which is
 *     why the mask is visible at all. `.masked("band")` takes the SAME
 *     Math every point receives and applies it only as far as the lane
 *     says: `move()` slides the band out of the column and leaves the
 *     rest standing.
 *   THE DEFORMERS, twice. `twist`, `taper`, `bend` and `orient` + `peak`
 *     deform the whole cloud on the upper row and the selected band alone
 *     on the lower one. Nothing about the chains differs between the two
 *     rows but the one call, which is the whole claim: a mask is one more
 *     lane on the cloud, so a masked deformer is the same chain reading
 *     one more channel.
 *
 * EDIT THESE FIRST
 *   kFeather   — the selector's soft edge (0 = hard 0/1 selection).
 *   kTwistDeg / kTaper / kBendDeg / kPeak — the deformer amounts. Both
 *                rows read them, so the two stay comparable.
 */
// TAGS: Geometry/Points

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace geometry = sigil::geometry;

namespace {

constexpr int kCount = 1400;
constexpr float kFeather = 0.35f;
constexpr float kTwistDeg = 150.0f;
constexpr float kTaper = 0.25f;
constexpr float kBendDeg = 80.0f;
constexpr float kPeak = 70.0f;
constexpr float kPanel = 220.0f;
constexpr float kHeight = 300.0f;  // the column: y in [-150, 150]

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.90f, 0.93f, 0.97f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.captionLabel = {.size = 12, .track = 0.4f};
  return look;
}

const SkColor4f kFrame{0.24f, 0.28f, 0.36f, 1};

/** A thin vertical loop: points scatter along it with a radial spread,
 *  so the cloud is a fuzzy column standing on the y axis. */
std::vector<glm::vec3> column() {
  return {{0, -kHeight / 2, 0},
          {0, kHeight / 2, 0},
          {0, kHeight / 2, 1},
          {0, -kHeight / 2, 1}};
}

geometry::mesh::camera::Camera lookAtColumn() {
  geometry::mesh::camera::Camera camera;
  camera.eye = {300, 140, 850};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 38;
  return camera;
}

/** THE SINK. The point stamp is baked into the program BY VALUE, once per
 *  describe: asked for inside the body it is a 64 px surface rasterised on
 *  every paint, which at `Cache::None` is every frame. */
Element splat(geometry::mesh::Cloud cloud) {
  // KEYLESS: what the program closes over is a whole point cloud, which no
  // key spells — and the sink paints live at `Cache::None`, so its node was
  // never going to prune.
  return custom([cloud = std::move(cloud), sprite = kit::dotSprite()](
                    SkCanvas& canvas, const PaintContext& paint) {
           geometry::mesh::points::BillboardStyle style;
           style.sprite = sprite;
           style.size = 7;
           style.sizeLane = "size";
           style.tintLane = "tint";
           style.additive = false;
           style.depthSort = true;
           geometry::mesh::points::drawBillboards(canvas, cloud, lookAtColumn(),
                                                  paint.size, style);
         })
      .inset(0)
      .cache(Cache::None);
}

Element field(geometry::mesh::Cloud cloud) {
  return sketch::kit::well(
             {.width = kPanel, .height = 236, .keyline = Fill::color(kFrame)})
      .children({splat(std::move(cloud))});
}

/** The shared head of every chain: the column, spread, sized, and a
 *  band across its middle selected into "band" — feathered so the ramp
 *  shows a gradient at the edges. Colour then reads the mask: cool
 *  outside, hot inside. */
geometry::mesh::pop::Builder base() {
  const std::vector<glm::vec4> stops = {{0.16f, 0.22f, 0.45f, 1},
                                        {0.85f, 0.35f, 0.30f, 1},
                                        {1.00f, 0.85f, 0.35f, 1}};
  return geometry::mesh::pop::on(column())
      .count(kCount)
      .window(0.5f, 0.5f)
      .spread(28)
      .seed(3)
      .vary(0.5f)
      .select("band", geometry::mesh::pop::Select::Shape::Box, {0, 20, 0},
              {80, 55, 80}, kFeather)
      .rampBy("band", 0, stops);
}

}  // namespace

struct PopDeform {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {1240, 890}});
    // Every cloud is cooked once in setup; nothing here reads the clock.
    ctx.captureAt(0.05);

    using Builder = geometry::mesh::pop::Builder;
    // ONE ROW PER DEFORMER, cooked twice: the second cooking adds
    // `.masked("band")` to the same chain and nothing else.
    struct Deform {
      const char* call;
      const char* note;
      const char* maskedCall;
      const char* maskedNote;
      Builder (*link)(Builder);
    };
    static const std::array<Deform, 4> kDeforms{{
        // Twist about an axis standing BESIDE the column, so a symmetric
        // column has something to show: it becomes a helix.
        {"twist(150°, +Y, origin -55x)", "a helix: more turn with height",
         "twist(…).masked(\"band\")", "only the band turns",
         [](Builder b) {
           return b.twist(kTwistDeg, {0, 1, 0}, -kHeight / 2, kHeight / 2,
                          {-55, 0, 0});
         }},
        {"taper(0.25, +Y)", "toward the axis at the top",
         "taper(…).masked(\"band\")", "only the band narrows",
         [](Builder b) {
           return b.taper(kTaper, {0, 1, 0}, -kHeight / 2, kHeight / 2);
         }},
        {"bend(80°, +Y, +X)", "the band arcs; past it, rigid",
         "bend(…).masked(\"band\")", "only the band arcs",
         [](Builder b) {
           return b.bend(kBendDeg, {0, 1, 0}, {1, 0, 0}, -kHeight / 4,
                         kHeight / 2);
         }},
        // Peak pushes along Dir — the loop tangent, i.e. straight up the
        // column — after orient() has tipped Dir over by 60 degrees, so the
        // push leans out instead of lengthening the column.
        {"orient(60°) . peak(70)", "push along a re-aimed Dir",
         "peak(…).masked(\"band\")", "only the band is pushed",
         [](Builder b) {
           return b.orient(geometry::mesh::camera::place({}, 0, 0, 60))
               .peak(kPeak);
         }},
    }};

    const char* names[] = {"TWIST", "TAPER", "BEND", "ORIENT + PEAK"};
    const char* settings[] = {"150° · offset axis", "top scale 0.25",
                              "80° toward +X", "60° direction · push 70"};
    std::vector<sketch::kit::ComparisonCase> whole{
        {.title = "SOURCE / THE MASK",
         .control = "Box · feather 0.35",
         .figure = field(base().cloud()),
         .note = "Warm points belong to the selected band; cool points lie "
                 "outside."}};
    std::vector<sketch::kit::ComparisonCase> masked{
        {.title = "MOVE / BAND ONLY",
         .control = "+90 along X",
         .figure = field(base().move({90, 0, 0}).masked("band").cloud()),
         .note =
             "A simple translation reveals how the feather weights the edit."}};
    for (size_t i = 0; i < kDeforms.size(); ++i) {
      const Deform& how = kDeforms[i];
      whole.push_back({.title = names[i],
                       .control = settings[i],
                       .figure = field(how.link(base()).cloud()),
                       .note = how.note});
      masked.push_back(
          {.title = names[i],
           .control = settings[i],
           .figure = field(how.link(base()).masked("band").cloud()),
           .note = how.maskedNote});
    }
    ctx.composer.render(sketch::kit::page(
        {.title = "The same deformation, with a mask",
         .subtitle = "1,400 points · a feathered band is the only difference "
                     "between the two operator rows",
         .footer = "Read down each column: identical amounts and camera. The "
                   "mask is a lane on the cloud, so every deformer can read "
                   "the same selection."},
        box().column().gap(20).children(
            {text("WHOLE CLOUD / EACH POINT RECEIVES THE FULL EDIT")
                 .styleClass("section"),
             sketch::kit::comparison(
                 {.cases = std::move(whole), .measure = 1160, .gap = 15}),
             text("SELECTED BAND / THE MASK WEIGHTS THE SAME EDIT")
                 .styleClass("section"),
             sketch::kit::comparison(
                 {.cases = std::move(masked), .measure = 1160, .gap = 15})})));
  }
};

SIGIL_SKETCH(
    PopDeform, "Kit · API",
    "geometry::pop select() and masked(), then twist / taper / bend / peak "
    "— one column, six chains, every one GPU-executable")
