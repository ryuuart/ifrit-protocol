/** @file
 * Stroke passes at paint: each pass's claim on the outline resolved to spans,
 * the collision warning, and the per-instance resolution of gate values,
 * the path phase, the track progresses and a bound fill.
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
#include "PaintInternal.h"
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

std::vector<std::vector<Span>> detail::Instance::resolveSpans(
    const SkPath& outline) const {
  std::vector<std::vector<Span>> out;
  const ElementNode& node = *desc;
  if (!node.hasStrokePasses()) return out;
  const std::vector<StrokePass>& passes = node.strokeData->passes;
  out.resize(passes.size());

  // Every animatable endpoint, resolved for this frame, in the order the
  // description declared them — the order spanAnims is indexed by.
  std::vector<float> values;
  values.reserve(spanAnims.size());
  size_t slot = 0;
  auto push = [&](const Animatable<float>& v) {
    const AnimatedFloat* a =
        slot < spanAnims.size() ? spanAnims[slot].get() : nullptr;
    values.push_back(resolveFloatAt(a, v));
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
  in.fitRects = &spanFitRects;

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

std::vector<float> detail::Instance::resolveGateValues() const {
  std::vector<float> values;
  const ElementNode& node = *desc;
  if (!node.hasMasks()) return values;
  size_t slot = 0;
  const auto push = [&](const Animatable<float>& v) {
    const AnimatedFloat* a =
        slot < maskAnims.size() ? maskAnims[slot].get() : nullptr;
    values.push_back(resolveFloatAt(a, v));
    ++slot;
  };
  for (const Mask& m : node.fxData->masks) {
    if (m.with.kind == Gate::Kind::Spans)
      for (const Spans::Term& t : m.with.where.terms) {
        push(t.begin);
        push(t.end);
        push(t.offset);
      }
    else if (m.with.kind == Gate::Kind::Edge)
      push(m.with.fraction);
  }
  return values;
}

float detail::Instance::resolvePathAt() const {
  if (!desc || !desc->textData) return 0.0f;
  const std::optional<TextPath>& baseline = desc->textData->onPath;
  if (!baseline) return 0.0f;
  return resolveFloat(kTextPathAt, baseline->at);
}

std::vector<float> detail::Instance::resolveTrackValues() const {
  std::vector<float> values;
  const std::span<const Track> tracks =
      desc->textData ? std::span<const Track>(desc->textData->tracks)
                     : std::span<const Track>();
  values.reserve(tracks.size());
  for (size_t i = 0; i < tracks.size(); ++i) {
    const AnimatedFloat* a =
        i < trackAnims.size() ? trackAnims[i].get() : nullptr;
    values.push_back(resolveFloatAt(a, tracks[i].progress));
  }
  return values;
}

Fill detail::Instance::resolveBoundFill() const {
  const ElementNode& node = *desc;
  if (node.paint.fill)
    if (const choreograph::Output<Fill>* binding = node.paint.fill->binding())
      return binding->value();
  return {};
}

std::array<float, 2> detail::Instance::resolvePatternOffset() const {
  // Only the TOP-LEVEL bound offset of the node's fill material is a
  // scalar-lane input. A nested one (in a blend layer, in a child slot)
  // keeps the material on the opaque live path — see
  // animatedBeyondBoundOffset — and never reaches this lane. All-zero when
  // unbound, matching the ContentScalars guard, so a node without the
  // channel compares equal to itself forever.
  const Material* m = liveMaterialOf(*desc);
  if (!m || !m->hasBoundOffset()) return {};
  const SkPoint pan = m->boundOffsetValue();
  return {pan.x(), pan.y()};
}

}  // namespace sigil::compose
