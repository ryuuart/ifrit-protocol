/** @file
 * The pixel styles' paint: the bevel pair as four edge strokes in a
 * stated order (or as a mitred ring), the brackets as L's on the box, the
 * tick rail as a walk along an edge, the scanlines as rows clipped to the
 * outline, and the stipple as a tint through a mask tile.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkTileMode.h>
#include <sigilcompose/brush/PixelStyles.h>

#include <cmath>
#include <map>
#include <mutex>

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
void paintMitredRing(SkCanvas& c, SkRect box, const SkColor4f& near,
                     const SkColor4f& far, float nearWidth, float farWidth,
                     float bias, bool antiAlias) {
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
  if (far.fA > 0.0f && wf > 0.0f) {
    p.setColor4f(far, nullptr);
    c.drawPath(ring.detach(), p);
  } else {
    ring.reset();
  }

  if (near.fA <= 0.0f || wn <= 0.0f) return;
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
  p.setColor4f(near, nullptr);
  c.drawPath(n.detach(), p);
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

/** The mask tile a stipple is drawn through, cut once for the process.
 *  Every stipple of the same lattice shares one image, so a desktop of
 *  greyed-out controls holds one 2 × 2 bitmap between them. */
sk_sp<SkImage> maskTile(uint64_t bits, int size) {
  static std::mutex lock;
  static std::map<std::pair<uint64_t, int>, sk_sp<SkImage>> cut;
  const std::lock_guard held(lock);
  auto [at, fresh] = cut.try_emplace({bits, size}, nullptr);
  if (fresh) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(size, size));
    bm.eraseColor(SK_ColorTRANSPARENT);
    for (int y = 0; y < size; ++y)
      for (int x = 0; x < size; ++x)
        if ((bits >> (y * size + x)) & 1u) *bm.getAddr32(x, y) = 0xFFFFFFFFu;
    bm.setImmutable();
    at->second = bm.asImage();
  }
  return at->second;
}

}  // namespace

void BevelPair::paint(SkCanvas& c, const PaintContext& ctx) const {
  using geometry::path::Edge;
  using geometry::path::has;
  // The near edges are the top and the left; sunken swaps the tones (and
  // their widths) onto the far ones and changes nothing else.
  const SkColor4f& near = sunken ? dark : light;
  const SkColor4f& far = sunken ? light : dark;
  const float nearWidth = sunken ? darkWidth : lightWidth;
  const float farWidth = sunken ? lightWidth : darkWidth;
  // A full ring is drawn exactly as it was before there was a mask to
  // narrow it — no strip, no second clip, nothing to disagree about at a
  // corner where both bands stand.
  const bool whole = edges == Edge::All;
  const SkRect bounds = ctx.outline.getBounds();
  if (corner != BevelCorner::Square) {
    SkRect box = bounds;
    if (!antiAlias)
      box = SkRect::MakeLTRB(std::round(box.left()), std::round(box.top()),
                             std::round(box.right()), std::round(box.bottom()));
    c.save();
    c.clipPath(ctx.outline, SkClipOp::kIntersect, antiAlias);
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
  // Inside the silhouette: each edge is stroked at double width and the
  // half outside the shape is clipped away, so the mark never fattens the
  // silhouette it dresses. The clip is the WHOLE outline, not the edge —
  // an open edge encloses nothing.
  c.clipPath(ctx.outline, SkClipOp::kIntersect, antiAlias);
  SkPaint p;
  p.setAntiAlias(antiAlias);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeCap(SkPaint::kButt_Cap);
  p.setStrokeJoin(SkPaint::kMiter_Join);
  const auto edge = [&](Edge which, const SkColor4f& tone, float width) {
    if (width <= 0.0f || tone.fA <= 0.0f || !has(edges, which)) return;
    p.setStrokeWidth(width * 2.0f);
    p.setColor4f(tone, nullptr);
    if (whole) {
      c.drawPath(geometry::path::edges(ctx.outline, which, step), p);
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
    c.drawPath(geometry::path::edges(ctx.outline, which, step), p);
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

void Stipple::paint(SkCanvas& c, const PaintContext& ctx) const {
  if (bits == 0 || size <= 0 || size > 8 || cell <= 0.0f) return;
  const sk_sp<SkImage> tile = maskTile(bits, size);
  if (!tile) return;
  SkPaint p;
  p.setAntiAlias(false);
  SkMatrix local;
  local.setScale(cell, cell);
  p.setShader(tile->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                               SkSamplingOptions(SkFilterMode::kNearest),
                               &local));
  // The mask carries coverage, not colour: kSrcIn stamps the one colour
  // into every set cell and leaves the clear ones alone.
  p.setColorFilter(
      SkColorFilters::Blend(color.toSkColor(), SkBlendMode::kSrcIn));
  c.save();
  c.clipPath(ctx.outline, SkClipOp::kIntersect, false);
  c.drawRect(ctx.outline.getBounds(), p);
  c.restore();
}

Stipple dither(SkColor4f color, int on, int size, float cell) {
  // The Bayer threshold matrix, built by the recursion that defines it:
  // each step quadruples the lattice, the four quadrants offset by
  // 0, 2, 3, 1 quarters of the range, which is what spreads a tone's
  // cells as far from each other as the lattice allows.
  int b[8][8] = {{0}};
  int n = 1;
  while (n < size) {
    for (int y = 0; y < n; ++y)
      for (int x = 0; x < n; ++x) {
        const int v = b[y][x] * 4;
        b[y][x] = v;
        b[y][x + n] = v + 2;
        b[y + n][x] = v + 3;
        b[y + n][x + n] = v + 1;
      }
    n *= 2;
  }
  uint64_t bits = 0;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x)
      if (b[y][x] < on) bits |= uint64_t{1} << (y * size + x);
  return Stipple{color, bits, size, cell};
}

}  // namespace sigil::compose::styles
