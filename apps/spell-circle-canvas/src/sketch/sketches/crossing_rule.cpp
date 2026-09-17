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
 * Two subjects, four rules. The {7/2} heptagram is seven chords, so it has
 * seven knots and an odd count, which is why `alternate()` cannot close and
 * leaves one seam. The three rings are three strands with six knots, which
 * is where the cyclic `pairs` dominance is exactly right: 0 over 1 over 2
 * over 0 is the impossible braid, and no ordering of a draw list can spell
 * it.
 *
 * EDIT THESE FIRST
 *   kReach       — the full width of a strand's mark, px. The patch is
 *                  sized from it.
 *   kPatchRadius — the cap on a patch's reach from its knot, px. Half the
 *                  arc distance to the neighbouring knot; larger and two
 *                  lenses merge and one strand claims both.
 */

// TAGS: Geometry/Diagrams

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace arrange = sigil::geometry::arrange;
namespace path = sigil::geometry::path;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 950};
constexpr float kCell = 240;        // the drawn square of one cell
constexpr float kReach = 15;        // a strand's full mark width, px
constexpr float kPatchRadius = 30;  // the cap on one patch's reach, px

constexpr SkColor4f kCasing{0.05f, 0.05f, 0.06f, 1};
constexpr SkColor4f kCore{0.86f, 0.80f, 0.66f, 1};
constexpr SkColor4f kPin{0.92f, 0.36f, 0.30f, 1};

/** The specimen sheet, in this one's own look.
 *
 *  A hot-reload fixture grounds a copy of this file in a colour of its
 *  own by writing one into this theme's palette and reads the corner
 *  pixel back, so the canvas and the page must both take their ground
 *  from here — which they do, because the theme is bound before
 *  `stage()` runs. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  // Behind one cell's specimen, a shade off the sheet's own ground.
  look.palette.cellGround = {0.11f, 0.11f, 0.13f, 1};
  look.type.captionLabel = {.size = 11.5f, .track = 0.6f};
  return look;
}

/** Seven chords of a regular heptagon, each joining a vertex to the one
 *  two round — the {7/2} star as seven separate strands, because a
 *  crossing is between two strands and a self-crossing path has one. */
std::vector<SkPath> heptagram() {
  constexpr int n = 7;
  const float r = kCell * 0.40f;
  const SkPoint c{kCell * 0.5f, kCell * 0.5f};
  SkPoint v[n];
  for (int i = 0; i < n; ++i)
    v[i] = arrange::onRing((size_t)i, n, c, {r, r}, -1.5707963f, 6.2831853f,
                           arrange::Turn::Closed);
  std::vector<SkPath> strands;
  for (int i = 0; i < n; ++i) {
    SkPathBuilder b;
    b.moveTo(v[i]).lineTo(v[(i + 2) % n]);
    strands.push_back(b.detach());
  }
  return strands;
}

/** Three rings on a triangle, overlapping pairwise — six knots, and the
 *  arrangement whose only consistent reading is a cycle. */
std::vector<SkPath> rings() {
  const float r = kCell * 0.24f;
  const SkPoint c{kCell * 0.5f, kCell * 0.52f};
  std::vector<SkPath> strands;
  for (int i = 0; i < 3; ++i) {
    const SkPoint at =
        arrange::onRing((size_t)i, 3, c, {r * 0.62f, r * 0.62f}, -1.5707963f,
                        6.2831853f, arrange::Turn::Closed);
    SkPathBuilder b;
    b.addCircle(at.fX, at.fY, r);
    strands.push_back(b.detach());
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
  casing.setColor4f(kCasing);
  SkPaint core = casing;
  core.setStrokeWidth(kReach - 5);
  core.setColor4f(kCore);

  const auto strand = [&](size_t i) {
    canvas.drawPath(strands[i], casing);
    canvas.drawPath(strands[i], core);
  };
  for (size_t i = 0; i < strands.size(); ++i) strand(i);

  for (const path::Crossing& x : path::discoverCrossings(strands)) {
    const size_t over = rule.decide(x) == path::Order::Over ? x.a : x.b;
    canvas.save();
    canvas.clipPath(path::crossingPatch(strands[x.a], kReach, strands[x.b],
                                        kReach, x.at, kPatchRadius),
                    true);
    strand(over);
    canvas.restore();
    if ((int)x.index == pinnedIndex) {
      SkPaint mark;
      mark.setAntiAlias(true);
      mark.setStyle(SkPaint::kStroke_Style);
      mark.setStrokeWidth(1.5f);
      mark.setColor4f(kPin);
      canvas.drawCircle(x.at, kReach, mark);
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
                        pinned](SkCanvas& canvas) {
                         paintWeave(canvas, strands, rule, pinned);
                       })
                    .width(kCell)
                    .height(kCell)
                    .fill(Fill::color(sketch::kit::theme().palette.cellGround)),
      .note = note};
}

}  // namespace

struct CrossingRuleSheet {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const auto sevenCycle = [] {
      std::vector<std::pair<int, int>> d;
      for (int i = 0; i < 7; ++i) d.emplace_back(i, (i + 1) % 7);
      return path::crossing::pairs(d);
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "Who passes over whom?",
         .subtitle = "Read down to compare topology. Read across to compare "
                     "the rule that decides each crossing.",
         .footer = "A cyclic dominance relation cannot be expressed by "
                   "painting whole strands in one global order."},
        box().column().gap(24).children(
            {sketch::kit::sectionHeader(
                 {.label = "SEVEN STRANDS · SEVEN CROSSINGS",
                  .note = "An odd cycle exposes the alternating seam"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("ALTERNATE", "hept.alternate", heptagram(),
                            path::crossing::alternate(), -1, "alternate()",
                            "Seven crossings cannot close an alternating "
                            "cycle; one seam repeats."),
                       cell("REPEAT A SEQUENCE", "hept.sequence", heptagram(),
                            path::crossing::sequence({path::Order::Over,
                                                      path::Order::Over,
                                                      path::Order::Under}),
                            -1, "sequence(Over, Over, Under)",
                            "Repeat over, over, under by the crossing "
                            "ordinal."),
                       cell("STRAND DOMINANCE", "hept.pairs", heptagram(),
                            sevenCycle(), -1, "pairs(i, i + 1)",
                            "Each strand dominates the next around a cycle."),
                       cell("PIN ONE CROSSING", "hept.except", heptagram(),
                            path::crossing::alternate().except(
                                0, path::Order::Under),
                            0, "alternate().except(0, Under)",
                            "The ring marks the crossing whose decision was "
                            "overridden.")},
                  .measure = 1020,
                  .gap = 20}),
             sketch::kit::sectionHeader(
                 {.label = "THREE RINGS · SIX CROSSINGS",
                  .note = "The geometry stays fixed; only the policy changes"}),
             sketch::kit::comparison(
                 {.cases = {cell("ALTERNATE", "ring.alternate", rings(),
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
                            cell(
                                "STRAND DOMINANCE", "ring.pairs", rings(),
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
                  .gap = 20})})));
  }
};

SIGIL_SKETCH(CrossingRuleSheet, "Kit · API",
             "who passes over whom: discoverCrossings numbering the knots, "
             "the four CrossingRule spellings deciding each, and "
             "crossingPatch bounding the repaint")
