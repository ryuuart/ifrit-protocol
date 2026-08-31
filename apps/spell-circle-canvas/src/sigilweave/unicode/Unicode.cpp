/** @file
 * Every Unicode question the engine asks, answered through ICU: transcoding,
 * character properties, script itemization, case mapping, the line, word
 * and sentence segmenters, and bidirectional levels, over thread-local
 * scratch objects reused across calls.
 */

#include "sigilweave/unicode/Unicode.h"

#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>

#include <memory>

namespace sigil::weave::unicode {

namespace {

// One iterator per thread and per rule set, re-targeted per call: opening
// a break iterator loads its rule data and costs far more than running
// one. It borrows the text only for the duration of one call.
struct BreakIteratorCloser {
  void operator()(UBreakIterator* iterator) const { ubrk_close(iterator); }
};
using BreakIteratorPtr = std::unique_ptr<UBreakIterator, BreakIteratorCloser>;

// Points `iterator` at `text`, opening it (root locale) on first use.
// Returns nullptr when ICU refuses.
UBreakIterator* targetIterator(BreakIteratorPtr& iterator,
                               UBreakIteratorType type,
                               std::u16string_view text) {
  const UChar* units = reinterpret_cast<const UChar*>(text.data());
  const int32_t length = static_cast<int32_t>(text.size());
  UErrorCode status = U_ZERO_ERROR;
  if (!iterator)
    iterator.reset(ubrk_open(type, "", units, length, &status));
  else
    ubrk_setText(iterator.get(), units, length, &status);
  if (U_FAILURE(status)) return nullptr;
  return iterator.get();
}

// Every boundary the iterator reports after 0, ascending.
void collectBoundaries(UBreakIterator* iterator,
                       std::vector<uint32_t>& boundaries) {
  for (int32_t boundary = ubrk_next(iterator); boundary != UBRK_DONE;
       boundary = ubrk_next(iterator))
    boundaries.push_back(static_cast<uint32_t>(boundary));
}

// Case maps `source` into `out` at `outOffset` through one of ICU's
// u_strTo* functions, growing `out` when the first attempt overflows.
template <typename MapFunction>
bool mapInto(std::u16string_view source, MapFunction&& mapFunction,
             const char* locale, size_t outOffset, std::u16string& out) {
  UErrorCode status = U_ZERO_ERROR;
  out.resize(outOffset + source.size() + 8);
  int32_t written =
      mapFunction(reinterpret_cast<UChar*>(out.data() + outOffset),
                  static_cast<int32_t>(out.size() - outOffset),
                  reinterpret_cast<const UChar*>(source.data()),
                  static_cast<int32_t>(source.size()), locale, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    out.resize(outOffset + static_cast<size_t>(written));
    written = mapFunction(reinterpret_cast<UChar*>(out.data() + outOffset),
                          static_cast<int32_t>(out.size() - outOffset),
                          reinterpret_cast<const UChar*>(source.data()),
                          static_cast<int32_t>(source.size()), locale, &status);
  }
  if (U_FAILURE(status)) return false;
  out.resize(outOffset + static_cast<size_t>(written));
  return true;
}

}  // namespace

// ── Transcoding ────────────────────────────────────────────────────────

std::u16string toUtf16(std::u8string_view utf8) {
  if (utf8.empty()) return {};
  std::u16string utf16;
  utf16.resize(utf8.size());  // UTF-16 never has more units than UTF-8 bytes
  UErrorCode status = U_ZERO_ERROR;
  int32_t codeUnitsWritten = 0;
  u_strFromUTF8(reinterpret_cast<UChar*>(utf16.data()),
                static_cast<int32_t>(utf16.size()), &codeUnitsWritten,
                reinterpret_cast<const char*>(utf8.data()),
                static_cast<int32_t>(utf8.size()), &status);
  if (U_FAILURE(status)) return {};
  utf16.resize(static_cast<size_t>(codeUnitsWritten));
  return utf16;
}

std::u8string toUtf8(std::u16string_view utf16) {
  if (utf16.empty()) return {};
  std::u8string utf8;
  utf8.resize(utf16.size() * 3);  // a UTF-16 unit expands to at most 3 bytes
  UErrorCode status = U_ZERO_ERROR;
  int32_t bytesWritten = 0;
  u_strToUTF8WithSub(
      reinterpret_cast<char*>(utf8.data()), static_cast<int32_t>(utf8.size()),
      &bytesWritten, reinterpret_cast<const UChar*>(utf16.data()),
      static_cast<int32_t>(utf16.size()), 0xFFFD, nullptr, &status);
  if (U_FAILURE(status)) return {};
  utf8.resize(static_cast<size_t>(bytesWritten));
  return utf8;
}

char32_t decodeAt(std::u16string_view text, size_t& offset) {
  int32_t cursor = static_cast<int32_t>(offset);
  UChar32 codePoint;
  U16_NEXT(text.data(), cursor, static_cast<int32_t>(text.size()), codePoint);
  offset = static_cast<size_t>(cursor);
  return static_cast<char32_t>(codePoint);
}

// ── Character properties ───────────────────────────────────────────────

bool isWhitespace(char32_t codePoint) {
  return u_isWhitespace(static_cast<UChar32>(codePoint));
}

bool isHardLineBreak(char16_t unit) {
  return unit == u'\n' || unit == u'\r' || unit == 0x0085 || unit == 0x2028 ||
         unit == 0x2029;
}

bool inheritsTypeface(char32_t codePoint) {
  if (codePoint == 0x200D /*ZWJ*/ || codePoint == 0x200C /*ZWNJ*/) return true;
  if (codePoint >= 0xFE00 && codePoint <= 0xFE0F)  // variation selectors
    return true;
  if (u_isUWhiteSpace(static_cast<UChar32>(codePoint))) return true;
  const int8_t type = u_charType(static_cast<UChar32>(codePoint));
  return type == U_NON_SPACING_MARK || type == U_ENCLOSING_MARK ||
         type == U_COMBINING_SPACING_MARK || type == U_CONTROL_CHAR ||
         type == U_FORMAT_CHAR;
}

bool mayRequireBidi(char32_t codePoint) {
  if (codePoint < 0x0590) return false;
  if (codePoint <= 0x08FF)  // Hebrew, Arabic, Syriac, Thaana, NKo, ...
    return true;
  if (codePoint == 0x200F || codePoint == 0x202B || codePoint == 0x202E ||
      codePoint == 0x2067)
    return true;  // RLM / RLE / RLO / RLI
  return (codePoint >= 0xFB1D && codePoint <= 0xFDFF) ||
         (codePoint >= 0xFE70 && codePoint <= 0xFEFF) ||
         (codePoint >= 0x10800 && codePoint <= 0x10FFF) ||
         (codePoint >= 0x1E800 && codePoint <= 0x1EFFF);
}

VerticalOrientation verticalOrientation(char32_t codePoint) {
  switch (u_getIntPropertyValue(static_cast<UChar32>(codePoint),
                                UCHAR_VERTICAL_ORIENTATION)) {
    case U_VO_ROTATED:
      return VerticalOrientation::kRotated;
    case U_VO_TRANSFORMED_UPRIGHT:
      return VerticalOrientation::kTransformedUpright;
    case U_VO_TRANSFORMED_ROTATED:
      return VerticalOrientation::kTransformedRotated;
    default:
      return VerticalOrientation::kUpright;
  }
}

// ── Scripts ────────────────────────────────────────────────────────────

Script scriptOf(char32_t codePoint) {
  UErrorCode status = U_ZERO_ERROR;
  const UScriptCode script =
      uscript_getScript(static_cast<UChar32>(codePoint), &status);
  return U_FAILURE(status) ? kCommonScript : static_cast<Script>(script);
}

Script scriptLimit() noexcept {
  return static_cast<Script>(USCRIPT_CODE_LIMIT);
}

const char* scriptShortName(Script script) noexcept {
  if (script < 0 || script >= scriptLimit()) return nullptr;
  return uscript_getShortName(static_cast<UScriptCode>(script));
}

bool isIdeographicScript(Script script) noexcept {
  switch (static_cast<UScriptCode>(script)) {
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

std::vector<ScriptRun> itemize(std::u16string_view text) {
  std::vector<ScriptRun> runs;
  itemize(text, runs);
  return runs;
}

void itemize(std::u16string_view text, std::vector<ScriptRun>& runs) {
  runs.clear();
  const int32_t textLength = static_cast<int32_t>(text.size());
  Script currentScript = kCommonScript;
  int32_t codeUnitOffset = 0;
  while (codeUnitOffset < textLength) {
    const int32_t codePointStart = codeUnitOffset;
    UChar32 codePoint;
    U16_NEXT(text.data(), codeUnitOffset, textLength, codePoint);
    const Script script = scriptOf(static_cast<char32_t>(codePoint));
    if (!isSpecificScript(script))
      continue;  // stays in the current run whatever it is
    if (!isSpecificScript(currentScript)) {
      currentScript = script;  // leading common adopts the first real script
    } else if (script != currentScript) {
      runs.push_back({static_cast<uint32_t>(codePointStart), currentScript});
      currentScript = script;
    }
  }
  runs.push_back({static_cast<uint32_t>(textLength), currentScript});
}

// ── Case mapping ───────────────────────────────────────────────────────

bool caseMap(std::u16string_view text, Case mapping, std::string_view locale,
             std::u16string& out) {
  // ICU wants a C string; an empty tag selects the process default locale.
  static thread_local std::string localeScratch;
  const char* localeName = nullptr;
  if (!locale.empty()) {
    localeScratch.assign(locale);
    localeName = localeScratch.c_str();
  }
  switch (mapping) {
    case Case::kUpper:
      return mapInto(text, u_strToUpper, localeName, 0, out);
    case Case::kLower:
      return mapInto(text, u_strToLower, localeName, 0, out);
    case Case::kCapitalize: {
      if (text.empty()) {
        out.clear();
        return true;
      }
      // Titlecase exactly the first code point; the remainder is untouched
      // (u_strToTitle over the whole text would lowercase it).
      int32_t firstEnd = 0;
      UChar32 firstCodePoint;
      U16_NEXT(text.data(), firstEnd, static_cast<int32_t>(text.size()),
               firstCodePoint);
      static_cast<void>(firstCodePoint);
      auto titleFirst = [](UChar* dest, int32_t destCapacity, const UChar* src,
                           int32_t srcLength, const char* mapLocale,
                           UErrorCode* status) {
        return u_strToTitle(dest, destCapacity, src, srcLength,
                            /*titleIter=*/nullptr, mapLocale, status);
      };
      if (!mapInto(text.substr(0, static_cast<size_t>(firstEnd)), titleFirst,
                   localeName, 0, out))
        return false;
      out.append(text.substr(static_cast<size_t>(firstEnd)));
      return true;
    }
  }
  return false;
}

std::u16string caseMapped(std::u16string_view text, Case mapping,
                          std::string_view locale) {
  std::u16string out;
  if (!caseMap(text, mapping, locale, out)) out.assign(text);
  return out;
}

// ── Segmentation ───────────────────────────────────────────────────────

std::vector<uint32_t> lineBreaks(std::u16string_view text) {
  std::vector<uint32_t> breaks;
  lineBreaks(text, breaks);
  return breaks;
}

void lineBreaks(std::u16string_view text, std::vector<uint32_t>& breaks) {
  breaks.clear();
  static thread_local BreakIteratorPtr lineIterator;
  if (UBreakIterator* iterator = targetIterator(lineIterator, UBRK_LINE, text))
    collectBoundaries(iterator, breaks);
  const uint32_t textLength = static_cast<uint32_t>(text.size());
  if (breaks.empty() || breaks.back() != textLength)
    breaks.push_back(textLength);
}

std::vector<uint32_t> wordBoundaries(std::u16string_view text) {
  std::vector<uint32_t> boundaries;
  boundaries.push_back(0);
  static thread_local BreakIteratorPtr wordIterator;
  if (UBreakIterator* iterator = targetIterator(wordIterator, UBRK_WORD, text))
    collectBoundaries(iterator, boundaries);
  const uint32_t textLength = static_cast<uint32_t>(text.size());
  if (boundaries.back() != textLength) boundaries.push_back(textLength);
  return boundaries;
}

std::vector<uint32_t> sentenceStarts(std::u16string_view text) {
  std::vector<uint32_t> starts;
  if (text.empty()) return starts;
  static thread_local BreakIteratorPtr sentenceIterator;
  UBreakIterator* iterator =
      targetIterator(sentenceIterator, UBRK_SENTENCE, text);
  if (!iterator) return starts;
  const int32_t textLength = static_cast<int32_t>(text.size());
  starts.push_back(0);
  for (int32_t boundary = ubrk_next(iterator); boundary != UBRK_DONE;
       boundary = ubrk_next(iterator))
    if (boundary > 0 && boundary < textLength)
      starts.push_back(static_cast<uint32_t>(boundary));
  return starts;
}

// ── Bidirectional analysis ─────────────────────────────────────────────

std::vector<BidiRun> bidi(std::u16string_view text, BaseDirection base) {
  std::vector<BidiRun> runs;
  if (text.empty()) return runs;
  const uint32_t textLength = static_cast<uint32_t>(text.size());

  UBiDiLevel paragraphLevel = UBIDI_DEFAULT_LTR;
  switch (base) {
    case BaseDirection::kLeftToRight:
      paragraphLevel = 0;
      break;
    case BaseDirection::kRightToLeft:
      paragraphLevel = 1;
      break;
    case BaseDirection::kAutoLeftToRight:
      paragraphLevel = UBIDI_DEFAULT_LTR;
      break;
    case BaseDirection::kAutoRightToLeft:
      paragraphLevel = UBIDI_DEFAULT_RTL;
      break;
  }

  // A left-to-right base over text with no right-to-left character resolves
  // to one level-0 run, so the full pass runs only for text that can need it.
  if (base == BaseDirection::kLeftToRight ||
      base == BaseDirection::kAutoLeftToRight) {
    bool needsAnalysis = false;
    size_t offset = 0;
    while (offset < text.size() && !needsAnalysis)
      needsAnalysis = mayRequireBidi(decodeAt(text, offset));
    if (!needsAnalysis) {
      runs.push_back({0, textLength, 0});
      return runs;
    }
  }

  // One analyzer per thread, re-targeted per call.
  struct BidiCloser {
    void operator()(UBiDi* analyzer) const { ubidi_close(analyzer); }
  };
  static thread_local std::unique_ptr<UBiDi, BidiCloser> analyzer;
  if (!analyzer) analyzer.reset(ubidi_open());
  if (!analyzer) {
    runs.push_back({0, textLength, 0});
    return runs;
  }

  UErrorCode status = U_ZERO_ERROR;
  ubidi_setPara(analyzer.get(), reinterpret_cast<const UChar*>(text.data()),
                static_cast<int32_t>(text.size()), paragraphLevel, nullptr,
                &status);
  const UBiDiDirection direction =
      U_SUCCESS(status) ? ubidi_getDirection(analyzer.get()) : UBIDI_LTR;
  if (direction != UBIDI_MIXED) {
    runs.push_back(
        {0, textLength, static_cast<uint8_t>(direction == UBIDI_RTL ? 1 : 0)});
    return runs;
  }
  const UBiDiLevel* levels = ubidi_getLevels(analyzer.get(), &status);
  if (U_FAILURE(status) || !levels) {
    runs.push_back({0, textLength, 0});
    return runs;
  }
  uint32_t runStart = 0;
  for (uint32_t unit = 1; unit < textLength; ++unit) {
    if (levels[unit] == levels[runStart]) continue;
    runs.push_back({runStart, unit, static_cast<uint8_t>(levels[runStart])});
    runStart = unit;
  }
  runs.push_back(
      {runStart, textLength, static_cast<uint8_t>(levels[runStart])});
  return runs;
}

}  // namespace sigil::weave::unicode
