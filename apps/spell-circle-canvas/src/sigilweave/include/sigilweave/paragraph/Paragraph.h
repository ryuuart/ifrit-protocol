#pragma once

/** @file
 * @ingroup weave-document
 *
 * The document model: UTF-16 text carrying normalized style spans, optional
 * inline placeholders, and a horizontal or vertical-RL writing mode. Build
 * one with ParagraphBuilder or append directly, edit it in place, and hand
 * the result to layoutParagraph() with a FontContext and a flow geometry.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sigilweave/paragraph/Hyphenation.h"
#include "sigilweave/paragraph/Word.h"
#include "sigilweave/style/Style.h"
#include "sigilweave/unicode/Unicode.h"

/** THE TEXT ENGINE: shaping, line breaking and painting for styled text,
 *  built directly on HarfBuzz and ICU over Skia's drawing primitives. A
 *  passage is a `Paragraph`, analysed into `Word`s and shaped through a
 *  per-thread `FontContext` so an edit reshapes only the words it
 *  touched; `layoutParagraph()` breaks them into a `FlowGeometry` and
 *  answers a `ParagraphLayout`, which draws, measures and is queried by a
 *  `Selector`. Reach for it when text must be SET rather than merely
 *  drawn; a single label is cheaper through `kit::drawLabel`. The engine
 *  draws: it owns no window, no device and no document format. */
namespace sigil::weave {

class FontContext;

/// Style applied to a contiguous UTF-16 range. Spans are kept normalized:
/// sorted, non-overlapping, covering the whole text.
struct StyleSpan {
  uint32_t start = 0;  ///< inclusive start, UTF-16 code units
  uint32_t end = 0;    ///< exclusive end, UTF-16 code units
  TextStyle style;     ///< applies to every code unit in [start, end)
};

/// UTF-16 code-unit range into a Paragraph's text, end exclusive. The common
/// currency between the query layer (Query.h), scoped searches, and batch
/// restyling.
struct CharRange {
  uint32_t start = 0;  ///< inclusive start, UTF-16 code units
  uint32_t end = 0;    ///< exclusive end, UTF-16 code units
  /** Returns whether this half-open range contains no UTF-16 code units. */
  [[nodiscard]] bool empty() const noexcept { return end <= start; }
  /** Compares both endpoints. */
  bool operator==(const CharRange&) const = default;
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
  void appendText(std::u8string_view utf8, const TextStyle& style);
  /** Appends UTF-16 text using `style`. */
  void appendText(std::u16string_view utf16, const TextStyle& style);

  /** Sets the writing mode. Vertical mode re-itemizes and re-shapes (once —
   * both orientations are separate shape-cache entries, so toggling back
   * and forth is warm).
   */
  void setWritingMode(WritingMode mode);
  /** Returns the direction in which this paragraph is shaped. */
  [[nodiscard]] WritingMode writingMode() const noexcept {
    return m_writingMode;
  }

  /** Sets whether a soft hyphen (U+00AD) opens a break opportunity. True
   * (the default) lets a breaker end a line there and render
   * `Word::hyphenGlyph`; false fuses the word into one unbreakable `Word`
   * that wraps or overflows whole. Re-runs the ICU segmentation.
   * @trap `layoutParagraph` sets this from `HyphenationOptions::enabled`,
   * so a caller going through it must not call this. */
  void setSoftHyphenBreaks(bool enabled);
  /** Returns whether a soft hyphen opens a break opportunity. */
  [[nodiscard]] bool softHyphenBreaks() const noexcept {
    return m_softHyphenBreaks;
  }

  /** Sets what is asked where INSIDE a word may break. Empty (the default)
   * leaves the author's soft hyphens as the only discretionary
   * opportunities. Consulted once per word during analysis, so setting it
   * re-runs the segmentation, and the paragraph KEEPS the hyphenator.
   * @silent soft-hyphen breaks are off, that being the switch for the
   * whole discretionary idea. */
  void setHyphenator(std::shared_ptr<const Hyphenator> hyphenator,
                     HyphenationLimits limits);
  /** Sets which characters may not stand at a line's edge. A prohibition
   * is a break opportunity the segmentation never opens, so no breaker
   * learns a rule.
   * @trap `layoutParagraph` sets this from
   * `ParagraphLayoutOptions::kinsoku`, so a caller going through it must
   * not call this. */
  void setKinsoku(KinsokuTable table);
  /** THIS PARAGRAPH, TOLD APART FROM EVERY OTHER — a number issued once
   * when it is built and never issued again, so a cache keyed on it cannot
   * be answered for a different paragraph that happens to stand where a
   * freed one stood.
   */
  [[nodiscard]] uint64_t identity() const { return m_identity; }

  /** A NUMBER THAT CHANGES WHENEVER THE WORD LIST CAN HAVE CHANGED — an
   * edit, a restyle, a change of break settings — so anything keyed on
   * this paragraph's WORDS misses rather than answering stale.
   * @trap It is not `revision`, which counts edits alone and stands still
   * while a restyle moves every word; and shaping more of the text does
   * not change it, a shaped word's advance never moving after. */
  [[nodiscard]] uint64_t wordRevision() const { return m_wordRevision; }

  /** Sets the TAILORING the line segmentation runs under: a BCP 47 tag,
   * optionally carrying ICU's line-break keyword — "ja@lb=strict",
   * "zh@lb=loose" — and empty is the untailored behaviour. It is where a
   * script's own prohibitions come from before any table does; a
   * `KinsokuTable` is the seam for a house's additions on top.
   */
  void setLineBreakLocale(std::string locale);
  /** The tailoring the line segmentation runs under. */
  [[nodiscard]] const std::string& lineBreakLocale() const {
    return m_lineBreakLocale;
  }
  /** Returns the prohibitions this paragraph was segmented under. */
  [[nodiscard]] const KinsokuTable& kinsoku() const noexcept {
    return m_kinsoku;
  }
  /** Returns what is asked where inside a word may break, or empty. */
  [[nodiscard]] const std::shared_ptr<const Hyphenator>& hyphenator()
      const noexcept {
    return m_hyphenator;
  }

  // ── Inline placeholders (pills, icons, images in the flow) ────────────
  /** Appends a U+FFFC-backed inline slot using `style` for flow context.
   *
   * Slots map to records by occurrence order, so direct text edits should
   * not add or remove object-replacement characters.
   */
  void appendPlaceholder(const Placeholder& placeholder,
                         const TextStyle& style);
  /** Returns inline object records in their text occurrence order. */
  const std::vector<Placeholder>& placeholders() const {
    return m_placeholders;
  }
  /** Resizes a slot and invalidates layout while leaving real words cache-hot.
   */
  void setPlaceholder(size_t index, const Placeholder& placeholder);

  // ── Editing (UTF-16 ranges) ───────────────────────────────────────────
  /** Replaces UTF-16 range `[start, end)` with UTF-8 and adjusts spans. */
  void replaceText(uint32_t start, uint32_t end, std::u8string_view utf8);
  /** Applies shaping and paint configuration to a UTF-16 range (splits spans
   * as needed). Re-shapes only words whose shaping inputs actually changed
   * — the rest hit the cache.
   */
  void setStyle(uint32_t start, uint32_t end, const TextStyle& style);
  /** Applies draw-time paint to one UTF-16 range without re-analyzing
   * text. Shaping keys are untouched and the text did not move, so the
   * next ensure only re-derives the already-shaped words' segments against
   * the new span list; the cost is bounded by the shaped prefix rather
   * than by the text.
   */
  void setPaint(uint32_t start, uint32_t end, const PaintStyle& paint);
  /** Applies one paint to sanitized ranges in a single span-list rebuild
   * (batch form): restyling N marker ranges costs one pass, not N quadratic
   * rebuilds. Ranges may arrive unsorted/overlapping; they are sanitized
   * internally.
   */
  void setPaint(std::span<const CharRange> ranges, const PaintStyle& paint);

  /** Returns the paragraph's UTF-16 storage. */
  const std::u16string& text() const { return m_text; }
  /** Returns normalized style spans covering `text()`. */
  const std::vector<StyleSpan>& spans() const { return m_spans; }

  // ── Edit history (external range tracking) ────────────────────────────
  /// Every text mutation is recorded under a monotonically increasing
  /// revision, so external structures (e.g. Query.h's MarkerSet) can keep
  /// UTF-16 ranges in sync without wrapping every edit call. History is
  /// bounded and, when it fills, the older half is discarded in one go — so
  /// the lookback a consumer can count on is half the cap, not the cap. A
  /// consumer that falls further behind than that must rebuild its ranges.
  struct TextEdit {
    uint32_t start = 0;     ///< UTF-16 position where the edit applied
    uint32_t removed = 0;   ///< UTF-16 units deleted at `start`
    uint32_t inserted = 0;  ///< UTF-16 units inserted at `start`
  };
  /** Returns the current monotonically increasing text revision. */
  uint64_t revision() const { return m_revision; }
  /** Appends the edits after `sinceRevision`, oldest first, to `edits`.
   * Returns false when history no longer reaches back that far and the
   * expired edit history requires the caller to rebuild tracked ranges.
   */
  [[nodiscard("expired edit history requires rebuilding tracked ranges")]]
  bool editsSince(uint64_t sinceRevision, std::vector<TextEdit>& edits) const;

  // ── Analysis ──────────────────────────────────────────────────────────
  /** Ensures analysis and glyph data are available for the whole paragraph.
   * Runs segmentation + shaping if anything changed since the last call.
   */
  void ensureShaped(FontContext& fontContext);
  /** Ensures break, bidi and script analysis without shaping glyphs:
   * segmentation only — ICU boundaries, bidi, scripts, no HarfBuzz work —
   * so `words` gets its break and direction structure and no glyphs or
   * widths. Layout drives shaping lazily from here.
   */
  void ensureAnalyzed(FontContext& fontContext);
  /** Lazily shapes words in `[0, wordCount)`, ascending and idempotent —
   * the breakers call this just ahead of their frontier.
   * @silent the frontier has already passed @p wordCount, which is the
   * answer on all but the few words that actually advance it.
   */
  void ensureShapedTo(FontContext& fontContext, uint32_t wordCount) {
    if (!m_dirty && !m_paintDirty && wordCount <= m_shapedWordCount) return;
    shapeWordsTo(fontContext, wordCount);
  }
  /** Returns the number of words whose glyph data is currently available. */
  uint32_t shapedWordCount() const { return m_shapedWordCount; }
  /** Returns whether analysis or paint reconciliation is pending. */
  bool needsShaping() const { return m_dirty || m_paintDirty; }
  /** Returns the analyzed line-break units in logical text order. */
  const std::vector<Word>& words() const { return m_words; }

  /** Returns the UTF-16 offset where each sentence of the text starts,
   * ascending, the first entry always 0 and empty for empty text; the
   * sentence containing an offset is the last entry not greater than it.
   * ICU sentence segmentation, run on first call and reused until the text
   * changes, and independent of `ensureAnalyzed` — no shaping, no words
   * and no fonts are involved. */
  [[nodiscard]] std::span<const uint32_t> sentenceStarts() const;

  /// Line-height inputs from a span's font (the "strut"): the ascent and
  /// height of a default single-spaced line, plus the two heights a frame
  /// may seat its first baseline on.
  struct Strut {
    float ascent = 0;  ///< baseline distance below the line top, px (positive)
    float height = 0;  ///< default single-spaced line height, px
    float capHeight = 0;  ///< distance from baseline to cap top, px
    float xHeight = 0;    ///< distance from baseline to x-height, px
  };
  /** Returns positive ascent and default line height from the first span. */
  [[nodiscard]] Strut strut(FontContext& fontContext) const;
  /** Returns the same, measured from the first span the UTF-16 offset
   * `textOffset` falls in — A BLOCK'S OWN STRUT, which is what its pitch is
   * measured from. A text of one style answers identically wherever it is
   * asked, which is why a layout that says nothing about blocks lays out
   * exactly as it always did. */
  [[nodiscard]] Strut strutAt(FontContext& fontContext,
                              uint32_t textOffset) const;

  /** Returns cache-hot unwrapped width without final trailing whitespace.
   *
   * The unwrapped single-line width is content plus inter-word glue, the
   * final word's trailing whitespace excluded. Shapes on demand.
   */
  [[nodiscard]] float naturalWidth(FontContext& fontContext);

 private:
  // Analyses if it must and shapes forward to `wordCount`; the frontier
  // test that spares the call lives in ensureShapedTo().
  void shapeWordsTo(FontContext& fontContext, uint32_t wordCount);

  void markDirty() {
    m_dirty = true;
    ++m_wordRevision;
  }
  // Paint edits only move span boundaries: analysis (words, scripts, bidi)
  // stands and the shaped prefix just needs its segments re-derived.
  void markPaintDirty() {
    if (!m_dirty) m_paintDirty = true;
  }
  void recordEdit(uint32_t start, uint32_t removedLength,
                  uint32_t insertedLength);
  void normalizeSpans();
  void analyze(FontContext& fontContext);
  // Splits every word of `boundaries` at the offsets the hyphenator names,
  // under m_hyphenationLimits, marking the added boundaries in `isHyphen`.
  void openPatternBreaks(std::vector<unicode::LineBreak>& boundaries,
                         std::vector<uint8_t>& isHyphen) const;
  void reshapeShapedPrefix(FontContext& fontContext);
  void shapeWordContent(FontContext& fontContext, Word& word);

  std::u16string m_text;
  std::vector<StyleSpan> m_spans;
  std::vector<Word> m_words;
  std::vector<Placeholder> m_placeholders;
  WritingMode m_writingMode = WritingMode::kHorizontal;
  // Whether analyze() keeps the UAX#14 boundary a soft hyphen opens.
  bool m_softHyphenBreaks = true;
  // The tailoring the line segmentation runs under; empty is untailored.
  std::string m_lineBreakLocale;
  // Where inside a word analyze() opens further break opportunities;
  // held, and empty for the typed soft hyphens alone.
  std::shared_ptr<const Hyphenator> m_hyphenator;
  HyphenationLimits m_hyphenationLimits;
  KinsokuTable m_kinsoku;
  bool m_dirty = true;
  // Changes with everything that can move a word; see wordRevision().
  uint64_t m_wordRevision = 1;
  // Issued once per paragraph and never reissued; see identity().
  uint64_t m_identity = nextIdentity();
  static uint64_t nextIdentity();
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

  // Sentence starts are derived from the text alone, so they survive every
  // style edit and are rebuilt only when recordEdit() fires.
  mutable std::vector<uint32_t> m_sentenceStarts;
  mutable bool m_sentenceStartsValid = false;

  uint64_t m_revision = 0;
  std::vector<TextEdit> m_editHistory;
  uint64_t m_editHistoryBaseRevision = 0;
};

/// SkParagraph-style builder for the push/pop idiom; thin sugar over
/// Paragraph::appendText.
class ParagraphBuilder {
 public:
  /** Starts a paragraph with `baseStyle` at the bottom of the style stack. */
  explicit ParagraphBuilder(const TextStyle& baseStyle) {
    m_styleStack.push_back(baseStyle);
  }

  /** Pushes a style used by subsequent text and placeholder additions. */
  ParagraphBuilder& pushStyle(const TextStyle& style) {
    m_styleStack.push_back(style);
    return *this;
  }
  /** Pops the active style while preserving the required base style. */
  ParagraphBuilder& popStyle() {
    if (m_styleStack.size() > 1) m_styleStack.pop_back();
    return *this;
  }
  /** Appends UTF-8 text using the active style. */
  /** The same run from UTF-8 held as `char`. */
  ParagraphBuilder& addText(std::string_view utf8) {
    return addText(std::u8string_view(
        reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
  }
  ParagraphBuilder& addText(std::u8string_view utf8) {
    m_paragraph.appendText(utf8, m_styleStack.back());
    return *this;
  }
  /** Appends UTF-16 text using the active style. */
  ParagraphBuilder& addText(std::u16string_view utf16) {
    m_paragraph.appendText(utf16, m_styleStack.back());
    return *this;
  }
  /** Appends an inline object using the active style for surrounding glue. */
  ParagraphBuilder& addPlaceholder(const Placeholder& placeholder) {
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

}  // namespace sigil::weave
