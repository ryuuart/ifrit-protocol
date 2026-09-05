/** @file
 * The coverage boundary: what a node DREW, as a path.
 *
 * A node's shape and its glyph outlines are both descriptions — one of a
 * box, one of a placement — and neither knows anything about an alpha
 * cut-out, a clip or a mask. Those are facts about the pixels, so this
 * answers them from the pixels: rasterise the node's layer into an alpha
 * surface, keep the pixels its paint covered, and trace their outline.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRegion.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** THE CEILING ON THE TRACE RASTER, in pixels along the node's longer
 *  side.
 *
 *  The trace rasterises at the NODE'S OWN DEVICE SCALE, so a step of the
 *  staircase it produces is one device pixel and the boundary is as fine
 *  as the edge the viewer is looking at — which is the whole reason to
 *  trace pixels rather than a shape. This is only the bound that keeps a
 *  very large node from asking for a very large surface: past it the
 *  raster is scaled down to fit and the steps grow, which is the same
 *  coarsening a decoration on a huge node can afford and the only
 *  alternative to refusing outright. */
constexpr int kMaxTraceRaster = 2048;

/** A pixel is COVERED when the node's paint reached at least half of it —
 *  the rule an unantialiased rasteriser uses, which puts the traced edge
 *  where the drawn edge is rather than half a pixel outside it. Paint
 *  that never reaches half coverage (a wash, a faint glow) is not a
 *  silhouette and traces to nothing. */
constexpr uint8_t kCovered = 128;

/** THE ALPHA, AS A REGION.
 *
 *  Skia carries no alpha-raster-to-path tracer, and it does carry
 *  everything a region needs: covered pixels become horizontal runs, runs
 *  that repeat down the raster become one band of rects, and SkRegion
 *  turns rects into a region and a region into its boundary path. The
 *  answer is exact for the raster it was given — every edge axis-aligned,
 *  every step one pixel — which is the honest shape of a traced raster and
 *  not an approximation of a smooth curve. */
SkRegion regionOfCoveredPixels(const SkPixmap& alpha) {
  const int width = alpha.width(), height = alpha.height();
  std::vector<SkIRect> rects;
  std::vector<SkIRect> row, previous;  // the runs of this row, and the last
  int bandTop = 0;
  const auto flushBand = [&](int bandBottom) {
    for (const SkIRect& run : previous)
      rects.push_back(
          SkIRect::MakeLTRB(run.left(), bandTop, run.right(), bandBottom));
  };
  for (int y = 0; y < height; ++y) {
    row.clear();
    const uint8_t* pixels = alpha.addr8(0, y);
    int runStart = -1;
    for (int x = 0; x < width; ++x) {
      const bool covered = pixels[x] >= kCovered;
      if (covered && runStart < 0) runStart = x;
      if (!covered && runStart >= 0) {
        row.push_back(SkIRect::MakeLTRB(runStart, 0, x, 1));
        runStart = -1;
      }
    }
    if (runStart >= 0) row.push_back(SkIRect::MakeLTRB(runStart, 0, width, 1));
    // Rows with the same runs are one band of rects rather than one set of
    // rects each: a box traces to a single rect however tall it is, and the
    // rect count a region has to merge stays proportional to the
    // silhouette's detail instead of to its height.
    const bool sameAsAbove =
        row.size() == previous.size() &&
        std::equal(row.begin(), row.end(), previous.begin());
    if (!sameAsAbove) {
      flushBand(y);
      previous = row;
      bandTop = y;
    }
  }
  flushBand(height);
  SkRegion region;
  region.setRects(rects.data(), (int)rects.size());
  return region;
}

}  // namespace

const SkPath& Composer::Impl::coverageOutline(Instance& inst, SkSize size,
                                              float contentScale) {
  const bool sized = size.width() > 0 && size.height() > 0;
  // The device scale is part of the answer, not just of its cost: the
  // staircase is one device pixel a step, so a node that moves to a denser
  // display traces a finer boundary and must be traced again.
  const bool stale = inst.paintDirty || inst.subtreeVolatile ||
                     inst.coverageOutlineSize != size ||
                     inst.coverageOutlineScale != contentScale;
  if (!stale || !sized) {
    if (!sized) {
      inst.coverageOutline.reset();
      inst.coverageOutlineSize = size;
      inst.coverageOutlineScale = contentScale;
    }
    return inst.coverageOutline;
  }
  inst.coverageOutlineSize = size;
  inst.coverageOutlineScale = contentScale;
  inst.coverageOutline.reset();

  const float longer = std::max(size.width(), size.height());
  const float scale = std::min(contentScale > 0 ? contentScale : 1.0f,
                               (float)kMaxTraceRaster / std::max(longer, 1.0f));
  const int width = std::max(1, (int)std::ceil(size.width() * scale));
  const int height = std::max(1, (int)std::ceil(size.height() * scale));
  // One channel, because coverage is the only channel the answer reads.
  const sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeA8(width, height));
  if (!surface) return inst.coverageOutline;
  SkCanvas& canvas = *surface->getCanvas();
  canvas.clear(SK_ColorTRANSPARENT);
  canvas.scale(scale, scale);

  // The trace's canvas is an offscreen raster at a scale of its own, so
  // nothing pinned to a device rect may be baked inside it: it is an
  // UNPINNED recording, the same refusal a recording under a declared
  // motion takes, and its own canvas is the device it draws on.
  //
  // At FULL OPACITY and the plain source blend, which is what the node's
  // layer holds: its own opacity and blend mode are applied to the
  // finished layer above this, and a silhouette that thinned as the node
  // faded would walk a decoration's edge for a reason that has nothing to
  // do with what was drawn.
  const Instance* outerTrace = coverageTrace;
  coverageTrace = &inst;
  const SkMatrix outerReplay = recordingReplay;
  const SkMatrix outerReplayInverse = recordingReplayInverse;
  recordingReplay = SkMatrix::I();
  recordingReplayInverse = SkMatrix::I();
  ++recordingDepth;
  ++unpinnedRecordingDepth;
  paintContent(inst, canvas, contentScale);
  --unpinnedRecordingDepth;
  --recordingDepth;
  recordingReplay = outerReplay;
  recordingReplayInverse = outerReplayInverse;
  coverageTrace = outerTrace;

  SkPixmap alpha;
  if (!surface->peekPixels(&alpha)) return inst.coverageOutline;
  const SkRegion covered = regionOfCoveredPixels(alpha);
  if (covered.isEmpty()) return inst.coverageOutline;
  inst.coverageOutline =
      covered.getBoundaryPath().makeScale(1.0f / scale, 1.0f / scale);
  return inst.coverageOutline;
}

}  // namespace sigil::compose
