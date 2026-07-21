// Paint phase: the volatility walk that decides what may cache, the node
// silhouette resolution, and the stacking painter with its three cache tiers
// (live paint, auto SkPicture of provably-static subtrees, and Cache::Texture
// raster bakes). See DESIGN.md "Stacking and compositing" and "Caching".

#include "ComposeRuntime.h"

#include <sigilimage/ImageAsset.h>

#include <sigilweave/Choreograph.h>
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
#include <cmath>

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Volatility & caching

bool Composer::Impl::computeVolatile(Instance &inst) {
  const ElementNode &node = *inst.desc;

  auto boundOrRunning = [&](Instance::Slot slot, const PropValue<float> &v) {
    if (std::holds_alternative<const choreograph::Output<float> *>(v))
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
  ownPaint |= boundOrRunning(Instance::kTx, node.paint.translateX);
  ownPaint |= boundOrRunning(Instance::kTy, node.paint.translateY);
  ownPaint |= boundOrRunning(Instance::kRotate, node.paint.rotate);
  ownPaint |= boundOrRunning(Instance::kScale, node.paint.scale);
  ownPaint |= boundOrRunning(Instance::kSkewX, node.paint.skewX);
  ownPaint |= boundOrRunning(Instance::kSkewY, node.paint.skewY);

  // Content volatility: what actually invalidates the node's own recording
  // (bound/lerping fills, per-frame programs, animated decorations and image
  // frames).
  bool ownContent = inst.anims[Instance::kFillLerp] &&
                    inst.anims[Instance::kFillLerp]->value.isConnected();
  if (node.paint.fill &&
      std::holds_alternative<const choreograph::Output<Fill> *>(
          *node.paint.fill))
    ownContent = true;
  if (node.liveMaterial && node.liveMaterial->isLive())
    ownContent = true; // truly live (bound/uTime) — geometry-dependent
                       // materials resolve at record time and stay cacheable
  if (node.textMetricFill && node.textMetricFill->isLive())
    ownContent = true; // animated chrome type paints per frame
  if (node.cacheMode == Cache::None)
    ownContent = true;
  for (const Decoration &d : node.backgrounds)
    ownContent |= d.animated();
  for (const Decoration &d : node.foregrounds)
    ownContent |= d.animated();
  if (node.kind == Kind::Image && node.imageAsset && node.imageAsset->animated())
    ownContent = true;
  if (node.hasTrim) { // moving trim rebuilds the painted geometry
    ownContent |= boundOrRunning(Instance::kTrimStart, node.trimStart);
    ownContent |= boundOrRunning(Instance::kTrimEnd, node.trimEnd);
    ownContent |= boundOrRunning(Instance::kTrimOffset, node.trimOffset);
  }
  if (node.glyphFx) // moving glyph progress rebuilds the glyph transforms
    ownContent |= boundOrRunning(Instance::kGlyphProgress,
                                 node.glyphFx->progress);

  bool childrenVolatile = false;
  for (auto &child : inst.children)
    childrenVolatile |= computeVolatile(*child);

  // subtreeVolatile gates the node's own caches: blocked by content volatility
  // here or ANY volatility below (children paint inside the recording,
  // transforms included) — but not by own paint volatility.
  const bool blocked = ownContent || childrenVolatile;
  if (blocked != inst.subtreeVolatile) {
    inst.subtreeVolatile = blocked;
    inst.paintDirty = true; // cacheability changed → re-record/drop
  }
  if (inst.subtreeVolatile) {
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
  for (const Echo &e : node.echoes)
    bleed = std::max(
        bleed, std::max(std::abs(e.offset.fX), std::abs(e.offset.fY)));
  if (bleed > 0)
    local.outset(bleed, bleed);
  // Routed elements paint their derive-resolved PATH, which is not bounded
  // by the layout rect (a connector's box is one thing, its wire another) —
  // the cull must hold the route plus its stroke reach.
  if ((!node.connectFrom.empty() || !node.railAnchors.empty()) &&
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
        m.preScale(scl, scl);
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
  const bool routed = !node.connectFrom.empty() || !node.railAnchors.empty();
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
  if (node.hasTrim) {
    const float off = inst.resolveFloat(Instance::kTrimOffset, node.trimOffset);
    const float s0 =
        inst.resolveFloat(Instance::kTrimStart, node.trimStart) + off;
    const float e0 = inst.resolveFloat(Instance::kTrimEnd, node.trimEnd) + off;
    sk_sp<SkPathEffect> fx;
    bool emptyWindow = false;
    if (node.trimMode == TrimMode::Wrap) {
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
            contour->getSegment(s * len, len, &stitched, true);
            contour->getSegment(0, e * len, &stitched,
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
  const bool hasEffect = node.layerEffect && node.layerEffect->imageFilter();
  if (hasEffect) {
    SkPaint effectPaint;
    effectPaint.setImageFilter(node.layerEffect->imageFilter());
    canvas.saveLayer(nullptr, &effectPaint);
  }
  const PaintContext paintCtx{{bounds.width(), bounds.height()},
                              std::move(outlinePath),
                              elapsed(),
                              contentScale,
                              ticker.active(),
                              &fonts};

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
  if (node.liveMaterial) {
    resolvedFill = node.liveMaterial->resolve(paintCtx);
  } else if (node.paint.fill) {
    Fill fill;
    if (const auto *binding =
            std::get_if<const choreograph::Output<Fill> *>(&*node.paint.fill))
      fill = (*binding)->value();
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
  if (!node.echoes.empty() && resolvedFill &&
      resolvedFill->kind != Fill::Kind::None) {
    for (const Echo &e : node.echoes) {
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
      if (inst.measuredRev != inst.contentRev ||
          (node.layoutOptions.alignment !=
               sigil::weave::TextAlignment::kStart &&
           inst.measuredForWidth != bounds.width()))
        layoutText(inst, bounds.width());
      // Misprint echoes of the TEXT, under the real pass (kinetic text
      // draws its own buckets — echoes skip it by contract).
      if (!node.echoes.empty() && !(node.glyphFx && node.glyphFx->effect)) {
        for (const Echo &e : node.echoes) {
          sigil::weave::PaintStyle stamp;
          stamp.foreground.setColor4f(e.color, nullptr);
          canvas.save();
          canvas.translate(e.offset.fX, e.offset.fY);
          inst.textLayout.drawBatched(&canvas, *inst.paragraph, &stamp);
          canvas.restore();
        }
      }
      if (node.glyphFx && node.glyphFx->effect) {
        paintKineticText(inst, canvas, *node.glyphFx);
      } else if (node.textMetricFill) {
        // Chrome type: the material's unit square mapped to the text's
        // metric band — x across the widest line, y from the first line's
        // cap top (real cap height when the face reports one) to the last
        // line's baseline.
        sigil::weave::PaintStyle metric;
        bool havePaint = false;
        const Fill f = (node.textMetricFill->isLive() ||
                        node.textMetricFill->geometryDependent())
                           ? node.textMetricFill->resolve(paintCtx)
                           : node.textMetricFill->toFill();
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
                                    havePaint ? &metric : nullptr);
      } else {
        inst.textLayout.drawBatched(&canvas, *inst.paragraph);
      }
    }
    break;
  case Kind::Image:
    if (node.imageAsset && !node.imageAsset->frames().empty()) {
      const auto &frame = node.imageAsset->frameAt(elapsed() * 1000.0);
      if (frame.image) {
        if (node.imageRegion)
          canvas.drawImageRect(frame.image, *node.imageRegion, bounds,
                               SkSamplingOptions(SkFilterMode::kLinear), nullptr,
                               SkCanvas::kStrict_SrcRectConstraint);
        else
          canvas.drawImageRect(frame.image, bounds,
                               SkSamplingOptions(SkFilterMode::kLinear));
      }
    }
    break;
  case Kind::Custom:
    if (node.program)
      node.program(canvas, paintCtx);
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

  if (hasEffect)
    canvas.restore();
}

void Composer::Impl::paint(Instance &inst, SkCanvas &canvas) {
  const ElementNode &node = *inst.desc;
  const SkRect rect = instanceRect(inst);

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
  const float skx = inst.resolveFloat(Instance::kSkewX, node.paint.skewX);
  const float sky = inst.resolveFloat(Instance::kSkewY, node.paint.skewY);
  if (tx != 0 || ty != 0)
    canvas.translate(tx, ty);
  if (rot != 0 || scl != 1 || skx != 0 || sky != 0) {
    const SkPoint origin =
        resolveOrigin(node.paint, rect.width(), rect.height());
    canvas.translate(origin.x(), origin.y());
    if (rot != 0)
      canvas.rotate(rot);
    if (scl != 1)
      canvas.scale(scl, scl);
    if (skx != 0 || sky != 0)
      canvas.skew(std::tan(skx * 0.017453293f),
                  std::tan(sky * 0.017453293f));
    canvas.translate(-origin.x(), -origin.y());
  }

  const bool hasBackdrop =
      node.backdropEffect && node.backdropEffect->imageFilter();
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
                               node.backdropEffect->imageFilter().get(), 0);
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
      std::holds_alternative<const choreograph::Output<float> *>(
          node.paint.opacity) ||
      (inst.anims[Instance::kOpacity] &&
       inst.anims[Instance::kOpacity]->value.isConnected());
  const bool leafDirectBlend =
      (node.kind == Kind::Box || node.kind == Kind::Stack) &&
      inst.children.empty() && node.backgrounds.empty() &&
      node.foregrounds.empty() && !node.layerEffect && !node.backdropEffect &&
      !node.clipContent && !opacityLive && node.cacheMode != Cache::Texture;
  const bool needsLayer =
      (opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver) &&
      !leafDirectBlend;
  if (needsLayer) {
    SkPaint layerPaint;
    layerPaint.setAlphaf(opacity);
    layerPaint.setBlendMode(node.paint.blendMode);
    canvas.saveLayer(nullptr, &layerPaint);
  }
  const SkBlendMode leafBlend =
      leafDirectBlend ? node.paint.blendMode : SkBlendMode::kSrcOver;
  const float leafOpacity = leafDirectBlend ? opacity : 1.0f;

  // Automatic caching at topmost provably-static subtrees: pictures by
  // default, a rasterized image under Cache::Texture (the raster-target pixel
  // win — replaying a picture re-rasterizes, blitting doesn't).
  if (!liveOnly && !inst.subtreeVolatile &&
      node.cacheMode == Cache::Texture && !node.backdropEffect) {
    // Rasterize at the canvas's current scale so zoomed hosts stay crisp — but
    // quantized UP to a coarse step, so a continuously changing scale (window
    // resize, pinch zoom) reuses one bake per step instead of re-rasterizing
    // every frame. Between steps the draw minifies slightly, which stays sharp.
    SkMatrix total = canvas.getTotalMatrix();
    const float raw = std::clamp(
        std::max(std::abs(total.getScaleX()), std::abs(total.getScaleY())),
        0.25f, 4.0f);
    static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                           1.5f, 2.0f, 3.0f, 4.0f};
    float scale = kBakeSteps[std::size(kBakeSteps) - 1];
    for (float step : kBakeSteps)
      if (step >= raw) { scale = step; break; }
    // Bake the full PAINT bounds, not just the box — decoration bleed and
    // overflowing children truncate otherwise (same rule as the picture
    // cull).
    const SkRect bake = recordBounds(inst);
    if (!inst.textureImage || inst.paintDirty || inst.textureScale != scale ||
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
      inst.textureBakeRect = bake;
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    canvas.drawImageRect(inst.textureImage, bake,
                         SkSamplingOptions(SkFilterMode::kLinear));
  } else if (!liveOnly && !inst.subtreeVolatile &&
             node.cacheMode != Cache::None &&
             // A zero-sized node (auto-height layout() containers, spacer
             // shims) must NOT record: SkPictureRecorder with an EMPTY cull
             // rect rejects every op, silently swallowing overflowing
             // children. Painted live instead — its children keep their own
             // per-node caches, so the cost is one traversal shim.
             rect.width() >= 0.5f && rect.height() >= 0.5f &&
             (node.cacheMode == Cache::Picture || !inst.children.empty() ||
              node.kind == Kind::Text || node.kind == Kind::Custom ||
              !node.backgrounds.empty() || !node.foregrounds.empty() ||
              node.layerEffect)) {
    // (Childless Image leaves deliberately absent: one drawImageRect is
    // cheaper than a nested picture indirection — tile maps stay flat inside
    // their chunk's recording. Cache::Picture opts back in.)
    if (!inst.picture || inst.paintDirty ||
        inst.bakedLeafOpacity != leafOpacity ||
        inst.bakedLeafBlend != leafBlend) {
      // The cull must hold everything the subtree paints: declared
      // decoration bleed (the aero-study fix) AND children that overflow
      // the box via layout or static transforms (recordBounds) — the
      // recorder quick-rejects ops outside it.
      const SkRect cull = recordBounds(inst);
      SkPictureRecorder recorder;
      SkCanvas *rec = recorder.beginRecording(cull);
      paintContent(inst, *rec, hostScale, leafBlend, leafOpacity);
      inst.picture = recorder.finishRecordingAsPicture();
      inst.bakedLeafOpacity = leafOpacity; // a settled transition re-bakes
      inst.bakedLeafBlend = leafBlend;     // (the recording froze them in)
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    canvas.drawPicture(inst.picture);
  } else {
    stats.nodesPainted++;
    paintContent(inst, canvas, hostScale, leafBlend, leafOpacity);
    inst.paintDirty = false;
  }

  if (needsLayer)
    canvas.restore();
  canvas.restore();
}

} // namespace sigil::compose
