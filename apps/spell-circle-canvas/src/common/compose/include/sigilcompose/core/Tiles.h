#pragma once

/** @file
 * SigilCompose tiles — slicing one baked picture into a run of tile-sized
 * rasters: the per-tile canvas transform, and the bounding-box re-record
 * that makes replaying the whole picture per tile cheap.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>

namespace sigil::compose {

/** Slicing ONE baked picture into a run of tiles.
 *
 *  A strip far longer than any texture — a marquee, a scrolling ribbon, a
 *  hanging scroll — is authored as a single element tree and baked with
 *  `snapshot()`, which has no size limit because a picture is vector. The
 *  consumer then wants it as N tile-sized rasters. That slice is a clip
 *  and a translate and nothing else: **there is no windowed bake, and
 *  there is no need for one.** Replaying the whole picture per tile,
 *  behind `sliceable()` below, is as cheap as extracting each tile's ops
 *  in advance would be.
 *
 *  What DOES go wrong is the transform, and that is what these two verbs
 *  exist to own.
 *
 *  **Author the strip in the tiles' own orientation.** If the tiles are
 *  tall, the tree is a `column()`; if they are wide, a `row()`. The
 *  temptation is to author across and transpose on the way out, and a
 *  transpose has determinant -1 — it composes with whatever mirroring the
 *  consumer's own sampling already applies, and the mirror bookkeeping
 *  stops being local to either side. `Flow` therefore offers only the two
 *  non-transposing slices, on purpose.
 *
 *  **`Facing` is a statement about the CONSUMER, not the picture.** A
 *  texture sampled onto a surface whose u runs backwards — a ribbon wall
 *  mirrors its own u — shows glyphs reversed unless the tile was baked
 *  reversed to match. `Facing::Mirrored` pre-flips ACROSS the strip, on
 *  the axis perpendicular to `flow`, so that such a consumer reads it the
 *  right way round. Get this wrong and the art is legible in an offline
 *  PNG of the tile and mirrored on the surface, so it will not show up
 *  until the texture is in place. */
namespace tiles {

/** Which way the run of tiles marches through the picture. */
enum class Flow {
  Down,   ///< a column strip: tile k is the k-th slice down
  Across  ///< a row strip: tile k is the k-th slice rightward
};

/** Whether the tile is pre-flipped for a consumer that samples mirrored. */
enum class Facing {
  Forward,  ///< the tile reads like the picture
  Mirrored  ///< flipped across the strip, for mirrored sampling
};

/** The canvas transform that brings tile @p index of a @p tile -sized run
 *  into view. Concat it, then draw the picture:
 *
 *  ```
 *  SkAutoCanvasRestore restore(canvas, true);
 *  canvas->clear(SK_ColorTRANSPARENT);
 *  canvas->concat(tiles::window(size, k, Flow::Down, Facing::Mirrored));
 *  canvas->drawPicture(strip);
 *  ```
 *
 *  The surface's own bounds are the clip, so nothing else is needed —
 *  neighbouring tiles share their boundary texels and the seams vanish. */
SkMatrix window(SkISize tile, int index, Flow flow = Flow::Down,
                Facing facing = Facing::Forward);

/** The same picture, re-recorded behind a bounding-box hierarchy, so each
 *  `window()` replay visits only the ops that meet its tile instead of all
 *  of them.
 *
 *  Worth it past a handful of tiles and not before: building the
 *  hierarchy costs a pass over the picture, which a two-tile run does not
 *  earn back. Slicing WITHOUT it is quadratic, because every tile walks
 *  every tile's ops, so the saving grows with the tile count while the
 *  build cost does not.
 *
 *  It exists as a verb because the obvious one-liner has a trap:
 *  `drawPicture()` into a recorder stores a NESTED reference the
 *  bounding-box hierarchy cannot see into, leaving the tree empty and the
 *  replay cost unchanged. This flattens with `playback()` instead. */
sk_sp<SkPicture> sliceable(const sk_sp<SkPicture>& art);

}  // namespace tiles

}  // namespace sigil::compose
