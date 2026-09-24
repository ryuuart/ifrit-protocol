/** @file
 * The definition as JSON text, for the Python generator: the model
 * written out whole, one key per member, in the model's own order.
 */

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "ProtocolOutputs.h"

namespace sigil::protocol::generator {
namespace {

/** @p text as a JSON string, quotes included. */
std::string quoted(const std::string& text) {
  std::string out = "\"";
  for (const char each : text) {
    switch (each) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(each) < 0x20) {
          char escaped[8];
          std::snprintf(escaped, sizeof escaped, "\\u%04x", each);
          out += escaped;
        } else {
          out += each;
        }
    }
  }
  return out + "\"";
}

const char* truth(bool value) { return value ? "true" : "false"; }

/** Writes JSON with two spaces a level, one member per line, so the
 *  description reads as a file and differs line by line. */
class Writer {
 public:
  explicit Writer(std::ostringstream& out) : m_out(out) {}

  void open(const std::string& key, char bracket) {
    member(key);
    m_out << bracket;
    m_first.push_back(true);
  }
  void openItem(char bracket) {
    item();
    m_out << bracket;
    m_first.push_back(true);
  }
  void close(char bracket) {
    const bool empty = m_first.back();
    m_first.pop_back();
    if (!empty) m_out << "\n" << indent();
    m_out << bracket;
  }
  void text(const std::string& key, const std::string& value) {
    member(key);
    m_out << quoted(value);
  }
  void raw(const std::string& key, const std::string& value) {
    member(key);
    m_out << value;
  }
  void flag(const std::string& key, bool value) { raw(key, truth(value)); }
  void lines(const std::string& key, const std::vector<std::string>& values) {
    open(key, '[');
    for (const std::string& value : values) {
      item();
      m_out << quoted(value);
    }
    close(']');
  }
  void documented(const Documented& part) {
    lines("documentation", part.documentation);
    flag("experimental", part.experimental);
  }

 private:
  std::string indent() const { return std::string(m_first.size() * 2, ' '); }
  void item() {
    if (!m_first.empty()) {
      m_out << (m_first.back() ? "\n" : ",\n");
      m_first.back() = false;
    }
    m_out << indent();
  }
  void member(const std::string& key) {
    item();
    m_out << quoted(key) << ": ";
  }

  std::ostringstream& m_out;
  std::vector<bool> m_first;
};

}  // namespace

std::string description(const Model& model) {
  std::ostringstream out;
  Writer writer(out);
  writer.openItem('{');
  writer.text("empty", model.empty);

  writer.open("domains", '[');
  for (const Domain& domain : model.domains) {
    writer.openItem('{');
    writer.text("name", domain.name);
    writer.text("space", domain.space);
    writer.text("service", domain.service);
    writer.documented(domain);
    writer.open("events", '{');
    writer.documented(domain.events);
    writer.close('}');
    writer.open("commands", '[');
    for (const Command& command : domain.commands) {
      writer.openItem('{');
      writer.text("name", command.name);
      writer.text("method", command.method);
      writer.text("parameters", command.parameters);
      writer.text("result", command.result);
      writer.flag("asynchronous", command.asynchronous);
      writer.flag("dispatcher", command.dispatcher);
      writer.documented(command);
      writer.close('}');
    }
    writer.close(']');
    writer.open("eventList", '[');
    for (const Event& event : domain.eventList) {
      writer.openItem('{');
      writer.text("name", event.name);
      writer.text("method", event.method);
      writer.text("payload", event.payload);
      writer.documented(event);
      writer.close('}');
    }
    writer.close(']');
    writer.close('}');
  }
  writer.close(']');

  writer.open("tables", '[');
  for (const Table& table : model.tables) {
    writer.openItem('{');
    writer.text("name", table.name);
    writer.documented(table);
    writer.open("fields", '[');
    for (const Field& field : table.fields) {
      writer.openItem('{');
      writer.text("name", field.name);
      writer.text("type", field.type);
      writer.text("reference", field.reference);
      writer.flag("vector", field.vector);
      writer.flag("optional", field.optional);
      writer.flag("required", field.required);
      writer.raw("default",
                 field.defaultValue.empty() ? "null" : field.defaultValue);
      writer.documented(field);
      writer.close('}');
    }
    writer.close(']');
    writer.close('}');
  }
  writer.close(']');

  writer.open("enumerations", '[');
  for (const Enumeration& enumeration : model.enumerations) {
    writer.openItem('{');
    writer.text("name", enumeration.name);
    writer.text("underlying", enumeration.underlying);
    writer.documented(enumeration);
    writer.open("values", '[');
    for (const EnumerationValue& value : enumeration.values) {
      writer.openItem('{');
      writer.text("name", value.name);
      writer.raw("value", std::to_string(value.value));
      writer.documented(value);
      writer.close('}');
    }
    writer.close(']');
    writer.close('}');
  }
  writer.close(']');

  writer.close('}');
  out << "\n";
  return out.str();
}

}  // namespace sigil::protocol::generator
