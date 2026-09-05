/** @file
 * The Graphite context a plate is drawn through, read off the device the
 * process holds.
 */

#include "sigilsketch/plate/Graphite.h"

#include <sigilgeometry/device/Device.h>
#include <sigilsketch/core/Device.h>

namespace sigil::sketch {

skia::GraphiteContext* deviceGraphite() {
  geometry::device::Device* const on = device();
  return on ? on->graphite() : nullptr;
}

}  // namespace sigil::sketch
