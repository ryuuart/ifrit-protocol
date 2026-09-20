#pragma once

/** @file
 * @ingroup data-decode
 * DELIMITER-SEPARATED TEXT INTO A TABLE — the format a spreadsheet, a
 * gazetteer and a published dataset all leave the building in. The
 * quoting rule is the one every such file is written by: a field may be
 * wrapped in double quotes, a quoted field may hold the delimiter,
 * newlines and quotes, and a quote inside a quoted field is written
 * twice. A line ends at a newline, at a carriage return, or at the
 * pair, so a file written on any of the three machines reads the same.
 */

#include <sigildata/table/Table.h>

#include <optional>
#include <string_view>

namespace sigil::data {

/** How a file is read. Every prop has an answer that suits an ordinary
 *  file, so an ordinary file needs none of them. */
struct CsvOptions {
  /** What separates two fields. Zero infers it, from the resource's
   *  name where it ends `.tsv` or `.csv` and otherwise from whichever
   *  of comma, tab and semicolon cuts the first line into the most
   *  fields, a tie going to the comma. */
  char delimiter = '\0';
  /** Whether the first line names the columns. Without one, columns are
   *  named by their 1-based position. */
  bool header = true;
  /** A line beginning with this character is not data. Zero means every
   *  line is. */
  char comment = '\0';

  bool operator==(const CsvOptions&) const = default;
};

/** THE TABLE IN @p text, or nothing when it holds no field at all. A
 *  column's type is the one every non-empty cell in it shares, an empty
 *  cell is a missing cell, and a row shorter than the header is padded
 *  with missing cells while one longer is cut to the header's shape.
 *  @trap A header line with no rows under it IS a table, its columns
 *  every one of them empty; only text with nothing in it but space
 *  answers nothing, so a caller can tell "no rows" from "no data". */
std::optional<Table> decodeCsv(std::string_view text,
                               const CsvOptions& options = {},
                               std::string_view name = {});

/** THE INSTANT @p text names, or nothing when it names none: a date,
 *  `2019-03-08`, or a date and a time joined by `T` or a space,
 *  `2019-03-08T14:25:00`, with optional fractional seconds and an
 *  optional `Z`.
 *  @trap No zone offset is read — a time with no `Z` is taken as UTC,
 *  because a file that meant a local time did not say which. */
std::optional<Instant> decodeInstant(std::string_view text);

}  // namespace sigil::data
