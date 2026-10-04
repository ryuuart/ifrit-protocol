#pragma once

/** @file
 * The families a CSS `font-family` value names, read out of the one
 * string an author writes.
 */

#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::detail {

/** THE FAMILIES @p list NAMES, in the order written: the value split at
 *  each comma outside a quoted name, each name with the space around it
 *  dropped, a quoted name taken between its quotes as written and an
 *  unquoted one with each run of spaces inside it read as one. A generic
 *  name such as `serif` is a name like any other here; what it stands
 *  for is the font manager's to answer. Empty names are passed over. */
[[nodiscard]] std::vector<std::string> familiesOf(std::string_view list);

}  // namespace sigil::compose::detail
