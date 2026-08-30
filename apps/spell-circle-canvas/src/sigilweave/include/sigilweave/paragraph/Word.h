#pragma once

/** @file
 * @ingroup document
 *
 * The atomic layout unit: a Word is the text between two line-break
 * opportunities, measured as content plus trailing glue, carrying its bidi
 * level and break flags, and — once shaped — its segments, each a
 * cache-shared ShapedWord placed at an offset in one of the forms a
 * vertical column can take. WordSegmentList is the segment storage: one
 * segment lives inside the object and only a split word allocates, behind
 * bytes whose container type the library alone sees.
 */

#include <cstddef>
#include <cstdint>
#include <span>

#include "sigilweave/fonts/Shaper.h"

namespace sigil::weave {

/// How a segment is placed relative to the flow direction. kFlow is every
/// segment of a horizontal paragraph; the others only appear in vertical
/// paragraphs (resolved from ShapingStyle::verticalForm / UTR#50).
enum class SegmentForm : uint8_t {
  kFlow,         ///< pen-aligned with the interval (horizontal fast path)
  kUpright,      ///< vertical-shaped word stacked down the column
  kRotated,      ///< horizontal-shaped word rotated to the interval direction
  kTateChuYoko,  ///< horizontal-shaped word set upright across the column
};

/// One shaped run inside a Word (usually the only one; more when a style
/// boundary, script change, or font fallback splits the word).
struct WordSegment {
  ShapedWordRef shaped;     ///< cache-shared glyph run this segment draws
  uint32_t styleIndex = 0;  ///< index into Paragraph::spans()
  float advanceOffset = 0;  ///< pen offset from the word origin (for
                            ///< kTateChuYoko this lands on the run's baseline)
  SegmentForm form = SegmentForm::kFlow;  ///< vertical-text placement; always
                                          ///< kFlow in horizontal paragraphs
  /// First UTF-16 unit of the text this segment shaped, so a glyph's cluster
  /// (ShapedWord::clusters, an offset inside that text) maps back to a
  /// position in Paragraph::text(). Length-changing case mapping
  /// (ShapingStyle::textTransform) makes the mapping approximate, exactly as
  /// it does for the clusters themselves.
  uint32_t textBegin = 0;
};

/// An inline object slot woven into the flow (SkParagraph's placeholder
/// idea): the breakers treat it as an unbreakable word of the given size and
/// the layout reports the rect where it landed, so callers can draw pills,
/// icons, or images *inside* the text flow. Anchored in the text as an
/// object-replacement character (U+FFFC), matched to its record by
/// occurrence order.
struct Placeholder {
  float width = 0;   ///< advance the breakers reserve, px
  float height = 0;  ///< box height the line must accommodate, px
  /// The box's bottom edge sits this far below the baseline (0 = bottom on
  /// the baseline, like an inline image; ~descent centres a pill on
  /// x-height).
  float baselineDrop = 0;
};

/// The shaped runs of one Word, in text order. Nearly every word is a single
/// uniform run, so one segment lives inside the object and only a word that
/// a style boundary, script change or font fallback splits allocates. The
/// container behind the bytes is an implementation detail of the library:
/// readers see a span, and only Paragraph fills the list.
class WordSegmentList {
 public:
  WordSegmentList() noexcept;
  WordSegmentList(const WordSegmentList& other);
  WordSegmentList(WordSegmentList&& other) noexcept;
  WordSegmentList& operator=(const WordSegmentList& other);
  WordSegmentList& operator=(WordSegmentList&& other) noexcept;
  ~WordSegmentList();

  /** Returns the segments in text order. The view is invalidated by the
   * next append or clear.
   */
  [[nodiscard]] std::span<const WordSegment> view() const noexcept;

 private:
  friend class Paragraph;
  void append(WordSegment segment);
  void clear() noexcept;

  // Room for the inline container: its size and alignment are checked
  // where it is instantiated, so a container that outgrows this space
  // fails to compile rather than overrunning it.
  alignas(8) std::byte m_storage[48];
};

/// The atomic layout unit: the text between two line-break opportunities.
/// Content and trailing whitespace are measured separately so justification
/// can treat the whitespace as stretchable glue.
struct Word {
  uint32_t textBegin = 0;      ///< content range, whitespace excluded
  uint32_t textEnd = 0;        ///< exclusive end of the content range
  uint32_t whitespaceEnd = 0;  ///< == textEnd when there is no trailing space

  /** Returns the shaped runs in text order (usually exactly one; more when
   * a style boundary, script change, or font fallback splits the word).
   * Empty until the word is shaped (Paragraph::ensureShapedTo) and for
   * placeholder words, which carry no glyphs.
   */
  [[nodiscard]] std::span<const WordSegment> segments() const noexcept {
    return m_segments.view();
  }
  float width = 0;       ///< content advance
  float spaceWidth = 0;  ///< trailing-whitespace advance (justification glue)

  uint8_t bidiLevel = 0;  ///< UBA embedding level; odd means right-to-left
  bool mandatoryBreakAfter = false;  ///< '\n' and friends
  /// Trailing whitespace contains a tab (U+0009). With
  /// ParagraphLayoutOptions::tabStops configured, both breakers replace
  /// this word's glue with an advance to the next stop (greedy fits
  /// against tab-resolved widths as it goes; Knuth-Plass scores every
  /// candidate line at its tab-resolved width). Without tab stops, tabs
  /// measure as spaces.
  bool tabAfter = false;
  /// Break opportunity with zero glue (CJK): justification may expand here
  /// even though there is no space.
  bool ideographic = false;
  /// Content ended with a soft hyphen (U+00AD, stripped from shaping): a
  /// discretionary break. `hyphenGlyph` is the cached shaped "-" to render
  /// when a breaker actually breaks here. A hyphen the analysis refused to
  /// break at (see Paragraph::setSoftHyphenBreaks) is interior to its word
  /// and sets nothing.
  bool hyphenBreak = false;
  ShapedWordRef hyphenGlyph;  ///< only set alongside `hyphenBreak`

  /// \>= 0: this word is Paragraph::placeholders()[placeholderIndex] — no
  /// glyphs, `width` comes from the placeholder record.
  int placeholderIndex = -1;

 private:
  friend class Paragraph;
  WordSegmentList m_segments;
};

}  // namespace sigil::weave
