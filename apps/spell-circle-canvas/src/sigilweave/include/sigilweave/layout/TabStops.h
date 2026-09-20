#pragma once

/** @file
 * @ingroup weave-layout
 *
 * THE STOPS A TAB ADVANCES TO: where the pen goes, what it aligns there,
 * and what fills the gap behind it.
 */

#include <cstdint>
#include <string>
#include <vector>

namespace sigil::weave {

/** One tab stop: where the pen goes, what it aligns there, and what
 * fills the gap behind it. `kCharacter` lines the FIRST `alignOn` in the
 * following text up on the stop — the decimal column a table of figures
 * wants — and falls back to `kEnd` for a cell holding no such character.
 * `leader` is set repeatedly across the gap, clipped to it, in the style
 * of the text ahead of the tab.
 */
struct TabStop {
  /** What the stop pins at its position. */
  enum class Align : uint8_t {
    kStart,     ///< the text after the tab begins on the stop
    kCenter,    ///< that text straddles the stop
    kEnd,       ///< that text ends on the stop
    kCharacter  ///< the first `alignOn` in it lands on the stop
  };
  float position = 0;           ///< px from the interval's start
  Align align = Align::kStart;  ///< what the stop pins there
  char16_t alignOn = u'.';      ///< kCharacter: the character pinned
  std::u16string leader;        ///< repeated across the gap; may be empty
  bool operator==(const TabStop&) const = default;
};

/** Tab-character handling for straight horizontal left-to-right flows. A
 * word whose trailing whitespace contains a tab advances the pen to the
 * next stop instead of its measured glue: through `stops` first, then
 * repeating every `interval` px past the last explicit one. Tab gaps are
 * rigid under justification, and only the gaps past a line's last tab
 * absorb slack.
 * @silent no stop lies ahead, or nothing is configured at all, and then
 * tabs keep their shaped space-equivalent width.
 */
struct TabStopOptions {
  std::vector<TabStop> stops;  ///< explicit stops, ascending
  float interval = 0;          ///< repeat spacing past the last explicit
                               ///< stop; 0 disables repetition
  bool operator==(const TabStopOptions&) const = default;
};

}  // namespace sigil::weave
