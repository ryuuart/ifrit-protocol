/** @file
 * The pixel styles' paint: the bevel pair as four edge strokes in a
 * stated order (or as a mitred ring), the brackets as L's on the box and
 * the tick rail as a walk along an edge, each in the ink it names.
 */

#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Skia.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkRect.h>
#include <sigilcompose/brush/PixelStyles.h>

#include <cmath>

#include "Ink.h"

namespace sigil::compose::styles {
namespace {

/** THE MITRED RING, as two fills rather than four strokes: the whole ring
 *  in the far tone, then the near region over it. Complementary by
 *  construction — a diagonal drawn twice from two sides leaves a seam
 *  wherever the two rasterisations disagree, and at 1 px that seam IS the
 *  mark.
 *
 *  `bias` is where the diagonal stands relative to the corner, in px: a
 *  positive bias hands the pixel on the diagonal to the near band, a
 *  negative one to the far band. It is half a pixel because that is the
 *  distance from a corner to the centre of the pixel the corner names.
 */
void paintMitredRing(SkCanvas& c, SkRect box, const Fill& near,
                     const Fill& far, float nearWidth,
                     float farWidth, float bias, bool antiAlias) {
  // A bevel deeper than half the box is the whole box; the far band would
  // otherwise cross the near one and the ring would turn inside out.
  const float half = std::min(box.width(), box.height()) * 0.5f;
  const float wn = std::min(std::max(nearWidth, 0.0f), half);
  const float wf = std::min(std::max(farWidth, 0.0f), half);
  if (wn <= 0.0f && wf <= 0.0f) return;
  const float l = box.left(), t = box.top(), r = box.right(), b = box.bottom();

  SkPaint p;
  p.setAntiAlias(antiAlias);
  p.setStyle(SkPaint::kFill_Style);

  // The ring: the box less the rectangle the two bands leave standing.
  SkPathBuilder ring;
  ring.addRect(box);
  ring.addRect(SkRect::MakeLTRB(l + wn, t + wn, r - wf, b - wf),
               SkPathDirection::kCCW);
  if (wf > 0.0f && detail::layInk(p, far)) {
    c.drawPath(ring.detach(), p);
  } else {
    ring.reset();
  }

  if (wn <= 0.0f) return;
  SkPaint nearPaint = p;
  nearPaint.setShader(nullptr);
  if (!detail::layInk(nearPaint, near)) return;
  // The near region: the top and left bands, cut at the top-right and the
  // bottom-left by the two diagonals. Each diagonal is clamped where it
  // leaves the box, which is what the bias moves it past.
  SkPathBuilder n;
  n.moveTo(l, t);
  if (bias > 0.0f) {
    n.lineTo(r, t);
    n.lineTo(r, t + bias);
  } else {
    n.lineTo(r + bias, t);
  }
  n.lineTo(r + bias - wn, t + wn);
  n.lineTo(l + wn, t + wn);
  n.lineTo(l + wn, b + bias - wn);
  if (bias > 0.0f) {
    n.lineTo(l + bias, b);
    n.lineTo(l, b);
  } else {
    n.lineTo(l, b + bias);
  }
  n.close();
  c.drawPath(n.detach(), nearPaint);
}

/** THE STRIP one side of a masked ring occupies in the ring's box, cut
 *  back at either end under `Sliced` by the depth of the band the mask
 *  left out there. Where the neighbouring band IS drawn the strip runs to
 *  the corner and the two overlap, which is what leaves the corner to
 *  whichever rule the corner mode states.
 *
 *  The near tones are the top and the left, so each side is as deep as
 *  the tone that lands on it. */
SkRect band(geometry::path::Edge which, const SkRect& box,
            geometry::path::Edge mask, BevelEnds ends, float nearWidth,
            float farWidth) {
  using geometry::path::Edge;
  using geometry::path::has;
  const auto depth = [&](Edge e) {
    return e == Edge::Top || e == Edge::Left ? nearWidth : farWidth;
  };
  const auto cut = [&](Edge e) {
    return has(mask, e) || ends == BevelEnds::Mitred ? 0.0f : depth(e);
  };
  SkRect strip = box;
  switch (which) {
    case Edge::Top:
      strip.fBottom = box.fTop + nearWidth;
      break;
    case Edge::Bottom:
      strip.fTop = box.fBottom - farWidth;
      break;
    case Edge::Left:
      strip.fRight = box.fLeft + nearWidth;
      break;
    default:
      strip.fLeft = box.fRight - farWidth;
      break;
  }
  if (which == Edge::Left || which == Edge::Right) {
    strip.fTop += cut(Edge::Top);
    strip.fBottom -= cut(Edge::Bottom);
  } else {
    strip.fLeft += cut(Edge::Left);
    strip.fRight -= cut(Edge::Right);
  }
  return strip;
}

}  // namespace

void BevelPair::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& c = *pen.canvas();
  using geometry::path::Edge;
  using geometry::path::has;
  // The near edges are the top and the left; sunken swaps the tones (and
  // their widths) onto the far ones and changes nothing else.
  const Fill lit = resolveFill(light, ctx);
  const Fill shaded = resolveFill(dark, ctx);
  const Fill& near = sunken ? shaded : lit;
  const Fill& far = sunken ? lit : shaded;
  const float nearWidth = sunken ? darkWidth : lightWidth;
  const float farWidth = sunken ? lightWidth : darkWidth;
  // A full ring is drawn exactly as it was before there was a mask to
  // narrow it — no strip, no second clip, nothing to disagree about at a
  // corner where both bands stand.
  const bool whole = edges == Edge::All;
  // THE SHAPE THE RING DRESSES. `outline` is it unless an edge adaptor or
  // a span gate narrowed the outline to runs that BOUND NO AREA and left
  // the shape in `silhouette`. Everything the ring is built from is the
  // SHAPE's: clipping to a run that encloses nothing would discard the
  // whole ring, and a band's facing is classified against a bounds centre
  // a run does not have. What the narrowed outline decides is how much of
  // the ring is SHOWN, which is the band below.
  const SkPath& shape = ctx.silhouette.empty() ? geometry::path::toSk(ctx.outline) : geometry::path::toSk(ctx.silhouette);
  const SkRect bounds = shape.getBounds();
  // …and, where the outline WAS narrowed, the band that limits the ring to
  // the part of the boundary that is shown. A run bounds no area, so it
  // cannot be a clip until it is given a width: the ring's own depth to
  // each side of it leaves exactly the ring along the run and nothing
  // else. Empty when nothing narrowed the outline, which is every panel
  // that wears a bevel and no gate.
  SkPath revealed;
  if (!ctx.silhouette.empty()) {
    SkPaint depth;
    depth.setStyle(SkPaint::kStroke_Style);
    depth.setStrokeWidth(std::max(nearWidth, farWidth) * 2.0f);
    revealed = skpathutils::FillPathWithPaint(geometry::path::toSk(ctx.outline), depth);
  }
  const auto clipToShape = [&](SkCanvas& canvas) {
    canvas.clipPath(shape, SkClipOp::kIntersect, antiAlias);
    if (!revealed.isEmpty())
      canvas.clipPath(revealed, SkClipOp::kIntersect, antiAlias);
  };
  if (corner != BevelCorner::Square) {
    SkRect box = bounds;
    if (!antiAlias)
      box = SkRect::MakeLTRB(std::round(box.left()), std::round(box.top()),
                             std::round(box.right()), std::round(box.bottom()));
    c.save();
    clipToShape(c);
    if (!whole) {
      // The ring is one pair of fills over the whole box, so the mask is
      // a clip: the strips of the sides it selects, and nothing else.
      SkPathBuilder kept;
      for (Edge e : {Edge::Top, Edge::Right, Edge::Bottom, Edge::Left})
        if (has(edges, e))
          kept.addRect(band(e, box, edges, ends, nearWidth, farWidth));
      c.clipPath(kept.detach(), SkClipOp::kIntersect, antiAlias);
    }
    paintMitredRing(c, box, near, far, nearWidth, farWidth,
                    corner == BevelCorner::Mitre ? 0.5f : -0.5f, antiAlias);
    c.restore();
    return;
  }
  c.save();
  // Inside the shape: each edge is stroked at double width and the half
  // outside the shape is clipped away, so the mark never fattens the
  // silhouette it dresses. The clip is the WHOLE shape, not the edge — an
  // open edge encloses nothing.
  clipToShape(c);
  SkPaint p;
  p.setAntiAlias(antiAlias);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeCap(SkPaint::kButt_Cap);
  p.setStrokeJoin(SkPaint::kMiter_Join);
  const auto edge = [&](Edge which, const Fill& tone, float width) {
    if (width <= 0.0f || !has(edges, which)) return;
    p.setShader(nullptr);
    if (!detail::layInk(p, tone)) return;
    p.setStrokeWidth(width * 2.0f);
    if (whole) {
      c.drawPath(geometry::path::edges(shape, which, step), p);
      return;
    }
    // A sliced band is cut ACROSS its run and never across its depth: the
    // sub-contour it is stroked along already follows the silhouette, and
    // a chamfer or a round carries its band further in from the box than
    // a straight edge does.
    SkRect kept = band(which, bounds, edges, ends, nearWidth, farWidth);
    if (which == Edge::Left || which == Edge::Right) {
      kept.fLeft = bounds.fLeft;
      kept.fRight = bounds.fRight;
    } else {
      kept.fTop = bounds.fTop;
      kept.fBottom = bounds.fBottom;
    }
    c.save();
    c.clipRect(kept, SkClipOp::kIntersect, antiAlias);
    c.drawPath(geometry::path::edges(shape, which, step), p);
    c.restore();
  };
  // Vertical edges first, horizontal ones over them: the top-right corner
  // is the top edge's and the bottom-left the bottom's.
  edge(Edge::Left, near, nearWidth);
  edge(Edge::Right, far, farWidth);
  edge(Edge::Top, near, nearWidth);
  edge(Edge::Bottom, far, farWidth);
  c.restore();
}

void Brackets::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& c = *pen.canvas();
  using geometry::shapes::Corner;
  using geometry::shapes::has;
  if (arm <= 0.0f || width <= 0.0f) return;
  SkPaint p;
  p.setAntiAlias(antiAlias);
  if (!detail::layInk(p, resolveFill(ink, ctx))) return;
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setStrokeCap(SkPaint::kButt_Cap);
  p.setStrokeJoin(SkPaint::kMiter_Join);
  const float w = ctx.size.x, h = ctx.size.y;
  // The stroke is centred on its path, so the path stands half a width
  // further in than the gap for the mark's outer edge to land on it.
  const float o = gap + width * 0.5f;
  const auto corner = [&](float x, float y, float sx, float sy) {
    SkPathBuilder b;
    b.moveTo(x + sx * arm, y);
    b.lineTo(x, y);
    b.lineTo(x, y + sy * arm);
    c.drawPath(b.detach(), p);
  };
  if (has(corners, Corner::TopLeft)) corner(o, o, 1, 1);
  if (has(corners, Corner::TopRight)) corner(w - o, o, -1, 1);
  if (has(corners, Corner::BottomRight)) corner(w - o, h - o, -1, -1);
  if (has(corners, Corner::BottomLeft)) corner(o, h - o, 1, -1);
}

void TickRail::paint(draw::Pen& pen, const PaintContext& ctx) const {
  SkCanvas& c = *pen.canvas();
  using geometry::path::Edge;
  using geometry::path::has;
  if (pitch <= 0.0f || width <= 0.0f) return;
  SkPaint p;
  p.setAntiAlias(antiAlias);
  if (!detail::layInk(p, resolveFill(ink, ctx))) return;
  const float w = ctx.size.x, h = ctx.size.y;
  const auto rail = [&](Edge which) {
    const bool vertical = which == Edge::Left || which == Edge::Right;
    const float run = vertical ? h : w;
    int i = 0;
    // The loop walks a distance; the accumulated float is the position.
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float d = pitch * phase; d < run; d += pitch, ++i) {
      const bool long_ = majorEvery > 0 && i % majorEvery == 0;
      const float len = long_ ? major : minor;
      if (len <= 0.0f) continue;
      SkRect mark;
      switch (which) {
        case Edge::Top:
          mark = SkRect::MakeXYWH(d, 0, width, len);
          break;
        case Edge::Bottom:
          mark = SkRect::MakeXYWH(d, h - len, width, len);
          break;
        case Edge::Left:
          mark = SkRect::MakeXYWH(0, d, len, width);
          break;
        default:
          mark = SkRect::MakeXYWH(w - len, d, len, width);
          break;
      }
      c.drawRect(mark, p);
    }
  };
  if (has(edge, Edge::Top)) rail(Edge::Top);
  if (has(edge, Edge::Bottom)) rail(Edge::Bottom);
  if (has(edge, Edge::Left)) rail(Edge::Left);
  if (has(edge, Edge::Right)) rail(Edge::Right);
}

}  // namespace sigil::compose::styles
