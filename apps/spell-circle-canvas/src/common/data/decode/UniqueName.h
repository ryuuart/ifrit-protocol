#pragma once

/** @file
 * The one rule both decoders name a repeated column by. Private to this
 * target: no public header mentions it.
 */

#include <sigildata/table/Table.h>

#include <string>

namespace sigil::data {

/** @p wanted, or @p wanted with a `_2`, `_3` … suffix when @p table
 *  already holds a column of that name.
 *
 *  Two columns cannot share a name: the second would replace the first
 *  and the source's data would be gone. So a source that writes a name
 *  twice keeps every column, the repeats numbered by their occurrence,
 *  and a suffix that is itself already taken keeps counting. */
inline std::string unusedName(const Table& table, std::string wanted) {
  if (!table.has(wanted)) return wanted;
  for (int occurrence = 2;; ++occurrence) {
    std::string candidate = wanted + "_" + std::to_string(occurrence);
    if (!table.has(candidate)) return candidate;
  }
}

}  // namespace sigil::data
