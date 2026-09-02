#pragma once

/** @file
 * The mesh painter that draws on a device: the same `MeshStyle`, the
 * same geometry and the same canvas, rasterised instead of sorted.
 */

#include <sigilgeometry/mesh/render/Runtime.h>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::world::diligent {

/**
 * The `render::Runtime` whose executor draws on @p device.
 *
 * WHAT RUNS WHERE. A mesh draw is one pipeline over the mesh's vertices:
 * the style's three modes are a uniform rather than three programs, the
 * shading is per vertex in view space exactly as the host executor's is,
 * the primitive lane multiplies the shaded colour, and the texture is
 * read through the sampler its placement, its wrap and its filter ask
 * for. The pixels are then READ BACK and drawn onto the canvas the
 * caller passed, premultiplied, under whatever transform that canvas
 * carries — so a mesh lands where the host executor would have put it
 * whatever surface the canvas is.
 *
 * A PANEL DRAW IS THE CANVAS'S OWN. `drawPanel` concats the perspective
 * transform and runs the caller's 2D content, which is what the host
 * executor does, because that content is Skia's to draw and a device has
 * nothing to add to it: a panel on a GPU-backed canvas was already on
 * the GPU. The two executors therefore agree about a panel exactly,
 * which is what the parity test asserts.
 *
 * WHAT DIFFERS FROM THE HOST, and neither is a shading disagreement:
 * the host sorts triangles back to front and antialiases their edges,
 * while this depth-tests them and does not. So the two draw the same
 * picture and not the same bytes, and what they disagree about is a
 * silhouette.
 *
 * WHAT THIS COSTS, said plainly: one readback per mesh draw. A canvas
 * does not name the texture behind it, so there is nothing to compare
 * against this device to decide that the pixels could be bound where
 * they stand instead — the zero-copy path SigilSkia offers needs a
 * caller who holds both the surface and the device, which a
 * `render::Executor` is not handed.
 *
 * WHEN A DRAW MAY BE TAKEN. Each mesh draw opens and closes a device
 * frame of its own, because the heap a draw's uniforms are written into
 * is refilled once a frame and a run of draws that never finished one
 * would exhaust it. The device's command context is shared with every
 * other runtime standing on it, so a draw taken from INSIDE a frame's
 * pass body would close that frame early. Draw between frames — which is
 * where a canvas draw stands anyway.
 *
 * Two runtimes made by one call to this compare equal; two separate
 * calls do not, because they hold separate device state.
 */
::sigil::geometry::mesh::render::Runtime painterRuntime(
    ::sigil::geometry::device::Device& device);

}  // namespace sigil::world::diligent
