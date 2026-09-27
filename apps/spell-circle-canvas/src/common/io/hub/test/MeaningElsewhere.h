#pragma once

/** @file
 * What the other image of a program does, for the case that a type held
 * under two identities is one meaning: register a decoder for a type of
 * its own, and load it back through its own copy of the type.
 */

#include <sigilio/hub/Hub.h>

#include <cstddef>
#include <optional>
#include <string_view>
#include <typeindex>

namespace sigil::io::test {

/** What a shade loaded elsewhere held, read in the image that loaded it:
 *  its text's length, or nothing when the load answered null. */
std::optional<size_t> loadShadeElsewhere(Hub& hub, std::string_view uri);

/** The identity of this other image's `Shade`, which the cases compare
 *  against their own to know the two are distinct. */
std::type_index shadeIdentityElsewhere();

/** Registers, from this other image, the decoder of its own `Shade` —
 *  the text of the resource, whole. */
void registerShadeElsewhere(Hub& hub);

}  // namespace sigil::io::test
