#pragma once

/** @file
 * DELIMITER-SEPARATED TEXT INTO A TABLE — the format a spreadsheet, a
 * gazetteer and a published dataset all leave the building in.
 *
 * The quoting rule is the one every such file is written by: a field may
 * be wrapped in double quotes, a quoted field may hold the delimiter,
 * newlines and quotes, and a quote inside a quoted field is written
 * twice. Anything else is a field's own characters.
 *
 * A line ends at a newline, at a carriage return, or at the pair, so a
 * file written on any of the three machines reads the same.
 */

#include <sigildata/table/Table.h>

#include <optional>
#include <string_view>

namespace sigil::data {

/** How a file is read. Every prop has an answer that suits an ordinary
 *  file, so an ordinary file needs none of them. */
struct CsvOptions {
  /** What separates two fields. Zero infers it: the resource's name
   *  decides when it ends in `.tsv` or `.csv`, and otherwise whichever
   *  of comma, tab and semicolon cuts the first line into the most
   *  fields wins — a tie going to the comma. */
  char delimiter = '\0';
  /** Whether the first line names the columns. Without one, columns are
   *  named by their 1-based position. */
  bool header = true;
  /** A line beginning with this character is not data. Zero means every
   *  line is. */
  char comment = '\0';

  bool operator==(const CsvOptions&) const = default;
};

/** THE TABLE IN @p text, or nothing when it holds no field at all.
 *
 *  A header line with no rows under it is a table: its columns, every
 *  one of them empty. Only text with nothing in it but space answers
 *  nothing, so a caller can tell "no rows" from "no data at all".
 *
 *  A column's type is the one every non-empty cell in it shares:
 *  numbers make a number column, the words `true`, `false`, `yes` and
 *  `no` in any case a boolean column, instants a time column, and
 *  anything else a text column. An empty cell is a missing cell, and in
 *  a number column it is also not a number, so a gap reads as a gap
 *  whichever way it is asked about. A number may carry a leading sign,
 *  either one, and the words `nan` and `inf` in any case are numbers as
 *  well — a `nan` cell is therefore a missing cell. A number too big for
 *  a double is NOT one, because answering it rounded to infinity would
 *  be a different number, so its column is text and the digits survive.
 *
 *  A row with fewer fields than the header has missing cells at its end;
 *  a row with more has its extra fields dropped, since a table's shape
 *  is the header's. A header that writes one name twice keeps both
 *  columns, the later one numbered by its occurrence — `name`, `name_2`
 *  — because two columns of one name are one column and the earlier
 *  one's data would be gone. An empty header name is the column's
 *  1-based position.
 *
 *  A quote that is never closed runs to the end of the text: the field
 *  holds the rest of the file rather than the file being refused, since
 *  a truncated download is still worth the rows above the cut. Bytes are
 *  never inspected for an encoding — a leading byte order mark is
 *  dropped and everything else reaches a text cell as it was written. */
std::optional<Table> decodeCsv(std::string_view text,
                               const CsvOptions& options = {},
                               std::string_view name = {});

/** THE INSTANT @p text names, or nothing when it names none.
 *
 *  A date, `2019-03-08`, or a date and a time joined by `T` or a space,
 *  `2019-03-08T14:25:00`, with optional fractional seconds and an
 *  optional `Z`. No zone offset is read: a time with no `Z` is taken as
 *  UTC, because a file that meant a local time did not say which local
 *  time it meant. */
std::optional<Instant> decodeInstant(std::string_view text);

}  // namespace sigil::data
