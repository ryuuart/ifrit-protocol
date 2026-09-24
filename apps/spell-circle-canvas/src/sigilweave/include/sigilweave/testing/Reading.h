#pragma once

/** @file
 * @ingroup weave-testing
 *
 * A laid passage read back as plain values: its lines with their boxes,
 * the fit each was set at and how bad that fit is, its runs, every glyph
 * where it rests, and the places a word could break at a hyphen beside
 * the ones a line took. Every value compares with `==`, so a case states
 * what it expects as a value rather than walking the layout itself.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkTypes.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "sigilweave/layout/PositionedRun.h"
#include "sigilweave/testing/Passage.h"

namespace sigil::weave::testing {

/** One glyph where the layout left it: the face's glyph, its absolute
 *  rest position with the line's fit applied, and where it sits in the
 *  text. */
struct GlyphPlacement {
  SkGlyphID glyph = 0;
  SkPoint rest = {0, 0};      ///< absolute origin, fit applied
  float advance = 0;          ///< this glyph's pen travel
  uint32_t textIndex = 0;     ///< its cluster as an offset into the text
  uint32_t wordIndex = 0;     ///< into Paragraph::words()
  int lineIndex = 0;          ///< the line it landed on
  bool transformed = false;   ///< turned onto a contour or a rotated line
  SkVector tangent = {1, 0};  ///< the direction it was turned to
  bool operator==(const GlyphPlacement&) const = default;
};

/** One draw of the layout: which word, where, how far it advanced and
 *  what the line's fit did to its glyphs. */
struct RunReading {
  uint32_t wordIndex = 0;
  int lineIndex = 0;
  SkPoint origin = {0, 0};
  float advance = 0;          ///< the advance it took where it landed
  uint32_t glyphCount = 0;    ///< none for a placeholder
  bool transformed = false;   ///< drawn from a positioned blob
  bool hyphen = false;        ///< the hyphen a broken word draws
  int placeholderIndex = -1;  ///< \>= 0 for an inline object's slot
  GlyphFit fit;
  bool operator==(const RunReading&) const = default;
};

/** ONE LINE AS IT WAS SET. `natural` is what the line's words and gaps
 *  measure before any fit: content, each interior gap at the word spacing
 *  the justification aims at, and a hyphen the line broke at. The
 *  adjustment ratio is the slack against the measure over what the gaps
 *  may open (or, overfull, close), from the justification's own limits;
 *  the badness is TeX's measure of it, 100·|ratio|³ capped at 10000, with
 *  a line that is overfull past its shrink or has nothing to stretch at
 *  the cap. A block's last line that fits scores zero, its slack going to
 *  the end of the paragraph as TeX's parfillskip takes it.
 *  @silent the line turned a run, ran down a column, or held a tab or an
 *  ideographic gap, or the options set tab stops, a mojikumi table, tsume,
 *  hanging punctuation or block styles of their own: its gaps follow
 *  rules this reading does not restate, so the ratio and the badness are
 *  absent. The rest of the line is still read. */
struct LineReading {
  int lineIndex = 0;
  /// The band the line's runs occupy, ascent to descent — or the column's
  /// band down a vertical one; empty for a line of turned runs.
  SkRect box = SkRect::MakeEmpty();
  uint32_t textBegin = 0;  ///< first UTF-16 unit on the line
  uint32_t textEnd = 0;    ///< one past the last, trailing glue included
  float measure = 0;       ///< the length of the interval it was set in
  float natural = 0;       ///< what its words and gaps measure unfitted
  float extent = 0;        ///< first pen to last advance, as set
  /// The gap between each two words in pen order, as set: what the fit
  /// spent in the word spaces. Empty for a line of one word and for
  /// turned runs.
  std::vector<float> gaps;
  GlyphFit fit;               ///< what the fit did to the glyphs
  bool endsInHyphen = false;  ///< the line broke inside a word
  std::optional<float> adjustmentRatio;
  std::optional<float> badness;
  bool operator==(const LineReading&) const = default;
};

/** THE WHOLE PASSAGE AS VALUES. */
struct Reading {
  std::vector<LineReading> lines;      ///< ascending by line index
  std::vector<RunReading> runs;        ///< in the layout's draw order
  std::vector<GlyphPlacement> glyphs;  ///< every placed glyph, draw order
  /// Where a word could break at a hyphen, as the offset the text resumes
  /// at after the break — a soft hyphen the author typed or a pattern
  /// table's point alike — ascending.
  std::vector<uint32_t> hyphenationPoints;
  /// The subset of those a line actually broke at.
  std::vector<uint32_t> hyphensTaken;
  int lineCount = 0;
  bool overflowed = false;  ///< the geometry ran out before the text did
  bool ellipsized = false;  ///< an overflow marker ended the last line
  bool operator==(const Reading&) const = default;
};

/** Reads @p passage back as values. Derived, not stored: reading twice
 *  answers the same value. */
[[nodiscard]] Reading read(const Passage& passage);

}  // namespace sigil::weave::testing
