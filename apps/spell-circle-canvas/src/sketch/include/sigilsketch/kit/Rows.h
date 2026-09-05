#pragma once

/** @file
 * A NAME AND THE FIGURE THAT ANSWERS IT: one row of it, and the table
 * several of them make.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
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
  /** A colour standing BEFORE the name, for a row that is also a key —
   *  a tier, a channel, a series on a chart beside it. Fill::none()
   *  (default) draws none and spends no room. */
  compose::Fill swatch;
};

/** HOW A ROW IS SET — the widths and the mark, with none of the words. */
struct Readout {
  /** The width the whole row ranges across. 0 sets the name and the
   *  figure side by side at the theme's label gap and lets the row size
   *  itself; a measure fixes the row and pushes the figure to the far
   *  edge, which is what makes a stack of rows line up on their
   *  figures. */
  float measure = 0;
  /** The width the NAME column takes. 0 lets each name size itself,
   *  which ranges the figures only when the names happen to be one
   *  length; a measure is what makes a key-and-figure table a table. */
  float nameMeasure = 0;
  /** The side of a reading's swatch; unset is the theme's. */
  std::optional<float> swatch;
  float swatchCorners = 0;
  /** A hairline between neighbouring rows, in the theme's rule colour. */
  bool ruled = false;
};

/** ONE ROW: @p reading's name at the left in the theme's quiet register
 *  and its figure at the right in the theme's figure colour, set in the
 *  face a CALL is set in so the digits are one width.
 *
 *      sketch::kit::labelRow({.name = u8"nodes", .value = u8"1 248"},
 *                            {.measure = 220})
 *
 *  A FIGURE A SKETCH MEASURED ABOUT ITS OWN EXECUTION goes through
 *  `ctx.measured` BEFORE it reaches here. This component arranges a row;
 *  what the number is, and whether it is pinned, is the sketch's. */
[[nodiscard]] compose::Element labelRow(const Reading& reading,
                                        const Readout& how = {});

/** THE TABLE — @p rows set as @p how says, spaced by the theme's row gap.
 *
 *      sketch::kit::readout({{u8"nodes", nodes}, {u8"instances", live}},
 *                           {.measure = 220, .nameMeasure = 168})
 */
[[nodiscard]] compose::Element readout(std::vector<Reading> rows,
                                       const Readout& how = {});

}  // namespace sigil::sketch::kit
