#pragma once

/** @file
 * @ingroup weave-testing
 *
 * A laid passage read back as plain values: its lines with their boxes,
 * the fit each was set at and the score the breaker chose it at, its runs,
 * every glyph
 * where it rests, and the places a word could break at a hyphen beside
 * the ones a line took. Every value compares with `==`, so a case states
 * what it expects as a value rather than walking the layout itself.
 */


#include <cstdint>
#include <vector>

#include <glm/vec2.hpp>

#include "sigilgeometry/path/Outline.h"
#include "sigilweave/layout/ParagraphLayout.h"
#include "sigilweave/layout/PositionedRun.h"
#include "sigilweave/testing/Passage.h"

namespace sigil::weave::testing {

/** One glyph where the layout left it: the face's glyph, its absolute
 *  rest position with the line's fit applied, and where it sits in the
 *  text. */
struct GlyphPlacement {
  uint16_t glyph = 0;
  glm::vec2 rest{0, 0};       ///< absolute origin, fit applied
  float advance = 0;          ///< this glyph's pen travel
  uint32_t textIndex = 0;     ///< its cluster as an offset into the text
  uint32_t wordIndex = 0;     ///< into Paragraph::words()
  int lineIndex = 0;          ///< the line it landed on
  bool transformed = false;   ///< turned onto a contour or a rotated line
  glm::vec2 tangent{1, 0};    ///< the direction it was turned to
  bool operator==(const GlyphPlacement&) const = default;
};

/** One draw of the layout: which word, where, how far it advanced and
 *  what the line's fit did to its glyphs. */
struct RunReading {
  uint32_t wordIndex = 0;
  int lineIndex = 0;
  glm::vec2 origin{0, 0};
  float advance = 0;          ///< the advance it took where it landed
  uint32_t glyphCount = 0;    ///< none for a placeholder
  bool transformed = false;   ///< drawn from a positioned blob
  bool hyphen = false;        ///< the hyphen a broken word draws
  int placeholderIndex = -1;  ///< \>= 0 for an inline object's slot
  GlyphFit fit;
  bool operator==(const RunReading&) const = default;
};

/** ONE LINE AS IT WAS SET: where it landed and what the fit spent, read
 *  off its runs, beside the score the optimizing breaker chose it at, read
 *  from the layout rather than worked out again — what a LineScore holds
 *  and the traps it names are the breaker's.
 *  @silent the greedy breaker set the line: `scores` is empty, because no
 *  break was weighed. The rest of the line is still read. */
struct LineReading {
  int lineIndex = 0;
  /// The band the line's runs occupy, ascent to descent — or the column's
  /// band down a vertical one; empty for a line of turned runs.
  geometry::path::Rect box;
  uint32_t textBegin = 0;  ///< first UTF-16 unit on the line
  uint32_t textEnd = 0;    ///< one past the last, trailing glue included
  float measure = 0;       ///< the length of the interval it was set in
  float extent = 0;        ///< first pen to last advance, as set
  /// The gap between each two words in pen order, as set: what the fit
  /// spent in the word spaces. Empty for a line of one word and for
  /// turned runs.
  std::vector<float> gaps;
  GlyphFit fit;               ///< what the fit did to the glyphs
  bool endsInHyphen = false;  ///< the line broke inside a word
  /// How the optimizing breaker scored the line, one entry per interval
  /// it was set in — one, unless an exclusion split the line — in the
  /// order they were placed.
  std::vector<LineScore> scores;
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
