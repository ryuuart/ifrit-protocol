/** @file
 * HANDS THE HUB THE TRANSPORTS THIS PROGRAM LINKS. This file is compiled
 * into every target that links the transport feature rather than into
 * the feature's own archive: an archive member nothing names is left out
 * of the program, and a static initializer in it would never run. Its one
 * reference to the installer is what brings the transports in.
 */

#include "sigilio/advanced/Transport.h"
#include "sigilio/transport/Transport.h"

namespace {

const bool linked = [] {
  sigil::io::detail::setLinkedTransports(
      &sigil::io::detail::installLinkedTransports);
  return true;
}();

}  // namespace
