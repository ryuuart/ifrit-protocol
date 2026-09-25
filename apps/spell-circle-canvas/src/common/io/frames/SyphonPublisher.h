#pragma once

/** @file
 * The Syphon server behind the seam, reached only by the factory beside
 * it: Objective-C stays inside the one translation unit that talks to
 * the framework.
 */

#include <sigilio/frames/Publisher.h>

#include <memory>
#include <string>

namespace sigil::io::frames {

/** A Syphon server named @p name on @p metalDevice, or null when the
 *  server could not be stood up. */
std::unique_ptr<Publisher> makeSyphonPublisher(std::string name,
                                               void* metalDevice);

}  // namespace sigil::io::frames
