/** @file
 * The resource directory found: the executable's "resources" sibling
 * checked for the ICU data file, and the three-step resolution order.
 */

#include "ResourceDir.h"

#include <sigilio/source/Places.h>

#include <filesystem>

namespace sigil::scry {

std::string executableAdjacentResourceDir() {
  const std::filesystem::path executable = io::executablePath();
  if (executable.empty()) return {};
  const std::filesystem::path dir = executable.parent_path() / "resources";
  std::error_code error;
  if (!std::filesystem::is_regular_file(dir / "icudt67l.dat", error)) return {};
  return dir.string();
}

std::string resolveResourceDir(const std::string& configured) {
  if (!configured.empty()) return configured;
  std::string staged = executableAdjacentResourceDir();
  if (!staged.empty()) return staged;
  return SIGILSCRY_DEFAULT_RESOURCE_DIR;
}

}  // namespace sigil::scry
