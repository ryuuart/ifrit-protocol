#pragma once

/** @file
 * @ingroup media-core
 * DECODING BYTES ALREADY IN HAND, with no hub: `media::decode<T>` over
 * the same decoders a hub registers — `media::Image`, `media::Channels`,
 * `media::Video` — each found through the header that declares its
 * document.
 */

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

namespace sigil::media {

/** A document decoded with options of its own: one whose header declares
 *  `loadOptions(std::type_identity<Document>)`, the hook a hub's
 *  `load<T>(uri, options)` reads too. */
template <class Document>
concept ConfiguredDocument =
    requires { loadOptions(std::type_identity<Document>{}); };

/** The options a ConfiguredDocument is decoded with. */
template <ConfiguredDocument Document>
using DocumentOptions = decltype(loadOptions(std::type_identity<Document>{}));

/** @p bytes DECODED AS A @p Document, with @p options — `ImageOptions`
 *  for an image, `VideoOptions` for a video; null when the bytes are not
 *  one. @p nameHint only sharpens sniffing: nothing dispatches on it. */
template <ConfiguredDocument Document>
std::shared_ptr<const Document> decode(
    std::span<const std::byte> bytes, DocumentOptions<Document> options = {},
    const std::filesystem::path& nameHint = {}) {
  auto value = decodeDocument(std::type_identity<Document>{}, bytes, options,
                              nameHint);
  if (!value) return nullptr;
  return std::make_shared<const Document>(std::move(*value));
}

/** The same for a document that takes no options — `media::Channels`. */
template <class Document>
  requires(!ConfiguredDocument<Document>)
std::shared_ptr<const Document> decode(
    std::span<const std::byte> bytes,
    const std::filesystem::path& nameHint = {}) {
  auto value = decodeDocument(std::type_identity<Document>{}, bytes, nameHint);
  if (!value) return nullptr;
  return std::make_shared<const Document>(std::move(*value));
}

}  // namespace sigil::media
