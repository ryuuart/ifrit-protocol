#pragma once

/** @file
 * @ingroup io-hub
 * THE HOST'S CLOCK ON A HUB: what moves every replayed recording forward
 * and runs what stands on the hub's advance. A driving host calls
 * advance() once a frame; a reader of feeds never does.
 */

#include <chrono>

#include "sigilio/advanced/Lease.h"

namespace sigil::io {

class Hub;

/** Advances every replayed recording on @p hub to the steady time since
 *  it was made. A live feed is unaffected. */
void advance(Hub& hub);

/** The same, to @p time — an absolute time on the caller's own clock,
 *  not a step. */
void advance(Hub& hub, std::chrono::duration<double> time);

/** Runs @p callback on every advance of @p hub for as long as the lease
 *  lives: given the time that advance was given, after every replayed
 *  recording has been advanced to it, on the advancing thread and in the
 *  order the callbacks were registered.
 *  @trap A callback registered from inside an advance runs from the
 *  NEXT one. */
[[nodiscard]] Lease onAdvance(Hub& hub, Lease::Callback callback);

}  // namespace sigil::io
