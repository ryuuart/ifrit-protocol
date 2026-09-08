#pragma once

/** @file
 * THE SINKS a cooked chain is spent into: the pairs of points near
 * enough to be joined, one Mesh with a stamp at every point, one Mesh
 * with a profile carried along the points as a rail, and camera-facing
 * sprites splatted straight onto a canvas.
 *
 * A sink stands on the cloud a cook produced rather than on the chain,
 * which is why topology lives here and not in the Cloud: a cloud is one
 * value per point, and an edge is two points.
 */

#include <string>
#include <vector>

#include "sigilgeometry/mesh/pop/Runtime.h"
#include "sigilgeometry/mesh/pop/Sweep.h"

namespace sigil::geometry::mesh {
namespace pop {

/** WHICH POINTS ARE NEAR ENOUGH TO BE JOINED, and to what.
 *
 *  `radius` is how far a point looks. `maxPerPoint` bounds how many
 *  neighbours one point may reach — zero is every neighbour in range, and
 *  one is "join each point to whichever is nearest", which is what a
 *  constellation of a scatter is. `pieceLane` names a scalar lane whose
 *  .x says which piece a point belongs to; with `acrossPiecesOnly` set,
 *  only points of DIFFERENT pieces are joined, which is what makes this
 *  a bridge between islands rather than a mesh over one. An unnamed piece
 *  lane leaves every point in the same piece, so `acrossPiecesOnly` must
 *  be off for any pair to be found at all. */
struct Connect {
  float radius = 1;
  int maxPerPoint = 0;
  std::string pieceLane;
  bool acrossPiecesOnly = false;
  bool operator==(const Connect&) const = default;
};

/** THE CONNECTION SINK: the pairs of points within reach of each other,
 *  each pair once, the lower index first.
 *
 *  It is a SINK and not an operator, and that is the whole answer to
 *  where the edges live: a `Cloud` is positions plus lanes, all parallel
 *  and all one value per point, and topology is neither — one edge is two
 *  points, and a point may own any number of them. Growing the cloud a
 *  second kind of storage would make every operator, every lane
 *  accessor, every append and every executor answer for it, and the
 *  operators would still not read it. Answering the pairs instead costs
 *  the cloud nothing and is the currency every consumer of them already
 *  wants: a line for the canvas, an edge for a mesh, a spring for a
 *  solver. A caller that wants the pairs drawn walks them and draws
 *  them; a caller that wants them cooked hands them to whatever forms
 *  the primitives. */
std::vector<glm::uvec2> connectAdjacent(const Cloud& cloud,
                                        const Connect& connect);

/** The mesh-forming sink: cook @p chain and stamp @p stamp at every
 *  point into ONE Mesh (dir orients, size scales, tint colors) — a
 *  pop-DESCRIBED 3D model, drawable by render::drawMesh on the Skia
 *  painter and placeable in a 3D set alike. The cook runs on @p runtime;
 *  the stamping stands on its cloud. */
Mesh cookMesh(const Chain& chain, const Mesh& stamp,
              const Runtime& runtime = Runtime::cpu());

/** The swept sink: the chain's cooked points become the PATH — a
 *  Catmull-Rom through P in chain order, closed by @p closed, so
 *  Jitter/Noise/Math edits BEND the sweep — and pop::sweep carries
 *  @p profile along it. Any 2D cross-section works, from
 *  sections::circle() to a star flattened out of a path operator. The same
 * nondestructive description, a different former. */
Mesh cookSweep(const Chain& chain, const path::Polyline& profile,
               bool closed = false,
               const SweepOptions& options = {.segments = 160},
               const Runtime& runtime = Runtime::cpu());

/** The splatting sink: the chain's cooked points drawn onto @p canvas
 *  as camera-facing sprites — the one sink that forms no geometry,
 *  because a billboard faces the eye and so is answered where the eye
 *  is rather than in the world. Sizes come from the "size" lane and
 *  tints from "tint" wherever @p style names them, the cook runs on
 *  @p runtime, and the splatting stands on its cloud. */
void cookBillboards(const Chain& chain, SkCanvas& canvas,
                    const camera::Camera& camera, SkSize viewport,
                    const points::BillboardStyle& style = {},
                    const Runtime& runtime = Runtime::cpu());

}  // namespace pop
}  // namespace sigil::geometry::mesh
