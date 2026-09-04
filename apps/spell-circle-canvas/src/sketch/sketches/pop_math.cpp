/** @file
 * pop_math — the point operators that write a lane, and the two that
 * change what the set IS.
 *
 * Every operator here addresses attributes BY NAME. The conventional
 * lanes — P, T, Dir, Scale, Color, Tex — are only well-known names; any
 * other name creates a custom float4 on first write, flows through every
 * filter after it, and comes out on the cooked cloud. That is what makes
 * a mask a lane rather than a parameter: `Select` writes one from a
 * region of space, and every per-point filter's `mask` field reads a
 * lane's .x as how much of its write each point receives.
 *
 * Three of the eight are not per-point maps, and the boundary is worth
 * seeing on one sheet. `Delete` changes the COUNT, which no map can do.
 * `Normal` reads a point's own position to decide a sense. `Mix` reads
 * two lanes and writes a third, which is still per-point but is the op
 * that makes one attribute a function of another.
 *
 * EDIT THESE FIRST
 *   kMotes   — points in the cloud every cell starts from.
 *   kFactor  — the Mix weight.
 *   kFeather — the fraction of Select's extent that fades.
 */

#include <include/core/SkCanvas.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Points.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <functional>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace gm = sigil::geometry::mesh;
namespace camera = sigil::geometry::mesh::camera;
namespace points = sigil::geometry::mesh::points;

using namespace sigil::compose;
using sigil::compose::toU8;
using pop = sigil::geometry::mesh::pop;

namespace {

constexpr SkSize kCanvas = {1100, 672};
constexpr float kCell = 252;
constexpr float kPicture = 202;

constexpr int kMotes = 4200;      // points every cell starts from
constexpr float kFactor = 0.55f;  // the Mix weight
constexpr float kFeather = 0.6f;  // the fraction of Select's extent that fades

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.09f, 0.095f, 0.11f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(11.5f, kInk, 0.8f),
          .note = mono(10, kAsh),
          .gap = 7,
          .noteMeasure = kCell};
}

camera::Camera stage() {
  camera::Camera view;
  view.eye = {0, 96, 250};
  view.target = {0, 0, 0};
  view.fovYDeg = 40;
  return view;
}

points::BillboardStyle splat() {
  points::BillboardStyle style;
  style.size = 2.4f;
  style.additive = false;
  return style;
}

/** The cloud every cell starts from: points on a torus, tinted along
 *  their own T so a rewrite of any other lane is visible against a
 *  colouring that did not change. */
pop::Builder base() {
  return pop::on(gm::torus(74, 24, 64, 32), kMotes)
      .seed(3)
      .rampBy({{0.34f, 0.60f, 0.96f, 1}, {0.98f, 0.68f, 0.32f, 1}});
}

Element cell(const char* call, const std::string& note, pop::Builder chain) {
  return kit::cell(
      voice(), toU8(call), toU8(note),
      kit::well({.width = kCell,
                 .height = kPicture,
                 .ground = Fill::color(kCellGround)},
                custom(call, [chain = std::move(chain)](
                                 SkCanvas& canvas, const PaintContext& pc) {
                  chain.billboards(canvas, stage(), pc.size, splat());
                })));
}

}  // namespace

struct PopMath final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    // The selection every masked cell reads, and the two halves Delete
    // cuts the set into — counted here so the captions can say so.
    const auto selected = [] {
      return base().select("core", pop::Select::Shape::Sphere, {0, 0, 74},
                           {70, 70, 70}, kFeather);
    };
    const size_t kept = selected().keep("core").cloud().size();
    const size_t dropped = selected().drop("core").cloud().size();

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("POP MATH \xc2\xb7 Math, Fill, Affine, Lookup, "
                           "Select, Mix, Normal, Delete"),
             .subtitle = toU8("dials \xc2\xb7 the operator \xc2\xb7 the Mix "
                              "weight (0.55) \xc2\xb7 the Select feather "
                              "(0.6 of the extent)"),
             .footer = toU8("an operator names the lane it writes, and a "
                            "name nothing has written yet is all zeros "
                            "\xe2\x80\x94 which is why naming an unwritten "
                            "lane as a mask selects nobody rather than "
                            "everybody"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {kit::cells(
                          {.cells =
                               {cell("the cloud, uncut",
                                     kit::format(
                                         "pop::on(torus, %d) with a two-stop "
                                         "Lookup on T \xc2\xb7 every cell "
                                         "below starts here",
                                         kMotes),
                                     base()),
                                cell("Math{.lane = P, .mul = {1, 2.4, 1, 1}}",
                                     "lane = lane * mul + add, per component "
                                     "\xc2\xb7 the diagonal case of Affine, "
                                     "and the one that needs no matrix",
                                     base().op(
                                         pop::Math{pop::Lane::P,
                                                   {1, 2.4f, 1, 1}})),
                                cell("Affine{.matrix = rotate * shear}",
                                     "the whole affine vocabulary in one op "
                                     "\xc2\xb7 as a POSITION the "
                                     "translation applies; as a DIRECTION "
                                     "only the upper 3x3 acts",
                                     base().affine(
                                         glm::rotate(
                                             glm::mat4(1.0f),
                                             0.5f,
                                             glm::vec3{0, 0, 1}) *
                                         glm::mat4{1, 0, 0, 0, 0.55f, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1})),
                                cell("Lookup{.from = P, .weights = {0,1,0,0}}",
                                     "key = dot(from, weights), remapped "
                                     "from [low, high] onto a table of "
                                     "stops and sampled \xc2\xb7 colour by "
                                     "HEIGHT, not by T",
                                     base().rampBy(pop::Lane::P,
                                                   1, {{0.10f, 0.14f, 0.30f, 1}, {0.30f, 0.85f, 0.72f, 1}, {1.00f, 0.95f, 0.55f, 1}},
                                                   -24, 24))},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {
                                   cell(
                                       "Select{.feather = 0.6} \xe2\x86\x92 "
                                       "Math{.mask = \"core\"}",
                                       "a mask is a LANE: 1 inside the region, "
                                       "0 outside, feathered across the outer "
                                       "0.6 \xc2\xb7 the write lands in "
                                       "proportion",
                                       selected().masked("core").op(
                                           pop::Math{
                                               pop::Lane::P,
                                               {1, 1, 1, 1},
                                               {0, 58, 0, 0}})),
                                   cell("Fill{\"anchor\"} \xe2\x86\x92 "
                                        "Mix{P, anchor, P, 0.55}",
                                        kit::format(
                                            "to = a + (b - a) * factor "
                                            "\xc2\xb7 Fill invented the lane "
                                            "on first write and Mix drew the "
                                            "cloud %.0f%% of the way to it",
                                            (double)(kFactor * 100)),
                                        base()
                                            .fill("anchor", {0, 86, 0, 1})
                                            .mix(pop::Lane::P, "anchor",
                                                 pop::Lane::P, kFactor)),
                                   cell(
                                       "Normal{.sense = +1} \xe2\x86\x92 "
                                       "Peak{34}",
                                       "Dir made unit and turned to face AWAY "
                                       "from the centre, then every point "
                                       "pushed along its own \xc2\xb7 without "
                                       "the Normal the pushes disagree",
                                       base().normal(1.0f, {0, 0, 0}).peak(34)),
                                   cell("Delete{.mask = \"core\", .keep}",
                                        kit::format(
                                            "the count is what this op moves: "
                                            "%zu kept, %zu dropped, of %d "
                                            "\xc2\xb7 every lane compacted "
                                            "through one permutation",
                                            kept, dropped, kMotes),
                                        selected().keep("core"))},
                           .gap = 14})},
                 .column = true,
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(PopMath, "Kit \xc2\xb7 API",
             "eight point operators over one scattered torus: the lane "
             "rewrites, the mask a Select writes, and the two that change "
             "what the set is")
