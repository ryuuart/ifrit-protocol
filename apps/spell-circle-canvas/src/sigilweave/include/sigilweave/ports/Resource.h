#pragma once

/** @file
 * @ingroup weave-ports
 *
 * A FONT FILE AS A RESOURCE: the face its bytes hold, read through the
 * platform's font manager, and `registerDecoders(hub)`, after which
 * `hub.load<Face>(uri)` answers a bundled font the way a hub answers an
 * image — cached per resource, kept as long as anything holds it, and read
 * again when the file changes. `registerDecoders` is a template over the
 * hub, so this library knows no resource library.
 */

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

#include "sigilweave/style/Face.h"

namespace sigil::weave::ports {

/** THE FACE THE BYTES OF A FONT FILE HOLD — TrueType, OpenType or a
 *  collection's first face — read through `systemFontManager()`, so it
 *  shapes and draws as a face found by family does. The bytes are copied:
 *  the face outlives them. No face when they are not a font. */
Face faceFromBytes(std::span<const std::byte> bytes);

/** Puts the `Face` decoder on @p hub, after which `hub.load<Face>(uri)`
 *  answers the face a font resource holds. A host calls this once,
 *  wherever it builds its hub.
 *  @trap A font file that changes on disk is read again as a NEW face:
 *  styles holding the old one keep it, since faces compare by identity,
 *  so a host that reloads asks the hub again and restyles with the
 *  answer. */
template <typename Hub>
void registerDecoders(Hub& hub) {
  registerDecoder<Face>(
      hub, [](const auto& bytes, std::string_view) -> std::optional<Face> {
        Face face = faceFromBytes(bytes.span());
        if (!face) return std::nullopt;
        return face;
      });
}

}  // namespace sigil::weave::ports
