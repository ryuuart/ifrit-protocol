/** @file
 * The pixel styles' paint: the bevel pair as four edge strokes in a
 * stated order, the brackets as L's on the box, the tick rail as a walk
 * along an edge, and the scanlines as rows clipped to the outline.
 */

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <sigilcompose/brush/PixelStyles.h>

#include <cmath>

namespace sigil::compose::styles {

void BevelPair::paint(SkCanvas& c, const PaintContext& ctx) const {
  using geometry::path::Edge;
  // The near edges are the top and the left; sunken swaps the tones (and
  // their widths) onto the far ones and changes nothing else.
  const SkColor4f& near = sunken ? dark : light;
  const SkColor4f& far = sunken ? light : dark;
  const float nearWidth = sunken ? darkWidth : lightWidth;
  const float farWidth = sunken ? lightWidth : darkWidth;
  c.save();
  // Inside the silhouette: each edge is stroked at double width and the
  // half outside the shape is clipped away, so the mark never fattens the
  // silhouette it dresses. The clip is the WHOLE outline, not the edge —
  // an open edge encloses nothing.
  c.clipPath(ctx.outline, SkClipOp::kIntersect, true);
  SkPaint p;
  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeCap(SkPaint::kButt_Cap);
  p.setStrokeJoin(SkPaint::kMiter_Join);
  const auto edge = [&](Edge which, const SkColor4f& tone, float width) {
    if (width <= 0.0f || tone.fA <= 0.0f) return;
    p.setStrokeWidth(width * 2.0f);
    p.setColor4f(tone, nullptr);
    c.drawPath(geometry::path::edges(ctx.outline, which, step), p);
  };
  // Vertical edges first, horizontal ones over them: the top-right corner
  // is the top edge's and the bottom-left the bottom's.
  edge(Edge::Left, near, nearWidth);
  edge(Edge::Right, far, farWidth);
  edge(Edge::Top, near, nearWidth);
  edge(Edge::Bottom, far, farWidth);
  c.restore();
}

void Brackets::paint(SkCanvas& c, const PaintContext& ctx) const {
  using geometry::shapes::Corner;
  using geometry::shapes::has;
  if (arm <= 0.0f || width <= 0.0f) return;
  SkPaint p;
  p.setAntiAlias(antiAlias);
  p.setColor4f(color, nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setStrokeCap(SkPaint::kButt_Cap);
  p.setStrokeJoin(SkPaint::kMiter_Join);
  const float w = ctx.size.width(), h = ctx.size.height();
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

void TickRail::paint(SkCanvas& c, const PaintContext& ctx) const {
  using geometry::path::Edge;
  using geometry::path::has;
  if (pitch <= 0.0f || width <= 0.0f) return;
  SkPaint p;
  p.setAntiAlias(antiAlias);
  p.setColor4f(color, nullptr);
  const float w = ctx.size.width(), h = ctx.size.height();
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

void Scanlines::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (period <= 0.0f || on <= 0.0f) return;
  c.save();
  c.clipPath(ctx.outline, false);
  SkPaint p;
  p.setAntiAlias(false);
  p.setColor4f(color, nullptr);
  p.setBlendMode(blend);
  const float w = ctx.size.width(), h = ctx.size.height();
  // Start one period above the top so a phase in either direction keeps
  // the first row whole.
  const float start = std::fmod(phase, period) - period;
  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
  for (float y = start; y < h; y += period)
    c.drawRect(SkRect::MakeXYWH(0, y, w, on), p);
  c.restore();
}

}  // namespace sigil::compose::styles
