/** @file
 * The mesh-forming sinks: a cooked chain stamped into one mesh with its
 * promoted lanes, or treated as a path and swept with a profile.
 */

#include <string>

#include "sigilgeometry/mesh/pop/Pop.h"

namespace sigil::geometry::mesh {

// The tier's other features this file stands on, pulled in so the code
// below reads as one vocabulary.
using curve::Spline3;
using path::Polyline;

namespace {

/** pop's attribute names -> the Cloud lane cook() exported them under.
 *  The builtins land on the conventional lowercase lanes; "Tex" and
 *  every custom keep their own name. One table, so the prim class
 *  addresses attributes with exactly the same spelling the point class
 *  does. "Id" is reserved and handled by promoteToPrims. */
std::string cloudLaneFor(const std::string& attr) {
  if (attr == "T") return "t";
  if (attr == "Dir") return "dir";
  if (attr == "Scale") return "size";
  if (attr == "Color") return "tint";
  return attr;  // "P" has no lane; "Tex" and customs keep their names
}

}  // namespace

Mesh pop::cookMesh(const pop::Chain& chain, const Mesh& stamp) {
  const Cloud cloud = cook(chain);
  points::InstanceOptions options;
  options.orientLane = "dir";
  options.scaleLane = "size";
  options.tintLane = "tint";
  Mesh out = points::instance(cloud, stamp, options);
  // The texture hint: "Tex" = {uOff, vOff, uScale, vScale} per point
  // remaps each stamped point's uv block — atlas selection, sprite
  // variety, per-point texture windows.
  if (const std::vector<glm::vec4>* tex = cloud.colorIf("Tex")) {
    const size_t stampVerts = stamp.vertexCount();
    // A uv-less stamp instances with an EMPTY uv lane; only remap
    // when the instanced uvs actually cover every stamped vertex.
    if (out.uvs.size() == cloud.size() * stampVerts) {
      for (size_t point = 0; point < cloud.size(); ++point) {
        const glm::vec4& cell = (*tex)[point];
        for (size_t v = 0; v < stampVerts; ++v) {
          glm::vec2& uv = out.uvs[point * stampVerts + v];
          uv = {cell.x + uv.x * cell.z, cell.y + uv.y * cell.w};
        }
      }
    }
  }
  // The PRIMITIVE class: every Promote op bakes a point lane onto the
  // stamped triangles. Each point owns stamp.triangleCount() of them,
  // which is exactly the run points::promoteToPrims addresses.
  for (const pop::Op& op : chain)
    if (const auto* promote = std::get_if<pop::Promote>(&op))
      points::promoteToPrims(
          out, cloud,
          promote->from.name == "Id" ? "Id" : cloudLaneFor(promote->from.name),
          promote->to.empty() ? promote->from.name : promote->to);
  return out;
}

namespace {

Spline3 pathThrough(const pop::Chain& chain, bool closed) {
  Spline3 path;
  path.points = pop::cook(chain).positions;
  path.closed = closed;
  return path;
}

}  // namespace

Mesh pop::cookSweep(const pop::Chain& chain, const Polyline& profile,
                    bool closed, const curve::SweepOptions& options) {
  const Spline3 path = pathThrough(chain, closed);
  if (path.points.size() < 2) return {};
  return curve::sweep(path, profile, options);
}

}  // namespace sigil::geometry::mesh
