#pragma once

/** @file
 * @ingroup io-transport
 * What a test reaches for when it stands in for a transport: the one
 * way to put a message on a feed without opening a socket.
 */

#include "sigilio/hub/Feed.h"

namespace sigil::io::testing {

/** THE PRODUCER'S SIDE OF A FEED THE TEST HOLDS: every message delivered
 *  through it arrives on @p feed as a transport's would, with the sender
 *  and the time it names. A feed a hub opened keeps its transport's end;
 *  the inlet is a second way in beside it. */
Inlet inletOf(const Feed& feed);

}  // namespace sigil::io::testing
