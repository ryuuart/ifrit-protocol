#pragma once

/** @file
 * @ingroup query
 *
 * Optional convenience layer over Paragraph — nothing in the core pipeline
 * depends on it, and applications with their own selection/annotation
 * systems can ignore it entirely. It exists for the HTML/JS-flavoured
 * workflow: find ranges by substring, word, or ICU regular expression,
 * style them, or register them in a MarkerSet that follows subsequent
 * edits the way DOM Range objects do.
 */

#include "Paragraph.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace textflow {

class FontContext;

// CharRange (a UTF-16 code-unit range, end exclusive) lives in Paragraph.h —
// it is shared with Paragraph's batch restyling API.

/** Returns every non-overlapping UTF-16 occurrence, ordered left-to-right. */
[[nodiscard]] std::vector<CharRange>
findAllOccurrences(const Paragraph &paragraph, std::u16string_view needle);
/** Converts a UTF-8 needle and returns every non-overlapping occurrence. */
[[nodiscard]] std::vector<CharRange>
findAllOccurrences(const Paragraph &paragraph, std::u8string_view utf8Needle);

/** Returns every non-overlapping occurrence inside the clamped `scope`,
 * as if that window were the whole string — a match never extends past
 * either edge.
 *
 * This is the cost control for large documents: scope the query to what
 * the layout actually placed (e.g. up to
 * ParagraphLayout::firstUnplacedWord's textBegin) and the search is bounded
 * by the geometry, not the text.
 */
[[nodiscard]] std::vector<CharRange>
findAllOccurrences(const Paragraph &paragraph, std::u16string_view needle,
                   CharRange scope);
/** Searches for a UTF-8 needle only inside the clamped UTF-16 `scope`. */
[[nodiscard]] std::vector<CharRange>
findAllOccurrences(const Paragraph &paragraph, std::u8string_view utf8Needle,
                   CharRange scope);

/** Returns every match of an ICU regular expression (full Unicode
 * semantics; the pattern is UTF-8), or std::nullopt when the pattern does
 * not compile.
 */
[[nodiscard("invalid regular expressions are reported as nullopt")]]
std::optional<std::vector<CharRange>>
findRegexMatches(const Paragraph &paragraph, std::u8string_view utf8Pattern);
/** Applies an ICU regular expression only inside the clamped `scope`. */
[[nodiscard("invalid regular expressions are reported as nullopt")]]
std::optional<std::vector<CharRange>>
findRegexMatches(const Paragraph &paragraph, std::u8string_view utf8Pattern,
                 CharRange scope);

/** Returns content-only ranges for the paragraph's analyzed words — the
 * line-break segments the layout itself uses, trailing whitespace excluded.
 * Shapes on demand (cache-hot).
 */
[[nodiscard]] std::vector<CharRange> wordRanges(Paragraph &paragraph,
                                                FontContext &fontContext);

/// Named range sets that follow edits. Ranges are adjusted by replaying the
/// paragraph's recorded edit ops:
///   - text inserted/removed before a range shifts it;
///   - a replacement overlapping a range is absorbed into it (the range
///     grows to cover the inserted text);
///   - ranges that collapse to empty are dropped.
///
/// An insertion exactly at a range's start joins the range; one exactly at
/// its (exclusive) end does not.
class MarkerSet {
public:
  MarkerSet() = default;
  /** Starts tracking at the paragraph's current revision. */
  explicit MarkerSet(const Paragraph &paragraph)
      : m_revision(paragraph.revision()) {}

  /** Stores ranges valid for the revision last synchronized or constructed. */
  void setRanges(std::string name, std::vector<CharRange> ranges);
  /** Returns a named range collection, or null when it does not exist. */
  [[nodiscard]] const std::vector<CharRange> *
  rangesFor(std::string_view name) const;
  /** Removes a named range collection when present. */
  void remove(std::string_view name);

  /** Replays edits since the last synchronization.
   *
   * Returns false and clears all ranges when the paragraph's bounded edit
   * history no longer reaches the tracked revision.
   */
  [[nodiscard("false means tracked ranges were cleared and must be rebuilt")]]
  bool synchronize(const Paragraph &paragraph);

  /** Synchronizes, then applies a complete style to a named range set. */
  void applyStyle(Paragraph &paragraph, std::string_view name,
                  const TextStyle &style);
  /** Synchronizes, then applies paint to a named range set. */
  void applyPaint(Paragraph &paragraph, std::string_view name,
                  const PaintStyle &paint);

private:
  uint64_t m_revision = 0;
  /// Linear scan by design: marker sets hold a handful of named collections
  /// (selection, search hits, spell errors, …), where a flat vector beats a
  /// hash map and keeps third-party containers out of the public headers.
  std::vector<std::pair<std::string, std::vector<CharRange>>> m_markers;
};

} // namespace textflow
