#pragma once

/** @file
 * @ingroup motion-time
 *
 * `Duration`, the one time unit every length, delay, step and period in
 * this library is stated in, and `readDuration`, which reads one from CSS
 * text. Both are SigilCore's, so a library that measures a span and one
 * that animates over it state it in the same unit without linking each
 * other; they are named here as well so a motion caller spells them in
 * its own namespace.
 */

#include <sigilcore/compute/Duration.h>

namespace sigil::motion {

using core::Duration;
using core::readDuration;

}  // namespace sigil::motion
