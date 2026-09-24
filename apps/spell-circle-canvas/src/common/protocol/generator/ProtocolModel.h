/** @file
 * THE DEFINITION AS THE GENERATOR READS IT: every domain with its
 * commands and events, every table with its fields and every
 * enumeration with its values, each carrying the documentation and the
 * attributes the definition wrote over it — read out of the reflected
 * schema into plain values, so every writer walks the same model and
 * none of them reads the schema again.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sigil::protocol::generator {

/** What the definition wrote over one of its parts: the lines of its
 *  three-slash comment, and whether it is marked (experimental). */
struct Documented {
  std::vector<std::string> documentation;
  bool experimental = false;
};

/** One field of a table. `type` is the schema's own word for what one
 *  value of it is — `bool`, `uint`, `double`, `string`, `table` — and
 *  `reference` the table or enumeration it names, fully qualified with
 *  dots; `vector` says the field holds many; `optional` is a scalar the
 *  definition declared `= null`, present or absent. `defaultValue` is the
 *  declared default as JSON text, empty where the field has none. */
struct Field : Documented {
  std::string name;
  std::string type;
  std::string reference;
  bool vector = false;
  bool optional = false;
  bool required = false;
  std::string defaultValue;
};

/** One table, named fully qualified with dots, its fields in the order
 *  the definition declares them. */
struct Table : Documented {
  std::string name;
  std::vector<Field> fields;
};

/** One value of an enumeration. */
struct EnumerationValue : Documented {
  std::string name;
  int64_t value = 0;
};

/** One enumeration, named fully qualified with dots. */
struct Enumeration : Documented {
  std::string name;
  std::string underlying;
  std::vector<EnumerationValue> values;
};

/** One command: `method` is how the wire names it, `clock.step`;
 *  `parameters` and `result` are its tables. A command marked
 *  (asynchronous) is answered through a reply; `dispatcher` is true for
 *  `enable` and `disable`, which no agent answers. */
struct Command : Documented {
  std::string name;
  std::string method;
  std::string parameters;
  std::string result;
  bool asynchronous = false;
  bool dispatcher = false;
};

/** One event: `payload` is the table it carries. */
struct Event : Documented {
  std::string name;
  std::string method;
  std::string payload;
};

/** One domain: `name` is the last word of its namespace and the first
 *  of every method, `space` the namespace with dots, `service` the
 *  domain service's own name. `events` documents the events service,
 *  whose calls are `eventList`. */
struct Domain : Documented {
  std::string name;
  std::string space;
  std::string service;
  Documented events;
  std::vector<Command> commands;
  std::vector<Event> eventList;
};

/** The whole definition. `empty` names the table a command with nothing
 *  to say takes or answers. */
struct Model {
  std::vector<Domain> domains;
  std::vector<Table> tables;
  std::vector<Enumeration> enumerations;
  std::string empty;
};

/** THE MODEL THE REFLECTED SCHEMA @p bfbs HOLDS, or nothing, with @p why
 *  naming the first thing the definition got wrong: a domain, a
 *  command, an event, a table or a field with no documentation, an
 *  events service with no domain beside it, an event that is not marked
 *  (streaming: "server"), a command that is, or a domain with events
 *  and no `enable` and `disable`. */
std::optional<Model> readModel(std::span<const uint8_t> bfbs, std::string* why);

/** The table @p name names, or null. */
const Table* tableNamed(const Model& model, const std::string& name);

/** The enumeration @p name names, or null. */
const Enumeration* enumerationNamed(const Model& model, const std::string& name);

}  // namespace sigil::protocol::generator
