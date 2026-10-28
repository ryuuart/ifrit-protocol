/** @file
 * crossing_rule — who passes over whom, as one comparable value.
 *
 * Crossings are DISCOVERED, never authored: `discoverCrossings` flattens a
 * list of strands and reports every proper crossing among them, numbered
 * along the boundary. A `CrossingRule` then answers one question per
 * knot — is the lower-indexed strand over or under — and `crossingPatch`
 * gives the region where the two marks actually overlap there, which is
 * the shape a repaint of the winner is clipped to.
 *
 * Three subjects, four rules. The {7/2} heptagram is seven chords, so it has
 * seven knots and an odd count, which is why `alternate()` cannot close and
 * leaves one seam. The three rings are three strands with six knots, which
 * is where the cyclic `pairs` dominance is exactly right: 0 over 1 over 2
 * over 0 is the impossible braid, and no ordering of a draw list can spell
 * it.
 *
 * The first two rows paint the decision by hand, so every step shows: the
 * strands are free-standing paths, and the winner of each knot is drawn
 * again clipped to its patch. The last row hands the SAME rule values to
 * `brush::weave`, which shapes its strands from one node's outline — here
 * `kit::braid`, three waves at evenly spread phases round a chamfered
 * plaque — and finds and repairs every knot itself. Parallel rails never
 * cross, so an offset strand set cannot braid at all; the waves trade
 * sides, which is what gives a rule something to decide.
 *
 * EDIT THESE FIRST
 *   kReach       — the full width of a strand's mark, px. The patch is
 *                  sized from it.
 *   kPatchRadius — the cap on a patch's reach from its knot, px. Half the
 *                  arc distance to the neighbouring knot; larger and two
 *                  lenses merge and one strand claims both.
 *   kStrands, kAmplitude, kWavelength — the braid on the plaque.
 */

// TAGS: Geometry/Diagrams

#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace arrange = sigil::geometry::arrange;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1370};
constexpr float kCell = 240;        // the drawn square of one cell
constexpr float kReach = 15;        // a strand's full mark width, px
constexpr float kPatchRadius = 30;  // the cap on one patch's reach, px
constexpr float kPlaque = 168;      // the woven plaque's side, px
constexpr int kStrands = 3;         // the braid's strands
constexpr float kAmplitude = 10;    // how far a strand swings, px
constexpr float kWavelength = 84;   // px per cycle of it

constexpr material::Color kCasing{0.05f, 0.05f, 0.06f, 1};
constexpr material::Color kCore{0.86f, 0.80f, 0.66f, 1};
constexpr material::Color kPin{0.92f, 0.36f, 0.30f, 1};

/** The specimen sheet, in this one's own look.
 *
 *  A hot-reload fixture grounds a copy of this file in a colour of its
 *  own by writing one into this theme's palette and reads the corner
 *  pixel back, so the canvas and the page must both take their ground
 *  from here — which they do, because the theme is bound before
 *  `stage()` runs. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
  // Behind one cell's specimen, a shade off the sheet's own ground.
  look.palette.cellGround = {0.11f, 0.11f, 0.13f, 1};
  return look;
}

/** Seven chords of a regular heptagon, each joining a vertex to the one
 *  two round — the {7/2} star as seven separate strands, because a
 *  crossing is between two strands and a self-crossing path has one. */
std::vector<SkPath> heptagram() {
  constexpr int kPoints = 7;
  const float radius = kCell * 0.40f;
  const glm::vec2 centre{kCell * 0.5f, kCell * 0.5f};
  SkPoint vertices[kPoints];
  for (int vertex = 0; vertex < kPoints; ++vertex)
    vertices[vertex] =
        path::toSk(arrange::onRing((size_t)vertex, kPoints,
                                   {.center = centre,
                                    .radii = {radius, radius},
                                    .fromDegrees = -90,
                                    .sweepDegrees = 360,
                                    .turn = arrange::Turn::Closed}));
  std::vector<SkPath> strands;
  for (int vertex = 0; vertex < kPoints; ++vertex)
    strands.push_back(SkPathBuilder()
                          .moveTo(vertices[vertex])
                          .lineTo(vertices[(vertex + 2) % kPoints])
                          .detach());
  return strands;
}

/** Three rings on a triangle, overlapping pairwise — six knots, and the
 *  arrangement whose only consistent reading is a cycle. */
std::vector<SkPath> rings() {
  const float radius = kCell * 0.24f;
  const glm::vec2 centre{kCell * 0.5f, kCell * 0.52f};
  std::vector<SkPath> strands;
  for (int ring = 0; ring < 3; ++ring) {
    const glm::vec2 at =
        arrange::onRing((size_t)ring, 3,
                        {.center = centre,
                         .radii = {radius * 0.62f, radius * 0.62f},
                         .fromDegrees = -90,
                         .sweepDegrees = 360,
                         .turn = arrange::Turn::Closed});
    strands.push_back(SkPathBuilder().addCircle(at.x, at.y, radius).detach());
  }
  return strands;
}

/** Every strand drawn casing-then-core, then the winner of each knot
 *  drawn again clipped to the patch that knot occupies. */
void paintWeave(SkCanvas& canvas, const std::vector<SkPath>& strands,
                const path::CrossingRule& rule, int pinnedIndex) {
  SkPaint casing;
  casing.setAntiAlias(true);
  casing.setStyle(SkPaint::kStroke_Style);
  casing.setStrokeWidth(kReach);
  casing.setStrokeCap(SkPaint::kRound_Cap);
  casing.setColor4f(material::skia::toSkColor(kCasing));
  SkPaint core = casing;
  core.setStrokeWidth(kReach - 5);
  core.setColor4f(material::skia::toSkColor(kCore));

  const auto strand = [&](size_t index) {
    canvas.drawPath(strands[index], casing);
    canvas.drawPath(strands[index], core);
  };
  for (size_t index = 0; index < strands.size(); ++index) strand(index);

  for (const path::Crossing& knot : path::discoverCrossings(strands)) {
    const size_t over =
        rule.decide(knot) == path::Order::Over ? knot.a : knot.b;
    canvas.save();
    canvas.clipPath(
        path::crossingPatch(strands[knot.a], kReach, strands[knot.b], kReach,
                            knot.at, kPatchRadius),
        true);
    strand(over);
    canvas.restore();
    if ((int)knot.index == pinnedIndex) {
      SkPaint mark;
      mark.setAntiAlias(true);
      mark.setStyle(SkPaint::kStroke_Style);
      mark.setStrokeWidth(1.5f);
      mark.setColor4f(material::skia::toSkColor(kPin));
      canvas.drawCircle(path::toSk(knot.at), kReach, mark);
    }
  }
}

/** One captioned cell: the call, the drawing, then what it did. */
sketch::kit::ComparisonCase cell(const char* title, std::string key,
                                 std::vector<SkPath> strands,
                                 path::CrossingRule rule, int pinned,
                                 const char* call, const char* note) {
  return {
      .title = title,
      .control = call,
      .figure = custom(std::move(key),
                       [strands = std::move(strands), rule = std::move(rule),
                        pinned](sigil::draw::Pen& pen) {
                         SkCanvas& canvas = *pen.canvas();
                         paintWeave(canvas, strands, rule, pinned);
                       })
                    .width(kCell)
                    .height(kCell)
                    .fill(var("palette-cell-ground")),
      .note = note};
}

/** One woven cell: a chamfered plaque whose outline carries the braid,
 *  and @p rule deciding every knot the weave finds on it. */
sketch::kit::ComparisonCase woven(const char* title, path::CrossingRule rule,
                                  const char* call, const char* note) {
  return {
      .title = title,
      .control = call,
      .figure =
          box()
              .width(kCell)
              .height(kCell)
              .fill(var("palette-cell-ground"))
              .justifyContent(Justify::Center)
              .alignItems(Align::Center)
              .children(
                  {box()
                       .width(kPlaque)
                       .height(kPlaque)
                       .shape(shapes::chamfered(18))
                       .stroke(Decoration(brush::weave(
                           kit::braid(kStrands, kAmplitude, kWavelength,
                                      Decoration(brush::layers(
                                          {Decoration(brush::solid(
                                               6.0f, Fill::color(kCasing))),
                                           Decoration(brush::solid(
                                               3.5f, Fill::color(kCore)))}))),
                           std::move(rule))))}),
      .note = note};
}

}  // namespace

struct CrossingRuleSheet {
  void setup(sketch::Scene2DContext& ctx) {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(
        ctx, {.theme = sheetTheme(), .size = kCanvas, .captureAt = 0.05});

    const auto sevenCycle = [] {
      std::vector<std::pair<int, int>> dominance;
      for (int strand = 0; strand < 7; ++strand)
        dominance.emplace_back(strand, (strand + 1) % 7);
      return path::crossing::pairs(dominance);
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "Who passes over whom?",
         .subtitle = "Read down to compare topology. Read across to compare "
                     "the rule that decides each crossing.",
         .footer = "A cyclic dominance relation cannot be expressed by "
                   "painting whole strands in one global order."},
        box().column().gap(24).children(
            {sketch::kit::section(
                 {.label = "SEVEN STRANDS · SEVEN CROSSINGS",
                  .note = "An odd cycle exposes the alternating seam"},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("ALTERNATE", "hept.alternate", heptagram(),
                                path::crossing::alternate(), -1, "alternate()",
                                "Seven crossings cannot close an alternating "
                                "cycle; one seam repeats."),
                           cell("REPEAT A SEQUENCE", "hept.sequence",
                                heptagram(),
                                path::crossing::sequence({path::Order::Over,
                                                          path::Order::Over,
                                                          path::Order::Under}),
                                -1, "sequence(Over, Over, Under)",
                                "Repeat over, over, under by the crossing "
                                "ordinal."),
                           cell("STRAND DOMINANCE", "hept.pairs", heptagram(),
                                sevenCycle(), -1, "pairs(i, i + 1)",
                                "Each strand dominates the next around a "
                                "cycle."),
                           cell("PIN ONE CROSSING", "hept.except", heptagram(),
                                path::crossing::alternate().except(
                                    0, path::Order::Under),
                                0, "alternate().except(0, Under)",
                                "The ring marks the crossing whose decision "
                                "was "
                                "overridden.")},
                      .measure = 1020,
                      .gap = 20})),
             sketch::kit::section(
                 {.label = "THREE RINGS · SIX CROSSINGS",
                  .note = "The geometry stays fixed; only the policy changes"},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("ALTERNATE", "ring.alternate", rings(),
                                path::crossing::alternate(), -1, "alternate()",
                                "Alternating six crossing ordinals does not "
                                "produce a cyclic weave."),
                           cell("REPEAT A SEQUENCE", "ring.sequence", rings(),
                                path::crossing::sequence({path::Order::Over,
                                                          path::Order::Under,
                                                          path::Order::Under}),
                                -1, "sequence(Over, Under, Under)",
                                "Repeat over, under, under across the six "
                                "crossings."),
                           cell("STRAND DOMINANCE", "ring.pairs", rings(),
                                path::crossing::pairs({{0, 1}, {1, 2}, {2, 0}}),
                                -1, "pairs(0→1, 1→2, 2→0)",
                                "Every ring passes over one neighbour and "
                                "under the other."),
                           cell("PIN ONE CROSSING", "ring.except", rings(),
                                path::crossing::pairs({{0, 1}, {1, 2}, {2, 0}})
                                    .except(3, path::Order::Under),
                                3, "pairs(...).except(3, Under)",
                                "The ring marks the corrected crossing in the "
                                "cyclic weave.")},
                      .measure = 1020,
                      .gap = 20})),
             sketch::kit::section(
                 {.label = "THE SAME RULES THROUGH brush::weave",
                  .note = "braid(3, 10, 84) round one plaque; the weave "
                          "finds each knot"},
                 sketch::kit::comparison(
                     {.cases =
                          {woven("ALTERNATE", path::crossing::alternate(),
                                 "alternate()",
                                 "Over, under, over round the outline: the "
                                 "ordinary braid."),
                           woven("REPEAT A SEQUENCE",
                                 path::crossing::sequence({path::Order::Over,
                                                           path::Order::Over,
                                                           path::Order::Under}),
                                 "sequence(Over, Over, Under)",
                                 "The same sequence value as above, read "
                                 "along the plaque's knots."),
                           woven(
                               "STRAND DOMINANCE",
                               path::crossing::pairs({{0, 1}, {1, 2}, {2, 0}}),
                               "pairs(0→1, 1→2, 2→0)",
                               "Each strand over the next, all the way "
                               "round: the cycle a draw order cannot "
                               "spell."),
                           woven("PIN ONE CROSSING",
                                 path::crossing::alternate().except(
                                     0, path::Order::Under),
                                 "alternate().except(0, Under)",
                                 "The first knot overridden; the rest still "
                                 "alternate.")},
                      .measure = 1020,
                      .gap = 20}))})));
  }
};

SIGIL_SKETCH(CrossingRuleSheet, "Kit · API",
             "who passes over whom: discoverCrossings numbering the knots, "
             "the four CrossingRule spellings deciding each, crossingPatch "
             "bounding the repaint, and the same rules handed to "
             "brush::weave braiding a plaque's outline")
