/** @file
 * Where the binary stands, per platform, and where a process may scribble.
 */

#include "sigilio/source/Places.h"

#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace sigil::io {

std::filesystem::path executablePath() {
  std::filesystem::path executable;
#ifdef __APPLE__
  // The first call answers the size the second needs, and reports a
  // truncation by asking for more rather than by failing.
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string buffer(size, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
  buffer.resize(std::strlen(buffer.c_str()));
  executable = buffer;
#else
  std::error_code linkError;
  executable = std::filesystem::read_symlink("/proc/self/exe", linkError);
  if (linkError) return {};
#endif
  std::error_code error;
  std::filesystem::path resolved =
      std::filesystem::weakly_canonical(executable, error);
  if (error) return {};
  return resolved;
}

std::filesystem::path scratchDirectory(std::string_view label) {
  std::error_code error;
  const std::filesystem::path temp =
      std::filesystem::temp_directory_path(error);
  if (error) return {};
  return temp / (std::string(label) + "_" + std::to_string(::getpid()));
}

}  // namespace sigil::io
