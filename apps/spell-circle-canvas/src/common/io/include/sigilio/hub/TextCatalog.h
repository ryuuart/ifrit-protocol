#pragma once

/** @file
 * @ingroup io-hub
 * One directory of text resources mounted at one URI prefix — the shape
 * a directory of AUTHORED shaders takes: a consumer keeps its `.sksl` or
 * `.slang` files wherever it likes, asks for each by name, and may warm
 * the whole directory before the first ask. A shader a library SHIPS is
 * not this: it is compiled into that library's archive, and reading one
 * costs no hub.
 */

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "sigilio/hub/Hub.h"

namespace sigil::io {

/** A hub of its own with one directory mounted at one prefix. `text()`
 *  is a file beneath the directory, through that hub's cache, and
 *  `preload()` fetches every one of them concurrently; the hub itself
 *  is reachable for anything else — a typed decode, a poll, a lease. */
class TextCatalog {
 public:
  /** @p prefix is a namespace, and every name asked for is BENEATH it,
   *  so a prefix that does not already end in a separator is given one
   *  — otherwise `"shader://glow"` and `"Glow.sksl"` would run together
   *  into one word. */
  TextCatalog(std::string prefix, std::filesystem::path directory)
      : m_prefix(std::move(prefix)) {
    if (!m_prefix.empty() && !m_prefix.ends_with('/')) m_prefix += '/';
    m_hub.mount(m_prefix, std::move(directory));
  }

  /** The text of `name` beneath the directory; nullopt when it is not
   *  there. */
  std::optional<std::string> text(std::string_view name) {
    return m_hub.text(m_prefix + std::string(name));
  }

  /** Fetches every file beneath the directory into the byte cache
   *  concurrently and returns how many are ready. */
  size_t preload() { return m_hub.preload(m_prefix); }

  /** The directory URI every name is read beneath. */
  std::string_view prefix() const { return m_prefix; }
  /** The hub underneath, for a caller that wants a resource this
   *  catalogue does not name. */
  Hub& hub() { return m_hub; }

 private:
  std::string m_prefix;
  Hub m_hub;
};

}  // namespace sigil::io
