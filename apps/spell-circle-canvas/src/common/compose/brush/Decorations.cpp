/** @file
 * The concrete decorations that plug the Decoration seam: path formatting,
 * lattice slices, contour walks, washes and borders.
 */

#include <include/core/SkClipOp.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/effects/Sk1DPathEffect.h>
#include <include/effects/SkDashPathEffect.h>
#include <sigilcompose/advanced/PathEffect.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilskia/draw/Direct.h>

#include <cmath>

#include "paint/FillLowering.h"
#include "StrokeReach.h"
#include "paint/HeldCoverage.h"

namespace sigil::compose {

float PathFormat::bleed() const {
  if (align == Align::Inner) return 0.0f;
  // An Outer stroke is laid at twice its width and its inside half cut
  // away, so its outer edge stands as far off as the doubled band's.
  const float laid = align == Align::Outer ? width * 2 : width;
  return detail::strokeReach(laid, join, cap);
}

bool PathFormat::staysOnOutline() const {
  // Clipped to the shape it dresses, whatever it lays.
  if (align == Align::Inner) return true;
  // A path effect and a stamped path lay their own geometry, whose extent
  // no number here states.
  return !effect && !(stampAdvance > 0 && !stampPath.empty());
}

void PathFormat::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  SkPaint p;
  p.setAntiAlias(antiAlias);
  p.setStyle(SkPaint::kStroke_Style);
  // Inner/Outer: clip to the shape's side and stroke DOUBLE width — the
  // visible half lands entirely on the kept side (the standard trick).
  const bool aligned = align != Align::Center;
  p.setStrokeWidth(aligned ? width * 2 : width);
  p.setStrokeCap(geometry::path::toSk(cap));
  p.setStrokeJoin(geometry::path::toSk(join));
  const Fill stroke = resolveFill(strokeFill, ctx);
  if (stroke.kind == Fill::Kind::None) return;
  if (stroke.kind == Fill::Kind::Color)
    p.setColor4f(material::skia::toSkColor(stroke.colorValue), nullptr);
  else if (stroke.kind == Fill::Kind::Paint)
    p.setShader(material::skia::staticShader(detail::paintOf(stroke)));

  sk_sp<SkPathEffect> chosen = effect ? effect->skia : nullptr;
  if (!chosen && stampAdvance > 0 && !stampPath.empty())
    chosen =
        SkPath1DPathEffect::Make(geometry::path::toSk(stampPath), stampAdvance,
                                 phase(), SkPath1DPathEffect::kRotate_Style);
  if (!chosen && !dashIntervals.empty())
    chosen = SkDashPathEffect::Make(
        SkSpan(dashIntervals.data(), dashIntervals.size()), phase());
  p.setPathEffect(std::move(chosen));

  // The decoration's own trim window (wrapping; the marching sliver).
  const SkPath outline = geometry::path::toSk(ctx.outline);
  const SkPath* drawn = &outline;
  SkPath windowed;
  const float off = trimPhase ? trimPhase->value() : trimOffset;
  const float s0 = trimStart + off, e0 = trimEnd + off;
  const float span = e0 - s0;
  if (span > 0.0f && span < 1.0f) {
    const float s = s0 - std::floor(s0);
    const float e = e0 - std::floor(e0);
    SkPathBuilder window;
    for (const geometry::path::Contour& contour :
         geometry::path::contoursOf(outline)) {
      const float len = contour.length();
      if (s < e) {
        geometry::path::appendSegment(window, contour, s * len, e * len);
      } else if (s > e) {
        geometry::path::appendSegment(window, contour, s * len, len);
        // A closed contour has a real seam, so joining both pieces avoids
        // doubled caps there. An open route has no seam: continuing without
        // a moveTo would invent a straight chord from its end to its start.
        geometry::path::appendSegment(window, contour, 0, e * len,
                                      !contour.closed());
      }
    }
    windowed = window.detach();
    if (!windowed.isEmpty()) drawn = &windowed;
  } else if (span <= 0.0f) {
    return;  // empty window — nothing to stroke
  }

  if (aligned) {
    // The alignment clips to what the SHAPE encloses, which is the outline
    // itself unless an adaptor narrowed the outline to runs that bound no
    // area and left the shape in `silhouette`.
    const SkPath& shape = ctx.silhouette.empty()
                              ? geometry::path::toSk(ctx.outline)
                              : geometry::path::toSk(ctx.silhouette);
    canvas.save();
    canvas.clipPath(
        shape,
        align == Align::Inner ? SkClipOp::kIntersect : SkClipOp::kDifference,
        true);
    canvas.drawPath(*drawn, p);
    canvas.restore();
  } else {
    canvas.drawPath(*drawn, p);
  }
}

void Slice::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  if (!asset || asset->frames().empty()) return;
  sk_sp<SkImage> img = asset->frames().front().image;
  if (!img) return;
  const SkRect dst = SkRect::MakeWH(ctx.size.x, ctx.size.y);
  skia::draw::drawLattice(canvas, std::move(img), xDivs, yDivs, dst,
                          material::skia::toSkFilterMode(filter), density);
}

/** The walk's replayed stamp and the node it was baked from. */
struct ContourWalk::StampCache {
  sk_sp<SkPicture> picture;
  const void* bakedFor = nullptr;
};

std::shared_ptr<ContourWalk::StampCache> ContourWalk::freshStampCache() {
  return std::make_shared<StampCache>();
}

void ContourWalk::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
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

  size_t index = 0;  // runs across contours — the sequence's position
  for (const geometry::path::Contour& contour :
       geometry::path::Contour::of(ctx.outline)) {
    const float length = contour.length();
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float d = 0; d < length; d += spacing) {
      const auto measured = contour.at(d);
      if (!measured) continue;
      PathSample sample{measured->position, measured->tangent, d,
                        length > 0 ? d / length : 0};
      // This sample's OWN art (stampAt): baked per call, uncached — see
      // the field note. The shell box is needed because snapshot() sizes
      // by the root's CHILDREN and ignores the root's own dimensions.
      sk_sp<SkPicture> own;
      if (stampAt && ctx.fonts)
        if (std::optional<Element> e = stampAt(sample, index))
          own = snapshot(box().children({std::move(*e)}), *ctx.fonts);
      const sk_sp<SkPicture>& art = own ? own : stampPicture;
      canvas.save();
      canvas.translate(sample.position.x, sample.position.y);
      canvas.rotate(geometry::path::degrees(
          std::atan2(sample.tangent.y, sample.tangent.x)));
      if (art) {
        const SkRect cull = art->cullRect();
        canvas.save();
        canvas.translate(-cull.width() / 2, -cull.height() / 2);
        canvas.drawPicture(art);
        canvas.restore();
      }
      if (draw) draw(pen, sample, ctx);
      canvas.restore();
      ++index;
    }
  }
}

bool Wash::usesWorldSpace() const {
  return material::skia::paint(material).usesWorldSpace();
}

void Wash::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  const float a = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
  if (a <= 0.0f) return;
  // The material is lowered to the executor's paint where it is drawn,
  // against this node's frame.
  const Fill fill = resolveFill(material, ctx);
  SkPaint p;
  p.setAntiAlias(true);
  p.setBlendMode(material::skia::toSkBlendMode(blend));
  if (fill.kind == Fill::Kind::Color) {
    material::Color c = fill.colorValue;
    c.a *= a;
    p.setColor4f(material::skia::toSkColor(c), nullptr);
  } else if (fill.kind == Fill::Kind::Paint) {
    p.setShader(material::skia::staticShader(detail::paintOf(fill)));
    p.setAlphaf(a);
  } else {
    return;
  }
  canvas.drawPath(geometry::path::toSk(ctx.outline), p);
}

float Border::bleed() const {
  // An inward copy of the outline stands `inset` inside it everywhere, so
  // the rule's reach past the outline is its own reach less the inset.
  return std::max(
      0.0f, detail::strokeReach(std::max(width, cornerWidth), join, cap) - inset);
}

bool Border::staysOnOutline() const { return inset >= 0; }

void Border::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& canvas = *pen.canvas();
  // `width` is the RUN's width, and Weighted mode has a second width for
  // the corners — so in that mode width == 0 means "corners only, no runs
  // between them", which is a real frame. The bail-out therefore tests
  // the HEAVIEST of the two widths, or weightedCorners(0, w, …) would
  // draw nothing at all.
  const float heaviest =
      mode == Mode::Weighted ? std::max(width, cornerWidth) : width;
  if (ctx.outline.empty() || heaviest <= 0) return;
  const SkPath base = inset != 0 ? geometry::path::insetOutline(
                                       geometry::path::toSk(ctx.outline), inset)
                                 : geometry::path::toSk(ctx.outline);

  // A fill written as the ink in force, or as a custom property, takes
  // its colour from the node the border is painted under.
  const Fill resolved = resolveFill(fill, ctx);
  auto strokeWith = [&](const SkPath& path, float w) {
    if (path.isEmpty() || w <= 0) return;
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(w);
    p.setStrokeCap(geometry::path::toSk(cap));
    p.setStrokeJoin(geometry::path::toSk(join));
    if (resolved.kind == Fill::Kind::Color)
      p.setColor4f(material::skia::toSkColor(resolved.colorValue), nullptr);
    else if (resolved.kind == Fill::Kind::Paint)
      p.setShader(material::skia::staticShader(detail::paintOf(resolved)));
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
      strokeWith(geometry::path::toSk(lines::cornerBrackets(
                     geometry::path::fromSk(base), corner, cornerAngleDegrees)),
                 width);
      break;
    case Mode::Gapped:
      strokeWith(geometry::path::toSk(lines::cornerGaps(
                     geometry::path::fromSk(base), corner, cornerAngleDegrees)),
                 width);
      break;
    case Mode::Weighted:
      // Two passes over complementary windows: the runs BETWEEN corners at
      // `width`, then the corners themselves at `cornerWidth` — a rule that
      // thickens where it turns.
      strokeWith(geometry::path::toSk(lines::cornerGaps(
                     geometry::path::fromSk(base), corner, cornerAngleDegrees)),
                 width);
      strokeWith(geometry::path::toSk(lines::cornerBrackets(
                     geometry::path::fromSk(base), corner, cornerAngleDegrees)),
                 cornerWidth > 0 ? cornerWidth : width);
      break;
  }
}

namespace decorations {
void paintOn(draw::Pen& pen, const PaintContext& ctx,
             geometry::path::Outline outline, const Decoration& decoration) {
  PaintContext local = ctx;
  local.outline = std::move(outline);
  decoration.paint(pen, local);
}
}  // namespace decorations

}  // namespace sigil::compose
