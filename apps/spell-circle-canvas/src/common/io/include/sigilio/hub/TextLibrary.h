#pragma once

/** @file
 * A synchronized, cached collection of text resources rooted at one
 * directory. Its resource hub and synchronization stay in the implementation.
 */

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace sigil::io {

/**
 * Text files under one root, loaded and cached through an internal Hub.
 *
 * Calls may arrive concurrently. `poll()` checks previously loaded files for
 * edits; the next `text()` returns the reloaded contents. Libraries with the
 * same prefix and root share that cache, so one loading phase can preload for
 * the components that later read the sources.
 */
class TextLibrary {
 public:
  TextLibrary(std::string uriPrefix, std::filesystem::path root);
  ~TextLibrary();

  TextLibrary(TextLibrary&&) noexcept;
  TextLibrary& operator=(TextLibrary&&) noexcept;
  TextLibrary(const TextLibrary&) = delete;
  TextLibrary& operator=(const TextLibrary&) = delete;

  /** Text at @p uri, or no value when it is outside this library or missing. */
  std::optional<std::string> text(std::string_view uri);

  /** Fetches the distinct URIs concurrently into the byte cache. */
  size_t preload(std::span<const std::string_view> uris);
  /** Fetches every regular file below the root concurrently. */
  size_t preload();

  /** Reloads changed files already read by this library. */
  bool poll();

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::io
