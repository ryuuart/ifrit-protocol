// Paint phase: the volatility walk that decides what may cache, the node
// silhouette resolution, and the stacking painter with its three cache tiers
// (live paint, auto SkPicture of provably-static subtrees, and Cache::Texture
// raster bakes). See DESIGN.md "Stacking and compositing" and "Caching".

#include "ComposeRuntime.h"

#include <sigilimage/ImageAsset.h>

#include <sigilweave/Choreograph.h>
#include <sigilweave/FontContext.h>
#include <sigilweave/Shaper.h> // makeFont — textFill's cap-height metrics

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
#include <include/effects/SkTrimPathEffect.h>

#include <algorithm>
#include <cstring>
#include <chrono>
#include <cmath>

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Null-safe views into ElementNode's rare-field blocks (see ComposeInternal.h)

namespace {

inline const Material *liveMaterialOf(const ElementNode &n) {
  return n.materialData && n.materialData->live ? &*n.materialData->live
                                                : nullptr;
}
inline const Material *metricFillOf(const ElementNode &n) {
  return n.textData && n.textData->metricFill ? &*n.textData->metricFill
                                              : nullptr;
}
inline const GlyphFx *glyphFxOf(const ElementNode &n) {
  return n.textData && n.textData->glyphFx ? &*n.textData->glyphFx : nullptr;
}
inline const sigil::image::ImageAsset *imageAssetOf(const ElementNode &n) {
  return n.imageData ? n.imageData->asset.get() : nullptr;
}
inline const Effect *layerEffectOf(const ElementNode &n) {
  return n.fxData && n.fxData->layerEffect ? &*n.fxData->layerEffect : nullptr;
}
inline const Effect *backdropEffectOf(const ElementNode &n) {
  return n.fxData && n.fxData->backdropEffect ? &*n.fxData->backdropEffect
                                              : nullptr;
}
inline const std::vector<Echo> &echoesOf(const ElementNode &n) {
  static const std::vector<Echo> kNoEchoes;
  return n.fxData ? n.fxData->echoes : kNoEchoes;
}

} // namespace

// ---------------------------------------------------------------------------
// VariationDrive — the paint-side gate + per-frame coordinate

namespace {

/** The node's live font-variation override, or null: probes the layout's
 *  shaped typefaces ONCE per text content (advance-invariance on the
 *  driven axis) and refuses advance-variant axes with a one-time warning —
 *  the shaped positions must stay the truth. Returns a pointer to
 *  thread-local state consumed by the immediately following drawBatched. */
const sigil::weave::ParagraphLayout::LiveVariations *
liveDriveImpl(Instance &inst, const ElementNode &node,
              sigil::weave::FontContext &fonts) {
  const TextData *text = node.textData ? &*node.textData : nullptr;
  if (!text || !text->driveValue)
    return nullptr;
  if (inst.driveProbe < 0) {
    char tag[5] = {text->driveTag[0], text->driveTag[1], text->driveTag[2],
                   text->driveTag[3], 0};
    bool invariant = false, sawTypeface = false;
    for (const sigil::weave::PositionedRun &run : inst.textLayout.runs) {
      if (!run.shaped || !run.shaped->typeface)
        continue;
      sawTypeface = true;
      if (!fonts.axisIsAdvanceInvariant(run.shaped->typeface, tag)) {
        invariant = false;
        break;
      }
      invariant = true;
    }
    inst.driveProbe = (sawTypeface && invariant) ? 1 : 0;
    if (inst.driveProbe == 0)
      SkDebugf("sigilcompose variationDrive: axis \"%s\" is absent or "
               "moves advances on this font — drive refused (text draws at "
               "its shaped coordinates; GRAD is the advance-invariant "
               "weight, or re-render discretely)\n",
               tag);
  }
  if (inst.driveProbe != 1)
    return nullptr;
  static thread_local sigil::weave::FontVariation coordinate;
  static thread_local sigil::weave::ParagraphLayout::LiveVariations live;
  std::memcpy(coordinate.tag, text->driveTag, 4);
  coordinate.value = text->driveValue->value();
  live.fonts = &fonts;
  live.variations = {&coordinate, 1};
  return &live;
}

/** §30: every animated scalar under a `Cache::Group` root, in tree order.
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
 *  floats per node: ~2000 reads for kumiko's 523 strips, against the 113 ms
 *  of GPU work (ROADMAP §29) it is deciding whether to skip. */
void collectGroupScalars(const Instance &inst, bool root,
                         std::vector<float> &out) {
  const ElementNode &node = *inst.desc;
  const auto push = [&](Instance::Slot slot, const Animatable<float> &v) {
    if (v.binding() ||
        (inst.anims[slot] && inst.anims[slot]->value.isConnected()))
      out.push_back(inst.resolveFloat(slot, v));
  };
  if (!root) {
    push(Instance::kOpacity, node.paint.opacity);
    push(Instance::kTx, node.paint.translateX);
    push(Instance::kTy, node.paint.translateY);
    push(Instance::kRotate, node.paint.rotate);
    push(Instance::kScale, node.paint.scale);
    push(Instance::kScaleX, node.paint.scaleX);
    push(Instance::kScaleY, node.paint.scaleY);
    push(Instance::kSkewX, node.paint.skewX);
    push(Instance::kSkewY, node.paint.skewY);
  }
  // Mask gates: the same argument, over the per-mask vector. Only LIVE
  // values are pushed, so the vector's LENGTH still carries a motion
  // connecting or disconnecting.
  if (node.hasMasks()) {
    size_t slot = 0;
    for (const Mask &m : node.fxData->masks) {
      const auto pushGate = [&](const Animatable<float> &v) {
        const AnimatedFloat *a =
            slot < inst.maskAnims.size() ? inst.maskAnims[slot].get() : nullptr;
        if (v.binding() || (a && a->value.isConnected()))
          out.push_back(inst.resolveFloatAt(a, v));
        ++slot;
      };
      if (m.with.kind == Gate::Kind::Spans)
        for (const Spans::Term &t : m.with.where.terms) {
          pushGate(t.begin);
          pushGate(t.end);
          pushGate(t.offset);
        }
      else if (m.with.kind == Gate::Kind::Edge)
        pushGate(m.with.fraction);
    }
  }
  if (const GlyphFx *g = glyphFxOf(node))
    push(Instance::kGlyphProgress, g->progress);
  if (inst.anims[Instance::kFillLerp] &&
      inst.anims[Instance::kFillLerp]->value.isConnected())
    out.push_back(inst.anims[Instance::kFillLerp]->value.value());
  for (const auto &child : inst.children)
    collectGroupScalars(*child, false, out);
}

} // namespace

// ---------------------------------------------------------------------------
// Stroke passes: resolving each pass's claim, and saying so when two
// claims collide.

namespace {

std::string passLabel(const detail::StrokePass &pass, size_t index) {
  if (!pass.name.empty())
    return "\"" + pass.name + "\"";
  return "#" + std::to_string(index);
}

/** One boundary, one mark: two claims on the same run is a mistake with
 *  no sensible rendering, so it is said out loud once per shape of the
 *  problem. Layering two marks on ONE run is a composite brush, and the
 *  message says so — that is the only place an author learns it. */
void warnOverlappingClaims(const std::string &a, const std::string &b,
                           Span shared) {
  static std::vector<std::string> seen;
  const std::string key = a + "|" + b;
  for (const std::string &k : seen)
    if (k == key)
      return;
  if (seen.size() >= 16)
    return;
  seen.push_back(key);
  SkDebugf("compose: span passes %s and %s both claim %.3f–%.3f of the "
           "same boundary. One boundary, one mark: spans partition it, they "
           "do not stack — and the law reads across BOTH z-halves, so a "
           "background(spans, ...) pass and a stroke(spans, ...) pass "
           "collide the same way two strokes do. To layer two marks on one "
           "run, make them ONE pass with a composite brush "
           "(Brush{}.layer(a).layer(b), or a LayeredBrush); to keep them apart, "
           "give the second pass a disjoint span (or spans::rest()).\n",
           a.c_str(), b.c_str(), shared.begin, shared.end);
}

} // namespace

std::vector<std::vector<Span>>
detail::Instance::resolveSpans(const SkPath &outline) const {
  std::vector<std::vector<Span>> out;
  const ElementNode &node = *desc;
  if (!node.hasStrokePasses())
    return out;
  const std::vector<StrokePass> &passes = node.strokeData->passes;
  out.resize(passes.size());

  // Every animatable endpoint, resolved for this frame, in the order the
  // description declared them — the order spanAnims is indexed by.
  std::vector<float> values;
  values.reserve(spanAnims.size());
  size_t slot = 0;
  auto push = [&](const Animatable<float> &v) {
    const AnimatedFloat *a =
        slot < spanAnims.size() ? spanAnims[slot].get() : nullptr;
    values.push_back(resolveFloatAt(a, v));
    ++slot;
  };
  for (const StrokePass &pass : passes)
    for (const Spans::Term &term : pass.where.terms) {
      push(term.begin);
      push(term.end);
      push(term.offset);
    }

  SpanInput in;
  in.outline = &outline;
  in.fitRects = &spanFitRects;

  size_t valueBase = 0;
  for (size_t i = 0; i < passes.size(); ++i) {
    std::vector<float> mine(values.begin() + (long)valueBase,
                            values.begin() +
                                (long)(valueBase + passes[i].where.valueCount()));
    valueBase += passes[i].where.valueCount();
    in.values = &mine;
    out[i] = passes[i].where.resolve(in);
  }

  // rest(): the complement, resolved AFTER the claims it is defined
  // against. Bare rest() takes everything the other CLAIMING passes left;
  // rest("name") is one named pass's complement and may overlay.
  for (size_t i = 0; i < passes.size(); ++i) {
    if (!passes[i].where.hasRest())
      continue;
    std::vector<Span> against;
    bool named = false;
    for (const Spans::Term &term : passes[i].where.terms) {
      if (term.rule != Spans::Rule::Rest || term.key.empty())
        continue;
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
  // stroke never gets here (it is an ordinary foreground), so no existing
  // scene can become an error — the §27 alias-first law.
  for (size_t i = 0; i < passes.size(); ++i) {
    if (passes[i].where.hasRest())
      continue;
    for (size_t j = i + 1; j < passes.size(); ++j) {
      if (passes[j].where.hasRest())
        continue;
      if (std::optional<Span> shared = spansOverlap(out[i], out[j]))
        warnOverlappingClaims(passLabel(passes[i], i), passLabel(passes[j], j),
                              *shared);
    }
  }
  return out;
}

std::vector<float> detail::Instance::resolveGateValues() const {
  std::vector<float> values;
  const ElementNode &node = *desc;
  if (!node.hasMasks())
    return values;
  size_t slot = 0;
  const auto push = [&](const Animatable<float> &v) {
    const AnimatedFloat *a =
        slot < maskAnims.size() ? maskAnims[slot].get() : nullptr;
    values.push_back(resolveFloatAt(a, v));
    ++slot;
  };
  for (const Mask &m : node.fxData->masks) {
    if (m.with.kind == Gate::Kind::Spans)
      for (const Spans::Term &t : m.with.where.terms) {
        push(t.begin);
        push(t.end);
        push(t.offset);
      }
    else if (m.with.kind == Gate::Kind::Edge)
      push(m.with.fraction);
  }
  return values;
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
//  - EDGE / SHAPE / ALPHA cut the PLANE. They wrap the selected outputs in
//    a canvas clip (edge, shape) or a kDstIn coverage layer (alpha).
//
// Both classes intersect for free, and across each other: span sets
// intersect as interval arithmetic, nested clips intersect by definition,
// stacked kDstIn layers multiply coverage. Where a mask selects EVERYTHING
// the plane gates are hoisted to wrap the whole node once rather than each
// group — cheaper, and the only way a nested pair of antialiased clips
// cannot compound its own edge.

namespace {

/** Does this span set claim the whole boundary? Then the boundary is
 *  untouched — a settled reveal must draw the path it would have drawn
 *  with no mask on it at all, bit for bit, or every settled plate in the
 *  corpus moves. */
bool claimsEverything(const std::vector<Span> &show) {
  return show.size() == 1 && show[0].begin <= 1e-6f && show[0].end >= 1.0f - 1e-6f;
}

/** Apply a resolved SHOW set to a boundary. `cut`, when asked for, says
 *  the geometry actually changed — which only the SURFACE needs, because
 *  only the surface has a cheap rrect to fall out of. Decorations always
 *  draw a path. */
SkPath gateOutline(const SkPath &src, const std::vector<Span> &show,
                   bool *cut = nullptr) {
  if (claimsEverything(show))
    return src;
  if (cut)
    *cut = true;
  if (show.empty())
    return SkPath();
  return detail::spanPath(src, show);
}

} // namespace

// ---------------------------------------------------------------------------
// Volatility & caching

bool Composer::Impl::computeVolatile(Instance &inst) {
  const ElementNode &node = *inst.desc;

  auto boundOrRunning = [&](Instance::Slot slot, const Animatable<float> &v) {
    if (v.binding())
      return true;
    return inst.anims[slot] && inst.anims[slot]->value.isConnected();
  };
  // Span passes: an animated reveal rebuilds the pass's geometry, and an
  // animated brush repaints it. Both are CONTENT volatility, like trim —
  // and deliberately kept out of the scalar/live-material memos, which
  // compare a fixed list of floats and cannot see a span claim.
  const bool spanVolatile = [&] {
    if (!node.hasStrokePasses())
      return false;
    size_t slot = 0;
    bool live = false;
    for (const StrokePass &pass : node.strokeData->passes) {
      live |= pass.what.isAnimated();
      for (const Spans::Term &term : pass.where.terms)
        for (const Animatable<float> *v :
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
  // Mask gates, split the way §3.6 requires. A gate whose animation is a
  // BOUNDED LIST OF FLOATS (spans endpoints, an edge fraction) is
  // memo-visible and joins scalarContent below; a gate driven by a LIVE
  // MATERIAL is not a float and refuses both memos, exactly as a live
  // material fill does. A shape gate is a static Region and moves nothing.
  bool maskScalarLive = false, maskOpaque = false;
  if (node.hasMasks()) {
    size_t slot = 0;
    const auto live = [&](const Animatable<float> &v) {
      const AnimatedFloat *a =
          slot < inst.maskAnims.size() ? inst.maskAnims[slot].get() : nullptr;
      if (v.binding() || (a && a->value.isConnected()))
        maskScalarLive = true;
      ++slot;
    };
    for (const Mask &m : node.fxData->masks) {
      switch (m.with.kind) {
      case Gate::Kind::Spans:
        for (const Spans::Term &t : m.with.where.terms) {
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
      case Gate::Kind::Alpha:
        if (m.with.coverage && m.with.coverage->isAnimated())
          maskOpaque = true;
        break;
      }
    }
  }
  // Paint-only volatility: transforms and opacity apply OUTSIDE the node's
  // content (in paint()'s matrix/layer stack), so a node animated only here
  // still replays its content picture — a spinning ornament re-records
  // nothing. Ancestors still can't cache across it (their recording would
  // freeze the motion), hence the return value.
  bool ownPaint = false;
  ownPaint |= boundOrRunning(Instance::kOpacity, node.paint.opacity);
  // The GEOMETRIC half, kept separately: a texture bake taken in device
  // space is pinned to one device rect, so it may only be taken when the
  // node is not moving. Opacity is deliberately not part of this — it does
  // not move the rect.
  bool moving = false;
  moving |= boundOrRunning(Instance::kTx, node.paint.translateX);
  moving |= boundOrRunning(Instance::kTy, node.paint.translateY);
  moving |= boundOrRunning(Instance::kRotate, node.paint.rotate);
  moving |= boundOrRunning(Instance::kScale, node.paint.scale);
  moving |= boundOrRunning(Instance::kSkewX, node.paint.skewX);
  moving |= boundOrRunning(Instance::kSkewY, node.paint.skewY);
  moving |= boundOrRunning(Instance::kScaleX, node.paint.scaleX);
  moving |= boundOrRunning(Instance::kScaleY, node.paint.scaleY);
  inst.transformLive = moving;
  ownPaint |= moving;

  // Content volatility: what actually invalidates the node's own recording
  // (bound/lerping fills, per-frame programs, animated decorations and image
  // frames).
  bool ownContent = inst.anims[Instance::kFillLerp] &&
                    inst.anims[Instance::kFillLerp]->value.isConnected();
  const bool nonLiveMatContent = ownContent; // everything below except the
                                             // live material slot
  if (node.paint.fill && node.paint.fill->binding())
    ownContent = true;
  const Material *nodeLiveMat = liveMaterialOf(node);
  const bool liveMat = nodeLiveMat && nodeLiveMat->isAnimated();
  if (liveMat)
    ownContent = true; // truly live (bound/uTime) — geometry-dependent
                       // materials resolve at record time and stay cacheable
  if (const Material *mf = metricFillOf(node); mf && mf->isAnimated())
    ownContent = true; // animated chrome type paints per frame
  if (node.cacheMode == Cache::None)
    ownContent = true;
  for (const Decoration &d : node.backgrounds)
    ownContent |= d.isAnimated();
  for (const Decoration &d : node.foregrounds)
    ownContent |= d.isAnimated();
  if (node.fxData)
    for (const Decoration &d : node.fxData->overlays)
      ownContent |= d.isAnimated();
  if (node.kind == Kind::Image && imageAssetOf(node) &&
      imageAssetOf(node)->animated())
    ownContent = true;
  ownContent |= spanVolatile;
  ownContent |= maskOpaque;
  // The MEMOIZABLE scalars, tracked apart from the rest of ownContent: each
  // rebuilds the painted geometry when it moves, and each is a number that
  // can sit still for a long time inside a running motion (§17).
  bool scalarContent = false;
  scalarContent |= maskScalarLive; // a moving gate re-cuts or re-clips
  if (const GlyphFx *g = glyphFxOf(node)) // moving glyph progress rebuilds
    scalarContent |= boundOrRunning(Instance::kGlyphProgress, g->progress);
  ownContent |= scalarContent;
  if (node.textData && node.textData->driveValue)
    ownContent = true; // VariationDrive repaints per frame (no reshape)

  // §30: what a SUBTREE VALUE MEMO can and cannot see. A group bake is held
  // by comparing floats, so every source of volatility in it must either BE
  // a float this frame can read back (the transform slots, opacity, the
  // mask gates, glyph progress, the fill lerp) or arrive as a description
  // change, which stales the group root through markPaintDirtyUp().
  // Everything listed here is neither: it moves pixels off the clock with no number to compare, and
  // a group holding a bake across one of them would blit last second's
  // picture forever. Refused outright rather than approximated — this is the
  // whole risk of the feature, and it is the one place to be conservative.
  bool opaqueToTheMemo = false;
  if (node.paint.fill && node.paint.fill->binding())
    opaqueToTheMemo = true; // a Fill is not a float
  if (liveMat)
    opaqueToTheMemo = true; // uTime / a bound uniform
  if (const Material *mf = metricFillOf(node); mf && mf->isAnimated())
    opaqueToTheMemo = true;
  if (node.cacheMode == Cache::None)
    opaqueToTheMemo = true; // declared per-frame volatility
  for (const Decoration &d : node.backgrounds)
    opaqueToTheMemo |= d.isAnimated();
  for (const Decoration &d : node.foregrounds)
    opaqueToTheMemo |= d.isAnimated();
  if (node.fxData)
    for (const Decoration &d : node.fxData->overlays)
      opaqueToTheMemo |= d.isAnimated();
  if (node.kind == Kind::Image && imageAssetOf(node) &&
      imageAssetOf(node)->animated())
    opaqueToTheMemo = true;
  if (node.textData && node.textData->driveValue)
    opaqueToTheMemo = true;
  opaqueToTheMemo |= spanVolatile;
  opaqueToTheMemo |= maskOpaque; // an alpha gate on a LIVE material

  bool childrenVolatile = false;
  bool childReadsBackdrop = false;
  bool childrenGroupSafe = true;
  for (auto &child : inst.children) {
    childrenVolatile |= computeVolatile(*child);
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
  // must ask about the whole subtree. A split bake (§15) replaces only the
  // node's OWN layer and draws children over the blit, so it must ask only
  // about the node's own paint — the children composite against the blit
  // exactly as they would against freshly rasterized pixels.
  inst.ownReadsBackdrop = backdropEffectOf(node) != nullptr ||
                          node.paint.blendMode != SkBlendMode::kSrcOver;
  inst.subtreeReadsBackdrop = inst.ownReadsBackdrop || childReadsBackdrop;

  // §30, the two halves. `groupSafe` is what a PARENT asks of this subtree —
  // and it includes this node's own backdrop read, because inside a group
  // bake a kMultiply child resolves against transparent black exactly as it
  // would under whole-subtree promotion. `groupRootOK` is what this node
  // asks of ITSELF, and deliberately does not: a group root's own blend and
  // opacity are applied by paint()'s saveLayer, outside the bake, exactly as
  // they would be applied outside the live paint. A backdrop FILTER on the
  // root is still fatal — it samples the destination, which the bake is not.
  inst.groupSafe =
      !opaqueToTheMemo && !inst.ownReadsBackdrop && childrenGroupSafe;
  inst.groupRootOK = node.cacheMode == Cache::Group && !opaqueToTheMemo &&
                     childrenGroupSafe && backdropEffectOf(node) == nullptr;
  if (node.cacheMode == Cache::Group && !inst.groupRootOK && !inst.groupWarned) {
    inst.groupWarned = true;
    // Loud, because the alternative is an author reading `live paint,
    // 663 ms` on a node they explicitly asked to bake and having no way to
    // learn that one descendant three levels down declined it for them.
    SkDebugf("sigilcompose Cache::Group: \"%s\" cannot bake — %s. A group is "
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
    inst.ownImage.reset(); // a volatile own paint can never hold a bake
  // The resolve-memo carve-out: volatility caused SOLELY by a live
  // material keeps its picture — paint() replays it while resolve() stays
  // stable and re-records only when the shader actually changes.
  const Material *mfLive = metricFillOf(node);
  inst.liveMatOnly = liveMat && ownContent && !nonLiveMatContent &&
                     !(mfLive && mfLive->isAnimated()) &&
                     node.cacheMode != Cache::None && !childrenVolatile;
  // (decoration/image/trim/glyph volatility all set ownContent through
  // nonLiveMatContent's snapshot point or below — re-derive precisely:)
  if (inst.liveMatOnly) {
    bool other = nonLiveMatContent;
    for (const Decoration &d : node.backgrounds)
      other |= d.isAnimated();
    for (const Decoration &d : node.foregrounds)
      other |= d.isAnimated();
    if (node.fxData)
      for (const Decoration &d : node.fxData->overlays)
        other |= d.isAnimated();
    if (node.kind == Kind::Image && imageAssetOf(node) &&
        imageAssetOf(node)->animated())
      other = true;
    other |= maskScalarLive || maskOpaque;
    if (const GlyphFx *g = glyphFxOf(node))
      other |= boundOrRunning(Instance::kGlyphProgress, g->progress);
    if (node.textData && node.textData->driveValue)
      other = true;
    other |= spanVolatile;
    if (other)
      inst.liveMatOnly = false;
  }
  // §17: the same carve-out for animated SCALARS. A node whose content
  // volatility is entirely mask gates and glyph progress keeps its
  // recording and
  // re-records only when one of those numbers actually ticks — a keyframe
  // hold segment repaints nothing. Deliberately disjoint from liveMatOnly:
  // a node with BOTH a live material and an animated trim takes neither
  // memo, which is the conservative answer and costs only what it costs
  // today.
  inst.scalarMemo = false;
  if (scalarContent && !liveMat && node.cacheMode != Cache::None &&
      !childrenVolatile) {
    bool other = nonLiveMatContent; // the fill lerp
    for (const Decoration &d : node.backgrounds)
      other |= d.isAnimated();
    for (const Decoration &d : node.foregrounds)
      other |= d.isAnimated();
    if (node.fxData)
      for (const Decoration &d : node.fxData->overlays)
        other |= d.isAnimated();
    if (node.kind == Kind::Image && imageAssetOf(node) &&
        imageAssetOf(node)->animated())
      other = true;
    if (const Material *mf = metricFillOf(node); mf && mf->isAnimated())
      other = true;
    if (node.textData && node.textData->driveValue)
      other = true;
    other |= spanVolatile;
    inst.scalarMemo = !other;
  }
  const bool memoized = inst.liveMatOnly || inst.scalarMemo;
  if (blocked != inst.subtreeVolatile) {
    inst.subtreeVolatile = blocked;
    if (!memoized)
      inst.paintDirty = true; // cacheability changed → re-record/drop
  }
  if (inst.subtreeVolatile && !memoized) {
    inst.picture.reset();
    // §30: a group root's bake is dropped by its OWN value memo, in paint(),
    // one frame at a time. Dropping it here instead would drop it every
    // frame — the subtree IS volatile, permanently, and that verdict is
    // precisely the one the group exists to look past. `picture` is still
    // reset: a group root never replays one, and leaving a stale recording
    // reachable is how the fall-through path would blit last frame's pixels
    // on the frame the memo just said not to.
    if (!inst.groupRootOK)
      inst.textureImage.reset();
  }
  return ownPaint || blocked;
}

// ---------------------------------------------------------------------------
// Silhouette

const SkPath &Composer::Impl::resolveOutline(Instance &inst, SkSize size) const {
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

/** Text whose baseline is a path: shape ONCE (real kerning, real
 *  ligatures), then place every glyph by arc length and rotate it to the
 *  tangent, through the same batched RSXform draw kinetic text uses.
 *
 *  Every contour is walked in order. Glyphs past the end of the path are
 *  dropped rather than piled on the last point — running off the end of a
 *  ring should look like running off the end of a ring. */
void Composer::Impl::paintTextOnPath(Instance &inst, SkCanvas &canvas,
                                     const TextPath &spec, SkSize size) {
  if (!spec.path)
    return;
  const SkPath baseline = spec.path(size);
  // ALL the contours, walked in order as one baseline. It used to be the
  // first one only, silently: a trajectory clipped to the frame produces
  // several contours, and the KSP study's hyperbola lost its label with
  // no diagnostic at all.
  static thread_local std::vector<sk_sp<SkContourMeasure>> contours;
  contours.clear();
  for (SkContourMeasureIter iter(baseline, false);;) {
    sk_sp<SkContourMeasure> c = iter.next();
    if (!c)
      break;
    if (c->length() > 0)
      contours.push_back(std::move(c));
  }
  if (contours.empty())
    return;
  float length = 0;
  for (const auto &c : contours)
    length += c->length();
  // One arc-length coordinate over the whole chain.
  auto posTan = [&](float d, SkPoint *pos, SkVector *tan) {
    for (const auto &c : contours) {
      if (d <= c->length())
        return c->getPosTan(d, pos, tan);
      d -= c->length();
    }
    const auto &last = contours.back();
    return last->getPosTan(last->length(), pos, tan);
  };

  // The run's own width, from the shaped advances — this is what Align
  // measures against, and it is why the run has to be shaped first.
  float runWidth = 0;
  sigil::weave::forEachPlacedGlyph(
      inst.textLayout, *inst.paragraph,
      [&](const sigil::weave::ShapedWord *, SkGlyphID, float advance, SkColor,
          SkPoint rest) {
        runWidth = std::max(runWidth, rest.x() + advance);
      });

  float start = spec.at * length;
  if (spec.align == TextPath::Align::Center)
    start -= runWidth * 0.5f;
  else if (spec.align == TextPath::Align::End)
    start -= runWidth;

  // A CLOSED baseline has no ends: fraction 0 and 1 are the same point, so
  // a centred run at at=0 must straddle the seam rather than lose the half
  // that lands at a negative distance. (An open path keeps the drop — a
  // run walking off the end of a line should look like it.)
  //
  // "Closed" here means geometrically closed, not flagged closed:
  // shapes::arc() defaults to a 359.9-degree sweep and is the library's
  // own spelling for a ring, but addArc leaves it open. Dropping half a
  // centred caption off a ring because of a tenth of a degree is not a
  // behaviour anyone wants.
  bool closed = contours.size() == 1 && contours.front()->isClosed();
  if (!closed) {
    SkPoint head, tail;
    SkVector ignored;
    if (posTan(0, &head, &ignored) && posTan(length, &tail, &ignored))
      closed = SkPoint::Distance(head, tail) <=
               std::max(1.0f, length * 0.002f);
  }

  // autoFlip is a decision about the RUN, not about each glyph. Turning
  // glyphs over one at a time reverses the reading order — a caption on
  // the lower half of a clockwise ring came out mirrored — so the run
  // decides once and then walks its along-path coordinate backwards.
  //
  // The decision is a MAJORITY over the run, not a reading at its
  // midpoint. A midpoint sample is exactly ambiguous where the tangent is
  // vertical, which is precisely where a ring caption centred on 12 or 6
  // o'clock puts it: `tan.x < 0` is false at x == 0, so the most natural
  // spelling of all — circle(), at = 0, Center, autoFlip — silently did
  // nothing. Sampling across the run has no such point.
  //
  // A run that wraps PAST the crossover cannot be fixed by one flip, and
  // this model does not pretend otherwise: the majority reads right way
  // up and the tail does not. That is a real limitation, and the
  // engraver's answer to it is two runs — top and bottom set separately,
  // which is how ring inscriptions have always been cut. See ROADMAP.md.
  bool flipRun = false;
  if (spec.autoFlip) {
    constexpr int kVotes = 9;
    int upsideDown = 0, upright = 0;
    for (int i = 0; i < kVotes; ++i) {
      float at = start + runWidth * ((float)i + 0.5f) / (float)kVotes;
      if (closed)
        at = std::fmod(std::fmod(at, length) + length, length);
      SkPoint pos;
      SkVector tan;
      if (!posTan(std::clamp(at, 0.0f, length), &pos, &tan))
        continue;
      if (tan.x() < 0)
        ++upsideDown;
      else if (tan.x() > 0)
        ++upright;
    }
    flipRun = upsideDown > upright;
  }

  // The centroid the Radial orientation faces away from: the bounds of
  // the resolved baseline, which for every dial-shaped path is its centre.
  const SkRect baselineBounds = baseline.getBounds();
  const SkPoint centroid{baselineBounds.centerX(), baselineBounds.centerY()};

  static thread_local sigil::weave::GlyphRSXformBatches batches;
  batches.clear();
  sigil::weave::forEachPlacedGlyph(
      inst.textLayout, *inst.paragraph,
      [&](const sigil::weave::ShapedWord *font, SkGlyphID glyph, float advance,
          SkColor color, SkPoint rest) {
        // The glyph rides its own CENTRE along the path, so a wide glyph
        // on a tight curve leans about its middle rather than its left
        // sidebearing.
        const float along = rest.x() + advance * 0.5f;
        float d = flipRun ? start + runWidth - along : start + along;
        if (closed)
          d = std::fmod(std::fmod(d, length) + length, length);
        else if (d < 0 || d > length)
          return;
        SkPoint pos;
        SkVector tangent;
        if (!posTan(d, &pos, &tangent))
          return;
        const float mag = std::hypot(tangent.x(), tangent.y());
        if (mag <= 1e-6f)
          return;
        float dirX = tangent.x() / mag, dirY = tangent.y() / mag;
        if (flipRun) {
          dirX = -dirX;
          dirY = -dirY;
        }
        // Perpendicular offset, positive to the LEFT of travel (outward on
        // a clockwise circle). The path replaces the glyph's own baseline.
        // Measured along TRAVEL even under Radial orientation, so `offset`
        // keeps meaning "how far off the baseline the type rides"
        // regardless of which way the glyph ends up facing.
        pos.offset(dirY * spec.offset, -dirX * spec.offset);
        // Radial: the glyph's BASELINE runs along the radius, so the run
        // reads outward from the centre like a spoke. That is how an
        // astrolabe limb, a compass rose and a radial axis label their
        // divisions — you turn the instrument to read them.
        //
        // Note this is genuinely a different thing from what Tangent
        // already does. On a circle, "up points outward" IS the tangent
        // orientation (a clock face's 6 is upside down for exactly that
        // reason), so the only orientation onPath was missing is the one
        // where the type radiates.
        if (spec.orient == TextPath::Orient::Upright) {
          dirX = 1.0f;
          dirY = 0.0f;
        } else if (spec.orient == TextPath::Orient::Radial) {
          const float ox = pos.x() - centroid.x(), oy = pos.y() - centroid.y();
          const float omag = std::hypot(ox, oy);
          if (omag > 1e-6f) {
            dirX = ox / omag;
            dirY = oy / omag;
          }
        }
        // Quantized for the same reason kinetic text quantizes: a
        // continuous per-glyph angle mints a fresh glyph mask per letter
        // in Skia's cache. 64 steps is 5.6 degrees, which on a ring whose
        // own letters sit ~7 degrees apart is under a pixel of lean at
        // label sizes.
        float cosv = 1.0f, sinv = 0.0f;
        sigil::weave::quantizeAngle(std::atan2(dirY, dirX), cosv, sinv);
        batches.addGlyph(font, color, glyph, advance * 0.5f, pos, cosv, sinv);
      });
  batches.draw(&canvas);
}

void Composer::Impl::paintKineticText(Instance &inst, SkCanvas &canvas,
                                      const GlyphFx &fx) {
  const float master = std::clamp(
      inst.resolveFloat(Instance::kGlyphProgress, fx.progress), 0.0f, 1.0f);

  size_t count = 0;
  sigil::weave::forEachPlacedGlyph(inst.textLayout, *inst.paragraph,
                                   [&](auto &&...) { ++count; });
  if (count == 0)
    return;

  float each = std::max(fx.stagger.eachMs, 0.0f);
  if (fx.stagger.amountMs > 0 && count > 1)
    each = fx.stagger.amountMs / (float)(count - 1); // GSAP amount mode
  const float duration = std::max(fx.stagger.durationMs, 1.0f);
  const float total = duration + each * (float)(count - 1);

  static thread_local sigil::weave::GlyphRSXformBatches batches;
  batches.clear();

  size_t i = 0;
  sigil::weave::forEachPlacedGlyph(
      inst.textLayout, *inst.paragraph,
      [&](const sigil::weave::ShapedWord *font, SkGlyphID glyph, float advance,
          SkColor color, SkPoint rest) {
        float order = (float)i;
        switch (fx.stagger.from) {
        case Stagger::From::End:
          order = (float)(count - 1 - i);
          break;
        case Stagger::From::Center:
          order = std::abs((float)i - (float)(count - 1) * 0.5f) * 2.0f;
          break;
        case Stagger::From::Start:
          break;
        }
        const float t = std::clamp(
            (master * total - order * each) / duration, 0.0f, 1.0f);
        const GlyphMod mod = fx.effect(
            GlyphInfo{i, count, rest, advance, font->fontSize}, t);
        ++i;
        if (mod.alpha <= 0.003f || mod.scale <= 0.001f)
          return;
        // Quantize alpha so fades don't mint a batch bucket per glyph.
        const float alpha =
            std::round(std::clamp(mod.alpha, 0.0f, 1.0f) * 32.0f) / 32.0f;
        const SkColor tinted = SkColorSetA(
            color, (U8CPU)((float)SkColorGetA(color) * alpha + 0.5f));
        float cosv = 1.0f, sinv = 0.0f;
        if (mod.rotateDeg != 0)
          sigil::weave::quantizeAngle(mod.rotateDeg * 0.017453293f, cosv,
                                      sinv);
        cosv *= mod.scale;
        sinv *= mod.scale;
        batches.addGlyph(font, tinted, glyph, advance * 0.5f,
                         {rest.x() + mod.dx + advance * 0.5f,
                          rest.y() + mod.dy},
                         cosv, sinv);
      });
  batches.draw(&canvas);
}

// ---------------------------------------------------------------------------
// Recording bounds

/** The rect a node's RECORDING must cover, in its own local space: its box,
 *  grown by its decorations' declared bleed(), unioned with every child's
 *  bounds mapped through that child's layout offset and static paint
 *  transforms. SkPictureRecorder quick-rejects ops outside the cull rect at
 *  record time, so a child translated beyond its parent's box would silently
 *  vanish from the cached path without this (the same failure family as the
 *  bleed truncation — overflow is legal, the cull must hold it). Animated
 *  transforms are fine: resolveFloat reads the record-time value, and a
 *  RUNNING transform makes the subtree volatile — nothing records at all.
 *  A clipped node contributes only its own box: children can't escape. */
SkRect Composer::Impl::ownPaintBounds(Instance &inst) {
  const ElementNode &node = *inst.desc;
  const SkRect rect = instanceRect(inst);
  SkRect local = SkRect::MakeWH(rect.width(), rect.height());
  float bleed = 0;
  for (const Decoration &d : node.backgrounds)
    bleed = std::max(bleed, d.bleed());
  for (const Decoration &d : node.foregrounds)
    bleed = std::max(bleed, d.bleed());
  if (node.fxData)
    for (const Decoration &d : node.fxData->overlays)
      bleed = std::max(bleed, d.bleed());
  if (node.strokeData)
    for (const detail::StrokePass &pass : node.strokeData->passes)
      bleed = std::max(bleed, pass.what.bleed());
  // A band reaches profile.max() px off its spine, and the profile is
  // REQUIRED to know that number — which is the whole reason `max()` is
  // part of the seam. Without it a varying width could only be clipped
  // silently — the trap the deleted `Ribbon::widthFn`/`widthMax` pair left
  // open, §25.
  if (const Across *band = node.bandWidth())
    bleed = std::max(bleed, band->profile.max());
  for (const Echo &e : echoesOf(node))
    bleed = std::max(
        bleed, std::max(std::abs(e.offset.fX), std::abs(e.offset.fY)));
  if (bleed > 0)
    local.outset(bleed, bleed);
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
  // A BAND is the same problem: the bleed above covers the width axis, but
  // a BORROWED spine (band(around(key))) can sit anywhere relative to this
  // node's own box, so the cull has to hold the spine itself — exactly the
  // routed case one paragraph up, and for the same reason.
  if (const Across *band = node.bandWidth()) {
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

SkRect Composer::Impl::recordBounds(Instance &inst) {
  const ElementNode &node = *inst.desc;
  SkRect local = ownPaintBounds(inst);
  if (node.clipContent)
    return local;
  for (auto &child : inst.children) {
    const ElementNode &cn = *child->desc;
    const SkRect crect = instanceRect(*child);
    SkRect cb = recordBounds(*child); // child-local
    const float tx = child->resolveFloat(Instance::kTx, cn.paint.translateX);
    const float ty = child->resolveFloat(Instance::kTy, cn.paint.translateY);
    const float rot = child->resolveFloat(Instance::kRotate, cn.paint.rotate);
    const float scl = child->resolveFloat(Instance::kScale, cn.paint.scale);
    const float sx = child->resolveFloat(Instance::kScaleX, cn.paint.scaleX);
    const float sy = child->resolveFloat(Instance::kScaleY, cn.paint.scaleY);
    const float skx = child->resolveFloat(Instance::kSkewX, cn.paint.skewX);
    const float sky = child->resolveFloat(Instance::kSkewY, cn.paint.skewY);
    SkMatrix m = SkMatrix::Translate(crect.left() + tx, crect.top() + ty);
    if (rot != 0 || scl != 1 || skx != 0 || sky != 0) {
      const SkPoint origin =
          resolveOrigin(cn.paint, crect.width(), crect.height());
      m.preTranslate(origin.x(), origin.y());
      if (rot != 0)
        m.preRotate(rot);
      if (scl != 1)
        m.preScale(scl * sx, scl * sy);
      if (skx != 0 || sky != 0)
        m.preSkew(std::tan(skx * 0.017453293f), std::tan(sky * 0.017453293f));
      m.preTranslate(-origin.x(), -origin.y());
    }
    local.join(m.mapRect(cb));
  }
  return local;
}

// ---------------------------------------------------------------------------
// The stacking painter

void Composer::Impl::paintContent(Instance &inst, SkCanvas &canvas,
                                  float contentScale, SkBlendMode leafBlend,
                                  float leafOpacity, Phase phase) {
  const ElementNode &node = *inst.desc;
  // §15. The two halves of a node's paint, split at the children loop. A
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
  const bool routed = node.deriveData &&
                      (!node.deriveData->connectFrom.empty() ||
                       !node.deriveData->railAnchors.empty());
  const Across *bandWidth = node.bandWidth();
  const bool customShape = (node.shapeFn || bandWidth) && !routed;
  SkPath outlinePath;
  if (routed) {
    outlinePath = inst.connectorPath; // derive phase routed it
  } else if (bandWidth) {
    // A BAND's shape is derived: the region its spine sweeps at the
    // profile's width, on the declared side. The spine is guide data
    // (authored here) or borrowed geometry (derive resolved it).
    const SkPath spine =
        node.deriveData->bandSpine
            ? node.deriveData->bandSpine({bounds.width(), bounds.height()})
            : inst.bandSpine;
    outlinePath = detail::bandRegion(spine, *bandWidth,
                                     node.deriveData->bandFormation);
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
  const std::vector<Mask> *masks =
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
    for (const Mask &m : *masks) {
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
      const auto narrow = [&](std::optional<std::vector<Span>> &slot) {
        slot = slot ? intersectSpans(*slot, show) : show;
      };
      if (m.what.selects(Parts::kSurface))
        narrow(surfaceShow);
      if (m.what.selects(Parts::kMarks))
        narrow(marksShow);
      for (const std::string &label : m.what.names) {
        auto it = std::find_if(namedShow.begin(), namedShow.end(),
                               [&](const auto &e) { return e.first == label; });
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

  // The node's own layer effect wraps everything painted here, so it is
  // captured by picture recordings and BAKED by texture snapshots.
  const Effect *layerFx = layerEffectOf(node);
  const bool hasEffect = layerFx && layerFx->imageFilter();
  if (hasEffect) {
    SkPaint effectPaint;
    effectPaint.setImageFilter(layerFx->imageFilter());
    // BOUNDED: with nullptr bounds the layer allocates at the CLIP size —
    // on a root-level canvas that filtered 900x640 for a 92x72 icon
    // shadow (13.5ms/frame, the aero-icon defect). recordBounds is what
    // the subtree actually paints; Skia expands it for the filter reach.
    const SkRect content = recordBounds(inst);
    canvas.saveLayer(&content, &effectPaint);
  }
  // The MARKS' boundary is what a decoration receives: every decoration
  // dresses the outline, and a spans gate over the marks is a cut of that
  // outline. The surface keeps its own (they are the same path, and the
  // same object, whenever one mask gates both — the common case).
  const PaintContext paintCtx{{bounds.width(), bounds.height()},
                              std::move(marksPath),
                              elapsed(),
                              contentScale,
                              ticker.active(),
                              &fonts,
                              inst.borrowedPaths.empty()
                                  ? nullptr
                                  : &inst.borrowedPaths};

  // ---- the masking family, part 2: the PLANE gates ------------------------
  //
  // `by::edge` and `by::shape` are canvas clips; `by::alpha` is a kDstIn
  // coverage layer. All three intersect for free — nested clips by
  // definition, stacked kDstIn layers by multiplication.
  //
  // A gate whose selection is EVERYTHING is hoisted to wrap the whole node
  // once, exactly where wipe()'s clip used to sit. That is not only the
  // cheap path: applying one antialiased clip per paint group would
  // compound its own edge coverage where the groups overlap, and the
  // hoisted form is the one that reproduces wipe() bit for bit.
  struct PlaneGate {
    const Mask *mask = nullptr;
    float fraction = 1.0f; // Edge
  };
  std::vector<PlaneGate> plane;
  bool granularPlane = false;
  if (masks) {
    size_t valueBase = 0;
    for (const Mask &m : *masks) {
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

  /** wipe()'s half-plane, verbatim: the region lying before a moving edge
   *  at `angleDeg`, built in the edge's own frame and rotated into place —
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
    if (plane.empty())
      return -1;
    int base = -1;
    const auto hit = [&](const Mask &m) {
      if (m.what.isEverything() != wholeNode)
        return false;
      if (wholeNode)
        return true;
      return cls == Parts::kMarks ? m.what.selectsMark(label)
                                  : m.what.selects(cls);
    };
    for (const PlaneGate &g : plane) {
      const Mask &m = *g.mask;
      if (!hit(m) || m.with.kind == Gate::Kind::Alpha)
        continue;
      if (base < 0)
        base = canvas.getSaveCount();
      if (m.with.kind == Gate::Kind::Edge) {
        // A container of absolutely-positioned children measures ZERO, and
        // a half-plane built from an empty box is empty — so a FULL reveal
        // once hid an entire subtree. A reveal at 1 must never hide
        // anything, and an empty box has nothing to reveal along.
        if (bounds.isEmpty())
          continue;
        canvas.save();
        canvas.clipPath(edgeRegion(m.with.angleDeg, g.fraction), true);
      } else { // Shape — and its complement, the missing clipOut()
        canvas.save();
        canvas.clipPath(m.with.region.resolve(fullOutline),
                        m.with.outside ? SkClipOp::kDifference
                                       : SkClipOp::kIntersect,
                        true);
      }
    }
    for (const PlaneGate &g : plane) {
      const Mask &m = *g.mask;
      if (!hit(m) || m.with.kind != Gate::Kind::Alpha)
        continue;
      if (base < 0)
        base = canvas.getSaveCount();
      const SkRect layerBox = recordBounds(inst);
      canvas.saveLayer(&layerBox, nullptr);
      SkPaint cover;
      cover.setAntiAlias(true);
      cover.setBlendMode(SkBlendMode::kDstIn);
      if (m.with.coverage) {
        const Material &mat = *m.with.coverage;
        const Fill f = (mat.isAnimated() || mat.geometryDependent())
                           ? mat.resolve(paintCtx)
                           : mat.toFill();
        if (f.kind == Fill::Kind::Shader && f.shaderValue)
          cover.setShader(f.shaderValue);
        else if (f.kind == Fill::Kind::Color)
          cover.setColor4f(f.colorValue, nullptr);
        else
          cover.setColor4f({0, 0, 0, 0}, nullptr); // Fill::none() shows nothing
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
    if (base >= 0)
      canvas.restoreToCount(base);
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
    if (!node.hasStrokePasses())
      return;
    if (!spanClaims)
      spanClaims = inst.resolveSpans(fullOutline);
    const std::vector<detail::StrokePass> &passes = node.strokeData->passes;
    for (size_t i = 0; i < passes.size() && i < spanClaims->size(); ++i) {
      if (passes[i].half != half || (*spanClaims)[i].empty())
        continue;
      // …and the gate intersects the claim, which is the whole of
      // `.stroke(spans::corners(18), brk).mask(parts::marks(), upTo(t))`:
      // reticle brackets that light up as a sweep reaches them.
      std::vector<Span> run = (*spanClaims)[i];
      if (marksShow)
        run = intersectSpans(run, *marksShow);
      if (!passes[i].name.empty())
        for (const auto &[label, show] : namedShow)
          if (label == passes[i].name)
            run = intersectSpans(run, show);
      if (run.empty())
        continue;
      const size_t cover = coverStack.size();
      const int saves = granularPlane
                            ? enterGates(false, Parts::kMarks, passes[i].name)
                            : -1;
      const PaintContext passCtx{
          paintCtx.size,
          detail::spanPath(fullOutline, run),
          paintCtx.elapsedSeconds,
          paintCtx.contentScale,
          paintCtx.animating,
          paintCtx.fonts,
          paintCtx.borrowed};
      passes[i].what.paint(canvas, passCtx);
      if (granularPlane)
        leaveGates(saves, cover);
    }
  };

  /** Paint one unqualified mark, under whatever gates address it by name.
   *  The common case — no named mask, no granular plane gate — is the
   *  decoration's own paint call and nothing else. */
  const auto paintMark = [&](const Decoration &d, detail::MarkSlot slot,
                             size_t index) {
    std::string_view label;
    if (node.fxData)
      for (const detail::MarkLabel &l : node.fxData->markNames)
        if (l.slot == slot && l.index == index) {
          label = l.name;
          break;
        }
    const std::vector<Span> *refine = nullptr;
    if (!label.empty())
      for (const auto &[name, show] : namedShow)
        if (name == label) {
          refine = &show;
          break;
        }
    const size_t cover = coverStack.size();
    const int saves =
        granularPlane ? enterGates(false, Parts::kMarks, label) : -1;
    if (refine) {
      std::vector<Span> run = *refine;
      if (marksShow)
        run = intersectSpans(run, *marksShow);
      const PaintContext markCtx{paintCtx.size,
                                 gateOutline(fullOutline, run),
                                 paintCtx.elapsedSeconds,
                                 paintCtx.contentScale,
                                 paintCtx.animating,
                                 paintCtx.fonts,
                                 paintCtx.borrowed};
      d.paint(canvas, markCtx);
    } else {
      d.paint(canvas, paintCtx);
    }
    if (granularPlane)
      leaveGates(saves, cover);
  };

  // Background decorations paint beneath the fill (the CSS box-shadow
  // ordering): shadow/pattern layers first, then the surface. Decorations
  // are NEVER clipped — they dress the outline (shadows keep their
  // reach, outer strokes survive on clipped nodes; the aero-study fix).
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
  } else if (const Material *live = liveMaterialOf(node)) {
    resolvedFill = inst.hasPendingLiveFill
                       ? inst.pendingLiveFill
                       : live->resolve(paintCtx);
  } else if (node.paint.fill) {
    Fill fill;
    if (const choreograph::Output<Fill> *binding = node.paint.fill->binding())
      fill = binding->value();
    else if (inst.anims[Instance::kFillLerp] &&
             inst.anims[Instance::kFillLerp]->started &&
             inst.anims[Instance::kFillLerp]->value.isConnected()) {
      const float t = inst.anims[Instance::kFillLerp]->value.value();
      fill = inst.fillTo;
      for (int i = 0; i < 4; ++i)
        fill.colorValue.vec()[i] =
            inst.fillFrom.colorValue.vec()[i] +
            (inst.fillTo.colorValue.vec()[i] -
             inst.fillFrom.colorValue.vec()[i]) * t;
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
    for (const Echo &e : echoesOf(node)) {
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
    const Fill &fill = *resolvedFill;
    SkPaint paint;
    paint.setAntiAlias(true);
    if (fill.kind == Fill::Kind::Color)
      paint.setColor4f(fill.colorValue, nullptr);
    else
      paint.setShader(fill.shaderValue);
    // Leaf fast path: paint() proved a layer is unnecessary and routed the
    // node's blend/opacity straight onto the fill.
    paint.setBlendMode(leafBlend);
    if (leafOpacity < 1.0f)
      paint.setAlphaf(paint.getAlphaf() * leafOpacity);
    if (customShape || trimmed)
      canvas.drawPath(surfacePath, paint);
    else
      canvas.drawRRect(rrect, paint);
  }
  if (granularPlane && emitOwn)
    leaveGates(surfaceSaves, surfaceCover);

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
  if (emitOwn)
  switch (node.kind) {
  case Kind::Text:
    if (inst.paragraph) {
      // Yoga skips the measure callback when both dimensions are fully
      // determined (absolute + all four insets); lay out on demand at the
      // resolved width so such text still paints. Aligned text (center/
      // end/justify) additionally must be laid out at its FINAL width —
      // lines place within the flow width, so a measure-time constraint
      // that differs from the resolved box would push them off target.
      const bool onPathRun = node.textData && node.textData->onPath;
      if (inst.measuredRev != inst.contentRev ||
          (!onPathRun && node.textData &&
           node.textData->layoutOptions.alignment !=
               sigil::weave::TextAlignment::kStart &&
           inst.measuredForWidth != bounds.width()))
        layoutText(inst, bounds.width());
      // Misprint echoes of the TEXT, under the real pass (kinetic text
      // draws its own buckets — echoes skip it by contract).
      const GlyphFx *glyphs = glyphFxOf(node);
      if (!echoesOf(node).empty() && !(glyphs && glyphs->effect)) {
        for (const Echo &e : echoesOf(node)) {
          sigil::weave::PaintStyle stamp;
          stamp.foreground.setColor4f(e.color, nullptr);
          canvas.save();
          canvas.translate(e.offset.fX, e.offset.fY);
          inst.textLayout.drawBatched(&canvas, *inst.paragraph, &stamp);
          canvas.restore();
        }
      }
      const TextPath *onPath =
          onPathRun ? &*node.textData->onPath : nullptr;
      if (glyphs && glyphs->effect) {
        paintKineticText(inst, canvas, *glyphs);
      } else if (onPath) {
        paintTextOnPath(inst, canvas, *onPath,
                        {bounds.width(), bounds.height()});
      } else if (const Material *metricMat = metricFillOf(node);
                 metricMat ||
                 (node.textData && node.textData->hasTextStroke)) {
        // Chrome type: the material's unit square mapped to the text's
        // metric band — x across the widest line, y from the first line's
        // cap top (real cap height when the face reports one) to the last
        // line's baseline.
        //
        // The override replaces the whole PaintStyle for every run, so it
        // starts as a COPY of the paragraph's own style and swaps only the
        // foreground — textFill supersedes the fill, not the underlays,
        // overlays and decorations around it (a chrome wordmark keeps its
        // cast shadow and dark keyline).
        sigil::weave::PaintStyle metric =
            inst.paragraph->spans().empty()
                ? sigil::weave::PaintStyle{}
                : inst.paragraph->spans().front().style.paint;
        metric.foreground.setShader(nullptr);
        bool havePaint = false;
        // textStroke(): a stroke pass on the glyphs, UNDER the fill. It
        // joins the style's own underlays rather than replacing them, so
        // an engraved face keeps its cast shadow.
        if (node.textData && node.textData->hasTextStroke) {
          sigil::weave::PaintLayer outline;
          outline.paint.setAntiAlias(true);
          outline.paint.setStyle(SkPaint::kStroke_Style);
          outline.paint.setStrokeWidth(node.textData->textStrokeWidth);
          outline.paint.setStrokeJoin(SkPaint::kRound_Join);
          const Fill &sf = node.textData->textStrokeFill;
          if (sf.kind == Fill::Kind::Shader && sf.shaderValue)
            outline.paint.setShader(sf.shaderValue);
          else
            outline.paint.setColor4f(sf.kind == Fill::Kind::Color
                                         ? sf.colorValue
                                         : SkColor4f{0, 0, 0, 1},
                                     nullptr);
          metric.addUnderlay(outline);
          havePaint = true;
        }
        if (!metricMat) {
          inst.textLayout.drawBatched(&canvas, *inst.paragraph,
                                      havePaint ? &metric : nullptr,
                                      liveDriveImpl(inst, node, fonts));
          break;
        }
        // Geometry-dependent materials resolve against a UNIT box here,
        // not the node's. The local matrix below already maps the
        // shader's [0,1]² onto the metric band, so uResolution baked from
        // the node's layout size would divide a second time: a
        // `linearUnit` ramp came out at t ≈ 0.003 and every glyph painted
        // the first stop, flat and silently. Material.h advertises
        // textFill and the Unit ramps as the same trick, and this is what
        // makes that true.
        PaintContext metricCtx = paintCtx;
        metricCtx.size = {1.0f, 1.0f};
        const Fill f =
            (metricMat->isAnimated() || metricMat->geometryDependent())
                ? metricMat->resolve(metricCtx)
                : metricMat->toFill();
        if (f.kind == Fill::Kind::Shader && f.shaderValue &&
            !inst.lines.empty()) {
          const sigil::weave::ShapedWord *firstFont = nullptr;
          sigil::weave::forEachPlacedGlyph(
              inst.textLayout, *inst.paragraph,
              [&](const sigil::weave::ShapedWord *font, SkGlyphID, float,
                  SkColor, SkPoint) {
                if (!firstFont)
                  firstFont = font;
              });
          float capH = 0;
          if (firstFont && firstFont->typeface) {
            SkFontMetrics fm;
            sigil::weave::makeFont(firstFont->typeface, firstFont->fontSize)
                .getMetrics(&fm);
            capH = fm.fCapHeight;
          }
          const sigil::weave::LineMetrics &first = inst.lines.front();
          if (capH <= 0)
            capH = first.ascent; // face reports none — the ascent band
          float left = first.left, right = first.right;
          for (const sigil::weave::LineMetrics &line : inst.lines) {
            left = std::min(left, line.left);
            right = std::max(right, line.right);
          }
          const float top = first.baseline - capH;
          const float bottom = inst.lines.back().baseline;
          SkMatrix map = SkMatrix::Translate(left, top);
          map.preScale(std::max(right - left, 1.0f),
                       std::max(bottom - top, 1.0f));
          metric.foreground.setShader(f.shaderValue->makeWithLocalMatrix(map));
          havePaint = true;
        } else if (f.kind == Fill::Kind::Color) {
          metric.foreground.setColor4f(f.colorValue, nullptr);
          havePaint = true;
        }
        inst.textLayout.drawBatched(&canvas, *inst.paragraph,
                                    havePaint ? &metric : nullptr,
                                    liveDriveImpl(inst, node, fonts));
      } else {
        inst.textLayout.drawBatched(&canvas, *inst.paragraph, nullptr,
                                    liveDriveImpl(inst, node, fonts));
      }
    }
    break;
  case Kind::Image:
    if (imageAssetOf(node) && !imageAssetOf(node)->frames().empty()) {
      const auto &frame = imageAssetOf(node)->frameAt(elapsed() * 1000.0);
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

  if (granularPlane && emitOwn)
    leaveGates(contentSaves, contentCover);

  // Children in stacking order (each clean static child replays its own nested
  // picture — ancestor re-records don't repaint clean subtrees).
  const size_t kidsCover = coverStack.size();
  const int kidsSaves =
      granularPlane && emitChildren ? enterGates(false, Parts::kChildren, {})
                                    : -1;
  if (emitChildren)
    for (size_t index : inst.paintOrder)
      paint(*inst.children[index], canvas);
  if (granularPlane && emitChildren)
    leaveGates(kidsSaves, kidsCover);

  if (node.clipContent)
    canvas.restore(); // decorations below stay unclipped

  // FOREGROUNDS PAINT AFTER THE CHILDREN, so they belong to the children
  // half and can never be in an own-paint bake. §15's spec originally read
  // "own paint is everything except the children"; it is not.
  if (emitChildren)
    for (size_t i = 0; i < node.foregrounds.size(); ++i)
      paintMark(node.foregrounds[i], detail::MarkSlot::Foreground, i);

  // Span-qualified stroke passes, in declaration order, in the same slot
  // as the unqualified strokes they append to. Each one paints against
  // the sub-geometry it CLAIMED, so a brush that knows nothing about
  // spans (a PathFormat, a Brush, a brush::Pattern) dresses part of a
  // boundary with no new vocabulary.
  if (emitChildren)
    paintSpanHalf(detail::StrokePass::Half::Foreground);

  leaveGates(hoistSaves, hoistCover);

  if (hasEffect)
    canvas.restore();
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
 *  break-even stability is ~0.5; promote at 0.5, keep until 0.3, and a
 *  material bound to a continuous Output (rate 0) never gets close.
 *  quantizeTime(10) at 60 FPS settles near 0.83. */
constexpr float kStablePromote = 0.5f;
constexpr float kStableKeep = 0.3f;

/** A readable, ACTIONABLE identity for a profile row: the author's own
 *  key() when there is one (that is what they will search for), else the
 *  node kind and its painted size, which is usually enough to find it. */
std::string profileLabel(const detail::Instance &inst, const SkRect &rect) {
  const detail::ElementNode &node = *inst.desc;
  const char *kind = "box";
  switch (node.kind) {
  case detail::Kind::Box: kind = "box"; break;
  case detail::Kind::Text: kind = "text"; break;
  case detail::Kind::Image: kind = "image"; break;
  case detail::Kind::Custom: kind = "custom"; break;
  default: break;
  }
  char buf[96];
  std::snprintf(buf, sizeof buf, "%s %.0fx%.0f", kind, rect.width(),
                rect.height());
  if (!node.key.empty())
    return node.key + " (" + buf + ")";
  return buf;
}

/** Scoped per-node timer. RAII because paint() has several early returns
 *  and a half-written row would be worse than no row at all. */
struct ProfileScope {
  Composer::Impl *impl = nullptr;
  size_t row = SIZE_MAX;
  double savedChildren = 0;
  std::chrono::steady_clock::time_point start;

  ProfileScope(Composer::Impl *i, const detail::Instance &inst,
               const SkRect &rect)
      : impl(i) {
    if (!impl->profileEnabled)
      return;
    row = impl->profileRows.size();
    impl->profileRows.push_back(
        Composer::NodeCost{profileLabel(inst, rect), 0, 0, impl->profDepth,
                           Composer::CacheState::Live});
    savedChildren = impl->profChildMs;
    impl->profChildMs = 0;
    ++impl->profDepth;
    start = std::chrono::steady_clock::now();
  }
  ~ProfileScope() {
    if (row == SIZE_MAX)
      return;
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

} // namespace

void Composer::Impl::paint(Instance &inst, SkCanvas &canvas) {
  const ElementNode &node = *inst.desc;
  const SkRect rect = instanceRect(inst);
  ProfileScope profileScope(this, inst, rect);

  const float opacity = std::clamp(
      inst.resolveFloat(Instance::kOpacity, node.paint.opacity), 0.0f, 1.0f);
  if (opacity <= 0.0f)
    return;

  // (Size-change invalidation for recordings — including geometry-dependent
  // materials' baked uResolution — happens in ensureLayout's
  // syncLayoutRects pass, which sees every relayout; paint() may never reach
  // a node whose ancestor replays a cached picture.)

  canvas.save();
  canvas.translate(rect.left(), rect.top());

  const float tx = inst.resolveFloat(Instance::kTx, node.paint.translateX);
  const float ty = inst.resolveFloat(Instance::kTy, node.paint.translateY);
  const float rot = inst.resolveFloat(Instance::kRotate, node.paint.rotate);
  const float scl = inst.resolveFloat(Instance::kScale, node.paint.scale);
  const float sx = inst.resolveFloat(Instance::kScaleX, node.paint.scaleX);
  const float sy = inst.resolveFloat(Instance::kScaleY, node.paint.scaleY);
  const float skx = inst.resolveFloat(Instance::kSkewX, node.paint.skewX);
  const float sky = inst.resolveFloat(Instance::kSkewY, node.paint.skewY);
  if (tx != 0 || ty != 0)
    canvas.translate(tx, ty);
  if (rot != 0 || scl != 1 || sx != 1 || sy != 1 || skx != 0 || sky != 0) {
    const SkPoint origin =
        resolveOrigin(node.paint, rect.width(), rect.height());
    canvas.translate(origin.x(), origin.y());
    if (rot != 0)
      canvas.rotate(rot);
    if (scl != 1 || sx != 1 || sy != 1)
      canvas.scale(scl * sx, scl * sy);
    if (skx != 0 || sky != 0)
      canvas.skew(std::tan(skx * 0.017453293f),
                  std::tan(sky * 0.017453293f));
    canvas.translate(-origin.x(), -origin.y());
  }

  const bool hasBackdrop =
      backdropEffectOf(node) && backdropEffectOf(node)->imageFilter();
  if (hasBackdrop) {
    // The filtered backdrop composites as a CLOSED pass clipped to the
    // node's shape — the node's own decorations and overflowing children
    // then paint unclipped above it (CSS clips the FILTER REGION to the
    // element, not the element's overflow).
    canvas.save();
    if (node.shapeFn)
      canvas.clipPath(resolveOutline(inst, {rect.width(), rect.height()}), true);
    else
      canvas.clipRRect(cornersRRect(SkRect::MakeWH(rect.width(), rect.height()),
                                    node.corners),
                       true);
    SkCanvas::SaveLayerRec rec(nullptr, nullptr,
                               backdropEffectOf(node)->imageFilter().get(), 0);
    canvas.saveLayer(rec);
    canvas.restore(); // composite the filtered backdrop through the clip
    canvas.restore(); // release the clip — content is NOT bounded by it
  }

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
      !layerEffectOf(node) &&
      !backdropEffectOf(node) &&
      !node.clipContent && !opacityLive && node.cacheMode != Cache::Texture &&
      node.cacheMode != Cache::Group; // (same reason: bakes isolate)
  const bool needsLayer =
      (opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver) &&
      !leafDirectBlend;
  if (needsLayer) {
    SkPaint layerPaint;
    layerPaint.setAlphaf(opacity);
    layerPaint.setBlendMode(node.paint.blendMode);
    // BOUNDED like the effect layer: nullptr would allocate a clip-sized
    // (often full-canvas) layer for every fading container — entrance
    // opacity ramps paid a fullscreen composite per animated group.
    const SkRect content = recordBounds(inst);
    canvas.saveLayer(&content, &layerPaint);
  }
  const SkBlendMode leafBlend =
      leafDirectBlend ? node.paint.blendMode : SkBlendMode::kSrcOver;
  const float leafOpacity = leafDirectBlend ? opacity : 1.0f;

  // The live-material resolve probe: when the node's only volatility is
  // its live material, resolve NOW — a stable shader means the cached
  // picture is still exact and simply replays (the quantized-sea rule:
  // repaint at the material's rate, not the frame rate).
  bool liveStable = false;
  inst.hasPendingLiveFill = false;
  if (inst.liveMatOnly && liveMaterialOf(node)) {
    PaintContext probe{{rect.width(), rect.height()},
                       SkPath(),
                       elapsed(),
                       hostScale,
                       ticker.active(),
                       &fonts};
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

  // §17's probe: the animated content scalars AS OF THIS FRAME. Same
  // argument as the material's — identical inputs mean identical pixels, so
  // a recording made with these numbers is still exact while they hold.
  Instance::ContentScalars scalarsNow;
  if (inst.scalarMemo) {
    // §3.6's repair in one line: every mask gate's animated numbers, as a
    // bounded per-node list. A masked node keeps the §17 memo — which is
    // exactly what the 58 trim→spans ports gave up when the only
    // element-level door closed.
    scalarsNow.gates = inst.resolveGateValues();
    if (const GlyphFx *g = glyphFxOf(node))
      scalarsNow.glyph =
          inst.resolveFloat(Instance::kGlyphProgress, g->progress);
  }
  const bool scalarsStable = inst.scalarMemo && !inst.paintDirty &&
                             (inst.picture || inst.textureImage) &&
                             scalarsNow == inst.bakedScalars;
  // "May this node keep its cached pixels?" — either nothing about it is
  // volatile, or every input it reads is memoized and provably unchanged.
  const bool memoized = inst.liveMatOnly || inst.scalarMemo;
  const bool cacheHolds = !inst.subtreeVolatile || memoized;
  // …and "are they still the RIGHT pixels?" — the two memos answer for
  // their own input and abstain on the other.
  const bool memoStale = (inst.liveMatOnly && !liveStable) ||
                         (inst.scalarMemo && !scalarsStable);

  // Automatic caching at topmost provably-static subtrees: pictures by
  // default, a rasterized image under Cache::Texture (the raster-target pixel
  // win — replaying a picture re-rasterizes, blitting doesn't).
  // COMPOSE_PROF=<ms> prints any draw above the threshold — cached-texture
  // blits, picture replays (which re-EXECUTE recorded ops on raster), and
  // live paints. Nested lines overlap (inclusive of children); any
  // unparsable value means 4ms.
  static const double kProfMs = [] {
    const char *env = getenv("COMPOSE_PROF");
    if (!env)
      return -1.0;
    const double v = atof(env);
    return v > 0.0 ? v : 4.0;
  }();
  const auto profDraw = [&](const char *what, auto &&draw) {
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
  // replay; anything else keeps replaying. See Composer::setAutoTexturePromotion.
  const SkMatrix &totalM = canvas.getTotalMatrix();
  // Upright, unmirrored, unrotated and unskewed — and MEASURED, after this
  // gate was relaxed on an argument and the measurement took it back.
  //
  // The argument was: a device-space bake concatenates the full matrix into
  // the layer and blits with the matrix reset at an integer offset, so it
  // cannot resample and must be exact at any angle; `upright` was a
  // leftover from the LOCAL bake, which really is resampled (mean |Δ|
  // 13.5/255 at ±90°). It sounded right, `TextureBakeSurvivesAQuarterTurn`
  // appeared to support it at 45°, and it is wrong. Measured on a 220 px
  // box carrying a runtime shader, promoted vs. the identical unpromoted
  // render:
  //
  //     rotate(0),  bounds inside the canvas    0 pixels differ
  //     rotate(0),  bounds overflow the canvas  0 pixels differ
  //     rotate(45), bounds inside the canvas    5 pixels differ, Δ1
  //     rotate(30), bounds inside the canvas    2 pixels differ, Δ1
  //     rotate(45), bounds overflow the canvas  1157 differ, Δ up to 40
  //
  // Two separate effects, and the mechanism explains both. A shader's local
  // coordinates come from INVERTING the CTM, and the layer's CTM differs
  // from the canvas's by an integer device translation. Inverting a
  // rotation maps that integer offset through irrational entries, so the
  // cancellation is only approximate — while an axis-aligned matrix maps it
  // through ±1 and 0 and cancels exactly. Hence 0 at 0° and ±1 LSB at every
  // other angle. Separately, a bake rect larger than the device clip gives
  // Skia a different clip to rasterize the AA edges against, and that one
  // is worth tens of levels, not one.
  //
  // The quarter-turn test does not contradict this: its pill is colour
  // fills at a size that fits, so it exercises neither effect, and
  // `Cache::Texture` is opt-in — the author accepted the trade. Automatic
  // promotion did not, and `ARefusalSaysWhy` already states the standard
  // for exactly this case: agreement to within 1 LSB "is not agreement".
  //
  // So the gate stays square, and the refusal now names Cache::Texture as
  // the remedy instead of describing the geometry, which is what a
  // dunhuang_star_chart author staring at a CONSTANT −0.42° tilt and 24.5
  // ms of a 29.9 ms frame actually needed to be told.
  const bool upright = totalM.getSkewX() == 0 && totalM.getSkewY() == 0 &&
                       totalM.getScaleX() > 0 && totalM.getScaleY() > 0 &&
                       !totalM.hasPerspective();
  // recordBounds() walks the whole subtree, and three tiers below ask for
  // it. Memoised per paint() so the walk happens at most once; lazy so a
  // node that reaches none of them never pays for it at all.
  SkRect localPaintBounds = SkRect::MakeEmpty();
  bool localBoundsDone = false;
  const auto localBoundsOf = [&]() -> const SkRect & {
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
  // decoration: a node reading `live paint · 663 ms` with no reason beside
  // it is exactly how sixteen studies shipped over the 60 FPS gate.
  //
  // ALL of them, not the first. `why` used to be a first-match `else if`
  // chain, so a node that is both volatile and clipped reported only
  // `Volatile` — and an author who fixed the volatility then met a second
  // refusal nobody had mentioned. The split bake GROWS that population
  // rather than shrinking it, so the mask lands in the same batch. `why` is
  // derived from the mask below rather than computed alongside it, which is
  // what stops the two from ever disagreeing.
  using Prom = Composer::Promotion;
  uint16_t refusals = 0;
  const auto flag = [&](Prom p) {
    refusals |= (uint16_t)(1u << (unsigned)p);
  };
  // autoPromoteEffective, not autoPromote: the backend-aware default (off on
  // GPU unless the host asked) is applied in draw(). See ComposeRuntime.h.
  const bool optedOut =
      !autoPromoteEffective || node.cacheMode != Cache::Auto;
  if (optedOut)
    flag(Prom::OptedOut);
  if (!contentStable)
    flag(Prom::Volatile);
  if (leafBlend != SkBlendMode::kSrcOver || leafOpacity < 1.0f)
    flag(Prom::Composited);
  if (layerEffectOf(node) || node.clipContent)
    flag(Prom::Filtered);
  if (inst.subtreeReadsBackdrop) // incl. this node's own backdrop()
    flag(Prom::ReadsBackdrop);
  if (rect.width() < 0.5f || rect.height() < 0.5f)
    flag(Prom::TooBig); // degenerate, not large — same "cannot bake" bucket
  if (!upright)
    flag(Prom::Transformed);

  // The PRIMARY verdict: the first refusal in the order an author should
  // address them (their own switches first, then content, then geometry).
  static constexpr Prom kRefusalOrder[] = {
      Prom::OptedOut,  Prom::Volatile,      Prom::Composited,
      Prom::Transformed, Prom::Filtered,    Prom::ReadsBackdrop,
      Prom::TooBig};
  Prom why = Prom::Cheap;
  for (Prom p : kRefusalOrder)
    if (refusals & (uint16_t)(1u << (unsigned)p)) {
      why = p;
      break;
    }

  // recordingDepth == 0, for the SAME reason the Cache::Texture device
  // path checks it (and the split bake): a device-space bake blits with
  // canvas.resetMatrix() + drawImage() at an ABSOLUTE device rect, and a
  // picture can be replayed under a different matrix than it was recorded
  // at. Recorded into an ancestor's picture and replayed at the 2x capture
  // scale, that blit draws a 1x texture at 1x coords on a 2x canvas — the
  // wordmark of y2k_chrome came out at quarter size in the wrong place,
  // 283k pixels and a peak of 0.92. The Cache::Texture path guarded this
  // from the day it shipped; automatic promotion never did.
  const bool promotable =
      why == Prom::Cheap && !liveOnly && recordingDepth == 0;
  if (!promotable)
    inst.autoTexture = false;
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
      if (inst.hotFrames < 255)
        ++inst.hotFrames;
      if (inst.hotFrames >= kPromoteFrames) {
        inst.autoTexture = true;
        inst.paintDirty = true; // force the first bake
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
    // above is really guarding, and the measurement in its comment is why
    // the guard is not about resampling.
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
    if (device.width() > 0 && device.height() > 0 &&
        area <= 16 * 1024 * 1024 && affordable) {
      // Re-bake when the recording is stale, when the device rect moved or
      // resized (which is how a transform-SCALE change arrives here), or —
      // the temporal case — when the live material has actually ticked and
      // the baked shader is no longer the one this frame resolves to.
      if (!inst.textureImage || inst.paintDirty ||
          memoStale || inst.textureBakeRect != SkRect::Make(device)) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas *lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM); // identical device geometry, offset by ints
          paintContent(inst, *lc, hostScale, leafBlend, leafOpacity);
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = SkRect::Make(device);
          inst.bakedLiveShader =
              inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue
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
        if (needsLayer)
          canvas.restore();
        canvas.restore();
        return;
      }
    }
    inst.autoTexture = false; // could not bake — fall through to the picture
    note(Prom::TooBig);
  }

  // ---- §15: the SPLIT bake ------------------------------------------------
  //
  // Volatility is declared per NODE, and a node is one verdict. So a static
  // full-canvas ground plane carrying one 264 px disc on a bound Output is
  // `subtreeVolatile`, nothing about it is cached, and the whole plane is
  // re-rasterized every frame in order to redraw the disc on top of it —
  // 34.9 ms and 14.9 ms of SELF time on two 888x666 nodes of genesis_fire,
  // both reporting "its content changes every frame" about a child.
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
  //  - clipContent and a whole-node mask() are NOT excluded, and the spec
  //    this was written from said clips were. They wrap both halves, and the
  //    phase flag skips only the CONTENT — the clip is opened and closed
  //    inside
  //    each phase, so both halves get the identical clip, in the identical
  //    device geometry, and the composition is unchanged. A GRANULAR mask
  //    is narrower still: its scope is entered and left around one paint
  //    group, inside the half that group belongs to. That correction
  //    is not academic: §15's own citation node, genesis_fire's regolith(),
  //    carries `.clip(true)` because it clips its disc to a limb outline —
  //    which is exactly WHY the disc is a child. Excluding clips would have
  //    shipped a feature that refuses the example it was written for.
  //
  // And the promotion is measured on the OWN paint alone. A split candidate
  // paints in two phases from the first eligible frame precisely so that
  // half can be timed by itself: judging it by the node's total would bake
  // a cheap ground plane because it carries an expensive child, which is
  // the same inversion that made the corpus's largest cost centre invisible
  // to the promoter before leaves were measured.
  const bool splitCandidate =
      !optedOut && !liveOnly && inst.subtreeVolatile &&
      !inst.ownContentVolatile && // the CHILDREN are what block this node
      !inst.children.empty() && !inst.ownReadsBackdrop &&
      !layerEffectOf(node) && leafBlend == SkBlendMode::kSrcOver &&
      leafOpacity >= 1.0f && rect.width() >= 0.5f && rect.height() >= 0.5f &&
      recordingDepth == 0 && !inst.transformLive &&
      // `upright` for the same measured reason promotion needs it, and it
      // is the SAME construction: an integer device offset concatenated
      // onto the node's matrix. Under rotation a shader's local coordinates
      // come back through an inverse that cannot cancel that offset
      // exactly, and the antialiased edges land ~1 LSB apart. Leaving this
      // out was the split quietly holding itself to a weaker standard than
      // the promoter beside it.
      upright;
  if (!splitCandidate)
    inst.splitBake = false;
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
          SkCanvas *lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM); // identical device geometry, offset by ints
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
          if (inst.ownRebakes < 255)
            ++inst.ownRebakes;
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
        if (inst.ownHotFrames < 255)
          ++inst.ownHotFrames;
        if (inst.ownHotFrames >= kPromoteFrames) {
          inst.splitBake = true;
          inst.ownPaintDirty = true; // force the first bake
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
    if (needsLayer)
      canvas.restore();
    canvas.restore();
    return;
  }

  // ---- §30: Cache::Group — the whole subtree, held by a VALUE memo --------
  //
  // The problem this exists for, stated as the shape it has rather than as
  // one study: MANY SMALL ROTATED PIECES FORMING ONE STATIC ASSEMBLY, each
  // piece carrying a bound entrance. kumiko_asanoha is 523 hinoki strips,
  // each an SkSL wood grain plus a BevelEmboss arris, each rotated to its
  // jig angle, each with a bound opacity and scale on a 6.4 s loop that is
  // finished by 3.4 s and then holds. Nothing in that description is
  // cacheable by the volatility rule and everything in it is cacheable for
  // three seconds in every six.
  //
  // WHY THE BAKE IS THE EASY HALF. This is the same construction the device
  // path below and whole-subtree promotion already use: paintContent into a
  // transparent layer whose canvas carries the node's exact matrix offset by
  // an INTEGER device translation, then blit with the matrix reset. The
  // children's rotations, their bevels and their mutual compositing all
  // happen INSIDE that bake at full precision — which is exactly why it is
  // pixel-safe where the per-strip Cache::Texture the study tried was not.
  // That one isolated each piece into its own layer, so every arris and
  // every abutment resolved against transparent black instead of against
  // its neighbour, and 34% of the panel's pixels moved.
  //
  // WHY THE INVALIDATION IS THE HARD HALF, AND THE WHOLE FEATURE. A group
  // may hold a bake only while it is provably not changing, and "not
  // changing" cannot be read off the volatility verdict — that verdict says
  // Volatile forever, correctly, because the bindings never disconnect. So
  // the group compares VALUES, as §17 does for one node's content scalars,
  // generalised to a whole subtree's bound transforms and opacities. Every
  // frame: gather them, compare with last frame's, and on any difference at
  // all DROP THE BAKE and paint live. A bake taken while the entrance is
  // running would freeze the entrance, and it would look completely fine in
  // any still.
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
    // optimisation — it is the difference between 2871 differing pixels and
    // zero. §25 measured it on the promoter: a bake rect LARGER than the
    // device clip hands Skia a different clip to rasterize antialiased edges
    // against, and that is worth tens of levels, not the one LSB an integer
    // offset under rotation costs. A lattice of rotated boards with bevel
    // bleed overruns its own canvas on all four sides, so this fires on
    // exactly the content the feature exists for: measured here at peak
    // channel delta 12 before the intersection and 0 after.
    //
    // Nothing visible is lost — content outside the device clip does not
    // reach the canvas either way — and `getDeviceClipBounds()` is in base
    // device coordinates, the same space the blit's resetMatrix() draws in,
    // including inside the saveLayer an opacity/blend group opens.
    SkIRect device = deviceRectOf();
    const SkIRect clip = canvas.getDeviceClipBounds();
    if (!device.intersect(clip))
      device = SkIRect::MakeEmpty();
    else
      device = SkIRect::MakeLTRB(clip.left(), clip.top(), device.right(),
                                 device.bottom());
    const bool rectStable =
        !inst.deviceRectSeen || device == inst.lastDeviceRect;
    inst.lastDeviceRect = device;
    inst.deviceRectSeen = true;

    // THE DROP. Not "re-bake": a group whose bindings are ticking is
    // ticking for a while, and re-baking each of those frames would pay the
    // bake on top of the paint. Hold the pixels only while they are right.
    if (!settled || inst.paintDirty)
      inst.textureImage.reset();

    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
    const bool affordable =
        inst.textureImage ||
        std::max(promotedBytesLast, promotedBytes) + bytes <= kPromotedBudget;
    if (settled && !inst.paintDirty && !inst.transformLive && rectStable &&
        !totalM.hasPerspective() && device.width() > 0 &&
        device.height() > 0 && area <= 16 * 1024 * 1024 && affordable) {
      const SkRect want = SkRect::Make(device);
      if (!inst.textureImage || !inst.textureDeviceSpace ||
          inst.textureBakeRect != want) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas *lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM); // identical device geometry, offset by ints
          // No leaf blend and no leaf opacity: bakes isolate, and the node's
          // own blend/opacity are applied by the saveLayer wrapping the blit
          // — which is why leafDirectBlend excludes Cache::Group.
          paintContent(inst, *lc, hostScale);
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = want;
          inst.textureScale = maxScaleOf(totalM);
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
          profileRows[profileScope.row].cacheState = Composer::CacheState::Group;
          profileRows[profileScope.row].promotion = Composer::Promotion::AskedFor;
        }
        canvas.save();
        canvas.resetMatrix();
        profDraw("group blit", [&] {
          canvas.drawImage(inst.textureImage, (float)device.left(),
                           (float)device.top(), SkSamplingOptions());
        });
        canvas.restore();
        if (needsLayer)
          canvas.restore();
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
    // interpolates two texels. Measured on rotated type — mean |Δ| 13.5
    // against 1.4 upright, and 21% less gradient across the axis whose
    // device edge falls on a half pixel. Correct scale was necessary and
    // not sufficient.
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
          SkCanvas *lc = layer->getCanvas();
          lc->translate(-(float)deviceR.left(), -(float)deviceR.top());
          lc->concat(totalM); // identical device geometry, offset by ints
          paintContent(inst, *lc, hostScale); // no leaf blend: bakes isolate
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = bakeRect;
          inst.textureScale = maxScaleOf(totalM);
          inst.bakedLiveShader =
              inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue
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
          profileRows[profileScope.row].promotion = Composer::Promotion::AskedFor;
        }
        // Identity CTM is global canvas space even inside a saveLayer (the
        // layer device carries its own origin), so an opacity/blend bake
        // still composites through the layer above.
        canvas.save();
        canvas.resetMatrix();
        profDraw("blit", [&] {
          canvas.drawImage(inst.textureImage, (float)deviceR.left(),
                           (float)deviceR.top(), SkSamplingOptions());
        });
        canvas.restore();
        if (needsLayer)
          canvas.restore();
        canvas.restore();
        return;
      }
    }
    // Rasterize at the canvas's current scale so zoomed hosts stay crisp — but
    // quantized UP to a coarse step, so a continuously changing scale (window
    // resize, pinch zoom) reuses one bake per step instead of re-rasterizing
    // every frame. Between steps the draw minifies slightly, which stays sharp.
    SkMatrix total = canvas.getTotalMatrix();
    // maxScaleOf, NOT the matrix diagonal: a ±90° node's diagonal is (0, 0)
    // and clamped to the 0.25 floor, which baked quarter-resolution type and
    // upscaled it 4× (see ComposeRuntime.h for the measured error).
    const float raw = std::clamp(maxScaleOf(total), 0.25f, 4.0f);
    static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                           1.5f, 2.0f, 3.0f, 4.0f};
    float scale = kBakeSteps[std::size(kBakeSteps) - 1];
    for (float step : kBakeSteps)
      if (step >= raw) { scale = step; break; }
    // bakeScale(): opt-in reduced raster scale — the bake evaluates fewer
    // pixels and the blit below linear-upscales through the same dst rect.
    scale = std::max(0.1f, scale * node.bakeScale);
    // Bake the full PAINT bounds, not just the box — decoration bleed and
    // overflowing children truncate otherwise (same rule as the picture
    // cull).
    const SkRect bake = localBounds;
    if (!inst.textureImage || inst.paintDirty || inst.textureScale != scale ||
        inst.textureDeviceSpace || memoStale ||
        inst.textureBakeRect != bake) {
      const int pw = std::max(1, (int)std::ceil(bake.width() * scale));
      const int ph = std::max(1, (int)std::ceil(bake.height() * scale));
      sk_sp<SkSurface> layer =
          canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
      if (!layer)
        layer = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
      layer->getCanvas()->scale(scale, scale);
      layer->getCanvas()->translate(-bake.left(), -bake.top());
      paintContent(inst, *layer->getCanvas(), scale); // no leaf blend:
      inst.textureImage = layer->makeImageSnapshot(); // bakes isolate
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
      canvas.drawImageRect(inst.textureImage, dst,
                           SkSamplingOptions(SkFilterMode::kLinear));
    });
  } else if (!liveOnly && cacheHolds && node.cacheMode != Cache::None &&
             // A zero-sized node (auto-height layout() containers, spacer
             // shims) must NOT record: SkPictureRecorder with an EMPTY cull
             // rect rejects every op, silently swallowing overflowing
             // children. Painted live instead — its children keep their own
             // per-node caches, so the cost is one traversal shim.
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
    if (!inst.picture || inst.paintDirty ||
        memoStale || inst.bakedLeafOpacity != leafOpacity ||
        inst.bakedLeafBlend != leafBlend) {
      // The cull must hold everything the subtree paints: declared
      // decoration bleed (the aero-study fix) AND children that overflow
      // the box via layout or static transforms (recordBounds) — the
      // recorder quick-rejects ops outside it.
      const SkRect cull = recordBounds(inst);
      SkPictureRecorder recorder;
      SkCanvas *rec = recorder.beginRecording(cull);
      // A picture can be replayed under a DIFFERENT matrix than it was
      // recorded at (an ancestor with a live transform keeps its picture
      // and replays it under the motion). Anything inside must therefore
      // be matrix-independent — which a device-space bake, snapped to one
      // particular device rect, is not.
      ++recordingDepth;
      paintContent(inst, *rec, hostScale, leafBlend, leafOpacity);
      --recordingDepth;
      inst.picture = recorder.finishRecordingAsPicture();
      inst.bakedLeafOpacity = leafOpacity; // a settled transition re-bakes
      inst.bakedLeafBlend = leafBlend;     // (the recording froze them in)
      inst.bakedLiveShader =
          inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue : nullptr;
      inst.bakedScalars = scalarsNow;
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    if (profileScope.row != SIZE_MAX)
      profileRows[profileScope.row].cacheState = Composer::CacheState::Picture;
    // The measurement that drives promotion. Two clock reads per candidate
    // node per frame; the thing being measured is a full rasterisation, so
    // the overhead is not close to material.
    const auto replayStart = std::chrono::steady_clock::now();
    profDraw("replay", [&] { canvas.drawPicture(inst.picture); });
    accrue(std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - replayStart)
               .count());
  } else {
    stats.nodesPainted++;
    // A LEAF never records a picture (one drawRect beats a nested
    // recording) and so was never measured — which made the single most
    // expensive object in this corpus, a full-canvas box carrying one
    // grain shader, structurally invisible to the promoter. Measure the
    // live draw too, but ONLY for a node that could actually be promoted:
    // that keeps the clock reads off the thousands of ineligible nodes.
    // Measured cost of the pair: 51 ns. The densest study in the corpus
    // paints 1664 nodes, so the ceiling is 85 us/frame — against the
    // 663 ms leaf this exists to find.
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

  if (needsLayer)
    canvas.restore();
  canvas.restore();
}

} // namespace sigil::compose
