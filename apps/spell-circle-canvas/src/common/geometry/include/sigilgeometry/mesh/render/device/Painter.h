#pragma once

/** @file
 * @ingroup geometry-device
 *
 * The mesh painter that draws on a device: the same `MeshStyle`, the
 * same geometry and the same canvas, rasterised instead of sorted.
 */

#include <sigilgeometry/mesh/render/Runtime.h>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::geometry::mesh::render {

/** THE `Runtime` WHOSE EXECUTOR DRAWS ON @p device — the device twin of
 *  `Runtime::cpu()`. A mesh draw is one pipeline over the mesh's
 *  vertices, shaded per vertex as the host executor shades, and the
 *  pixels are READ BACK and drawn premultiplied onto the canvas passed;
 *  a panel draw is the canvas's own. Two runtimes from one call compare
 *  equal, two calls do not.
 *  @trap Each mesh draw opens and closes a device frame of its own, so
 *  a draw taken from inside a frame's pass body would close it early:
 *  draw BETWEEN frames, which is where a canvas draw stands anyway. */
Runtime deviceRuntime(::sigil::geometry::device::Device& device);

}  // namespace sigil::geometry::mesh::render
