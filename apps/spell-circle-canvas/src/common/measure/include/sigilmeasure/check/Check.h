#pragma once

/** @file
 * @ingroup measure-check
 * A verified claim — its label, the value it expected, the value it got
 * and a verdict — the overloads that produce one from two numbers, the
 * rows that stand beside claims without judging anything, and the table
 * a run of them prints as.
 */

#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::measure {

/** WHAT A ROW'S VERDICT MEANS to the run it stands in. A verification
 *  is not only claims: beside them stand the measurements they were
 *  made from, the titles that group them, and claims about the SUBJECT
 *  rather than the construction. What a row is, is stated here rather
 *  than typed into its text, so a reader of the table and a build that
 *  counts its failures read the same thing. */
enum class Standing : uint8_t {
  /** A claim about the construction: its FAIL fails the run. */
  Claim,
  /** A claim about the subject: its verdict is printed as any claim's is
   *  and never counted against the run. */
  Finding,
  /** A measurement reported beside the claims and judged by nobody: its
   *  value is printed with no verdict. */
  Reading,
  /** A title over the rows that follow: a label alone. */
  Heading,
};

/** One claim, its evidence, and its verdict. The printed line is
 *  COMPUTED from the same two values it reports, so the sentence and
 *  the measurement cannot drift apart, and the verdict is a value
 *  rather than a string, so a set of them can fail a build — see
 *  `CheckTable::failures()`. */
struct Check {
  std::string label;
  std::string expected, actual;  ///< already formatted, for printing
  bool pass = false;
  Standing standing = Standing::Claim;

  /** Whether this row carries a verdict at all — a claim or a finding. */
  bool judged() const {
    return standing == Standing::Claim || standing == Standing::Finding;
  }
};

/** THE TWO COLUMNS A CHECK PRINTS IN: the label padded to `labelWidth`,
 *  the value right-aligned in `valueWidth`.
 *  @trap A long label is NOT truncated: it pushes the value column
 *  right rather than losing the qualifier at the end of a claim. */
struct CheckColumns {
  int labelWidth = 44;
  int valueWidth = 8;
};

namespace detail {
inline std::string formatCount(long count) {
  char text[32];
  std::snprintf(text, sizeof text, "%ld", count);
  return text;
}
inline std::string formatNumber(double number) {
  char text[48];
  std::snprintf(text, sizeof text, "%.6g", number);
  return text;
}
}  // namespace detail

/** Integer identity — the conservation check, where two counts must
 *  agree exactly.
 *  @trap Constrained to integral types on purpose: a `long` parameter
 *  would swallow a float through an implicit truncation and report
 *  EXACT on two numbers that differ. A float pair is a compile error
 *  here and belongs to the tolerance overload. */
template <std::integral T, std::integral U>
Check check(std::string label, T expected, U actual) {
  return {std::move(label), detail::formatCount((long)expected),
          detail::formatCount((long)actual), expected == actual};
}

/** Float agreement within @p tolerance, in the values' own units.
 *  @trap There is NO default tolerance: how closely a measured value
 *  and a solved one must agree is a property of the construction being
 *  checked, not of this header. */
inline Check check(std::string label, double expected, double actual,
                   double tolerance) {
  Check made{std::move(label), detail::formatNumber(expected),
             detail::formatNumber(actual),
             std::fabs(expected - actual) <= tolerance};
  made.expected += " \xc2\xb1 " + detail::formatNumber(tolerance);
  return made;
}

/** Text identity — for a claim whose evidence is a name or a spelling
 *  rather than a number. Byte comparison; no trimming, no case folding. */
inline Check check(std::string label, std::string_view expected,
                   std::string_view actual) {
  return {std::move(label), std::string(expected), std::string(actual),
          expected == actual};
}

/** The bare assertion, for a claim with no two numbers to compare
 *  ("every interior arc endpoint has degree 2"). */
inline Check check(std::string label, bool condition) {
  return {std::move(label), "true", condition ? "true" : "false", condition};
}

/** @p claim restated as a FINDING: a claim about the subject, whose
 *  verdict is computed and printed exactly as it was and never counted
 *  against the run. `finding(check("legend holds", 1.0, measured, 0.01))`
 *  is how a plate is shown to contradict its own legend. */
inline Check finding(Check claim) {
  claim.standing = Standing::Finding;
  return claim;
}

/** A READING: @p value reported under @p label with no verdict — a
 *  residual in scientific notation, a count, a ratio, a name — what the
 *  claims beside it were made from. */
inline Check reading(std::string label, std::string value) {
  return {std::move(label), "", std::move(value), true, Standing::Reading};
}
/** A reading of a number, formatted as the tolerance overload formats
 *  its values. */
inline Check reading(std::string label, double value) {
  return reading(std::move(label), detail::formatNumber(value));
}
/** A reading of a count. */
template <std::integral T>
Check reading(std::string label, T value) {
  return reading(std::move(label), detail::formatCount((long)value));
}

/** A HEADING: @p title over the rows that follow it. */
inline Check heading(std::string title) {
  return {std::move(title), "", "", true, Standing::Heading};
}

/** A run of checks in the order they were made, printed as one table:
 *  every row through `line()` at a shared width, then a summary
 *  row, so a run of claims reads as a column and ends with its verdict. */
struct CheckTable {
  std::vector<Check> rows;

  /** Appends @p row and answers this table, so rows chain. */
  CheckTable& add(Check row) {
    rows.push_back(std::move(row));
    return *this;
  }
  /** How many claims in the table did not hold — an exit code for a
   *  verification run.
   *  @trap A finding that fails is not among them: its failing is a
   *  statement about the subject, and is counted by `findings()`. */
  int failures() const;
  /** How many findings in the table did not hold — the things the run
   *  found out about its subject. */
  int findings() const;
  /** Whether every claim held; a finding that did not hold is not a
   *  failure. */
  bool pass() const { return failures() == 0; }
  /** How many rows carry a verdict — the claims and the findings. */
  int checks() const;

  /** One string per row at @p columns, then a final `  <n> checks, <m> failed` line
   *  (`all passed` when none did), with the findings after it when one
   *  did not hold. The readings and headings are printed and not
   *  counted.
   *  @silent a table with no rows: it prints nothing rather than a
   *  summary of nothing. */
  std::vector<std::string> lines(CheckColumns columns = {}) const;
};

}  // namespace sigil::measure
