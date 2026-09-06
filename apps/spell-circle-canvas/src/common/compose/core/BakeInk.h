#pragma once

/** @file
 * Internal to the kernel — WHERE A BAKE'S INK IS, as a coarse grid of
 * tiles, and the blit that draws only the tiles that carry any.
 *
 * A bake held in local space is blitted through the node's own transform,
 * and a transform that is not an integer translation resamples: every
 * pixel of the bake's rect runs the sampler, whether the texel it reads is
 * ink or transparent black. A ring of type, an arc layer, a glow round a
 * figure — the shapes a bake is most worth taking for — are a thin band
 * inside a square, so most of that work writes nothing.
 *
 * So the bake records which of its tiles hold any non-transparent texel,
 * and the blit draws those tiles instead of the whole rect. The pixels are
 * the same pixels, for three reasons that are each load-bearing:
 *
 *  - the tiles PARTITION the rect and are drawn without antialiasing, so
 *    every device pixel centre falls inside exactly one tile quad under
 *    Skia's fill rule — no seam, no doubled coverage. (The single blit is
 *    not antialiased either, so this changes nothing about the node's own
 *    edges.)
 *  - each tile draws under `kFast_SrcRectConstraint`, so a bilinear sample
 *    at a tile's edge still reads the true neighbouring texels out of the
 *    same image rather than clamping at the sub-rect.
 *  - a tile is kept if any texel in it OR IN A ONE-TEXEL BORDER around it
 *    is non-transparent. Bilinear reads a 2x2 neighbourhood, so a dropped
 *    tile can be reached from at most one texel away; the border is what
 *    makes dropping it exact rather than nearly exact.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>
#include <include/core/SkRegion.h>
#include <include/core/SkSamplingOptions.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sigil::compose::detail {

/** Where a bake's ink is, one flag per square tile of its image.
 *
 *  `empty()` is the state of every bake that was never scanned — a GPU
 *  surface whose pixels cannot be read back, an image too small to be
 *  worth the arithmetic, one whose ink fills it anyway — and it means
 *  "draw the whole rect", which is what the blit did before this existed.
 */
struct InkGrid {
  int tile = 0;   ///< tile side in texels; 0 when there is no grid
  int cols = 0;   ///< tiles across the image
  int rows = 0;   ///< tiles down the image
  std::vector<uint8_t> covered;  ///< row-major, 1 where the tile carries ink
  bool empty() const { return tile <= 0; }
  bool at(int col, int row) const {
    return covered[(size_t)row * (size_t)cols + (size_t)col] != 0;
  }
};

/** THE TILE SIDE, in pixels — the grid's own resolution and the size of
 *  the patches the blit is admitted in. It is one number because the two
 *  are the same grain: a device patch is tested by mapping it back into
 *  the image, so a coarse grid dilates a fine patch's answer and a coarse
 *  patch dilates a fine grid's. Coarser leaves a band's own tiles ringed
 *  by empty ones that are kept anyway; finer costs a longer scan and more
 *  region rects for a share of the blit that stops shrinking. */
inline constexpr int kInkTile = 16;

/** Below this the blit is already cheap and the grid is pure overhead. A
 *  small bake is a badge, a label, a seal — solid, and blitted in less
 *  time than the scan would take. */
inline constexpr int kInkMinSide = 4 * kInkTile;

/** How much of a bake must be EMPTY before drawing it in pieces beats
 *  drawing it whole. Each tile costs a draw call, so a bake that is nearly
 *  solid pays the calls and saves nothing. */
inline constexpr float kInkMaxCoverage = 0.8f;

/** Scan @p px for the tiles that carry ink, dilated by one texel so a
 *  bilinear sample taken in a dropped tile still finds nothing there.
 *
 *  Returns an empty grid when the image is too small to be worth slicing
 *  or when its ink covers it anyway — both mean "blit it whole". */
inline InkGrid inkGridOf(const SkPixmap& px) {
  if (px.width() < kInkMinSide || px.height() < kInkMinSide) return {};
  if (px.colorType() != kBGRA_8888_SkColorType &&
      px.colorType() != kRGBA_8888_SkColorType)
    return {};  // the alpha byte is not where the scan below expects it
  InkGrid grid;
  grid.tile = kInkTile;
  grid.cols = (px.width() + kInkTile - 1) / kInkTile;
  grid.rows = (px.height() + kInkTile - 1) / kInkTile;
  grid.covered.assign((size_t)grid.cols * (size_t)grid.rows, 0);
  // One pass over the pixels, marking the tile each non-transparent texel
  // belongs to. Premultiplied N32: a texel with a zero alpha byte is
  // transparent black whatever the other three bytes say.
  for (int y = 0; y < px.height(); ++y) {
    const uint32_t* row = px.addr32(0, y);
    const int tileRow = y / kInkTile;
    uint8_t* marks = grid.covered.data() + (size_t)tileRow * (size_t)grid.cols;
    for (int x = 0; x < px.width(); ++x)
      if ((row[x] >> 24) != 0) marks[x / kInkTile] = 1;
  }
  // THE ONE-TEXEL BORDER, applied as a whole-tile dilation. A texel on a
  // tile's edge is read by a bilinear sample taken in the tile beside it,
  // so that neighbour must be drawn too. Dilating by a full tile rather
  // than by one texel over-keeps by at most one ring of tiles and needs no
  // second scan — the exactness is what matters here, not the last few
  // per cent.
  std::vector<uint8_t> grown = grid.covered;
  for (int r = 0; r < grid.rows; ++r)
    for (int c = 0; c < grid.cols; ++c) {
      if (!grid.at(c, r)) continue;
      for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc) {
          const int rr = r + dr, cc = c + dc;
          if (rr < 0 || cc < 0 || rr >= grid.rows || cc >= grid.cols) continue;
          grown[(size_t)rr * (size_t)grid.cols + (size_t)cc] = 1;
        }
    }
  grid.covered.swap(grown);
  const size_t lit = (size_t)std::count(grid.covered.begin(),
                                        grid.covered.end(), (uint8_t)1);
  if ((float)lit > kInkMaxCoverage * (float)grid.covered.size()) return {};
  return grid;
}

/** Draw @p image across @p dst, skipping the parts of the canvas no ink of
 *  it can reach.
 *
 *  Every piece is the SAME draw the single blit would have made — the whole
 *  image across the whole @p dst, under the canvas's own matrix — admitted
 *  by a clip on WHOLE DEVICE PIXELS. Both halves of that are load-bearing,
 *  and the two obvious alternatives are each wrong in a way that only shows
 *  up as a handful of pixels in a whole plate:
 *
 *   - cutting the destination into pieces and mapping each back to a
 *     sub-image rebuilds the src-to-dst mapping per piece, and it lands a
 *     fraction of a texel off the one the single blit used: a seam of
 *     off-by-one channels down every tile edge.
 *   - clipping to pieces in the node's OWN space puts the boundaries on
 *     arbitrary lines through the device grid, and a pixel whose centre
 *     falls on a boundary two pieces share can be admitted by neither.
 *
 *  A region of whole device pixels has neither problem: it is a SET of
 *  pixels rather than an outline, so nothing can fall between its parts,
 *  and the draw it admits is bit-for-bit the draw that would have covered
 *  the whole rect.
 *
 *  IT IS A DEVICE-SPACE CLIP, so the caller says which device: @p toDevice
 *  maps @p dst onto the pixels the draw really lands on and @p deviceClip
 *  bounds them, and neither is read off the canvas. The canvas may be a
 *  recording, whose ops are replayed under a matrix of its own; a region
 *  ignores that matrix, exactly as it ignores every other, so one computed
 *  in the recording's own space would be applied unchanged in the space the
 *  recording is replayed into — the wrong units in the wrong place, cutting
 *  the bake to pieces. A caller recording into a picture passes the matrix
 *  the picture is replayed under, and owes the pin that keeps it true.
 *
 *  With an empty grid this is the single blit, unchanged. */
inline void drawInkedImage(SkCanvas& canvas, const sk_sp<SkImage>& image,
                           const InkGrid& ink, const SkRect& dst,
                           const SkMatrix& toDevice, const SkIRect& deviceClip,
                           const SkSamplingOptions& sampling,
                           const SkPaint* paint) {
  SkMatrix toLocal;
  if (ink.empty() || !toDevice.invert(&toLocal)) {
    canvas.drawImageRect(image, dst, sampling, paint);
    return;
  }
  SkIRect area = deviceClip;
  SkIRect want;
  toDevice.mapRect(dst).roundOut(&want);
  if (!area.intersect(want)) return;  // nothing of the bake is on the canvas

  const float sx = (float)image->width() / dst.width();
  const float sy = (float)image->height() / dst.height();
  // Does any lit tile of the bake lie under this patch of the canvas? The
  // patch's corners come back into the image through the inverse, and the
  // box they span is grown by a texel for the bilinear neighbourhood the
  // sampler reads outside them.
  const auto inked = [&](const SkIRect& patch) {
    SkRect back = toLocal.mapRect(SkRect::Make(patch));
    back.offset(-dst.left(), -dst.top());
    const int c0 = std::max(0, (int)std::floor((back.left() * sx - 1.0f) /
                                               (float)ink.tile));
    const int c1 = std::min(ink.cols - 1,
                            (int)std::floor((back.right() * sx + 1.0f) /
                                            (float)ink.tile));
    const int r0 = std::max(0, (int)std::floor((back.top() * sy - 1.0f) /
                                               (float)ink.tile));
    const int r1 = std::min(ink.rows - 1,
                            (int)std::floor((back.bottom() * sy + 1.0f) /
                                            (float)ink.tile));
    for (int r = r0; r <= r1; ++r)
      for (int c = c0; c <= c1; ++c)
        if (ink.at(c, r)) return true;
    return false;
  };

  // The lit patches, unioned into ONE device-space region and clipped in
  // one go. A region is a set of whole pixels, so it has no boundary a
  // pixel can fall through, and the raster blitter walks its spans — which
  // is the same work as clipping each patch separately without the save,
  // matrix and clip-stack churn of a hundred draws.
  SkRegion lit;
  for (int y = area.top(); y < area.bottom(); y += kInkTile) {
    const int bottom = std::min(y + kInkTile, area.bottom());
    for (int x = area.left(); x < area.right(); x += kInkTile) {
      const int right = std::min(x + kInkTile, area.right());
      SkIRect patch = SkIRect::MakeLTRB(x, y, right, bottom);
      if (!inked(patch)) continue;
      // Adjacent lit patches join into one rect before they reach the
      // region: a band crossing a row leaves a dozen of them lit in a row.
      while (patch.right() < area.right()) {
        const SkIRect next = SkIRect::MakeLTRB(
            patch.right(), y, std::min(patch.right() + kInkTile, area.right()),
            bottom);
        if (!inked(next)) break;
        patch.fRight = next.fRight;
      }
      x = patch.right() - kInkTile;
      lit.op(patch, SkRegion::kUnion_Op);
    }
  }
  if (lit.isEmpty()) return;
  canvas.save();
  canvas.clipRegion(lit);  // device pixels, unaffected by the CTM
  canvas.drawImageRect(image, dst, sampling, paint);
  canvas.restore();
}

}  // namespace sigil::compose::detail
