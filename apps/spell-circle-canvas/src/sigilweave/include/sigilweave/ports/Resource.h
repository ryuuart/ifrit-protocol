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

#include <sigilcore/report/Error.h>

#include <concepts>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "sigilweave/style/Face.h"

namespace sigil::weave::ports {

/** THE FACE THE BYTES OF A FONT FILE HOLD — TrueType, OpenType or a
 *  collection's first face — read through `systemFontManager()`, so it
 *  shapes and draws as a face found by family does. The bytes are copied:
 *  the face outlives them. No face when they are not a font. */
Face faceFromBytes(std::span<const std::byte> bytes);

/** What a hub lists under a font resource whose bytes hold no face. */
inline core::Error unreadFace() {
  return {"the bytes are no font face this build reads", {}, {}};
}

/** Puts the `Face` decoder on @p hub, after which `hub.load<Face>(uri)`
 *  answers the face a font resource holds, and a resource that holds none
 *  is listed among the hub's problems. A host calls this once,
 *  wherever it builds its hub.
 *  @trap A font file that changes on disk is read again as a NEW face:
 *  styles holding the old one keep it, since faces compare by identity,
 *  so a host that reloads asks the hub again and restyles with the
 *  answer. */
template <typename Hub>
void registerDecoders(Hub& hub) {
  registerDecoder<Face>(
      hub,
      [](const auto& bytes,
         std::string_view) -> std::expected<Face, core::Error> {
        Face face = faceFromBytes(bytes.span());
        if (!face) return std::unexpected(unreadFace());
        return face;
      });
}

/** The same decoder, which also hands every face it reads to
 *  @p arrived as `arrived(face, path)`, the path being the file the face
 *  was read from where the resource has one — what a host calls to make
 *  each font its hub loads known to a font context by family, through
 *  `FontContext::adopt`. A file read again after an edit arrives again as
 *  its new face.
 *  @trap @p arrived runs on whichever thread decoded the face, which is
 *  the thread that asked the hub for it or polled it. */
template <typename Hub, typename Arrived>
  requires std::invocable<Arrived&, const Face&, std::string_view>
void registerDecoders(Hub& hub, Arrived arrived) {
  registerDecoder<Face>(
      hub,
      [arrived = std::move(arrived)](
          const auto& bytes,
          std::string_view path) mutable -> std::expected<Face, core::Error> {
        Face face = faceFromBytes(bytes.span());
        if (!face) return std::unexpected(unreadFace());
        arrived(face, path);
        return face;
      });
}

}  // namespace sigil::weave::ports
