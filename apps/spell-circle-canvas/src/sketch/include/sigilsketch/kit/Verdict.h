#pragma once

/** @file
 * @ingroup sketch-kit
 *
 * A RUN OF CHECKS DRAWN: every row of a `measure::CheckTable` as a line of
 * a table — the heading over the rows it groups, a claim or a finding with
 * the value it got and its verdict, a reading with its value alone.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilmeasure/check/Check.h>
#include <sigilsketch/kit/Rows.h>

#include <optional>
#include <vector>

namespace sigil::sketch::kit {

/** WHICH ROWS OF A TABLE A VERDICT DRAWS. */
enum class VerdictRows {
  /** Every row: headings, claims, findings and readings. */
  Every,
  /** The rows that carry a verdict — the claims and the findings. */
  Judged,
  /** The judged rows that did not hold: the panel a plate raises only
   *  when its construction is wrong. */
  Failures,
};

/** HOW A VERDICT IS SET. The colours are none of these: a row's verdict
 *  cell names the class `checkPass` or `checkFail`, a heading the class
 *  `checkHeading`, and `Theme::styleSheet()` states all three from the
 *  palette's `pass`, `fail` and `ink`, so a sketch restyles a verdict with
 *  a rule and never with a prop. */
struct Verdict {
  VerdictRows rows = VerdictRows::Every;
  /** The label, the value and the verdict, in that order, as a table's
   *  columns take them. Empty sets the label at 220 px, the value at 72 px
   *  in the figure register, and the verdict in what is left. */
  std::vector<Column> columns;
  /** A mark before each judged row in its verdict's colour. */
  bool swatches = true;
  /** A hairline between neighbouring rows. */
  bool ruled = false;
  /** A last row counting the claims and the ones that failed. */
  bool summary = false;
  /** The verdict cell of a row that held. */
  compose::Utf8 passed = u8"PASS";
  /** The verdict cell of a row that did not hold, followed by the value
   *  the row expected. */
  compose::Utf8 failed = u8"FAIL want ";
  /** The verdict cell of a reading, which is judged by nobody. */
  compose::Utf8 unjudged;
};

/** THE VERDICT — @p table's rows set as @p how says, in the theme in force.
 *
 *      sketch::kit::verdict(table)
 *      sketch::kit::verdict(table, {.rows = sketch::kit::VerdictRows::Failures})
 *
 *  Every verdict is the check's own: the kit words it and does not judge. */
[[nodiscard]] compose::Element verdict(const measure::CheckTable& table,
                                       const Verdict& how = {});

}  // namespace sigil::sketch::kit
