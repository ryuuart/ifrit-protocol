/** @file
 * The resource directory found: the executable's own path (through the
 * dynamic linker on Apple, /proc on Linux), its "resources" sibling
 * checked for the ICU data file, and the three-step resolution order.
 */

#include "ResourceDir.h"

#include <cstring>
#include <filesystem>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace sigil::scry {

std::string executableAdjacentResourceDir() {
  std::filesystem::path executable;
#ifdef __APPLE__
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string buffer(size, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
  buffer.resize(std::strlen(buffer.c_str()));
  executable = buffer;
#else
  std::error_code ec;
  executable = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec) return {};
#endif
  std::error_code ec;
  executable = std::filesystem::weakly_canonical(executable, ec);
  if (ec) return {};
  std::filesystem::path dir = executable.parent_path() / "resources";
  if (!std::filesystem::is_regular_file(dir / "icudt67l.dat", ec)) return {};
  return dir.string();
}

std::string resolveResourceDir(const std::string& configured) {
  if (!configured.empty()) return configured;
  std::string staged = executableAdjacentResourceDir();
  if (!staged.empty()) return staged;
  return SIGILSCRY_DEFAULT_RESOURCE_DIR;
}

}  // namespace sigil::scry
