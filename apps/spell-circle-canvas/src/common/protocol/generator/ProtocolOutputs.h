/** @file
 * THE GENERATOR'S OUTPUTS, each one document written from the model:
 * a domain's agent header, a domain's client header, the list of every
 * table, and the description the Python generator reads the definition
 * from.
 */

#pragma once

#include <string>

#include "ProtocolModel.h"

namespace sigil::protocol::generator {

/** `<Domain>Agent.h`: the agent interface a host implements, one pure
 *  virtual per command the agent answers; the events emitter, one member
 *  per event; and `wire()`, which mounts an agent on an endpoint. */
std::string agentHeader(const Model& model, const Domain& domain);

/** `<Domain>Client.h`: the domain as a caller speaks it, one member per
 *  command answering through a reply and one per event subscribing a
 *  listener. */
std::string clientHeader(const Model& model, const Domain& domain);

/** `Tables.h`: every table of the definition as one list of value types
 *  and one list of the names the definition spells them by, in the same
 *  order — what a case walks to carry every table across its JSON form
 *  without a hand-kept list that could miss one. */
std::string tablesHeader(const Model& model);

/** The whole definition as JSON text — every domain, command, event,
 *  table, field and enumeration with its documentation and its marks —
 *  which the Python generator writes the Python client and the
 *  reference pages from. */
std::string description(const Model& model);

}  // namespace sigil::protocol::generator
