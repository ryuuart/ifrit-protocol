#pragma once

/** @file
 * A stamp copied onto every point of a cloud on a device: the vertices
 * dispatched, the indices left on the host.
 */

#include <sigilgeometry/mesh/pop/Stamp.h>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::geometry::mesh::points {

/**
 * THE DEVICE EXECUTOR, beside the CPU one: the `StampRuntime` that forms
 * a stamping's vertices on @p device.
 *
 * WHAT RUNS WHERE. The VERTICES — the per-vertex arithmetic, which is
 * the whole of what stamping computes — are one compute dispatch over
 * the stamp and the points, read back once at the end. The indices stay
 * on the host and are not a second piece of arithmetic: each point
 * contributes the stamp's own indices shifted by where its vertices
 * begin, which is integer.
 *
 * THE TWO TIERS ARE HELD TO BIT IDENTITY, not to a distance, for the
 * same reason the swept rings are: one piece of Slang compiled twice
 * under a float model pinned at both ends.
 *
 * Two runtimes made by one call to this compare equal; two separate
 * calls do not, because they hold separate device state.
 */
StampRuntime deviceRuntime(::sigil::geometry::device::Device& device);

}  // namespace sigil::geometry::mesh::points
