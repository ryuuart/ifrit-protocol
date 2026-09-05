#pragma once

/** @file
 * One directory of text resources mounted at one URI prefix — the shape
 * a directory of AUTHORED shaders takes: a consumer keeps its `.sksl` or
 * `.slang` files wherever it likes, asks for each by name, and may warm
 * the whole directory before the first ask. A stock value over the hub,
 * so a catalogue is a declaration rather than a hub, a mount and a
 * lookup written out again. A shader a library SHIPS is not this: it is
 * compiled into that library's archive, and reading one costs no hub.
 */

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "sigilio/hub/Hub.h"

namespace sigil::io {

/** A hub of its own with one directory mounted at one prefix.
 *
 *  `text(name)` is the file `name` beneath the directory, through the
 *  hub's cache, so a file edited on disk reaches the next ask after
 *  `poll()`. `preload()` fetches every file beneath the directory
 *  concurrently. The hub is reachable for anything else — a typed
 *  decode, a poll, a lease. */
class TextCatalog {
 public:
  TextCatalog(std::string prefix, std::filesystem::path directory)
      : m_prefix(std::move(prefix)) {
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

  std::string_view prefix() const { return m_prefix; }
  Hub& hub() { return m_hub; }

 private:
  std::string m_prefix;
  Hub m_hub;
};

}  // namespace sigil::io
