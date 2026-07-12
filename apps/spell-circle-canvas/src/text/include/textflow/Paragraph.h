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

// One shaped run inside a Word (usually the only one; more when a style
// boundary, script change, or font fallback splits the word).
struct WordSegment {
  ShapedWordRef shaped;
  uint32_t styleIndex = 0; // index into Paragraph::spans()
  float xOffset = 0;       // pen offset from the word origin
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
};

// Text + styles + the cached analysis pipeline (ICU line breaks, bidi,
// script itemization, font fallback, word shaping). Mutations mark the
// analysis dirty; ensureShaped() re-runs it, and the FontContext shape cache
// makes that re-run a hash lookup for every word that didn't change — that
// cache is the incremental-update mechanism, exactly like Chrome's
// ShapeCache re-shaping only the edited word.
class Paragraph {
public:
  Paragraph() = default;

  // ── Building ──────────────────────────────────────────────────────────
  void clear();
  void appendText(std::string_view utf8, const TextStyle &style);
  void appendText(std::u16string_view utf16, const TextStyle &style);

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

  // ── Analysis ──────────────────────────────────────────────────────────
  // Runs segmentation + shaping if anything changed since the last call.
  void ensureShaped(FontContext &ctx);
  bool needsShaping() const { return m_dirty; }
  const std::vector<Word> &words() const { return m_words; }

  // Line-height inputs from the first span's font (the "strut"): returns
  // {ascent (positive), height} for a default single-spaced line.
  struct Strut {
    float ascent = 0;
    float height = 0;
  };
  Strut strut(FontContext &ctx) const;

private:
  void markDirty() { m_dirty = true; }
  void normalizeSpans();
  void analyze(FontContext &ctx);

  std::u16string m_text;
  std::vector<StyleSpan> m_spans;
  std::vector<Word> m_words;
  bool m_dirty = true;
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
