#pragma once

/** @file
 * A NAME AND THE FIGURE THAT ANSWERS IT: one row of it, the readout
 * several of them make, the fixed-column table for the reading that is
 * more than a pair, and the bars a column of values is drawn as.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::data {
/** The tabular value a plot reads its two columns out of; its own words
 *  are `<sigildata/table/Table.h>`, which this names and does not
 *  include. */
class Table;
}  // namespace sigil::data

namespace sigil::sketch::kit {

/** ONE READING: what was measured, what it came to, and — where the
 *  figure alone would not say — what that means. */
struct Reading {
  compose::Utf8 name;
  compose::Utf8 value;
  /** After the figure, in the quieter ink: a unit, a bound, a verdict. */
  compose::Utf8 note;
  /** A mark standing BEFORE the name, for a row that is also a key —
   *  a tier, a channel, a series on a chart beside it. Empty (default)
   *  draws none and spends no room. */
  compose::SurfacePaint swatch;
  /** THE COLOUR THIS ROW IS SET IN, over the theme's own: a foot row in
   *  cinnabar, a reading in the colour of the thing it reads. Unset is
   *  the theme's, which is the common case. WHICH rows are lit is the
   *  data's business, which is why this is a colour on the row and not a
   *  register on the theme. */
  std::optional<SkColor4f> ink;
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
  std::optional<float> swatchSide;
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
  std::vector<compose::Utf8> cells;
  /** Before the first column, for a table that is also a key. */
  compose::SurfacePaint swatch;
  /** Names the row, so a query can read it back and a reveal can address
   *  it one row at a time. Empty keys nothing. */
  std::string key;
  /** THE COLOUR THIS ROW IS SET IN, over the theme's own — the foot a
   *  table's own reading is, the row a verdict lights. Unset is the
   *  theme's. */
  std::optional<SkColor4f> ink;
};

/** ONE COLUMN OF A TABLE. */
struct Column {
  /** The word over it, in the theme's section register. Empty in every
   *  column heads the table with nothing and spends no room. */
  compose::Utf8 head;
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
  std::optional<float> swatchSide;
  float swatchCorners = 0;
  /** A hairline between neighbouring rows. */
  bool ruled = false;
  /** A hairline under the HEAD, where the columns are headed. */
  bool headRuled = false;
};

/** THE TABLE — @p rows in @p how's columns, at the theme's row gap.
 *
 *      sketch::kit::table(rows, {.columns = {{.width = 126}, {.width = 46,
 * .figure = true}, {.width = 66}, {}}})
 *
 *  A READOUT and a TABLE are different readings. A readout is a PAIR
 *  ranged to opposite edges of one measure, which is what makes a stack
 *  of them line up on their figures; a table is N columns each at its
 *  own width, which is what a reading of more than a name and a figure
 *  needs. Neither is the other with a field set. */
[[nodiscard]] compose::Element table(std::vector<Row> rows, const Table& how);

/** HOW A COLUMN OF BARS IS DRAWN — the label at the left, the bar against
 *  the largest of the values, and the figure after it, in the theme's
 *  figure colour and register.
 *
 *  A BAR CHART IS NOT A ROW OF METERS. A meter is one fraction of a known
 *  whole; this is N rows against an extent DERIVED from the values, which
 *  is what a plot of a column is, and it is why no scale is stated
 *  anywhere. */
struct Bars {
  /** The longest bar, px. */
  float length = 150;
  /** The value the longest bar stands for; 0 derives it from the values
   *  themselves. */
  double largest = 0;
  /** The width the labels take, px; 0 lets each size itself. */
  float labelMeasure = 96;
  /** Unset is the theme's bar height. */
  std::optional<float> barHeight;
  /** Between the label, the bar and the figure; unset is the theme's
   *  label gap. */
  std::optional<float> gap;
  /** Between rows; unset is the theme's row gap. */
  std::optional<float> rowGap;
  /** The bar itself; unset is the theme's figure colour. */
  std::optional<compose::SurfacePaint> bar;
  /** The track behind it — the room the longest bar takes, so a short bar
   *  reads against the extent. Unset is the theme's figure dimmed;
   *  `Fill::none()` draws no track. */
  std::optional<compose::SurfacePaint> rest;
};

/** THE BARS — one row per value, @p labels read in the same order.
 *
 *      sketch::kit::bars(names, populations, {.length = 150})
 */
[[nodiscard]] compose::Element bars(std::span<const compose::Utf8> labels,
                                    std::span<const double> values,
                                    const Bars& how = {});

/** THE SAME, read straight off two columns of @p table: @p labels names
 *  its text column and @p values its number column. A column that is not
 *  there, or that holds something else, draws no row — which is a table
 *  the sketch has not loaded, and the sketch's own note to write. */
[[nodiscard]] compose::Element bars(const data::Table& table,
                                    std::string_view labels,
                                    std::string_view values,
                                    const Bars& how = {});

}  // namespace sigil::sketch::kit
