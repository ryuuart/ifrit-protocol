#pragma once

/** @file
 * The Syphon server behind the seam, reached only by the factory beside
 * it: Objective-C stays inside the one translation unit that talks to
 * the framework.
 */

#include <sigilsketch/publish/Publisher.h>

#include <memory>
#include <string>

namespace sigil::sketch {

/** A Syphon server named @p name on @p mtlDevice, or null when the
 *  server could not be stood up. */
std::unique_ptr<Publisher> makeSyphonPublisher(std::string name,
                                               void* mtlDevice);

}  // namespace sigil::sketch
