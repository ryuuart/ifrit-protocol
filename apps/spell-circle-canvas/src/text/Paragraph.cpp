#include "textflow/Paragraph.h"
#include "FontContextImpl.h"
#include "textflow/FontContext.h"

#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>

#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>

#include <algorithm>
#include <cassert>

namespace textflow {

namespace {

std::u16string utf8ToUtf16(std::string_view utf8) {
  if (utf8.empty())
    return {};
  std::u16string out;
  out.resize(utf8.size()); // UTF-16 never has more units than UTF-8 bytes
  UErrorCode status = U_ZERO_ERROR;
  int32_t written = 0;
  u_strFromUTF8(reinterpret_cast<UChar *>(out.data()),
                static_cast<int32_t>(out.size()), &written, utf8.data(),
                static_cast<int32_t>(utf8.size()), &status);
  if (U_FAILURE(status))
    return {};
  out.resize(written);
  return out;
}

bool isHardBreakChar(char16_t c) {
  return c == u'\n' || c == u'\r' || c == 0x0085 || c == 0x2028 || c == 0x2029;
}

// Scripts whose break opportunities carry no glue (no spaces between words).
bool isIdeographicScript(UScriptCode script) {
  switch (script) {
  case USCRIPT_HAN:
  case USCRIPT_HIRAGANA:
  case USCRIPT_KATAKANA:
  case USCRIPT_KATAKANA_OR_HIRAGANA:
  case USCRIPT_HANGUL:
  case USCRIPT_BOPOMOFO:
  case USCRIPT_YI:
    return true;
  default:
    return false;
  }
}

// Codepoints that never trigger a font switch: they inherit the run's
// typeface (marks, joiners, variation selectors, whitespace, controls).
bool inheritsFont(UChar32 cp) {
  if (cp == 0x200D /*ZWJ*/ || cp == 0x200C /*ZWNJ*/)
    return true;
  if (cp >= 0xFE00 && cp <= 0xFE0F) // variation selectors
    return true;
  if (u_isUWhiteSpace(cp))
    return true;
  const int8_t type = u_charType(cp);
  return type == U_NON_SPACING_MARK || type == U_ENCLOSING_MARK ||
         type == U_COMBINING_SPACING_MARK || type == U_CONTROL_CHAR ||
         type == U_FORMAT_CHAR;
}

ScriptTag hbScriptFor(UScriptCode script) {
  if (script <= USCRIPT_INHERITED || script >= USCRIPT_CODE_LIMIT)
    return static_cast<ScriptTag>(HB_SCRIPT_COMMON); // hb falls back to DFLT
  // hb_script_from_string re-parses a 4-char tag every call; memoize the
  // whole (small, dense) UScriptCode space once.
  static const auto table = [] {
    std::array<ScriptTag, USCRIPT_CODE_LIMIT> t{};
    for (int sc = 0; sc < USCRIPT_CODE_LIMIT; ++sc) {
      const char *name = uscript_getShortName(static_cast<UScriptCode>(sc));
      t[static_cast<size_t>(sc)] = static_cast<ScriptTag>(
          name ? hb_script_from_string(name, -1) : HB_SCRIPT_COMMON);
    }
    return t;
  }();
  return table[static_cast<size_t>(script)];
}

// Codepoints that can force right-to-left directionality: the RTL script
// blocks plus the explicit RTL controls. When a paragraph has none, the
// whole UBiDi pass is skipped.
bool maybeRtl(UChar32 cp) {
  if (cp < 0x0590)
    return false;
  if (cp <= 0x08FF) // Hebrew, Arabic, Syriac, Thaana, NKo, Samaritan, ...
    return true;
  if (cp == 0x200F || cp == 0x202B || cp == 0x202E || cp == 0x2067)
    return true; // RLM / RLE / RLO / RLI
  return (cp >= 0xFB1D && cp <= 0xFDFF) || (cp >= 0xFE70 && cp <= 0xFEFF) ||
         (cp >= 0x10800 && cp <= 0x10FFF) || (cp >= 0x1E800 && cp <= 0x1EFFF);
}

struct ScriptRun {
  uint32_t end = 0;
  UScriptCode script = USCRIPT_COMMON;
};

// Classic script itemization: COMMON/INHERITED characters attach to the
// preceding real script (or the following one at the start of the text).
// Piggybacks RTL detection on the same codepoint walk so the (expensive)
// UBiDi pass can be skipped for the overwhelmingly common LTR-only case.
std::vector<ScriptRun> itemizeScripts(const std::u16string &text,
                                      bool &anyRtl) {
  std::vector<ScriptRun> runs;
  const int32_t len = static_cast<int32_t>(text.size());
  UScriptCode current = USCRIPT_COMMON;
  anyRtl = false;
  int32_t i = 0;
  while (i < len) {
    const int32_t cpStart = i;
    UChar32 cp;
    U16_NEXT(text.data(), i, len, cp);
    anyRtl |= maybeRtl(cp);
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode sc = uscript_getScript(cp, &status);
    if (U_FAILURE(status))
      sc = USCRIPT_COMMON;
    if (sc <= USCRIPT_INHERITED)
      continue; // stays in the current run whatever it is
    if (current <= USCRIPT_INHERITED) {
      current = sc; // leading common text adopts the first real script
    } else if (sc != current) {
      runs.push_back({static_cast<uint32_t>(cpStart), current});
      current = sc;
    }
  }
  runs.push_back({static_cast<uint32_t>(len), current});
  return runs;
}

} // namespace

void Paragraph::clear() {
  m_text.clear();
  m_spans.clear();
  m_words.clear();
  markDirty();
}

void Paragraph::appendText(std::string_view utf8, const TextStyle &style) {
  appendText(std::u16string_view(utf8ToUtf16(utf8)), style);
}

void Paragraph::appendText(std::u16string_view utf16, const TextStyle &style) {
  if (utf16.empty())
    return;
  const uint32_t start = static_cast<uint32_t>(m_text.size());
  m_text.append(utf16);
  m_spans.push_back({start, static_cast<uint32_t>(m_text.size()), style});
  markDirty();
}

void Paragraph::normalizeSpans() {
  if (m_text.empty()) {
    m_spans.clear();
    return;
  }
  std::stable_sort(m_spans.begin(), m_spans.end(),
                   [](const StyleSpan &a, const StyleSpan &b) {
                     return a.start < b.start;
                   });
  // Drop empties, clamp to text, and fill any gaps with the previous span's
  // style so every position resolves to exactly one span.
  std::vector<StyleSpan> fixed;
  const uint32_t len = static_cast<uint32_t>(m_text.size());
  uint32_t cursor = 0;
  for (StyleSpan &span : m_spans) {
    span.start = std::min(span.start, len);
    span.end = std::min(span.end, len);
    if (span.end <= span.start)
      continue;
    if (span.start > cursor) {
      const TextStyle &fillStyle =
          fixed.empty() ? span.style : fixed.back().style;
      fixed.push_back({cursor, span.start, fillStyle});
    }
    if (span.start < cursor)
      span.start = cursor; // overlapping spans: later one yields
    if (span.end <= span.start)
      continue;
    // Merge adjacent equal-styled spans, otherwise repeated restyling
    // fragments the span list without bound (and every span boundary splits
    // a word into separately shaped segments).
    if (!fixed.empty() && fixed.back().end == span.start &&
        fixed.back().style == span.style) {
      fixed.back().end = span.end;
    } else {
      fixed.push_back(span);
    }
    cursor = span.end;
  }
  if (cursor < len) {
    const TextStyle fillStyle = fixed.empty() ? TextStyle{} : fixed.back().style;
    if (!fixed.empty() && fixed.back().end == cursor &&
        fixed.back().style == fillStyle)
      fixed.back().end = len;
    else
      fixed.push_back({cursor, len, fillStyle});
  }
  m_spans = std::move(fixed);
}

void Paragraph::replaceText(uint32_t start, uint32_t end,
                            std::string_view utf8) {
  const uint32_t len = static_cast<uint32_t>(m_text.size());
  start = std::min(start, len);
  end = std::min(std::max(end, start), len);
  const std::u16string ins = utf8ToUtf16(utf8);
  const uint32_t insLen = static_cast<uint32_t>(ins.size());

  // Style the inserted range like the text at the edit point.
  TextStyle insStyle;
  for (const StyleSpan &span : m_spans) {
    if (span.start <= start && (start < span.end || span.end == len)) {
      insStyle = span.style;
      break;
    }
  }
  if (m_spans.empty())
    insStyle = TextStyle{};

  m_text.replace(start, end - start, ins);

  const uint32_t delLen = end - start;
  auto remap = [&](uint32_t x, bool isEnd) -> uint32_t {
    // Delete [start, end) …
    if (x > start)
      x = (x >= end) ? x - delLen : start;
    // … then open a gap of insLen at `start` (positions equal to start stay
    // put as ends, shift as starts — the new span owns the gap).
    if (x > start || (x == start && !isEnd))
      x += insLen;
    return x;
  };
  for (StyleSpan &span : m_spans) {
    span.start = remap(span.start, false);
    span.end = remap(span.end, true);
  }
  if (insLen > 0)
    m_spans.push_back({start, start + insLen, insStyle});
  normalizeSpans();
  markDirty();
}

void Paragraph::setStyle(uint32_t start, uint32_t end, const TextStyle &style) {
  const uint32_t len = static_cast<uint32_t>(m_text.size());
  start = std::min(start, len);
  end = std::min(std::max(end, start), len);
  if (start == end)
    return;

  std::vector<StyleSpan> next;
  next.reserve(m_spans.size() + 2);
  for (const StyleSpan &span : m_spans) {
    if (span.end <= start || span.start >= end) {
      next.push_back(span);
      continue;
    }
    if (span.start < start)
      next.push_back({span.start, start, span.style});
    if (span.end > end)
      next.push_back({end, span.end, span.style});
  }
  next.push_back({start, end, style});
  m_spans = std::move(next);
  normalizeSpans();
  markDirty();
}

void Paragraph::setPaint(uint32_t start, uint32_t end,
                         const PaintStyle &paint) {
  const uint32_t len = static_cast<uint32_t>(m_text.size());
  start = std::min(start, len);
  end = std::min(std::max(end, start), len);
  if (start == end)
    return;

  std::vector<StyleSpan> next;
  next.reserve(m_spans.size() + 2);
  for (const StyleSpan &span : m_spans) {
    if (span.end <= start || span.start >= end) {
      next.push_back(span);
      continue;
    }
    if (span.start < start)
      next.push_back({span.start, start, span.style});
    StyleSpan mid{std::max(span.start, start), std::min(span.end, end),
                  span.style};
    mid.style.paint = paint;
    next.push_back(mid);
    if (span.end > end)
      next.push_back({end, span.end, span.style});
  }
  m_spans = std::move(next);
  normalizeSpans();
  // Shaping keys are untouched: re-analysis is pure cache hits, and a layout
  // that only re-draws (Layout::draw reads span paints live) needs nothing.
  markDirty();
}

Paragraph::Strut Paragraph::strut(FontContext &ctx) const {
  ShapingStyle shaping;
  if (!m_spans.empty())
    shaping = m_spans.front().style.shaping;
  sk_sp<SkTypeface> typeface =
      shaping.typeface ? shaping.typeface : ctx.defaultTypeface();
  const SkFont font = makeFont(typeface, shaping.size);
  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  Strut strut;
  strut.ascent = -metrics.fAscent;
  strut.height = -metrics.fAscent + metrics.fDescent + metrics.fLeading;
  return strut;
}

void Paragraph::ensureShaped(FontContext &ctx) {
  if (!m_dirty)
    return;
  normalizeSpans();
  analyze(ctx);
  m_dirty = false;
}

void Paragraph::analyze(FontContext &ctx) {
  m_words.clear();
  if (m_text.empty())
    return;

  const UChar *text = reinterpret_cast<const UChar *>(m_text.data());
  const int32_t len = static_cast<int32_t>(m_text.size());
  UErrorCode status = U_ZERO_ERROR;

  // ── Line-break opportunities (UAX #14 via ICU) ─────────────────────────
  FontContext::Impl &impl = ctx.impl();
  if (!impl.lineBreaker) {
    impl.lineBreaker = ubrk_open(UBRK_LINE, "", text, len, &status);
  } else {
    ubrk_setText(impl.lineBreaker, text, len, &status);
  }
  if (U_FAILURE(status) || !impl.lineBreaker)
    return;

  std::vector<int32_t> boundaries;
  for (int32_t b = ubrk_next(impl.lineBreaker); b != UBRK_DONE;
       b = ubrk_next(impl.lineBreaker))
    boundaries.push_back(b);
  if (boundaries.empty() || boundaries.back() != len)
    boundaries.push_back(len);

  // ── Script runs (also detects whether bidi is needed at all) ──────────
  bool anyRtl = false;
  const std::vector<ScriptRun> scripts = itemizeScripts(m_text, anyRtl);

  // ── Bidi levels (skipped entirely for LTR-only text) ──────────────────
  UBiDi *bidi = nullptr;
  const UBiDiLevel *levels = nullptr;
  uint8_t uniformLevel = 0; // used when the whole paragraph is one direction
  if (anyRtl) {
    bidi = ubidi_openSized(len, 0, &status);
    if (bidi && U_SUCCESS(status)) {
      ubidi_setPara(bidi, text, len, UBIDI_DEFAULT_LTR, nullptr, &status);
      if (U_SUCCESS(status)) {
        const UBiDiDirection direction = ubidi_getDirection(bidi);
        if (direction == UBIDI_MIXED)
          levels = ubidi_getLevels(bidi, &status);
        else if (direction == UBIDI_RTL)
          uniformLevel = 1;
      }
      if (U_FAILURE(status))
        levels = nullptr;
    }
  }

  // Running resolvers (all inputs are visited in ascending order).
  size_t spanIdx = 0;
  auto styleIndexAt = [&](uint32_t pos) -> uint32_t {
    while (spanIdx + 1 < m_spans.size() && m_spans[spanIdx].end <= pos)
      spanIdx++;
    return static_cast<uint32_t>(spanIdx);
  };
  size_t scriptIdx = 0;
  auto scriptRunAt = [&](uint32_t pos) -> const ScriptRun & {
    while (scriptIdx + 1 < scripts.size() && scripts[scriptIdx].end <= pos)
      scriptIdx++;
    return scripts[scriptIdx];
  };

  // ── Words: one per break segment, split into uniform shaped runs ───────
  m_words.reserve(boundaries.size());
  std::u16string wsScratch;
  uint32_t glueMemoStyle = ~0u;
  std::u16string_view glueMemoText;
  float glueMemoWidth = 0;
  int32_t prev = 0;
  for (int32_t boundary : boundaries) {
    if (boundary <= prev)
      continue;

    // Trailing whitespace (including any hard-break characters at the end).
    int32_t wsStart = boundary;
    bool hardBreak = false;
    while (wsStart > prev) {
      const char16_t c = m_text[wsStart - 1];
      if (isHardBreakChar(c)) {
        hardBreak = true;
        wsStart--;
      } else if (u_isWhitespace(c)) {
        wsStart--;
      } else {
        break;
      }
    }

    Word word;
    word.textBegin = static_cast<uint32_t>(prev);
    word.wsEnd = static_cast<uint32_t>(boundary);
    word.mandatoryBreakAfter = hardBreak;
    word.bidiLevel = levels ? levels[prev] : uniformLevel;

    // A trailing soft hyphen (U+00AD) is a discretionary break: it never
    // shapes, but marks that a hyphen may be rendered if a breaker splits
    // here.
    if (wsStart > prev && m_text[wsStart - 1] == 0x00AD) {
      word.hyphenBreak = true;
      wsStart--;
    }
    word.textEnd = static_cast<uint32_t>(wsStart);

    if (wsStart > prev) {
      UChar32 firstCp;
      int32_t tmp = prev;
      U16_NEXT(text, tmp, len, firstCp);
      UErrorCode scStatus = U_ZERO_ERROR;
      word.ideographic =
          isIdeographicScript(uscript_getScript(firstCp, &scStatus));
    }

    // Split [prev, wsStart) wherever style, script, bidi level, or the
    // fallback-resolved typeface changes; shape each piece via the cache.
    int32_t pos = prev;
    while (pos < wsStart) {
      const uint32_t styleIndex = styleIndexAt(static_cast<uint32_t>(pos));
      const StyleSpan &span = m_spans[styleIndex];
      const ScriptRun &scriptRun = scriptRunAt(static_cast<uint32_t>(pos));
      const uint8_t level = levels ? levels[pos] : uniformLevel;

      int32_t limit = std::min<int32_t>(
          {wsStart, static_cast<int32_t>(span.end),
           static_cast<int32_t>(scriptRun.end)});
      if (levels) {
        int32_t l = pos;
        while (l < limit && levels[l] == level)
          l++;
        limit = l;
      }

      // Extend while the resolved typeface stays put.
      const char *lang = span.style.shaping.language.empty()
                             ? nullptr
                             : span.style.shaping.language.c_str();
      sk_sp<SkTypeface> resolved;
      int32_t segEnd = pos;
      int32_t scan = pos;
      while (scan < limit) {
        const int32_t cpStart = scan;
        UChar32 cp;
        U16_NEXT(text, scan, limit, cp);
        if (inheritsFont(cp)) {
          segEnd = scan;
          continue;
        }
        sk_sp<SkTypeface> t =
            ctx.resolveTypeface(span.style.shaping.typeface, cp, lang);
        if (!resolved) {
          resolved = std::move(t);
        } else if (t.get() != resolved.get()) {
          segEnd = cpStart;
          break;
        }
        segEnd = scan;
      }
      if (!resolved)
        resolved = span.style.shaping.typeface ? span.style.shaping.typeface
                                               : ctx.defaultTypeface();
      if (segEnd <= pos)
        segEnd = limit > pos ? limit : pos + 1; // defensive: always progress

      ShapedWordRef shaped = shapeWord(
          ctx, span.style.shaping, resolved,
          std::u16string_view(m_text).substr(pos, segEnd - pos),
          hbScriptFor(scriptRun.script), (level & 1) != 0);
      word.segments.push_back({shaped, styleIndex, word.width});
      word.width += shaped->advance;
      pos = segEnd;
    }

    if (word.hyphenBreak) {
      // Shape the hyphen the breaker may render here, in the style of the
      // word's tail. Content-addressed like everything else: one entry per
      // style, shared by every hyphenatable word.
      const uint32_t styleIndex = word.segments.empty()
                                      ? styleIndexAt(static_cast<uint32_t>(prev))
                                      : word.segments.back().styleIndex;
      const StyleSpan &span = m_spans[styleIndex];
      sk_sp<SkTypeface> face = span.style.shaping.typeface
                                   ? span.style.shaping.typeface
                                   : ctx.defaultTypeface();
      word.hyphenGlyph =
          shapeWord(ctx, span.style.shaping, face, u"-",
                    static_cast<ScriptTag>(HB_SCRIPT_COMMON), false);
    }

    // Trailing whitespace becomes justification glue. Hard-break characters
    // are zero-width; tabs measure as spaces (documented simplification).
    // The overwhelmingly common glue (" " in the same style as the previous
    // word) is memoized locally, skipping even the shape-cache probe.
    if (wsStart < boundary) {
      wsScratch.clear();
      bool needsScratch = false;
      for (int32_t k = wsStart; k < boundary; ++k) {
        const char16_t c = m_text[k];
        if (isHardBreakChar(c) || c == u'\t') {
          needsScratch = true;
          break;
        }
      }
      std::u16string_view ws;
      if (needsScratch) {
        for (int32_t k = wsStart; k < boundary; ++k) {
          const char16_t c = m_text[k];
          if (isHardBreakChar(c))
            continue;
          wsScratch.push_back(c == u'\t' ? u' ' : c);
        }
        ws = wsScratch;
      } else {
        ws = std::u16string_view(m_text).substr(
            static_cast<size_t>(wsStart),
            static_cast<size_t>(boundary - wsStart));
      }
      if (!ws.empty()) {
        const uint32_t styleIndex = styleIndexAt(static_cast<uint32_t>(wsStart));
        if (styleIndex == glueMemoStyle && ws == glueMemoText) {
          word.spaceWidth = glueMemoWidth;
        } else {
          const StyleSpan &span = m_spans[styleIndex];
          sk_sp<SkTypeface> spaceFace = span.style.shaping.typeface
                                            ? span.style.shaping.typeface
                                            : ctx.defaultTypeface();
          ShapedWordRef shapedWs =
              shapeWord(ctx, span.style.shaping, spaceFace, ws,
                        static_cast<ScriptTag>(HB_SCRIPT_COMMON), false);
          word.spaceWidth = shapedWs->advance;
          if (!needsScratch) { // scratch-backed views don't outlive the loop
            glueMemoStyle = styleIndex;
            glueMemoText = ws;
            glueMemoWidth = word.spaceWidth;
          }
        }
      }
    }

    m_words.push_back(std::move(word));
    prev = boundary;
  }

  if (bidi)
    ubidi_close(bidi);
}

} // namespace textflow
