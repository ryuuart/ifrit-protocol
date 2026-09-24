/** @file
 * PARAMETERS HELD AGAINST THEIR TABLE before any handler reads them, so
 * a refusal names the parameter it stopped at in the definition's own
 * words rather than a position in the text.
 */

#pragma once

#include <sigildata/decode/Json.h>

#include <optional>
#include <string>
#include <string_view>

namespace sigil::protocol {

/** WHY @p parameters DO NOT FIT @p method's TABLE, naming the parameter:
 *  a member the table does not declare, or a value no field of that
 *  type holds — text where a number stands, a number where text does,
 *  a whole number out of its type's range, a name no value of the
 *  enumeration carries. Nothing where each member fits as far as its
 *  top level; what lies deeper is the reading's to refuse. */
std::optional<std::string> misfit(std::string_view method,
                                  const data::Json& parameters);

}  // namespace sigil::protocol
