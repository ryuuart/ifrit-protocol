#pragma once

/** @file
 * Whether the web engine this library was built against can boot on this
 * machine: the ICU tables and CA certificates it lays pages out with are
 * distributed with the application rather than with the dylibs, so a
 * build whose resource folder never arrived links and starts and then
 * renders nothing.
 */

#include <string>

namespace sigil::scry::runtime {

/**
 * True when the resource directory the engine would boot with holds the
 * runtime data it needs. When false, @p why names what is missing and
 * where it was looked for.
 *
 * It answers for the MACHINE, not for the process: it creates no
 * renderer and consumes nothing, so a caller may ask before deciding
 * whether to create the one renderer a process is allowed.
 */
bool available(std::string* why = nullptr);

}  // namespace sigil::scry::runtime
