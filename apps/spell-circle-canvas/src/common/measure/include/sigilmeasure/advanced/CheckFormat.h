#pragma once

/** @file
 * @ingroup measure-check
 * How one check prints, and what a run of loose checks counts to — the
 * formatter a table is written through, for a caller that sets its own
 * rows rather than printing a `CheckTable`.
 */

#include <sigilmeasure/check/Check.h>

#include <span>
#include <string>

namespace sigil::measure {

/** @p row as one printed line: `  <label padded> <actual, right-aligned>
 *  PASS`, or `… FAIL want <expected>` — the shape of `"  %-44s %8ld
 *  %s"` at the default columns. A reading stops after its value and a
 *  heading is its label alone, unindented.
 *  @trap A long label is NOT truncated: it pushes the value column right
 *  rather than losing the qualifier at the end of a claim. */
[[nodiscard]] std::string line(const Check& row, CheckColumns columns = {});

/** How many CLAIMS in @p rows failed.
 *  @trap A finding that fails is not among them; `findings()` counts
 *  those. */
[[nodiscard]] int failures(std::span<const Check> rows);

/** How many findings in @p rows failed. */
[[nodiscard]] int findings(std::span<const Check> rows);

}  // namespace sigil::measure
