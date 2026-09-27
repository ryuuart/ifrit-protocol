#pragma once

/** @file
 * @ingroup material-substance
 *
 * THE ARCHIVE A GRAPH IS COOKED FROM: a `.sbsar` decoded once through the
 * resource hub, every graph in it described once — inputs, outputs,
 * embedded presets — and shared, immutable, by every cook made from it.
 * Nothing here cooks; `CookScheduler` does.
 */

#include <sigilmaterial/substance/Substance.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace sigil::material::sbsar {

/** A DECODED `.sbsar`: its graphs, each described once. Copies share one
 *  decode. Empty — no graphs — when default-constructed or when the SDK
 *  is not available. */
class Archive {
 public:
  Archive() = default;
  /** @p bytes decoded as an archive; nothing when they are not one or
   *  the SDK is not available. */
  static std::optional<Archive> decode(std::span<const std::byte> bytes);

  /** How many graphs the archive holds. */
  size_t graphCount() const;
  /** The graph at @p index, described; its presets are the embedded
   *  ones. @p index must be below `graphCount()`. */
  const Description& graph(size_t index) const;
  /** The graph's url inside the archive (`pkg://Autumn_Leaves`). */
  const std::string& url(size_t index) const;
  /** The graph whose label or url is @p labelOrUrl; the first for an
   *  empty name; nothing when absent. */
  std::optional<size_t> find(std::string_view labelOrUrl) const;

  /** The decode itself, for the cook made from it. */
  struct Decoded;
  const std::shared_ptr<const Decoded>& decoded() const { return m_decoded; }

 private:
  std::shared_ptr<const Decoded> m_decoded;
};

/** The name a resource hub registers and asks for an archive under. */
inline std::string_view meaningName(std::type_identity<Archive>) {
  return "material.sbsar.Archive";
}

/** Registers on @p hub how an `Archive` is decoded from bytes, so
 *  `hub.load<Archive>(uri)` answers. `load()` below calls it when it has
 *  to. */
void registerDecoder(io::Hub& hub);

/** The archive at @p uri through @p hub's cache, registering the decoder
 *  first when the hub has none; null when it cannot be read. */
std::shared_ptr<const Archive> load(io::Hub& hub, std::string_view uri);

/** The linked engine's name and version, for diagnostics; empty without
 *  the SDK. */
std::string engineVersion();

}  // namespace sigil::material::sbsar
