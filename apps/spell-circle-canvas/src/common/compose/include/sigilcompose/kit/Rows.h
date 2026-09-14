#pragma once

/** @file
 * SigilCompose KIT — A NAME AND THE FIGURE THAT ANSWERS IT: one reading,
 * the readout a stack of them makes, the table a reading of more than a
 * pair needs, and the bars a column of values is drawn as.
 *
 * A READOUT AND A TABLE ARE DIFFERENT READINGS, and neither is the other
 * with a field set. A readout is a PAIR ranged to opposite edges of one
 * measure, which is what makes a stack of them line up on their figures;
 * a table is N columns each at its own width, which is what a reading of
 * more than a name and a figure needs — a key, a cost, the tier it took
 * and the condition that refused it.
 *
 * Every prop is the CONTENT and the ARRANGEMENT — the words, the values,
 * the widths, the air. Every face, size and colour is the CASCADE's: a
 * name and a note are set in the class `captionNote`, a figure in
 * `readout`, a table's head in `section`, and the `weave::StyleSheet` in
 * force where the component lands says what those are.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilcompose/kit/Part.h>
#include <sigilcompose/kit/Specimen.h>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sigil::compose::kit {

/** The leaf a table's head cell defaults to: @p text in the class
 *  `section`, the register a name INSIDE the content is set in. */
[[nodiscard]] inline Element section(const Utf8& text) {
  return compose::text(text).styleClass("section");
}

// ---------------------------------------------------------------------------
// The reading, and the readout a stack of them makes

/** ONE READING: what was measured, what it came to, and — where the
 *  figure alone would not say — what that means. */
struct Reading {
  Utf8 name;
  Utf8 value;
  /** After the figure, in the quieter register: a unit, a bound, a
   *  verdict. */
  Utf8 note;
  /** A patch standing BEFORE the name, for a row that is also a key — a
   *  tier, a channel, a series on a chart beside it. None (default) draws
   *  none and spends no room. */
  SurfacePaint swatch;
  /** THE COLOUR THIS ROW IS SET IN, over whatever its lines' classes name:
   *  a foot row in cinnabar, a reading in the colour of the thing it
   *  reads. Unset leaves each line in its own class's colour, which is
   *  the common case. It is a colour and not a class because WHICH rows
   *  are lit is the data's business and a sheet cannot say it. */
  std::optional<SkColor4f> ink;
};

/** HOW A ROW IS SET — the widths, the mark and the air, with none of the
 *  words. One value sets a whole stack, so the rows of a readout line up
 *  on their figures.
 *
 *  NO TYPE IS HERE. The three lines are PARTS: `nameLine` and `noteLine`
 *  default to `captionNote` and `valueLine` to `figure`, leaves in the
 *  class of the sheet in force where the row lands. */
struct Rows {
  /** The width the whole row ranges across, px. 0 sets the name and the
   *  figure side by side at `labelGap` and lets the row size itself; a
   *  measure fixes the row and pushes the figure to the far edge, which
   *  is what makes a stack of rows line up on their figures. */
  float measure = 0.0f;
  /** The width the NAME column takes, px. 0 lets each name size itself,
   *  which ranges the figures only when the names happen to be one
   *  length. */
  float nameMeasure = 0.0f;
  /** Between the rows of a readout, px. */
  float gap = 5.0f;
  /** Between a name and the figure that answers it where the two stand
   *  side by side rather than ranged to opposite edges, px. */
  float labelGap = 10.0f;
  /** A line between neighbouring rows. Fill::none() (default) rules
   *  none. */
  Fill divider;
  float dividerWidth = 1.0f;
  /** The side of a reading's swatch and its radius, px. */
  float swatchSide = 10.0f;
  float swatchCorners = 0.0f;
  /** THE THREE LINES, as functions of their text and then of this value;
   *  a part takes the parameters it names. Empty means the default. */
  Part<Utf8, Rows> nameLine = captionNote;
  Part<Utf8, Rows> valueLine = figure;
  Part<Utf8, Rows> noteLine = captionNote;
};

/** ONE ROW: @p one's name at the left and its figure at the right of
 *  @p how's measure, with the mark that stands before the name.
 *
 *      kit::reading({.name = u8"nodes", .value = u8"1 248"},
 *                   {.measure = 220})
 */
[[nodiscard]] Element reading(const Reading& one, const Rows& how = {});

/** THE READOUT — @p rows stacked at @p how's gap, ruled between where a
 *  divider is named. */
[[nodiscard]] Element readout(std::span<const Reading> rows,
                              const Rows& how = {});

// ---------------------------------------------------------------------------
// The table

/** ONE COLUMN OF A TABLE: what heads it, how wide it stands, and whether
 *  what it carries is a figure. */
struct Column {
  /** The head cell's words. Empty in every column heads the table with
   *  nothing and spends no room. */
  Utf8 head;
  /** The width it takes, px. 0 lets it size itself, which is what the
   *  LAST column usually wants, since nothing ranges after it. */
  float width = 0.0f;
  /** Sets the column's cells in the class `readout` rather than in
   *  `captionNote`, so its digits read as measured figures. */
  bool figure = false;
};

/** HOW A TABLE IS SET. */
struct Table {
  /** In order across. A row with more words than there are columns sets
   *  the surplus in the last column's class at its own width. */
  std::vector<Column> columns;
  /** Between columns, px. */
  float gap = 10.0f;
  /** Between rows, px. */
  float rowGap = 5.0f;
  /** A line between neighbouring rows. Fill::none() (default) rules
   *  none. */
  Fill divider;
  float dividerWidth = 1.0f;
  /** A line under the head, in the divider's fill — or, where no divider
   *  is named, in the ink in force. */
  bool headRuled = false;
  /** ONE PATCH PER ROW, in row order, standing before that row's first
   *  column. A row past the end of this run, or one whose patch is none,
   *  carries no mark and spends no room. */
  std::span<const SurfacePaint> swatches;
  float swatchSide = 10.0f;
  float swatchCorners = 0.0f;
  /** ONE KEY PER ROW, in row order, so a query can read a row back and a
   *  reveal can address the table one row at a time. An empty name keys
   *  nothing. */
  std::span<const std::string> keys;
  /** THE HEAD CELL, as a function of its words and then of this table.
   *  Empty is `section`. */
  Part<Utf8, Table> headLine = section;
  /** ONE BODY CELL, as a function of its words, this table, the index of
   *  the column it stands in and the index of its ROW — a part takes the
   *  parameters it names, so a table that dresses a column names three
   *  and one that lights a row names four. Empty sets each cell in its
   *  own column's class — `readout` for a figure column, `captionNote`
   *  for the rest. */
  Part<Utf8, Table, std::size_t, std::size_t> cellLine;
};

/** THE TABLE — @p rows in @p how's columns, each row its own run of
 *  cells, so a row may stop short of the columns or run past them.
 *
 *      kit::table(rows, {.columns = {{u8"KEY", 126}, {u8"COST", 46, true}}})
 */
[[nodiscard]] Element table(std::span<const std::span<const Utf8>> rows,
                            const Table& how);
/** The same from ONE ROW-MAJOR RUN of cells, `how.columns.size()` per
 *  row — the rectangular table, whose rows are all one length. */
[[nodiscard]] Element table(std::span<const Utf8> cells, const Table& how);

// ---------------------------------------------------------------------------
// The bars a column of values is drawn as

/** HOW A COLUMN OF BARS IS DRAWN: the label at the left, the bar against
 *  the largest value, and the figure after it.
 *
 *  IT IS NOT A METER. A meter is one fraction of a known whole; this is N
 *  rows against an extent DERIVED from the values, which is what a plot
 *  of a column is. */
struct Bars {
  /** The longest bar, px. */
  float length = 150.0f;
  /** The value the longest bar stands for. 0 (default) derives it from
   *  the values themselves, which is what makes a column of data a plot
   *  without a scale being stated anywhere. */
  double largest = 0.0;
  /** The width the labels take, px. 0 lets each size itself. */
  float labelMeasure = 96.0f;
  float barHeight = 11.0f;
  /** Between the label, the bar and the figure, px. */
  float gap = 8.0f;
  /** Between rows, px. */
  float rowGap = 4.0f;
  /** The bar itself. None (default) is the ink in force. */
  SurfacePaint bar;
  /** The track behind the bar — the room the longest bar takes, so a
   *  short bar reads against the extent. None (default) draws none. */
  SurfacePaint rest;
  /** THE LABEL, as a function of its words and then of this value. Empty
   *  is `captionNote`. */
  Part<Utf8, Bars> labelLine = captionNote;
  /** THE FIGURE AFTER THE BAR, as a function of the VALUE, because how a
   *  number reads is the data's business and not the kit's. Empty is the
   *  value to the nearest whole number, in the class `readout`. */
  Part<double, Bars> figureLine;
};

/** THE BARS — one row per value, @p labels read in the same order.
 *
 *      kit::bars(names, populations, {.length = 150})
 */
[[nodiscard]] Element bars(std::span<const Utf8> labels,
                           std::span<const double> values,
                           const Bars& how = {});

}  // namespace sigil::compose::kit
