/** @file
 * The reflected schema read into the model every writer walks, and the
 * rules of the definition's shape checked on the way: what a domain,
 * its events and its commands must be, and that every part of it is
 * documented.
 */

#include "ProtocolModel.h"

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/reflection.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace sigil::protocol::generator {
namespace {

using Attributes =
    flatbuffers::Vector<flatbuffers::Offset<reflection::KeyValue>>;
using Lines = flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>;

/** The suffix that makes a service a domain's events. */
constexpr const char* kEventsSuffix = "Events";

/** The lines of a three-slash comment, each without the one space flatc
 *  keeps after the slashes. */
std::vector<std::string> linesOf(const Lines* lines) {
  std::vector<std::string> out;
  if (!lines) return out;
  for (const flatbuffers::String* line : *lines) {
    std::string text = line->str();
    if (!text.empty() && text[0] == ' ') text.erase(0, 1);
    out.push_back(text);
  }
  return out;
}

/** The value an attribute carries, or nothing where it is not there. */
std::optional<std::string> attributeOf(const Attributes* attributes,
                                       const char* key) {
  if (!attributes) return std::nullopt;
  for (const reflection::KeyValue* each : *attributes)
    if (each->key() && each->key()->str() == key)
      return each->value() ? each->value()->str() : std::string();
  return std::nullopt;
}

Documented documentedOf(const Lines* lines, const Attributes* attributes) {
  Documented out;
  out.documentation = linesOf(lines);
  out.experimental = attributeOf(attributes, "experimental").has_value();
  return out;
}

/** The last dotted word of a qualified name: `Clock` of
 *  `sigil.protocol.clock.Clock`. */
std::string lastWord(const std::string& qualified) {
  const size_t dot = qualified.rfind('.');
  return dot == std::string::npos ? qualified : qualified.substr(dot + 1);
}

/** Everything before the last dotted word. */
std::string spaceOf(const std::string& qualified) {
  const size_t dot = qualified.rfind('.');
  return dot == std::string::npos ? std::string() : qualified.substr(0, dot);
}

/** The schema's own word for a scalar or a string, empty for anything
 *  else. */
std::string wordOf(reflection::BaseType type) {
  switch (type) {
    case reflection::Bool: return "bool";
    case reflection::Byte: return "byte";
    case reflection::UByte: return "ubyte";
    case reflection::Short: return "short";
    case reflection::UShort: return "ushort";
    case reflection::Int: return "int";
    case reflection::UInt: return "uint";
    case reflection::Long: return "long";
    case reflection::ULong: return "ulong";
    case reflection::Float: return "float";
    case reflection::Double: return "double";
    case reflection::String: return "string";
    default: return std::string();
  }
}

/** A double as JSON text that reads back as the same double, with a
 *  point in it so every reader takes it for a real number. */
std::string realText(double value) {
  char buffer[40];
  for (int precision = 1; precision <= 17; ++precision) {
    std::snprintf(buffer, sizeof buffer, "%.*g", precision, value);
    if (std::strtod(buffer, nullptr) == value) break;
  }
  std::string text = buffer;
  if (text.find_first_of(".e") == std::string::npos) text += ".0";
  return text;
}

class Reader {
 public:
  explicit Reader(const reflection::Schema& schema) : m_schema(schema) {}

  std::optional<Model> read(std::string* why) {
    Model model;
    readEnumerations(model);
    readTables(model);
    readDomains(model);
    if (!m_why.empty()) {
      if (why) *why = m_why;
      return std::nullopt;
    }
    return model;
  }

 private:
  void refuse(const std::string& why) {
    if (m_why.empty()) m_why = why;
  }

  void needsDocumentation(const Documented& part, const std::string& name) {
    if (part.documentation.empty())
      refuse(name + " has no documentation, and every part of the definition"
                    " is documented where it is declared");
  }

  void readEnumerations(Model& model) {
    for (const reflection::Enum* each : *m_schema.enums()) {
      const std::string name = each->name()->str();
      if (each->is_union()) {
        refuse("the union " + name +
               " has no place in the definition: a command takes one table"
               " and answers one");
        continue;
      }
      Enumeration enumeration;
      static_cast<Documented&>(enumeration) =
          documentedOf(each->documentation(), each->attributes());
      enumeration.name = name;
      enumeration.underlying = wordOf(each->underlying_type()->base_type());
      needsDocumentation(enumeration, "the enumeration " + name);
      for (const reflection::EnumVal* value : *each->values()) {
        EnumerationValue out;
        static_cast<Documented&>(out) =
            documentedOf(value->documentation(), value->attributes());
        out.name = value->name()->str();
        out.value = value->value();
        needsDocumentation(out, "the value " + name + "." + out.name);
        enumeration.values.push_back(std::move(out));
      }
      model.enumerations.push_back(std::move(enumeration));
    }
  }

  /** The declared default of a scalar field, as JSON text. */
  std::string defaultOf(const reflection::Field& field,
                        const std::string& reference) {
    const reflection::BaseType type = field.type()->base_type();
    if (field.optional() || type == reflection::String ||
        type == reflection::Obj || type == reflection::Vector)
      return std::string();
    if (!reference.empty()) {
      const reflection::Enum* enumeration =
          m_schema.enums()->Get(field.type()->index());
      for (const reflection::EnumVal* value : *enumeration->values())
        if (value->value() == field.default_integer())
          return "\"" + value->name()->str() + "\"";
      return std::to_string(field.default_integer());
    }
    if (type == reflection::Bool)
      return field.default_integer() ? "true" : "false";
    if (type == reflection::Float || type == reflection::Double) {
      if (!std::isfinite(field.default_real())) {
        refuse("the field " + field.name()->str() +
               " declares a default that is no finite number, which JSON"
               " cannot carry");
        return std::string();
      }
      return realText(field.default_real());
    }
    return std::to_string(field.default_integer());
  }

  Field fieldOf(const reflection::Field& field, const std::string& table) {
    Field out;
    static_cast<Documented&>(out) =
        documentedOf(field.documentation(), field.attributes());
    out.name = field.name()->str();
    out.required = field.required();
    const reflection::Type& type = *field.type();
    reflection::BaseType one = type.base_type();
    if (one == reflection::Vector) {
      out.vector = true;
      one = type.element();
    }
    const std::string where = "the field " + table + "." + out.name;
    if (one == reflection::Obj) {
      const reflection::Object* named = m_schema.objects()->Get(type.index());
      if (named->is_struct())
        refuse(where + " holds a struct; the definition holds tables");
      out.type = "table";
      out.reference = named->name()->str();
    } else {
      out.type = wordOf(one);
      if (out.type.empty())
        refuse(where + " is neither a scalar, a string, a table nor a vector"
                       " of one");
      if (type.index() >= 0 && one != reflection::String)
        out.reference = m_schema.enums()->Get(type.index())->name()->str();
    }
    // Reflection calls every field that is no scalar optional, since a
    // string or a table may be left out of a buffer; here optional means
    // what the definition wrote: a scalar declared `= null`, which a
    // value holds as present or absent. An absent string reads as empty
    // and an absent table as absent, which the type already says.
    out.optional = field.optional() && !out.vector && out.type != "string" &&
                   out.type != "table";
    if (!out.vector) out.defaultValue = defaultOf(field, out.reference);
    needsDocumentation(out, where);
    return out;
  }

  void readTables(Model& model) {
    for (const reflection::Object* each : *m_schema.objects()) {
      const std::string name = each->name()->str();
      if (each->is_struct()) {
        refuse("the struct " + name + " has no place in the definition,"
               " which says everything as tables");
        continue;
      }
      Table table;
      static_cast<Documented&>(table) =
          documentedOf(each->documentation(), each->attributes());
      table.name = name;
      needsDocumentation(table, "the table " + name);
      // The reflected fields are sorted by name; their ids are the order
      // the definition declares them in.
      std::map<uint16_t, const reflection::Field*> declared;
      for (const reflection::Field* field : *each->fields())
        if (!field->deprecated()) declared[field->id()] = field;
      for (const auto& [id, field] : declared)
        table.fields.push_back(fieldOf(*field, name));
      if (lastWord(name) == "Empty" && table.fields.empty())
        model.empty = name;
      model.tables.push_back(std::move(table));
    }
    if (model.empty.empty())
      refuse("the definition declares no table Empty with no fields, which a"
             " command with nothing to say takes and answers");
  }

  void readDomains(Model& model) {
    std::map<std::string, const reflection::Service*> services;
    for (const reflection::Service* each : *m_schema.services())
      services[each->name()->str()] = each;

    for (const auto& [name, service] : services) {
      const std::string simple = lastWord(name);
      const std::string suffix = kEventsSuffix;
      const bool events = simple.size() > suffix.size() &&
                          simple.compare(simple.size() - suffix.size(),
                                         suffix.size(), suffix) == 0;
      if (events) {
        const std::string domain =
            name.substr(0, name.size() - suffix.size());
        if (services.find(domain) == services.end())
          refuse("the events service " + name + " has no domain " + domain +
                 " beside it");
        continue;
      }
      Domain domain;
      static_cast<Documented&>(domain) =
          documentedOf(service->documentation(), service->attributes());
      domain.space = spaceOf(name);
      domain.name = lastWord(domain.space);
      domain.service = simple;
      needsDocumentation(domain, "the domain " + name);
      for (const reflection::RPCCall* call : *service->calls())
        domain.commands.push_back(commandOf(*call, domain));
      const auto found = services.find(name + suffix);
      if (found != services.end()) {
        domain.events = documentedOf(found->second->documentation(),
                                     found->second->attributes());
        needsDocumentation(domain.events, "the events service " + name + suffix);
        for (const reflection::RPCCall* call : *found->second->calls())
          domain.eventList.push_back(eventOf(*call, domain));
      }
      checkSubscription(domain);
      model.domains.push_back(std::move(domain));
    }
  }

  Command commandOf(const reflection::RPCCall& call, const Domain& domain) {
    Command out;
    static_cast<Documented&>(out) =
        documentedOf(call.documentation(), call.attributes());
    out.name = call.name()->str();
    out.method = domain.name + "." + out.name;
    out.parameters = call.request()->name()->str();
    out.result = call.response()->name()->str();
    out.asynchronous = attributeOf(call.attributes(), "asynchronous").has_value();
    out.dispatcher = out.name == "enable" || out.name == "disable";
    needsDocumentation(out, "the command " + out.method);
    if (attributeOf(call.attributes(), "streaming"))
      refuse("the command " + out.method +
             " is marked streaming, which only an event is");
    return out;
  }

  Event eventOf(const reflection::RPCCall& call, const Domain& domain) {
    Event out;
    static_cast<Documented&>(out) =
        documentedOf(call.documentation(), call.attributes());
    out.name = call.name()->str();
    out.method = domain.name + "." + out.name;
    out.payload = call.response()->name()->str();
    needsDocumentation(out, "the event " + out.method);
    if (attributeOf(call.attributes(), "streaming") != "server")
      refuse("the event " + out.method +
             " is not marked (streaming: \"server\"), which every event is");
    return out;
  }

  /** A domain with events answers enable and disable; one without has
   *  nothing for them to start. */
  void checkSubscription(const Domain& domain) {
    bool enable = false;
    bool disable = false;
    for (const Command& command : domain.commands) {
      enable = enable || command.name == "enable";
      disable = disable || command.name == "disable";
    }
    if (!domain.eventList.empty() && !(enable && disable))
      refuse("the domain " + domain.name +
             " has events and no enable and disable to start and stop them");
    if (domain.eventList.empty() && (enable || disable))
      refuse("the domain " + domain.name +
             " has enable or disable and no events for them to start");
  }

  const reflection::Schema& m_schema;
  std::string m_why;
};

}  // namespace

std::optional<Model> readModel(std::span<const uint8_t> bfbs, std::string* why) {
  flatbuffers::Verifier verifier(bfbs.data(), bfbs.size());
  if (!reflection::VerifySchemaBuffer(verifier)) {
    if (why) *why = "the bytes are no reflected schema";
    return std::nullopt;
  }
  const reflection::Schema* schema = reflection::GetSchema(bfbs.data());
  if (!schema->services() || schema->services()->size() == 0) {
    if (why)
      *why = "the reflected schema carries no services: the definition"
             " declares none, or flatc dropped them";
    return std::nullopt;
  }
  return Reader(*schema).read(why);
}

const Table* tableNamed(const Model& model, const std::string& name) {
  for (const Table& table : model.tables)
    if (table.name == name) return &table;
  return nullptr;
}

const Enumeration* enumerationNamed(const Model& model,
                                    const std::string& name) {
  for (const Enumeration& enumeration : model.enumerations)
    if (enumeration.name == name) return &enumeration;
  return nullptr;
}

}  // namespace sigil::protocol::generator
