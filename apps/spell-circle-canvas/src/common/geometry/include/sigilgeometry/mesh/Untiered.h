#pragma once

/** @file
 * TRANSITIONAL. The mesh tier's names, also reachable without naming
 * the tier, plus `space` as a scope over the camera and the renderer —
 * the two features the one former scope split into. A caller still
 * spelling `sigil::geometry::Mesh`, `sigil::geometry::decode::model` or
 * `sigil::geometry::space::Camera` compiles while it moves to the tier
 * spellings. Delete this file, and the include of it in Mesh.h, once no
 * caller needs it.
 */

namespace sigil::geometry {

namespace mesh {
namespace camera {}
namespace render {}
namespace codec {
namespace decode {}
namespace encode {}
}  // namespace codec
using namespace codec;
}  // namespace mesh

using namespace mesh;

namespace space {
using namespace mesh::camera;
using namespace mesh::render;
}  // namespace space

}  // namespace sigil::geometry
