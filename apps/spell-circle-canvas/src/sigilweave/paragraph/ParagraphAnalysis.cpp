/** @file
 * Turning text into words: the ICU boundaries, the scripts, the bidi
 * levels and the fallback the analysis walks, the pattern breaks a
 * hyphenator opens inside a word, the strut every line is at least as
 * tall as, and the natural width a paragraph would take unwrapped.
 */

#include <include/core/SkFontMetrics.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <string>
#include <vector>

#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/paragraph/Paragraph.h"
#include "sigilweave/unicode/Unicode.h"

namespace sigil::weave {

namespace {

/** The strut of one shaping style, which is the whole of what a strut is:
 *  the face's own metrics at the size it is set. */
Paragraph::Strut strutOfShaping(FontContext& fontContext,
                                const ShapingStyle& shaping) {
  const SkFontMetrics metrics = faceMetrics(fontContext, shaping);
  Paragraph::Strut strut;
  strut.ascent = -metrics.fAscent;
  strut.height = lineHeightOf(metrics);
  strut.capHeight = metrics.fCapHeight;
  strut.xHeight = metrics.fXHeight;
  return strut;
}

}  // namespace

Paragraph::Strut Paragraph::strut(FontContext& fontContext) const {
  ShapingStyle shaping;
  if (!m_spans.empty()) shaping = m_spans.front().style.shaping;
  return strutOfShaping(fontContext, shaping);
}

Paragraph::Strut Paragraph::strutAt(FontContext& fontContext,
                                    uint32_t textOffset) const {
  ShapingStyle shaping;
  if (!m_spans.empty()) {
    // The span the offset falls in; past the end, the last one, so a block
    // that begins where the text does not still answers with real metrics.
    shaping = m_spans.back().style.shaping;
    for (const StyleSpan& span : m_spans)
      if (textOffset < span.end) {
        shaping = span.style.shaping;
        break;
      }
  }
  return strutOfShaping(fontContext, shaping);
}

float Paragraph::naturalWidth(FontContext& fontContext) {
  ensureShaped(fontContext);
  float width = 0;
  for (size_t wordIndex = 0; wordIndex < m_words.size(); ++wordIndex) {
    width += m_words[wordIndex].width;
    if (wordIndex + 1 < m_words.size()) width += m_words[wordIndex].spaceWidth;
  }
  return width;
}

void Paragraph::ensureAnalyzed(FontContext& fontContext) {
  if (m_dirty) {
    normalizeSpans();
    analyze(fontContext);
    m_dirty = false;
    m_paintDirty = false;
    return;
  }
  if (m_paintDirty) {
    m_paintDirty = false;
    reshapeShapedPrefix(fontContext);
  }
}

// Paint-only reconcile: the text didn't change, so line breaks, scripts,
// and bidi levels all stand — skip analyze()'s O(text) ICU passes and only
// re-derive segments for the words that were already shaped, against the
// new span list. Unchanged words hit the shape cache; only a boundary
// landing mid-word shapes new sub-segments.
void Paragraph::reshapeShapedPrefix(FontContext& fontContext) {
  const uint32_t previouslyShapedWordCount = m_shapedWordCount;
  m_shapedWordCount = 0;
  m_shapingSpanCursor = 0;
  m_shapingScriptCursor = 0;
  m_cachedWhitespaceStyleIndex = ~0u;
  m_cachedWhitespaceText.clear();
  for (; m_shapedWordCount < previouslyShapedWordCount; ++m_shapedWordCount) {
    Word& word = m_words[m_shapedWordCount];
    if (word.placeholderIndex >= 0)
      continue;  // slots carry no glyphs; width comes from the record
    word.m_segments.clear();
    word.width = 0;
    word.spaceWidth = 0;
    shapeWordContent(fontContext, word);
  }
}

void Paragraph::shapeWordsTo(FontContext& fontContext, uint32_t wordCount) {
  ensureAnalyzed(fontContext);
  const uint32_t upTo =
      std::min(wordCount, static_cast<uint32_t>(m_words.size()));
  for (; m_shapedWordCount < upTo; ++m_shapedWordCount)
    shapeWordContent(fontContext, m_words[m_shapedWordCount]);
}

void Paragraph::ensureShaped(FontContext& fontContext) {
  ensureAnalyzed(fontContext);
  ensureShapedTo(fontContext, static_cast<uint32_t>(m_words.size()));
}

std::span<const uint32_t> Paragraph::sentenceStarts() const {
  if (m_sentenceStartsValid) return m_sentenceStarts;
  m_sentenceStartsValid = true;
  m_sentenceStarts = unicode::sentenceStarts(m_text);
  return m_sentenceStarts;
}

uint64_t Paragraph::nextIdentity() {
  // Monotone for the life of the process and never reused, which is the
  // whole of what an identity has to be.
  static std::atomic<uint64_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

void Paragraph::setLineBreakLocale(std::string locale) {
  if (m_lineBreakLocale == locale) return;
  m_lineBreakLocale = std::move(locale);
  markDirty();  // break opportunities are decided in analyze()
}

void Paragraph::setKinsoku(KinsokuTable table) {
  if (m_kinsoku == table) return;
  m_kinsoku = std::move(table);
  markDirty();  // break opportunities are decided in analyze()
}

void Paragraph::openPatternBreaks(std::vector<unicode::LineBreak>& boundaries,
                                  std::vector<uint8_t>& isHyphen) const {
  const HyphenationLimits& limits = m_hyphenationLimits;
  std::vector<unicode::LineBreak> merged;
  std::vector<uint8_t> mergedFlags;
  merged.reserve(boundaries.size());
  mergedFlags.reserve(boundaries.size());
  std::vector<uint32_t> points;
  size_t spanCursor = 0;
  int32_t segmentStart = 0;
  for (const unicode::LineBreak& breakEntry : boundaries) {
    const uint32_t breakOffset = breakEntry.offset;
    const auto boundary = static_cast<int32_t>(breakOffset);
    if (boundary <= segmentStart) {
      merged.push_back(breakEntry);
      mergedFlags.push_back(0);
      continue;
    }
    // The word's content: the trailing whitespace and any typed soft hyphen
    // are the analysis's own business and never reach the hyphenator.
    int32_t contentEnd = boundary;
    while (contentEnd > segmentStart &&
           (unicode::isHardLineBreak(m_text[contentEnd - 1]) ||
            unicode::isWhitespace(m_text[contentEnd - 1])))
      --contentEnd;
    if (contentEnd > segmentStart && m_text[contentEnd - 1] == 0x00AD)
      --contentEnd;
    const auto length = static_cast<int32_t>(contentEnd - segmentStart);
    const bool longEnough =
        length >= limits.minimumWordLength &&
        length > limits.minimumLettersBefore + limits.minimumLettersAfter;
    if (longEnough) {
      while (spanCursor + 1 < m_spans.size() &&
             m_spans[spanCursor].end <= static_cast<uint32_t>(segmentStart))
        ++spanCursor;
      const std::u16string_view word(m_text.data() + segmentStart,
                                     static_cast<size_t>(length));
      size_t firstOffset = 0;
      const bool capitalised =
          unicode::isUpperCase(unicode::decodeAt(word, firstOffset));
      if (limits.capitalizedWords || !capitalised) {
        points.clear();
        const std::string& tag =
            m_spans.empty() ? std::string()
                            : m_spans[spanCursor].style.shaping.languageTag;
        m_hyphenator->breakPoints(word, tag, points);
        for (const uint32_t offset : points) {
          const auto inside = static_cast<int32_t>(offset);
          if (inside < limits.minimumLettersBefore ||
              length - inside < limits.minimumLettersAfter)
            continue;
          const auto absolute = static_cast<uint32_t>(segmentStart + inside);
          if (!merged.empty() && merged.back().offset >= absolute) continue;
          merged.push_back({absolute, false});
          mergedFlags.push_back(1);
        }
      }
    }
    merged.push_back(breakEntry);
    mergedFlags.push_back(0);
    segmentStart = boundary;
  }
  boundaries.swap(merged);
  isHyphen.swap(mergedFlags);
}

void Paragraph::analyze(FontContext& fontContext) {
  static_cast<void>(fontContext);  // analysis reads only the text
  m_words.clear();
  if (m_text.empty()) return;

  const int32_t textLength = static_cast<int32_t>(m_text.size());

  // ── Line-break opportunities (UAX #14) ─────────────────────────────────
  // Scratch buffers persist across analyses (one FontContext == one thread
  // by contract, so thread_local matches the ownership model).
  static thread_local std::vector<unicode::LineBreak> boundaries;
  unicode::lineBreaks(m_text, boundaries, m_lineBreakLocale);

  // A soft hyphen (U+00AD) is the one discretionary break opportunity the
  // word list carries. When it is turned off, the boundary UAX#14 opened
  // right after the hyphen is dropped here so the two halves fuse into a
  // single unbreakable Word: no breaker can split there, and the hyphen —
  // now interior to the word — shapes as the zero-advance default-ignorable
  // it is, so the word wraps or overflows whole and unmarked. A hyphen
  // followed by whitespace keeps its boundary either way, because the break
  // belongs to the space and not to the hyphen.
  if (!m_softHyphenBreaks) {
    size_t keptCount = 0;
    for (const unicode::LineBreak& entry : boundaries) {
      if (entry.offset < static_cast<uint32_t>(textLength) &&
          m_text[static_cast<size_t>(entry.offset) - 1] == 0x00AD)
        continue;
      boundaries[keptCount++] = entry;
    }
    boundaries.resize(keptCount);
    if (boundaries.empty())
      boundaries.push_back({static_cast<uint32_t>(textLength), false});
  }

  // KINSOKU SHORI: a boundary that would open a line with a character
  // that may not open one, or close a line with one that may not close
  // one, is simply not a boundary. Dropping it here is push-out — the
  // word before comes down with the character — and it means neither
  // breaker knows the rule, exactly as neither knows the soft-hyphen one.
  if (!m_kinsoku.empty()) {
    size_t keptCount = 0;
    for (const unicode::LineBreak& entry : boundaries) {
      const uint32_t boundary = entry.offset;
      const bool interior = boundary < static_cast<uint32_t>(textLength);
      const bool opensProhibited =
          interior &&
          m_kinsoku.notLineStart.find(m_text[boundary]) != std::u16string::npos;
      const bool closesProhibited =
          interior && boundary > 0 &&
          m_kinsoku.notLineEnd.find(m_text[boundary - 1]) !=
              std::u16string::npos;
      // A break the text itself demands is never a prohibition's to drop.
      if ((opensProhibited || closesProhibited) && !entry.mandatory) continue;
      boundaries[keptCount++] = entry;
    }
    boundaries.resize(keptCount);
    if (boundaries.empty())
      boundaries.push_back({static_cast<uint32_t>(textLength), false});
  }

  // Pattern hyphenation: break opportunities INSIDE a word, which UAX #14
  // never opens because where a word may be split is a fact about a
  // language and not about Unicode. Each opportunity becomes its own
  // boundary, so a hyphenated word is several Words that a breaker may end
  // a line at exactly as it may end one at a typed soft hyphen — and the
  // flag beside each boundary is what says which of them carry a hyphen.
  static thread_local std::vector<uint8_t> patternBreak;
  patternBreak.assign(boundaries.size(), 0);
  if (m_hyphenator && m_softHyphenBreaks)
    openPatternBreaks(boundaries, patternBreak);

  // ── Script runs ────────────────────────────────────────────────────────
  static thread_local std::vector<unicode::ScriptRun> scriptRuns;
  unicode::itemize(m_text, scriptRuns);

  // ── Bidi levels (one run for LTR-only text, per-unit levels otherwise) ─
  const std::vector<unicode::BidiRun> bidiRuns = unicode::bidi(m_text);
  m_uniformBidirectionalLevel = 0;
  m_bidirectionalLevels.clear();
  if (bidiRuns.size() == 1) {
    m_uniformBidirectionalLevel = bidiRuns.front().level;
  } else if (bidiRuns.size() > 1) {
    m_bidirectionalLevels.resize(static_cast<size_t>(textLength));
    for (const unicode::BidiRun& run : bidiRuns)
      std::fill(m_bidirectionalLevels.begin() + run.start,
                m_bidirectionalLevels.begin() + run.end, run.level);
  }
  const uint8_t* bidiLevels =
      m_bidirectionalLevels.empty() ? nullptr : m_bidirectionalLevels.data();
  const uint8_t uniformLevel = m_uniformBidirectionalLevel;

  // ── Words: one per break segment (segment/shape runs resolved lazily) ──
  m_words.reserve(boundaries.size());
  int placeholdersSeen = 0;
  int32_t segmentStart = 0;
  for (size_t boundaryIndex = 0; boundaryIndex < boundaries.size();
       ++boundaryIndex) {
    const int32_t boundary =
        static_cast<int32_t>(boundaries[boundaryIndex].offset);
    const bool patternHyphen = patternBreak[boundaryIndex] != 0;
    if (boundary <= segmentStart) continue;

    // Object-replacement characters are placeholder slots (see
    // appendPlaceholder): unbreakable fixed-size words with no glyphs.
    // A slot is usually isolated as a break unit of its own, but trailing
    // closing punctuation glues to the character before it (e.g. "￼."), so
    // slots are peeled off the front of the segment rather than expected
    // to arrive alone.
    while (segmentStart < boundary &&
           m_text[static_cast<size_t>(segmentStart)] == 0xFFFC &&
           static_cast<size_t>(placeholdersSeen) < m_placeholders.size()) {
      Word placeholderWord;
      placeholderWord.textBegin = static_cast<uint32_t>(segmentStart);
      placeholderWord.textEnd = placeholderWord.whitespaceEnd =
          static_cast<uint32_t>(segmentStart + 1);
      placeholderWord.bidiLevel =
          bidiLevels ? bidiLevels[segmentStart] : uniformLevel;
      placeholderWord.placeholderIndex = placeholdersSeen++;
      placeholderWord.width =
          m_placeholders[static_cast<size_t>(placeholderWord.placeholderIndex)]
              .width;
      m_words.push_back(std::move(placeholderWord));
      ++segmentStart;
    }
    if (boundary <= segmentStart)
      continue;  // the segment was nothing but slots

    // Trailing whitespace (including any hard-break characters at the end).
    int32_t whitespaceStart = boundary;
    while (whitespaceStart > segmentStart &&
           (unicode::isHardLineBreak(m_text[whitespaceStart - 1]) ||
            unicode::isWhitespace(m_text[whitespaceStart - 1])))
      --whitespaceStart;

    Word word;
    word.textBegin = static_cast<uint32_t>(segmentStart);
    word.whitespaceEnd = static_cast<uint32_t>(boundary);
    // WHETHER THE TEXT DEMANDS A BREAK HERE IS THE SEGMENTATION'S ANSWER,
    // reported beside the boundary that opened. Re-deriving it from the
    // characters before the boundary is a second, worse implementation of
    // a rule the analysis already applied.
    word.mandatoryBreakAfter = boundaries[boundaryIndex].mandatory;
    word.bidiLevel = bidiLevels ? bidiLevels[segmentStart] : uniformLevel;

    // A trailing soft hyphen (U+00AD) is a discretionary break: it never
    // shapes, but marks that a hyphen may be rendered if a breaker splits
    // here.
    if (whitespaceStart > segmentStart &&
        m_text[whitespaceStart - 1] == 0x00AD) {
      word.hyphenBreak = true;
      --whitespaceStart;
    }
    // A pattern opportunity consumes no character: the word simply ends
    // where the language says it may, and the hyphen is drawn only if a
    // breaker takes the break.
    if (patternHyphen) word.hyphenBreak = true;
    word.textEnd = static_cast<uint32_t>(whitespaceStart);

    // Tab-aware layouts (ParagraphLayoutOptions::tabStops) treat the glue
    // after this word as an advance-to-stop instead of measured whitespace.
    for (int32_t codeUnitIndex = whitespaceStart;
         codeUnitIndex < boundary && !word.tabAfter; ++codeUnitIndex)
      word.tabAfter = m_text[static_cast<size_t>(codeUnitIndex)] == u'\t';

    if (whitespaceStart > segmentStart) {
      size_t codePointEnd = static_cast<size_t>(segmentStart);
      word.ideographic =
          unicode::isFullWidth(unicode::decodeAt(m_text, codePointEnd));
    }

    // Glyphs, widths, and glue come later: shapeWordContent() fills them in
    // lazily (see ensureShapedTo), so text past the layout frontier never
    // pays for HarfBuzz.
    m_words.push_back(std::move(word));
    segmentStart = boundary;
  }

  // Persist what lazy shaping needs: the script runs (the levels are
  // already in place).
  m_scriptRunEnds.clear();
  m_scriptRunEnds.reserve(scriptRuns.size());
  for (const unicode::ScriptRun& run : scriptRuns)
    m_scriptRunEnds.push_back({run.end, run.script});
  m_shapedWordCount = 0;
  m_shapingSpanCursor = 0;
  m_shapingScriptCursor = 0;
  m_cachedWhitespaceStyleIndex = ~0u;
  m_cachedWhitespaceText.clear();
}

// Shapes one word's content: segment splitting (style / script / bidi /
// fallback-typeface / vertical-form boundaries), the discretionary hyphen,
// and trailing-whitespace glue. Called in ascending word order only — the

}  // namespace sigil::weave
