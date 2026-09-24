/** @file
 * A domain's client header: the domain as an in-process caller speaks
 * it, typed on both sides of the text that crosses the wire.
 */

#include <sstream>
#include <string>

#include "ProtocolOutputs.h"
#include "ProtocolWriting.h"

namespace sigil::protocol::generator {

std::string clientHeader(const Model& model, const Domain& domain) {
  const std::string space = cppSpace(domain.space);
  std::ostringstream out;
  writeBanner(out, "The " + domain.name + " domain's C++ client");
  out << "#pragma once\n\n";
  out << "#include <sigilprotocol/definition/Call.h>\n";
  out << "#include <sigilprotocol/protocol_values.h>\n\n";
  out << "#include <functional>\n#include <utility>\n\n";
  out << "namespace " << space << " {\n\n";

  writeDocComment(
      out, domain, "",
      "THE CLIENT: this domain as a caller speaks it, one member per command\n"
      "answering through a reply, once, and one per event handing each to a\n"
      "listener. The parameters cross as their JSON form and the answer is\n"
      "read back as the result's table, so an in-process caller and a\n"
      "socket say the same text.");
  out << "class " << domain.service << "Client {\n public:\n";
  out << "  explicit " << domain.service
      << "Client(Caller caller) : m_caller(std::move(caller)) {}\n";

  for (const Command& command : domain.commands) {
    out << "\n";
    writeDocComment(out, command, "  ");
    const std::string result = valueType(command.result, space);
    const bool nothing = command.parameters == model.empty;
    out << "  void " << command.name << "(";
    if (!nothing)
      out << "const " << valueType(command.parameters, space)
          << "& parameters,\n      ";
    out << "Reply<" << result << "> reply) const {\n";
    out << "    callWith<" << result << ">(m_caller, \"" << command.method
        << "\",\n";
    out << "        "
        << (nothing ? valueType(command.parameters, space) + "{}"
                    : std::string("parameters"))
        << ", std::move(reply));\n";
    out << "  }\n";
  }

  for (const Event& event : domain.eventList) {
    out << "\n";
    writeDocComment(out, event, "  ",
                    "Every one is handed to @p listener once this client has\n"
                    "sent enable.");
    const std::string payload = valueType(event.payload, space);
    out << "  void on" << raised(event.name) << "(\n";
    out << "      std::function<void(const " << payload
        << "&)> listener) const {\n";
    out << "    listenFor<" << payload << ">(m_caller, \"" << event.method
        << "\", std::move(listener));\n";
    out << "  }\n";
  }

  out << "\n private:\n  Caller m_caller;\n};\n\n";
  out << "}  // namespace " << space << "\n";
  return out.str();
}

}  // namespace sigil::protocol::generator
