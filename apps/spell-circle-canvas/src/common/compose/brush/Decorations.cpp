/** @file
 * The concrete decorations that plug the Decoration seam: path formatting,
 * shadows, lattice slices, contour walks, washes and borders.
 */

#include <include/core/SkClipOp.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPathBuilder.h>
#include <include/effects/Sk1DPathEffect.h>
#include <include/effects/SkDashPathEffect.h>
#include <sigilcompose/brush/Decorations.h>

#include <cmath>

namespace sigil::compose {

void PathFormat::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  SkPaint p;
  p.setAntiAlias(antiAlias);
  p.setStyle(SkPaint::kStroke_Style);
  // Inner/Outer: clip to the shape's side and stroke DOUBLE width — the
  // visible half lands entirely on the kept side (the standard trick).
  const bool aligned = align != Align::Center;
  p.setStrokeWidth(aligned ? width * 2 : width);
  p.setStrokeCap(cap);
  p.setStrokeJoin(join);
  const Fill stroke =
      strokeMaterial ? resolveFill(*strokeMaterial, ctx) : strokeFill;
  if (stroke.kind == Fill::Kind::Color)
    p.setColor4f(stroke.colorValue, nullptr);
  else if (stroke.kind == Fill::Kind::Shader)
    p.setShader(stroke.shaderValue);

  sk_sp<SkPathEffect> chosen = effect;
  if (!chosen && stampAdvance > 0 && !stampPath.isEmpty())
    chosen = SkPath1DPathEffect::Make(stampPath, stampAdvance, phase(),
                                      SkPath1DPathEffect::kRotate_Style);
  if (!chosen && !dashIntervals.empty())
    chosen = SkDashPathEffect::Make(
        SkSpan(dashIntervals.data(), dashIntervals.size()), phase());
  p.setPathEffect(std::move(chosen));

  // The decoration's own trim window (wrapping; the marching sliver).
  const SkPath* drawn = &ctx.outline;
  SkPath windowed;
  const float off =
      trimPhase ? motion::resolveFloatAt(nullptr, *trimPhase) : trimOffset;
  const float s0 = trimStart + off, e0 = trimEnd + off;
  const float span = e0 - s0;
  if (span > 0.0f && span < 1.0f) {
    const float s = s0 - std::floor(s0);
    const float e = e0 - std::floor(e0);
    SkPathBuilder window;
    SkContourMeasureIter iter(ctx.outline, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      if (s < e) {
        (void)contour->getSegment(s * len, e * len, &window, true);
      } else if (s > e) {
        (void)contour->getSegment(s * len, len, &window, true);
        // A closed contour has a real seam, so joining both pieces avoids
        // doubled caps there. An open route has no seam: continuing without
        // a moveTo would invent a straight chord from its end to its start.
        (void)contour->getSegment(0, e * len, &window, !contour->isClosed());
      }
    }
    windowed = window.detach();
    if (!windowed.isEmpty()) drawn = &windowed;
  } else if (span <= 0.0f) {
    return;  // empty window — nothing to stroke
  }

  if (aligned) {
    canvas.save();
    canvas.clipPath(
        ctx.outline,
        align == Align::Inner ? SkClipOp::kIntersect : SkClipOp::kDifference,
        true);
    canvas.drawPath(*drawn, p);
    canvas.restore();
  } else {
    canvas.drawPath(*drawn, p);
  }
}

void Slice::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  if (!asset || asset->frames().empty()) return;
  sk_sp<SkImage> img = asset->frames().front().image;
  if (!img) return;
  const SkRect dst = SkRect::MakeWH(ctx.size.width(), ctx.size.height());
  skia::draw::drawLattice(canvas, *gpuCache, std::move(img), xDivs, yDivs, dst,
                          filter, density);
}

void ContourWalk::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  if ((!draw && !stamp && !stampAt) || spacing <= 0) return;

  // Bake (or re-bake) the stamp element: once per description for
  // static stamps, once per paint for animated ones.
  const void* stampNode = stamp ? stamp->node().get() : nullptr;
  if (stampCache->bakedFor != stampNode) {
    stampCache->picture.reset();
    stampCache->bakedFor = stampNode;
  }
  if (stamp && ctx.fonts && (!stampCache->picture || animatedWalk))
    stampCache->picture = snapshot(*stamp, *ctx.fonts);
  const sk_sp<SkPicture>& stampPicture = stampCache->picture;

  SkContourMeasureIter iter(ctx.outline, false);
  size_t index = 0;  // runs across contours — the sequence's position
  while (sk_sp<SkContourMeasure> contour = iter.next()) {
    const float length = contour->length();
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float d = 0; d < length; d += spacing) {
      SkPoint pos;
      SkVector tan;
      if (!contour->getPosTan(d, &pos, &tan)) continue;
      PathSample sample{pos, tan, d, length > 0 ? d / length : 0};
      // This sample's OWN art (stampAt): baked per call, uncached — see
      // the field note. The shell box is needed because snapshot() sizes
      // by the root's CHILDREN and ignores the root's own dimensions.
      sk_sp<SkPicture> own;
      if (stampAt && ctx.fonts)
        if (std::optional<Element> e = stampAt(sample, index))
          own = snapshot(box().child(std::move(*e)), *ctx.fonts);
      const sk_sp<SkPicture>& art = own ? own : stampPicture;
      canvas.save();
      canvas.translate(pos.x(), pos.y());
      canvas.rotate(std::atan2(tan.y(), tan.x()) * 180.0f / 3.14159265f);
      if (art) {
        const SkRect cull = art->cullRect();
        canvas.save();
        canvas.translate(-cull.width() / 2, -cull.height() / 2);
        canvas.drawPicture(art);
        canvas.restore();
      }
      if (draw) draw(canvas, sample, ctx);
      canvas.restore();
      ++index;
    }
  }
}

void Wash::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  const float a = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
  if (a <= 0.0f) return;
  const Fill fill = resolveFill(material, ctx);
  SkPaint p;
  p.setAntiAlias(true);
  p.setBlendMode(blend);
  if (fill.kind == Fill::Kind::Color) {
    SkColor4f c = fill.colorValue;
    c.fA *= a;
    p.setColor4f(c, nullptr);
  } else if (fill.kind == Fill::Kind::Shader) {
    p.setShader(fill.shaderValue);
    p.setAlphaf(a);
  } else {
    return;
  }
  canvas.drawPath(ctx.outline, p);
}

void Border::paint(SkCanvas& canvas, const PaintContext& ctx) const {
  // `width` is the RUN's width, and Weighted mode has a second width for
  // the corners — so in that mode width == 0 means "corners only, no runs
  // between them", which is a real frame. The bail-out therefore tests
  // the HEAVIEST of the two widths, or weightedCorners(0, w, …) would
  // draw nothing at all.
  const float heaviest =
      mode == Mode::Weighted ? std::max(width, cornerWidth) : width;
  if (ctx.outline.isEmpty() || heaviest <= 0) return;
  const SkPath base =
      inset != 0 ? lines::insetOutline(ctx.outline, inset) : ctx.outline;

  auto strokeWith = [&](const SkPath& path, float w) {
    if (path.isEmpty() || w <= 0) return;
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(w);
    p.setStrokeCap(cap);
    p.setStrokeJoin(join);
    if (fill.kind == Fill::Kind::Color)
      p.setColor4f(fill.colorValue, nullptr);
    else if (fill.kind == Fill::Kind::Shader)
      p.setShader(fill.shaderValue);
    if (!dash.empty())
      p.setPathEffect(
          SkDashPathEffect::Make(SkSpan(dash.data(), dash.size()), phase()));
    canvas.drawPath(path, p);
  };

  switch (mode) {
    case Mode::Continuous:
      strokeWith(base, width);
      break;
    case Mode::Bracket:
      strokeWith(lines::cornerBrackets(base, corner, cornerAngleDeg), width);
      break;
    case Mode::Gapped:
      strokeWith(lines::cornerGaps(base, corner, cornerAngleDeg), width);
      break;
    case Mode::Weighted:
      // Two passes over complementary windows: the runs BETWEEN corners at
      // `width`, then the corners themselves at `cornerWidth` — a rule that
      // thickens where it turns.
      strokeWith(lines::cornerGaps(base, corner, cornerAngleDeg), width);
      strokeWith(lines::cornerBrackets(base, corner, cornerAngleDeg),
                 cornerWidth > 0 ? cornerWidth : width);
      break;
  }
}

namespace decorations {
void paintOn(SkCanvas& canvas, const PaintContext& ctx, SkPath outline,
             const Decoration& decoration) {
  PaintContext local = ctx;
  local.outline = std::move(outline);
  decoration.paint(canvas, local);
}
}  // namespace decorations

}  // namespace sigil::compose
