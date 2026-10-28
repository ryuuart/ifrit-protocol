/** @file
 * pop_operators — every point operator over a cooked cloud, on one
 * cached sink.
 *
 * Every operator addresses attributes BY NAME. The conventional lanes —
 * P, T, Dir, Scale, Color, Tex — are only well-known names; any other
 * name creates a custom float4 on first write, flows through every
 * filter after it, and comes out on the cooked cloud. That is what makes
 * a mask a lane rather than a parameter: `Select` writes one from a
 * region of space, and every per-point filter's `mask` field reads a
 * lane's .x as how much of its write each point receives.
 *
 * Four rows, read top to bottom:
 *
 *   01  the lane rewrites — Math, Affine, Lookup — on a torus tinted
 *       along its own T, so a rewrite of any other lane shows against a
 *       colouring that did not change.
 *   02  where an edit lands — a feathered Select masking a move, Mix
 *       toward a named value, Normal then Peak, and Delete, the one
 *       operator that changes the COUNT.
 *   03  the deformers — twist, taper, bend, orient then peak — each
 *       cooked twice on one column: the whole cloud on the left of a
 *       cell, the same chain plus `.masked("band")` on the right.
 *   04  the two operators that make order matter — `order()`, a
 *       permutation, on a sink with no depth sort, where the last sprite
 *       drawn wins the pixel; and Relax (`smooth()`), which eases each
 *       point toward its chain-order neighbours and heals the kinks
 *       Noise leaves.
 *
 * ONE SINK. Every cloud is cooked once in setup and splatted by
 * `drawBillboards` into a well cached as a texture: nothing on the sheet
 * moves, so the pixels are reached once and blitted after. The sprites
 * are compose shapes painted once through the texture door.
 *
 * EDIT THESE FIRST
 *   kMotes   — points in the torus every cell of rows 01 and 02 starts from.
 *   kFactor  — the Mix weight.
 *   kFeather — the fraction of Select's extent that fades.
 *   kTwistDegrees / kTaper / kBendDegrees / kPeak — the deformer amounts;
 *              both halves of a cell read them, so the pair stays
 *              comparable.
 *   kNoise / kRelaxPasses — the kink Relax is asked to heal, and how
 *              hard it smooths.
 */

// TAGS: Geometry/Points

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmedia/core/Picture.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace media = sigil::media;
namespace shapes = sigil::geometry::shapes;
namespace gm = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace points = sigil::geometry::mesh::points;
namespace pop = sigil::geometry::mesh::pop;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1200, 1640};
constexpr float kCell = 268;
constexpr float kPicture = 218;
constexpr float kMeasure = 1120;
constexpr float kCaseGap = 16;

constexpr int kMotes = 4200;  // points in the torus rows 01 and 02 start from
constexpr float kFactor = 0.55f;  // the Mix weight
constexpr float kFeather = 0.6f;  // the fraction of Select's extent that fades

constexpr int kColumnPoints = 1400;
constexpr float kColumnHeight = 300;  // the column: y in [-150, 150]
constexpr float kBandFeather = 0.35f;
constexpr float kTwistDegrees = 150;
constexpr float kTaper = 0.25f;
constexpr float kBendDegrees = 80;
constexpr float kPeak = 70;

constexpr int kCrownPoints = 240;  // few, big and overlapping
constexpr float kNoise = 20;       // the displacement Relax has to heal
constexpr int kRelaxPasses = 12;

// ---------------------------------------------------------------------------
// The four subjects, each with the camera that looks at it

/** Rows 01 and 02: points on a torus, tinted along their own T. */
pop::Builder torus() {
  return pop::on(gm::torus(74, 24, 64, 32), kMotes)
      .seed(3)
      .rampBy({{0.34f, 0.60f, 0.96f, 1}, {0.98f, 0.68f, 0.32f, 1}});
}
const camera::Camera kTorusView{.eye = {0, 120, 330},
                                .target = {0, 0, 0},
                                .verticalFieldOfViewDegrees = 40};

/** Row 03: a fuzzy column standing on the y axis, a band across its
 *  middle selected into "band" and feathered, and the colour reading the
 *  mask — cool outside, hot inside. */
pop::Builder column() {
  return pop::on(std::vector<glm::vec3>{{0, -kColumnHeight / 2, 0},
                                        {0, kColumnHeight / 2, 0},
                                        {0, kColumnHeight / 2, 1},
                                        {0, -kColumnHeight / 2, 1}})
      .count(kColumnPoints)
      .window(0.5f, 0.5f)
      .spread(28)
      .seed(3)
      .vary(0.5f)
      .select("band", pop::Select::Shape::Box, {0, 20, 0}, {80, 55, 80},
              kBandFeather)
      .rampBy("band", 0,
              {{0.16f, 0.22f, 0.45f, 1},
               {0.85f, 0.35f, 0.30f, 1},
               {1.00f, 0.85f, 0.35f, 1}});
}
const camera::Camera kColumnView{.eye = {300, 140, 820},
                                 .target = {0, 0, 0},
                                 .verticalFieldOfViewDegrees = 38};

/** Row 04, left: a crown, a ring with a threefold wave, whose near and
 *  far arcs overlap on screen — the only condition under which draw
 *  order is visible at all. Colour follows depth, so a mis-ordered
 *  sprite is a dark dot on top of a bright one. */
pop::Builder crown() {
  std::vector<glm::vec3> loop;
  for (int index = 0; index < 72; ++index) {
    const float angle = 6.2831853f * (float)index / 72.0f;
    loop.emplace_back(215 * std::cos(angle), 190 * std::sin(3 * angle),
                      215 * std::sin(angle));
  }
  return pop::on(std::move(loop))
      .count(kCrownPoints)
      .spread(62)
      .seed(5)
      .vary(0.45f)
      .rampBy(pop::Attribute::P, 2,
              {{0.06f, 0.08f, 0.16f, 1},
               {0.20f, 0.38f, 0.55f, 1},
               {0.95f, 0.98f, 1.00f, 1}},
              -230, 230);
}
const camera::Camera kCrownView{.eye = {0, 180, 1160},
                                .target = {0, 0, 0},
                                .verticalFieldOfViewDegrees = 34};

/** Row 04, right: a ring pushed off its loop by Noise, the kink Relax
 *  exists to take out. */
pop::Builder kinked() {
  std::vector<glm::vec3> loop;
  for (int index = 0; index < 96; ++index) {
    const float angle = 6.2831853f * (float)index / 96.0f;
    loop.emplace_back(64 * std::cos(angle), 24 * std::sin(2 * angle),
                      64 * std::sin(angle));
  }
  return pop::on(std::move(loop))
      .count(1400)
      .seed(4)
      .noise(kNoise, 0.075f)
      .rampBy({{0.98f, 0.84f, 0.42f, 1}, {0.42f, 0.86f, 0.72f, 1}});
}
const camera::Camera kLoopView{
    .eye = {0, 74, 235}, .target = {0, 0, 0}, .verticalFieldOfViewDegrees = 40};

// ---------------------------------------------------------------------------
// The sink

/** One splat: a cooked @p cloud seen through @p view in @p style. The
 *  lanes a cook exports, "size" and "tint", are named so a chain that
 *  varied either shows it. */
struct Splat {
  gm::Cloud cloud;
  camera::Camera view;
  points::BillboardStyle style;
};

/** ONE CELL: the splats a well holds side by side, painted once and held
 *  as a texture. The sink closes over COOKED points rather than over the
 *  chain that describes them, so nothing is cooked again when the well is
 *  painted. */
Element figure(const std::string& key, std::vector<Splat> splats) {
  const float across = kCell / (float)splats.size();
  return sketch::kit::well(
      {.width = kCell, .height = kPicture},
      box().row().cover().children(
          each(splats, [&](const Splat& splat, size_t index) {
            return custom(key + "." + std::to_string(index),
                          [splat](sigil::draw::Pen& pen,
                                  const PaintContext& paint) {
                            points::drawBillboards(*pen.canvas(), splat.cloud,
                                                   splat.view, paint.size,
                                                   splat.style);
                          })
                .width(across)
                .height(kPicture)
                .cache(Cache::Texture);
          })));
}

/** A WHITE STAMP drawn as a compose shape and painted once through the
 *  texture door. White because the sink tints per point: a coloured stamp
 *  multiplies into every tint. */
media::Picture stamp(sketch::Scene2DContext& context, Element shape) {
  return context
      .texture(box().width(64).height(64).padding(2).children(
                   {std::move(shape).flexGrow()}),
               {64, 64})
      .frameAt()
      .image;
}

const material::Color kWhite{1, 1, 1, 1};
const material::Color kBlack{0, 0, 0, 1};

}  // namespace

struct PopOperators {
  void setup(sketch::Scene2DContext& ctx) {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.styleSheet = sketch::kit::featureStyleSheet(
                                 sketch::kit::Density::Spacious),
                             .size = kCanvas,
                             .captureAt = 0.05});

    const points::BillboardStyle torusStyle{.size = 2.4f,
                                            .sizeLane = "size",
                                            .tintLane = "tint",
                                            .additive = false};
    const auto onTorus = [&](const char* key, pop::Builder chain) {
      return figure(key, {{chain.cloud(), kTorusView, torusStyle}});
    };

    // The selection the masked cell reads, and the two halves Delete cuts
    // the set into — counted here so the caption can say so.
    const auto selected = [] {
      return torus().select("core", pop::Select::Shape::Sphere, {0, 0, 74},
                            {70, 70, 70}, kFeather);
    };
    const size_t kept = selected().keep("core").cloud().size();
    const size_t dropped = selected().drop("core").cloud().size();

    // ROW 03: one chain per deformer, cooked whole and cooked masked.
    // A hard-rimmed disc rather than a soft dot, so a point's place reads.
    const points::BillboardStyle columnStyle{
        .sprite =
            stamp(ctx, box().shape(shapes::circle()).fill(Fill::color(kWhite))),
        .size = 7,
        .sizeLane = "size",
        .tintLane = "tint",
        .additive = false};
    struct Deformer {
      const char* title;
      const char* control;
      pop::Builder (*link)(pop::Builder);
    };
    static const std::array<Deformer, 4> kDeformers{{
        // Twist about an axis standing BESIDE the column, so a symmetric
        // column has something to show: it becomes a helix.
        {"TWIST", "twist(150°) · axis −55x",
         [](pop::Builder chain) {
           return chain.twist(kTwistDegrees, {0, 1, 0}, -kColumnHeight / 2,
                              kColumnHeight / 2, {-55, 0, 0});
         }},
        {"TAPER", "taper(0.25) toward the top",
         [](pop::Builder chain) {
           return chain.taper(kTaper, {0, 1, 0}, -kColumnHeight / 2,
                              kColumnHeight / 2);
         }},
        {"BEND", "bend(80°) toward +X",
         [](pop::Builder chain) {
           return chain.bend(kBendDegrees, {0, 1, 0}, {1, 0, 0},
                             -kColumnHeight / 4, kColumnHeight / 2);
         }},
        // Peak pushes along Dir, the loop's tangent straight up the
        // column, after orient() has tipped Dir over by 60 degrees, so
        // the push leans out instead of lengthening the column.
        {"ORIENT + PEAK", "orient(60°) · peak(70)",
         [](pop::Builder chain) {
           return chain.orient(camera::place({}, 0, 0, 60)).peak(kPeak);
         }},
    }};
    std::vector<sketch::kit::ComparisonCase> deformed;
    for (const Deformer& deformer : kDeformers)
      deformed.push_back(
          {.title = deformer.title,
           .control = deformer.control,
           .figure = figure(
               std::string("deform.") + deformer.title,
               {{deformer.link(column()).cloud(), kColumnView, columnStyle},
                {deformer.link(column()).masked("band").cloud(), kColumnView,
                 columnStyle}}),
           .note = "Left: every point. Right: the same chain, masked by "
                   "the band."});

    // ROW 04: no depth sort and no additive blend, because both hide what
    // order() does; the rim is black because a tint multiplies, so it
    // survives every colour.
    const points::BillboardStyle crownStyle{
        .sprite = stamp(ctx, box()
                                 .shape(shapes::circle())
                                 .fill(Fill::color(kBlack))
                                 .padding(4)
                                 .children({box()
                                                .flexGrow()
                                                .shape(shapes::circle())
                                                .fill(Fill::color(kWhite))})),
        .size = 34,
        .sizeLane = "size",
        .tintLane = "tint",
        .additive = false,
        .depthSort = false};
    const points::BillboardStyle strandStyle{.size = 3.2f,
                                             .sizeLane = "size",
                                             .tintLane = "tint",
                                             .additive = false};

    ctx.composer.render(sketch::kit::page(
        {.title = "One cloud, different questions",
         .subtitle = "Every point operator, cooked once and splatted by one "
                     "cached sink · one camera within each row",
         .footer = "Selection writes a continuous lane; a masked operator "
                   "weights its edit by that lane. keep() changes the "
                   "membership of the cloud, order() its sequence."},
        box().column().gap(26).children(
            {document::h2("01 / REWRITE A VALUE · 4,200 POINTS"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "SOURCE",
                        .control = "colour follows T",
                        .figure = onTorus("source", torus()),
                        .note = "The unchanged torus is the reference for "
                                "every edit."},
                       {.title = "STRETCH",
                        .control = "P.y × 2.4",
                        .figure =
                            onTorus("stretch",
                                    torus().operation(pop::Math{
                                        pop::Attribute::P, {1, 2.4f, 1, 1}})),
                        .note = "Multiply one component. The colour lane stays "
                                "intact."},
                       {.title = "SHEAR + ROTATE",
                        .control = "Affine · rotate × shear",
                        .figure = onTorus(
                            "affine",
                            torus().affine(
                                glm::rotate(glm::mat4(1.0f), 0.5f,
                                            glm::vec3{0, 0, 1}) *
                                glm::mat4{1, 0, 0, 0, 0.55f, 1, 0, 0, 0, 0, 1,
                                          0, 0, 0,
                                          0, 1})),
                        .note = "One matrix acts on position. Translation is "
                                "excluded for directions."},
                       {.title = "RECOLOUR",
                        .control = "Lookup · height −24…24",
                        .figure =
                            onTorus("lookup",
                                    torus().rampBy(pop::Attribute::P, 1,
                                                   {{0.10f, 0.14f, 0.30f, 1},
                                                    {0.30f, 0.85f, 0.72f, 1},
                                                    {1.00f, 0.95f, 0.55f, 1}},
                                                   -24, 24)),
                        .note = "Only tint changes. Low, middle and high "
                                "points find new colours."}},
                  .measure = kMeasure,
                  .gap = kCaseGap}),
             document::h2("02 / CHOOSE WHERE THE EDIT LANDS"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "FEATHER A MOVE",
                        .control = "Select core → move +58y",
                        .figure = onTorus(
                            "masked", selected().masked("core").operation(
                                          pop::Math{pop::Attribute::P,
                                                    {1, 1, 1, 1},
                                                    {0, 58, 0, 0}})),
                        .note = "A soft selection blends the displacement into "
                                "untouched points."},
                       {.title = "DRAW TO AN ANCHOR",
                        .control = "Fill anchor → Mix 55%",
                        .figure = onTorus(
                            "mix", torus()
                                       .fill("anchor", {0, 86, 0, 1})
                                       .mix(pop::Attribute::P, "anchor",
                                            pop::Attribute::P, kFactor)),
                        .note = "Every position travels the same fraction "
                                "toward a named value."},
                       {.title = "PUSH OUTWARD",
                        .control = "Normal outward → Peak 34",
                        .figure = onTorus(
                            "peak", torus().normal(1.0f, {0, 0, 0}).peak(34)),
                        .note = "Normalize the direction before pushing each "
                                "point along it."},
                       {.title = "KEEP THE SELECTION",
                        .control = "Delete · keep core",
                        .figure = onTorus("keep", selected().keep("core")),
                        .note = kit::formatted(
                            "%zu kept · %zu removed. Every lane follows the "
                            "same permutation.",
                            kept, dropped)}},
                  .measure = kMeasure,
                  .gap = kCaseGap}),
             document::h2("03 / DEFORM THE WHOLE CLOUD, THEN ONLY THE "
                          "SELECTED BAND · 1,400 POINTS"),
             sketch::kit::comparison({.cases = std::move(deformed),
                                      .measure = kMeasure,
                                      .gap = kCaseGap}),
             document::h2("04 / ORDER AND RELAX: THE OPERATORS THAT READ "
                          "THEIR NEIGHBOURS"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "SCATTER ORDER",
                        .control = "240 opaque sprites · no depth sort",
                        .figure =
                            figure("crown.scatter",
                                   {{crown().cloud(), kCrownView, crownStyle}}),
                        .note = "Dark, distant sprites cover bright, near "
                                "ones. The last mark wins."},
                       {.title = "DEPTH ORDER",
                        .control = "order({0, 0, 1}) · farthest first",
                        .figure = figure("crown.sorted",
                                         {{crown().order({0, 0, 1}).cloud(),
                                           kCrownView, crownStyle}}),
                        .note = "Near sprites land last; overlap agrees with "
                                "the depth the colour encodes."},
                       {.title = "A NOISED LOOP",
                        .control = "noise amplitude 20 · 0 passes",
                        .figure = figure(
                            "loop.noised",
                            {{kinked().cloud(), kLoopView, strandStyle}}),
                        .note = "Consecutive points jump across the displaced "
                                "loop."},
                       {.title = "RELAXED",
                        .control = "smooth(0.9) · 12 passes",
                        .figure = figure(
                            "loop.relaxed",
                            {{kinked().smooth(0.9f, kRelaxPasses).cloud(),
                              kLoopView, strandStyle}}),
                        .note = "Each point eases toward its chain-order "
                                "neighbours: the kinks soften, the broad "
                                "displacement stays."}},
                  .measure = kMeasure,
                  .gap = kCaseGap})})));
  }
};

SIGIL_SKETCH(PopOperators, "Kit · API",
             "every point operator over a cooked cloud on one cached sink: "
             "the lane rewrites, a Select mask, the deformers whole and "
             "masked, order() as a permutation and Relax healing a noised "
             "loop")
