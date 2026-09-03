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
#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilweave/style/Type.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
namespace geometry = sigil::geometry;

namespace {

constexpr int kCount = 1400;
constexpr float kFeather = 0.35f;
constexpr float kTwistDeg = 150.0f;
constexpr float kTaper = 0.25f;
constexpr float kBendDeg = 80.0f;
constexpr float kPeak = 70.0f;
constexpr float kPanel = 180.0f;
constexpr float kHeight = 300.0f;  // the column: y in [-150, 150]
constexpr float kLead = 374.0f;    // two panels and the gap between them

const SkColor4f kGround{0.055f, 0.06f, 0.085f, 1};
const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.24f, 0.28f, 0.36f, 1};
const SkColor4f kRule{0.19f, 0.20f, 0.26f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

kit::Caption voice(float measure) {
  return {.where = kit::Caption::Where::Split,
          .label = label(12, kInk, 0.4f),
          .note = label(10.5f, kDim, 0.2f),
          .gap = 5,
          .noteMeasure = measure};
}

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
  camera.eye = {260, 120, 720};
  camera.target = {0, 0, 0};
  camera.fovYDeg = 30;
  return camera;
}

/** The point stamp, baked once for the process. */
const sk_sp<SkImage>& disc() {
  static const sk_sp<SkImage> img = kit::dotSprite();
  return img;
}

Element splat(geometry::mesh::Cloud cloud) {
  return custom([cloud = std::move(cloud)](SkCanvas& canvas,
                                           const PaintContext& paint) {
           geometry::mesh::points::BillboardStyle style;
           style.sprite = disc();
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

Element panel(const char* title, const char* note, Element inner) {
  // The cell is held to the picture's width: a call longer than its own
  // panel would otherwise widen the cell and the two rows would stop
  // lining up column for column.
  return kit::cell(voice(kPanel), toU8(title), toU8(note),
                   box()
                       .width(kPanel)
                       .height(kPanel * 1.6f)
                       .clip()
                       .stroke(stroke(1.0f, Fill::color(kFrame)))
                       .child(std::move(inner)))
      .width(Dim(kPanel));
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

struct PopDeform final : sketch::Sketch {
  geometry::mesh::Cloud selected, moved;
  geometry::mesh::Cloud twisted, tapered, bent, peaked;
  geometry::mesh::Cloud twistedM, taperedM, bentM, peakedM;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1240, 860);
    ctx.background(kGround);
    // Every cloud is cooked once in setup; nothing here reads the clock.
    ctx.captureAt(0.05);

    using Builder = geometry::mesh::pop::Builder;
    // The four chains, written once and cooked twice: the second call
    // adds `.masked("band")` and nothing else.
    const auto twist = [](Builder b) {
      // Twist about an axis standing BESIDE the column, so a symmetric
      // column has something to show: it becomes a helix.
      return b.twist(kTwistDeg, {0, 1, 0}, -kHeight / 2, kHeight / 2,
                     {-55, 0, 0});
    };
    const auto taper = [](Builder b) {
      return b.taper(kTaper, {0, 1, 0}, -kHeight / 2, kHeight / 2);
    };
    const auto bend = [](Builder b) {
      return b.bend(kBendDeg, {0, 1, 0}, {1, 0, 0}, -kHeight / 4, kHeight / 2);
    };
    // Peak pushes along Dir — the loop tangent, i.e. straight up the
    // column — after orient() has tipped Dir over by 60 degrees, so the
    // push leans out instead of lengthening the column.
    const auto peak = [](Builder b) {
      return b.orient(geometry::mesh::camera::place({}, 0, 0, 60)).peak(kPeak);
    };

    selected = base().cloud();
    // The mask at work: everyone gets the same Math, taken by "band".
    moved = base().move({90, 0, 0}).masked("band").cloud();

    twisted = twist(base()).cloud();
    tapered = taper(base()).cloud();
    bent = bend(base()).cloud();
    peaked = peak(base()).cloud();

    twistedM = twist(base()).masked("band").cloud();
    taperedM = taper(base()).masked("band").cloud();
    bentM = bend(base()).masked("band").cloud();
    peakedM = peak(base()).masked("band").cloud();

    Element whole = kit::cells(
        {.cells = {panel("select(\"band\", Box, feather 0.35)",
                         "the mask lane itself \xe2\x80\x94 the colour ramp "
                         "reads it, so the feather is visible",
                         splat(selected)),
                   panel("move({90,0,0}).masked(\"band\")",
                         "one Math, taken by the mask: the band slides out "
                         "and the rest stands",
                         splat(moved)),
                   panel("twist(150\xc2\xb0, +Y, origin -55x)",
                         "a helix: more turn with height", splat(twisted)),
                   panel("taper(0.25, +Y)", "toward the axis at the top",
                         splat(tapered)),
                   panel("bend(80\xc2\xb0, +Y, +X)",
                         "the band arcs; past it, rigid", splat(bent)),
                   panel("orient(60\xc2\xb0) . peak(70)",
                         "push along a re-aimed Dir", splat(peaked))},
         .gap = 14});

    Element banded = kit::cells(
        {.cells = {box()
                       .width(Dim(kLead))
                       .column()
                       .gap(6)
                       .child(text(toU8("\xe2\x80\xa6" "and the same four, "
                                        ".masked(\"band\")"),
                                   label(13, kInk, 0.6f)))
                       .child(text(toU8("a mask is one more lane on the "
                                        "cloud, so a masked deformer is the "
                                        "same chain reading one more "
                                        "channel. The four calls below are "
                                        "the four above with one more link "
                                        "in each; the amounts are shared "
                                        "constants, so the two rows are "
                                        "comparable by construction."),
                                   label(11, kDim))
                                  .width(Dim(kLead))),
                   panel("twist(\xe2\x80\xa6).masked(\"band\")",
                         "only the band turns", splat(twistedM)),
                   panel("taper(\xe2\x80\xa6).masked(\"band\")",
                         "only the band narrows", splat(taperedM)),
                   panel("bend(\xe2\x80\xa6).masked(\"band\")",
                         "only the band arcs", splat(bentM)),
                   panel("peak(\xe2\x80\xa6).masked(\"band\")",
                         "only the band is pushed", splat(peakedM))},
         .gap = 14});

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("POP DEFORM \xc2\xb7 select() writes a lane, "
                           "masked() takes it"),
             .subtitle = toU8("one column of 1,400 points \xc2\xb7 twist, "
                              "taper, bend and orient+peak, on the whole "
                              "cloud above and on the selected band below"),
             .footer = toU8("every chain is cooked once by the CPU "
                            "reference executor and splatted by "
                            "points::drawBillboards \xc2\xb7 all ten are "
                            "GPU-executable unchanged"),
             .titleStyle = label(15, kInk, 2.0f),
             .subtitleStyle = label(11, kDim, 0.6f),
             .footerStyle = label(10.5f, kDim, 0.2f),
             .marginX = 30,
             .marginTop = 22,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells({.cells = {std::move(whole), std::move(banded)},
                        .column = true,
                        .gap = 22}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(
    PopDeform, "Kit \xc2\xb7 API",
    "geometry::pop select() and masked(), then twist / taper / bend / peak "
    "\xe2\x80\x94 one column, six chains, every one GPU-executable")
