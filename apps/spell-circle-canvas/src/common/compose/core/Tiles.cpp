/** @file
 * tiles::, the slicing of one baked picture into a run of tile-sized
 * rasters: the window a tile index names.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <sigilgeometry/path/Skia.h>

#include "ComposeRuntime.h"

namespace sigil::compose {

namespace tiles {

geometry::path::Transform window(glm::vec2 tile, int index, Flow flow,
                                 Facing facing) {
  const float w = tile.x;
  const float h = tile.y;
  const float step = -(float)index * (flow == Flow::Down ? h : w);
  // The step runs ALONG the flow; the mirror, when asked for, runs ACROSS
  // it — the axis perpendicular to the slicing. Both are written out as
  // one matrix so no call site has to get the concat order right.
  if (flow == Flow::Down) {
    return geometry::path::fromSk(
        facing == Facing::Mirrored
            ? SkMatrix::MakeAll(-1, 0, w, 0, 1, step, 0, 0, 1)
            : SkMatrix::MakeAll(1, 0, 0, 0, 1, step, 0, 0, 1));
  }
  return geometry::path::fromSk(
      facing == Facing::Mirrored
          ? SkMatrix::MakeAll(1, 0, step, 0, -1, h, 0, 0, 1)
          : SkMatrix::MakeAll(1, 0, step, 0, 1, 0, 0, 0, 1));
}

}  // namespace tiles

}  // namespace sigil::compose
