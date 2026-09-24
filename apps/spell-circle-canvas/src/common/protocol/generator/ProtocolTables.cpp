/** @file
 * Every table of the definition as one type list and one list of names,
 * for a case that walks them all.
 */

#include <sstream>
#include <string>

#include "ProtocolOutputs.h"
#include "ProtocolWriting.h"

namespace sigil::protocol::generator {

std::string tablesHeader(const Model& model) {
  std::ostringstream out;
  writeBanner(out, "Every table of the protocol definition, as one list");
  out << "#pragma once\n\n";
  out << "#include <sigilprotocol/protocol_values.h>\n\n";
  out << "#include <array>\n#include <string_view>\n#include <tuple>\n\n";
  out << "namespace sigil::protocol {\n\n";
  out << "/** EVERY TABLE OF THE DEFINITION, as its value type, in the order\n"
         " *  the reflected schema lists them. */\n";
  out << "using Tables = std::tuple<\n";
  for (size_t i = 0; i < model.tables.size(); ++i)
    out << "    " << valueType(model.tables[i].name, "sigil::protocol")
        << (i + 1 == model.tables.size() ? ">;\n\n" : ",\n");
  out << "/** Each one's name as the definition spells it, in the same order. */\n";
  out << "inline constexpr std::array<std::string_view, " << model.tables.size()
      << "> tableNames{\n";
  for (size_t i = 0; i < model.tables.size(); ++i)
    out << "    \"" << model.tables[i].name << "\""
        << (i + 1 == model.tables.size() ? "};\n\n" : ",\n");
  out << "}  // namespace sigil::protocol\n";
  return out.str();
}

}  // namespace sigil::protocol::generator
