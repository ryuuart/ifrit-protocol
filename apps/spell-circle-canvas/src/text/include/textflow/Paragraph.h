#pragma once

#include "Shaper.h"
#include "Style.h"

#include <absl/container/inlined_vector.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace textflow {

class FontContext;

// Style applied to a contiguous UTF-16 range. Spans are kept normalized:
// sorted, non-overlapping, covering the whole text.
struct StyleSpan {
  uint32_t start = 0;
  uint32_t end = 0;
  TextStyle style;
};

// How a segment is placed relative to the flow direction. kFlow is every
// segment of a horizontal paragraph; the others only appear in vertical
// paragraphs (resolved from ShapingStyle::verticalForm / UTR#50).
enum class SegmentForm : uint8_t {
  kFlow,        // pen-aligned with the interval (horizontal fast path)
  kUpright,     // vertical-shaped word stacked down the column
  kRotated,     // horizontal-shaped word rotated to the interval direction
  kTateChuYoko, // horizontal-shaped word set upright across the column
};

// One shaped run inside a Word (usually the only one; more when a style
// boundary, script change, or font fallback splits the word).
struct WordSegment {
  ShapedWordRef shaped;
  uint32_t styleIndex = 0; // index into Paragraph::spans()
  float xOffset = 0;       // pen offset from the word origin (for
                           // kTateChuYoko this lands on the run's baseline)
  SegmentForm form = SegmentForm::kFlow;
};

// An inline object slot woven into the flow (SkParagraph's placeholder
// idea): the breakers treat it as an unbreakable word of the given size and
// the layout reports the rect where it landed, so callers can draw pills,
// icons, or images *inside* the text flow. Anchored in the text as an
// object-replacement character (U+FFFC), matched to its record by
// occurrence order.
struct Placeholder {
  float width = 0;
  float height = 0;
  // The box's bottom edge sits this far below the baseline (0 = bottom on
  // the baseline, like an inline image; ~descent centres a pill on x-height).
  float baselineDrop = 0;
};

// The atomic layout unit: the text between two line-break opportunities.
// Content and trailing whitespace are measured separately so justification
// can treat the whitespace as stretchable glue.
struct Word {
  uint32_t textBegin = 0; // content range, whitespace excluded
  uint32_t textEnd = 0;
  uint32_t wsEnd = 0; // end of trailing whitespace (== textEnd when none)

  // Inline storage: nearly every word is a single uniform run, so the
  // common case costs no allocation when the word list is rebuilt.
  absl::InlinedVector<WordSegment, 1> segments;
  float width = 0;      // content advance
  float spaceWidth = 0; // trailing-whitespace advance (justification glue)

  uint8_t bidiLevel = 0;
  bool mandatoryBreakAfter = false; // '\n' and friends
  // Break opportunity with zero glue (CJK): justification may expand here
  // even though there is no space.
  bool ideographic = false;
  // Content ended with a soft hyphen (U+00AD, stripped from shaping): a
  // discretionary break. `hyphenGlyph` is the cached shaped "-" to render
  // when a breaker actually breaks here.
  bool hyphenBreak = false;
  ShapedWordRef hyphenGlyph;

  // ≥0: this word is Paragraph::placeholders()[placeholderIndex] — no
  // glyphs, `width` comes from the placeholder record.
  int placeholderIndex = -1;
};

// Text + styles + the cached analysis pipeline (ICU line breaks, bidi,
// script itemization, font fallback, word shaping). Mutations mark the
// analysis dirty; ensureShaped() re-runs it, and the FontContext shape cache
// makes that re-run a hash lookup for every word that didn't change — that
// cache is the incremental-update mechanism, exactly like Chrome's
// ShapeCache re-shaping only the edited word.
// Flow direction the paragraph is shaped for. Vertical-RL is the CJK book
// layout: characters run top to bottom, columns right to left. The layout
// geometry must match (columns with dir=(0,1), e.g. VerticalBlockFlow).
enum class WritingMode : uint8_t { kHorizontal, kVerticalRL };

class Paragraph {
public:
  Paragraph() = default;

  // ── Building ──────────────────────────────────────────────────────────
  void clear();
  void appendText(std::string_view utf8, const TextStyle &style);
  void appendText(std::u16string_view utf16, const TextStyle &style);

  // Vertical mode re-itemizes and re-shapes (once — both orientations are
  // separate shape-cache entries, so toggling back and forth is warm).
  void setWritingMode(WritingMode mode);
  WritingMode writingMode() const { return m_writingMode; }

  // ── Inline placeholders (pills, icons, images in the flow) ────────────
  // Appends a slot at the end of the text (a U+FFFC character); `style`
  // decides the surrounding glue/justification context. Slots are matched
  // to records by occurrence order, so avoid replaceText edits that add or
  // remove U+FFFC characters directly.
  void appendPlaceholder(const Placeholder &placeholder,
                         const TextStyle &style);
  const std::vector<Placeholder> &placeholders() const {
    return m_placeholders;
  }
  // Resizing a slot re-runs analysis (cache-hot for all real words).
  void setPlaceholder(size_t index, const Placeholder &placeholder);

  // ── Editing (UTF-16 ranges) ───────────────────────────────────────────
  // Replaces [start, end) with `utf8`; spans shift/split to follow.
  void replaceText(uint32_t start, uint32_t end, std::string_view utf8);
  // Restyles a range (splits spans as needed). Re-shapes only words whose
  // shaping inputs actually changed — the rest hit the cache.
  void setStyle(uint32_t start, uint32_t end, const TextStyle &style);
  // Paint-only restyle: same span surgery, but shaping keys are untouched,
  // so ensureShaped() re-shapes nothing at all.
  void setPaint(uint32_t start, uint32_t end, const PaintStyle &paint);

  const std::u16string &text() const { return m_text; }
  const std::vector<StyleSpan> &spans() const { return m_spans; }

  // ── Edit history (external range tracking) ────────────────────────────
  // Every text mutation is recorded under a monotonically increasing
  // revision, so external structures (e.g. Query.h's MarkerSet) can keep
  // UTF-16 ranges in sync without wrapping every edit call. History is
  // bounded; a consumer that falls too far behind must rebuild its ranges.
  struct EditOp {
    uint32_t start = 0;
    uint32_t removed = 0;  // UTF-16 units deleted at `start`
    uint32_t inserted = 0; // UTF-16 units inserted at `start`
  };
  uint64_t revision() const { return m_revision; }
  // Appends the ops after `sinceRevision`, oldest first. Returns false when
  // history no longer reaches back that far.
  bool editsSince(uint64_t sinceRevision, std::vector<EditOp> &out) const;

  // ── Analysis ──────────────────────────────────────────────────────────
  // Runs segmentation + shaping if anything changed since the last call.
  void ensureShaped(FontContext &ctx);
  // Segmentation only (ICU boundaries, bidi, scripts — no HarfBuzz work):
  // words() gets its break/direction structure but no glyphs or widths yet.
  // Layout drives shaping lazily from here, so a paragraph that overflows
  // its geometry only ever shapes the words that can actually land.
  void ensureAnalyzed(FontContext &ctx);
  // Shapes words [0, wordCount); ascending and idempotent — the breakers
  // call this just ahead of their frontier.
  void ensureShapedTo(FontContext &ctx, uint32_t wordCount);
  uint32_t shapedWordCount() const { return m_shapedWords; }
  bool needsShaping() const { return m_dirty; }
  const std::vector<Word> &words() const { return m_words; }

  // Line-height inputs from the first span's font (the "strut"): returns
  // {ascent (positive), height} for a default single-spaced line.
  struct Strut {
    float ascent = 0;
    float height = 0;
  };
  Strut strut(FontContext &ctx) const;

  // Unwrapped single-line width: content plus inter-word glue, the final
  // word's trailing whitespace excluded. Shapes on demand (cache-hot).
  float naturalWidth(FontContext &ctx);

private:
  void markDirty() { m_dirty = true; }
  void recordEdit(uint32_t start, uint32_t removed, uint32_t inserted);
  void normalizeSpans();
  void analyze(FontContext &ctx);
  void shapeWordContent(FontContext &ctx, Word &word);

  std::u16string m_text;
  std::vector<StyleSpan> m_spans;
  std::vector<Word> m_words;
  std::vector<Placeholder> m_placeholders;
  WritingMode m_writingMode = WritingMode::kHorizontal;
  bool m_dirty = true;

  // Itemization results analyze() leaves behind for lazy shaping
  // (ensureShapedTo). `script` is a UScriptCode, stored as int32_t to keep
  // ICU out of this header.
  struct ScriptRunEnd {
    uint32_t end = 0;
    int32_t script = 0;
  };
  std::vector<ScriptRunEnd> m_scripts;
  std::vector<uint8_t> m_levels; // per-unit bidi levels; empty when uniform
  uint8_t m_uniformLevel = 0;
  uint32_t m_shapedWords = 0; // words [0, m_shapedWords) carry glyphs
  uint32_t m_shapeSpanCursor = 0, m_shapeScriptCursor = 0;
  uint32_t m_glueMemoStyle = ~0u; // common-glue memo (" " in one style)
  std::u16string m_glueMemoText;
  float m_glueMemoWidth = 0;

  uint64_t m_revision = 0;
  std::vector<EditOp> m_edits; // ops [m_editsBase, m_revision)
  uint64_t m_editsBase = 0;
};

// SkParagraph-style builder for the push/pop idiom; thin sugar over
// Paragraph::appendText.
class ParagraphBuilder {
public:
  explicit ParagraphBuilder(const TextStyle &base) { m_stack.push_back(base); }

  ParagraphBuilder &pushStyle(const TextStyle &style) {
    m_stack.push_back(style);
    return *this;
  }
  ParagraphBuilder &pop() {
    if (m_stack.size() > 1)
      m_stack.pop_back();
    return *this;
  }
  ParagraphBuilder &addText(std::string_view utf8) {
    m_paragraph.appendText(utf8, m_stack.back());
    return *this;
  }
  Paragraph build() { return std::move(m_paragraph); }

private:
  Paragraph m_paragraph;
  std::vector<TextStyle> m_stack;
};

} // namespace textflow
