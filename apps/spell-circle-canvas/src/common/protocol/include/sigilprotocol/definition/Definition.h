#pragma once

/** @file
 * @ingroup protocol-definition
 * THE DEFINITION ITSELF, as the bytes a client reads to learn what a
 * host speaks.
 */

#include <cstddef>
#include <span>

namespace sigil::protocol {

/** THE DEFINITION AS ITS REFLECTED SCHEMA: the bytes flatc writes for
 *  protocol.fbs with its documentation, its attributes and its services
 *  kept — every domain, command, event and table this build speaks. What
 *  an endpoint serves to a client that asks, and what the generator
 *  read. The bytes live as long as the program does. */
std::span<const std::byte> definition();

}  // namespace sigil::protocol
