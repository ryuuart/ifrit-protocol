/** @file
 * tiles::, the slicing of one baked picture into a run of tile-sized
 * rasters: the window a tile index names, and the picture a slice is
 * taken from.
 */

#include <include/core/SkBBHFactory.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>

#include "ComposeRuntime.h"

namespace sigil::compose {

namespace tiles {

SkMatrix window(SkISize tile, int index, Flow flow, Facing facing) {
  const float w = (float)tile.width();
  const float h = (float)tile.height();
  const float step = -(float)index * (flow == Flow::Down ? h : w);
  // The step runs ALONG the flow; the mirror, when asked for, runs ACROSS
  // it — the axis perpendicular to the slicing. Both are written out as
  // one matrix so no call site has to get the concat order right.
  if (flow == Flow::Down) {
    return facing == Facing::Mirrored
               ? SkMatrix::MakeAll(-1, 0, w, 0, 1, step, 0, 0, 1)
               : SkMatrix::MakeAll(1, 0, 0, 0, 1, step, 0, 0, 1);
  }
  return facing == Facing::Mirrored
             ? SkMatrix::MakeAll(1, 0, step, 0, -1, h, 0, 0, 1)
             : SkMatrix::MakeAll(1, 0, step, 0, 1, 0, 0, 0, 1);
}

sk_sp<SkPicture> sliceable(const sk_sp<SkPicture>& art) {
  if (!art) return nullptr;
  SkRTreeFactory rtree;
  SkPictureRecorder recorder;
  // playback(), NOT drawPicture(): drawPicture on a recording canvas stores
  // a nested reference the hierarchy cannot index into, which leaves the
  // tree empty and the slice exactly as expensive as before.
  art->playback(recorder.beginRecording(art->cullRect(), &rtree));
  return recorder.finishRecordingAsPicture();
}

}  // namespace tiles

}  // namespace sigil::compose
