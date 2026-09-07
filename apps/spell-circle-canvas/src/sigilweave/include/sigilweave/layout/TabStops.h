#pragma once

/** @file
 * @ingroup layout
 *
 * THE STOPS A TAB ADVANCES TO: where the pen goes, what it aligns there,
 * and what fills the gap behind it.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace sigil::weave {

/** One tab stop: where the pen goes, what it aligns there, and what fills
 * the gap behind it.
 *
 * `kStart` puts the text after the stop, `kEnd` ends it there, `kCenter`
 * straddles it, and `kCharacter` lines the FIRST `alignOn` in the following
 * text up on it — which is the decimal column a table of figures wants, and
 * falls back to `kEnd` for a cell that holds no such character.
 *
 * `leader` is set repeatedly across the gap the stop opened, clipped to it,
 * in the style of the text ahead of the tab: a run of dots between a
 * heading and its page number is one string here rather than typed content.
 */
struct TabStop {
  enum class Align : uint8_t { kStart, kCenter, kEnd, kCharacter };
  float position = 0;           ///< px from the interval's start
  Align align = Align::kStart;  ///< what the stop pins there
  char16_t alignOn = u'.';      ///< kCharacter: the character pinned
  std::u16string leader;        ///< repeated across the gap; may be empty
  bool operator==(const TabStop&) const = default;
};

/** Tab-character handling for straight horizontal flows.
 *
 * A word whose trailing whitespace contains a tab advances the pen to the
 * next stop instead of its measured glue: first through `stops` (ascending,
 * px from each line interval's start), then repeating every `interval` px
 * past the last explicit stop. With no stop ahead (or no configuration at
 * all — the default) tabs keep their shaped space-equivalent width.
 *
 * Both breakers resolve stops identically: greedy fits against tab-resolved
 * widths as it goes, and Knuth-Plass scores every candidate line at its
 * tab-resolved width. Stops are line-local — alignment other than kStart
 * shifts the resolved line as a whole. Tab gaps are rigid under
 * justification, and gaps at or before a line's last tab never stretch or
 * shrink (the following stop would swallow the adjustment and unpin the
 * column); only the gaps past the last tab absorb slack.
 * Scope: straight horizontal intervals, LTR lines.
 */
struct TabStopOptions {
  std::vector<TabStop> stops;  ///< explicit stops, ascending
  float interval = 0;          ///< repeat spacing past the last explicit
                               ///< stop; 0 disables repetition
  bool operator==(const TabStopOptions&) const = default;
};

}  // namespace sigil::weave
