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

#include <sigilcompose/core/Core.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkPathBuilder.h>

#include <cmath>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1180, 660};
constexpr float kCell = 250;   // the drawn square of one cell
constexpr float kReach = 15;   // a strand's full mark width, px
constexpr float kPatchRadius = 30;  // the cap on one patch's reach, px

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.11f, 0.11f, 0.13f, 1};
constexpr SkColor4f kCasing{0.05f, 0.05f, 0.06f, 1};
constexpr SkColor4f kCore{0.86f, 0.80f, 0.66f, 1};
constexpr SkColor4f kPin{0.92f, 0.36f, 0.30f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

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

/** One captioned cell: the drawing, then the rule that made it. */
Element cell(std::string key, std::vector<SkPath> strands,
             path::CrossingRule rule, int pinned, const char* caption) {
  return box()
      .column()
      .gap(7)
      .child(custom(key,
                    [strands = std::move(strands), rule = std::move(rule),
                     pinned](SkCanvas& canvas, const PaintContext&) {
                      paintWeave(canvas, strands, rule, pinned);
                    })
                 .width(kCell)
                 .height(kCell)
                 .fill(Fill::color(kCellGround)))
      .child(text(toU8(caption), label(11.5f, kAsh, 0.4f)).width(kCell));
}

}  // namespace

struct CrossingRuleSheet final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    const auto sevenCycle = [] {
      std::vector<std::pair<int, int>> d;
      for (int i = 0; i < 7; ++i) d.emplace_back(i, (i + 1) % 7);
      return path::crossing::pairs(d);
    };

    ctx.composer.render(
        box()
            .column()
            .padding(26, 22)
            .gap(16)
            .child(text(toU8("CROSSING RULE \xc2\xb7 discoverCrossings + "
                             "CrossingRule + crossingPatch"),
                        label(14, kInk, 2.4f)))
            .child(text(toU8("dials \xc2\xb7 the rule (below each cell) "
                             "\xc2\xb7 the patch width (reach 15 px, cap 30 "
                             "px)"),
                        label(11.5f, kAsh, 0.8f)))
            .child(
                box()
                    .row()
                    .gap(14)
                    .child(cell("hept.alternate", heptagram(),
                                path::crossing::alternate(), -1,
                                "{7/2} heptagram \xc2\xb7 crossing::alternate() "
                                "\xe2\x80\x94 seven knots, so the over-under "
                                "run cannot close and one seam doubles"))
                    .child(cell("hept.sequence", heptagram(),
                                path::crossing::sequence(
                                    {path::Order::Over, path::Order::Over,
                                     path::Order::Under}),
                                -1,
                                "crossing::sequence({Over, Over, Under}) "
                                "\xe2\x80\x94 any repeating pattern, read off "
                                "the knot's ordinal"))
                    .child(cell("hept.pairs", heptagram(), sevenCycle(), -1,
                                "crossing::pairs({{i, i+1}}) \xe2\x80\x94 "
                                "strand dominance round a seven-cycle, which "
                                "no draw order can spell"))
                    .child(cell("hept.except", heptagram(),
                                path::crossing::alternate().except(
                                    0, path::Order::Under),
                                0,
                                "alternate().except(0, Under) \xe2\x80\x94 one "
                                "positional pin, ringed; pins move when the "
                                "geometry does")))
            .child(
                box()
                    .row()
                    .gap(14)
                    .child(cell("ring.alternate", rings(),
                                path::crossing::alternate(), -1,
                                "three rings \xc2\xb7 crossing::alternate() "
                                "\xe2\x80\x94 six knots alternating by "
                                "ordinal, which is not a weave here"))
                    .child(cell("ring.sequence", rings(),
                                path::crossing::sequence(
                                    {path::Order::Over, path::Order::Under,
                                     path::Order::Under}),
                                -1,
                                "crossing::sequence({Over, Under, Under}) "
                                "\xe2\x80\x94 the same six knots on a "
                                "three-long pattern"))
                    .child(cell("ring.pairs", rings(),
                                path::crossing::pairs({{0, 1}, {1, 2}, {2, 0}}),
                                -1,
                                "crossing::pairs({{0,1},{1,2},{2,0}}) "
                                "\xe2\x80\x94 the cyclic dominance: every ring "
                                "over one and under another"))
                    .child(cell("ring.except", rings(),
                                path::crossing::pairs({{0, 1}, {1, 2}, {2, 0}})
                                    .except(3, path::Order::Under),
                                3,
                                "pairs(...).except(3, Under) \xe2\x80\x94 the "
                                "cycle with knot 3 corrected by hand, ringed"))));
  }
};

SIGIL_SKETCH(CrossingRuleSheet, "Kit \xc2\xb7 API",
             "who passes over whom: discoverCrossings numbering the knots, "
             "the four CrossingRule spellings deciding each, and "
             "crossingPatch bounding the repaint")
