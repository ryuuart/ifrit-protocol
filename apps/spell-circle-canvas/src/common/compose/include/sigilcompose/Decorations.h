#pragma once

/** @file
 * SigilCompose decoration primitives — the extension over the kernel's
 * Decoration seam (see API.md "Primitives, not a zoo"). Concrete
 * treatments are data over three general primitives, each a thin value
 * struct over machinery Skia ships:
 *
 *  - PathFormat: format any stroke along the node's outline — width and
 *    paint plus an optional dash pattern, a stamped path repeated along
 *    the contour (vines, chains), or any custom SkPathEffect.
 *  - Slice: map an image onto the box through a lattice
 *    (drawImageLattice: N-patch; nine-slice is the 3×3 case).
 *  - ContourWalk: walk the outline by arc length and run a draw program
 *    at each sample, the canvas pre-positioned at the sample with x
 *    along the tangent — the general procedural border.
 *
 * All are DecorationScheme values: attach with .background()/.foreground().
 */

#include "sigilcompose/Compose.h"

#include <sigilimage/ImageAsset.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <include/effects/Sk1DPathEffect.h>
#include <include/effects/SkDashPathEffect.h>

#include <cmath>
#include <optional>

namespace sigil::compose {

/** Stroke the node's outline, formatted by data: dashes, a stamped
 *  path, or a custom effect (composable — dash of a stamp is legal in
 *  Skia by chaining effects yourself via `effect`). */
struct PathFormat {
  float width = 1.0f;
  Fill strokeFill = Fill::color({1, 1, 1, 1});

  /** Dash on/off intervals in px (empty → solid). */
  std::vector<SkScalar> dashIntervals;
  float dashPhase = 0.0f;

  /** Stamp this path repeatedly along the contour (advance px apart),
   *  rotated to follow it — vines, chains, ornament runs. */
  SkPath stampPath;
  float stampAdvance = 0.0f;

  /** Escape hatch: any SkPathEffect; overrides dash/stamp when set. */
  sk_sp<SkPathEffect> effect;

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(width);
    if (strokeFill.kind == Fill::Kind::Color)
      p.setColor4f(strokeFill.colorValue, nullptr);
    else if (strokeFill.kind == Fill::Kind::Shader)
      p.setShader(strokeFill.shaderValue);

    sk_sp<SkPathEffect> chosen = effect;
    if (!chosen && stampAdvance > 0 && !stampPath.isEmpty())
      chosen = SkPath1DPathEffect::Make(stampPath, stampAdvance, dashPhase,
                                        SkPath1DPathEffect::kRotate_Style);
    if (!chosen && !dashIntervals.empty())
      chosen = SkDashPathEffect::Make(
          SkSpan(dashIntervals.data(), dashIntervals.size()), dashPhase);
    p.setPathEffect(std::move(chosen));
    canvas.drawPath(ctx.outline, p);
  }
};

/** Image-onto-box through a lattice (per-cell stretch); nine-slice is
 *  xDivs/yDivs of size 2. Empty divs stretch the whole image. */
struct Slice {
  std::shared_ptr<const sigil::image::ImageAsset> asset;
  std::vector<int> xDivs;
  std::vector<int> yDivs;

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if (!asset || asset->frames().empty())
      return;
    const sk_sp<SkImage> &img = asset->frames().front().image;
    if (!img)
      return;
    const SkRect dst = SkRect::MakeWH(ctx.size.width(), ctx.size.height());
    if (xDivs.empty() && yDivs.empty()) {
      canvas.drawImageRect(img, dst,
                           SkSamplingOptions(SkFilterMode::kLinear));
      return;
    }
    SkCanvas::Lattice lattice{};
    lattice.fXDivs = xDivs.data();
    lattice.fYDivs = yDivs.data();
    lattice.fXCount = (int)xDivs.size();
    lattice.fYCount = (int)yDivs.size();
    lattice.fBounds = nullptr;
    lattice.fRectTypes = nullptr;
    lattice.fColors = nullptr;
    canvas.drawImageLattice(img.get(), lattice, dst,
                            SkFilterMode::kLinear, nullptr);
  }
};

/** One arc-length sample along the outline. */
struct PathSample {
  SkPoint position;
  SkVector tangent;
  float distance = 0.0f;
  float fraction = 0.0f; // 0..1 within its contour
};

/** Walk the outline at `spacing` px intervals; at every sample the
 *  canvas is translated to the sample and rotated so +x follows the
 *  tangent. The general procedural border. Two bodies, composable:
 *
 *  - `draw`: a raw program per sample (per-step images, SkSL, nested
 *    drawing). Set `animatedWalk` when it depends on
 *    ctx.elapsedSeconds (declared volatility).
 *  - `stamp`: a full element subtree — laid out and recorded ONCE via
 *    snapshot() (at intrinsic size, its own decorations and all), then
 *    replayed per sample centered on the contour. Recursion is closed:
 *    the stamp's decorations may walk their own contours. With
 *    `animatedWalk` the stamp re-records each paint, sampling any
 *    bound ch::Outputs at their current values.
 *
 *  When both are set, `stamp` replays first, then `draw` runs on top. */
struct ContourWalk {
  float spacing = 16.0f;
  std::function<void(SkCanvas &, const PathSample &, const PaintContext &)>
      draw;
  bool animatedWalk = false;

  std::optional<Element> stamp;

  bool animated() const { return animatedWalk; }

  void paint(SkCanvas &canvas, const PaintContext &ctx) const {
    if ((!draw && !stamp) || spacing <= 0)
      return;

    // Bake (or re-bake) the stamp element: once per description for
    // static stamps, once per paint for animated ones.
    if (stamp && ctx.fonts && (!stampCache->picture || animatedWalk))
      stampCache->picture = snapshot(*stamp, *ctx.fonts);
    const sk_sp<SkPicture> &stampPicture = stampCache->picture;

    SkContourMeasureIter iter(ctx.outline, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float length = contour->length();
      for (float d = 0; d < length; d += spacing) {
        SkPoint pos;
        SkVector tan;
        if (!contour->getPosTan(d, &pos, &tan))
          continue;
        PathSample sample{pos, tan, d, length > 0 ? d / length : 0};
        canvas.save();
        canvas.translate(pos.x(), pos.y());
        canvas.rotate(std::atan2(tan.y(), tan.x()) * 180.0f / 3.14159265f);
        if (stampPicture) {
          const SkRect cull = stampPicture->cullRect();
          canvas.save();
          canvas.translate(-cull.width() / 2, -cull.height() / 2);
          canvas.drawPicture(stampPicture);
          canvas.restore();
        }
        if (draw)
          draw(canvas, sample, ctx);
        canvas.restore();
      }
    }
  }

  /** @private Replay cache, shared across the by-value copies
   *  Decoration makes; paint() is const, the bake is memoization.
   *  (Public to keep ContourWalk an aggregate for designated init.) */
  struct StampCache {
    sk_sp<SkPicture> picture;
  };
  std::shared_ptr<StampCache> stampCache = std::make_shared<StampCache>();
};

} // namespace sigil::compose
