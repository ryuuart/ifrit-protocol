#pragma once

/** @file
 * @ingroup unicode
 *
 * The Unicode text analysis a layout engine needs, as plain values over
 * UTF-16 text: transcoding, locale-aware case mapping, script itemization,
 * bidirectional levels, line, word and sentence segmentation, and the
 * per-character properties that decide where a run may break, which glyphs
 * inherit their neighbours' typeface, and how a character stands in a
 * vertical column. Every function takes text or a code point and returns
 * its answer; nothing here holds state a caller can see, and nothing here
 * knows about fonts, styles, or a canvas.
 *
 * Positions are UTF-16 code-unit offsets, as everywhere else in the
 * library. Scratch objects the analyses reuse (break iterators, the bidi
 * analyzer) are thread-local, so every function is safe to call from any
 * thread with no shared state.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::weave::unicode {

// ── Transcoding ────────────────────────────────────────────────────────

/** Converts UTF-8 to UTF-16. Malformed input yields an empty string. */
[[nodiscard]] std::u16string toUtf16(std::u8string_view utf8);

/** Converts UTF-16 to UTF-8. An unpaired surrogate becomes U+FFFD. */
[[nodiscard]] std::u8string toUtf8(std::u16string_view utf16);

/** Decodes the code point starting at `offset` and advances `offset` past
 * it. An unpaired surrogate decodes as its own code unit value. `offset`
 * must be below `text.size()`.
 */
[[nodiscard]] char32_t decodeAt(std::u16string_view text, size_t& offset);

// ── Character properties ───────────────────────────────────────────────

/** Whether a code point is whitespace in the sense that separates words:
 * the space separators other than the no-break spaces (U+00A0, U+2007,
 * U+202F), plus the control characters that separate lines, fields and
 * paragraphs (TAB through CR, U+001C through U+001F, NEL, LINE SEPARATOR,
 * PARAGRAPH SEPARATOR).
 */
[[nodiscard]] bool isWhitespace(char32_t codePoint);

/** Whether one UTF-16 unit forces a line break after itself — the four
 * line-break classes that break unconditionally: the mandatory-break
 * characters (VT, FF, LINE SEPARATOR, PARAGRAPH SEPARATOR), CR, LF and
 * NEL. It is a question about the CHARACTER; whether a given BOUNDARY is
 * mandatory is LineBreak::mandatory, which is the segmentation's answer
 * and covers the sequences (CR LF) a character cannot.
 */
[[nodiscard]] bool isHardLineBreak(char16_t unit);

/** Whether a code point never triggers a typeface switch and instead takes
 * the typeface of the run it sits in: joiners, variation selectors,
 * whitespace, controls and format characters, and every combining mark.
 */
[[nodiscard]] bool inheritsTypeface(char32_t codePoint);

/** Whether a code point can force right-to-left directionality: its
 * bidirectional class is one of the right-to-left ones, the Arabic
 * letters included. Text with no such code point resolves to one uniform
 * left-to-right level without a full bidirectional pass. It is a property
 * lookup rather than a range test, so a script Unicode adds tomorrow
 * cannot be missed by it.
 */
[[nodiscard]] bool mayRequireBidi(char32_t codePoint);

/** Whether a code point is set in a FULL-WIDTH CELL — East Asian Width
 * Wide or Fullwidth. That is the property behind every question this
 * engine asks about "ideographic" text: a full-width character has no
 * spaces around it and the zero-width gap beside it is what a justified
 * CJK line spends its slack on. It is a character property and not a
 * script one, so fullwidth Latin (Ａ Ｂ Ｃ) set among kanji answers the
 * same way the kanji do.
 */
[[nodiscard]] bool isFullWidth(char32_t codePoint);

/// How a character stands in a vertical column (UTR#50 Vertical_Orientation):
/// upright as in CJK, rotated a quarter turn as in Latin, or upright with a
/// substitute glyph the font supplies for the vertical form.
enum class VerticalOrientation : uint8_t {
  kUpright,             ///< U: stands upright as is
  kRotated,             ///< R: rotates a quarter turn clockwise
  kTransformedUpright,  ///< Tu: upright, in the font's vertical variant
  kTransformedRotated,  ///< Tr: rotated unless the font has a vertical variant
};

/** Returns a code point's UTR#50 vertical orientation. */
[[nodiscard]] VerticalOrientation verticalOrientation(char32_t codePoint);

// ── Scripts ────────────────────────────────────────────────────────────

/// A script code. The values are ICU's UScriptCode enumerators, carried as
/// an integer so no ICU header is needed to hold one; kCommonScript and
/// kInheritedScript are the two codes that name no script of their own.
using Script = int32_t;
inline constexpr Script kCommonScript = 0;     ///< Zyyy: no specific script
inline constexpr Script kInheritedScript = 1;  ///< Zinh: takes its neighbours'

/** Returns a code point's script. Unassigned code points are kCommonScript.
 */
[[nodiscard]] Script scriptOf(char32_t codePoint);

/** Whether a script code names a specific script, as opposed to Common or
 * Inherited.
 */
[[nodiscard]] constexpr bool isSpecificScript(Script script) noexcept {
  return script > kInheritedScript;
}

/** One past the largest script code scriptOf() can return. */
[[nodiscard]] Script scriptLimit() noexcept;

/** Returns a script's four-letter ISO 15924 code ("Latn", "Hani", …), or
 * nullptr for a code outside [0, scriptLimit()).
 */
[[nodiscard]] const char* scriptShortName(Script script) noexcept;

/// A maximal run of text in one script; `end` is exclusive, and the run
/// starts where the previous one ended (0 for the first).
struct ScriptRun {
  uint32_t end = 0;               ///< exclusive end, UTF-16 units
  Script script = kCommonScript;  ///< the run's script
};

/** Splits text into script runs. Common and Inherited characters attach to
 * the preceding specific script, or to the following one when they open
 * the text. Text with no specific script at all is one kCommonScript run.
 * Empty text yields one empty run ending at 0.
 */
[[nodiscard]] std::vector<ScriptRun> itemize(std::u16string_view text);

/** itemize() into caller-owned storage, which is cleared first, so a caller
 * re-itemizing every frame amortizes its allocation away.
 */
void itemize(std::u16string_view text, std::vector<ScriptRun>& runs);

// ── Case mapping ───────────────────────────────────────────────────────

/// A case mapping. Full Unicode mappings, so the result may be longer than
/// the input ("ß" uppercases to "SS").
enum class Case : uint8_t {
  kUpper,       ///< every character to upper case
  kLower,       ///< every character to lower case
  kCapitalize,  ///< the first code point to title case; the rest untouched
};

/** Case-maps text into `out`, replacing its contents; `locale` is a BCP 47
 * tag ("tr" makes "i" uppercase to "İ") and empty means the process
 * default locale. Returns false, leaving `out` unspecified, when ICU
 * refuses the mapping.
 */
bool caseMap(std::u16string_view text, Case mapping, std::string_view locale,
             std::u16string& out);

/** Returns the case-mapped text, or the text unchanged when caseMap()
 * fails.
 */
[[nodiscard]] std::u16string caseMapped(std::u16string_view text, Case mapping,
                                        std::string_view locale = {});

// ── Segmentation ───────────────────────────────────────────────────────

/// ONE LINE-BREAK OPPORTUNITY: where a line may end, and whether it MUST.
/// The offset is one past the unit the break follows. `mandatory` is the
/// segmentation's own answer — the rule that opened the boundary was one
/// of UAX#14's unconditional ones — so a CR LF pair, which no per-character
/// test can judge, is one mandatory break and not two.
struct LineBreak {
  uint32_t offset = 0;
  bool mandatory = false;
  bool operator==(const LineBreak&) const = default;
};

/** Returns every line-break opportunity in the text (UAX#14), ascending:
 * the offsets a line may end at, each one past the unit it follows, always
 * ending with `text.size()` and never containing 0. Empty text yields one
 * opportunity at 0. A break after a soft hyphen (U+00AD) is reported like
 * any other.
 *
 * `locale` selects the tailoring the segmentation runs under: a BCP 47 tag,
 * optionally carrying ICU's line-break keyword ("ja@lb=strict" sets the
 * strict Japanese rules a printed page uses, "zh@lb=loose" the loose ones),
 * and empty is the untailored root behaviour every text gets by default.
 * Iterators are cached per locale, so alternating between two costs no
 * more than staying in one.
 */
[[nodiscard]] std::vector<LineBreak> lineBreaks(std::u16string_view text,
                                                std::string_view locale = {});

/** lineBreaks() into caller-owned storage, which is cleared first. */
void lineBreaks(std::u16string_view text, std::vector<LineBreak>& breaks,
                std::string_view locale = {});

/** Returns every GRAPHEME CLUSTER boundary in the text (UAX#29), ascending,
 * starting with 0 and ending with `text.size()`; the clusters are the
 * ranges between consecutive entries. A cluster is what a reader calls one
 * character, so it is the unit anything cutting text apart must land on:
 * a combining mark, a Hangul syllable, a regional-indicator pair and an
 * emoji ZWJ sequence are each indivisible here. Empty text yields {0}.
 */
[[nodiscard]] std::vector<uint32_t> graphemeBoundaries(
    std::u16string_view text);

/** graphemeBoundaries() into caller-owned storage, cleared first. */
void graphemeBoundaries(std::u16string_view text,
                        std::vector<uint32_t>& boundaries);

/** Returns every word boundary in the text (UAX#29), ascending, starting
 * with 0 and ending with `text.size()`; the words are the ranges between
 * consecutive entries, punctuation and spaces included as words of their
 * own. Empty text yields {0}.
 */
[[nodiscard]] std::vector<uint32_t> wordBoundaries(std::u16string_view text);

/** Returns the offset where each sentence starts (UAX#29), ascending, the
 * first entry always 0; the sentence containing an offset is the last entry
 * not greater than it. Empty text yields no entries.
 */
[[nodiscard]] std::vector<uint32_t> sentenceStarts(std::u16string_view text);

// ── Bidirectional analysis ─────────────────────────────────────────────

/// The paragraph direction a bidirectional analysis resolves against.
enum class BaseDirection : uint8_t {
  kLeftToRight,      ///< level 0, whatever the text
  kRightToLeft,      ///< level 1, whatever the text
  kAutoLeftToRight,  ///< the first strong character decides; LTR if none
  kAutoRightToLeft,  ///< the first strong character decides; RTL if none
};

/// A maximal run of text at one embedding level. Odd levels read right to
/// left.
struct BidiRun {
  uint32_t start = 0;  ///< inclusive start, UTF-16 units
  uint32_t end = 0;    ///< exclusive end, UTF-16 units
  uint8_t level = 0;   ///< UAX#9 embedding level
};

/** Resolves the text's embedding levels (UAX#9) into runs that cover it
 * from 0 to `text.size()` with no gaps. Text whose direction is uniform is
 * one run at level 0 or 1. Empty text yields no runs. A failed analysis
 * yields one run at level 0.
 */
[[nodiscard]] std::vector<BidiRun> bidi(
    std::u16string_view text,
    BaseDirection base = BaseDirection::kAutoLeftToRight);

}  // namespace sigil::weave::unicode
