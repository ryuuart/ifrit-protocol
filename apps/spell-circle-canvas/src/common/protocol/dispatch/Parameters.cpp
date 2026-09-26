#include "Parameters.h"

#include <flatbuffers/reflection.h>

#include <cmath>
#include <limits>
#include <string>

#include "Declared.h"

namespace sigil::protocol {
namespace {

/** The definition's word for a field's type: the scalar's own, the
 *  enumeration's name, or what a string, a list and a table are. */
std::string typeWord(const reflection::Field& field,
                     const reflection::Schema& schema) {
  const reflection::Type& type = *field.type();
  if (type.index() >= 0 && type.base_type() != reflection::Obj &&
      type.base_type() != reflection::Vector) {
    const std::string_view name = schema.enums()
                                      ->Get(static_cast<uint32_t>(type.index()))
                                      ->name()
                                      ->string_view();
    return std::string(name.substr(name.rfind('.') + 1));
  }
  switch (type.base_type()) {
    case reflection::Bool:
      return "bool";
    case reflection::Byte:
      return "byte";
    case reflection::UByte:
      return "ubyte";
    case reflection::Short:
      return "short";
    case reflection::UShort:
      return "ushort";
    case reflection::Int:
      return "int";
    case reflection::UInt:
      return "uint";
    case reflection::Long:
      return "long";
    case reflection::ULong:
      return "ulong";
    case reflection::Float:
      return "float";
    case reflection::Double:
      return "double";
    case reflection::String:
      return "string";
    case reflection::Vector:
    case reflection::Vector64:
      return "a list";
    case reflection::Obj:
      return "a table";
    default:
      return "a value";
  }
}

/** The smallest and largest whole numbers @p type holds; nothing for a
 *  type that is not a whole number. */
std::optional<std::pair<double, double>> wholeRange(reflection::BaseType type) {
  switch (type) {
    case reflection::Byte:
      return std::pair{-128.0, 127.0};
    case reflection::UByte:
      return std::pair{0.0, 255.0};
    case reflection::Short:
      return std::pair{-32768.0, 32767.0};
    case reflection::UShort:
      return std::pair{0.0, 65535.0};
    case reflection::Int:
      return std::pair{-2147483648.0, 2147483647.0};
    case reflection::UInt:
      return std::pair{0.0, 4294967295.0};
    case reflection::Long:
      return std::pair{
          static_cast<double>(std::numeric_limits<int64_t>::min()),
          static_cast<double>(std::numeric_limits<int64_t>::max())};
    case reflection::ULong:
      return std::pair{
          0.0, static_cast<double>(std::numeric_limits<uint64_t>::max())};
    default:
      return std::nullopt;
  }
}

/** What @p value is, in words: for a refusal that says what it found. */
std::string kindWord(const data::Json& value) {
  switch (value.kind()) {
    case data::Json::Kind::Null:
      return "null";
    case data::Json::Kind::Boolean:
      return "a boolean";
    case data::Json::Kind::Number:
      return "a number";
    case data::Json::Kind::String:
      return "text";
    case data::Json::Kind::Array:
      return "a list";
    case data::Json::Kind::Object:
      return "a table";
  }
  return "a value";
}

/** Why @p value does not fit @p field, or nothing where it does. Null
 *  fits every field, standing for its default or its absence. */
std::optional<std::string> misfitField(const reflection::Field& field,
                                       const reflection::Schema& schema,
                                       const data::Json& value) {
  if (value.null()) return std::nullopt;
  const reflection::Type& type = *field.type();
  const std::string named = "parameter " + field.name()->str();
  const std::string expected = typeWord(field, schema);
  const auto refuse = [&]() {
    return named + " holds " + kindWord(value) + ", and the table's " +
           field.name()->str() + " is " + expected;
  };
  const reflection::BaseType base = type.base_type();
  const bool enumeration = type.index() >= 0 && base != reflection::Obj &&
                           base != reflection::Vector &&
                           base != reflection::Vector64;
  if (enumeration && value.kind() == data::Json::Kind::String) {
    const reflection::Enum* declared =
        schema.enums()->Get(static_cast<uint32_t>(type.index()));
    for (const reflection::EnumVal* each : *declared->values())
      if (each->name()->string_view() == value.string()) return std::nullopt;
    return named + " names " + std::string(value.string()) +
           ", which is no value of " + expected;
  }
  switch (base) {
    case reflection::Bool:
      if (value.kind() == data::Json::Kind::Boolean) return std::nullopt;
      return refuse();
    case reflection::Float:
    case reflection::Double:
      if (value.kind() == data::Json::Kind::Number) return std::nullopt;
      return refuse();
    case reflection::String:
      if (value.kind() == data::Json::Kind::String) return std::nullopt;
      return refuse();
    case reflection::Vector:
    case reflection::Vector64:
      if (value.kind() == data::Json::Kind::Array) return std::nullopt;
      return refuse();
    case reflection::Obj:
      if (value.kind() == data::Json::Kind::Object) return std::nullopt;
      return refuse();
    default:
      break;
  }
  if (const std::optional<std::pair<double, double>> range = wholeRange(base)) {
    if (value.kind() != data::Json::Kind::Number) return refuse();
    const double number = value.number();
    if (std::floor(number) != number || number < range->first ||
        number > range->second)
      return named + " holds " + jsonText(value) + ", which " +
             (enumeration ? "no value of " : "no ") + expected + " holds";
    // An enumeration given by its number is one of the numbers it
    // declares, as one given by name is one of its names.
    if (enumeration) {
      const reflection::Enum* declared =
          schema.enums()->Get(static_cast<uint32_t>(type.index()));
      for (const reflection::EnumVal* each : *declared->values())
        if (static_cast<double>(each->value()) == number) return std::nullopt;
      return named + " holds " + jsonText(value) +
             ", which is no value of " + expected;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace

std::optional<std::string> misfit(std::string_view method,
                                  const data::Json& parameters) {
  const Declared& definition = declared();
  const auto found = definition.parameters.find(method);
  if (found == definition.parameters.end() || !found->second ||
      !definition.schema)
    return std::nullopt;
  if (parameters.null()) return std::nullopt;
  if (parameters.kind() != data::Json::Kind::Object)
    return "the parameters are " + kindWord(parameters) + ", not a table";
  const reflection::Object& table = *found->second;
  for (const auto& [name, value] : parameters.object()) {
    const reflection::Field* field = nullptr;
    if (table.fields())
      for (const reflection::Field* each : *table.fields())
        if (each->name()->string_view() == name) field = each;
    if (!field)
      return "there is no parameter " + name + ": the table " +
             table.name()->str() + " does not declare it";
    if (std::optional<std::string> why =
            misfitField(*field, *definition.schema, value))
      return why;
  }
  return std::nullopt;
}

}  // namespace sigil::protocol
