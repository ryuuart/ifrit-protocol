/** @file
 * The Procreate `.brush` reader: the two images out of the archive.
 */

#include "Images.h"
#include "Zip.h"

#include <sigildraw/brush/format/Procreate.h>

#include <algorithm>
#include <string>
#include <utility>

namespace sigil::draw::brush::format {

namespace {

std::string lowered(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char letter) {
                   return (char)std::tolower(letter);
                 });
  return out;
}

/** Whether a path inside the archive names one of the two pictures. A
 *  brush writes them at the root or one directory down, and has done so
 *  under more than one capitalisation, so the name is matched rather
 *  than the exact path. */
bool namesPart(std::string_view path, std::string_view part) {
  const std::string name = lowered(path);
  return name.find(part) != std::string::npos;
}

bool isPicture(std::string_view path) {
  const std::string name = lowered(path);
  return name.ends_with(".png") || name.ends_with(".jpg") ||
         name.ends_with(".jpeg");
}

}  // namespace

std::optional<Tool> decodeProcreateBrush(std::span<const std::byte> bytes) {
  const std::vector<ZipEntry> entries = readZip(bytes);
  if (entries.empty()) return std::nullopt;

  sk_sp<SkImage> shape;
  sk_sp<SkImage> grain;
  for (const ZipEntry& entry : entries) {
    if (!isPicture(entry.name)) continue;
    if (!shape && namesPart(entry.name, "shape"))
      shape = decodeArtwork(entry.bytes);
    else if (!grain && namesPart(entry.name, "grain"))
      grain = decodeArtwork(entry.bytes);
  }
  if (!shape && !grain) return std::nullopt;

  Tool tool;
  tool.tip = shape ? Tip::Image : Tip::Nib;
  tool.opacity = 1.0f;
  tool.markerTip = false;
  tool.pressure = {1.0f, 1.0f, 1.0f};
  tool.pressure.variation.reset();
  if (shape) tool.shape = Shape{.image = std::move(shape)};
  if (grain) tool.grain = Grain{.image = std::move(grain)};
  return tool;
}

}  // namespace sigil::draw::brush::format
