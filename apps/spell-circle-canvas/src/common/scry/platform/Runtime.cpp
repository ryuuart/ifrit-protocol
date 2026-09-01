/** @file
 * The runtime probe: the resource directory resolved the way the engine
 * resolves it, and the two files it must hold.
 */

#include "sigilscry/platform/Runtime.h"

#include <filesystem>

#include "ResourceDir.h"

namespace sigil::scry::runtime {

bool available(std::string* why) {
  const std::string dir = resolveResourceDir({});
  std::string missing;
  std::error_code ec;
  // The ICU data the layout engine reads and the certificate bundle its
  // network stack starts with: both are looked for, so a half-installed
  // folder reports which half.
  for (const char* file : {"icudt67l.dat", "cacert.pem"}) {
    if (std::filesystem::is_regular_file(std::filesystem::path(dir) / file, ec))
      continue;
    if (!missing.empty()) missing += ", ";
    missing += file;
  }
  if (missing.empty()) return true;
  if (why)
    *why = "the web engine's runtime data is missing (" + missing + " under " +
           (dir.empty() ? std::string("no resource directory") : dir) + ")";
  return false;
}

}  // namespace sigil::scry::runtime
