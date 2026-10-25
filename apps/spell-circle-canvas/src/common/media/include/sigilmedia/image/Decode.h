#pragma once

/** @file
 * @ingroup media-image
 * THE IMAGE DECODERS: bytes routed between the backends by sniffing
 * content — Skia's codecs for the web formats and their animation, a KTX
 * with uncompressed texels read from its header, and the optional SVG and
 * OpenImageIO backends where they are built in. Reached as
 * `media::decode<media::Image>(bytes, {…})`, `media::decode<media::Channels>`
 * and, through a hub, `hub.load<…>(uri)`; the functions below are the
 * hooks those find by argument-dependent lookup.
 */

/** @defgroup media-image Images
 *  Encoded stills and animations decoded whole into an `Image`, their
 *  float planes into `Channels`, and pixels encoded back out as the bytes
 *  of a format.
 *  @{ */
/** @} */

#include <sigilcore/report/Error.h>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <type_traits>

#include "sigilmedia/core/Decode.h"
#include "sigilmedia/core/Image.h"
#include "sigilmedia/core/Metadata.h"
#include "sigilmedia/image/Channels.h"

namespace sigil::media {

/** @p bytes decoded as an image: the Skia codecs first unless a layer is
 *  named (layers are OpenImageIO's), then KTX, SVG and OpenImageIO; or
 *  the error that says why none of them read the bytes. */
std::expected<Image, core::Error> decodeDocument(std::type_identity<Image>,
                                    std::span<const std::byte> bytes,
                                    const ImageOptions& options,
                                    const std::filesystem::path& nameHint);

/** @p bytes decoded as every channel they carry, or the error that says
 *  why no decoder read them. */
std::expected<Channels, core::Error> decodeDocument(
    std::type_identity<Channels>,
                                       std::span<const std::byte> bytes,
                                       const std::filesystem::path& nameHint);

/** What @p bytes say about themselves as an image, without a pixel
 *  decode, along the same route; or the error that says why no route
 *  read them. */
std::expected<Metadata, core::Error> probeDocument(std::type_identity<Image>,
                                      std::span<const std::byte> bytes,
                                      const std::filesystem::path& nameHint);

}  // namespace sigil::media
