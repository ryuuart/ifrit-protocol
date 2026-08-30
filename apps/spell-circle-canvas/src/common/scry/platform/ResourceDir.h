#pragma once

/** @file
 * Where the Ultralight runtime data (icudt67l.dat, cacert.pem) is found
 * at run time. Internal to the library.
 */

#include <string>

namespace sigil::scry {

/** The "resources" directory next to the running executable, if it holds
 *  the Ultralight runtime data (ultralight_copy_resources() stages it
 *  there at build time); empty otherwise. */
std::string executableAdjacentResourceDir();

/** The resource directory the engine boots with: @p configured when
 *  set, else the folder staged next to the executable, else the SDK
 *  location found at configure time and compiled in. */
std::string resolveResourceDir(const std::string& configured);

}  // namespace sigil::scry
