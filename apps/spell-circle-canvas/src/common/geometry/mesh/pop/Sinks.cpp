/** @file
 * The sinks a chain ends at: stamped into one mesh with its promoted
 * lanes, treated as a path and swept with a profile, or splatted onto a
 * canvas as camera-facing sprites. Every one of them stands on the cloud
 * the runtime cooked; what each does with it afterwards is its own.
 */

#include <string>

#include "sigilgeometry/mesh/pop/Pop.h"

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
                    bool closed, const curve::SweepOptions& options,
                    const pop::Runtime& runtime) {
  const Spline3 path = pathThrough(chain, closed, runtime);
  if (path.points.size() < 2) return {};
  return curve::sweep(path, profile, options);
}

}  // namespace sigil::geometry::mesh
