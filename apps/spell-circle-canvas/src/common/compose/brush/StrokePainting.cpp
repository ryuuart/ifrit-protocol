/** @file
 * The stroke grammar's engine: each span-qualified pass's claim on the
 * outline resolved to spans with the rest() complements and the collision
 * warning, the StrokeResolver value that carries this into the kernel, and
 * the span-qualified verbs (`stroke(spans, …)`, `background(spans, …)`)
 * that install it on a description.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkStrokeRec.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/effects/SkTrimPathEffect.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/fonts/Shaper.h>  // makeFont — textFill's cap-height metrics

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "ComposeRuntime.h"
#include "SpanArithmetic.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Stroke passes: resolving each pass's claim, and saying so when two
// claims collide.

namespace {

std::string passLabel(const detail::StrokePass& pass, size_t index) {
  if (!pass.name.empty()) return "\"" + pass.name + "\"";
  return "#" + std::to_string(index);
}

/** One boundary, one mark: two claims on the same run is a mistake with
 *  no sensible rendering, so it is said out loud once per shape of the
 *  problem. Layering two marks on ONE run is a composite brush, and the
 *  message says so — that is the only place an author learns it. */
void warnOverlappingClaims(const std::string& a, const std::string& b,
                           Span shared) {
  static std::vector<std::string> seen;
  const std::string key = a + "|" + b;
  for (const std::string& k : seen)
    if (k == key) return;
  if (seen.size() >= 16) return;
  seen.push_back(key);
  SkDebugf(
      "compose: span passes %s and %s both claim %.3f–%.3f of the "
      "same boundary. One boundary, one mark: spans partition it, they "
      "do not stack — and the law reads across BOTH z-halves, so a "
      "background(spans, ...) pass and a stroke(spans, ...) pass "
      "collide the same way two strokes do. To layer two marks on one "
      "run, make them ONE pass with a composite brush "
      "(Brush{}.layer(a).layer(b), or a LayeredBrush); to keep them apart, "
      "give the second pass a disjoint span (or spans::rest()).\n",
      a.c_str(), b.c_str(), shared.begin, shared.end);
}

}  // namespace

namespace {

/** Every stroke pass's claimed runs for this frame, in pass order, with
 *  rest() complements applied — the body behind StrokeResolverOps::claims. */
std::vector<std::vector<Span>> resolveSpans(const Instance& inst,
                                            const SkPath& outline) {
  std::vector<std::vector<Span>> out;
  const ElementNode& node = *inst.desc;
  if (!node.hasStrokePasses()) return out;
  const std::vector<StrokePass>& passes = node.strokeData->passes;
  out.resize(passes.size());

  // Every animatable endpoint, resolved for this frame, in the order the
  // description declared them — the order spanAnims is indexed by.
  std::vector<float> values;
  values.reserve(inst.spanAnims.size());
  size_t slot = 0;
  auto push = [&](const motion::Animatable<float>& v) {
    const AnimatedFloat* a =
        slot < inst.spanAnims.size() ? inst.spanAnims[slot].get() : nullptr;
    values.push_back(inst.resolveFloatAt(a, v));
    ++slot;
  };
  for (const StrokePass& pass : passes)
    for (const Spans::Term& term : pass.where.terms) {
      push(term.begin);
      push(term.end);
      push(term.offset);
    }

  SpanInput in;
  in.outline = &outline;
  in.fitRects = &inst.spanFitRects;

  size_t valueBase = 0;
  for (size_t i = 0; i < passes.size(); ++i) {
    std::vector<float> mine(
        values.begin() + (long)valueBase,
        values.begin() + (long)(valueBase + passes[i].where.valueCount()));
    valueBase += passes[i].where.valueCount();
    in.values = &mine;
    out[i] = passes[i].where.resolve(in);
  }

  // rest(): the complement, resolved AFTER the claims it is defined
  // against. Bare rest() takes everything the other CLAIMING passes left;
  // rest("name") is one named pass's complement and may overlay.
  for (size_t i = 0; i < passes.size(); ++i) {
    if (!passes[i].where.hasRest()) continue;
    std::vector<Span> against;
    bool named = false;
    for (const Spans::Term& term : passes[i].where.terms) {
      if (term.rule != Spans::Rule::Rest || term.key.empty()) continue;
      named = true;
      for (size_t j = 0; j < passes.size(); ++j)
        if (passes[j].name == term.key)
          against.insert(against.end(), out[j].begin(), out[j].end());
    }
    if (!named)
      for (size_t j = 0; j < passes.size(); ++j)
        if (j != i && !passes[j].where.hasRest())
          against.insert(against.end(), out[j].begin(), out[j].end());
    std::vector<Span> rest = complementSpans(normalizeSpans(against));
    // A pass may union rest() with explicit terms; keep both.
    rest.insert(rest.end(), out[i].begin(), out[i].end());
    out[i] = normalizeSpans(rest);
  }

  // The no-overlap law, over the CLAIMING passes only. An unqualified
  // stroke never gets here — it is an ordinary foreground — so overlaying
  // marks on a whole boundary is never diagnosed as a claim collision.
  for (size_t i = 0; i < passes.size(); ++i) {
    if (passes[i].where.hasRest()) continue;
    for (size_t j = i + 1; j < passes.size(); ++j) {
      if (passes[j].where.hasRest()) continue;
      if (std::optional<Span> shared = spansOverlap(out[i], out[j]))
        warnOverlappingClaims(passLabel(passes[i], i), passLabel(passes[j], j),
                              *shared);
    }
  }
  return out;
}

/** The engine as a StrokeResolverOps: every operation forwards to the
 *  span arithmetic and the claim resolution above. One value for every
 *  stroked node. */
struct StrokeEngine final : StrokeResolverOps {
  bool operator==(const StrokeEngine&) const { return true; }
  std::vector<Span> normalize(const std::vector<Span>& spans) const override {
    return normalizeSpans(spans);
  }
  std::vector<Span> intersect(const std::vector<Span>& a,
                              const std::vector<Span>& b) const override {
    return intersectSpans(a, b);
  }
  std::vector<Span> complement(const std::vector<Span>& spans) const override {
    return complementSpans(spans);
  }
  SkPath spanPath(const SkPath& src,
                  const std::vector<Span>& spans) const override {
    return detail::spanPath(src, spans);
  }
  std::vector<std::vector<Span>> claims(const Instance& inst,
                                        const SkPath& outline) const override {
    return resolveSpans(inst, outline);
  }
  SkPath bandRegion(const SkPath& spine, const Across& width,
                    Formation formation) const override {
    return detail::bandRegion(spine, width, formation);
  }
};

}  // namespace

const StrokeResolver& detail::strokeResolver() {
  static const StrokeResolver kResolver{StrokeEngine{}};
  return kResolver;
}

// ---------------------------------------------------------------------------
// The span-qualified verbs: declared on Element by the kernel, defined here
// so a description that claims runs of its boundary carries the engine
// that resolves them.

Element& Element::stroke(Spans where, Decoration what, std::string name) {
  return addSpanPass(std::move(where), std::move(what), std::move(name),
                     (int)detail::StrokePass::Half::Foreground);
}

Element& Element::background(Spans where, Decoration what, std::string name) {
  return addSpanPass(std::move(where), std::move(what), std::move(name),
                     (int)detail::StrokePass::Half::Background);
}

/** The one body both span-qualified slots share — see StrokePass: the two
 *  halves differ only in where the mark lands, so everything upstream of
 *  the paint (the fit() borrows, the claim ledger, the pass list) is one
 *  thing and must stay one thing. */
Element& Element::addSpanPass(Spans where, Decoration what, std::string name,
                              int half) {
  // A fit() term borrows another element's resolved box, so the keys ride
  // into DeriveData where the ONE derive-registration walk finds them —
  // the flowAround pattern, not a second phase.
  for (const Spans::Term& t : where.terms)
    if (t.rule == Spans::Rule::Fit && !t.key.empty()) {
      detail::DeriveData& derive = m_node->deriveData.ensure();
      derive.spanFitKeys.push_back(t.key);
      // A gap sized from where a node LANDED is a read of its box.
      derive.reads.push_back({t.key, sigil::core::Facet::Bounds});
    }
  claimBorrows(what);
  detail::StrokeData& strokes = m_node->strokeData.ensure();
  if (!strokes.resolver) strokes.resolver = detail::strokeResolver();
  strokes.passes.push_back(detail::StrokePass{std::move(where), std::move(what),
                                              std::move(name),
                                              (detail::StrokePass::Half)half});
  return *this;
}

}  // namespace sigil::compose
