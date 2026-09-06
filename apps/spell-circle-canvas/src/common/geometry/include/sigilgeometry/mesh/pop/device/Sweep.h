#pragma once

/** @file
 * A profile carried along a rail on a device: the ring vertices
 * dispatched, everything the rings are joined into left on the host.
 */

#include <sigilgeometry/mesh/pop/Sweep.h>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::geometry::mesh::pop {

/**
 * THE DEVICE EXECUTOR, beside the CPU one: the `SweepRuntime` that forms
 * a sweep's rings on @p device.
 *
 * WHAT RUNS WHERE. The RING VERTICES — the per-vertex arithmetic, which
 * is the whole of what a sweep computes — are one compute dispatch over
 * the rail and the profile, read back once at the end. Everything else a
 * sweep is made of stays on the host and is not a second piece of
 * arithmetic: the quads that join four ring vertices and the fan that
 * closes an end are integer, the taper is an arbitrary host function
 * evaluated once per ring, and a geometric normal is a reduction over
 * triangles that do not exist until the vertices do.
 *
 * THE TWO TIERS ARE HELD TO BIT IDENTITY, not to a distance. That is
 * possible because the ring arithmetic is one piece of Slang compiled
 * twice under a float model pinned at both ends: the generated C++ is
 * compiled with contraction off, the SPIR-V is compiled precise and so
 * carries one `NoContraction` decoration per arithmetic result, and the
 * device's driver is told not to relax its floating point before the
 * instance exists.
 *
 * Two runtimes made by one call to this compare equal; two separate
 * calls do not, because they hold separate device state.
 */
SweepRuntime sweepDeviceRuntime(::sigil::geometry::device::Device& device);

}  // namespace sigil::geometry::mesh::pop
