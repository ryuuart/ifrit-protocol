#pragma once

/** @file
 * @ingroup layout
 *
 * Optional cache for high-frequency labels, captions, and other short text.
 * It complements layoutSingleLine() while keeping application-specific path
 * geometry out of the core TextFlow library.
 */

#include "Paragraph.h"

#include <concepts>
#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>

namespace textflow {

namespace detail {

/** Text view encodings accepted by the shared single-line cache machinery. */
template <typename View>
concept CacheableTextView =
    std::same_as<std::remove_cvref_t<View>, std::u8string_view> ||
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
  /** Creates a cache that empties itself rather than exceed
   *  `maximumEntries`, invalidating previously returned references. */
  explicit SingleLineParagraphCache(size_t maximumEntries = 1024);
  ~SingleLineParagraphCache();

  // Move-only: the pimpl owns node-map storage whose entry addresses are
  // part of the API contract (returned references stay valid), so copying
  // a cache would silently duplicate paragraphs without their guarantees.
  /** Transfers the cache; references into it stay valid. */
  SingleLineParagraphCache(SingleLineParagraphCache &&) noexcept;
  /** Replaces this cache with the source's entries, dropping its own. */
  SingleLineParagraphCache &operator=(SingleLineParagraphCache &&) noexcept;
  SingleLineParagraphCache(const SingleLineParagraphCache &) = delete;
  SingleLineParagraphCache &operator=(const SingleLineParagraphCache &) = delete;

  /** Returns the cached paragraph for UTF-8 text, creating it when absent. */
  [[nodiscard]] Paragraph &paragraphFor(std::u8string_view utf8,
                                        const sk_sp<SkTypeface> &typeface,
                                        float fontSize);

  /** Returns the same cache namespace through a UTF-16 input view. */
  [[nodiscard]] Paragraph &paragraphFor(std::u16string_view utf16,
                                        const sk_sp<SkTypeface> &typeface,
                                        float fontSize);

  /** Removes every cached paragraph and invalidates returned references. */
  void clear();

private:
  template <detail::CacheableTextView View>
  Paragraph &paragraphForImpl(View text, const sk_sp<SkTypeface> &typeface,
                              float fontSize);

  struct Impl; ///< node-based map storage; keeps hash-map deps in the .cpp
  std::unique_ptr<Impl> m_impl;
};

} // namespace textflow
