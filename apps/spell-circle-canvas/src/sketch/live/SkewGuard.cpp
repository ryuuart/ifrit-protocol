/** @file
 * Which framework header postdates the image that is running.
 */

#include "SkewGuard.h"

#include <dlfcn.h>

#include <fstream>
#include <string_view>
#include <system_error>

#include "sigilsketch/live/Host.h"

namespace sigil::sketch {

namespace {

/** Every public header of a framework library is an ABI boundary, and
 *  the line is drawn there rather than at a list of file names. */
bool abiBoundaryHeader(const std::filesystem::path& p) {
  return p.generic_string().find("/include/sigil") != std::string::npos;
}

}  // namespace

std::string newerHeaderThanHost(const std::filesystem::path& flagsFile,
                                std::filesystem::file_time_type hostTime) {
  std::ifstream flags(flagsFile);
  std::string token;
  std::error_code ec;
  while (flags >> token) {
    if (token.size() > 2 && token.compare(0, 2, "-I") == 0)
      token.erase(0, 2);
    else
      continue;
    if (!token.empty() && token.front() == '"')
      token = token.substr(1, token.size() - 2);
    // Repository headers only — dependency trees are immutable in
    // practice and huge to scan.
    if (token.find("/src/") == std::string::npos) continue;
    for (auto it = std::filesystem::recursive_directory_iterator(token, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); ++it) {
      const std::filesystem::path& p = it->path();
      if (p.extension() != ".h" && p.extension() != ".hpp") continue;
      if (!abiBoundaryHeader(p)) continue;
      auto t = std::filesystem::last_write_time(p, ec);
      if (!ec && t > hostTime) return p.string();
    }
  }
  return {};
}

std::filesystem::file_time_type hostBinaryTime() {
  // TAKEN ONCE, at the first ask, which is before any rebuild of this
  // executable can land: the file behind a running image is replaced in
  // place, so a stat taken after that replacement describes the NEW
  // binary and postdates every header — and the guard that exists for
  // exactly that moment would wave it through.
  static const std::filesystem::file_time_type stamp = [] {
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&hostBinaryTime), &info) &&
        info.dli_fname) {
      std::error_code ec;
      auto t = std::filesystem::last_write_time(info.dli_fname, ec);
      if (!ec) return t;
    }
    return std::filesystem::file_time_type{};
  }();
  return stamp;
}

}  // namespace sigil::sketch
