/** @file
 * The probe a hub asks: the image route first, then a video's container.
 */

#include "sigilmedia/advanced/Resource.h"

namespace sigil::media {

std::optional<Metadata> probe(std::span<const std::byte> bytes,
                              const std::filesystem::path& nameHint) {
  if (auto image = probeDocument(std::type_identity<Image>{}, bytes, nameHint))
    return *image;
  if (auto video = probeDocument(std::type_identity<Video>{}, bytes, nameHint))
    return *video;
  return std::nullopt;
}

}  // namespace sigil::media
