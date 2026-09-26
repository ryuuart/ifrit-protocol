/** @file
 * The filename question: the format an extension names, and the extension
 * each format is written with.
 */

#include "sigilmedia/advanced/Formats.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace sigil::media {

std::optional<Format> formatForPath(const std::filesystem::path& path) {
  std::string extension = path.extension().string();
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (extension == ".png") return Format::Png;
  if (extension == ".jpg" || extension == ".jpeg") return Format::Jpeg;
  if (extension == ".webp") return Format::Webp;
  if (extension == ".exr") return Format::Exr;
  if (extension == ".mp4" || extension == ".m4v") return Format::Mp4;
  return std::nullopt;
}

const char* extensionFor(Format format) {
  switch (format) {
    case Format::Png:
      return ".png";
    case Format::Jpeg:
      return ".jpg";
    case Format::Webp:
      return ".webp";
    case Format::Exr:
      return ".exr";
    case Format::Mp4:
      return ".mp4";
  }
  return ".png";
}

}  // namespace sigil::media
