#pragma once

/** @file
 * A NAME AND THE FIGURE THAT ANSWERS IT: one row of it, and the table
 * several of them make.
 */

#include <sigilcompose/core/Element.h>
#include <sigilsketch/kit/Theme.h>

#include <string>
#include <vector>

namespace sigil::sketch::kit {

/** ONE READING: what was measured, what it came to, and — where the
 *  figure alone would not say — what that means. */
struct Reading {
  std::u8string name;
  std::u8string value;
  /** After the figure, in the quieter ink: a unit, a bound, a verdict. */
  std::u8string note;
};

/** ONE ROW: @p reading's name at the left in the theme's quiet register
 *  and its figure at the right in the theme's figure colour, set in the
 *  face a CALL is set in so the digits are one width.
 *
 *      sketch::kit::labelRow({.name = toU8("nodes"),
 *                             .value = toU8("1 248")}, 220)
 *
 *  @p measure is the width the two range across. 0 sets them side by side
 *  at the theme's label gap and lets the row size itself; a measure fixes
 *  the row and pushes the figure to the far edge, which is what makes a
 *  stack of rows line up on their figures.
 *
 *  A FIGURE A SKETCH MEASURED ABOUT ITS OWN EXECUTION goes through
 *  `ctx.measured` BEFORE it reaches here. This component arranges a row;
 *  what the number is, and whether it is pinned, is the sketch's. */
[[nodiscard]] compose::Element labelRow(const Reading& reading,
                                        float measure = 0);

/** A TABLE OF READINGS, one per row. */
struct Readout {
  std::vector<Reading> rows;
  /** The width every row ranges across; 0 lets each size itself. */
  float measure = 0;
  /** A hairline between neighbouring rows, in the theme's rule colour. */
  bool ruled = false;
};

/** THE TABLE, spaced by the theme's row gap.
 *
 *      sketch::kit::readout({.rows = {{u8"nodes", nodes},
 *                                     {u8"instances", instances}},
 *                            .measure = 220})
 */
[[nodiscard]] compose::Element readout(const Readout& table);

}  // namespace sigil::sketch::kit
