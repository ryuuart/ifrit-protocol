/** @file
 * A domain's agent header: the interface a host implements, the events
 * it sends, and the function that mounts the one on an endpoint.
 */

#include <sstream>
#include <string>

#include "ProtocolOutputs.h"
#include "ProtocolWriting.h"

namespace sigil::protocol::generator {
namespace {

/** Whether a command takes the empty table, and so takes nothing. */
bool takesNothing(const Command& command, const std::string& empty) {
  return command.parameters == empty;
}

void writeAgent(std::ostream& out, const Domain& domain,
                const std::string& space, const std::string& empty) {
  writeDocComment(
      out, domain, "",
      "THE AGENT: what a host implements to answer this domain, one member\n"
      "per command. A command answered at once returns its answer; one\n"
      "marked asynchronous is handed a reply and answers through it, once.\n"
      "enable and disable are the dispatcher's, and no agent answers them.");
  out << "class " << domain.service << "Agent {\n public:\n";
  out << "  virtual ~" << domain.service << "Agent() = default;\n";
  for (const Command& command : domain.commands) {
    if (command.dispatcher) continue;
    out << "\n";
    writeDocComment(out, command, "  ");
    const std::string result = valueType(command.result, space);
    const std::string parameters =
        takesNothing(command, empty)
            ? std::string()
            : "const " + valueType(command.parameters, space) + "& parameters";
    if (command.asynchronous) {
      out << "  virtual void " << command.name << "(" << parameters
          << (parameters.empty() ? "" : ", ") << "Reply<" << result
          << "> reply) = 0;\n";
    } else {
      out << "  virtual Answer<" << result << "> " << command.name << "("
          << parameters << ") = 0;\n";
    }
  }
  out << "};\n\n";
}

void writeEvents(std::ostream& out, const Domain& domain,
                 const std::string& space) {
  writeDocComment(
      out, domain.events, "",
      "THE EVENTS: one member per event, each sending its table's JSON form\n"
      "under the event's method through the emit a dispatcher hands it.\n"
      "False where the table cannot hold what it was given.");
  out << "class " << domain.service << "Events {\n public:\n";
  out << "  explicit " << domain.service
      << "Events(Emit emit) : m_emit(std::move(emit)) {}\n";
  for (const Event& event : domain.eventList) {
    out << "\n";
    writeDocComment(out, event, "  ");
    out << "  bool " << event.name << "(const "
        << valueType(event.payload, space) << "& event) const {\n";
    out << "    return emitEvent(m_emit, \"" << event.method << "\", event);\n";
    out << "  }\n";
  }
  out << "\n private:\n  Emit m_emit;\n};\n\n";
}

void writeWire(std::ostream& out, const Domain& domain,
               const std::string& space, const std::string& empty) {
  out << "/** MOUNTS @p agent ON @p endpoint: one handler per command the agent\n"
         " *  answers, filed under the command's method. Parameters that do\n"
         " *  not fit the command's table are refused before the agent is\n"
         " *  asked; enable and disable are the dispatcher's own and are not\n"
         " *  mounted here. The agent outlives the endpoint's handlers. */\n";
  out << "template <Mounts Endpoint>\n";
  out << "void wire(Endpoint& endpoint, " << domain.service
      << "Agent& agent) {\n";
  for (const Command& command : domain.commands) {
    if (command.dispatcher) continue;
    const std::string parameters = valueType(command.parameters, space);
    const std::string result = valueType(command.result, space);
    const bool nothing = takesNothing(command, empty);
    out << "  endpoint.mount(\n";
    out << "      \"" << command.method << "\",\n";
    out << "      " << (command.asynchronous ? "answerLater" : "answerNow")
        << "<" << parameters << ", " << result << ">(\n";
    out << "          \"" << command.method << "\",\n";
    out << "          [&agent](const " << parameters << "&"
        << (nothing ? "" : " parameters");
    if (command.asynchronous) {
      out << ",\n                   Reply<" << result << "> reply) {\n";
      out << "            agent." << command.name << "("
          << (nothing ? "" : "parameters, ") << "std::move(reply));\n";
    } else {
      out << ") {\n";
      out << "            return agent." << command.name << "("
          << (nothing ? "" : "parameters") << ");\n";
    }
    out << "          }));\n";
  }
  out << "}\n\n";
}

}  // namespace

std::string agentHeader(const Model& model, const Domain& domain) {
  const std::string space = cppSpace(domain.space);
  const std::string& empty = model.empty;
  std::ostringstream out;
  writeBanner(out, "The " + domain.name +
                       " domain's agent, its events and the function that"
                       " mounts it");
  out << "#pragma once\n\n";
  out << "#include <sigilprotocol/definition/Mount.h>\n";
  out << "#include <sigilprotocol/protocol_values.h>\n\n";
  out << "#include <utility>\n\n";
  out << "namespace " << space << " {\n\n";
  writeAgent(out, domain, space, empty);
  if (!domain.eventList.empty()) writeEvents(out, domain, space);
  writeWire(out, domain, space, empty);
  out << "}  // namespace " << space << "\n";
  return out.str();
}

}  // namespace sigil::protocol::generator
