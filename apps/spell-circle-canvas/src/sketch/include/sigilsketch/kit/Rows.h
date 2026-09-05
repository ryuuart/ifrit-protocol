#pragma once

/** @file
 * A NAME AND THE FIGURE THAT ANSWERS IT: one row of it, the readout
 * several of them make, and the fixed-column table for the reading that
 * is more than a pair.
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

/** ONE ROW OF A TABLE: its words in column order, with the mark that
 *  stands before them. */
struct Row {
  std::vector<std::u8string> cells;
  /** Before the first column, for a table that is also a key. */
  compose::Fill swatch;
  /** Names the row, so a query can read it back and a reveal can address
   *  it one row at a time. Empty keys nothing. */
  std::string key;
};

/** ONE COLUMN OF A TABLE. */
struct Column {
  /** The width it takes. 0 lets it size itself, which is what the LAST
   *  column usually wants, since nothing ranges after it. */
  float width = 0;
  /** Sets the column in the theme's figure colour and in the face a CALL
   *  is set in, so its digits are one width. false sets it in the quiet
   *  register a name is set in. */
  bool figure = false;
};

/** HOW A TABLE IS SET. */
struct Table {
  /** In order across. A row with more words than there are columns sets
   *  the surplus in the last column's register at its own width. */
  std::vector<Column> columns;
  /** Between columns; unset is the theme's label gap. */
  std::optional<float> gap;
  /** The side of a row's swatch; unset is the theme's. */
  std::optional<float> swatch;
  float swatchCorners = 0;
  /** A hairline between neighbouring rows. */
  bool ruled = false;
};

/** THE TABLE — @p rows in @p how's columns, at the theme's row gap.
 *
 *      sketch::kit::table(rows, {.columns = {{126}, {46, true}, {66}, {}}})
 *
 *  A READOUT and a TABLE are different readings. A readout is a PAIR
 *  ranged to opposite edges of one measure, which is what makes a stack
 *  of them line up on their figures; a table is N columns each at its
 *  own width, which is what a reading of more than a name and a figure
 *  needs. Neither is the other with a field set. */
[[nodiscard]] compose::Element table(std::vector<Row> rows,
                                     const Table& how);

}  // namespace sigil::sketch::kit
