#pragma once

/** @file
 * @ingroup compose-core
 *
 * SigilCompose tiles — slicing one baked picture into a run of tile-sized
 * rasters: the per-tile canvas transform, and the bounding-box re-record
 * that makes replaying the whole picture per tile cheap.
 */

#include <glm/vec2.hpp>
#include <sigilgeometry/path/Transform.h>

namespace sigil::compose {

/** Slicing ONE baked picture into a run of tiles.
 *
 *  A strip far longer than any texture — a marquee, a scrolling ribbon, a
 *  hanging scroll — is authored as a single element tree and baked with
 *  `snapshot()`, which has no size limit because a picture is vector. The
 *  consumer then wants it as N tile-sized rasters. That slice is a clip
 *  and a translate and nothing else: **there is no windowed bake, and
 *  there is no need for one.** Replaying the whole picture per tile,
 *  recorded behind a bounding-box hierarchy with `snapshot`'s
 *  `SnapshotOptions::sliceable`, is as cheap as extracting each tile's
 *  ops in advance would be.
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

/** The transform that brings tile @p index of a run of @p tile -sized
 *  tiles into view: concatenate it onto the tile's canvas, then draw the
 *  whole picture.
 *
 *  The surface's own bounds are the clip, so nothing else is needed —
 *  neighbouring tiles share their boundary texels and the seams vanish. */
geometry::path::Transform window(glm::vec2 tile, int index,
                                 Flow flow = Flow::Down,
                                 Facing facing = Facing::Forward);

}  // namespace tiles

}  // namespace sigil::compose
