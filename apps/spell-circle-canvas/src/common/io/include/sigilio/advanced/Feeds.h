#pragma once

/** @file
 * @ingroup io-hub
 * Every feed a hub has open — what a wire inspector lists.
 */

#include <vector>

#include "sigilio/hub/Feed.h"

namespace sigil::io {

class Hub;

/** Every feed currently held by someone on @p hub, in opening order. */
std::vector<Feed> feeds(const Hub& hub);

}  // namespace sigil::io
