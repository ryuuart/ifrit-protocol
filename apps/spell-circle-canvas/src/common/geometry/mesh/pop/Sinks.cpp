/** @file
 * The sinks a chain ends at: stamped into one mesh with its promoted
 * lanes, treated as a path and swept with a profile, or splatted onto a
 * canvas as camera-facing sprites. Every one of them stands on the cloud
 * the runtime cooked; what each does with it afterwards is its own.
 */

#include <algorithm>
#include <cstdint>
#include <glm/geometric.hpp>
#include <string>
#include <vector>

#include "sigilgeometry/mesh/pop/Pop.h"
#include "sigilgeometry/path/Neighbours.h"

namespace sigil::geometry::mesh {

// The tier's other features this file stands on, pulled in so the code
// below reads as one vocabulary.
using curve::Spline3;
using path::Polyline;

namespace {

}  // namespace

Mesh pop::cookMesh(const pop::Chain& chain, const Mesh& stamp,
                   const pop::Runtime& runtime) {
  const Cloud cloud = cook(chain, runtime);
  // The texture hint — "Tex" = {uOff, vOff, uScale, vScale} per point,
  // which remaps each stamped vertex's uv for atlas selection and sprite
  // variety — is the stamping operator's own, applied as the vertex is
  // formed rather than walked over afterwards.
  Mesh out = points::instance(cloud, stamp, points::stampOptions(cloud));
  // The PRIMITIVE class: every Promote op bakes a point lane onto the
  // stamped triangles. Each point owns stamp.triangleCount() of them,
  // which is exactly the run points::promoteToPrims addresses.
  for (const pop::Op& op : chain)
    if (const auto* promote = std::get_if<pop::Promote>(&op))
      points::promoteToPrims(
          out, cloud,
          promote->from.name == "Id"
              ? "Id"
              : std::string(cloudLaneFor(promote->from.name)),
          promote->to.empty() ? promote->from.name : promote->to);
  return out;
}

namespace {

Spline3 pathThrough(const pop::Chain& chain, bool closed,
                    const pop::Runtime& runtime) {
  Spline3 path;
  path.points = pop::cook(chain, runtime).positions;
  path.closed = closed;
  return path;
}

}  // namespace

void pop::cookBillboards(const pop::Chain& chain, SkCanvas& canvas,
                         const camera::Camera& camera, SkSize viewport,
                         const points::BillboardStyle& style,
                         const pop::Runtime& runtime) {
  // The size and tint lanes a cook exports are "size" and "tint"; a
  // style that named neither takes them, so a chain that varied either
  // shows it without the caller repeating the table.
  points::BillboardStyle splat = style;
  const Cloud cloud = cook(chain, runtime);
  if (splat.sizeLane.empty() && cloud.scalarIf("size")) splat.sizeLane = "size";
  if (splat.tintLane.empty() && cloud.colorIf("tint")) splat.tintLane = "tint";
  points::drawBillboards(canvas, cloud, camera, viewport, splat);
}

Mesh pop::cookSweep(const pop::Chain& chain, const Polyline& profile,
                    bool closed, const pop::SweepOptions& options,
                    const pop::Runtime& runtime) {
  const Spline3 path = pathThrough(chain, closed, runtime);
  if (path.points.size() < 2) return {};
  return pop::sweep(path, profile, options);
}

std::vector<glm::uvec2> pop::connectAdjacent(const Cloud& cloud,
                                             const pop::Connect& connect) {
  std::vector<glm::uvec2> pairs;
  if (cloud.positions.size() < 2 || !(connect.radius > 0)) return pairs;

  const std::vector<float>* pieces =
      connect.pieceLane.empty() ? nullptr : cloud.scalarIf(connect.pieceLane);
  const path::Neighbours index(cloud.positions, connect.radius);
  std::vector<uint32_t> found;
  std::vector<uint32_t> keep;
  for (uint32_t i = 0; i < (uint32_t)cloud.positions.size(); ++i) {
    const glm::vec3 here = cloud.positions[i];
    index.within(here, connect.radius, found);
    keep.clear();
    for (const uint32_t other : found) {
      if (other == i) continue;
      if (connect.acrossPiecesOnly) {
        // Without a piece lane every point is in the same piece, so
        // nothing is across one — which is what the caller asked for.
        if (!pieces || (*pieces)[other] == (*pieces)[i]) continue;
      }
      keep.push_back(other);
    }
    if (connect.maxPerPoint > 0 && keep.size() > (size_t)connect.maxPerPoint) {
      // Nearest first, and only as many as asked for. The whole list is
      // ordered rather than the head selected, so which neighbours a point
      // keeps does not depend on the order the grid answered in.
      std::sort(keep.begin(), keep.end(), [&](uint32_t a, uint32_t b) {
        const float da = glm::length(cloud.positions[a] - here);
        const float db = glm::length(cloud.positions[b] - here);
        return da != db ? da < db : a < b;
      });
      keep.resize((size_t)connect.maxPerPoint);
    }
    // Lower index first, so a pair found from both ends is one entry and
    // a pair only one end kept — which `maxPerPoint` makes possible — is
    // still one edge rather than none.
    for (const uint32_t other : keep)
      pairs.push_back(i < other ? glm::uvec2{i, other}
                                : glm::uvec2{other, i});
  }
  std::sort(pairs.begin(), pairs.end(), [](glm::uvec2 a, glm::uvec2 b) {
    return a.x != b.x ? a.x < b.x : a.y < b.y;
  });
  pairs.erase(std::unique(pairs.begin(), pairs.end(),
                          [](glm::uvec2 a, glm::uvec2 b) {
                            return a.x == b.x && a.y == b.y;
                          }),
              pairs.end());
  return pairs;
}

}  // namespace sigil::geometry::mesh
