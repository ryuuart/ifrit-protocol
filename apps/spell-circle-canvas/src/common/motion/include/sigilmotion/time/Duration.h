#pragma once

/** @file
 * @ingroup motion-time
 *
 * `Duration`, the one time unit every length, delay, step and period in
 * this library is stated in: a `std::chrono` duration counted in seconds
 * as a double, so `380ms`, `1.2s` and `1s / 60.0` all convert to it at a
 * call site with no cast and no unit suffix on a field name.
 */

#include <chrono>

namespace sigil::motion {

/** A LENGTH OF TIME, in seconds as a double.
 *
 *  Every chrono literal converts to it implicitly — `380ms`, `1.2s`,
 *  `std::chrono::microseconds(16667)` — because a double count of seconds
 *  loses nothing an integer count of a smaller unit held. `count()` reads
 *  it back as seconds, which is the unit a phase or a spring is written
 *  in. */
using Duration = std::chrono::duration<double>;

}  // namespace sigil::motion
