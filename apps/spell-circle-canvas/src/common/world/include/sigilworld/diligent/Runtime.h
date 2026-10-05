#pragma once

/** @file
 * @ingroup world-diligent
 * The GPU executor, as a VALUE: the one that performs a frame's passes,
 * on a device.
 */

#include <sigilworld/advanced/Runtime.h>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

/** THE DEVICE SIDE OF SIGILWORLD, ON DILIGENT ENGINE: the executor that
 *  records a frame's passes on a graphics device, and the import that
 *  brings a mesh across to it. Reach for it to run a frame on real
 *  hardware; everything above it is written against the executor seam
 *  and never names this catalogue. */
namespace sigil::world::diligent {

/** The executor that performs a frame's passes on @p device. The
 * frame's resources live there for as long as the executor does, and
 * nothing crosses back until something asks: the value installs itself
 * as the `Targets::ImageSource`. Every value made from one device shares
 * its queue.
 * @trap Two frames carrying the value ONE call made compare equal; two
 * separate calls do not, holding separate device state.
 */
Runtime runtime(::sigil::geometry::device::Device& device);

}  // namespace sigil::world::diligent
