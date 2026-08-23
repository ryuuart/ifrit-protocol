// Paint phase: the volatility walk that decides what may cache, the node
// silhouette resolution, and the stacking painter with its cache tiers —
// live paint, an automatic SkPicture over provably-static subtrees, a bake
// of the node's own paint with live children over it, a whole-subtree bake
// held by a value memo, and Cache::Texture raster bakes.

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
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
#include <sigilimage/ImageAsset.h>
#include <sigilweave/Choreograph.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/Shaper.h>  // makeFont — textFill's cap-height metrics

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Null-safe views into ElementNode's rare-field blocks (see ComposeInternal.h)

namespace {

inline const Material* liveMaterialOf(const ElementNode& n) {
  return n.materialData && n.materialData->live ? &*n.materialData->live
                                                : nullptr;
}
inline const Material* metricFillOf(const ElementNode& n) {
  return n.textData && n.textData->metricFill ? &*n.textData->metricFill
                                              : nullptr;
}
/** The node's fx() tracks, or an empty span. */
inline std::span<const Track> tracksOf(const ElementNode& n) {
  return n.textData ? std::span<const Track>(n.textData->tracks)
                    : std::span<const Track>();
}
/** Does this node draw its text through the fx() path? A track list whose
 *  every effect is empty is not fx text — the same test the volatility
 *  walk, the paint dispatch and the echo exclusion all read. */
inline bool hasTextFx(const ElementNode& n) {
  for (const Track& t : tracksOf(n))
    if (t.effect) return true;
  return false;
}
inline const sigil::image::ImageAsset* imageAssetOf(const ElementNode& n) {
  return n.imageData ? n.imageData->asset.get() : nullptr;
}
inline const Effect* layerEffectOf(const ElementNode& n) {
  return n.fxData && n.fxData->layerEffect ? &*n.fxData->layerEffect : nullptr;
}
inline const Effect* backdropEffectOf(const ElementNode& n) {
  return n.fxData && n.fxData->backdropEffect ? &*n.fxData->backdropEffect
                                              : nullptr;
}
inline const std::vector<Echo>& echoesOf(const ElementNode& n) {
  static const std::vector<Echo> kNoEchoes;
  return n.fxData ? n.fxData->echoes : kNoEchoes;
}

}  // namespace

// ---------------------------------------------------------------------------
// The two SUBSTITUTION gates a dressed glyph can ask for — a driven
// variable-font axis, and a code-point swap. Both replace what the SHAPER
// decided while keeping the pen positions it computed, so both are refused
// wherever that would move a letter, and both memoize their verdict: a
// verdict is a property of the face, and probing one costs metrics calls.

namespace {

/** What driving one axis on one face is allowed to do, and across what
 *  range. `min`/`max` are the axis's own design range, which is what the
 *  driven value is stepped across — a bounded step count over the range
 *  keeps the varied-clone population bounded whatever numbers an effect
 *  feeds it.
 */
struct AxisGate {
  bool allowed = false;
  float min = 0, max = 0;
};

/** The gate for (face, axis), probed once and remembered. Probing samples
 *  every glyph advance at both extremes of the axis, so it is a per-face
 *  cost and never a per-frame one. */
const AxisGate& axisGate(sigil::weave::FontContext& fonts,
                         const sk_sp<SkTypeface>& face, const char (&tag)[5]) {
  static thread_local std::map<std::pair<uint32_t, uint32_t>, AxisGate> table;
  const uint32_t axisTag = SkSetFourByteTag(tag[0], tag[1], tag[2], tag[3]);
  auto [entry, fresh] =
      table.try_emplace({face ? face->uniqueID() : 0u, axisTag}, AxisGate{});
  AxisGate& gate = entry->second;
  if (!fresh) return gate;
  gate.allowed = face && fonts.axisIsAdvanceInvariant(face, tag);
  if (!gate.allowed) {
    // ONE REFUSAL FOR EVERY VERB THAT REACHES A DRAW-TIME AXIS —
    // variationDrive, an fx::axis track, spanAxis — because it is one gate
    // and they all fail it for the same reason. Naming a verb here would
    // send an author reading about the one they did not write.
    SkDebugf(
        "sigilcompose: axis \"%s\" is absent or moves advances on this font "
        "— refused (the glyphs keep the pen positions shaping gave them, so "
        "the text draws at its shaped coordinates; GRAD is the "
        "advance-invariant weight, or re-shape through a style)\n",
        tag);
    return gate;
  }
  const int count = face->getVariationDesignParameters({});
  if (count > 0) {
    std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
    face->getVariationDesignParameters({axes.data(), axes.size()});
    for (const auto& axis : axes)
      if (axis.tag == axisTag) {
        gate.min = axis.min;
        gate.max = axis.max;
      }
  }
  return gate;
}

/** How many steps the driven-axis ladder offers a glyph rendered at
 *  `pixelSize`.
 *
 *  A driven coordinate is snapped so a smooth sweep lands on a BOUNDED set
 *  of faces: every distinct coordinate is a distinct clone, a distinct batch
 *  bucket and a distinct set of glyph-atlas strikes.
 *
 *  How coarse that ladder may be is a visual question, and the answer
 *  depends on the size the glyph is rendered at. One step is a fixed
 *  distance in the axis's own design units; a design unit displaces an
 *  outline by a fixed fraction of the em; and a fixed fraction of the em is
 *  MORE PIXELS the larger the em is drawn. So one step's visible effect
 *  grows in proportion to the rendered size, and a step count that does not
 *  grow with it is a ladder that disappears on a caption and shows on a
 *  headline. It rises in proportion instead.
 *
 *  Both ends are clamped. Below the floor a finer ladder buys nothing the
 *  eye can use at any size the type is still legible at; the ceiling is what
 *  makes the retained clone population bounded at all, which is the only
 *  reason a ladder exists rather than the raw coordinate. */
int axisLadderSteps(float pixelSize) {
  constexpr float kStepsPerPixel = 4.0f;
  constexpr int kMinSteps = 64, kMaxSteps = 512;
  return std::clamp((int)std::lround(pixelSize * kStepsPerPixel), kMinSteps,
                    kMaxSteps);
}

/** The face a driven axis asks for, or null when the gate refuses it — the
 *  glyph then draws at its shaped face, which is the whole refusal.
 *
 *  Off a continuous track the coordinate is snapped to the ladder above and
 *  the clone is memoized, so the faces a scene can reach are bounded and
 *  each is rasterized once. ON one, the coordinate passes through raw and
 *  the clone is TRANSIENT: an unsnapped value has no bounded set to memoize,
 *  so retaining it would add a permanently held clone per frame for as long
 *  as the process runs. The price of the opt-out is therefore a fresh face
 *  and fresh glyph rasterization every frame — constant per frame, and
 *  exactly what "continuous" is asking for. */
sk_sp<SkTypeface> drivenFace(sigil::weave::FontContext& fonts,
                             const sk_sp<SkTypeface>& base, float pixelSize,
                             const sigil::weave::FontVariation& axis,
                             bool continuous) {
  const char tag[5] = {axis.tag[0], axis.tag[1], axis.tag[2], axis.tag[3], 0};
  const AxisGate& gate = axisGate(fonts, base, tag);
  if (!gate.allowed) return nullptr;
  sigil::weave::FontVariation coordinate = axis;
  coordinate.value = std::clamp(axis.value, gate.min, gate.max);
  if (gate.max > gate.min) {
    if (continuous)
      return fonts.variedTypefaceTransient(base, {&coordinate, 1});
    const float steps = (float)axisLadderSteps(pixelSize);
    const float span = gate.max - gate.min;
    coordinate.value =
        gate.min + std::round((coordinate.value - gate.min) / span * steps) *
                       (span / steps);
  }
  // A degenerate range offers one reachable coordinate, which is a ladder of
  // one whether or not the track asked for a ladder.
  return fonts.variedTypeface(base, {&coordinate, 1});
}

/** The glyph a code-point substitution resolves to, or 0 when it is
 *  refused.
 *
 *  A substitution draws its replacement at the ORIGINAL glyph's pen
 *  position, so it is sound exactly when the two advance the pen equally
 *  ALONG THE AXIS THAT PEN STEPS ON: a level run steps by the horizontal
 *  advance, an upright column by the vertical one. Reading the wrong axis
 *  refuses a kana-to-digit churn down a column whose glyphs all step one em
 *  down it, and admits a pair that really would shift the column below the
 *  swap. A mismatch on the measured axis is a reshape and not a redraw, so
 *  it is refused.
 *
 *  Both advances are a property of the FACE — em fractions, not pixels — so
 *  one probe answers for every size the pair is ever drawn at, and BOTH
 *  axes are read on that one probe. The axis therefore stays out of the
 *  memo key: a pair's replacement glyph is the same glyph whichever way the
 *  run flows, and only the verdict differs, so the entry carries a verdict
 *  per axis and the lookup picks the one the run asked for. Keying on the
 *  axis instead would probe the same pair twice for two facts one probe
 *  already has. */
SkGlyphID substituteGlyph(sigil::weave::FontContext& fonts,
                          const sk_sp<SkTypeface>& face, SkGlyphID original,
                          char32_t codepoint, bool vertical) {
  if (!face) return 0;
  struct Verdict {
    SkGlyphID replacement = 0;       ///< 0: the face cannot draw the code point
    bool alike[2] = {false, false};  ///< indexed by the axis: level, upright
  };
  using Key = std::tuple<uint32_t, SkGlyphID, uint32_t>;
  static thread_local std::map<Key, Verdict> table;
  auto [entry, fresh] = table.try_emplace(
      Key{face->uniqueID(), original, (uint32_t)codepoint}, Verdict{});
  Verdict& verdict = entry->second;
  if (fresh) {
    verdict.replacement = face->unicharToGlyph((SkUnichar)(uint32_t)codepoint);
    for (int axis = 0; verdict.replacement && axis < 2; ++axis) {
      // A thousandth of the em: no face's equal-advance pair misses it and
      // no proportional pair meets it.
      constexpr float kAdvanceEpsilonEm = 0.001f;
      const bool down = axis == 1;
      verdict.alike[axis] =
          std::abs(fonts.glyphAdvanceEm(face, original, down) -
                   fonts.glyphAdvanceEm(face, verdict.replacement, down)) <=
          kAdvanceEpsilonEm;
    }
  }
  if (verdict.replacement == 0) return 0;
  const int axis = vertical ? 1 : 0;
  if (verdict.alike[axis]) return verdict.replacement;
  // Once per face and axis: a scramble over a proportional charset would
  // otherwise report every character of it, one line each.
  static thread_local std::unordered_set<uint64_t> warned;
  if (warned.insert(((uint64_t)face->uniqueID() << 1) | (uint64_t)axis).second)
    SkDebugf(
        "sigilcompose fx: a code-point substitution on this font is "
        "proportional %s — refused (the replacement is drawn at the "
        "original's pen position, so a different advance would move every "
        "letter after it; substitute within an equal-advance charset, or "
        "change the text and re-shape)\n",
        vertical ? "down a column" : "along a line");
  return 0;
}

/** Every animated scalar under a `Cache::Group` root, in tree order.
 *
 *  This is the whole invalidation mechanism, and therefore the whole risk.
 *  What it gathers is the set of numbers that can change what the bake looks
 *  like WITHOUT changing any description — which is exactly the set
 *  `computeVolatile` calls volatility and refuses to cache across. Two rules
 *  keep it honest:
 *
 *   - **Only LIVE slots are pushed.** A plain or settled value cannot move
 *     without a patch, and a patch calls markPaintDirtyUp() on the group.
 *     So the vector's LENGTH is part of the comparison: a motion connecting
 *     or disconnecting changes it, and the group re-bakes.
 *   - **The root's own transform and opacity are excluded.** They are
 *     applied by paint()'s matrix and saveLayer, outside the bake, and a
 *     fading group would otherwise drop its bake on every frame of the fade
 *     for a change the bake does not contain. (Its own transform moving is
 *     handled separately and more strictly — a device-pinned bake is refused
 *     outright while the node moves.) The root's CONTENT scalars are inside
 *     paintContent and are gathered like everyone else's.
 *
 *  Cost is one traversal of the subtree per frame, reading a handful of
 *  floats per node — set against the entire paint of that subtree, which is
 *  what it decides whether to skip. */
void collectGroupScalars(const Instance& inst, bool root,
                         std::vector<float>& out) {
  const ElementNode& node = *inst.desc;
  const auto push = [&](Instance::Slot slot, const Animatable<float>& v) {
    if (v.binding() ||
        (inst.anims[slot] && inst.anims[slot]->value.isConnected()))
      out.push_back(inst.resolveFloat(slot, v));
  };
  // Every slot the table can reach, in enum order (kSlotSpecs,
  // ComposeRuntime.h — the one enumeration of Instance::Slot). The root's
  // own transform and opacity are the exclusion argued above; its CONTENT
  // scalars are inside paintContent and are gathered like everyone else's.
  //
  // THE ORDER OF THIS VECTOR IS ARBITRARY BUT MUST BE STABLE. It is only
  // ever compared against the vector this same function produced on the
  // previous frame (Impl::paint, `groupScratch == inst.groupPrev`), so any
  // fixed permutation of the gathered values computes the identical
  // verdict; what would break the memo is an order that varies between
  // frames for the same tree.
  for (const SlotSpec& spec : kSlotSpecs) {
    if (root && spec.role != SlotRole::Content) continue;
    if (const Animatable<float>* v = slotValueOf(spec, node))
      push(spec.slot, *v);
  }
  // Mask gates: the same argument, over the per-mask vector. Only LIVE
  // values are pushed, so the vector's LENGTH still carries a motion
  // connecting or disconnecting.
  if (node.hasMasks()) {
    size_t slot = 0;
    for (const Mask& m : node.fxData->masks) {
      const auto pushGate = [&](const Animatable<float>& v) {
        const AnimatedFloat* a =
            slot < inst.maskAnims.size() ? inst.maskAnims[slot].get() : nullptr;
        if (v.binding() || (a && a->value.isConnected()))
          out.push_back(inst.resolveFloatAt(a, v));
        ++slot;
      };
      if (m.with.kind == Gate::Kind::Spans)
        for (const Spans::Term& t : m.with.where.terms) {
          pushGate(t.begin);
          pushGate(t.end);
          pushGate(t.offset);
        }
      else if (m.with.kind == Gate::Kind::Edge)
        pushGate(m.with.fraction);
    }
  }
  // fx() track progresses: the same argument again, over the per-track
  // vector. Only LIVE values are pushed, so the vector's LENGTH still
  // carries a track's motion connecting or disconnecting.
  if (node.textData)
    for (size_t i = 0; i < node.textData->tracks.size(); ++i) {
      const Animatable<float>& v = node.textData->tracks[i].progress;
      const AnimatedFloat* a =
          i < inst.trackAnims.size() ? inst.trackAnims[i].get() : nullptr;
      if (v.binding() || (a && a->value.isConnected()))
        out.push_back(inst.resolveFloatAt(a, v));
    }
  // The kFillLerp row (SlotRole::Bespoke): a synthesized progress with no
  // Animatable in the description, so it is read straight off the motion.
  if (inst.anims[Instance::kFillLerp] &&
      inst.anims[Instance::kFillLerp]->value.isConnected())
    out.push_back(inst.anims[Instance::kFillLerp]->value.value());
  for (const auto& child : inst.children)
    collectGroupScalars(*child, false, out);
}

}  // namespace

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
    std::vector<Span> rest =
        complementSpans(normalizeSpans(std::move(against)));
    // A pass may union rest() with explicit terms; keep both.
    rest.insert(rest.end(), out[i].begin(), out[i].end());
    out[i] = normalizeSpans(std::move(rest));
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

// ---------------------------------------------------------------------------
// The masking family, at paint
//
// A mask is (selection, gate). The gates fall into two mechanical classes
// and the split is the whole implementation:
//
//  - SPANS cuts the BOUNDARY. It rewrites the path the selected
//    outline-tracing outputs trace — the surface (fill + echoes) and the
//    marks. Content and children do not trace a boundary, so a spans gate
//    over them is not a picture and does nothing.
//  - EDGE / SHAPE / COVERAGE cut the PLANE. They wrap the selected outputs
//    in a canvas clip (edge, shape) or a kDstIn/kDstOut coverage layer
//    (alpha and luma, each with its complement).
//
// Both classes intersect for free, and across each other: span sets
// intersect as interval arithmetic, nested clips intersect by definition,
// stacked kDstIn layers multiply coverage. Where a mask selects EVERYTHING
// the plane gates are hoisted to wrap the whole node once rather than each
// group — cheaper, and the only way a nested pair of antialiased clips
// cannot compound its own edge.

namespace {

/** Does this span set claim the whole boundary? Then the boundary is
 *  untouched, and returning the source path unchanged is required, not an
 *  optimisation: a fully settled reveal must draw exactly the path it would
 *  have drawn with no mask on it at all, bit for bit, or adding a mask that
 *  is currently showing everything moves pixels. */
bool claimsEverything(const std::vector<Span>& show) {
  return show.size() == 1 && show[0].begin <= 1e-6f &&
         show[0].end >= 1.0f - 1e-6f;
}

/** Apply a resolved SHOW set to a boundary. `cut`, when asked for, says
 *  the geometry actually changed — which only the SURFACE needs, because
 *  only the surface has a cheap rrect to fall out of. Decorations always
 *  draw a path. */
SkPath gateOutline(const SkPath& src, const std::vector<Span>& show,
                   bool* cut = nullptr) {
  if (claimsEverything(show)) return src;
  if (cut) *cut = true;
  if (show.empty()) return SkPath();
  return detail::spanPath(src, show);
}

// ---- the coverage law, in one place ---------------------------------------
//
// A coverage gate (`by::alpha`, `by::luma` and their complements) draws the
// Material over the masked group's layer and keeps what it covers. The gate
// asks two INDEPENDENT questions and each has exactly one mechanism:
//
//  - WHICH CHANNEL (Gate::Channel) — Alpha is the shader's own alpha and
//    needs no work at all. Luma weights the PREMULTIPLIED colour with
//    Rec. 601 on the ENCODED values, with no linearization, because
//    everything here composites in encoded sRGB. Premultiplied is what
//    makes a TRANSPARENT matte read as black, the way compositing
//    applications do.
//  - WHICH SIDE (Gate::outside) — the complement is `kDstOut` instead of
//    `kDstIn`. `dst * (1 - a)` IS `1 - coverage`, exactly, for any source,
//    so an inverted matte costs one enum value and no shader.

/** Rec. 601 luma of a resolved COLOUR, as a coverage alpha. `Fill`'s colour
 *  is unpremultiplied, so the premultiplied reading is written out:
 *  `a · dot(rgb, k)`. */
SkColor4f lumaCoverageColor(const SkColor4f& c) {
  const float y = 0.299f * c.fR + 0.587f * c.fG + 0.114f * c.fB;
  return {0, 0, 0, std::clamp(c.fA * y, 0.0f, 1.0f)};
}

/** …and of a resolved SHADER. A shader's channels arrive PREMULTIPLIED, so
 *  the same law is one dot product: `dot(a·rgb, k) == a · dot(rgb, k)`. The
 *  result `(0,0,0,Y')` is a valid premultiplied colour because the
 *  coefficients sum to 1, so `Y' <= a` always. */
sk_sp<SkShader> lumaCoverageShader(sk_sp<SkShader> src) {
  static const SkRuntimeEffect* effect = [] {
    auto result = SkRuntimeEffect::MakeForShader(SkString(R"(
uniform shader src;
half4 main(float2 p) {
  half4 c = src.eval(p);
  half y = clamp(dot(c.rgb, half3(0.299, 0.587, 0.114)), 0, 1);
  return half4(0, 0, 0, y);
}
)"));
    return result.effect.release();
  }();
  if (!effect || !src) return src;
  SkRuntimeEffect::ChildPtr child(std::move(src));
  return effect->makeShader(nullptr, {&child, 1});
}

}  // namespace

// ---------------------------------------------------------------------------
// Volatility & caching

/** The movement scan for released instances: re-checked once per draw, so
 *  an EXTERNALLY-driven output that moves re-declares volatility (and
 *  stales every ancestor recording) in the same frame — nothing stale ever
 *  replays. Cheap by construction: released nodes are few and each check is
 *  a handful of float resolves. */
// The node→root matrix, recomputed outside paint. The op sequence per level
// — preTranslate(rect), preConcat(matrix()) — is EXACTLY the pair paint()
// applies to curToRoot as it recurses, on the same resolved floats, so the
// result is bit-identical to the paint-side accumulation. The settle
// compare depends on that: an ulp of drift reads as motion, and the node
// never releases.
SkMatrix Composer::Impl::worldMatrixOf(Instance& inst) {
  std::vector<Instance*> chain;
  for (Instance* i = &inst; i; i = i->parent) chain.push_back(i);
  SkMatrix m = SkMatrix::I();
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    Instance& node = **it;
    const SkRect rect = instanceRect(node);
    m.preTranslate(rect.left(), rect.top());
    m.preConcat(transformOf(node).matrix({0, 0}, node.desc->paint, rect.width(),
                                         rect.height()));
  }
  return m;
}

namespace {
std::array<float, 6> worldSix(const SkMatrix& w) {
  return {w.getScaleX(), w.getSkewX(),  w.getTranslateX(),
          w.getSkewY(),  w.getScaleY(), w.getTranslateY()};
}
}  // namespace

std::array<float, 6> Composer::Impl::worldScalarsOf(Instance& inst) {
  if (!inst.hasWorldSpaceMaterial)
    return {};  // all-zero, matching the paint-side guard
  return worldSix(worldMatrixOf(inst));
}

void Composer::Impl::scanReleasedScalars() {
  if (volatileDirty || releasedScalars.empty())
    return;  // a pending recompute rebuilds the list (pointers may be stale)
  for (Instance* inst : releasedScalars) {
    Instance::ContentScalars now;
    now.gates = inst->resolveGateValues();
    now.tracks = inst->resolveTrackValues();
    // The node→root matrix: a released world-space node whose externally
    // driven transform resumes must re-declare THE FRAME it resumes, before
    // any recording carrying the old anchoring replays.
    now.world = worldScalarsOf(*inst);
    // The bound fill: a released node whose output is assigned while the
    // volatility walk is idle must re-declare before its settled colour
    // replays from an ancestor's recording or its own promoted bake.
    now.fill = inst->resolveBoundFill();
    // The bound tile pan: same argument, so a parked scroll re-declares
    // before its parked phase replays.
    now.pattern = inst->resolvePatternOffset();
    // …and onPath()'s phase: a released marquee whose output is driven
    // again must re-declare before its parked frame replays.
    now.pathAt = inst->resolvePathAt();
    if (!(now == inst->settledScalars)) {
      inst->settleFrames = 0;  // the hold is over: warm up from scratch
      inst->settledScalars = std::move(now);
      inst->markPaintDirtyUp();
      volatileDirty = true;  // re-walk this frame, before anything paints
    }
  }
}

bool Composer::Impl::computeVolatile(Instance& inst, bool movingAbove) {
  const ElementNode& node = *inst.desc;

  auto boundOrRunning = [&](Instance::Slot slot, const Animatable<float>& v) {
    if (v.binding()) return true;
    return inst.anims[slot] && inst.anims[slot]->value.isConnected();
  };
  // Span passes: an animated reveal rebuilds the pass's geometry, and an
  // animated brush repaints it. Both are CONTENT volatility, and both are
  // deliberately kept out of the scalar and live-material memos, which
  // compare a bounded per-node list of values and have nowhere to put an
  // open-ended pass list's endpoints.
  const bool spanVolatile = [&] {
    if (!node.hasStrokePasses()) return false;
    size_t slot = 0;
    bool live = false;
    for (const StrokePass& pass : node.strokeData->passes) {
      live |= pass.what.isAnimated();
      for (const Spans::Term& term : pass.where.terms)
        for (const Animatable<float>* v :
             {&term.begin, &term.end, &term.offset}) {
          if (v->binding())
            live = true;
          else if (slot < inst.spanAnims.size() && inst.spanAnims[slot] &&
                   inst.spanAnims[slot]->value.isConnected())
            live = true;
          ++slot;
        }
    }
    return live;
  }();
  // Mask gates, split by what a memo can compare. A gate whose animation is
  // a BOUNDED LIST OF FLOATS (spans endpoints, an edge fraction) is
  // memo-visible and joins scalarContent below; a gate driven by a LIVE
  // MATERIAL is not a float and refuses both memos, exactly as a live
  // material fill does. A shape gate is a static Region and moves nothing.
  bool maskScalarLive = false, maskOpaque = false;
  if (node.hasMasks()) {
    size_t slot = 0;
    const auto live = [&](const Animatable<float>& v) {
      const AnimatedFloat* a =
          slot < inst.maskAnims.size() ? inst.maskAnims[slot].get() : nullptr;
      if (v.binding() || (a && a->value.isConnected())) maskScalarLive = true;
      ++slot;
    };
    for (const Mask& m : node.fxData->masks) {
      switch (m.with.kind) {
        case Gate::Kind::Spans:
          for (const Spans::Term& t : m.with.where.terms) {
            live(t.begin);
            live(t.end);
            live(t.offset);
          }
          break;
        case Gate::Kind::Edge:
          live(m.with.fraction);
          break;
        case Gate::Kind::Shape:
          break;
        case Gate::Kind::Coverage:
          if (m.with.coverage && m.with.coverage->isAnimated())
            maskOpaque = true;
          break;
      }
    }
  }
  // THE SLOT ROLES, in one walk of kSlotSpecs (ComposeRuntime.h). This split
  // is where the table's three roles come FROM; the other three consumers
  // read it or ignore it, and none of them wanted a fourth thing.
  //
  //  - Opacity applies OUTSIDE the node's content (in paint()'s layer
  //    stack), so a node animated only there still replays its content
  //    picture — a fading ornament re-records nothing. Ancestors still
  //    can't cache across it (their recording would freeze the motion),
  //    hence the return value.
  //  - Geometric is kept separately because a texture bake taken in device
  //    space is pinned to one device rect and may only be taken while the
  //    node is not moving. Opacity is deliberately not part of it — it does
  //    not move the rect.
  //  - Content rebuilds the recording, and joins `scalarContent` below,
  //    which is the memoizable half of content volatility: the half whose
  //    inputs are floats this frame can read back and compare.
  bool ownPaint = false;
  bool moving = false;
  bool scalarContent = false;
  for (const SlotSpec& spec : kSlotSpecs) {
    const Animatable<float>* v = slotValueOf(spec, node);
    if (!v) continue;  // this node does not carry the block that holds the slot
    switch (spec.role) {
      case SlotRole::Opacity:
        ownPaint |= boundOrRunning(spec.slot, *v);
        break;
      case SlotRole::Geometric:
        moving |= boundOrRunning(spec.slot, *v);
        break;
      case SlotRole::Content:
        scalarContent |= boundOrRunning(spec.slot, *v);
        break;
      case SlotRole::Bespoke:
        break;  // unreachable: slotValueOf answers nullptr for a Bespoke row
    }
  }
  inst.transformLive = moving;
  inst.placementUnderMotion = moving || movingAbove;
  ownPaint |= moving;

  // Content volatility: what actually invalidates the node's own recording
  // (bound/lerping fills, per-frame programs, animated decorations and image
  // frames).
  //
  // THE TERMS ARE NAMED ONCE, AND EVERY CONSUMER IS A SUBTRACTION FROM
  // THEM. Four questions are asked of this one list — "is anything
  // volatile" (ownContent), "can a group's float memo SEE it"
  // (opaqueToTheMemo), "is the live material the ONLY one" (liveMatOnly),
  // "are the animated scalars the only ones" (scalarMemo). Each of them
  // could be written as its own enumeration of the terms, and the copies
  // would drift: a carve-out that forgets, say, a bound fill lets a node
  // carrying one AND an animated gate take a memo, keep the recording that
  // baked the old colour, and replay it for as long as the gate holds
  // still. Deriving each consumer by subtraction is what makes
  // `ownContent == liveMat | otherThanLiveMat == scalarContent |
  // otherThanScalar` true BY CONSTRUCTION rather than by review.
  const bool fillLerp = inst.anims[Instance::kFillLerp] &&
                        inst.anims[Instance::kFillLerp]->value.isConnected();
  const bool boundFill = node.paint.fill && node.paint.fill->binding();
  const Material* nodeLiveMat = liveMaterialOf(node);
  // A fill material whose ONLY animation is its own bound tile pan is NOT
  // the live-material lane — it is two floats, resolvable outside paint by
  // a pointer dereference, so it rides the memoized scalar lane exactly as
  // a gate fraction, the world matrix's six and a bound Fill do. A material
  // with a bound pan AND anything else (a live uniform, an elapsed-time
  // input, a nested pan in a blend layer) stays on the opaque live path.
  // Conservative, and the split is a partition: patternPan and liveMat can
  // never both be true.
  const bool liveMatAnimated = nodeLiveMat && nodeLiveMat->isAnimated();
  const bool patternPan = liveMatAnimated && nodeLiveMat->hasBoundOffset() &&
                          !nodeLiveMat->animatedBeyondBoundOffset();
  // truly live (bound/uTime) — geometry-dependent materials resolve at
  // record time and stay cacheable
  const bool liveMat = liveMatAnimated && !patternPan;
  const Material* mfLive = metricFillOf(node);
  const bool metricLive = mfLive && mfLive->isAnimated();  // chrome type
  const bool cacheNone = node.cacheMode == Cache::None;
  const bool decorLive = [&] {
    bool live = false;
    for (const Decoration& d : node.backgrounds) live |= d.isAnimated();
    for (const Decoration& d : node.foregrounds) live |= d.isAnimated();
    if (node.fxData)
      for (const Decoration& d : node.fxData->overlays) live |= d.isAnimated();
    return live;
  }();
  const bool imageLive = node.kind == Kind::Image && imageAssetOf(node) &&
                         imageAssetOf(node)->animated();
  // A LIVE effect: the filter is captured by the recording, so bound
  // uniforms on it are content volatility, exactly as they are on a fill
  // material.
  const bool liveEffect =
      (layerEffectOf(node) && layerEffectOf(node)->isAnimated()) ||
      (backdropEffectOf(node) && backdropEffectOf(node)->isAnimated());
  // A LIVE pass material on an fx() track — uTime, a bound uniform, a
  // bound block — repaints the pass's output every frame with no float the
  // scalar lane could compare, so it is opaque volatility, exactly as a
  // live textFill material is. A pass whose only motion is its track's
  // PROGRESS is not this: progress already rides the memoized scalar lane
  // above, and the recording replays once it settles.
  const bool passLive = [&] {
    for (const Track& t : tracksOf(node))
      if (t.effect)
        if (const Material* pm = t.effect.passMaterial())
          if (pm->isAnimated()) return true;
    return false;
  }();
  // The MEMOIZABLE scalars, tracked apart from the rest of ownContent: each
  // rebuilds the painted geometry when it moves, and each is a number that
  // can sit still for a long time inside a running motion. Declared and
  // filled with every SlotRole::Content slot up in the table walk; the MASK
  // GATES join here, because their count is a property of the description
  // and no fixed slot can hold them.
  scalarContent |= maskScalarLive;  // a moving gate re-cuts or re-clips
  // fx() TRACKS: a moving master progress rebuilds glyph geometry, so it is
  // content volatility — the memoizable half, because a progress is a float
  // this frame can read back. Every track counts, so a settled entrance
  // sitting under a live loop still declares.
  if (node.textData)
    for (size_t i = 0; i < node.textData->tracks.size(); ++i) {
      const Animatable<float>& v = node.textData->tracks[i].progress;
      const AnimatedFloat* a =
          i < inst.trackAnims.size() ? inst.trackAnims[i].get() : nullptr;
      if (v.binding() || (a && a->value.isConnected())) scalarContent = true;
    }
  // A world-space material under a CONNECTED transform — this node's own or
  // any ancestor's, threaded down this recursion as movingAbove — has its
  // node→root matrix changing off the describe clock, and that matrix is
  // baked into the recording. That is content volatility, and it joins the
  // MEMOIZED lane rather than the opaque one, because the matrix is six
  // floats ContentScalars carries: the recording survives between ticks,
  // the flag releases once the motion provably settles, and the per-draw
  // scan re-declares the frame it resumes. Note that for THIS node an
  // ancestor's transform is a content input, not a geometric one — it does
  // not move this node's own device rect.
  const bool worldUnderMotion =
      inst.hasWorldSpaceMaterial && (moving || movingAbove);
  scalarContent |= worldUnderMotion;
  // A BOUND fill joins the memoized lane too, even though a Fill is not a
  // float: its equality is structurally exact (kind, colour bitwise, shader
  // pointer) and resolving it is one pointer dereference, so "the value the
  // recording was baked with" is well defined. ContentScalars carries it,
  // so the recording survives between changes, the flag releases after
  // kScalarSettleFrames of identity — which is what lets a never-moving
  // bound accent be promoted like a plain colour — and the per-draw
  // released scan re-declares THE FRAME the output moves, before anything
  // stale replays.
  scalarContent |= boundFill;
  // …and the bound tile PAN, the third member of the lane. ContentScalars
  // carries the resolved pair, so each moved frame re-records with the new
  // phase and nothing re-describes, a parked scroll releases and promotes
  // like a static pattern, and the released scan re-declares THE FRAME the
  // pan resumes.
  scalarContent |= patternPan;
  /** The pre-release reading. The release below may set `scalarContent`
   *  false for a node that is provably holding still; the LIVE-MATERIAL
   *  memo asks a question about what the node DECLARES, not about whether
   *  it is currently moving, so it subtracts this and not the released
   *  value. */
  const bool scalarDeclared = scalarContent;
  // The stability RELEASE, read side. The settle counter accumulates at
  // PAINT time, because this walk re-runs only on reconcile or while the
  // ticker is active and so cannot count frames by itself. Here the walk
  // merely honours a warmed-up release and registers the instance for the
  // per-draw scan (scanReleasedScalars) that re-declares volatility THE
  // FRAME an externally-driven binding moves again. The node's own
  // recording was already kept by the scalar memo; what this frees is the
  // FLAG, so ancestors can cache across a settled reveal as well.
  if (scalarContent && inst.settleFrames >= Instance::kScalarSettleFrames) {
    Instance::ContentScalars now;
    now.gates = inst.resolveGateValues();
    now.tracks = inst.resolveTrackValues();
    now.world = worldScalarsOf(inst);    // a held world matrix releases too
    now.fill = inst.resolveBoundFill();  // …and a held bound fill
    now.pattern = inst.resolvePatternOffset();  // …and a held bound pan
    now.pathAt = inst.resolvePathAt();          // …and a held marquee phase
    if (now == inst.settledScalars) {
      scalarContent = false;  // released — provably holding still
      releasedScalars.push_back(&inst);
    } else {
      inst.settleFrames = 0;  // moved between walks: warm up again
      inst.settledScalars = std::move(now);
    }
  }
  // What a SUBTREE VALUE MEMO can and cannot see. A group bake is held by
  // comparing floats, so every source of volatility inside it must either
  // BE a float this frame can read back (the transform slots, opacity, the
  // mask gates, glyph progress, the fill lerp) or arrive as a description
  // change, which stales the group root through markPaintDirtyUp().
  // Everything named in `sharedOpaque` is neither: it moves pixels off the
  // clock with no number to compare, and a group holding a bake across one
  // of them would blit an old frame's picture indefinitely. Refused
  // outright rather than approximated — this is where the whole feature's
  // risk sits, and it is the one place to be conservative.
  //
  // The list is split in two: `sharedOpaque` is every term opaque to EVERY
  // memo, named once, while `boundFill` and `liveMat` are handled per
  // consumer below (the fill rides the memoized scalar lane, the live
  // material has its own memo). No consumer re-enumerates.
  const bool sharedOpaque = metricLive || cacheNone || decorLive || imageLive ||
                            spanVolatile || maskOpaque || liveEffect ||
                            passLive;
  // A bound fill still refuses Cache::Group, even though it rides the
  // node-level scalar lane. The group memo's currency is one flat float
  // vector gathered across the subtree (collectGroupScalars), and a Fill's
  // shader-kind value compares by POINTER identity, which is only sound
  // while an owning sk_sp keeps the allocation alive — a vector of floats
  // cannot hold that reference. Flattening the pointer into floats would
  // make a freed shader reallocated at the same address compare stable,
  // which is exactly the silent stale bake this refusal exists to prevent.
  //
  // A bound tile pan refuses the group as well, for a different reason: a
  // pan IS two floats, but collectGroupScalars does not gather it, so a
  // group bake held across a moving one would blit the parked phase. The
  // node-level release above still frees a settled pan's flag; only the
  // group BAKE stays refused.
  const bool opaqueToTheMemo =
      sharedOpaque || boundFill || liveMat || patternPan;
  // Everything volatile about this node EXCEPT its animated scalars, and
  // everything EXCEPT its live material. The two memo carve-outs below are
  // exactly these two subtractions, which is why neither can forget a term.
  // (`boundFill` is inside `scalarDeclared`, so it reaches
  // `otherThanLiveMat` through the scalar term, exactly as a gate does.)
  const bool otherThanScalar = sharedOpaque || liveMat || fillLerp;
  const bool otherThanLiveMat = sharedOpaque || fillLerp || scalarDeclared;
  const bool ownContent = otherThanScalar || scalarContent;

  bool childrenVolatile = false;
  bool childReadsBackdrop = false;
  bool childrenGroupSafe = true;
  for (auto& child : inst.children) {
    // A connected transform HERE moves every descendant's world matrix.
    childrenVolatile |= computeVolatile(*child, movingAbove || moving);
    childReadsBackdrop |= child->subtreeReadsBackdrop;
    childrenGroupSafe &= child->groupSafe;
  }
  // Does anything here composite against what is ALREADY on the canvas? If
  // so the subtree can never be baked into a transparent layer and blitted
  // back — a kMultiply child would resolve against transparent black. This
  // is the trap automatic promotion has to avoid, and it is invisible in a
  // still frame of the common case, so it is computed rather than assumed.
  // Split into halves, because the two cache strategies ask different
  // questions of it. Whole-subtree promotion bakes the children too, so it
  // must ask about the whole subtree. A split bake replaces only the node's
  // OWN layer and draws children over the blit, so it must ask only about
  // the node's own paint — the children composite against the blit exactly
  // as they would against freshly rasterized pixels.
  inst.ownReadsBackdrop = backdropEffectOf(node) != nullptr ||
                          node.paint.blendMode != SkBlendMode::kSrcOver;
  inst.subtreeReadsBackdrop = inst.ownReadsBackdrop || childReadsBackdrop;

  // The two halves of the group question. `groupSafe` is what a PARENT asks
  // of this subtree — and it includes this node's own backdrop read,
  // because inside a group bake a kMultiply child resolves against
  // transparent black exactly as it would under whole-subtree promotion.
  // `groupRootOK` is what this node asks of ITSELF, and deliberately does
  // not include its own blend and opacity: those are applied by paint()'s
  // saveLayer, outside the bake, exactly as they would be applied outside
  // the live paint. A backdrop FILTER on the root is still fatal — it
  // samples the destination, which the bake is not.
  //
  // A MOVING world-space field also refuses the group memo. The node→root
  // matrix is not among the floats collectGroupScalars gathers (only
  // transforms INSIDE the group are), so a bake held across an ancestor's
  // motion would blit stale anchoring. A fully static chain — no connected
  // transform anywhere above — keeps its group: description changes reach
  // it through markPaintDirtyUp and layout moves through syncLayoutRects.
  inst.groupSafe = !opaqueToTheMemo && !inst.ownReadsBackdrop &&
                   !worldUnderMotion && childrenGroupSafe;
  inst.groupRootOK = node.cacheMode == Cache::Group && !opaqueToTheMemo &&
                     !worldUnderMotion && childrenGroupSafe &&
                     backdropEffectOf(node) == nullptr;
  if (node.cacheMode == Cache::Group && !inst.groupRootOK &&
      !inst.groupWarned) {
    inst.groupWarned = true;
    // Loud, because the alternative is an author seeing a node they
    // explicitly asked to bake reported as live paint, with no way to learn
    // that one descendant several levels down declined it for them.
    SkDebugf(
        "sigilcompose Cache::Group: \"%s\" cannot bake — %s. A group is "
        "held by comparing FLOATS, so live materials (uTime or a bound "
        "uniform), animated decorations, animated images, bound fill(), "
        "variable-font drives, Cache::None leaves and non-srcOver "
        "blends below the root all refuse it.\n",
        node.key.empty() ? "(anon)" : node.key.c_str(),
        opaqueToTheMemo      ? "the group node itself carries volatility "
                               "the memo cannot see"
        : !childrenGroupSafe ? "something in its subtree carries "
                               "volatility the memo cannot see"
                             : "it carries a backdrop filter");
  }

  // subtreeVolatile gates the node's own caches: blocked by content volatility
  // here or ANY volatility below (children paint inside the recording,
  // transforms included) — but not by own paint volatility.
  const bool blocked = ownContent || childrenVolatile;
  // …and WHICH of the two it was. `subtreeVolatile && !ownContentVolatile`
  // is the split bake's whole population: the node's own paint is provably
  // static and only its children move.
  inst.ownContentVolatile = ownContent;
  if (ownContent)
    inst.ownImage.reset();  // a volatile own paint can never hold a bake
  // The resolve-memo carve-out: volatility caused SOLELY by a live
  // material keeps its picture — paint() replays it while resolve() stays
  // stable and re-records only when the shader actually changes. Stated as
  // the subtraction it is, so it cannot fall behind the list again.
  inst.liveMatOnly = liveMat && !otherThanLiveMat && !childrenVolatile;
  // The same carve-out for animated SCALARS. A node whose content
  // volatility is entirely memoizable numbers keeps its recording and
  // re-records only when one of them actually ticks, so a keyframe's hold
  // segment repaints nothing. Deliberately disjoint from liveMatOnly: a
  // node with BOTH a live material and an animated gate takes neither memo,
  // which is the conservative answer and costs no more than having no memo
  // at all.
  inst.scalarMemo = scalarContent && !otherThanScalar && !childrenVolatile;
  const bool memoized = inst.liveMatOnly || inst.scalarMemo;
  if (blocked != inst.subtreeVolatile) {
    inst.subtreeVolatile = blocked;
    if (!memoized)
      inst.paintDirty = true;  // cacheability changed → re-record/drop
  }
  if (inst.subtreeVolatile && !memoized) {
    inst.picture.reset();
    // A group root's bake is dropped by its OWN value memo, in paint(),
    // one frame at a time. Dropping it here instead would drop it every
    // frame — the subtree IS volatile, permanently, and that verdict is
    // precisely the one the group exists to look past. `picture` is still
    // reset: a group root never replays one, and leaving a stale recording
    // reachable is how the fall-through path would blit last frame's pixels
    // on the frame the memo just said not to.
    if (!inst.groupRootOK) inst.textureImage.reset();
  }
  return ownPaint || blocked;
}

// ---------------------------------------------------------------------------
// Silhouette

const SkPath& Composer::Impl::resolveOutline(Instance& inst,
                                             SkSize size) const {
  if (inst.outlineCacheDesc != inst.desc.get() ||
      inst.outlineCacheSize != size) {
    inst.outlineCache = inst.desc->shapeFn(size);
    inst.outlineCacheDesc = inst.desc.get();
    inst.outlineCacheSize = size;
  }
  return inst.outlineCache;
}

// ---------------------------------------------------------------------------
// Kinetic typography: master progress → stagger remap → per-glyph mods →
// batched RSXform draws (one per font/color bucket — never per glyph).

namespace {

/** THE FIELDS OF A TextPath THAT DECIDE THE LAYOUT, as opposed to the
 *  placement of glyphs the layout already made. How the baseline is shaped,
 *  which way the run reads and where along it the run RESTS decide which
 *  words land on which contour; the perpendicular offset, the orientation
 *  and the tangent snapping are read per glyph at paint and move nothing
 *  the breaker decided.
 *
 *  A BOUND phase rests at zero and is applied at paint, which is what makes
 *  a marquee a repaint rather than a reflow. */
bool samePathLayout(const TextPath& a, const TextPath& b) {
  const float restA = a.at.plain() ? *a.at.plain() : 0.0f;
  const float restB = b.at.plain() ? *b.at.plain() : 0.0f;
  return a.path == b.path && a.align == b.align && a.autoFlip == b.autoFlip &&
         restA == restB;
}

/** Directions a path tangent snaps to at LAYOUT time — the tangents baked
 *  into the path layout's rest poses, which nothing on the paint path
 *  reads (the painter re-derives every pose exactly and snaps with the
 *  size-cut ladder below). `TextPath::exactTangent` is the opt-out. */
constexpr int kPathTangentSteps = 64;

/** How many directions the rotation ladder offers a glyph rendered at
 *  `pixelSize`.
 *
 *  A turning glyph's rotation is snapped so it lands on a BOUNDED set of
 *  directions: every distinct rotation is both a batch bucket and a
 *  glyph-atlas strike, and lifting the ladder entirely mints a fresh strike
 *  per letter per frame for several times the price of any ladder measured
 *  here. How FINE the ladder must be is a visual question, and it has an
 *  exact answer.
 *
 *  One step turns the glyph by 2π/N, which sweeps a point `r` from the
 *  rotation centre through r·2π/N pixels. Take `r` as the glyph's own
 *  half-em — the far edge of its ink — and cut the ladder at sixteen steps
 *  per pixel of em, and that sweep is (px/2)·2π/(16·px) = π/16 ≈ 0.20 px AT
 *  EVERY SIZE. The number to stay under is a QUARTER of a pixel, because
 *  that is the phase grid a moving run's origins sit on: a ladder whose
 *  step sweeps further than the grid is the coarsest thing left in the
 *  motion and ticks letter by letter as each glyph crosses a step at its
 *  own moment, while one that sweeps less disappears underneath it.
 *
 *  Both ends are clamped. The floor keeps small rings on the ladder they
 *  have always had. The ceiling is what bounds the strike population at
 *  all — the ladder's whole reason to exist — and it binds from 128 px of
 *  em upward, where a step's sweep begins to pass the grid again;
 *  `TextPath::exactTangent` is the escape for artwork set that large. */
int tangentLadderSteps(float pixelSize) {
  constexpr float kStepsPerPixel = 16.0f;
  constexpr int kMinSteps = 64, kMaxSteps = 2048;
  return std::clamp((int)std::lround(pixelSize * kStepsPerPixel), kMinSteps,
                    kMaxSteps);
}

}  // namespace

/** THE RUN BROKEN ACROSS ITS BASELINE'S CONTOURS.
 *
 *  Shaped once — real kerning, real ligatures, real advances — and then
 *  laid out through SigilWeave's own contour-interval geometry: EVERY
 *  contour becomes one interval of one line, so which words land on which
 *  contour, how they fill it, and where each pen sits are the paragraph
 *  engine's answers rather than a second implementation of them. A word
 *  that does not fit the contour it reached starts the next one, and a run
 *  that outlasts the last contour simply stops.
 *
 *  Cached against everything that decides it — the content, the box the
 *  baseline resolves against, and the baseline value. The `at` phase is not
 *  among them: it re-places glyphs the layout already placed, at paint. */
void Composer::Impl::ensurePathLayout(Instance& inst, const TextPath& spec,
                                      SkSize size) {
  if (inst.pathValid && inst.pathRev == inst.contentRev &&
      inst.pathSize.width() == size.width() &&
      inst.pathSize.height() == size.height() && inst.pathSpec &&
      samePathLayout(*inst.pathSpec, spec))
    return;

  if (!inst.paragraph) return;  // no content materialized: nothing to place
  inst.pathValid = false;
  inst.pathIntervals.clear();
  inst.pathLayout = {};
  inst.pathRev = inst.contentRev;
  inst.pathSize = size;
  inst.pathSpec = spec;
  inst.pathTotalLength = 0;
  if (!spec.path) return;

  const SkPath baseline = spec.path(size);
  // The centre Orient::Radial radiates from: the bounds of the resolved
  // baseline, which for every dial-shaped path is its centre.
  const SkRect baselineBounds = baseline.getBounds();
  inst.pathCentroid = {baselineBounds.centerX(), baselineBounds.centerY()};

  // The run's own width, from the shaped advances. This is what Align
  // measures against, and it is why the run has to be shaped first — it
  // comes from the straight MEASURE layout, which is also the node's box.
  float runWidth = 0;
  sigil::weave::forEachPlacedGlyph(
      inst.textLayout, *inst.paragraph,
      [&](const sigil::weave::PlacedGlyph& placed) {
        runWidth = std::max(runWidth, placed.rest.x() + placed.advance);
      });

  static thread_local std::vector<sk_sp<SkContourMeasure>> contours;
  const auto measureContours = [](const SkPath& path) {
    contours.clear();
    float total = 0;
    for (SkContourMeasureIter iter(path, false);;) {
      sk_sp<SkContourMeasure> contour = iter.next();
      if (!contour) break;
      if (contour->length() > 0) {
        total += contour->length();
        contours.push_back(std::move(contour));
      }
    }
    return total;
  };
  const float length = measureContours(baseline);
  if (contours.empty()) return;
  inst.pathTotalLength = length;

  // One arc-length coordinate over the whole chain, for the two questions
  // that are about the BASELINE rather than about one glyph: is it closed,
  // and which way does the run read along it.
  const auto posTan = [](float distance, SkPoint* position, SkVector* tangent) {
    for (const auto& contour : contours) {
      if (distance <= contour->length())
        return contour->getPosTan(distance, position, tangent);
      distance -= contour->length();
    }
    const auto& last = contours.back();
    return last->getPosTan(last->length(), position, tangent);
  };

  // "Closed" here means geometrically closed, not flagged closed:
  // shapes::arc() defaults to a 359.9-degree sweep and is the library's own
  // spelling for a ring, but addArc leaves it open. Dropping half a centred
  // caption off a ring because of a tenth of a degree is not a behaviour
  // anyone wants.
  bool closed = contours.size() == 1 && contours.front()->isClosed();
  if (!closed) {
    SkPoint head, tail;
    SkVector ignored;
    if (posTan(0, &head, &ignored) && posTan(length, &tail, &ignored))
      closed = SkPoint::Distance(head, tail) <= std::max(1.0f, length * 0.002f);
  }

  const float restAt = spec.at.plain() ? *spec.at.plain() : 0.0f;
  inst.pathRestAt = restAt;
  float start = restAt * length;
  if (spec.align == TextPath::Align::Center)
    start -= runWidth * 0.5f;
  else if (spec.align == TextPath::Align::End)
    start -= runWidth;

  // autoFlip is a decision about the RUN, not about each glyph. Turning
  // glyphs over one at a time reverses the reading order — a caption on the
  // lower half of a clockwise ring would come out mirrored — so the run
  // decides once and then reads along the reversed baseline.
  //
  // The decision is a MAJORITY over the run, not a reading at its midpoint.
  // A midpoint sample is exactly ambiguous where the tangent is vertical,
  // which is precisely where a ring caption centred at the top or bottom of
  // a circle puts it: `tan.x < 0` is false at x == 0, so the most natural
  // spelling of all — a circle, at = 0, centred, autoFlip — would silently
  // do nothing. Sampling across the run has no such point.
  //
  // A run that wraps PAST the crossover cannot be fixed by one flip, and
  // this model does not pretend otherwise: the majority reads right way up
  // and the tail does not. Setting the top and bottom halves as two
  // separate runs is the way around it.
  bool flipRun = false;
  if (spec.autoFlip) {
    constexpr int kVotes = 9;
    int upsideDown = 0, upright = 0;
    for (int vote = 0; vote < kVotes; ++vote) {
      float at = start + runWidth * ((float)vote + 0.5f) / (float)kVotes;
      if (closed) at = std::fmod(std::fmod(at, length) + length, length);
      SkPoint position;
      SkVector tangent;
      if (!posTan(std::clamp(at, 0.0f, length), &position, &tangent)) continue;
      if (tangent.x() < 0)
        ++upsideDown;
      else if (tangent.x() > 0)
        ++upright;
    }
    flipRun = upsideDown > upright;
  }

  // A flipped run enters at the END of its stretch of baseline and walks
  // BACKWARDS along it: its pen still travels the letters in reading order,
  // but its arc position decreases and every tangent faces about. One
  // decision for the whole run, expressed as the interval's own direction
  // of travel — turning the letters over one at a time is what would
  // reverse the reading order.
  //
  // ONE LINE, one interval per contour from the ENTRY POINT onwards. `at`
  // is a fraction of the WHOLE baseline, contours chained end to end, which
  // is what lets seven chords of a heptagon carry seven captions addressed
  // by fraction alone. So the entry point picks the contour it falls in,
  // enters it partway, and every contour after that is offered whole.
  //
  // What a contour boundary IS, on the other hand, is a break: a word that
  // does not fit the contour it reached starts the next one rather than
  // bending across the gap between two disconnected curves.
  const float entry = flipRun ? start + runWidth : start;
  size_t entryContour = 0;
  float entryLocal = entry;
  while (entryContour + 1 < contours.size() &&
         entryLocal > contours[entryContour]->length()) {
    entryLocal -= contours[entryContour]->length();
    ++entryContour;
  }
  inst.pathIntervals.reserve(contours.size());
  const auto pushInterval = [&](size_t index, float localStart) {
    sigil::weave::LineInterval interval;
    interval.contour = contours[index];
    interval.contourStart = localStart;
    interval.advanceScale = flipRun ? -1.0f : 1.0f;
    interval.wrapContour = closed;
    // How much baseline this contour still offers from where the pen
    // entered it — the whole of it when the pen wraps, because a loop has
    // no end to run out of.
    const float contourLength = contours[index]->length();
    interval.length =
        closed
            ? contourLength
            : std::max(flipRun ? localStart : contourLength - localStart, 0.0f);
    inst.pathIntervals.push_back(std::move(interval));
  };
  if (flipRun) {
    pushInterval(entryContour, entryLocal);
    for (size_t index = entryContour; index-- > 0;)
      pushInterval(index, contours[index]->length());
  } else {
    pushInterval(entryContour, entryLocal);
    for (size_t index = entryContour + 1; index < contours.size(); ++index)
      pushInterval(index, 0.0f);
  }

  sigil::weave::LineSetFlow flow({inst.pathIntervals});
  sigil::weave::ParagraphLayoutOptions options = textLayoutOptions(inst);
  // The baseline places the run; an interval-relative alignment on top of
  // that would fight `at` and Align for the same authority.
  options.alignment = sigil::weave::TextAlignment::kStart;
  options.pathText.tangentRotationSteps =
      spec.exactTangent ? 0 : kPathTangentSteps;
  inst.pathLayout =
      sigil::weave::layoutParagraph(fonts, *inst.paragraph, flow, options);
  inst.pathValid = true;
}

std::optional<sigil::weave::PaintStyle> Composer::Impl::metricTextStyle(
    Instance& inst, const PaintContext& paintCtx) {
  const ElementNode& node = *inst.desc;
  const Material* metricMat = metricFillOf(node);
  const bool stroked = node.textData && node.textData->hasTextStroke;
  if (!metricMat && !stroked) return std::nullopt;

  // Chrome type: the material's unit square mapped to the text's metric
  // band — x across the widest line, y from the first line's cap top (real
  // cap height when the face reports one) to the last line's baseline.
  //
  // The override replaces the whole PaintStyle for every run, so it starts
  // as a COPY of the paragraph's own style and swaps only the foreground —
  // textFill supersedes the fill, not the underlays, overlays and
  // decorations around it (a chrome wordmark keeps its cast shadow and dark
  // keyline).
  sigil::weave::PaintStyle metric =
      inst.paragraph->spans().empty()
          ? sigil::weave::PaintStyle{}
          : inst.paragraph->spans().front().style.paint;
  metric.foreground.setShader(nullptr);
  bool havePaint = false;
  // textStroke(): a stroke pass on the glyphs, UNDER the fill. It joins the
  // style's own underlays rather than replacing them, so an engraved face
  // keeps its cast shadow.
  if (stroked) {
    sigil::weave::PaintLayer outline;
    outline.paint.setAntiAlias(true);
    outline.paint.setStyle(SkPaint::kStroke_Style);
    outline.paint.setStrokeWidth(node.textData->textStrokeWidth);
    outline.paint.setStrokeJoin(SkPaint::kRound_Join);
    const Fill& sf = node.textData->textStrokeFill;
    if (sf.kind == Fill::Kind::Shader && sf.shaderValue)
      outline.paint.setShader(sf.shaderValue);
    else
      outline.paint.setColor4f(
          sf.kind == Fill::Kind::Color ? sf.colorValue : SkColor4f{0, 0, 0, 1},
          nullptr);
    metric.addUnderlay(outline);
    havePaint = true;
  }
  if (!metricMat) return havePaint ? std::optional(metric) : std::nullopt;

  // Geometry-dependent materials resolve against a UNIT box here, not the
  // node's. The local matrix below already maps the shader's [0,1]² onto
  // the metric band, so uResolution baked from the node's layout size would
  // divide a second time: a `linearUnit` ramp came out at t ≈ 0.003 and
  // every glyph painted the first stop, flat and silently. Material.h
  // advertises textFill and the Unit ramps as the same trick, and this is
  // what makes that true.
  PaintContext metricCtx = paintCtx;
  metricCtx.size = {1.0f, 1.0f};
  const Fill f = (metricMat->isAnimated() || metricMat->geometryDependent())
                     ? metricMat->resolve(metricCtx)
                     : metricMat->toFill();
  if (f.kind == Fill::Kind::Shader && f.shaderValue && !inst.columns.empty()) {
    // A VERTICAL passage has no cap band to hang the ramp on: a column's
    // glyphs centre across its axis rather than standing on a baseline. The
    // unit square maps onto the COLUMN BLOCK instead — x across the columns,
    // y down them — so a ramp authored in [0,1]² still crosses the type,
    // reading down the page rather than across it.
    SkRect block = SkRect::MakeEmpty();
    for (const sigil::weave::ColumnMetrics& column : inst.columns)
      block.join(column.rect());
    SkMatrix map = SkMatrix::Translate(block.left(), block.top());
    map.preScale(std::max(block.width(), 1.0f), std::max(block.height(), 1.0f));
    metric.foreground.setShader(f.shaderValue->makeWithLocalMatrix(map));
    havePaint = true;
  } else if (f.kind == Fill::Kind::Shader && f.shaderValue &&
             !inst.lines.empty()) {
    const sigil::weave::ShapedWord* firstFont = nullptr;
    sigil::weave::forEachPlacedGlyph(
        inst.textLayout, *inst.paragraph,
        [&](const sigil::weave::PlacedGlyph& placed) {
          if (!firstFont) firstFont = placed.shaped;
        });
    float capH = 0;
    if (firstFont && firstFont->typeface) {
      SkFontMetrics fm;
      sigil::weave::makeFont(firstFont->typeface, firstFont->fontSize)
          .getMetrics(&fm);
      capH = fm.fCapHeight;
    }
    const sigil::weave::LineMetrics& first = inst.lines.front();
    if (capH <= 0) capH = first.ascent;  // face reports none — the ascent band
    float left = first.left, right = first.right;
    for (const sigil::weave::LineMetrics& line : inst.lines) {
      left = std::min(left, line.left);
      right = std::max(right, line.right);
    }
    const float top = first.baseline - capH;
    const float bottom = inst.lines.back().baseline;
    SkMatrix map = SkMatrix::Translate(left, top);
    map.preScale(std::max(right - left, 1.0f), std::max(bottom - top, 1.0f));
    metric.foreground.setShader(f.shaderValue->makeWithLocalMatrix(map));
    havePaint = true;
  } else if (f.kind == Fill::Kind::Color) {
    metric.foreground.setColor4f(f.colorValue, nullptr);
    havePaint = true;
  }
  return havePaint ? std::optional(metric) : std::nullopt;
}

// ---------------------------------------------------------------------------
// THE REST POSE of one glyph: where the baseline put its advance centre, and
// which way it faces there. Plain text rests level on its own straight
// baseline; a path run rests on the curve; a vertical column's glyph rests on
// the column axis. Returning false DROPS the glyph — a run walking off the end
// of an open baseline should look like it, rather than piling every remaining
// letter on the last point.
//
// ONE BODY for the painter and for the beatsOf query, because a mark placed
// beside a cascade must land where the letters land: on the curve where they
// ride a curve, down the column where they stand in a column.

namespace {

/** Everything the pose depends on beyond the glyph itself. */
struct PoseContext {
  const Instance* inst = nullptr;
  const sigil::weave::ParagraphLayout* layout = nullptr;
  const TextPath* onPath = nullptr;
  bool ridesPath = false;
  /** Where along the baseline the run sits this frame, as arc length — the
   *  delta on top of the `at` the path layout baked in. */
  float phaseArc = 0;
};

struct RestPose {
  SkPoint centre{0, 0};
  float cosine = 1, sine = 0;
  /// Glyph-local vector from the draw origin to `centre`. The horizontal
  /// convention, (halfAdvance, 0), is null here.
  std::optional<SkVector> centreOffset;
};

bool restPoseOf(const PoseContext& ctx, const sigil::weave::PlacedGlyph& placed,
                RestPose& pose) {
  const sigil::weave::ParagraphLayout& layout = *ctx.layout;
  if (!ctx.ridesPath) {
    pose.cosine = 1.0f;
    pose.sine = 0.0f;
    pose.centreOffset.reset();
    // A ROTATED run — Latin lying on its side in a CJK column — is placed
    // per glyph off its interval exactly as a path run is, and deviates
    // in the frame the interval turned it to.
    if (placed.transformed) {
      const sigil::weave::LineInterval* interval =
          placed.intervalIndex >= 0 &&
                  (size_t)placed.intervalIndex < layout.intervals.size()
              ? &layout.intervals[(size_t)placed.intervalIndex]
              : nullptr;
      if (!interval) return false;
      SkVector tangent;
      if (!interval->placeAt(placed.pen, 0.0f, layout.tangentRotationSteps,
                             &pose.centre, &tangent))
        return false;
      const float magnitude = std::hypot(tangent.x(), tangent.y());
      if (magnitude <= 1e-6f) return false;
      pose.cosine = tangent.x() / magnitude;
      pose.sine = tangent.y() / magnitude;
      return true;
    }
    // An UPRIGHT run stands level in a column that runs down the page: its
    // advance is vertical while the glyph is still drawn from a horizontal
    // origin, so the pose centre is the point on the COLUMN AXIS the pen
    // reached, and the back-out to the draw origin is whatever vector
    // separates the two.
    if (placed.shaped && placed.shaped->vertical) {
      const sigil::weave::LineInterval* interval =
          placed.intervalIndex >= 0 &&
                  (size_t)placed.intervalIndex < layout.intervals.size()
              ? &layout.intervals[(size_t)placed.intervalIndex]
              : nullptr;
      if (interval) {
        SkVector tangent;
        if (interval->placeAt(placed.pen, 0.0f, 0, &pose.centre, &tangent)) {
          pose.centreOffset = SkVector{pose.centre.x() - placed.rest.x(),
                                       pose.centre.y() - placed.rest.y()};
          return true;
        }
      }
    }
    // Horizontal flow, and 縦中横 — a horizontally shaped run set upright
    // across the column, whose advance runs across the page like any
    // other horizontal run's.
    pose.centre = {placed.rest.x() + placed.advance * 0.5f, placed.rest.y()};
    return true;
  }
  if (placed.intervalIndex < 0 ||
      (size_t)placed.intervalIndex >= ctx.inst->pathIntervals.size())
    return false;
  const sigil::weave::LineInterval& interval =
      ctx.inst->pathIntervals[(size_t)placed.intervalIndex];
  SkPoint position;
  SkVector tangent;
  // EXACT, not snapped: the snapping is a rasterization concession and
  // belongs to the rotation alone. `offset` rides the type off the
  // baseline along the perpendicular, so a tangent rounded onto a ladder
  // step would slide it along the curve by however far the rounding was.
  if (!interval.placeAt(placed.pen, ctx.phaseArc, 0, &position, &tangent))
    return false;
  const float magnitude = std::hypot(tangent.x(), tangent.y());
  if (magnitude <= 1e-6f) return false;
  float dirX = tangent.x() / magnitude, dirY = tangent.y() / magnitude;
  // Perpendicular offset, positive to the LEFT of travel (outward on a
  // clockwise circle). The path replaces the glyph's own baseline.
  // Measured along TRAVEL even under Radial orientation, so `offset`
  // keeps meaning "how far off the baseline the type rides" regardless of
  // which way the glyph ends up facing.
  position.offset(dirY * ctx.onPath->offset, -dirX * ctx.onPath->offset);
  // Radial: the glyph's BASELINE runs along the radius, so the run reads
  // outward from the centre like a spoke. That is how an astrolabe limb,
  // a compass rose and a radial axis label their divisions — you turn the
  // instrument to read them.
  //
  // Note this is genuinely a different thing from what Tangent already
  // does. On a circle, "up points outward" IS the tangent orientation (a
  // clock face's 6 is upside down for exactly that reason), so the only
  // orientation a path baseline was missing is the one where the type
  // radiates.
  if (ctx.onPath->orient == TextPath::Orient::Upright) {
    dirX = 1.0f;
    dirY = 0.0f;
  } else if (ctx.onPath->orient == TextPath::Orient::Radial) {
    const float ox = position.x() - ctx.inst->pathCentroid.x();
    const float oy = position.y() - ctx.inst->pathCentroid.y();
    const float radius = std::hypot(ox, oy);
    if (radius <= 1e-6f) return false;
    dirX = ox / radius;
    dirY = oy / radius;
  }
  // The ROTATION snaps, and only the rotation: a continuous per-glyph
  // angle mints a fresh glyph mask per letter in Skia's cache. The ladder
  // is cut by RENDERED SIZE (tangentLadderSteps): a fixed angular step
  // sweeps a bigger glyph's extremity through more pixels, so display
  // lettering on a turning ring gets a proportionally finer ladder and
  // does not tick letter by letter as each glyph crosses a step at its
  // own moment.
  pose.centre = position;
  pose.cosine = dirX;
  pose.sine = dirY;
  if (!ctx.onPath->exactTangent)
    sigil::weave::quantizeAngle(
        std::atan2(dirY, dirX),
        tangentLadderSteps(placed.shaped ? placed.shaped->fontSize : 0.0f),
        pose.cosine, pose.sine);
  return true;
}

/** The band a glyph occupies either side of its own baseline, from the
 *  face's own metrics. Memoized per (face, size) across a walk: a
 *  paragraph is a handful of distinct fonts however many letters it has. */
struct GlyphBand {
  float ascent = 0, descent = 0;
};

/** The memo key is the face AND the size: metrics scale with the size, and
 *  a mixed-style paragraph is one face at several of them — keyed on the
 *  face alone, every run after the first would wear the first one's band. */
using BandKey = std::pair<const void*, float>;

GlyphBand bandOf(const sigil::weave::ShapedWord* shaped,
                 std::vector<std::pair<BandKey, GlyphBand>>& memo) {
  if (!shaped || !shaped->typeface) return {};
  const BandKey key{shaped->typeface.get(), shaped->fontSize};
  for (const auto& [seen, band] : memo)
    if (seen == key) return band;
  SkFontMetrics metrics;
  sigil::weave::makeFont(shaped->typeface, shaped->fontSize)
      .getMetrics(&metrics);
  // Skia reports the ascent as a NEGATIVE offset from the baseline; the band
  // wants both halves positive.
  const GlyphBand band{-metrics.fAscent, metrics.fDescent};
  memo.emplace_back(key, band);
  return band;
}

/** One glyph's advance box, placed and turned the way the layout placed and
 *  turned it, as an axis-aligned bound.
 *
 *  The box is taken around the ADVANCE CENTRE the rest pose reports, which
 *  is what makes one rule cover all four baselines: a wrapped line and a
 *  mixed-style run differ only in where the centre and the band are, a path
 *  run and a rotated column run differ only in which way the box is turned,
 *  and an upright vertical glyph's advance runs down the column instead of
 *  across it. ONE body for three readers — the beatsOf query, the mark
 *  resolver and a pass track's uUnitRect — so none can disagree about
 *  where a unit is. */
SkRect glyphBox(const sigil::weave::PlacedGlyph& placed, const RestPose& pose,
                const GlyphBand& band) {
  const float size = placed.shaped ? placed.shaped->fontSize : 0.0f;
  const bool upright = placed.shaped && placed.shaped->vertical;
  const float halfAlong = std::abs(placed.advance) * 0.5f;
  // Along the advance, then across it. An upright vertical glyph advances
  // DOWN its column and is about one em wide across it; everything else
  // advances along its baseline and stands `band` tall across it.
  const float x0 = upright ? -size * 0.5f : -halfAlong;
  const float x1 = upright ? size * 0.5f : halfAlong;
  const float y0 = upright ? -halfAlong : -band.ascent;
  const float y1 = upright ? halfAlong : band.descent;
  SkRect box = SkRect::MakeEmpty();
  bool first = true;
  for (const SkPoint corner :
       {SkPoint{x0, y0}, SkPoint{x1, y0}, SkPoint{x1, y1}, SkPoint{x0, y1}}) {
    const SkPoint at{
        pose.centre.x() + corner.x() * pose.cosine - corner.y() * pose.sine,
        pose.centre.y() + corner.x() * pose.sine + corner.y() * pose.cosine};
    if (first) {
      box = SkRect::MakeLTRB(at.x(), at.y(), at.x(), at.y());
      first = false;
    } else {
      box.fLeft = std::min(box.fLeft, at.x());
      box.fTop = std::min(box.fTop, at.y());
      box.fRight = std::max(box.fRight, at.x());
      box.fBottom = std::max(box.fBottom, at.y());
    }
  }
  return box;
}

/** The seed a pass hands its unit in `uUnitPhase[i].y`: distinct per
 *  (outer, inner) beat, in [1, 256), and a pure function of the beat's
 *  numbering — so it is the same seed on every frame and after every
 *  relayout, which is what lets a seeded dissolve settle and cache. */
float passUnitSeed(uint32_t outer, uint32_t inner) {
  Rng rng(((uint64_t)outer << 32) | (uint64_t)inner);
  return 1.0f + rng.unit() * 255.0f;
}

}  // namespace

void Composer::Impl::paintTextFx(Instance& inst, SkCanvas& canvas,
                                 const sigil::weave::PaintStyle* override,
                                 const TextPath* onPath, SkSize size,
                                 const PaintContext& ctx) {
  if (!inst.paragraph) return;  // no content materialized: nothing to draw
  const std::span<const Track> tracks = tracksOf(*inst.desc);

  // ONE COMPOSITION ORDER, stated here because everything below assumes it:
  // THE BASELINE PLACES THE GLYPH, THEN THE TRACKS DEVIATE FROM THAT
  // PLACEMENT, IN THE FRAME THE BASELINE PUT IT IN. On a path run that
  // means a rise lifts a letter off the curve along its own local
  // perpendicular rather than straight up the canvas, and a track's
  // rotation adds to the tangent it was already turned to. The two are not
  // alternatives and neither wins: `fx()` and `onPath()` compose.
  if (onPath) ensurePathLayout(inst, *onPath, size);
  const bool ridesPath = onPath && inst.pathValid;
  if (onPath && !ridesPath) return;  // no measurable baseline: nothing rides
  const sigil::weave::ParagraphLayout& layout =
      ridesPath ? inst.pathLayout : inst.textLayout;

  // ONE walk builds the structure every track selects and staggers over —
  // the glyph list with its word/line/cluster/sentence numbering. Held
  // thread-locally: a frame of animated text rebuilds it, but never
  // reallocates it.
  static thread_local detail::GlyphStructure structure;
  structure.build(layout, *inst.paragraph);
  const uint32_t count = (uint32_t)structure.glyphs.size();
  if (count == 0) return;

  // WHERE ALONG the baseline the run sits this frame, as arc length. The
  // layout was built at the phase's RESTING value, so this is the delta the
  // paint applies on top — zero for a phase that never moves, and the whole
  // marquee for one bound to an output. It shifts the run's ENTRY POINT,
  // which is the same shift whichever way the run reads.
  float phaseArc = 0;
  if (ridesPath)
    phaseArc = (inst.resolvePathAt() - inst.pathRestAt) * inst.pathTotalLength;

  // Selections, resolved once per (content, layout width, selector list).
  // A regular expression over the paragraph is a per-EDIT cost this way,
  // not a per-frame one.
  bool selectionsStale = inst.selectionRev != inst.contentRev ||
                         inst.selectionWidth != inst.measuredForWidth ||
                         inst.selectionKeys.size() != tracks.size();
  if (!selectionsStale)
    for (size_t i = 0; i < tracks.size(); ++i)
      if (!(inst.selectionKeys[i] == tracks[i].where)) {
        selectionsStale = true;
        break;
      }
  if (!selectionsStale)
    for (const std::vector<uint8_t>& mask : inst.selectionMasks)
      if (mask.size() != count) {
        selectionsStale = true;
        break;
      }
  if (selectionsStale) {
    inst.selectionKeys.clear();
    inst.selectionMasks.clear();
    inst.selectionKeys.reserve(tracks.size());
    inst.selectionMasks.reserve(tracks.size());
    for (const Track& track : tracks) {
      inst.selectionKeys.push_back(track.where);
      inst.selectionMasks.push_back(detail::resolveSelection(
          track.where, structure, *inst.paragraph, inst.textNamedRuns));
    }
    inst.selectionRev = inst.contentRev;
    inst.selectionWidth = inst.measuredForWidth;
  }

  // Each track's cascade, numbered against whichever list its stagger names
  // — its own selection by default, the whole paragraph under beats::Text.
  struct Resolved {
    const Track* track = nullptr;
    const std::vector<uint8_t>* selected = nullptr;
    detail::TrackCascade resolved;
    float master = 1.0f;
  };
  // Kept across frames and reused in place: clearing would drop the two
  // per-glyph vectors' allocations, which is a fresh pair per track per
  // text node per frame on a page full of animated type.
  static thread_local std::vector<Resolved> resolved;
  size_t used = 0;
  for (size_t i = 0; i < tracks.size(); ++i) {
    const Track& track = tracks[i];
    if (!track.effect) continue;
    if (used == resolved.size()) resolved.emplace_back();
    Resolved& r = resolved[used++];
    r.track = &track;
    r.selected = &inst.selectionMasks[i];
    const AnimatedFloat* anim =
        i < inst.trackAnims.size() ? inst.trackAnims[i].get() : nullptr;
    r.master =
        std::clamp(inst.resolveFloatAt(anim, track.progress), 0.0f, 1.0f);
    r.resolved.build(track.stagger, structure, *r.selected);
  }
  // A path run with no tracks still draws: every glyph keeps the identity
  // deviation and rests on the curve.
  if (used == 0 && !ridesPath) return;
  const std::span<const Resolved> live(resolved.data(), used);

  // PASS TRACKS (fx::pass): each renders its addressed glyphs into its own
  // lane instead of the canvas, accumulating one rect and one local time
  // per (outer, inner) beat — the same enumeration beatsOfTrack reports,
  // from the same TrackCascade, so the pass and the query cannot disagree
  // about the schedule. A glyph a pass addresses draws only inside that
  // pass's layer; a glyph two passes address renders in both.
  struct PassLane {
    const Resolved* source = nullptr;
    sigil::weave::GlyphRSXformBatches batches;
    std::vector<std::pair<uint32_t, uint32_t>> keys;  // (outer, inner) beats
    std::vector<SkRect> rects;                        // one per beat
    std::vector<float> locals;                        // localT per beat
  };
  std::vector<std::unique_ptr<PassLane>> passes;
  for (const Resolved& r : live)
    if (r.track->effect.passMaterial()) {
      passes.push_back(std::make_unique<PassLane>());
      passes.back()->source = &r;
    }
  std::vector<std::pair<BandKey, GlyphBand>> bandMemo;

  static thread_local sigil::weave::GlyphRSXformBatches batches;
  batches.clear();

  // WHERE THIS RUN'S GLYPHS LAND MOVES FROM FRAME TO FRAME, which is what
  // decides whether their origins go on Skia's subpixel phase grid or on
  // whole pixels. Two ways a run creeps: a driven baseline phase (the
  // marquee runs under the type) and a driven transform at or above the
  // node (the figure turns under the type). Both make every letter's device
  // position advance by a fraction of a pixel per frame, which whole-pixel
  // origins cannot express — each letter stands still and then hops a whole
  // pixel at its own moment.
  //
  // Read off the DECLARATION rather than off a frame-to-frame diff, so a
  // marquee parked at a phase keeps the placement it was turning with. A
  // diff would hand a stopping ring one last quarter-pixel shift at the
  // moment it settled, which is a tick in exactly the place a tick is most
  // visible.
  const bool pathDriven =
      ridesPath && (onPath->at.binding() ||
                    (inst.anims[Instance::kTextPathAt] &&
                     inst.anims[Instance::kTextPathAt]->value.isConnected()));
  batches.subpixel = pathDriven || inst.placementUnderMotion;
  // A pass lane draws the same run into its own layer, and a letter must
  // not sit one place inside a pass and another outside it.
  for (const std::unique_ptr<PassLane>& lane : passes)
    lane->batches.subpixel = batches.subpixel;

  const PoseContext poseCtx{&inst, &layout, onPath, ridesPath, phaseArc};

  uint32_t ordinal = 0;
  sigil::weave::forEachPlacedGlyph(
      layout, *inst.paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const uint32_t g = ordinal++;
        if (g >= count) return;
        RestPose pose;
        if (!restPoseOf(poseCtx, placed, pose)) return;
        GlyphInfo info = structure.glyphs[g];

        // Every track that addresses this glyph, composed: offsets, shear
        // and rotations add, scale, alpha and the colour multiplier
        // multiply, and the two substitutions are last-one-wins.
        // A glyph no track addresses keeps the identity deviation and
        // draws at rest.
        GlyphMod mod;
        bool continuous = false;
        for (const Resolved& r : live) {
          if (!(*r.selected)[g]) continue;
          // A pass deviates nothing per glyph — its whole evaluation is
          // the layer draw below, downstream of every deviation here.
          if (r.track->effect.passMaterial()) continue;
          const detail::TrackCascade& rc = r.resolved;
          info.unitIndex = rc.outerUnit[g];
          info.unitCount =
              std::max<uint32_t>((uint32_t)rc.cascade.outerOrder.size(), 1u);
          const float t =
              rc.cascade.localTime(r.master, rc.outerUnit[g],
                                   rc.innerUnit.empty() ? 0u : rc.innerUnit[g]);
          Rng rng(detail::glyphSeed(info));
          detail::compose(mod, r.track->effect(info, t, rng));
          continuous |= r.track->continuous;
        }
        if (mod.alpha <= 0.003f || mod.scale <= 0.001f) return;
        // SNAP what an effect can drive continuously. Every distinct value
        // below is a distinct batch bucket AND a distinct glyph-atlas
        // strike, so a smooth sweep left alone would rasterize every
        // addressed letter afresh on every frame. Track::continuous is the
        // opt-out, and it buys smoothness with exactly that cost.
        const auto snap = [continuous](float value, float ceiling) {
          const float clamped = std::clamp(value, 0.0f, ceiling);
          return continuous ? clamped : std::round(clamped * 32.0f) / 32.0f;
        };
        // The colour multiplier's ALPHA folds into the fade rather than
        // snapping on a ladder of its own: two independent 32-step alphas
        // would be a thousand buckets where one is thirty-two.
        const float alpha = snap(mod.alpha * mod.colorMul.fA, 1.0f);
        if (alpha <= 0.0f) return;
        // A multiplier above 1 brightens, which is a legitimate tint; the
        // ceiling is only there so a runaway number cannot mint buckets
        // without bound.
        constexpr float kTintCeiling = 4.0f;
        const SkColor4f tint{snap(mod.colorMul.fR, kTintCeiling),
                             snap(mod.colorMul.fG, kTintCeiling),
                             snap(mod.colorMul.fB, kTintCeiling), 1.0f};
        // The additive and screen terms ride the same 32-step ladder,
        // ceilinged at 1: an add past full is clamped at the draw anyway,
        // and a screen past full has no headroom left to lift. RGB only —
        // their alpha components state nothing at a draw. The snap is what
        // bounds the memoized filter population, and Track::continuous
        // lifts it here exactly as it does for the multiplier.
        const SkColor4f flash{snap(mod.colorAdd.fR, 1.0f),
                              snap(mod.colorAdd.fG, 1.0f),
                              snap(mod.colorAdd.fB, 1.0f), 0.0f};
        const SkColor4f glow{snap(mod.colorScreen.fR, 1.0f),
                             snap(mod.colorScreen.fG, 1.0f),
                             snap(mod.colorScreen.fB, 1.0f), 0.0f};
        float cosv = 1.0f, sinv = 0.0f;
        if (mod.rotateDeg != 0) {
          const float radians = mod.rotateDeg * 0.017453293f;
          if (continuous) {
            cosv = std::cos(radians);
            sinv = std::sin(radians);
          } else {
            // The SAME size-cut ladder the baseline's tangent takes, and
            // for the same reason: a track's rotation composes with that
            // tangent onto one glyph, so a coarser ladder here would be the
            // coarsest thing in the letter's motion and would tick where the
            // baseline no longer does. Track::continuous is still the opt-out
            // that buys the exact angle at a fresh strike per letter per
            // frame.
            sigil::weave::quantizeAngle(
                radians,
                tangentLadderSteps(placed.shaped ? placed.shaped->fontSize
                                                 : 0.0f),
                cosv, sinv);
          }
        }
        const float halfAdvance = placed.advance * 0.5f;
        // The glyph-local back-out from the pose centre to the draw origin.
        // Horizontal type and tate-chu-yoko take the (halfAdvance, 0) the
        // RSXform convention assumes; an upright glyph in a column does not,
        // because half of ITS advance is a step down the page.
        const SkVector local =
            pose.centreOffset.value_or(SkVector{halfAdvance, 0});
        // THE DEVIATION APPLIES IN THE REST POSE'S OWN FRAME. On a level
        // baseline that is the canvas frame and this is the identity, so a
        // plain run is untouched; on a curve it is what makes `fx::rise`
        // lift a letter off the CURVE rather than off the canvas, and what
        // keeps a stagger's shove tangential to the lettering it belongs
        // to. The rotations compose the same way: the track's angle turns
        // the glyph from wherever the baseline had already turned it.
        const SkPoint centre = {
            pose.centre.x() + pose.cosine * mod.dx - pose.sine * mod.dy,
            pose.centre.y() + pose.sine * mod.dx + pose.cosine * mod.dy};
        const float turnCos = pose.cosine * cosv - pose.sine * sinv;
        const float turnSin = pose.sine * cosv + pose.cosine * sinv;

        // The whole span style rides along, so a letter in flight keeps the
        // gradient, stroke and glow passes it was styled with — and the
        // textFill/textStroke override, when the node carries one.
        sigil::weave::GlyphDress dress;
        dress.alphaScale = alpha;
        dress.colorMul = tint;
        dress.colorAdd = flash;
        dress.colorScreen = glow;
        if (pose.centreOffset) dress.centreOffset = &*pose.centreOffset;
        if (mod.axis && placed.shaped)
          dress.face =
              drivenFace(fonts, placed.shaped->typeface,
                         placed.shaped->fontSize, *mod.axis, continuous);
        SkGlyphID glyph = placed.glyph;
        if (mod.codepoint && placed.shaped)
          if (const SkGlyphID substitute =
                  substituteGlyph(fonts, placed.shaped->typeface, placed.glyph,
                                  mod.codepoint, placed.shaped->vertical))
            glyph = substitute;

        // ROUTING, per glyph and not per node: an RSXform carries a
        // rotation and ONE scale, so a glyph that shears or scales
        // unevenly draws under its own matrix while every glyph that does
        // not keeps the shared transform array untouched.
        SkMatrix matrix;
        if (mod.skewXDeg != 0 || mod.skewYDeg != 0 || mod.scaleX != 1 ||
            mod.scaleY != 1) {
          matrix.setAll(turnCos, -turnSin, centre.x(), turnSin, turnCos,
                        centre.y(), 0, 0, 1);
          // ONE shear carrying both angles, as the node's own skew lanes
          // take them — not an x shear applied after a y one, which would
          // put a product of the two tangents on the diagonal and scale the
          // glyph as well as leaning it.
          if (mod.skewXDeg != 0 || mod.skewYDeg != 0)
            matrix.preSkew(std::tan(mod.skewXDeg * 0.017453293f),
                           std::tan(mod.skewYDeg * 0.017453293f));
          matrix.preScale(mod.scale * mod.scaleX, mod.scale * mod.scaleY);
          // Innermost, so the pivot shift rides the scale exactly as it
          // does inside an RSXform.
          matrix.preTranslate(-local.x(), -local.y());
          dress.matrix = &matrix;
        } else {
          dress.center = centre;
          dress.cosine = turnCos * mod.scale;
          dress.sine = turnSin * mod.scale;
        }
        // ROUTE: a glyph a pass addresses is drawn inside that pass's
        // layer — with the deviation just composed, because the pass reads
        // pixels and the deviated pixels are the node's truth — and never
        // directly as well, which would double it under the pass's output.
        bool inPass = false;
        for (const std::unique_ptr<PassLane>& lane : passes) {
          if (!(*lane->source->selected)[g]) continue;
          inPass = true;
          lane->batches.addGlyph(placed.shaped,
                                 override ? *override : *placed.paint, glyph,
                                 halfAdvance, dress);
          // The beat this glyph belongs to, and its box joined into that
          // beat's rect — the same (outer, inner) walk beatsOfTrack takes.
          const detail::TrackCascade& rc = lane->source->resolved;
          const uint32_t outer = rc.outerUnit[g];
          const uint32_t inner = rc.innerUnit.empty() ? 0u : rc.innerUnit[g];
          const SkRect box =
              glyphBox(placed, pose, bandOf(placed.shaped, bandMemo));
          bool joined = false;
          for (size_t i = lane->keys.size(); i-- > 0;)
            if (lane->keys[i].first == outer && lane->keys[i].second == inner) {
              lane->rects[i].join(box);
              joined = true;
              break;
            }
          if (!joined) {
            lane->keys.emplace_back(outer, inner);
            lane->rects.push_back(box);
            lane->locals.push_back(
                rc.cascade.localTime(lane->source->master, outer, inner));
          }
        }
        if (!inPass)
          batches.addGlyph(placed.shaped, override ? *override : *placed.paint,
                           glyph, halfAdvance, dress);
      });
  batches.draw(&canvas);

  // THE PASSES, in track declaration order, each once: record the lane's
  // glyphs as a picture, hand it to the material as `uContent`, upload the
  // per-beat rects and phases, and draw ONE rect — the node's box grown by
  // the track's reach, which is the pass's whole footprint. The picture
  // shader rasterizes at the device's resolution, so the letters stay
  // sharp on a scaled host with no supersampled bake; everything is in the
  // node's own px, the frame main(xy) receives.
  for (const std::unique_ptr<PassLane>& lane : passes) {
    const auto n = (uint32_t)lane->keys.size();
    if (n == 0) continue;  // the selection resolved nothing: nothing to burn
    // THE DECLARED REST (fx::pass(…).restsAt(…)): when EVERY beat's
    // resolved local time sits on a declared pass-through phase, the layer
    // and the shader are skipped and the glyphs draw directly — the route
    // the compile refusal already takes, so a resting pass is
    // byte-identical to resting type. The test is EXACT equality, which is
    // what the schedule supplies: a one-shot cascade clamps a unit to
    // exactly 0 before its beat and exactly 1 after it. Under a looping
    // cascade a unit touches 0 only at the instant its beat re-opens, so a
    // rest declared at 0 effectively never engages there — correctly, the
    // cycle is always mid-flight somewhere — while units genuinely REST at
    // exactly 1 between beats, so a rest at 1 engages whenever no beat is
    // mid-cycle.
    const std::span<const float> rests =
        lane->source->track->effect.restPhases();
    if (!rests.empty()) {
      bool atRest = true;
      for (const float local : lane->locals) {
        bool declared = false;
        for (const float rest : rests) declared |= local == rest;
        if (!declared) {
          atRest = false;
          break;
        }
      }
      if (atRest) {
        lane->batches.draw(&canvas);
        continue;
      }
    }
    const float reach = lane->source->track->reachPx();
    const SkRect bounds =
        SkRect::MakeWH(size.width(), size.height()).makeOutset(reach, reach);
    // THE LAYER'S OWN FRAME is tile space — `bounds` translated so its
    // reach corner is the origin — and this is the ONE place node px and
    // tile px meet: the recording translates the glyphs in, and the local
    // matrix below translates the sampling back out. Skia's picture shader
    // anchors its rasterized tile at the local origin whatever the tile
    // rect's own origin says (the tile's offset survives into neither
    // backend's sampling matrix), so a tile handed to it starting at
    // (-reach, -reach) would land the whole layer a reach away from the
    // glyphs. Everything outside this pairing — main(xy), uUnitRect, the
    // rect drawn — stays in the node's own px and never sees tile space.
    const SkRect tile = SkRect::MakeWH(bounds.width(), bounds.height());
    SkPictureRecorder recorder;
    SkCanvas* layerCanvas = recorder.beginRecording(tile);
    layerCanvas->translate(reach, reach);
    lane->batches.draw(layerCanvas);
    const sk_sp<SkPicture> layer = recorder.finishRecordingAsPicture();
    static thread_local std::vector<float> rects, phases;
    rects.clear();
    phases.clear();
    rects.reserve((size_t)n * 4);
    phases.reserve((size_t)n * 2);
    for (uint32_t i = 0; i < n; ++i) {
      const SkRect& r = lane->rects[i];
      rects.insert(rects.end(), {r.x(), r.y(), r.width(), r.height()});
      phases.push_back(lane->locals[i]);
      phases.push_back(passUnitSeed(lane->keys[i].first, lane->keys[i].second));
    }
    detail::TextPassInputs inputs;
    const SkMatrix toTile = SkMatrix::Translate(-reach, -reach);
    inputs.content = layer->makeShader(SkTileMode::kDecal, SkTileMode::kDecal,
                                       SkFilterMode::kLinear, &toTile, &tile);
    inputs.rects = rects.data();
    inputs.phases = phases.data();
    inputs.units = n;
    const Material* material = lane->source->track->effect.passMaterial();
    sk_sp<SkShader> pass = material->resolvePass(inputs, ctx);
    if (!pass) {
      // The refusal already said why (no source, or it does not compile):
      // show resting letters rather than nothing, so the text survives
      // the mistake.
      lane->batches.draw(&canvas);
      continue;
    }
    SkPaint paint;
    paint.setShader(std::move(pass));
    canvas.drawRect(bounds, paint);
  }
}

// ---------------------------------------------------------------------------
// THE SCHEDULE, READ BACK
//
// Resolved out of the same three pieces the painter uses and in the same
// order — the glyph structure, the track's selection, and TrackCascade — so
// a mark placed from this is on the beat the glyphs are on by construction
// rather than by an author keeping two sets of numbers in step.
//
// Computed on demand rather than recorded during paint. A settled text node
// stops painting and replays a picture, and a query that read a paint-time
// buffer would answer with whatever the last live frame left behind; a query
// that resolves against the current layout and the current progress answers
// for the frame being asked about.

namespace {

/** Once per (mark key) whose selector found no glyphs. */
void warnMarkSelectsNothing(const std::string& key) {
  static std::set<std::string> warned;
  if (!warned.insert(key).second) return;
  SkDebugf(
      "[compose] mark(\"%s\") selects no glyphs in this text, so it places "
      "nothing and draws nothing. A selector naming a word, line or sentence "
      "the passage does not have, a style name no run was written with, and "
      "a pattern that does not compile all resolve to nothing.\n",
      key.c_str());
}

}  // namespace

void Composer::Impl::resolveTextMarks(Instance& inst) {
  inst.textMarkRects.clear();
  if (!inst.desc || !inst.desc->textData || !inst.paragraph) return;
  const detail::TextData& textData = *inst.desc->textData;
  const std::vector<detail::MarkAnchor>& marks = textData.marks;
  if (marks.empty()) return;
  // A PATH-laid run's marks stand on the curve the letters stand on: the
  // baseline resolves against the node's box (so the caller must not ask
  // before layout has settled that box), and the pose below then places
  // each advance box exactly where the paint places its glyph. A mark is a
  // LAYOUT answer, so it stands where the run RESTS on the curve — a run
  // driven along its baseline (`at` bound) is a paint-time deviation, and
  // a layout rect that chased it would be stale by the next frame.
  const TextPath* onPath = textData.onPath ? &*textData.onPath : nullptr;
  if (onPath) {
    const SkRect rect = instanceRect(inst);
    ensurePathLayout(inst, *onPath, {rect.width(), rect.height()});
    if (!inst.pathValid) return;  // no measurable baseline: nothing lands
  }

  // The layout the letters are drawn from — the path one where the run
  // rides a curve, otherwise the flow one this node measures by, the same
  // placement `paragraphLayout()` reports.
  const sigil::weave::ParagraphLayout& layout =
      onPath ? inst.pathLayout : inst.textLayout;
  static thread_local detail::GlyphStructure structure;
  structure.build(layout, *inst.paragraph);
  if (structure.glyphs.empty()) return;
  const auto count = (uint32_t)structure.glyphs.size();

  const PoseContext poseCtx{&inst, &layout, onPath, onPath != nullptr, 0.0f};
  std::vector<std::pair<BandKey, GlyphBand>> bandMemo;
  for (const detail::MarkAnchor& anchor : marks) {
    const std::vector<uint8_t> selected = detail::resolveSelection(
        anchor.where, structure, *inst.paragraph, inst.textNamedRuns);
    // ONE RECT FOR THE WHOLE SELECTION: the union of every advance box it
    // addressed, built from the same pose and the same box the schedule
    // read-back builds a beat's rect from, so a mark and a beat cannot
    // disagree about where a unit is.
    SkRect box = SkRect::MakeEmpty();
    bool any = false;
    uint32_t ordinal = 0;
    sigil::weave::forEachPlacedGlyph(
        layout, *inst.paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
          const uint32_t g = ordinal++;
          if (g >= count || !selected[g]) return;
          RestPose pose;
          if (!restPoseOf(poseCtx, placed, pose)) return;
          const SkRect glyph =
              glyphBox(placed, pose, bandOf(placed.shaped, bandMemo));
          if (any) {
            box.join(glyph);
          } else {
            box = glyph;
            any = true;
          }
        });
    if (!any) {
      warnMarkSelectsNothing(anchor.key);
      continue;
    }
    inst.textMarkRects.emplace_back(anchor.key, box);
  }
}

namespace {
/** ONE TRACK'S SCHEDULE RESOLVED FOR A QUERY — the front half `beatsOfTrack`
 *  and `cascadeSpanOfTrack` share: which layout the letters are on, which
 *  glyphs the track addresses, and the cascade those glyphs run. ONE BODY,
 *  so the span, the beats and the glyphs cannot disagree about the
 *  schedule. False means the track runs nothing a query can report: no
 *  text, no such track, no effect, or a path baseline that has not
 *  resolved. */
struct TrackSchedule {
  const Track* track = nullptr;
  const TextPath* onPath = nullptr;
  bool ridesPath = false;
  const sigil::weave::ParagraphLayout* layout = nullptr;
  uint32_t glyphCount = 0;
  std::vector<uint8_t> selected;
  detail::TrackCascade resolved;
};

bool resolveTrackSchedule(Instance& inst, size_t trackIndex,
                          TrackSchedule& out) {
  if (!inst.desc || !inst.paragraph) return false;
  const std::span<const Track> tracks = tracksOf(*inst.desc);
  if (trackIndex >= tracks.size()) return false;
  out.track = &tracks[trackIndex];
  if (!out.track->effect) return false;

  // The layout the last draw() left standing — the path one where the run
  // rides a curve, so the beats are on the curve the letters are on.
  if (inst.desc->textData && inst.desc->textData->onPath.has_value())
    out.onPath = &inst.desc->textData->onPath.value();
  out.ridesPath = out.onPath && inst.pathValid;
  if (out.onPath && !out.ridesPath) return false;
  out.layout = out.ridesPath ? &inst.pathLayout : &inst.textLayout;

  static thread_local detail::GlyphStructure structure;
  structure.build(*out.layout, *inst.paragraph);
  out.glyphCount = (uint32_t)structure.glyphs.size();
  if (out.glyphCount == 0) return false;

  out.selected = detail::resolveSelection(out.track->where, structure,
                                          *inst.paragraph, inst.textNamedRuns);
  out.resolved.build(out.track->stagger, structure, out.selected);
  return true;
}
}  // namespace

float Composer::Impl::cascadeSpanOfTrack(Instance& inst, size_t trackIndex) {
  TrackSchedule schedule;
  if (!resolveTrackSchedule(inst, trackIndex, schedule)) return 0.0f;
  return schedule.resolved.cascade.totalMs;
}

std::vector<Beat> Composer::Impl::beatsOfTrack(Instance& inst,
                                               size_t trackIndex) {
  TrackSchedule schedule;
  if (!resolveTrackSchedule(inst, trackIndex, schedule)) return {};
  const Track& track = *schedule.track;
  const auto count = schedule.glyphCount;
  const std::vector<uint8_t>& selected = schedule.selected;
  const detail::TrackCascade& resolved = schedule.resolved;
  const sigil::weave::ParagraphLayout& layout = *schedule.layout;

  const AnimatedFloat* anim = trackIndex < inst.trackAnims.size()
                                  ? inst.trackAnims[trackIndex].get()
                                  : nullptr;
  const float master =
      std::clamp(inst.resolveFloatAt(anim, track.progress), 0.0f, 1.0f);

  float phaseArc = 0;
  if (schedule.ridesPath)
    phaseArc = (inst.resolvePathAt() - inst.pathRestAt) * inst.pathTotalLength;
  const PoseContext poseCtx{&inst, &layout, schedule.onPath, schedule.ridesPath,
                            phaseArc};

  // ONE BEAT PER (outer, inner) PAIR THE TRACK ACTUALLY RUNS, in draw order,
  // and the rect is the union of the glyphs THIS track addresses in it: a
  // partitioning track reports where its own half of each unit sits, which
  // is the half its effect moves.
  std::vector<Beat> beats;
  std::vector<std::pair<uint32_t, uint32_t>> keys;
  std::vector<std::pair<BandKey, GlyphBand>> bandMemo;
  uint32_t ordinal = 0;
  sigil::weave::forEachPlacedGlyph(
      layout, *inst.paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
        const uint32_t g = ordinal++;
        if (g >= count || !selected[g]) return;
        RestPose pose;
        if (!restPoseOf(poseCtx, placed, pose)) return;
        const uint32_t outer = resolved.outerUnit[g];
        const uint32_t inner =
            resolved.innerUnit.empty() ? 0u : resolved.innerUnit[g];
        const SkRect box =
            glyphBox(placed, pose, bandOf(placed.shaped, bandMemo));
        for (size_t i = keys.size(); i-- > 0;)
          if (keys[i].first == outer && keys[i].second == inner) {
            beats[i].rect.join(box);
            return;
          }
        Beat beat;
        beat.rect = box;
        beat.unitIndex = outer;
        beat.startMs = resolved.cascade.startMs(outer, inner);
        beat.localT = resolved.cascade.localTime(master, outer, inner);
        // A beat that has begun and not finished. The clamped local time
        // reads 0 both before the beat opens and exactly as it does, and 1
        // for the whole of the rest of the track's life.
        beat.active = beat.localT > 0.0f && beat.localT < 1.0f;
        keys.emplace_back(outer, inner);
        beats.push_back(beat);
      });
  return beats;
}

// ---------------------------------------------------------------------------
// Recording bounds

/** The rect this node's OWN paint covers, in its own local space — children
 *  excluded; recordBounds() below adds the child union. The node's box,
 *  grown by every declared bleed (decorations, stroke passes, echo offsets,
 *  band width profiles, material reserves), then joined with the geometry a
 *  layout rect does not bound at all: a routed connector/rail path, a text
 *  run's path baseline, and a borrowed band spine, each outset by its own
 *  reach. */
SkRect Composer::Impl::ownPaintBounds(Instance& inst) {
  const ElementNode& node = *inst.desc;
  const SkRect rect = instanceRect(inst);
  SkRect local = SkRect::MakeWH(rect.width(), rect.height());
  float bleed = 0;
  for (const Decoration& d : node.backgrounds)
    bleed = std::max(bleed, d.bleed());
  for (const Decoration& d : node.foregrounds)
    bleed = std::max(bleed, d.bleed());
  if (node.fxData)
    for (const Decoration& d : node.fxData->overlays)
      bleed = std::max(bleed, d.bleed());
  if (node.strokeData)
    for (const detail::StrokePass& pass : node.strokeData->passes)
      bleed = std::max(bleed, pass.what.bleed());
  // A band reaches profile.max() px off its spine, and a width profile is
  // REQUIRED to be able to report that number — which is the whole reason
  // `max()` is part of that interface. A width function that cannot state
  // its own maximum can only be clipped silently.
  if (const Across* band = node.bandWidth())
    bleed = std::max(bleed, band->profile.max());
  for (const Echo& e : echoesOf(node))
    bleed =
        std::max(bleed, std::max(std::abs(e.offset.fX), std::abs(e.offset.fY)));
  // An fx() track throws glyphs OUTSIDE the text's box — a rise starts
  // below the line, a scatter starts anywhere in its disc — and a cull
  // taken at the box truncates them at the cached picture or texture
  // bounds, exactly as an under-reported decoration bleed does. Each track
  // declares how far it reaches, the same over-report-is-safe contract
  // `bleed()` and `reach()` carry.
  for (const Track& t : tracksOf(node))
    if (t.effect) bleed = std::max(bleed, t.reachPx());
  // A Material can declare a reserve too: a fill whose own outline escapes
  // the node's box is truncated at the cached picture or texture bounds
  // otherwise, exactly as an under-reported decoration bleed is. Both
  // carriers are checked — the live/geometry slot and the static recipe.
  if (node.materialData) {
    if (node.materialData->live)
      bleed = std::max(bleed, node.materialData->live->bleed());
    if (node.materialData->recipe)
      bleed = std::max(bleed, node.materialData->recipe->bleed());
  }
  if (bleed > 0) local.outset(bleed, bleed);
  // Routed elements paint their derive-resolved PATH, which is not bounded
  // by the layout rect (a connector's box is one thing, its wire another) —
  // the cull must hold the route plus its stroke reach.
  if (node.deriveData &&
      (!node.deriveData->connectFrom.empty() ||
       !node.deriveData->railAnchors.empty()) &&
      !inst.connectorPath.isEmpty()) {
    SkRect route = inst.connectorPath.getBounds();
    route.outset(bleed + 8.0f, bleed + 8.0f);
    local.join(route);
  }
  // A PATH BASELINE is the same problem once more. The baseline resolves
  // against the node's own box, so a `shapes::` generator normally stays
  // inside it — but nothing requires that: a Shape may return a curve well
  // outside the box, and `TextPath::offset` rides the type further off it
  // again. The glyphs then stand an ascent above that curve and a descent
  // below it, plus whatever the tracks reach, so the cull holds the curve
  // outset by the whole band. Over-reporting is safe here as everywhere;
  // under-reporting truncates the run at the cached picture or texture
  // bounds with no diagnostic.
  if (node.textData && node.textData->onPath) {
    const TextPath& spec = *node.textData->onPath;
    const SkPath baseline = spec.path({rect.width(), rect.height()});
    if (!baseline.isEmpty()) {
      const TextMetrics band = metrics(node.textData->style, fonts);
      const float reach =
          std::max(band.ascent, band.descent) + std::abs(spec.offset) + bleed;
      SkRect curve = baseline.getBounds();
      curve.outset(reach, reach);
      local.join(curve);
    }
  }
  // A BAND is the same problem: the bleed above covers the width axis, but
  // a BORROWED spine (band(around(key))) can sit anywhere relative to this
  // node's own box, so the cull has to hold the spine itself — exactly the
  // routed case one paragraph up, and for the same reason.
  if (const Across* band = node.bandWidth()) {
    const SkPath spine =
        node.deriveData->bandSpine
            ? node.deriveData->bandSpine({rect.width(), rect.height()})
            : inst.bandSpine;
    if (!spine.isEmpty()) {
      SkRect swept = spine.getBounds();
      swept.outset(bleed + band->profile.max(), bleed + band->profile.max());
      local.join(swept);
    }
  }
  return local;
}

// ---------------------------------------------------------------------------
// travel(): the motion path
//
// The animated lane is `t` — WHERE ALONG the curve the node sits — so the
// whole bind() chain applies to the schedule while the Shape supplies the
// geometry. A Shape is a function of a SIZE, and the curve is resolved
// against the PARENT's box (the frame the node moves in), so a relayout
// re-shapes the curve under a moving node. `t` is untouched by that: the
// node slides to the same fraction of the new curve rather than jumping to
// a different phase of its schedule.

std::optional<std::pair<SkPoint, float>> Composer::Impl::motionPathSample(
    Instance& inst, const SkSize& frame) {
  const ElementNode& node = *inst.desc;
  if (!node.motionData || !(bool)node.motionData->path) return std::nullopt;
  const MotionPath& spec = *node.motionData;

  // The table, cached against the two inputs that determine it: the Shape
  // VALUE and the size it was resolved at. No dirty flag — a comparable
  // scheme keeps its table across describes, a raw callable re-measures
  // (which is the escape hatch's documented cost, here as everywhere).
  if (!inst.motion) inst.motion = std::make_unique<Instance::MotionCache>();
  Instance::MotionCache& cache = *inst.motion;
  if (!(cache.shape == spec.path) || cache.size.width() != frame.width() ||
      cache.size.height() != frame.height()) {
    cache.shape = spec.path;
    cache.size = frame;
    cache.contours.clear();
    cache.starts.clear();
    cache.total = 0;
    cache.closed = true;
    const SkPath resolved = spec.path(frame);
    SkContourMeasureIter iter(resolved, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      if (!(len > 0)) continue;
      cache.closed = cache.closed && contour->isClosed();
      cache.starts.push_back(cache.total);
      cache.total += len;
      cache.contours.push_back(std::move(contour));
    }
    if (cache.contours.empty()) cache.closed = false;
  }
  if (!(cache.total > 0))
    return std::nullopt;  // no measurable length ⇒ not engaged

  // WRAP on a closed curve, CLAMP on an open one.
  const auto walk = [&](float u) {
    float w = cache.closed ? std::fmod(u, 1.0f) : std::clamp(u, 0.0f, 1.0f);
    if (cache.closed && w < 0.0f) w += 1.0f;
    const float want = w * cache.total;
    size_t i = cache.contours.size() - 1;
    for (size_t c = 0; c + 1 < cache.contours.size(); ++c)
      if (want < cache.starts[c + 1]) {
        i = c;
        break;
      }
    SkPoint pos{0, 0};
    SkVector tan{0, 0};
    const float len = cache.contours[i]->length();
    (void)cache.contours[i]->getPosTan(
        std::clamp(want - cache.starts[i], 0.0f, len), &pos, &tan);
    return pos;
  };

  const float t = inst.resolveFloat(Instance::kMotionT, spec.t);
  const SkPoint here = walk(t);
  float orient = 0;
  if (spec.lookAhead != 0.0f) {
    SkVector chord = walk(t + spec.lookAhead) - here;
    // At the end of an OPEN curve the forward chord collapses; hold the
    // last good one rather than reading atan2(0, 0).
    if (chord.length() <= 1e-6f) chord = here - walk(t - spec.lookAhead);
    if (chord.length() > 1e-6f)
      orient = std::atan2(chord.y(), chord.x()) * 180.0f / SK_FloatPI;
  }
  return std::make_pair(here, orient);
}

Composer::Impl::NodeTransform Composer::Impl::transformOf(Instance& inst) {
  const ElementNode& node = *inst.desc;
  NodeTransform out;
  out.rot = inst.resolveFloat(Instance::kRotate, node.paint.rotate);
  out.scl = inst.resolveFloat(Instance::kScale, node.paint.scale);
  out.sx = inst.resolveFloat(Instance::kScaleX, node.paint.scaleX);
  out.sy = inst.resolveFloat(Instance::kScaleY, node.paint.scaleY);
  out.skx = inst.resolveFloat(Instance::kSkewX, node.paint.skewX);
  out.sky = inst.resolveFloat(Instance::kSkewY, node.paint.skewY);

  const SkRect rect = instanceRect(inst);
  // The curve is resolved in the frame the node MOVES in — its parent's
  // box (a root node has none, so its own box, which is the canvas).
  const SkRect frameRect = inst.parent ? instanceRect(*inst.parent) : rect;
  if (std::optional<std::pair<SkPoint, float>> sample = motionPathSample(
          inst, SkSize{frameRect.width(), frameRect.height()})) {
    // PRECEDENCE: the path drives position OUTRIGHT (the lanes are not
    // read at all), and ADDS its tangent angle to rotate() rather than
    // replacing it — see MotionPath.
    const SkPoint origin =
        resolveOrigin(node.paint, rect.width(), rect.height());
    out.tx = sample->first.x() - rect.left() - origin.x();
    out.ty = sample->first.y() - rect.top() - origin.y();
    out.rot += sample->second;
    return out;
  }
  out.tx = inst.resolveFloat(Instance::kTx, node.paint.translateX);
  out.ty = inst.resolveFloat(Instance::kTy, node.paint.translateY);
  return out;
}

/** The rect a node's RECORDING must cover, in its own local space: its own
 *  paint bounds (ownPaintBounds above), unioned with every child's bounds
 *  mapped through that child's layout offset and static paint transforms.
 *
 *  WHAT THIS RECT DOES NOT DO, which is easy to assume it does:
 *  SkPictureRecorder does NOT reject ops outside the cull rect at record
 *  time. An op drawn wholly outside it is still recorded, even when the
 *  cull rect is EMPTY, and a plain drawPicture replays it — the pixels
 *  land. Culling against the cull rect happens only when a bounding-box
 *  hierarchy is attached (SkRTreeFactory clips each op's bounds to the cull
 *  rect as it builds the tree, so an outside op is dropped at PLAYBACK),
 *  and no BBH is attached here, so the picture path never culls.
 *  ComposeCullRect.PictureCullDoesNotCullWithoutABbh holds that behaviour
 *  down.
 *
 *  What the rect IS load-bearing for are this function's other three
 *  consumers, all of which clip for real: the BOUNDED saveLayer opened for
 *  a group opacity/blend and for a layer effect (saveLayer bounds ARE a
 *  clip), the Cache::Texture bake surface, which is sized from this rect
 *  mapped to device, and the dstIn coverage drawRect. A child translated
 *  beyond its parent's box vanishes through THOSE if the child union below
 *  is dropped — pinned by
 *  ComposeCache.OverflowingChildSurvives{GroupOpacityLayer,TextureBake}.
 *  Overflow is legal; the rect must hold it, the same way it must hold a
 *  decoration's declared bleed.
 *
 *  Animated transforms are fine here: resolveFloat reads the record-time
 *  value, and a RUNNING transform makes the subtree volatile, so nothing
 *  records at all. A clipped node contributes only its own box, because its
 *  children cannot escape it. */
SkRect Composer::Impl::recordBounds(Instance& inst) {
  const ElementNode& node = *inst.desc;
  SkRect local = ownPaintBounds(inst);
  if (node.clipContent) return local;
  for (auto& child : inst.children) {
    const ElementNode& cn = *child->desc;
    const SkRect crect = instanceRect(*child);
    SkRect cb = recordBounds(*child);  // child-local
    const NodeTransform tf = transformOf(*child);
    // The matrix comes from NodeTransform::matrix(), gate included, and not
    // from a copy of that build written here. One resolver, three consumers
    // — paint()'s matrix, this child union, and hitInstance()'s inverse —
    // and the three must build the SAME matrix or a node draws where it
    // cannot be hit. A hand-rolled gate here that omits a lane is
    // invisible: a child whose only transform was a per-axis scale would
    // contribute UNSCALED bounds, so its parent's effect layer, opacity
    // layer and texture bake would all be sized to the unscaled box and
    // truncate the overflow.
    const SkMatrix m = tf.matrix({crect.left(), crect.top()}, cn.paint,
                                 crect.width(), crect.height());
    local.join(m.mapRect(cb));
  }
  return local;
}

// ---------------------------------------------------------------------------
// The stacking painter

void Composer::Impl::paintContent(Instance& inst, SkCanvas& canvas,
                                  float contentScale, SkBlendMode leafBlend,
                                  float leafOpacity, Phase phase) {
  const ElementNode& node = *inst.desc;
  // The two halves of a node's paint, split at the children loop. A
  // split bake is only ever offered to a node with no layer effect — that
  // one WRAPS BOTH HALVES and a bake of the prefix alone would have to
  // reproduce it. A clip and a whole-node mask wrap both halves too, but
  // both are opened and closed inside EACH phase, so the phases stay a pair
  // of skips over an otherwise untouched function; the granular mask scopes
  // below are each opened and closed inside the half they belong to.
  const bool emitOwn = phase != Phase::ChildrenOnly;
  const bool emitChildren = phase != Phase::OwnOnly;
  const SkRect ownRect = instanceRect(inst);
  const SkRect bounds = SkRect::MakeWH(ownRect.width(), ownRect.height());
  const SkRRect rrect = cornersRRect(bounds, node.corners);

  // The node's shape: routed connector/rail path, custom outline(), or the
  // corner-rounded box.
  const bool routed =
      node.deriveData && (!node.deriveData->connectFrom.empty() ||
                          !node.deriveData->railAnchors.empty());
  const Across* bandWidth = node.bandWidth();
  const bool customShape = (node.shapeFn || bandWidth) && !routed;
  SkPath outlinePath;
  if (routed) {
    outlinePath = inst.connectorPath;  // derive phase routed it
  } else if (bandWidth) {
    // A BAND's shape is derived: the region its spine sweeps at the
    // profile's width, on the declared side. The spine is guide data
    // (authored here) or borrowed geometry (derive resolved it).
    const SkPath spine =
        node.deriveData->bandSpine
            ? node.deriveData->bandSpine({bounds.width(), bounds.height()})
            : inst.bandSpine;
    outlinePath =
        detail::bandRegion(spine, *bandWidth, node.deriveData->bandFormation);
  } else if (customShape) {
    outlinePath = resolveOutline(inst, {bounds.width(), bounds.height()});
  } else {
    SkPathBuilder outlineBuilder;
    outlineBuilder.addRRect(rrect);
    outlinePath = outlineBuilder.detach();
  }

  // (clip() applies AFTER the decorations' outline is settled — see below:
  // decorations dress the outline and stay unclipped; fill/content/children
  // clip. The clip keeps the UNMASKED shape — a mask is a paint reveal.)
  const SkPath clipShape = outlinePath;

  // ---- the masking family, part 1: the BOUNDARY gates ---------------------
  //
  // Resolve each mask's gate once, then hand every paint output the version
  // of the boundary its selection earns. Two outputs trace a boundary — the
  // SURFACE (fill + echo re-stamps) and the MARKS (every decoration, every
  // span pass) — so at most two cut paths exist, and in the overwhelmingly
  // common case (`mask(by::spans(…))`, the whole node) they are the same
  // path and are computed once.
  //
  // The claim ledger is deliberately resolved against the UNCUT boundary
  // below: an overlap between two span passes is a description-level
  // mistake, and it must not be a mistake that blinks in and out between
  // 0.3 and 0.7 of a transition because a gate was shrinking one of them.
  const std::vector<Mask>* masks =
      node.hasMasks() ? &node.fxData->masks : nullptr;
  const std::vector<float> gateValues =
      masks ? inst.resolveGateValues() : std::vector<float>{};
  // The SHOW set each of surface / marks is left with, as fractions of the
  // whole boundary — absent when no spans gate selects it.
  std::optional<std::vector<Span>> surfaceShow, marksShow;
  // …and the per-NAMED-mark refinement, for masks that address one label.
  std::vector<std::pair<std::string, std::vector<Span>>> namedShow;
  if (masks) {
    SpanInput gateIn;
    gateIn.outline = &outlinePath;
    gateIn.fitRects = &inst.spanFitRects;
    size_t valueBase = 0;
    for (const Mask& m : *masks) {
      const size_t count = m.with.valueCount();
      if (m.with.kind != Gate::Kind::Spans) {
        valueBase += count;
        continue;
      }
      const std::vector<float> mine(
          gateValues.begin() + (long)std::min(valueBase, gateValues.size()),
          gateValues.begin() +
              (long)std::min(valueBase + count, gateValues.size()));
      valueBase += count;
      gateIn.values = &mine;
      const std::vector<Span> show =
          normalizeSpans(m.with.where.resolve(gateIn));
      // THE INTERSECTION LAW: stacked masks both have to pass, so a second
      // gate over the same target narrows the first, never widens it.
      const auto narrow = [&](std::optional<std::vector<Span>>& slot) {
        slot = slot ? intersectSpans(*slot, show) : show;
      };
      if (m.what.selects(Parts::kSurface)) narrow(surfaceShow);
      if (m.what.selects(Parts::kMarks)) narrow(marksShow);
      for (const std::string& label : m.what.names) {
        auto it = std::find_if(namedShow.begin(), namedShow.end(),
                               [&](const auto& e) { return e.first == label; });
        if (it == namedShow.end())
          namedShow.emplace_back(label, show);
        else
          it->second = intersectSpans(it->second, show);
      }
    }
  }
  // `trimmed` says the SURFACE's geometry is no longer the corner box,
  // which is what decides whether the fill draws a path or the cheap rrect.
  bool cut = false;
  const SkPath fullOutline = outlinePath;
  SkPath surfacePath =
      surfaceShow ? gateOutline(fullOutline, *surfaceShow, &cut) : fullOutline;
  // …and the marks' boundary, which is the SAME OBJECT whenever one mask
  // gates both — the overwhelmingly common case, and the reason a whole-node
  // spans gate walks the boundary once rather than twice.
  SkPath marksPath = !marksShow ? fullOutline
                     : (surfaceShow && *marksShow == *surfaceShow)
                         ? surfacePath
                         : gateOutline(fullOutline, *marksShow);
  const bool trimmed = cut;

  // The MARKS' boundary is what a decoration receives: every decoration
  // dresses the outline, and a spans gate over the marks is a cut of that
  // outline. The surface keeps its own (they are the same path, and the
  // same object, whenever one mask gates both — the common case).
  //
  // Built BEFORE the effect's saveLayer because an effect's child Material
  // resolves against it — the node's box, the node's clock, exactly what
  // Material::child hands a fill's children.
  const PaintContext paintCtx{
      {bounds.width(), bounds.height()},
      std::move(marksPath),
      elapsed(),
      contentScale,
      ticker.active(),
      &fonts,
      inst.borrowedPaths.empty() ? nullptr : &inst.borrowedPaths,
      &inst.stampCache,
      curToRoot,        // node→root, as paint() stacked it
      rootLayoutSize};  // …and the canvas it maps into

  // The node's own layer effect wraps everything painted here, so it is
  // captured by picture recordings and BAKED by texture snapshots. A LIVE
  // effect (bound uniforms, a live child material) resolves here per paint,
  // and computeVolatile has declared such a node volatile, so this
  // recording is never cached stale.
  const Effect* layerFx = layerEffectOf(node);
  const sk_sp<SkImageFilter> layerFilter =
      layerFx ? layerFx->resolvedImageFilter(&paintCtx) : nullptr;
  const bool hasEffect = (bool)layerFilter;
  if (hasEffect) {
    SkPaint effectPaint;
    effectPaint.setImageFilter(layerFilter);
    // BOUNDED: with nullptr bounds the layer allocates at the CLIP size, so
    // a small icon's drop shadow on a root-level canvas would filter the
    // whole canvas. recordBounds is what the subtree actually paints; Skia
    // expands it for the filter's own reach.
    const SkRect content = recordBounds(inst);
    canvas.saveLayer(&content, &effectPaint);
  }

  // ---- the masking family, part 2: the PLANE gates ------------------------
  //
  // `by::edge` and `by::shape` are canvas clips; `by::alpha` is a kDstIn
  // coverage layer. All three intersect for free — nested clips by
  // definition, stacked kDstIn layers by multiplication.
  //
  // A gate whose selection is EVERYTHING is hoisted to wrap the whole node
  // once. That is not only the cheap path: applying one antialiased clip
  // per paint group would compound its own edge coverage wherever the
  // groups overlap, so the hoisted form is also the only one whose edge is
  // the clip's own.
  struct PlaneGate {
    const Mask* mask = nullptr;
    float fraction = 1.0f;  // Edge
  };
  std::vector<PlaneGate> plane;
  bool granularPlane = false;
  if (masks) {
    size_t valueBase = 0;
    for (const Mask& m : *masks) {
      if (m.with.kind != Gate::Kind::Spans) {
        PlaneGate g;
        g.mask = &m;
        if (m.with.kind == Gate::Kind::Edge)
          g.fraction =
              valueBase < gateValues.size() ? gateValues[valueBase] : 1.0f;
        plane.push_back(g);
        granularPlane |= !m.what.isEverything();
      }
      valueBase += m.with.valueCount();
    }
  }

  /** An edge gate's half-plane: the region lying before a moving edge at
   *  `angleDeg`, built in the edge's own frame and rotated into place —
   *  {p : (p - mid)·d <= edge}. */
  const auto edgeRegion = [&](float angleDeg, float t01) {
    const float t = std::clamp(t01, 0.0f, 1.0f);
    const float rad = angleDeg * SK_FloatPI / 180.0f;
    const float c = std::cos(rad), s = std::sin(rad);
    const SkPoint mid{bounds.centerX(), bounds.centerY()};
    const float reach =
        0.5f * (std::abs(bounds.width() * c) + std::abs(bounds.height() * s));
    const float wide =
        SkPoint{bounds.width(), bounds.height()}.length() * 0.5f + 1.0f;
    const float edge = -reach + 2.0f * reach * t;
    SkPathBuilder b;
    b.addRect(SkRect::MakeLTRB(-reach - 1.0f, -wide, edge, wide));
    SkMatrix m = SkMatrix::RotateDeg(angleDeg);
    m.postTranslate(mid.x(), mid.y());
    return b.detach().makeTransform(m);
  };

  // One entry/exit pair. Region gates go on first (they commute with the
  // coverage layers, so the order between the two kinds is free and this
  // one keeps the layer pops simple); coverage layers are popped in
  // reverse, each drawing its Material's alpha through kDstIn.
  std::vector<SkPaint> coverStack;
  const auto enterGates = [&](bool wholeNode, Parts::Bits cls,
                              std::string_view label) -> int {
    if (plane.empty()) return -1;
    int base = -1;
    const auto hit = [&](const Mask& m) {
      if (m.what.isEverything() != wholeNode) return false;
      if (wholeNode) return true;
      return cls == Parts::kMarks ? m.what.selectsMark(label)
                                  : m.what.selects(cls);
    };
    for (const PlaneGate& g : plane) {
      const Mask& m = *g.mask;
      if (!hit(m) || m.with.kind == Gate::Kind::Coverage) continue;
      if (base < 0) base = canvas.getSaveCount();
      if (m.with.kind == Gate::Kind::Edge) {
        // A container of absolutely-positioned children measures ZERO, and
        // a half-plane built from an empty box is empty — so clipping to it
        // would hide the entire subtree even at a full reveal. A reveal at 1
        // must never hide anything, and an empty box has no axis to reveal
        // along in the first place.
        if (bounds.isEmpty()) continue;
        canvas.save();
        canvas.clipPath(edgeRegion(m.with.angleDeg, g.fraction), true);
      } else {  // Shape — and its complement, the missing clipOut()
        canvas.save();
        canvas.clipPath(
            m.with.region.resolve(fullOutline),
            m.with.outside ? SkClipOp::kDifference : SkClipOp::kIntersect,
            true);
      }
    }
    for (const PlaneGate& g : plane) {
      const Mask& m = *g.mask;
      if (!hit(m) || m.with.kind != Gate::Kind::Coverage) continue;
      if (base < 0) base = canvas.getSaveCount();
      const SkRect layerBox = recordBounds(inst);
      canvas.saveLayer(&layerBox, nullptr);
      SkPaint cover;
      cover.setAntiAlias(true);
      // The complement is the blend mode and nothing else: kDstOut is
      // dst·(1 - a), which is 1 - coverage exactly, for any source.
      cover.setBlendMode(m.with.outside ? SkBlendMode::kDstOut
                                        : SkBlendMode::kDstIn);
      if (m.with.coverage) {
        const Material& mat = *m.with.coverage;
        const Fill f = (mat.isAnimated() || mat.geometryDependent())
                           ? mat.resolve(paintCtx)
                           : mat.toFill();
        const bool luma = m.with.channel == Gate::Channel::Luma;
        if (f.kind == Fill::Kind::Shader && f.shaderValue)
          cover.setShader(luma ? lumaCoverageShader(f.shaderValue)
                               : f.shaderValue);
        else if (f.kind == Fill::Kind::Color)
          cover.setColor4f(
              luma ? lumaCoverageColor(f.colorValue) : f.colorValue, nullptr);
        else
          cover.setColor4f({0, 0, 0, 0},
                           nullptr);  // Fill::none() shows nothing
      }
      coverStack.push_back(std::move(cover));
    }
    return base;
  };
  const auto leaveGates = [&](int base, size_t coverBase) {
    while (coverStack.size() > coverBase) {
      canvas.drawRect(recordBounds(inst), coverStack.back());
      coverStack.pop_back();
      canvas.restore();
    }
    if (base >= 0) canvas.restoreToCount(base);
  };
  // The whole-node hoist, in wipe()'s old position.
  const size_t hoistCover = coverStack.size();
  const int hoistSaves = enterGates(true, Parts::kAll, {});

  // Span-qualified passes, resolved ONCE per paint however many halves
  // read them: the claim ledger is one ledger (StrokePass), and resolving
  // it twice would also re-walk the boundary three or four times for
  // nothing.
  //
  // THE CLAIM LEDGER READS THE UNMASKED BOUNDARY — `fullOutline`, not the
  // cut path. A claim is a statement about where a mark goes; a gate is a
  // statement about how much of it exists yet. Resolving claims against a
  // shrinking boundary would make the no-overlap diagnostic a function of
  // the clock.
  std::optional<std::vector<std::vector<Span>>> spanClaims;
  auto paintSpanHalf = [&](detail::StrokePass::Half half) {
    if (!node.hasStrokePasses()) return;
    if (!spanClaims) spanClaims = inst.resolveSpans(fullOutline);
    const std::vector<detail::StrokePass>& passes = node.strokeData->passes;
    for (size_t i = 0; i < passes.size() && i < spanClaims->size(); ++i) {
      if (passes[i].half != half || (*spanClaims)[i].empty()) continue;
      // …and the gate intersects the claim, which is the whole of
      // `.stroke(spans::corners(18), brk).mask(parts::marks(), upTo(t))`:
      // reticle brackets that light up as a sweep reaches them.
      std::vector<Span> run = (*spanClaims)[i];
      if (marksShow) run = intersectSpans(run, *marksShow);
      if (!passes[i].name.empty())
        for (const auto& [label, show] : namedShow)
          if (label == passes[i].name) run = intersectSpans(run, show);
      if (run.empty()) continue;
      const size_t cover = coverStack.size();
      const int saves =
          granularPlane ? enterGates(false, Parts::kMarks, passes[i].name) : -1;
      const PaintContext passCtx{
          paintCtx.size,
          detail::spanPath(fullOutline, run),
          paintCtx.elapsedSeconds,
          paintCtx.contentScale,
          paintCtx.animating,
          paintCtx.fonts,
          paintCtx.borrowed,
          nullptr,  // stamps: deliberately not shared with a span pass
          paintCtx.toRoot,
          paintCtx.rootSize};
      passes[i].what.paint(canvas, passCtx);
      if (granularPlane) leaveGates(saves, cover);
    }
  };

  /** Paint one unqualified mark, under whatever gates address it by name.
   *  The common case — no named mask, no granular plane gate — is the
   *  decoration's own paint call and nothing else. */
  const auto paintMark = [&](const Decoration& d, detail::MarkSlot slot,
                             size_t index) {
    std::string_view label;
    if (node.fxData)
      for (const detail::MarkLabel& l : node.fxData->markNames)
        if (l.slot == slot && l.index == index) {
          label = l.name;
          break;
        }
    const std::vector<Span>* refine = nullptr;
    if (!label.empty())
      for (const auto& [name, show] : namedShow)
        if (name == label) {
          refine = &show;
          break;
        }
    const size_t cover = coverStack.size();
    const int saves =
        granularPlane ? enterGates(false, Parts::kMarks, label) : -1;
    if (refine) {
      std::vector<Span> run = *refine;
      if (marksShow) run = intersectSpans(run, *marksShow);
      const PaintContext markCtx{paintCtx.size,
                                 gateOutline(fullOutline, run),
                                 paintCtx.elapsedSeconds,
                                 paintCtx.contentScale,
                                 paintCtx.animating,
                                 paintCtx.fonts,
                                 paintCtx.borrowed,
                                 nullptr,  // stamps: not shared with a mark
                                 paintCtx.toRoot,
                                 paintCtx.rootSize};
      d.paint(canvas, markCtx);
    } else {
      d.paint(canvas, paintCtx);
    }
    if (granularPlane) leaveGates(saves, cover);
  };

  // Background decorations paint beneath the fill (the CSS box-shadow
  // ordering): shadow and pattern layers first, then the surface.
  // Decorations are NEVER clipped — they dress the outline, so shadows keep
  // their reach and an outer stroke survives on a node that clips its
  // content.
  if (emitOwn) {
    for (size_t i = 0; i < node.backgrounds.size(); ++i)
      paintMark(node.backgrounds[i], detail::MarkSlot::Background, i);
    // Span-qualified BACKGROUND passes land here, in the background half,
    // under the fill and therefore under the content and the children —
    // the z-slot the deleted trim() revealed and a stroke pass could not
    // reach.
    paintSpanHalf(detail::StrokePass::Half::Background);
  }

  // clip() bounds the fill, the content, and the children — not the
  // decorations (above and below), which trace the outline itself.
  if (node.clipContent) {
    canvas.save();
    if (customShape || routed)
      canvas.clipPath(clipShape, true);
    else
      canvas.clipRRect(rrect, true);
  }

  // Fill (background): a live material resolves per frame from its bound
  // uniforms + the PaintContext; otherwise the stored Fill (binding, lerp, or
  // plain).
  std::optional<Fill> resolvedFill;
  if (!emitOwn) {
    // ChildrenOnly: the prefix above already ran into the bake. Skip
    // straight past the fill, the echoes, the overlays and the leaf
    // content to the children loop. (The outline and the clip/wipe/effect
    // wrappers above are recomputed rather than skipped — they are cheap,
    // they must stay balanced against their restores below, and the
    // foregrounds still trace the outline.)
  } else if (const Material* live = liveMaterialOf(node)) {
    resolvedFill = inst.hasPendingLiveFill ? inst.pendingLiveFill
                                           : live->resolve(paintCtx);
  } else if (node.paint.fill) {
    Fill fill;
    if (const choreograph::Output<Fill>* binding = node.paint.fill->binding())
      fill = binding->value();
    else if (inst.anims[Instance::kFillLerp] &&
             inst.anims[Instance::kFillLerp]->started &&
             inst.anims[Instance::kFillLerp]->value.isConnected()) {
      const float t = inst.anims[Instance::kFillLerp]->value.value();
      fill = inst.fillTo;
      for (int i = 0; i < 4; ++i)
        fill.colorValue.vec()[i] = inst.fillFrom.colorValue.vec()[i] +
                                   (inst.fillTo.colorValue.vec()[i] -
                                    inst.fillFrom.colorValue.vec()[i]) *
                                       t;
      fill.kind = Fill::Kind::Color;
    } else {
      ResolvedProp<Fill> resolved =
          resolveProp(*node.paint.fill, node.nodeTransition);
      fill = resolved.target;
    }
    resolvedFill = fill;
  }

  // The SURFACE — the fill and its echo re-stamps — under whatever gates
  // select `parts::surface()`.
  const size_t surfaceCover = coverStack.size();
  const int surfaceSaves =
      granularPlane && emitOwn ? enterGates(false, Parts::kSurface, {}) : -1;

  // Misprint echoes of the FILL SHAPE, under the real pass (bottom first).
  if (!echoesOf(node).empty() && resolvedFill &&
      resolvedFill->kind != Fill::Kind::None) {
    for (const Echo& e : echoesOf(node)) {
      SkPaint stamp;
      stamp.setAntiAlias(true);
      stamp.setColor4f(e.color, nullptr);
      canvas.save();
      canvas.translate(e.offset.fX, e.offset.fY);
      if (customShape || trimmed)
        canvas.drawPath(surfacePath, stamp);
      else
        canvas.drawRRect(rrect, stamp);
      canvas.restore();
    }
  }

  if (resolvedFill && resolvedFill->kind != Fill::Kind::None) {
    const Fill& fill = *resolvedFill;
    SkPaint paint;
    paint.setAntiAlias(true);
    if (fill.kind == Fill::Kind::Color)
      paint.setColor4f(fill.colorValue, nullptr);
    else
      paint.setShader(fill.shaderValue);
    // Leaf fast path: paint() proved a layer is unnecessary and routed the
    // node's blend/opacity straight onto the fill.
    paint.setBlendMode(leafBlend);
    if (leafOpacity < 1.0f) paint.setAlphaf(paint.getAlphaf() * leafOpacity);
    if (customShape || trimmed)
      canvas.drawPath(surfacePath, paint);
    else
      canvas.drawRRect(rrect, paint);
  }
  if (granularPlane && emitOwn) leaveGates(surfaceSaves, surfaceCover);

  // Overlays: over the fill, under the content and children. The slot a
  // textured button needs so its own hazard stripe does not grey out its
  // label. Unclipped like the other decorations — they dress the outline.
  if (emitOwn && node.fxData)
    for (size_t i = 0; i < node.fxData->overlays.size(); ++i)
      paintMark(node.fxData->overlays[i], detail::MarkSlot::Overlay, i);

  // Content, under whatever gates select parts::content().
  const size_t contentCover = coverStack.size();
  const int contentSaves =
      granularPlane && emitOwn ? enterGates(false, Parts::kContent, {}) : -1;
  if (emitOwn) switch (node.kind) {
      case Kind::Text:
        if (inst.paragraph) {
          // Yoga skips the measure callback when both dimensions are fully
          // determined (absolute + all four insets); lay out on demand at the
          // resolved width so such text still paints. Aligned text (center/
          // end/justify) additionally must be laid out at its FINAL width —
          // lines place within the flow width, so a measure-time constraint
          // that differs from the resolved box would push them off target.
          const bool onPathRun = node.textData && node.textData->onPath;
          // Vertical columns hang off the RIGHT edge of the measure, so the
          // resolved width decides WHERE the first column stands and not
          // only where the text wraps — the same reason aligned text is
          // re-laid here, one axis over.
          const bool verticalRun =
              inst.paragraph && inst.paragraph->writingMode() !=
                                    sigil::weave::WritingMode::kHorizontal;
          if (inst.measuredRev != inst.contentRev ||
              (!onPathRun && node.textData &&
               (verticalRun || node.textData->alignment() !=
                                   sigil::weave::TextAlignment::kStart) &&
               (inst.measuredForWidth != bounds.width() ||
                (verticalRun && inst.measuredForHeight != bounds.height()))))
            layoutText(inst, bounds.width(),
                       verticalRun ? bounds.height() : 1.0e6f);
          // Misprint echoes of the TEXT, under the real pass (fx() text
          // draws its own buckets — echoes skip it by contract).
          if (!echoesOf(node).empty() && !hasTextFx(node)) {
            for (const Echo& e : echoesOf(node)) {
              sigil::weave::PaintStyle stamp;
              stamp.foreground.setColor4f(e.color, nullptr);
              canvas.save();
              canvas.translate(e.offset.fX, e.offset.fY);
              inst.textLayout.drawBatched(&canvas, *inst.paragraph, &stamp);
              canvas.restore();
            }
          }
          const TextPath* onPath =
              onPathRun ? &*node.textData->onPath : nullptr;
          // textFill()/textStroke() resolve to ONE glyph-paint override,
          // and every draw that takes one takes the SAME one: a letter in
          // flight, and a letter on a curve, are painted exactly as a
          // resting letter is.
          const std::optional<sigil::weave::PaintStyle> metric =
              metricTextStyle(inst, paintCtx);
          const sigil::weave::PaintStyle* glyphPaint =
              metric ? &*metric : nullptr;
          // One draw for both: the baseline places the glyph and the tracks
          // deviate from that placement. Neither wins over the other.
          if (hasTextFx(node) || onPath) {
            paintTextFx(inst, canvas, glyphPaint, onPath,
                        {bounds.width(), bounds.height()}, paintCtx);
          } else {
            inst.textLayout.drawBatched(&canvas, *inst.paragraph, glyphPaint);
          }
        }
        break;
      case Kind::Image:
        if (imageAssetOf(node) && !imageAssetOf(node)->frames().empty()) {
          const auto& frame = imageAssetOf(node)->frameAt(elapsed() * 1000.0);
          if (frame.image) {
            const SkSamplingOptions sampling = node.imageData->sampling;
            if (node.imageData->region)
              canvas.drawImageRect(frame.image, *node.imageData->region, bounds,
                                   sampling, nullptr,
                                   SkCanvas::kStrict_SrcRectConstraint);
            else
              canvas.drawImageRect(frame.image, bounds, sampling);
          }
        }
        break;
      case Kind::Custom:
        if (node.customData && node.customData->program)
          node.customData->program(canvas, paintCtx);
        break;
      case Kind::Box:
      case Kind::Stack:
      case Kind::Slot:
        break;
    }

  if (granularPlane && emitOwn) leaveGates(contentSaves, contentCover);

  // Children in stacking order (each clean static child replays its own nested
  // picture — ancestor re-records don't repaint clean subtrees).
  const size_t kidsCover = coverStack.size();
  const int kidsSaves = granularPlane && emitChildren
                            ? enterGates(false, Parts::kChildren, {})
                            : -1;
  if (emitChildren)
    for (size_t index : inst.paintOrder) paint(*inst.children[index], canvas);
  if (granularPlane && emitChildren) leaveGates(kidsSaves, kidsCover);

  if (node.clipContent) canvas.restore();  // decorations below stay unclipped

  // FOREGROUNDS PAINT AFTER THE CHILDREN, so they belong to the children
  // half and can never be in an own-paint bake. The own half is the
  // contiguous PREFIX up to the children loop, which is not the same thing
  // as "everything except the children".
  if (emitChildren)
    for (size_t i = 0; i < node.foregrounds.size(); ++i)
      paintMark(node.foregrounds[i], detail::MarkSlot::Foreground, i);

  // Span-qualified stroke passes, in declaration order, in the same slot
  // as the unqualified strokes they append to. Each one paints against
  // the sub-geometry it CLAIMED, so a brush that knows nothing about
  // spans (a PathFormat, a Brush, a brush::Pattern) dresses part of a
  // boundary with no new vocabulary.
  if (emitChildren) paintSpanHalf(detail::StrokePass::Half::Foreground);

  leaveGates(hoistSaves, hoistCover);

  if (hasEffect) canvas.restore();
}

namespace {

/** Promotion thresholds. A node must cost more than this to replay, for
 *  this many consecutive frames, before the library re-bakes it. 1 ms is
 *  ~6% of a 60 FPS frame — well above noise, far below the point where a
 *  sketch is in trouble; 8 frames keeps a one-off stall from promoting
 *  anything. */
constexpr double kPromoteMs = 1.0;
constexpr uint8_t kPromoteFrames = 8;

/** Temporal promotion. A node whose only volatility is a live material may
 *  hold a bake while the material is provably holding still, and re-bakes
 *  when it ticks — which is only a win if it ticks slower than the frame
 *  rate. A bake costs about what the replay it replaces costs, so the
 *  break-even stable fraction is around a half: promote at 0.5, keep until
 *  0.3. A material bound to a continuous output sits at 0 and never gets
 *  close; one quantized to a step slower than the frame rate sits well
 *  above the promote bar. */
constexpr float kStablePromote = 0.5f;
constexpr float kStableKeep = 0.3f;

/** A readable, ACTIONABLE identity for a profile row: the author's own
 *  key() when there is one (that is what they will search for), else the
 *  node kind and its painted size, which is usually enough to find it. */
std::string profileLabel(const detail::Instance& inst, const SkRect& rect) {
  const detail::ElementNode& node = *inst.desc;
  const char* kind = "box";
  switch (node.kind) {
    case detail::Kind::Box:
      kind = "box";
      break;
    case detail::Kind::Text:
      kind = "text";
      break;
    case detail::Kind::Image:
      kind = "image";
      break;
    case detail::Kind::Custom:
      kind = "custom";
      break;
    default:
      break;
  }
  char buf[96];
  std::snprintf(buf, sizeof buf, "%s %.0fx%.0f", kind, rect.width(),
                rect.height());
  if (!node.key.empty()) return node.key + " (" + buf + ")";
  return buf;
}

/** Scoped per-node timer. RAII because paint() has several early returns
 *  and a half-written row would be worse than no row at all. */
struct ProfileScope {
  Composer::Impl* impl = nullptr;
  size_t row = SIZE_MAX;
  double savedChildren = 0;
  std::chrono::steady_clock::time_point start;

  ProfileScope(Composer::Impl* i, const detail::Instance& inst,
               const SkRect& rect)
      : impl(i) {
    if (!impl->profileEnabled) return;
    row = impl->profileRows.size();
    impl->profileRows.push_back(Composer::NodeCost{profileLabel(inst, rect), 0,
                                                   0, impl->profDepth,
                                                   Composer::CacheState::Live});
    savedChildren = impl->profChildMs;
    impl->profChildMs = 0;
    ++impl->profDepth;
    start = std::chrono::steady_clock::now();
  }
  ~ProfileScope() {
    if (row == SIZE_MAX) return;
    const double total = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    impl->profileRows[row].totalMs = total;
    impl->profileRows[row].selfMs = total - impl->profChildMs;
    // Hand our whole cost up to the parent's child accumulator.
    impl->profChildMs = savedChildren + total;
    --impl->profDepth;
  }
};

}  // namespace

void Composer::Impl::paint(Instance& inst, SkCanvas& canvas) {
  const ElementNode& node = *inst.desc;
  const SkRect rect = instanceRect(inst);
  ProfileScope profileScope(this, inst, rect);

  const float opacity = std::clamp(
      inst.resolveFloat(Instance::kOpacity, node.paint.opacity), 0.0f, 1.0f);
  if (opacity <= 0.0f) return;

  // (Size-change invalidation for recordings — including geometry-dependent
  // materials' baked uResolution — happens in ensureLayout's
  // syncLayoutRects pass, which sees every relayout; paint() may never reach
  // a node whose ancestor replays a cached picture.)

  canvas.save();
  canvas.translate(rect.left(), rect.top());

  // ONE transform producer for the resolver's lanes: concatTo() is
  // matrix()'s op list applied as elementary canvas ops — byte-exactness
  // demands that sequence, see its comment — while recordBounds()'s child
  // union and hitInstance()'s inverse map and invert the composed matrix()
  // itself.
  const NodeTransform tf = transformOf(inst);
  tf.concatTo(canvas, node.paint, rect.width(), rect.height());

  // Accumulate the node→root matrix alongside the canvas ops — the same
  // T(rect)·matrix() product hitInstance() inverts, so a world-space
  // material draws its field exactly where the hit test says the node is.
  // NOT canvas.getTotalMatrix(): that includes the HOST's transform and any
  // bake-layer offset, and this matrix must stop at the composer root. RAII
  // because paint() returns from several places.
  if (!inst.parent) rootLayoutSize = SkSize{rect.width(), rect.height()};
  struct ToRootScope {
    SkMatrix* slot;
    SkMatrix saved;
    explicit ToRootScope(SkMatrix* s) : slot(s), saved(*s) {}
    ~ToRootScope() { *slot = saved; }
  } toRootScope(&curToRoot);
  curToRoot.preTranslate(rect.left(), rect.top());
  curToRoot.preConcat(
      tf.matrix({0, 0}, node.paint, rect.width(), rect.height()));

  const Effect* backdropFx = backdropEffectOf(node);
  sk_sp<SkImageFilter> backdropFilter;
  if (backdropFx) {
    // A backdrop effect's child materials resolve against the node's box
    // too — the same context the node's own paint builds, minus the marks
    // outline, because nothing of the node has been painted yet. Built
    // INSIDE the branch: every node reaches this line and only a few carry
    // a backdrop.
    const PaintContext backdropCtx{{rect.width(), rect.height()},
                                   SkPath(),
                                   elapsed(),
                                   hostScale,
                                   ticker.active(),
                                   &fonts,
                                   nullptr,
                                   &inst.stampCache,
                                   curToRoot,  // this node→root
                                   rootLayoutSize};
    backdropFilter = backdropFx->resolvedImageFilter(&backdropCtx);
  }
  const bool hasBackdrop = (bool)backdropFilter;
  if (hasBackdrop) {
    // The filtered backdrop composites as a CLOSED pass clipped to the
    // node's shape — the node's own decorations and overflowing children
    // then paint unclipped above it (CSS clips the FILTER REGION to the
    // element, not the element's overflow).
    canvas.save();
    if (node.shapeFn)
      canvas.clipPath(resolveOutline(inst, {rect.width(), rect.height()}),
                      true);
    else
      canvas.clipRRect(cornersRRect(SkRect::MakeWH(rect.width(), rect.height()),
                                    node.corners),
                       true);
    SkCanvas::SaveLayerRec rec(nullptr, nullptr, backdropFilter.get(), 0);
    canvas.saveLayer(rec);
    canvas.restore();  // composite the filtered backdrop through the clip
    canvas.restore();  // release the clip — content is NOT bounded by it
  }

  // The live-material resolve probe: when the node's only volatility is its
  // live material, resolve NOW — an unchanged shader means the cached
  // picture is still exact and simply replays, so the node repaints at the
  // material's own rate rather than the frame rate.
  bool liveStable = false;
  inst.hasPendingLiveFill = false;
  if (inst.liveMatOnly && liveMaterialOf(node)) {
    PaintContext probe{{rect.width(), rect.height()},
                       SkPath(),
                       elapsed(),
                       hostScale,
                       ticker.active(),
                       &fonts,
                       nullptr,
                       nullptr,
                       curToRoot,  // so the memo digest sees this move
                       rootLayoutSize};
    inst.pendingLiveFill = liveMaterialOf(node)->resolve(probe);
    inst.hasPendingLiveFill = true;
    liveStable = (inst.picture || inst.textureImage) && !inst.paintDirty &&
                 inst.pendingLiveFill.shaderValue == inst.bakedLiveShader;
    // The temporal-stability estimate. Material::resolve() memoizes on the
    // byte-identical digest of every varying input, so a stable shader
    // POINTER is a proof that the quantized inputs have not ticked — and
    // therefore that the pixels of the last bake are still the pixels this
    // frame wants. EMA, so one tick does not cost the promotion.
    inst.liveStableRate =
        inst.liveStableRate * 0.75f + (liveStable ? 0.25f : 0.0f);
  }

  // The scalar memo's probe: the animated content scalars AS OF THIS FRAME.
  // Same argument as the material's — identical inputs mean identical
  // pixels, so a recording made with these numbers is still exact while
  // they hold.
  Instance::ContentScalars scalarsNow;
  if (inst.scalarMemo) {
    // Every mask gate's animated numbers, as a bounded per-node list, so a
    // masked node can take this memo at all.
    scalarsNow.gates = inst.resolveGateValues();
    scalarsNow.tracks = inst.resolveTrackValues();
    // The node→root matrix as of THIS paint — curToRoot is exactly it here,
    // and the walk-side compares (release, scan) recompute it
    // bit-identically.
    if (inst.hasWorldSpaceMaterial)
      scalarsNow.world = {curToRoot.getScaleX(),     curToRoot.getSkewX(),
                          curToRoot.getTranslateX(), curToRoot.getSkewY(),
                          curToRoot.getScaleY(),     curToRoot.getTranslateY()};
    // The bound fill, through the SAME body the walk and scan call — the
    // value the recording below bakes is this frame's binding read, so the
    // memo compares exactly the Fill the recording was baked with.
    scalarsNow.fill = inst.resolveBoundFill();
    // The bound tile pan, under the same one-body rule: the recording bakes
    // the fill shader translated by exactly this read, so the memo compares
    // the pan it was baked with.
    scalarsNow.pattern = inst.resolvePatternOffset();
    // …and onPath()'s phase, on the same rule: the recording bakes the
    // glyph positions this phase produced.
    scalarsNow.pathAt = inst.resolvePathAt();
  }
  const bool scalarsStable = inst.scalarMemo && !inst.paintDirty &&
                             (inst.picture || inst.textureImage) &&
                             scalarsNow == inst.bakedScalars;
  // The settle warmup, write side: count consecutive stable paints, and on
  // crossing the bar request ONE volatility recompute — that walk performs
  // the actual release and registers the node for the per-draw movement
  // scan. Any instability resets the warmup, so a binding that is genuinely
  // moving pays nothing for this machinery beyond the compare.
  if (inst.scalarMemo) {
    if (scalarsStable) {
      inst.settledScalars = scalarsNow;
      if (inst.settleFrames < Instance::kScalarSettleFrames &&
          ++inst.settleFrames == Instance::kScalarSettleFrames)
        volatileDirty = true;
    } else {
      inst.settleFrames = 0;
    }
  }
  // "May this node keep its cached pixels?" — either nothing about it is
  // volatile, or every input it reads is memoized and provably unchanged.
  const bool memoized = inst.liveMatOnly || inst.scalarMemo;
  const bool cacheHolds = !inst.subtreeVolatile || memoized;
  // …and "are they still the RIGHT pixels?" — the two memos answer for
  // their own input and abstain on the other.
  const bool memoStale =
      (inst.liveMatOnly && !liveStable) || (inst.scalarMemo && !scalarsStable);

  // Fill-only leaves route blend/opacity straight onto the fill paint instead
  // of a (device-clip-sized!) saveLayer — a field of plus-blended shapes costs
  // path draws, not full-canvas layers. Excluded: live opacity (must stay
  // outside any cached recording) and texture bakes (blending must hit the
  // real destination, not the bake's transparent surface).
  const bool opacityLive =
      node.paint.opacity.binding() != nullptr ||
      (inst.anims[Instance::kOpacity] &&
       inst.anims[Instance::kOpacity]->value.isConnected());
  const bool leafDirectBlend =
      (node.kind == Kind::Box || node.kind == Kind::Stack) &&
      inst.children.empty() && node.backgrounds.empty() &&
      node.foregrounds.empty() && !node.hasStrokePasses() &&
      (!node.fxData ||
       (node.fxData->overlays.empty() && node.fxData->masks.empty())) &&
      !layerEffectOf(node) && !backdropEffectOf(node) && !node.clipContent &&
      !opacityLive && node.cacheMode != Cache::Texture &&
      node.cacheMode != Cache::Group;  // (same reason: bakes isolate)
  // A texture-cached node composites exactly ONE draw — its blit — so its
  // blend and opacity can ride that draw's paint instead of a
  // device-clip-sized saveLayer. Cheaper, and slightly more exact: no
  // full-canvas intermediate, and one fewer 8-bit requantisation.
  //
  // The predicate is EXACT by construction: it is the texture branch's own
  // entry condition (the memo probes above are hoisted so cacheHolds is
  // known here), and every exit of that branch ends in a single image draw
  // — the device blit, or the quantized-local blit it falls back to. A node
  // that fails the entry keeps the layer, so nothing can lose its blend.
  const bool deferBlendToBlit =
      (opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver) &&
      !leafDirectBlend && !liveOnly && cacheHolds &&
      node.cacheMode == Cache::Texture && !backdropEffectOf(node);
  const bool needsLayer =
      (opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver) &&
      !leafDirectBlend && !deferBlendToBlit;
  if (needsLayer) {
    SkPaint layerPaint;
    layerPaint.setAlphaf(opacity);
    layerPaint.setBlendMode(node.paint.blendMode);
    // BOUNDED like the effect layer: nullptr would allocate a clip-sized
    // (often full-canvas) layer for every fading container, so an entrance
    // opacity ramp would cost a fullscreen composite per animated group.
    const SkRect content = recordBounds(inst);
    canvas.saveLayer(&content, &layerPaint);
  }
  const SkBlendMode leafBlend =
      leafDirectBlend ? node.paint.blendMode : SkBlendMode::kSrcOver;
  const float leafOpacity = leafDirectBlend ? opacity : 1.0f;

  // Automatic caching at topmost provably-static subtrees: pictures by
  // default, a rasterized image under Cache::Texture (the raster-target pixel
  // win — replaying a picture re-rasterizes, blitting doesn't).
  // COMPOSE_PROF=<ms> prints any draw above the threshold — cached-texture
  // blits, picture replays (which re-EXECUTE recorded ops on raster), and
  // live paints. Nested lines overlap (inclusive of children); any
  // unparsable value means 4ms.
  static const double kProfMs = [] {
    const char* env = getenv("COMPOSE_PROF");
    if (!env) return -1.0;
    const double v = atof(env);
    return v > 0.0 ? v : 4.0;
  }();
  const auto profDraw = [&](const char* what, auto&& draw) {
    if (kProfMs < 0.0) {
      draw();
      return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    draw();
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    if (ms > kProfMs)
      SkDebugf("[prof] %s %s kind=%d rect=%.0fx%.0f %.1fms\n", what,
               node.key.empty() ? "(anon)" : node.key.c_str(), (int)node.kind,
               rect.width(), rect.height(), ms);
  };

  // ---- automatic texture promotion -----------------------------------
  // Eligibility is deliberately narrow. Everything here is a condition
  // under which a device-aligned bake is provably the same pixels as the
  // replay; anything else keeps replaying. See
  // Composer::setAutoTexturePromotion.
  const SkMatrix& totalM = canvas.getTotalMatrix();
  // Upright, unmirrored, unrotated and unskewed. It is tempting to drop
  // this: a device-space bake concatenates the full matrix into the layer
  // and blits with the matrix reset at an integer offset, so it cannot
  // resample and ought to be exact at any angle. It is not, for two
  // separate reasons, and automatic promotion is held to exact agreement
  // because the author never asked for it.
  //
  //  - A shader's local coordinates come from INVERTING the CTM, and the
  //    layer's CTM differs from the canvas's by an integer device
  //    translation. Inverting a rotation maps that integer offset through
  //    irrational entries, so the cancellation is only approximate, while
  //    an axis-aligned matrix maps it through ±1 and 0 and cancels exactly.
  //    Off-axis, shaded pixels land about one least-significant bit away.
  //  - A bake rect larger than the device clip hands Skia a different clip
  //    to rasterize the antialiased edges against. That one is worth many
  //    levels, not one, and it shows up wherever a rotated node's bounds
  //    overflow the canvas.
  //
  // A test that rotates a plain colour fill small enough to fit exercises
  // neither effect and will pass with this gate removed. `Cache::Texture`
  // is opt-in and does accept the trade, which is why the refusal message
  // points an author there rather than describing the geometry — a node
  // held off promotion by a constant fraction-of-a-degree tilt needs to be
  // told what to do about it.
  const bool upright = totalM.getSkewX() == 0 && totalM.getSkewY() == 0 &&
                       totalM.getScaleX() > 0 && totalM.getScaleY() > 0 &&
                       !totalM.hasPerspective();
  // recordBounds() walks the whole subtree, and three tiers below ask for
  // it. Memoised per paint() so the walk happens at most once; lazy so a
  // node that reaches none of them never pays for it at all.
  SkRect localPaintBounds = SkRect::MakeEmpty();
  bool localBoundsDone = false;
  const auto localBoundsOf = [&]() -> const SkRect& {
    if (!localBoundsDone) {
      localBoundsDone = true;
      localPaintBounds = recordBounds(inst);
    }
    return localPaintBounds;
  };
  const auto deviceRectOf = [&] {
    const SkRect f = totalM.mapRect(localBoundsOf());
    return SkIRect::MakeLTRB(
        (int)std::floor(f.left()), (int)std::floor(f.top()),
        (int)std::ceil(f.right()), (int)std::ceil(f.bottom()));
  };
  // The temporal rule: a node whose ONLY volatility is a live material is
  // promotable while that material is provably holding still, and re-bakes
  // when it ticks. Sticky, with hysteresis, so a material sitting near the
  // threshold does not promote and demote on alternate frames.
  const bool temporallyStable =
      inst.liveMatOnly &&
      inst.liveStableRate >= (inst.autoTexture ? kStableKeep : kStablePromote);
  const bool contentStable = !inst.subtreeVolatile || temporallyStable;

  // Every refusal below is a condition under which a bake would produce
  // DIFFERENT PIXELS, or would not pay for itself. Naming them is not
  // decoration: a node reported as expensive live paint with no reason
  // beside it gives an author nothing to act on.
  //
  // A BIT MASK, so ALL refusals are reported, not the first. A first-match
  // chain would report only `Volatile` for a node that is both volatile and
  // clipped, and an author who fixed the volatility would then meet a
  // second refusal nobody had mentioned. `why` below is derived FROM this
  // mask rather than computed alongside it, so the summary and the full set
  // cannot disagree.
  using Prom = Composer::Promotion;
  uint16_t refusals = 0;
  const auto flag = [&](Prom p) { refusals |= (uint16_t)(1u << (unsigned)p); };
  // autoPromoteEffective, not autoPromote: the backend-aware default (off on
  // GPU unless the host asked) is applied in draw(). See ComposeRuntime.h.
  const bool optedOut = !autoPromoteEffective || node.cacheMode != Cache::Auto;
  if (optedOut) flag(Prom::OptedOut);
  if (!contentStable) flag(Prom::Volatile);
  if (leafBlend != SkBlendMode::kSrcOver || leafOpacity < 1.0f)
    flag(Prom::Composited);
  if (layerEffectOf(node) || node.clipContent) flag(Prom::Filtered);
  if (inst.subtreeReadsBackdrop)  // incl. this node's own backdrop()
    flag(Prom::ReadsBackdrop);
  if (rect.width() < 0.5f || rect.height() < 0.5f)
    flag(Prom::TooBig);  // degenerate, not large — same "cannot bake" bucket
  if (!upright) flag(Prom::Transformed);

  // The PRIMARY verdict: the first refusal in the order an author should
  // address them (their own switches first, then content, then geometry).
  static constexpr Prom kRefusalOrder[] = {
      Prom::OptedOut, Prom::Volatile,      Prom::Composited, Prom::Transformed,
      Prom::Filtered, Prom::ReadsBackdrop, Prom::TooBig};
  Prom why = Prom::Cheap;
  for (Prom p : kRefusalOrder)
    if (refusals & (uint16_t)(1u << (unsigned)p)) {
      why = p;
      break;
    }

  // recordingDepth == 0, for the SAME reason the Cache::Texture device path
  // and the split bake check it: a device-space bake blits with
  // canvas.resetMatrix() + drawImage() at an ABSOLUTE device rect, and a
  // picture can be replayed under a different matrix than it was recorded
  // at. Recorded into an ancestor's picture and replayed at a different
  // capture scale, such a blit draws a texture baked for one scale at the
  // coordinates of another — wrong size, wrong place.
  const bool promotable =
      why == Prom::Cheap && !liveOnly && recordingDepth == 0;
  if (!promotable) inst.autoTexture = false;
  const auto note = [&](Prom p) {
    if (profileScope.row != SIZE_MAX) {
      profileRows[profileScope.row].promotion = p;
      profileRows[profileScope.row].refusals = refusals;
    }
  };
  note(why);

  /** What this node cost to paint the way it painted — a picture replay for
   *  a cached subtree, the live draw for a leaf — folded into the rolling
   *  estimate, and the promotion decision taken from it. */
  const auto accrue = [&](double cost) {
    // EMA so one scheduling hiccup neither promotes nor un-promotes.
    inst.replayMs = inst.replayMs * 0.6f + (float)cost * 0.4f;
    if (promotable && inst.replayMs > kPromoteMs) {
      if (inst.hotFrames < 255) ++inst.hotFrames;
      if (inst.hotFrames >= kPromoteFrames) {
        inst.autoTexture = true;
        inst.paintDirty = true;  // force the first bake
      } else {
        note(Prom::Warming);
      }
    } else if (inst.hotFrames > 0) {
      --inst.hotFrames;
    }
  };

  if (promotable && inst.autoTexture) {
    // Bake in DEVICE space, snapped OUT to whole device pixels, then blit
    // with the matrix reset. An integer device translation cannot change
    // rasterisation for an AXIS-ALIGNED matrix — which is what `upright`
    // above is guarding, and why that guard is about exactness rather than
    // about resampling.
    const SkIRect device = deviceRectOf();
    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
    // The bake this frame would ADD to what the previous frame already held.
    // A node keeping a bake it already has is never refused for budget —
    // dropping it would only make the next frame re-bake it.
    // max(), not a sum: promotedBytesLast is the previous frame's FULL
    // total and promotedBytes is this frame so far, and the two overlap.
    const bool affordable =
        inst.textureImage ||
        std::max(promotedBytesLast, promotedBytes) + bytes <= kPromotedBudget;
    if (device.width() > 0 && device.height() > 0 && area <= 16 * 1024 * 1024 &&
        affordable) {
      // Re-bake when the recording is stale, when the device rect moved or
      // resized (which is how a transform-SCALE change arrives here), or —
      // the temporal case — when the live material has actually ticked and
      // the baked shader is no longer the one this frame resolves to.
      if (!inst.textureImage || inst.paintDirty || memoStale ||
          inst.textureBakeRect != SkRect::Make(device)) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          paintContent(inst, *lc, hostScale, leafBlend, leafOpacity);
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = SkRect::Make(device);
          inst.bakedLiveShader = inst.hasPendingLiveFill
                                     ? inst.pendingLiveFill.shaderValue
                                     : nullptr;
          inst.bakedScalars = scalarsNow;
          inst.paintDirty = false;
          stats.picturesRecorded++;
          stats.texturesBaked++;
        }
      }
      if (inst.textureImage) {
        promotedBytes += bytes;
        if (profileScope.row != SIZE_MAX)
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::Promoted;
        note(Prom::Promoted);
        canvas.save();
        canvas.resetMatrix();
        canvas.drawImage(inst.textureImage, (float)device.left(),
                         (float)device.top(), SkSamplingOptions());
        canvas.restore();
        if (needsLayer) canvas.restore();
        canvas.restore();
        return;
      }
    }
    inst.autoTexture = false;  // could not bake — fall through to the picture
    note(Prom::TooBig);
  }

  // ---- the SPLIT bake -----------------------------------------------------
  //
  // Volatility is declared per NODE, and a node gets one verdict. So a
  // static full-canvas ground plane carrying one small child on a bound
  // output is `subtreeVolatile`, nothing about it is cached, and the whole
  // plane is re-rasterized every frame purely so the child can be redrawn
  // on top of it. The node reports "its content changes every frame" when
  // what changes is a child's.
  //
  // THE PIXEL-IDENTITY ARGUMENT, which is NOT promotion's argument.
  // Promotion bakes a whole subtree and blits it in place of everything the
  // node contains; the claim there is "an integer device translation cannot
  // change rasterisation". Here the bake replaces only PART of what the
  // node paints and the children are drawn over the blit afterwards, so the
  // claim needed is:
  //
  //   painting the own layer into a transparent device-aligned surface,
  //   blitting it, then painting the children over the result must produce
  //   the same pixels as painting own-then-children directly.
  //
  //  - The own paint is a run of srcOver draws into a transparent layer,
  //    blitted srcOver at an integer device offset. srcOver is associative,
  //    so the composite is the same composite. (The 8-bit double-rounding
  //    of the intermediate is the one real risk and it is asserted, not
  //    argued — see SplitBakeIsPixelIdenticalAcrossTheChildsMotion, whose
  //    own paint deliberately OVERLAPS itself so intra-layer compositing
  //    actually happens.)
  //  - A child with a non-srcOver blend is FINE here, and this is the one
  //    place the split is safer than promotion: the blit lands BEFORE the
  //    children, so the child resolves against the same destination bytes
  //    either way. Under promotion the child was inside the bake and would
  //    have resolved against transparent black. Same for a child with a
  //    backdrop filter — it samples a blitted copy of identical pixels.
  //  - The real failure is the node's OWN paint reading the backdrop, where
  //    the bake would resolve against transparent black. That is
  //    `ownReadsBackdrop`, which is why the flag was split from
  //    `subtreeReadsBackdrop` before any of this existed.
  //  - A LAYER EFFECT is the exclusion, and it is the only one of the three
  //    wrappers that is. An image filter applies to the UNION of own paint
  //    and children; filtering the own half alone and drawing the children
  //    over the result is a different picture.
  //  - clipContent and a whole-node mask() are NOT excluded, though it
  //    looks as though they should be. They wrap both halves, and the phase
  //    flag skips only the CONTENT — the clip is opened and closed inside
  //    EACH phase, so both halves get the identical clip in the identical
  //    device geometry and the composition is unchanged. A GRANULAR mask is
  //    narrower still: its scope is entered and left around one paint
  //    group, inside the half that group belongs to. Excluding clips would
  //    refuse the most common shape this feature exists for, since a
  //    backdrop that clips its moving child to an outline is exactly why
  //    that child is a separate node.
  //
  // And the promotion is judged on the OWN paint alone. A split candidate
  // paints in two phases from the first eligible frame precisely so that
  // half can be timed by itself: judging it by the node's total would
  // promote a cheap ground plane because it carries an expensive child, and
  // would leave an expensive plane unpromoted under a cheap one.
  const bool splitCandidate =
      !optedOut && !liveOnly && inst.subtreeVolatile &&
      !inst.ownContentVolatile &&  // the CHILDREN are what block this node
      !inst.children.empty() && !inst.ownReadsBackdrop &&
      !layerEffectOf(node) && leafBlend == SkBlendMode::kSrcOver &&
      leafOpacity >= 1.0f && rect.width() >= 0.5f && rect.height() >= 0.5f &&
      recordingDepth == 0 && !inst.transformLive &&
      // `upright` for the same reason promotion needs it, and it is the
      // SAME construction: an integer device offset concatenated onto the
      // node's matrix. Under rotation a shader's local coordinates come
      // back through an inverse that cannot cancel that offset exactly, and
      // the antialiased edges land about a least-significant bit apart.
      // Leaving it out would hold the split to a weaker standard than the
      // promoter beside it.
      upright;
  if (!splitCandidate) inst.splitBake = false;
  if (splitCandidate) {
    // ownPaintBounds, NOT recordBounds. recordBounds unions the children
    // in, so it moves every frame a child moves — and a bake rect that
    // moves every frame is a bake remade every frame, which is the one
    // failure mode that would make this feature cost more than it saves on
    // precisely the scenes it exists for. The own paint's extent does not
    // depend on the children at all.
    const SkRect ownF = totalM.mapRect(ownPaintBounds(inst));
    const SkIRect device = SkIRect::MakeLTRB(
        (int)std::floor(ownF.left()), (int)std::floor(ownF.top()),
        (int)std::ceil(ownF.right()), (int)std::ceil(ownF.bottom()));
    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
    const bool affordable =
        inst.ownImage ||
        std::max(promotedBytesLast, promotedBytes) + bytes <= kPromotedBudget;
    bool blitted = false;
    if (inst.splitBake && device.width() > 0 && device.height() > 0 &&
        area <= 16 * 1024 * 1024 && affordable) {
      // `ownPaintDirty`, NOT `paintDirty`. markPaintDirtyUp() propagates a
      // descendant's patch to every ancestor, which is right for a
      // recording (it baked the child's draw calls) and wrong here: the
      // children were never in this bake, and the whole point is that they
      // change. If that ever inverts, the feature silently does nothing and
      // still passes every pixel test.
      const SkRect want = SkRect::Make(device);
      if (!inst.ownImage || inst.ownPaintDirty || inst.ownBakeRect != want) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          paintContent(inst, *lc, hostScale, leafBlend, leafOpacity,
                       Phase::OwnOnly);
          inst.ownImage = layer->makeImageSnapshot();
          inst.ownBakeRect = want;
          inst.ownPaintDirty = false;
          stats.texturesBaked++;
          // A bake per frame costs MORE than the live draw it replaced, so
          // a node whose own paint really is being invalidated every frame
          // must not hold the promotion on the strength of a measurement
          // taken while it was still cheap. Three consecutive re-bakes and
          // it goes live and has to earn it again over the full warmup.
          if (inst.ownRebakes < 255) ++inst.ownRebakes;
          if (inst.ownRebakes > 3) {
            inst.splitBake = false;
            inst.ownHotFrames = 0;
            inst.ownRebakes = 0;
          }
        }
      } else {
        inst.ownRebakes = 0;
      }
      if (inst.ownImage && inst.splitBake) {
        promotedBytes += bytes;
        if (profileScope.row != SIZE_MAX)
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::SplitOwn;
        note(Prom::SplitBaked);
        canvas.save();
        canvas.resetMatrix();
        profDraw("split blit", [&] {
          canvas.drawImage(inst.ownImage, (float)device.left(),
                           (float)device.top(), SkSamplingOptions());
        });
        canvas.restore();
        blitted = true;
      }
    }
    if (!blitted) {
      // The own half, live and TIMED. This is the number the split is
      // promoted on — the node's own paint, with its children excluded by
      // construction rather than by subtraction.
      const auto ownStart = std::chrono::steady_clock::now();
      profDraw("live own", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity,
                     Phase::OwnOnly);
      });
      const double ownMs = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - ownStart)
                               .count();
      inst.ownPaintMs = inst.ownPaintMs * 0.6f + (float)ownMs * 0.4f;
      if (inst.ownPaintMs > kPromoteMs) {
        if (inst.ownHotFrames < 255) ++inst.ownHotFrames;
        if (inst.ownHotFrames >= kPromoteFrames) {
          inst.splitBake = true;
          inst.ownPaintDirty = true;  // force the first bake
        } else {
          note(Prom::Warming);
        }
      } else if (inst.ownHotFrames > 0) {
        --inst.ownHotFrames;
      }
    }
    // The children and the foregrounds over them — always live, whatever
    // happened above. Foregrounds paint AFTER the children, so they are in
    // this half and never in the bake.
    paintContent(inst, canvas, hostScale, leafBlend, leafOpacity,
                 Phase::ChildrenOnly);
    stats.nodesPainted++;
    inst.paintDirty = false;
    if (needsLayer) canvas.restore();
    canvas.restore();
    return;
  }

  // ---- Cache::Group — the whole subtree, held by a VALUE memo -------------
  //
  // The shape of the problem this exists for: MANY SMALL ROTATED PIECES
  // FORMING ONE STATIC ASSEMBLY, each piece carrying a bound entrance that
  // runs for a while and then holds. Nothing in that description is
  // cacheable by the volatility rule, because the bindings never
  // disconnect, and everything in it is cacheable for every frame the
  // entrance is not running.
  //
  // WHY THE BAKE IS THE EASY HALF. This is the same construction the device
  // path below and whole-subtree promotion already use: paintContent into a
  // transparent layer whose canvas carries the node's exact matrix offset by
  // an INTEGER device translation, then blit with the matrix reset. The
  // children's rotations, their bevels and their mutual compositing all
  // happen INSIDE that bake at full precision, which is what makes it
  // pixel-safe where per-piece Cache::Texture is not: baking each piece
  // separately isolates it into its own layer, so every shared edge and
  // abutment resolves against transparent black instead of against its
  // neighbour.
  //
  // WHY THE INVALIDATION IS THE HARD HALF, AND THE WHOLE FEATURE. A group
  // may hold a bake only while it is provably not changing, and "not
  // changing" cannot be read off the volatility verdict — that verdict says
  // Volatile forever, correctly. So the group compares VALUES, the same way
  // the per-node scalar memo does, generalised to a whole subtree's bound
  // transforms and opacities. Every frame: gather them, compare with last
  // frame's, and on any difference at all DROP THE BAKE and paint live. A
  // bake taken while the entrance is running would freeze the entrance, and
  // would look completely correct in any still frame.
  //
  // The refusals are in computeVolatile (`groupRootOK`), because they are
  // about what the memo can SEE, not about this frame.
  if (!liveOnly && inst.groupRootOK && recordingDepth == 0) {
    // Gather, compare, and become last frame — in that order. The swap is
    // what makes a settled group allocate nothing: `groupScratch` comes back
    // holding the vector that was `groupPrev`, at the right capacity.
    groupScratch.clear();
    collectGroupScalars(inst, /*root=*/true, groupScratch);
    const bool settled = inst.groupPrevSeen && groupScratch == inst.groupPrev;
    std::swap(inst.groupPrev, groupScratch);
    inst.groupPrevSeen = true;

    // The device rect, and the two "is it holding still" questions the
    // device path below asks for its own reasons — they are the same
    // questions here. `transformLive` is the node's own declared motion; the
    // rect comparison catches the motions no declaration can see (a resizing
    // host, a pinch zoom, an uncached ancestor's live transform). A bake
    // pinned to a rect that moves is a bake remade every frame, which costs
    // strictly more than the paint it replaces.
    // THE BAKE RECT IS CLIPPED TO THE CANVAS, and this is not an
    // optimisation — it is a correctness condition. A bake rect LARGER than
    // the device clip hands Skia a different clip to rasterize antialiased
    // edges against, and the resulting difference is many levels deep, not
    // the single least-significant bit an integer offset under rotation
    // costs. A lattice of rotated pieces with any bleed overruns its own
    // canvas on all four sides, so this fires on exactly the content the
    // feature exists for.
    //
    // Nothing visible is lost — content outside the device clip does not
    // reach the canvas either way — and `getDeviceClipBounds()` is in base
    // device coordinates, the same space the blit's resetMatrix() draws in,
    // including inside the saveLayer an opacity/blend group opens.
    SkIRect device = deviceRectOf();
    const SkIRect clip = canvas.getDeviceClipBounds();
    if (!device.intersect(clip)) device = SkIRect::MakeEmpty();
    const bool rectStable =
        !inst.deviceRectSeen || device == inst.lastDeviceRect;
    inst.lastDeviceRect = device;
    inst.deviceRectSeen = true;

    // THE DROP. Not "re-bake": a group whose bindings are ticking is
    // ticking for a while, and re-baking each of those frames would pay the
    // bake on top of the paint. Hold the pixels only while they are right.
    if (!settled || inst.paintDirty) inst.textureImage.reset();

    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
    const bool affordable =
        inst.textureImage ||
        std::max(promotedBytesLast, promotedBytes) + bytes <= kPromotedBudget;
    if (settled && !inst.paintDirty && !inst.transformLive && rectStable &&
        !totalM.hasPerspective() && device.width() > 0 && device.height() > 0 &&
        area <= 16 * 1024 * 1024 && affordable) {
      const SkRect want = SkRect::Make(device);
      if (!inst.textureImage || !inst.textureDeviceSpace ||
          inst.textureBakeRect != want) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          // No leaf blend and no leaf opacity: bakes isolate, and the node's
          // own blend/opacity are applied by the saveLayer wrapping the blit
          // — which is why leafDirectBlend excludes Cache::Group.
          paintContent(inst, *lc, hostScale);
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = want;
          inst.textureScale = maxScaleOf(totalM, localBoundsOf());
          inst.paintDirty = false;
          // A group root never replays a recording. It can have made one on
          // its very first frame — before it had a previous frame to compare
          // with, a group with a fully static subtree falls through to the
          // picture branch once — and holding it after that is bytes nobody
          // will ever read.
          inst.picture.reset();
          stats.picturesRecorded++;
          stats.texturesBaked++;
        }
      }
      if (inst.textureImage) {
        promotedBytes += bytes;
        if (profileScope.row != SIZE_MAX) {
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::Group;
          profileRows[profileScope.row].promotion =
              Composer::Promotion::AskedFor;
        }
        canvas.save();
        canvas.resetMatrix();
        profDraw("group blit", [&] {
          canvas.drawImage(inst.textureImage, (float)device.left(),
                           (float)device.top(), SkSamplingOptions());
        });
        canvas.restore();
        if (needsLayer) canvas.restore();
        canvas.restore();
        return;
      }
    }
    // Falls through: `cacheHolds` is false for a volatile group root, so the
    // picture branch below cannot take it either and the node paints LIVE.
    // That is the intended outcome on a ticking frame — the same paint the
    // scene did before this feature existed.
  }

  if (!liveOnly && cacheHolds && node.cacheMode == Cache::Texture &&
      !backdropEffectOf(node)) {
    // ---- the exact bake -------------------------------------------------
    // A bake held in LOCAL space and blitted through the node's transform
    // is resampled by whatever that transform is: at a quarter turn the
    // texel grid lands half a texel off the device grid and every sample
    // interpolates two texels, which softens edges and flattens gradients
    // across the axis whose device edge falls on a half pixel. Baking at
    // the correct scale is necessary for sharpness but not sufficient.
    //
    // Baking in DEVICE space, snapped OUT to whole device pixels and
    // blitted with the matrix reset, has nothing left to resample: the
    // texel grid IS the device grid, at any angle. Two conditions gate it,
    // and both are about not throwing away what the local bake is FOR:
    //
    //  - bakeScale must be 1. Its whole purpose is to rasterize BELOW
    //    device resolution and let the blit stretch it back.
    //  - we must not be inside a picture recording, because a device rect
    //    is not matrix-independent and a picture can replay elsewhere.
    //    This condition is also what makes the next one SOUND: every node
    //    that reaches the device path is painted every frame, so it has
    //    the history the next condition reads. A node painted once, into
    //    an ancestor's recording, is excluded before we get there.
    //  - the node must be HOLDING STILL, by both available measures, which
    //    are not the same measure:
    //      * `transformLive` — its own transform is declared as animating.
    //        A spinning ornament must keep the local bake and ride it,
    //        even on a frame where it happens to land on the same rect.
    //      * the device rect it lands on has not moved since last frame.
    //        A node with no animated property of its own still moves under
    //        a resizing window, a pinch zoom, a pan, or an uncached
    //        ancestor's live transform — none of which any per-node
    //        DECLARATION can see, and all of which would re-bake a
    //        device-pinned texture every frame.
    //    While either says "moving", the quantized local bake is correct
    //    and cheap: one bake per coarse scale step, reused across the rest.
    const SkRect localBounds = localBoundsOf();
    bool deviceRectStable = false;
    SkIRect deviceR = SkIRect::MakeEmpty();
    if (recordingDepth == 0) {
      deviceR = deviceRectOf();
      deviceRectStable = !inst.deviceRectSeen || deviceR == inst.lastDeviceRect;
      inst.lastDeviceRect = deviceR;
      inst.deviceRectSeen = true;
    }
    const int64_t deviceArea = (int64_t)deviceR.width() * deviceR.height();
    if (!inst.transformLive && deviceRectStable && recordingDepth == 0 &&
        node.bakeScale >= 1.0f && !totalM.hasPerspective() &&
        deviceR.width() > 0 && deviceR.height() > 0 &&
        deviceArea <= 16 * 1024 * 1024) {
      const SkRect bakeRect = SkRect::Make(deviceR);
      if (!inst.textureImage || inst.paintDirty || !inst.textureDeviceSpace ||
          memoStale || inst.textureBakeRect != bakeRect) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(deviceR.width(), deviceR.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(deviceR.width(), deviceR.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          lc->translate(-(float)deviceR.left(), -(float)deviceR.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          paintContent(inst, *lc, hostScale);  // no leaf blend: bakes isolate
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = bakeRect;
          inst.textureScale = maxScaleOf(totalM, localBounds);
          inst.bakedLiveShader = inst.hasPendingLiveFill
                                     ? inst.pendingLiveFill.shaderValue
                                     : nullptr;
          inst.bakedScalars = scalarsNow;
          inst.paintDirty = false;
          stats.picturesRecorded++;
          stats.texturesBaked++;
        }
      }
      if (inst.textureImage && inst.textureDeviceSpace) {
        if (profileScope.row != SIZE_MAX) {
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::Texture;
          profileRows[profileScope.row].promotion =
              Composer::Promotion::AskedFor;
        }
        // Identity CTM is global canvas space even inside a saveLayer (the
        // layer device carries its own origin), so an opacity/blend bake
        // still composites through the layer above.
        canvas.save();
        canvas.resetMatrix();
        profDraw("blit", [&] {
          if (deferBlendToBlit) {
            // The node's blend and opacity on the ONE draw it composites
            // as — cheaper and slightly MORE exact than the layer it
            // replaces: no full-canvas intermediate, one less rounding.
            SkPaint blit;
            blit.setAlphaf(opacity);
            blit.setBlendMode(node.paint.blendMode);
            canvas.drawImage(inst.textureImage, (float)deviceR.left(),
                             (float)deviceR.top(), SkSamplingOptions(), &blit);
          } else {
            canvas.drawImage(inst.textureImage, (float)deviceR.left(),
                             (float)deviceR.top(), SkSamplingOptions());
          }
        });
        canvas.restore();
        if (needsLayer) canvas.restore();
        canvas.restore();
        return;
      }
    }
    // Rasterize at the canvas's current scale so zoomed hosts stay crisp — but
    // quantized UP to a coarse step, so a continuously changing scale (window
    // resize, pinch zoom) reuses one bake per step instead of re-rasterizing
    // every frame. Between steps the draw minifies slightly, which stays sharp.
    SkMatrix total = canvas.getTotalMatrix();
    // maxScaleOf, NOT the matrix diagonal: a quarter-turned node's diagonal
    // is (0, 0) and would clamp to the 0.25 floor, baking at a quarter
    // resolution to be upscaled by the blit (see maxScaleOf in
    // ComposeRuntime.h). The node's local bounds locate the Jacobian
    // samples when the CTM carries a host perspective. This ladder feeds
    // the re-bake test below, so an underestimate here means a stale,
    // blurry bake rather than a wasted one.
    const float raw = std::clamp(maxScaleOf(total, localBounds), 0.25f, 4.0f);
    static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                           1.5f,  2.0f, 3.0f,  4.0f};
    float scale = kBakeSteps[std::size(kBakeSteps) - 1];
    for (float step : kBakeSteps)
      if (step >= raw) {
        scale = step;
        break;
      }
    // bakeScale(): opt-in reduced raster scale — the bake evaluates fewer
    // pixels and the blit below linear-upscales through the same dst rect.
    scale = std::max(0.1f, scale * node.bakeScale);
    // Bake the full PAINT bounds, not just the box — decoration bleed and
    // overflowing children truncate otherwise (same rule as the picture
    // cull).
    const SkRect bake = localBounds;
    if (!inst.textureImage || inst.paintDirty || inst.textureScale != scale ||
        inst.textureDeviceSpace || memoStale || inst.textureBakeRect != bake) {
      const int pw = std::max(1, (int)std::ceil(bake.width() * scale));
      const int ph = std::max(1, (int)std::ceil(bake.height() * scale));
      sk_sp<SkSurface> layer =
          canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
      if (!layer)
        layer = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
      layer->getCanvas()->scale(scale, scale);
      layer->getCanvas()->translate(-bake.left(), -bake.top());
      paintContent(inst, *layer->getCanvas(), scale);  // no leaf blend:
      inst.textureImage = layer->makeImageSnapshot();  // bakes isolate
      inst.textureScale = scale;
      inst.textureDeviceSpace = false;
      inst.textureBakeRect = bake;
      inst.bakedLiveShader =
          inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue : nullptr;
      inst.bakedScalars = scalarsNow;
      inst.paintDirty = false;
      stats.picturesRecorded++;
      stats.texturesBaked++;
    }
    if (profileScope.row != SIZE_MAX) {
      profileRows[profileScope.row].cacheState = Composer::CacheState::Texture;
      profileRows[profileScope.row].promotion = Composer::Promotion::AskedFor;
    }
    // Blit through the rect the bake ACTUALLY covers, not `bake`: pw/ph were
    // rounded UP, so stretching an image of ceil(w·s) texels across w local
    // units resamples the whole node by up to one texel's worth of scale.
    // The overshoot is transparent padding, so nothing new becomes visible.
    const SkRect dst = SkRect::MakeXYWH(
        bake.left(), bake.top(),
        (float)inst.textureImage->width() / inst.textureScale,
        (float)inst.textureImage->height() / inst.textureScale);
    profDraw("blit", [&] {
      if (deferBlendToBlit) {
        SkPaint blit;  // same rule as the device blit above
        blit.setAlphaf(opacity);
        blit.setBlendMode(node.paint.blendMode);
        canvas.drawImageRect(inst.textureImage, dst,
                             SkSamplingOptions(SkFilterMode::kLinear), &blit);
      } else {
        canvas.drawImageRect(inst.textureImage, dst,
                             SkSamplingOptions(SkFilterMode::kLinear));
      }
    });
  } else if (!liveOnly && cacheHolds && node.cacheMode != Cache::None &&
             // A zero-sized node (auto-height layout() containers, spacer
             // shims) must NOT record. NOT because an empty cull rect
             // rejects ops — it does not, see the note on ownPaintBounds —
             // but because the recording is pure overhead and it opens a
             // recordingDepth scope around the subtree, which is an input
             // to promotion. Control: delete this size
             // test and ComposeCache.{PromotionRefusesASubtreeThatBlends
             // WithTheCanvas, TheBlendingChildIsWhatCausesTheRefusal,
             // PromotionRefusesABackdropFilter} all fail. Painted live
             // instead — its children keep their own per-node caches, so
             // the cost is one traversal shim.
             rect.width() >= 0.5f && rect.height() >= 0.5f &&
             (node.cacheMode == Cache::Picture || !inst.children.empty() ||
              node.kind == Kind::Text || node.kind == Kind::Custom ||
              !node.backgrounds.empty() || !node.foregrounds.empty() ||
              node.hasStrokePasses() ||
              (node.fxData && !node.fxData->overlays.empty()) ||
              layerEffectOf(node) || memoized)) {
    // (liveMatOnly bare boxes DO record — the memo's point is replaying
    // the rasterized shader while resolve() stays stable.)
    // (Childless Image leaves deliberately absent: one drawImageRect is
    // cheaper than a nested picture indirection — tile maps stay flat inside
    // their chunk's recording. Cache::Picture opts back in.)
    if (!inst.picture || inst.paintDirty || memoStale ||
        inst.bakedLeafOpacity != leafOpacity ||
        inst.bakedLeafBlend != leafBlend) {
      // The same rect the layers and bakes use. Its job HERE is only to be
      // an honest bounds advertisement (SkPicture::cullRect) — this path
      // attaches no BBH, so nothing is culled against it either at record
      // or at playback; see the note on ownPaintBounds for the measurement.
      const SkRect cull = recordBounds(inst);
      SkPictureRecorder recorder;
      SkCanvas* rec = recorder.beginRecording(cull);
      // A picture can be replayed under a DIFFERENT matrix than it was
      // recorded at (an ancestor with a live transform keeps its picture
      // and replays it under the motion). Anything inside must therefore
      // be matrix-independent — which a device-space bake, snapped to one
      // particular device rect, is not.
      ++recordingDepth;
      paintContent(inst, *rec, hostScale, leafBlend, leafOpacity);
      --recordingDepth;
      inst.picture = recorder.finishRecordingAsPicture();
      inst.bakedLeafOpacity = leafOpacity;  // a settled transition re-bakes
      inst.bakedLeafBlend = leafBlend;      // (the recording froze them in)
      inst.bakedLiveShader =
          inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue : nullptr;
      inst.bakedScalars = scalarsNow;
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    if (profileScope.row != SIZE_MAX)
      profileRows[profileScope.row].cacheState = Composer::CacheState::Picture;
    // The measurement that drives promotion. Two clock reads per candidate
    // node per frame, against a full rasterisation — the overhead is not
    // close to material.
    const auto replayStart = std::chrono::steady_clock::now();
    profDraw("replay", [&] { canvas.drawPicture(inst.picture); });
    accrue(std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - replayStart)
               .count());
  } else {
    stats.nodesPainted++;
    // A LEAF never records a picture — one draw call beats a nested
    // recording — so without this it would never be timed at all, and the
    // most expensive single object a scene can hold, a full-canvas box
    // carrying one shader, would be structurally invisible to the promoter.
    // So the live draw is timed too, but ONLY for a node that could
    // actually be promoted: that keeps two clock reads per frame off every
    // ineligible node in the tree, of which there are usually thousands.
    if (!promotable) {
      profDraw("live", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity);
      });
    } else {
      const auto liveStart = std::chrono::steady_clock::now();
      profDraw("live", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity);
      });
      accrue(std::chrono::duration<double, std::milli>(
                 std::chrono::steady_clock::now() - liveStart)
                 .count());
    }
    inst.paintDirty = false;
  }

  if (needsLayer) canvas.restore();
  canvas.restore();
}

}  // namespace sigil::compose
