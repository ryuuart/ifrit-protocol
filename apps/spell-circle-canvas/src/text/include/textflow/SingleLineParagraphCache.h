#pragma once

/** @file
 * Optional cache for high-frequency labels, captions, and other short text.
 * It complements layoutSingleLine() while keeping application-specific path
 * geometry out of the core TextFlow library.
 */

#include "Paragraph.h"

#include <absl/container/node_hash_map.h>

#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

namespace textflow {

namespace detail {

/** Text view encodings accepted by the shared single-line cache machinery. */
template <typename View>
concept CacheableTextView =
    std::same_as<std::remove_cvref_t<View>, std::string_view> ||
    std::same_as<std::remove_cvref_t<View>, std::u16string_view>;

} // namespace detail

/**
 * Caches single-style paragraphs by text, typeface, and quantized font size.
 *
 * Returned references remain valid while other entries are inserted because
 * the cache uses a node-based map. They become invalid after clear() or when
 * the bounded cache clears itself before inserting a new generation.
 */
class SingleLineParagraphCache {
public:
  explicit SingleLineParagraphCache(size_t maximumEntries = 1024)
      : m_maximumEntries(maximumEntries) {}

  /** Returns the cached paragraph for UTF-8 text, creating it when absent. */
  [[nodiscard]] Paragraph &paragraphFor(std::string_view utf8,
                                        const sk_sp<SkTypeface> &typeface,
                                        float fontSize);

  /** Returns the same cache namespace through a UTF-16 input view. */
  [[nodiscard]] Paragraph &paragraphFor(std::u16string_view utf16,
                                        const sk_sp<SkTypeface> &typeface,
                                        float fontSize);

  /** Removes every cached paragraph and invalidates returned references. */
  void clear() { m_paragraphs.clear(); }

private:
  template <detail::CacheableTextView View>
  Paragraph &paragraphForImpl(View text, const sk_sp<SkTypeface> &typeface,
                              float fontSize);

  absl::node_hash_map<std::string, Paragraph> m_paragraphs;
  size_t m_maximumEntries;
};

} // namespace textflow
