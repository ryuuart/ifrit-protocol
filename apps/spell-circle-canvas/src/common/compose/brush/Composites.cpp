/** @file
 * THE COMPOSITES — a brush that is other brushes. The layered stack every
 * stroke is built out of; the geometry op that deviates an outline before
 * a mark is laid on it; `brush::Weave`, which resolves a set of strands
 * into one crossing picture; `Brush` itself, whose layers each take their
 * own shapers over the pipeline's; and `brush::Restyled`, one decoration
 * on a deviated outline.
 *
 * What every one of them shares is the rule a composite must not break:
 * an inner decoration is painted against a PaintContext that is the
 * outer one with its outline replaced, so the stamp store, the matrix to
 * the root and the root's size ride through unchanged.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkStrokeRec.h>
#include <include/core/SkVertices.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/effects/SkDiscretePathEffect.h>
#include <include/pathops/SkPathOps.h>
#include <sigilcompose/brush/Brushes.h>

#include <algorithm>
#include <any>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace sigil::compose {

void LayeredBrush::paint(SkCanvas& c, const PaintContext& ctx) const {
  for (const StrokeLayer& layer : layers) {
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(layer.width);
    p.setStrokeCap(layer.roundCap ? SkPaint::kRound_Cap : SkPaint::kButt_Cap);
    p.setColor4f(layer.color, nullptr);
    p.setBlendMode(layer.blend);
    if (layer.blurSigma > 0)
      p.setMaskFilter(
          SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, layer.blurSigma));
    if (!layer.dash.empty())
      p.setPathEffect(SkDashPathEffect::Make(
          SkSpan(layer.dash.data(), layer.dash.size()), layer.dashPhase));
    c.drawPath(ctx.outline, p);
  }
}

GeometryOp::GeometryOp(geometry::path::Shaper s) : m_bleed(s.bleed()) {
  m_held = s;
  m_equals = [](const std::any& a, const std::any& b) {
    return std::any_cast<const geometry::path::Shaper&>(a) ==
           std::any_cast<const geometry::path::Shaper&>(b);
  };
  m_apply = [held = std::move(s)](const SkPath& p) { return held.shape(p); };
}

namespace brush {

Solid solid(float width, Fill fill, PathFormat::Align align) {
  Solid s;
  s.width = width;
  s.strokeFill = std::move(fill);
  s.align = align;
  return s;
}

void Weave::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (strands.empty()) return;
  // 1. Resolve every strand's geometry. A relative strand is a
  //    displacement of the boundary in the (along, across) frame the
  //    band owns; an absolute one brings its own path — and if NO
  //    strand is relative, the boundary is simply an unpainted host.
  std::vector<SkPath> paths;
  paths.reserve(strands.size());
  for (const Strand& s : strands) {
    switch (s.path.source()) {
      case StrandPath::Source::Relative:
        paths.push_back(
            s.path.profile().max() == 0.0f
                ? ctx.outline
                : geometry::path::profileOffset(ctx.outline, s.path.profile()));
        break;
      case StrandPath::Source::Borrowed:
        paths.push_back(ctx.borrowedPath(s.path.key()));
        break;
      case StrandPath::Source::Authored:
        paths.push_back(s.path.path());
        break;
    }
  }

  const auto paintStrand = [&](size_t i) {
    // The context is the enclosing one with a different outline: every
    // other member — the stamp cache, the matrix to the root, the root's
    // size — is what the node was painted with, and a strand that lost
    // them would re-rasterise its stamps every frame and anchor a
    // world-space material to itself.
    PaintContext sub = ctx;
    sub.outline = paths[i];
    strands[i].brush.paint(c, sub);
  };

  // 2. List order first — the whole picture, correct wherever nothing
  //    crosses, which is every layers() and most of any weave.
  for (size_t i = 0; i < paths.size(); ++i) paintStrand(i);

  // 3. Repair the crossings the rule disagrees with. Crossings are
  //    DISCOVERED, never authored, and memoized on the resolved paths:
  //    a frame whose geometry did not move pays one vector-of-paths
  //    comparison instead of the flatten-and-test. The cache changes
  //    WHEN discovery runs and never what is drawn.
  if (!crossingCache->valid || crossingCache->key != paths) {
    crossingCache->found = geometry::path::discoverCrossings(paths);
    crossingCache->key = paths;
    crossingCache->valid = true;
    ++crossingCache->computes;
  }
  const std::vector<geometry::path::Crossing>& crossings = crossingCache->found;
  if (crossings.empty()) return;
  // WHAT THE WHOLE SET SAYS, before anything is asked of one of it: a
  // rule about the WALK — over, under, over along each strand — cannot
  // be answered crossing by crossing, because nothing in a Crossing says
  // how many crossings on its strand come before it. A rule about one
  // meeting ignores this.
  crossing.prepare(crossings);
  const auto reachOf = [&](size_t i) {
    // The MARK's full width, not the cull's bleed(): an Align::Inner
    // stroke bleeds zero while painting a mark `width` wide, so a region
    // built from bleed() would be too small to cover its own crossing.
    return patch > 0 ? patch : std::max(strands[i].brush.reach(ctx.size), 1.0f);
  };

  // Each strand's arc length, so a crossing's `along` fractions convert to
  // px — which is what bounds one knot's patch away from its neighbours'.
  // Also whether the strand is a CYCLE, because on a closed contour the
  // fractions 0.02 and 0.98 are neighbours, not opposites.
  std::vector<float> lengths(paths.size(), 0.0f);
  std::vector<char> cyclic(paths.size(), 0);
  for (size_t i = 0; i < paths.size(); ++i) {
    SkContourMeasureIter it(paths[i], false);
    int contours = 0;
    bool lastClosed = false;
    while (sk_sp<SkContourMeasure> m = it.next()) {
      lengths[i] += m->length();
      lastClosed = m->isClosed();
      ++contours;
    }
    // ONE closed contour, and no more: with several contours the `along`
    // parameter runs them end to end, so its two ends are not adjacent
    // and wrapping would be a lie.
    cyclic[i] = (contours == 1 && lastClosed) ? 1 : 0;
  }
  // THE KNOT'S TERRITORY: half the arc distance to the nearest adjacent
  // crossing, on either strand, whichever is closer. Without it the
  // lenses of an ordinary braid touch, pathops merges them into ONE
  // contour, and crossing 0's patch owns the whole run — the weave then
  // reads as a single strand laid on top of the others.
  const auto positionOn = [](const geometry::path::Crossing& x,
                             size_t strandIndex) {
    return x.a == strandIndex ? x.alongA : x.alongB;
  };
  const auto territoryOf = [&](const geometry::path::Crossing& x) {
    float limit = std::numeric_limits<float>::max();
    for (const size_t s : {x.a, x.b}) {
      const float mine = positionOn(x, s);
      for (const geometry::path::Crossing& other : crossings) {
        if (&other == &x || (other.a != s && other.b != s)) continue;
        float delta = std::abs(positionOn(other, s) - mine);
        // On a CYCLE the seam is not a boundary: two knots at 0.02 and
        // 0.98 sit 4% apart, not 96%. Without the wrap, crossings
        // straddling the seam read as maximally distant, their bound
        // vanishes, and their lenses merge — on two overlapping rings
        // that puts both knots in one patch painted in one colour.
        //
        // Conditional on closedness, because wrapping an OPEN strand
        // whose crossings sit near its two ends would over-clip: those
        // ends really are far apart.
        if (cyclic[s]) delta = std::min(delta, 1.0f - delta);
        const float gap = delta * lengths[s];
        if (gap > 0.01f) limit = std::min(limit, gap * 0.5f);
      }
    }
    // No neighbour on either strand: the lens needs no bound, and a
    // number large enough to contain it is the honest spelling of that.
    if (limit == std::numeric_limits<float>::max()) limit = 1e6f;
    return limit;
  };

  for (const geometry::path::Crossing& x : crossings) {
    const geometry::path::Order order = crossing.decide(x);
    const size_t top = order == geometry::path::Order::Over ? x.a : x.b;
    // `b` painted later, so it is already on top. Nothing to do.
    if (top == x.b) continue;
    c.save();
    c.clipPath(
        geometry::path::crossingPatch(paths[x.a], reachOf(x.a), paths[x.b],
                                      reachOf(x.b), x.at, territoryOf(x)),
        true);
    paintStrand(top);
    c.restore();
  }
}

Weave layers(std::vector<Decoration> stack) {
  Weave w;
  w.strands.reserve(stack.size());
  for (Decoration& d : stack)
    w.strands.push_back(Strand{geometry::path::profile::self(), std::move(d)});
  return w;
}

Weave weave(std::vector<Strand> strands, geometry::path::CrossingRule rule) {
  Weave w;
  w.strands = std::move(strands);
  w.crossing = std::move(rule);
  return w;
}

}  // namespace brush

void Brush::paint(SkCanvas& c, const PaintContext& ctx) const {
  SkPath styled = ctx.outline;
  for (const geometry::path::Shaper& g : pipeline) styled = g.shape(styled);
  for (const Layer& l : layers) {
    SkPath layerPath = styled;
    for (const geometry::path::Shaper& g : l.shapers)
      layerPath = g.shape(layerPath);
    PaintContext restyled = ctx;
    restyled.outline = std::move(layerPath);
    l.dec.paint(c, restyled);
  }
}

namespace brush {

void Restyled::paint(SkCanvas& c, const PaintContext& ctx) const {
  // No null check: GeometryOp::apply passes the path through unchanged
  // when it holds nothing.
  PaintContext restyled = ctx;
  restyled.outline = op.apply(ctx.outline);
  inner.paint(c, restyled);
}

}  // namespace brush

}  // namespace sigil::compose
