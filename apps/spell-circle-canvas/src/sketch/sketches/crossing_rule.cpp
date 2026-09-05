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

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1120, 736};
constexpr float kCell = 250;        // the drawn square of one cell
constexpr float kReach = 15;        // a strand's full mark width, px
constexpr float kPatchRadius = 30;  // the cap on one patch's reach, px

constexpr SkColor4f kCellGround{0.11f, 0.11f, 0.13f, 1};
constexpr SkColor4f kCasing{0.05f, 0.05f, 0.06f, 1};
constexpr SkColor4f kCore{0.86f, 0.80f, 0.66f, 1};
constexpr SkColor4f kPin{0.92f, 0.36f, 0.30f, 1};

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.type.captionLabel = {.size = 11.5f, .track = 0.6f};
  look.type.captionNote = {.size = 11, .track = 0.3f};
  return look;
}

/** The one voice every cell on this sheet is captioned in: the call over
 *  the picture, what it did under it, both ranged left at the cell's
 *  width. */
/** Seven chords of a regular heptagon, each joining a vertex to the one
 *  two round — the {7/2} star as seven separate strands, because a
 *  crossing is between two strands and a self-crossing path has one. */
std::vector<SkPath> heptagram() {
  constexpr int n = 7;
  const float r = kCell * 0.40f;
  const SkPoint c{kCell * 0.5f, kCell * 0.5f};
  SkPoint v[n];
  for (int i = 0; i < n; ++i) {
    const float a = -1.5707963f + 6.2831853f * (float)i / (float)n;
    v[i] = {c.fX + r * std::cos(a), c.fY + r * std::sin(a)};
  }
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
    const float a = -1.5707963f + 2.0943951f * (float)i;
    SkPathBuilder b;
    b.addCircle(c.fX + r * 0.62f * std::cos(a), c.fY + r * 0.62f * std::sin(a),
                r);
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
Element cell(std::string key, std::vector<SkPath> strands,
             path::CrossingRule rule, int pinned, const char* call,
             const char* note) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      custom(std::move(key),
             [strands = std::move(strands), rule = std::move(rule), pinned](
                 SkCanvas& canvas, const PaintContext&) {
               paintWeave(canvas, strands, rule, pinned);
             })
          .width(kCell)
          .height(kCell)
          .fill(Fill::color(kCellGround)));
}

}  // namespace

struct CrossingRuleSheet final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const auto sevenCycle = [] {
      std::vector<std::pair<int, int>> d;
      for (int i = 0; i < 7; ++i) d.emplace_back(i, (i + 1) % 7);
      return path::crossing::pairs(d);
    };

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("CROSSING RULE \xc2\xb7 discoverCrossings + "
                       "CrossingRule + crossingPatch"),
         .subtitle = toU8("dials \xc2\xb7 the rule (named on each cell) "
                          "\xc2\xb7 the patch width (reach 15 px, cap "
                          "30 px)"),
         .footer = toU8("a knot is decided, never drawn in order "
                        "\xe2\x80\x94 the cyclic dominance in the "
                        "third ring cell has no draw order at all")},
        kit::cells(
            {.cells = {kit::cells(
                           {.cells = {cell(
                                          "hept.alternate", heptagram(),
                                          path::crossing::alternate(), -1,
                                          "crossing::alternate()",
                                          "{7/2} heptagram \xe2\x80\x94 seven "
                                          "knots, so the over-under run cannot "
                                          "close and one seam doubles"),
                                      cell(
                                          "hept.sequence", heptagram(),
                                          path::crossing::sequence(
                                              {path::Order::Over,
                                               path::Order::Over,
                                               path::Order::Under}),
                                          -1,
                                          "crossing::sequence({Over, Over, "
                                          "Under})",
                                          "any repeating pattern, read off the "
                                          "knot's ordinal"),
                                      cell("hept.pairs",
                                           heptagram(), sevenCycle(),
                                           -1, "crossing::pairs({{i, i+1}})",
                                           "strand dominance round a "
                                           "seven-cycle, "
                                           "which no draw order can spell"),
                                      cell(
                                          "hept.except", heptagram(),
                                          path::crossing::alternate().except(0,
                                                                             path::Order::Under),
                                          0, "alternate().except(0, Under)",
                                          "one positional pin, ringed; pins "
                                          "move "
                                          "when the geometry does")},
                            .gap = 14}),
                       kit::cells({.cells = {cell("ring.alternate",
                                                  rings(), path::crossing::alternate(),
                                                  -1, "crossing::alternate()",
                                                  "three rings \xe2\x80\x94 "
                                                  "six knots "
                                                  "alternating by ordinal, "
                                                  "which is not a "
                                                  "weave here"),
                                             cell(
                                                 "ring.sequence", rings(),
                                                 path::crossing::sequence(
                                                     {path::Order::Over, path::Order::Under, path::Order::Under}),
                                                 -1,
                                                 "crossing::sequence({Over, "
                                                 "Under, Under})",
                                                 "the same six knots on a "
                                                 "three-long "
                                                 "pattern"),
                                             cell(
                                                 "ring.pairs", rings(),
                                                 path::crossing::pairs(
                                                     {{0, 1}, {1, 2}, {2, 0}}),
                                                 -1,
                                                 "crossing::pairs({{0,1},{1,2},"
                                                 "{2,0}})",
                                                 "the cyclic dominance: every "
                                                 "ring over "
                                                 "one and under another"),
                                             cell(
                                                 "ring.except", rings(),
                                                 path::crossing::pairs(
                                                     {{0, 1}, {1, 2}, {2, 0}})
                                                     .except(
                                                         3, path::Order::Under),
                                                 3,
                                                 "pairs(...).except(3, Under)",
                                                 "the cycle with knot 3 "
                                                 "corrected by "
                                                 "hand, ringed")},
                                   .gap = 14})},
             .column = true,
             .gap = 18})));
  }
};

SIGIL_SKETCH(CrossingRuleSheet, "Kit \xc2\xb7 API",
             "who passes over whom: discoverCrossings numbering the knots, "
             "the four CrossingRule spellings deciding each, and "
             "crossingPatch bounding the repaint")
