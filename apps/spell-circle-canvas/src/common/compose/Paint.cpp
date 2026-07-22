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

} // namespace

// ---------------------------------------------------------------------------
// Volatility & caching

bool Composer::Impl::computeVolatile(Instance &inst) {
  const ElementNode &node = *inst.desc;

  auto boundOrRunning = [&](Instance::Slot slot, const PropValue<float> &v) {
    if (v.binding())
      return true;
    return inst.anims[slot] && inst.anims[slot]->value.isConnected();
  };
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
  const bool liveMat = nodeLiveMat && nodeLiveMat->isLive();
  if (liveMat)
    ownContent = true; // truly live (bound/uTime) — geometry-dependent
                       // materials resolve at record time and stay cacheable
  if (const Material *mf = metricFillOf(node); mf && mf->isLive())
    ownContent = true; // animated chrome type paints per frame
  if (node.cacheMode == Cache::None)
    ownContent = true;
  for (const Decoration &d : node.backgrounds)
    ownContent |= d.animated();
  for (const Decoration &d : node.foregrounds)
    ownContent |= d.animated();
  if (node.fxData)
    for (const Decoration &d : node.fxData->overlays)
      ownContent |= d.animated();
  if (node.kind == Kind::Image && imageAssetOf(node) &&
      imageAssetOf(node)->animated())
    ownContent = true;
  // The MEMOIZABLE scalars, tracked apart from the rest of ownContent: each
  // rebuilds the painted geometry when it moves, and each is a number that
  // can sit still for a long time inside a running motion (§17).
  bool scalarContent = false;
  if (node.hasTrim()) { // moving trim rebuilds the painted geometry
    scalarContent |= boundOrRunning(Instance::kTrimStart,
                                    node.fxData->trimStart);
    scalarContent |= boundOrRunning(Instance::kTrimEnd, node.fxData->trimEnd);
    scalarContent |= boundOrRunning(Instance::kTrimOffset,
                                    node.fxData->trimOffset);
  }
  if (node.fxData && node.fxData->hasWipe) // a moving reveal re-clips
    scalarContent |= boundOrRunning(Instance::kWipe, node.fxData->wipeFraction);
  if (const GlyphFx *g = glyphFxOf(node)) // moving glyph progress rebuilds
    scalarContent |= boundOrRunning(Instance::kGlyphProgress, g->progress);
  ownContent |= scalarContent;
  if (node.textData && node.textData->driveValue)
    ownContent = true; // VariationDrive repaints per frame (no reshape)

  bool childrenVolatile = false;
  bool childReadsBackdrop = false;
  for (auto &child : inst.children) {
    childrenVolatile |= computeVolatile(*child);
    childReadsBackdrop |= child->subtreeReadsBackdrop;
  }
  // Does anything here composite against what is ALREADY on the canvas? If
  // so the subtree can never be baked into a transparent layer and blitted
  // back — a kMultiply child would resolve against transparent black. This
  // is the trap automatic promotion has to avoid, and it is invisible in a
  // still frame of the common case, so it is computed rather than assumed.
  inst.subtreeReadsBackdrop =
      childReadsBackdrop || backdropEffectOf(node) != nullptr ||
      node.paint.blendMode != SkBlendMode::kSrcOver;

  // subtreeVolatile gates the node's own caches: blocked by content volatility
  // here or ANY volatility below (children paint inside the recording,
  // transforms included) — but not by own paint volatility.
  const bool blocked = ownContent || childrenVolatile;
  // The resolve-memo carve-out: volatility caused SOLELY by a live
  // material keeps its picture — paint() replays it while resolve() stays
  // stable and re-records only when the shader actually changes.
  const Material *mfLive = metricFillOf(node);
  inst.liveMatOnly = liveMat && ownContent && !nonLiveMatContent &&
                     !(mfLive && mfLive->isLive()) &&
                     node.cacheMode != Cache::None && !childrenVolatile;
  // (decoration/image/trim/glyph volatility all set ownContent through
  // nonLiveMatContent's snapshot point or below — re-derive precisely:)
  if (inst.liveMatOnly) {
    bool other = nonLiveMatContent;
    for (const Decoration &d : node.backgrounds)
      other |= d.animated();
    for (const Decoration &d : node.foregrounds)
      other |= d.animated();
    if (node.fxData)
      for (const Decoration &d : node.fxData->overlays)
        other |= d.animated();
    if (node.kind == Kind::Image && imageAssetOf(node) &&
        imageAssetOf(node)->animated())
      other = true;
    if (node.fxData && node.fxData->hasWipe)
      other |= boundOrRunning(Instance::kWipe, node.fxData->wipeFraction);
    if (node.hasTrim())
      other |= boundOrRunning(Instance::kTrimStart, node.fxData->trimStart) ||
               boundOrRunning(Instance::kTrimEnd, node.fxData->trimEnd) ||
               boundOrRunning(Instance::kTrimOffset, node.fxData->trimOffset);
    if (const GlyphFx *g = glyphFxOf(node))
      other |= boundOrRunning(Instance::kGlyphProgress, g->progress);
    if (node.textData && node.textData->driveValue)
      other = true;
    if (other)
      inst.liveMatOnly = false;
  }
  // §17: the same carve-out for animated SCALARS. A node whose content
  // volatility is entirely trim/wipe/glyph keeps its recording and
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
      other |= d.animated();
    for (const Decoration &d : node.foregrounds)
      other |= d.animated();
    if (node.fxData)
      for (const Decoration &d : node.fxData->overlays)
        other |= d.animated();
    if (node.kind == Kind::Image && imageAssetOf(node) &&
        imageAssetOf(node)->animated())
      other = true;
    if (const Material *mf = metricFillOf(node); mf && mf->isLive())
      other = true;
    if (node.textData && node.textData->driveValue)
      other = true;
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
SkRect Composer::Impl::recordBounds(Instance &inst) {
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
                                  float leafOpacity) {
  const ElementNode &node = *inst.desc;
  const SkRect bounds = SkRect::MakeWH(YGNodeLayoutGetWidth(inst.yoga),
                                       YGNodeLayoutGetHeight(inst.yoga));
  const SkRRect rrect = cornersRRect(bounds, node.corners);

  // The node's shape: routed connector/rail path, custom outline(), or the
  // corner-rounded box.
  const bool routed = node.deriveData &&
                      (!node.deriveData->connectFrom.empty() ||
                       !node.deriveData->railAnchors.empty());
  const bool customShape = node.shapeFn && !routed;
  SkPath outlinePath;
  if (routed) {
    outlinePath = inst.connectorPath; // derive phase routed it
  } else if (customShape) {
    outlinePath = resolveOutline(inst, {bounds.width(), bounds.height()});
  } else {
    SkPathBuilder outlineBuilder;
    outlineBuilder.addRRect(rrect);
    outlinePath = outlineBuilder.detach();
  }

  // (clip() applies AFTER the decorations' outline is settled — see below:
  // decorations dress the outline and stay unclipped; fill/content/children
  // clip. The clip keeps the UNtrimmed shape — trim is a paint reveal.)
  const SkPath clipShape = outlinePath;

  // Trim Path: reveal [start, end] of the outline's arc length. Applied
  // before PaintContext is built, so the fill and every outline-following
  // decoration trace the trimmed path (draw-on borders, self-drawing wires).
  bool trimmed = false;
  if (node.hasTrim()) {
    const float off =
        inst.resolveFloat(Instance::kTrimOffset, node.fxData->trimOffset);
    const float s0 =
        inst.resolveFloat(Instance::kTrimStart, node.fxData->trimStart) + off;
    const float e0 =
        inst.resolveFloat(Instance::kTrimEnd, node.fxData->trimEnd) + off;
    sk_sp<SkPathEffect> fx;
    bool emptyWindow = false;
    if (node.fxData->trimMode == TrimMode::Wrap) {
      // The outline is a CYCLE: fractions wrap mod 1, and a window that
      // crosses the seam keeps both pieces (inverted trim = the complement
      // of the gap) — the marching-ants / orbiting-comet mode.
      const float span = e0 - s0;
      if (span <= 0.0f) {
        emptyWindow = true;
      } else if (span < 1.0f) {
        const float s = s0 - std::floor(s0);
        const float e = e0 - std::floor(e0);
        if (s < e) {
          fx = SkTrimPathEffect::Make(s, e);
        } else if (s > e) {
          // Seam-crossing window: stitch [s,1]+[0,e] into ONE contour per
          // source contour (segment 2 appended without a moveTo — its
          // start coincides with segment 1's end at the seam). Two pieces
          // would double-hit round caps and additive halo brushes there.
          SkPathBuilder stitched;
          SkContourMeasureIter iter(outlinePath, false);
          while (sk_sp<SkContourMeasure> contour = iter.next()) {
            const float len = contour->length();
            // Closed contours stitch into ONE run through the seam; OPEN
            // contours have no seam — joining the pieces would invent a
            // straight chord from the end back to the start.
            // getSegment's bool says whether it appended anything; the
            // emptiness check below already covers both calls, so the
            // per-call answer is genuinely not interesting here.
            (void)contour->getSegment(s * len, len, &stitched, true);
            (void)contour->getSegment(0, e * len, &stitched,
                                      !contour->isClosed());
          }
          SkPath stitchedPath = stitched.detach();
          if (!stitchedPath.isEmpty()) {
            outlinePath = std::move(stitchedPath);
            trimmed = true;
          }
        }
        // s == e with 0 < span < 1 is float noise on a full wrap; keep all.
      } // span >= 1: the window covers the whole cycle — no trim.
    } else {
      float s = std::clamp(s0, 0.0f, 1.0f);
      float e = std::clamp(e0, 0.0f, 1.0f);
      if (e < s)
        std::swap(s, e);
      if (s > 0.0f || e < 1.0f)
        fx = SkTrimPathEffect::Make(s, e);
    }
    if (emptyWindow) {
      outlinePath.reset();
      trimmed = true;
    } else if (fx) {
      SkPathBuilder dst;
      SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
      if (fx->filterPath(&dst, outlinePath, &rec)) {
        outlinePath = dst.detach();
        trimmed = true;
      }
    }
  }

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
  const PaintContext paintCtx{{bounds.width(), bounds.height()},
                              std::move(outlinePath),
                              elapsed(),
                              contentScale,
                              ticker.active(),
                              &fonts};

  // wipe(): a directional reveal, clipping the WHOLE node — decorations
  // included, because a reveal reveals. Unlike clipContent this is not a
  // containment rule, so it wraps everything below and restores at the
  // end. `trim()` cannot express it (it walks the perimeter) and
  // scaleX/scaleY squash rather than reveal.
  bool wiped = false;
  // A container of absolutely-positioned children measures ZERO, and a
  // half-plane built from an empty box is empty — so `.wipe(90, 1.0)`, a
  // FULL reveal, hid an entire subtree. A reveal at 1 must never hide
  // anything, and an empty box has nothing to reveal along.
  if (node.fxData && node.fxData->hasWipe && !bounds.isEmpty()) {
    const float t = std::clamp(
        inst.resolveFloat(Instance::kWipe, node.fxData->wipeFraction), 0.0f,
        1.0f);
    const float rad = node.fxData->wipeAngleDeg * SK_FloatPI / 180.0f;
    const float c = std::cos(rad), s = std::sin(rad);
    const SkPoint mid{bounds.centerX(), bounds.centerY()};
    // Half-extent along the wipe direction, and a span wide enough to
    // cover the box at any angle.
    const float reach =
        0.5f * (std::abs(bounds.width() * c) + std::abs(bounds.height() * s));
    const float wide =
        SkPoint{bounds.width(), bounds.height()}.length() * 0.5f + 1.0f;
    const float edge = -reach + 2.0f * reach * t;
    // The revealed half-plane, built in the wipe's own frame and rotated
    // into place: {p : (p - mid)·d <= edge}.
    SkPathBuilder b;
    b.addRect(SkRect::MakeLTRB(-reach - 1.0f, -wide, edge, wide));
    SkMatrix m = SkMatrix::RotateDeg(node.fxData->wipeAngleDeg);
    m.postTranslate(mid.x(), mid.y());
    canvas.save();
    canvas.clipPath(b.detach().makeTransform(m), true);
    wiped = true;
  }

  // Background decorations paint beneath the fill (the CSS box-shadow
  // ordering): shadow/pattern layers first, then the surface. Decorations
  // are NEVER clipped — they dress the outline (shadows keep their
  // reach, outer strokes survive on clipped nodes; the aero-study fix).
  for (const Decoration &decoration : node.backgrounds)
    decoration.paint(canvas, paintCtx);

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
  if (const Material *live = liveMaterialOf(node)) {
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
        canvas.drawPath(paintCtx.outline, stamp);
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
      canvas.drawPath(paintCtx.outline, paint);
    else
      canvas.drawRRect(rrect, paint);
  }

  // Overlays: over the fill, under the content and children. The slot a
  // textured button needs so its own hazard stripe does not grey out its
  // label. Unclipped like the other decorations — they dress the outline.
  if (node.fxData)
    for (const Decoration &decoration : node.fxData->overlays)
      decoration.paint(canvas, paintCtx);

  // Content
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
            (metricMat->isLive() || metricMat->geometryDependent())
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

  // Children in stacking order (each clean static child replays its own nested
  // picture — ancestor re-records don't repaint clean subtrees).
  for (size_t index : inst.paintOrder)
    paint(*inst.children[index], canvas);

  if (node.clipContent)
    canvas.restore(); // decorations below stay unclipped

  for (const Decoration &decoration : node.foregrounds)
    decoration.paint(canvas, paintCtx);

  if (wiped)
    canvas.restore();

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
      node.foregrounds.empty() &&
      (!node.fxData || (node.fxData->overlays.empty() &&
                        !node.fxData->hasWipe)) &&
      !layerEffectOf(node) &&
      !backdropEffectOf(node) &&
      !node.clipContent && !opacityLive && node.cacheMode != Cache::Texture;
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
    if (node.hasTrim()) {
      scalarsNow.trimStart =
          inst.resolveFloat(Instance::kTrimStart, node.fxData->trimStart);
      scalarsNow.trimEnd =
          inst.resolveFloat(Instance::kTrimEnd, node.fxData->trimEnd);
      scalarsNow.trimOffset =
          inst.resolveFloat(Instance::kTrimOffset, node.fxData->trimOffset);
    }
    if (node.fxData && node.fxData->hasWipe)
      scalarsNow.wipe =
          inst.resolveFloat(Instance::kWipe, node.fxData->wipeFraction);
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
  // Upright, unmirrored, unrotated and unskewed. A ±90° turn IS axis
  // aligned in the loose sense and is deliberately excluded here: the
  // scale then lives in the skew terms, and every bake path that mistook
  // the diagonal for the scale resampled it (see maxScaleOf). Reading
  // BOTH the skews and the signs of the diagonal is what keeps quarter
  // turns, flips and 180° rotations out.
  const bool upright = totalM.getSkewX() == 0 && totalM.getSkewY() == 0 &&
                       totalM.getScaleX() > 0 && totalM.getScaleY() > 0 &&
                       !totalM.hasPerspective();
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
  using Prom = Composer::Promotion;
  Prom why = Prom::Cheap;
  if (!autoPromote || node.cacheMode != Cache::Auto)
    why = Prom::OptedOut;
  else if (!contentStable)
    why = Prom::Volatile;
  else if (leafBlend != SkBlendMode::kSrcOver || leafOpacity < 1.0f)
    why = Prom::Composited;
  else if (!upright)
    why = Prom::Transformed;
  else if (backdropEffectOf(node) || layerEffectOf(node) || node.clipContent ||
           inst.subtreeReadsBackdrop)
    why = Prom::Filtered;
  else if (rect.width() < 0.5f || rect.height() < 0.5f)
    why = Prom::TooBig; // degenerate, not large — same "cannot bake" bucket

  const bool promotable = why == Prom::Cheap && !liveOnly;
  if (!promotable)
    inst.autoTexture = false;
  const auto note = [&](Prom p) {
    if (profileScope.row != SIZE_MAX)
      profileRows[profileScope.row].promotion = p;
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
    // rasterisation, so this is a literal copy of the replay's pixels.
    const SkRect local = recordBounds(inst);
    SkRect deviceF = totalM.mapRect(local);
    const SkIRect device = SkIRect::MakeLTRB(
        (int)std::floor(deviceF.left()), (int)std::floor(deviceF.top()),
        (int)std::ceil(deviceF.right()), (int)std::ceil(deviceF.bottom()));
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
    const SkRect localBounds = recordBounds(inst);
    bool deviceRectStable = false;
    SkIRect deviceR = SkIRect::MakeEmpty();
    if (recordingDepth == 0) {
      const SkRect deviceF = totalM.mapRect(localBounds);
      deviceR = SkIRect::MakeLTRB(
          (int)std::floor(deviceF.left()), (int)std::floor(deviceF.top()),
          (int)std::ceil(deviceF.right()), (int)std::ceil(deviceF.bottom()));
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
