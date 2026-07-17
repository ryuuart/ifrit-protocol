#pragma once

/** @file
 * The document model: UTF-16 text carrying normalized style spans, optional
 * inline placeholders, and a horizontal or vertical-RL writing mode. Build
 * one with ParagraphBuilder (fluent addText / pushStyle) or append directly,
 * then edit it in place — replaceText, setPaint, setShaping,
 * appendPlaceholder — with the shape cache absorbing every unchanged word.
 * Hand the finished Paragraph, plus a FontContext and a Flow geometry, to
 * layoutParagraph() (ParagraphLayout.h). Styling types live in Style.h; the
 * range-query and marker conveniences live in the optional Query.h.
 */

#include "InlineVector.h"
#include "Shaper.h"
#include "Style.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace textflow {

class FontContext;

/// Style applied to a contiguous UTF-16 range. Spans are kept normalized:
/// sorted, non-overlapping, covering the whole text.
struct StyleSpan {
  uint32_t start = 0;
  uint32_t end = 0;
  TextStyle style;
};

/// UTF-16 code-unit range into a Paragraph's text, end exclusive. The common
/// currency between the query layer (Query.h), scoped searches, and batch
/// restyling.
struct CharRange {
  uint32_t start = 0;
  uint32_t end = 0;
  /** Returns whether this half-open range contains no UTF-16 code units. */
  [[nodiscard]] bool empty() const noexcept { return end <= start; }
  bool operator==(const CharRange &) const = default;
};

/// How a segment is placed relative to the flow direction. kFlow is every
/// segment of a horizontal paragraph; the others only appear in vertical
/// paragraphs (resolved from ShapingStyle::verticalForm / UTR#50).
enum class SegmentForm : uint8_t {
  kFlow,        ///< pen-aligned with the interval (horizontal fast path)
  kUpright,     ///< vertical-shaped word stacked down the column
  kRotated,     ///< horizontal-shaped word rotated to the interval direction
  kTateChuYoko, ///< horizontal-shaped word set upright across the column
};

/// One shaped run inside a Word (usually the only one; more when a style
/// boundary, script change, or font fallback splits the word).
struct WordSegment {
  ShapedWordRef shaped;
  uint32_t styleIndex = 0; ///< index into Paragraph::spans()
  float advanceOffset = 0; ///< pen offset from the word origin (for
                           ///< kTateChuYoko this lands on the run's baseline)
  SegmentForm form = SegmentForm::kFlow;
};

/// An inline object slot woven into the flow (SkParagraph's placeholder
/// idea): the breakers treat it as an unbreakable word of the given size and
/// the layout reports the rect where it landed, so callers can draw pills,
/// icons, or images *inside* the text flow. Anchored in the text as an
/// object-replacement character (U+FFFC), matched to its record by
/// occurrence order.
struct Placeholder {
  float width = 0;
  float height = 0;
  /// The box's bottom edge sits this far below the baseline (0 = bottom on
  /// the baseline, like an inline image; ~descent centres a pill on
  /// x-height).
  float baselineDrop = 0;
};

/// The atomic layout unit: the text between two line-break opportunities.
/// Content and trailing whitespace are measured separately so justification
/// can treat the whitespace as stretchable glue.
struct Word {
  uint32_t textBegin = 0; ///< content range, whitespace excluded
  uint32_t textEnd = 0;
  uint32_t whitespaceEnd = 0; ///< == textEnd when there is no trailing space

  /// Inline storage: nearly every word is a single uniform run, so the
  /// common case costs no allocation when the word list is rebuilt.
  InlineVector<WordSegment, 1> segments;
  float width = 0;      ///< content advance
  float spaceWidth = 0; ///< trailing-whitespace advance (justification glue)

  uint8_t bidiLevel = 0;
  bool mandatoryBreakAfter = false; ///< '\n' and friends
  /// Trailing whitespace contains a tab (U+0009). With
  /// ParagraphLayoutOptions::tabStops configured, the greedy breaker
  /// replaces this word's glue with an advance to the next stop; without
  /// tab stops (and always under Knuth-Plass) tabs measure as spaces.
  bool tabAfter = false;
  /// Break opportunity with zero glue (CJK): justification may expand here
  /// even though there is no space.
  bool ideographic = false;
  /// Content ended with a soft hyphen (U+00AD, stripped from shaping): a
  /// discretionary break. `hyphenGlyph` is the cached shaped "-" to render
  /// when a breaker actually breaks here.
  bool hyphenBreak = false;
  ShapedWordRef hyphenGlyph;

  /// \>= 0: this word is Paragraph::placeholders()[placeholderIndex] — no
  /// glyphs, `width` comes from the placeholder record.
  int placeholderIndex = -1;
};

/// Flow direction the paragraph is shaped for. Vertical-RL is the CJK book
/// layout: characters run top to bottom, columns right to left. The layout
/// geometry must match (columns with dir=(0,1), e.g. VerticalBlockFlow).
enum class WritingMode : uint8_t { kHorizontal, kVerticalRL };

/**
 * Owns styled UTF-16 text and its incremental analysis/shaping pipeline.
 *
 * Mutations invalidate only the necessary analysis. Re-analysis resolves
 * unchanged words through FontContext's content-addressed shape cache, so an
 * edit normally sends only the changed word back through HarfBuzz.
 */
class Paragraph {
public:
  Paragraph() = default;

  // ── Building ──────────────────────────────────────────────────────────
  /** Removes all text, styles, placeholders, and cached analysis. */
  void clear();
  /** Appends UTF-8 text using `style`. */
  void appendText(std::u8string_view utf8, const TextStyle &style);
  /** Appends UTF-16 text using `style`. */
  void appendText(std::u16string_view utf16, const TextStyle &style);

  /** Sets the writing mode. Vertical mode re-itemizes and re-shapes (once —
   * both orientations are separate shape-cache entries, so toggling back
   * and forth is warm).
   */
  void setWritingMode(WritingMode mode);
  /** Returns the direction in which this paragraph is shaped. */
  [[nodiscard]] WritingMode writingMode() const noexcept {
    return m_writingMode;
  }

  // ── Inline placeholders (pills, icons, images in the flow) ────────────
  /** Appends a U+FFFC-backed inline slot using `style` for flow context.
   *
   * Slots map to records by occurrence order, so direct text edits should
   * not add or remove object-replacement characters.
   */
  void appendPlaceholder(const Placeholder &placeholder,
                         const TextStyle &style);
  /** Returns inline object records in their text occurrence order. */
  const std::vector<Placeholder> &placeholders() const {
    return m_placeholders;
  }
  /** Resizes a slot and invalidates layout while leaving real words cache-hot.
   */
  void setPlaceholder(size_t index, const Placeholder &placeholder);

  // ── Editing (UTF-16 ranges) ───────────────────────────────────────────
  /** Replaces UTF-16 range `[start, end)` with UTF-8 and adjusts spans. */
  void replaceText(uint32_t start, uint32_t end, std::u8string_view utf8);
  /** Applies shaping and paint configuration to a UTF-16 range (splits spans
   * as needed). Re-shapes only words whose shaping inputs actually changed
   * — the rest hit the cache.
   */
  void setStyle(uint32_t start, uint32_t end, const TextStyle &style);
  /** Applies draw-time paint to one UTF-16 range without re-analyzing text.
   *
   * Same span surgery as setStyle, but shaping keys are untouched and the
   * text didn't move, so the next ensure* skips ICU re-analysis entirely —
   * it only re-derives the already-shaped words' segments against the new
   * span list (pure shape-cache hits unless a boundary lands mid-word).
   * Cost is bounded by the shaped prefix, not the text.
   */
  void setPaint(uint32_t start, uint32_t end, const PaintStyle &paint);
  /** Applies one paint to sanitized ranges in a single span-list rebuild
   * (batch form): restyling N marker ranges costs one pass, not N quadratic
   * rebuilds. Ranges may arrive unsorted/overlapping; they are sanitized
   * internally.
   */
  void setPaint(std::span<const CharRange> ranges, const PaintStyle &paint);

  /** Returns the paragraph's UTF-16 storage. */
  const std::u16string &text() const { return m_text; }
  /** Returns normalized style spans covering `text()`. */
  const std::vector<StyleSpan> &spans() const { return m_spans; }

  // ── Edit history (external range tracking) ────────────────────────────
  /// Every text mutation is recorded under a monotonically increasing
  /// revision, so external structures (e.g. Query.h's MarkerSet) can keep
  /// UTF-16 ranges in sync without wrapping every edit call. History is
  /// bounded; a consumer that falls too far behind must rebuild its ranges.
  struct TextEdit {
    uint32_t start = 0;
    uint32_t removed = 0;  ///< UTF-16 units deleted at `start`
    uint32_t inserted = 0; ///< UTF-16 units inserted at `start`
  };
  /** Returns the current monotonically increasing text revision. */
  uint64_t revision() const { return m_revision; }
  /** Appends the edits after `sinceRevision`, oldest first, to `edits`.
   * Returns false when history no longer reaches back that far and the
   * expired edit history requires the caller to rebuild tracked ranges.
   */
  [[nodiscard("expired edit history requires rebuilding tracked ranges")]]
  bool editsSince(uint64_t sinceRevision, std::vector<TextEdit> &edits) const;

  // ── Analysis ──────────────────────────────────────────────────────────
  /** Ensures analysis and glyph data are available for the whole paragraph.
   * Runs segmentation + shaping if anything changed since the last call.
   */
  void ensureShaped(FontContext &fontContext);
  /** Ensures break, bidi, and script analysis without shaping glyphs.
   *
   * Segmentation only (ICU boundaries, bidi, scripts — no HarfBuzz work):
   * words() gets its break/direction structure but no glyphs or widths yet.
   * ParagraphLayout drives shaping lazily from here, so a paragraph that
   * overflows its geometry only ever shapes the words that can actually
   * land.
   */
  void ensureAnalyzed(FontContext &fontContext);
  /** Lazily shapes words in `[0, wordCount)`, ascending and idempotent — the
   * breakers call this just ahead of their frontier.
   */
  void ensureShapedTo(FontContext &fontContext, uint32_t wordCount);
  /** Returns the number of words whose glyph data is currently available. */
  uint32_t shapedWordCount() const { return m_shapedWordCount; }
  /** Returns whether analysis or paint reconciliation is pending. */
  bool needsShaping() const { return m_dirty || m_paintDirty; }
  /** Returns the analyzed line-break units in logical text order. */
  const std::vector<Word> &words() const { return m_words; }

  /// Line-height inputs from the first span's font (the "strut"): returns
  /// {ascent (positive), height} for a default single-spaced line.
  struct Strut {
    float ascent = 0;
    float height = 0;
  };
  /** Returns positive ascent and default line height from the first span. */
  [[nodiscard]] Strut strut(FontContext &fontContext) const;

  /** Returns cache-hot unwrapped width without final trailing whitespace.
   *
   * The unwrapped single-line width is content plus inter-word glue, the
   * final word's trailing whitespace excluded. Shapes on demand.
   */
  [[nodiscard]] float naturalWidth(FontContext &fontContext);

private:
  void markDirty() { m_dirty = true; }
  // Paint edits only move span boundaries: analysis (words, scripts, bidi)
  // stands and the shaped prefix just needs its segments re-derived.
  void markPaintDirty() {
    if (!m_dirty)
      m_paintDirty = true;
  }
  void recordEdit(uint32_t start, uint32_t removedLength,
                  uint32_t insertedLength);
  void normalizeSpans();
  void analyze(FontContext &fontContext);
  void reshapeShapedPrefix(FontContext &fontContext);
  void shapeWordContent(FontContext &fontContext, Word &word);

  std::u16string m_text;
  std::vector<StyleSpan> m_spans;
  std::vector<Word> m_words;
  std::vector<Placeholder> m_placeholders;
  WritingMode m_writingMode = WritingMode::kHorizontal;
  bool m_dirty = true;
  bool m_paintDirty = false;

  // Itemization results analyze() leaves behind for lazy shaping
  // (ensureShapedTo). `script` is a UScriptCode, stored as int32_t to keep
  // ICU out of this header.
  struct ScriptRunEnd {
    uint32_t end = 0;
    int32_t script = 0;
  };
  std::vector<ScriptRunEnd> m_scriptRunEnds;
  // Per-unit bidirectional levels; empty when every unit uses one level.
  std::vector<uint8_t> m_bidirectionalLevels;
  uint8_t m_uniformBidirectionalLevel = 0;
  // Words [0, m_shapedWordCount) currently carry glyph data.
  uint32_t m_shapedWordCount = 0;
  uint32_t m_shapingSpanCursor = 0;
  uint32_t m_shapingScriptCursor = 0;
  uint32_t m_cachedWhitespaceStyleIndex = ~0u;
  std::u16string m_cachedWhitespaceText;
  float m_cachedWhitespaceWidth = 0;

  uint64_t m_revision = 0;
  std::vector<TextEdit> m_editHistory;
  uint64_t m_editHistoryBaseRevision = 0;
};

/// SkParagraph-style builder for the push/pop idiom; thin sugar over
/// Paragraph::appendText.
class ParagraphBuilder {
public:
  /** Starts a paragraph with `baseStyle` at the bottom of the style stack. */
  explicit ParagraphBuilder(const TextStyle &baseStyle) {
    m_styleStack.push_back(baseStyle);
  }

  /** Pushes a style used by subsequent text and placeholder additions. */
  ParagraphBuilder &pushStyle(const TextStyle &style) {
    m_styleStack.push_back(style);
    return *this;
  }
  /** Pops the active style while preserving the required base style. */
  ParagraphBuilder &popStyle() {
    if (m_styleStack.size() > 1)
      m_styleStack.pop_back();
    return *this;
  }
  /** Appends UTF-8 text using the active style. */
  ParagraphBuilder &addText(std::u8string_view utf8) {
    m_paragraph.appendText(utf8, m_styleStack.back());
    return *this;
  }
  /** Appends UTF-16 text using the active style. */
  ParagraphBuilder &addText(std::u16string_view utf16) {
    m_paragraph.appendText(utf16, m_styleStack.back());
    return *this;
  }
  /** Appends an inline object using the active style for surrounding glue. */
  ParagraphBuilder &addPlaceholder(const Placeholder &placeholder) {
    m_paragraph.appendPlaceholder(placeholder, m_styleStack.back());
    return *this;
  }
  /** Transfers the completed paragraph out of this builder. */
  [[nodiscard("the completed paragraph should be retained")]] Paragraph
  build() {
    return std::move(m_paragraph);
  }

private:
  Paragraph m_paragraph;
  std::vector<TextStyle> m_styleStack;
};

} // namespace textflow
