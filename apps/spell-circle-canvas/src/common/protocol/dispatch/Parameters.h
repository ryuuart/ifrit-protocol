/** @file
 * PARAMETERS HELD AGAINST THEIR TABLE before any handler reads them, so
 * a refusal names the parameter it stopped at in the definition's own
 * words rather than a position in the text.
 */

#pragma once

#include <sigildata/decode/Dialect.h>
#include <sigildata/decode/Json.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::protocol {

/** @p value AS JSON TEXT: an envelope is text, and a value goes into one
 *  as the text its own dialect writes. */
inline std::string jsonText(const data::Json& value) {
  const std::vector<std::byte> bytes = data::encode(value, data::Dialect::Json);
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

/** WHY @p parameters DO NOT FIT @p method's TABLE, naming the parameter:
 *  a member the table does not declare, or a value no field of that
 *  type holds — text where a number stands, a number where text does,
 *  a whole number out of its type's range, a name or a number no value
 *  of the enumeration carries. Nothing where each member fits as far as
 *  its top level; what lies deeper is the reading's to refuse. */
std::optional<std::string> misfit(std::string_view method,
                                  const data::Json& parameters);

}  // namespace sigil::protocol
