/** @file
 * The one rule for where a sketch stands on disk, applied to a key and
 * to an entry file.
 */

#include "sigilsketch/core/Sources.h"

#include <algorithm>
#include <string>
#include <system_error>
#include <utility>

namespace sigil::sketch {

std::filesystem::path sourceOf(const std::filesystem::path& dir,
                               std::string_view key) {
  const std::filesystem::path name(std::string(key) + ".cpp");
  const std::filesystem::path entry = dir / std::filesystem::path(key) / name;
  std::error_code ec;
  if (std::filesystem::is_regular_file(entry, ec)) return entry;
  return dir / name;
}

bool directorySketch(const std::filesystem::path& entry) {
  return entry.extension() == ".cpp" && entry.has_parent_path() &&
         entry.parent_path().filename() == entry.stem();
}

std::vector<std::filesystem::path> sourcesUnder(
    const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> sources;
  std::error_code ec;
  for (auto it = std::filesystem::directory_iterator(dir, ec);
       !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
    if (it->path().extension() == ".cpp") sources.push_back(it->path());
  }
  std::sort(sources.begin(), sources.end());
  return sources;
}

std::vector<std::filesystem::path> unitsOf(const std::filesystem::path& entry) {
  std::vector<std::filesystem::path> units{entry};
  if (!directorySketch(entry)) return units;
  for (std::filesystem::path& source : sourcesUnder(entry.parent_path()))
    if (source != entry) units.push_back(std::move(source));
  return units;
}

}  // namespace sigil::sketch
