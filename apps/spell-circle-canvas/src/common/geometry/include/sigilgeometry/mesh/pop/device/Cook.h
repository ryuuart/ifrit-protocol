#pragma once

/** @file
 * The point-operator chain cooked on a device: the same chain, the same
 * lanes and the same answer, dispatched instead of stepped.
 */

#include <sigilgeometry/mesh/pop/Pop.h>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::geometry::mesh::pop {

/** THE DEVICE EXECUTOR, beside the CPU one: the `Runtime` that cooks a
 *  chain on @p device by dispatching the kernel this build compiled,
 *  rather than stepping the C++ that came out of the same source. The
 *  two are held to bit identity because there is one arithmetic and
 *  neither side re-derives it.
 *
 *  WHAT RUNS WHERE. A chain's GENERATOR is not a map over points — it
 *  is what makes them — so it is run on the host and its lanes
 *  uploaded, which is what makes the seed the two tiers share
 *  bit-identical. Every operator after it that has a kernel is one
 *  compute dispatch over those lanes, in chain order, and the cooked
 *  lanes are read back once at the end.
 *
 *  A RUNTIME OUTLIVES A CHAIN. The lane buffers are kept between cooks
 *  while their size still fits, and only the lanes THIS cook seeded or
 *  dispatched are read back — so a second chain through one runtime
 *  answers its own lanes and not the ones before it left behind.
 *
 *  WHAT IT DECLINES, and why each is a boundary rather than a gap:
 *  `Relax` reads points it does not own, so one lane cannot be both
 *  what is read and what is written; `Sort` is a permutation, which is
 *  a sorting network and not a per-point map; `Promote` addresses the
 *  primitives a sink has not formed yet; and `Noise` and `Deform` are
 *  defined in terms of a library sine, which is a different function
 *  from the polynomial a portable kernel would have to use — a kernel
 *  for either would change what the operator MEANS rather than where
 *  it runs. A chain holding one of them stops the cook with a message
 *  naming the operator and this runtime, the way any unsupported
 *  operator does.
 *
 *  Two runtimes made by one call to this compare equal; two separate
 *  calls do not, because they hold separate device state. */
Runtime deviceRuntime(::sigil::geometry::device::Device& device);

}  // namespace sigil::geometry::mesh::pop
